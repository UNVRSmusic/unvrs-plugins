#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

/*
    Simulates the signal path of an S2000-style 16-bit rack sampler.
    Runs at the oversampled rate Fo (an integer number of Hz).

        input
          -> analog anti-alias filter (8th-order Butterworth @ 0.45 * recRate)
          -> ADC: sample at recRate (11.025 / 22.05 / 32 / 44.1 kHz), hard clip, N-bit quantise
          -> sample engine: resample recRate -> 44.1 kHz (drop / linear / hermite)
          -> digital 2-pole resonant low-pass (at 44.1 kHz)
          -> DAC: 16-bit quantise @ 44.1 kHz
          -> reconstruction: hermite + 8th-order Butterworth @ 20 kHz  (or raw zero-order hold)
          -> analog output stage: asymmetric soft saturation, hiss, DC-blocking cap
        output

    The three rate domains (Fo, recRate, 44.1 kHz) are driven by one master tick counter
    using exact integer index arithmetic, so the output has a constant latency of
    getLatencyInOversampledSamples() samples at Fo.
*/
class ConverterEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int engineRate = 44100;   // the sampler's internal / DAC rate
    static constexpr int minRecRate = 11025;   // lowest selectable sampling rate (sets the fixed latency)

    enum class Interp { drop = 0, linear, hermite };

    struct Settings
    {
        int    recRate     = 44100;
        int    adcBits     = 16;
        Interp interp      = Interp::linear;
        bool   aaFilter    = true;
        bool   dacFilter   = true;
        float  cutoffHz    = 20000.0f;
        float  resonance   = 0.0f;   // 0..1
        float  drive       = 0.2f;   // 0..1
        float  noise       = 0.1f;   // 0..1
    };

    void prepare (int oversampledRateHz, int osFactor, int numChannelsIn)
    {
        fo = oversampledRateHz;
        numChannels = juce::jlimit (1, maxChannels, numChannelsIn);

        // Worst-case pipeline delay (see header comment), rounded up to a multiple of the
        // oversampling factor so that the host-rate latency is an integer.
        const double d = 2.0 * fo / engineRate + 2.0 * fo / minRecRate + 4.0;
        latency = (int) std::ceil (d / osFactor) * osFactor;

        reconCoeffs = juce::dsp::FilterDesign<double>::designIIRLowpassHighOrderButterworthMethod (20000.0, fo, 8);
        dcCoeff = std::exp (-2.0 * juce::MathConstants<double>::pi * 8.0 / fo);

        for (auto& c : ch)
        {
            c.recon.clear();
            for (auto& coeffs : reconCoeffs)
                c.recon.emplace_back (coeffs);
        }

        currentRecRate = -1; // forces AA design + reset on first update
        updateSettings (settings);
        reset();
    }

    int getLatencyInOversampledSamples() const noexcept { return latency; }

    void updateSettings (const Settings& s)
    {
        const bool rateChanged = s.recRate != currentRecRate;
        settings = s;

        if (rateChanged)
        {
            currentRecRate = s.recRate;
            aaCoeffs = juce::dsp::FilterDesign<double>::designIIRLowpassHighOrderButterworthMethod (0.45 * currentRecRate, fo, 8);

            for (auto& c : ch)
            {
                c.aa.clear();
                for (auto& coeffs : aaCoeffs)
                    c.aa.emplace_back (coeffs);
            }

            reset();
        }

        // TPT state-variable low-pass at the engine rate (Zavalishin)
        const double fc = juce::jlimit (20.0, 0.49 * engineRate, (double) s.cutoffHz);
        svfG = std::tan (juce::MathConstants<double>::pi * fc / engineRate);
        svfK = 2.0 - 1.95 * juce::jlimit (0.0, 1.0, (double) s.resonance); // k = 1/Q
        svfActive = s.cutoffHz < 19900.0f || s.resonance > 0.001f;

        driveGain = 1.0 + 9.0 * s.drive;
        driveNorm = 1.0 / (driveGain * (1.0 - std::pow (std::tanh (driveGain * driveBias), 2.0)));
        driveOffset = std::tanh (driveGain * driveBias);

        noiseAmp = 0.0006 * s.noise * s.noise; // up to ~ -64 dBFS
    }

    void reset()
    {
        tick = 0;
        nextRec = 0;
        nextEng = 0;

        for (auto& c : ch)
        {
            c.x.fill (0.0);
            c.r.fill (0.0);
            c.e.fill (0.0);
            for (auto& f : c.aa)    f.reset();
            for (auto& f : c.recon) f.reset();
            c.s1 = c.s2 = 0.0;
            c.dcX = c.dcY = 0.0;
        }

        clipFlag = false;
    }

    /** Processes one oversampled block in place. */
    void process (juce::dsp::AudioBlock<float>& block)
    {
        const auto n = (int) block.getNumSamples();
        const int nch = juce::jmin (numChannels, (int) block.getNumChannels());

        for (int i = 0; i < n; ++i)
        {
            // ---- push input through the analog anti-alias filter --------------------
            for (int c = 0; c < nch; ++c)
            {
                double v = block.getSample (c, i);
                if (settings.aaFilter)
                    for (auto& f : ch[c].aa) v = f.processSample (v);
                ch[c].x[(size_t) (tick & ringMask)] = v;
            }

            // ---- ADC: produce every recRate sample whose hermite window is available --
            for (;;)
            {
                const int64_t num = nextRec * (int64_t) fo;
                const int64_t idx = num / currentRecRate;
                if (idx + 2 > tick) break;
                const double frac = (double) (num % currentRecRate) / currentRecRate;

                for (int c = 0; c < nch; ++c)
                {
                    auto& st = ch[c];
                    double v = settings.aaFilter
                                 ? hermite (st.x, idx, frac)
                                 : st.x[(size_t) (idx & ringMask)];   // no AA: naive point sampling
                    st.r[(size_t) (nextRec & ringMask)] = quantise (v, settings.adcBits, true);
                }
                ++nextRec;
            }

            // ---- sample engine: recRate -> 44.1 kHz, filter, DAC ---------------------------
            for (;;)
            {
                const int64_t num = nextEng * (int64_t) currentRecRate;
                const int64_t idx = num / engineRate;
                if (idx + 2 > nextRec - 1) break;
                const double frac = (double) (num % engineRate) / engineRate;

                for (int c = 0; c < nch; ++c)
                {
                    auto& st = ch[c];
                    double v;
                    switch (settings.interp)
                    {
                        case Interp::drop:    v = st.r[(size_t) (idx & ringMask)]; break;
                        case Interp::hermite: v = hermite (st.r, idx, frac); break;
                        case Interp::linear:
                        default:
                        {
                            const double a = st.r[(size_t) (idx & ringMask)];
                            const double b = st.r[(size_t) ((idx + 1) & ringMask)];
                            v = a + frac * (b - a);
                            break;
                        }
                    }

                    if (svfActive)
                        v = svf (st, v);

                    st.e[(size_t) (nextEng & ringMask)] = quantise (v, 16, false);
                }
                ++nextEng;
            }

            // ---- DAC reconstruction + analog output stage --------------------------------
            const int64_t outPos = tick - latency;

            for (int c = 0; c < nch; ++c)
            {
                auto& st = ch[c];
                double v = 0.0;

                if (outPos >= 0)
                {
                    const int64_t num = outPos * (int64_t) engineRate;
                    const int64_t idx = num / fo;
                    const double frac = (double) (num % fo) / fo;

                    if (settings.dacFilter)
                    {
                        v = hermite (st.e, idx, frac);
                        for (auto& f : st.recon) v = f.processSample (v);
                    }
                    else
                    {
                        v = st.e[(size_t) (idx & ringMask)]; // raw zero-order hold: images & droop
                    }
                }

                // output op-amp stage
                if (settings.drive > 0.001f)
                    v = (std::tanh (driveGain * (v + driveBias)) - driveOffset) * driveNorm;

                if (noiseAmp > 0.0)
                    v += noiseAmp * (2.0 * st.rng.nextDouble() - 1.0);

                // coupling capacitor (DC block)
                const double y = v - st.dcX + dcCoeff * st.dcY;
                st.dcX = v;
                st.dcY = y;

                block.setSample (c, i, (float) y);
            }

            ++tick;
        }
    }

    bool getAndClearClip() noexcept { return clipFlag.exchange (false); }

