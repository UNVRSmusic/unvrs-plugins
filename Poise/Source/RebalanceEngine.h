#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

/*
    Poise spectral rebalancer.

    STFT (Hann analysis + Hann synthesis, 75 % overlap, latency = N). Per frame:
      1. mid-channel power in 1/3-octave bands, as density per octave (pink noise reads flat)
      2. smoothed band levels (SPEED) and a slow long-term average of the track itself
      3. deviation = mix of (level vs reference curve) and (level vs the track's own long-term
         balance) -> KEEP CHARACTER; the overall offset is removed, only the shape counts
      4. gain = -AMOUNT * deviation, smoothed across frequency (RESOLUTION), limited (RANGE,
         MODE, LOW/HIGH LIMIT), never boosting bands that are near silent
      5. per-bin zero-phase gain interpolated in log frequency, same for L and R
*/
class RebalanceEngine
{
public:
    static constexpr int maxBands = 32;
    enum Target { pink = 0, mix, warm, bright, learned };
    enum Mode { both = 0, cutOnly, boostOnly };

    struct Params
    {
        float amount = 0.5f, speed = 0.6f, resolution = 1.0f, range = 6.0f, keep = 0.4f, tilt = 0.0f;
        int target = mix, mode = both;
        float lowLimit = 30.0f, highLimit = 16000.0f, mix = 1.0f, outputDb = 0.0f;
        bool autoGain = true;
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        order = fs <= 50000.0 ? 12 : (fs <= 100000.0 ? 13 : 14);
        N = 1 << order;
        hop = N / 4;
        fft = std::make_unique<juce::dsp::FFT> (order);

        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N); // periodic Hann

        for (auto& v : inBuf) v.assign ((size_t) N, 0.0f);
        for (auto& v : outBuf) v.assign ((size_t) N, 0.0f);
        for (auto& v : spec) v.assign ((size_t) (2 * N), 0.0f);
        binGain.assign ((size_t) (N / 2 + 1), 1.0f);

        // 1/3-octave bands from 25 Hz up to 20 kHz (or 0.45 fs)
        numBands = 0;
        for (int i = 0; i < maxBands; ++i)
        {
            const double c = 25.0 * std::pow (2.0, i / 3.0);
            if (c > juce::jmin (20000.0, 0.45 * fs)) break;
            centres[(size_t) numBands++] = (float) c;
        }

        // triangular weights in log frequency (each band reaches its neighbours)
        const double binHz = fs / N;
        for (int b = 0; b < numBands; ++b)
        {
            auto& w = weights[(size_t) b];
            w.clear();
            const double c = centres[(size_t) b];
            for (int k = 1; k <= N / 2; ++k)
            {
                const double d = std::abs (std::log2 (k * binHz / c)) * 3.0;
                if (d < 1.0) w.push_back ({ k, (float) (1.0 - d) });
            }
            if (w.empty()) // very low bands may fall between bins
                w.push_back ({ juce::jlimit (1, N / 2, (int) std::lround (c / binHz)), 1.0f });
        }

        // band position of every bin, for interpolating gains
        binPos.resize ((size_t) (N / 2 + 1));
        for (int k = 0; k <= N / 2; ++k)
        {
            const double f = juce::jmax (1.0, k * binHz);
            binPos[(size_t) k] = (float) juce::jlimit (0.0, (double) (numBands - 1), std::log2 (f / 25.0) * 3.0);
        }

        reset();
    }

    void reset()
    {
        for (auto& v : inBuf) std::fill (v.begin(), v.end(), 0.0f);
        for (auto& v : outBuf) std::fill (v.begin(), v.end(), 0.0f);
        pos = count = 0;
        power.fill (0.0); longPower.fill (0.0); gainSm.fill (0.0f);
        longInit = false;
        autoGainDb = 0.0f;
    }

    int getLatency() const noexcept { return N; }
    void setParams (const Params& p) noexcept { params = p; }

    void process (float* const* io, int nch, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[juce::jmin (c, nch - 1)][i];
                inBuf[(size_t) c][(size_t) pos] = x;
                const float y = outBuf[(size_t) c][(size_t) pos];
                outBuf[(size_t) c][(size_t) pos] = 0.0f;
                if (c < nch) io[c][i] = y;
            }
            pos = (pos + 1) % N;
            if (++count >= hop)
            {
                count = 0;
                processFrame();
            }
        }
    }

    //==============================================================================
    // shared with the GUI
    int numBands = 0;
    std::array<float, maxBands> centres {};
    std::array<std::atomic<float>, maxBands> guiLevel {}, guiTarget {}, guiGain {};
    std::array<std::atomic<float>, maxBands> learnedCurve {};
    std::atomic<bool> hasLearned { false }, learning { false };

    static float presetTarget (int target, double f)
    {
        const double oct = std::log2 (f / 1000.0);
        switch (target)
        {
            case mix:    return (float) (-0.4 * oct - (f > 6000.0 ? 2.5 * std::log2 (f / 6000.0) : 0.0) - (f < 40.0 ? 3.0 * std::log2 (40.0 / f) : 0.0));
            case warm:   return (float) (-1.5 * oct);
            case bright: return (float) (1.0 * oct);
            default:     return 0.0f;
        }
    }