private:
    static constexpr int ringSize = 128;
    static constexpr int64_t ringMask = ringSize - 1;
    using Ring = std::array<double, ringSize>;

    struct Channel
    {
        Ring x {}, r {}, e {};
        std::vector<juce::dsp::IIR::Filter<double>> aa, recon;
        double s1 = 0.0, s2 = 0.0;      // SVF state
        double dcX = 0.0, dcY = 0.0;    // DC blocker state
        juce::Random rng;
    };

    static double hermite (const Ring& buf, int64_t idx, double t) noexcept
    {
        const double y0 = buf[(size_t) ((idx - 1) & ringMask)];
        const double y1 = buf[(size_t) ( idx      & ringMask)];
        const double y2 = buf[(size_t) ((idx + 1) & ringMask)];
        const double y3 = buf[(size_t) ((idx + 2) & ringMask)];

        const double c1 = 0.5 * (y2 - y0);
        const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    double quantise (double v, int bits, bool flagClip) noexcept
    {
        const double scale = (double) (1 << (bits - 1));
        const double maxV = (scale - 1.0) / scale;

        if (v > maxV || v < -1.0)
        {
            if (flagClip) clipFlag = true;
            v = juce::jlimit (-1.0, maxV, v);
        }

        return std::round (v * scale) / scale;
    }

    double svf (Channel& st, double x) const noexcept
    {
        const double g = svfG, k = svfK;
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;

        const double v3 = x - st.s2;
        const double v1 = a1 * st.s1 + a2 * v3;
        const double v2 = st.s2 + a2 * st.s1 + a3 * v3;
        st.s1 = 2.0 * v1 - st.s1;
        st.s2 = 2.0 * v2 - st.s2;
        return v2;
    }

    Settings settings;
    int fo = 176400, numChannels = 2, latency = 0, currentRecRate = -1;

    int64_t tick = 0, nextRec = 0, nextEng = 0;
    std::array<Channel, maxChannels> ch;

    juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<double>> aaCoeffs, reconCoeffs;

    double svfG = 1.0, svfK = 1.414;
    bool svfActive = false;

    static constexpr double driveBias = 0.12;
    double driveGain = 1.0, driveNorm = 1.0, driveOffset = 0.0;
    double noiseAmp = 0.0, dcCoeff = 0.999;

    std::atomic<bool> clipFlag { false };
};