private:
    struct W { int bin; float w; };

    void processFrame() noexcept
    {
        // ---- analysis -------------------------------------------------------------------
        for (int c = 0; c < 2; ++c)
        {
            auto& s = spec[(size_t) c];
            for (int i = 0; i < N; ++i)
                s[(size_t) i] = inBuf[(size_t) c][(size_t) ((pos + i) % N)] * window[(size_t) i];
            std::fill (s.begin() + N, s.end(), 0.0f);
            fft->performRealOnlyForwardTransform (s.data());
        }

        const auto& L = spec[0];
        const auto& R = spec[1];
        const double aLevel = std::exp (-(double) hop / (juce::jmax (0.02f, params.speed) * fs));
        const double aLong  = std::exp (-(double) hop / (8.0 * fs));
        const double aGain  = std::exp (-(double) hop / (juce::jmax (0.01f, params.speed * 0.5f) * fs));

        std::array<double, maxBands> lvl {}, lng {};
        double maxLvl = -300.0;
        for (int b = 0; b < numBands; ++b)
        {
            double sum = 0.0, wsum = 0.0;
            for (const auto& w : weights[(size_t) b])
            {
                const double re = 0.5 * (L[(size_t) (2 * w.bin)] + R[(size_t) (2 * w.bin)]);
                const double im = 0.5 * (L[(size_t) (2 * w.bin + 1)] + R[(size_t) (2 * w.bin + 1)]);
                sum += w.w * (re * re + im * im);
                wsum += w.w;
            }
            const double density = sum / wsum * centres[(size_t) b]; // per-octave density: pink reads flat
            power[(size_t) b] = aLevel * power[(size_t) b] + (1.0 - aLevel) * density;
            if (! longInit) longPower[(size_t) b] = power[(size_t) b];
            longPower[(size_t) b] = aLong * longPower[(size_t) b] + (1.0 - aLong) * power[(size_t) b];
            lvl[(size_t) b] = 10.0 * std::log10 (power[(size_t) b] + 1.0e-30);
            lng[(size_t) b] = 10.0 * std::log10 (longPower[(size_t) b] + 1.0e-30);
            if (inLimits (b)) maxLvl = juce::jmax (maxLvl, lvl[(size_t) b]);
        }
        if (maxLvl > -200.0) longInit = true;

        // ---- deviation from the target -------------------------------------------------------
        std::array<bool, maxBands> valid {};
        std::array<double, maxBands> target {};
        double offRef = 0.0, offSelf = 0.0;
        int nValid = 0;
        for (int b = 0; b < numBands; ++b)
        {
            const double f = centres[(size_t) b];
            const double t = params.target == learned && hasLearned.load() ? learnedCurve[(size_t) b].load()
                                                                            : presetTarget (params.target, f);
            target[(size_t) b] = t + params.tilt * std::log2 (f / 1000.0);
            valid[(size_t) b] = inLimits (b) && lvl[(size_t) b] > maxLvl - 50.0 && lvl[(size_t) b] > -160.0;
            if (valid[(size_t) b])
            {
                offRef += lvl[(size_t) b] - target[(size_t) b];
                offSelf += lvl[(size_t) b] - lng[(size_t) b];
                ++nValid;
            }
        }
        if (nValid > 0) { offRef /= nValid; offSelf /= nValid; }

        std::array<double, maxBands> raw {};
        for (int b = 0; b < numBands; ++b)
        {
            if (! valid[(size_t) b]) continue;
            const double devRef = lvl[(size_t) b] - target[(size_t) b] - offRef;
            const double devSelf = lvl[(size_t) b] - lng[(size_t) b] - offSelf;
            raw[(size_t) b] = -params.amount * ((1.0 - params.keep) * devRef + params.keep * devSelf);
        }

        // learning: average shape of the reference (offset removed)
        if (learning.load() && nValid > 3)
        {
            double meanLvl = 0.0;
            for (int b = 0; b < numBands; ++b)
                if (valid[(size_t) b]) meanLvl += lvl[(size_t) b];
            meanLvl /= nValid;
            for (int b = 0; b < numBands; ++b)
                if (valid[(size_t) b])
                {
                    learnSum[(size_t) b] += lvl[(size_t) b] - meanLvl; // shape only, level removed
                    ++learnCount[(size_t) b];
                }
            learnedPending = true;
        }
        else if (learnedPending && ! learning.load())
        {
            finishLearning();
        }

        // ---- smooth across frequency, limit, gate ---------------------------------------------
        const double sigma = juce::jmax (0.34, (double) params.resolution) * 3.0;
        std::array<float, maxBands> g {};
        double pSum = 0.0, pGain = 0.0;
        for (int b = 0; b < numBands; ++b)
        {
            double acc = 0.0, ws = 0.0;
            for (int j = 0; j < numBands; ++j)
            {
                if (! valid[(size_t) j]) continue;
                const double w = std::exp (-0.5 * (b - j) * (b - j) / (sigma * sigma));
                acc += w * raw[(size_t) j];
                ws += w;
            }
            double v = ws > 0.0 ? acc / ws : 0.0;
            if (! valid[(size_t) b]) v = juce::jmin (v, 0.0);            // never lift near-silent bands
            if (params.mode == cutOnly)   v = juce::jmin (v, 0.0);
            if (params.mode == boostOnly) v = juce::jmax (v, 0.0);
            v = juce::jlimit ((double) -params.range, (double) params.range, v);
            v *= limitFade (b);

            auto& gs = gainSm[(size_t) b];
            gs = (float) (aGain * gs + (1.0 - aGain) * v);
            g[(size_t) b] = gs * params.mix;

            pSum += power[(size_t) b];
            pGain += power[(size_t) b] * std::pow (10.0, g[(size_t) b] / 10.0);

            guiLevel[(size_t) b] = (float) (lvl[(size_t) b] - offRef);
            guiTarget[(size_t) b] = (float) target[(size_t) b];
            guiGain[(size_t) b] = g[(size_t) b];
        }

        const float agTarget = params.autoGain && pSum > 0.0 ? (float) (-10.0 * std::log10 (pGain / pSum)) : 0.0f;
        autoGainDb = (float) (aGain * autoGainDb + (1.0 - aGain) * agTarget);
        const float outLin = juce::Decibels::decibelsToGain (params.outputDb + autoGainDb);

        // ---- per-bin gains and resynthesis ----------------------------------------------------
        for (int k = 0; k <= N / 2; ++k)
        {
            const float p = binPos[(size_t) k];
            const int b0 = (int) p;
            const int b1 = juce::jmin (numBands - 1, b0 + 1);
            const float t = p - (float) b0;
            const float db = g[(size_t) b0] + t * (g[(size_t) b1] - g[(size_t) b0]);
            binGain[(size_t) k] = juce::Decibels::decibelsToGain (db) * outLin;
        }

        constexpr float olaScale = 2.0f / 3.0f; // Hann^2 at 75 % overlap sums to 1.5
        for (int c = 0; c < 2; ++c)
        {
            auto& s = spec[(size_t) c];
            for (int k = 0; k <= N / 2; ++k)
            {
                s[(size_t) (2 * k)] *= binGain[(size_t) k];
                s[(size_t) (2 * k + 1)] *= binGain[(size_t) k];
                if (k > 0 && k < N / 2)
                {
                    s[(size_t) (2 * (N - k))] *= binGain[(size_t) k];
                    s[(size_t) (2 * (N - k) + 1)] *= binGain[(size_t) k];
                }
            }
            fft->performRealOnlyInverseTransform (s.data());
            auto& o = outBuf[(size_t) c];
            for (int i = 0; i < N; ++i)
                o[(size_t) ((pos + i) % N)] += s[(size_t) i] * window[(size_t) i] * olaScale;
        }
    }

    bool inLimits (int b) const noexcept
    {
        const float f = centres[(size_t) b];
        return f >= params.lowLimit / 1.26f && f <= params.highLimit * 1.26f;
    }

    float limitFade (int b) const noexcept
    {
        const double f = centres[(size_t) b];
        const double below = std::log2 (params.lowLimit / f), above = std::log2 (f / params.highLimit);
        const double d = juce::jmax (below, above);         // octaves outside the range
        return (float) juce::jlimit (0.0, 1.0, 1.0 - d * 3.0); // fade over a third of an octave
    }

    void finishLearning() noexcept
    {
        learnedPending = false;
        double mean = 0.0;
        int n = 0;
        std::array<double, maxBands> v {};
        for (int b = 0; b < numBands; ++b)
            if (learnCount[(size_t) b] > 0) { v[(size_t) b] = learnSum[(size_t) b] / learnCount[(size_t) b]; mean += v[(size_t) b]; ++n; }
        if (n < 4) { clearLearnAccumulators(); return; }
        mean /= n;
        // fill bands that never had signal from their nearest neighbour
        for (int b = 0; b < numBands; ++b)
            if (learnCount[(size_t) b] == 0)
            {
                int best = -1;
                for (int d = 1; d < numBands && best < 0; ++d)
                {
                    if (b - d >= 0 && learnCount[(size_t) (b - d)] > 0) best = b - d;
                    else if (b + d < numBands && learnCount[(size_t) (b + d)] > 0) best = b + d;
                }
                v[(size_t) b] = best >= 0 ? v[(size_t) best] : mean;
            }
        for (int b = 0; b < numBands; ++b)
            learnedCurve[(size_t) b] = (float) (v[(size_t) b] - mean);
        hasLearned = true;
        clearLearnAccumulators();
    }

    void clearLearnAccumulators() noexcept { learnSum.fill (0.0); learnCount.fill (0); }

    double fs = 48000.0;
    int order = 12, N = 4096, hop = 1024, pos = 0, count = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, binGain, binPos;
    std::array<std::vector<float>, 2> inBuf, outBuf, spec;
    std::array<std::vector<W>, maxBands> weights;

    Params params;
    std::array<double, maxBands> power {}, longPower {}, learnSum {};
    std::array<int, maxBands> learnCount {};
    std::array<float, maxBands> gainSm {};
    bool longInit = false, learnedPending = false;
    float autoGainDb = 0.0f;
};
