#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>

/*
    ITU-R BS.1770 / EBU R128 loudness: K-weighting, momentary (400 ms),
    short-term (3 s) and gated integrated loudness. Integrated loudness uses a
    0.1 LU histogram of the 400 ms gating blocks (75 % overlap), so memory is
    constant no matter how long it runs. process() runs on the audio thread,
    the getters are safe to call from the GUI.
*/
class LoudnessMeter
{
public:
    static constexpr double silence = -100.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        design();
        subLen = juce::jmax (1, (int) std::lround (fs * 0.1));
        resetState();
    }

    void resetState()
    {
        for (auto& f : filters)
            f.z.fill ({});
        acc = 0.0;
        count = 0;
        subs.fill (0.0);
        numSubs = subPos = 0;
        momentary = (float) silence;
        shortTerm = (float) silence;
    }

    void resetIntegrated()
    {
        for (auto& b : hist)
            b.store (0, std::memory_order_relaxed);
    }

    void process (const float* const* data, int numChannels, int numSamples) noexcept
    {
        const int nch = juce::jmin (numChannels, 2);

        for (int i = 0; i < numSamples; ++i)
        {
            double sum = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                double v = data[c][i];
                v = filters[0].process (v, c);
                v = filters[1].process (v, c);
                sum += v * v;
            }

            acc += sum;
            if (++count >= subLen)
                closeSubBlock();
        }
    }

    float getMomentary() const noexcept { return momentary.load(); }
    float getShortTerm() const noexcept { return shortTerm.load(); }

    double getIntegrated() const
    {
        double sumE = 0.0;
        uint64_t total = 0;
        for (int b = 0; b < numBins; ++b)
            if (const auto c = hist[(size_t) b].load (std::memory_order_relaxed))
            {
                sumE += c * toEnergy (binLufs (b));
                total += c;
            }

        if (total == 0)
            return silence;

        const double gate = toLufs (sumE / (double) total) - 10.0;

        sumE = 0.0;
        total = 0;
        for (int b = 0; b < numBins; ++b)
            if (binLufs (b) >= gate)
                if (const auto c = hist[(size_t) b].load (std::memory_order_relaxed))
                {
                    sumE += c * toEnergy (binLufs (b));
                    total += c;
                }

        return total > 0 ? toLufs (sumE / (double) total) : silence;
    }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        std::array<std::array<double, 2>, 2> z {}; // [channel][state]

        double process (double x, int c) noexcept
        {
            auto& s = z[(size_t) c];
            const double y = b0 * x + s[0];
            s[0] = b1 * x - a1 * y + s[1];
            s[1] = b2 * x - a2 * y;
            return y;
        }
    };

    void design()
    {
        const double pi = juce::MathConstants<double>::pi;

        // stage 1: high shelf (head acoustics)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K  = std::tan (pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            auto& f = filters[0];
            f.b0 = (Vh + Vb * K / Q + K * K) / a0;
            f.b1 = 2.0 * (K * K - Vh) / a0;
            f.b2 = (Vh - Vb * K / Q + K * K) / a0;
            f.a1 = 2.0 * (K * K - 1.0) / a0;
            f.a2 = (1.0 - K / Q + K * K) / a0;
        }

        // stage 2: RLB high-pass
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K  = std::tan (pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            auto& f = filters[1];
            f.b0 = 1.0; f.b1 = -2.0; f.b2 = 1.0;
            f.a1 = 2.0 * (K * K - 1.0) / a0;
            f.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }

    void closeSubBlock() noexcept
    {
        subs[(size_t) subPos] = acc / subLen;
        subPos = (subPos + 1) % numSubSlots;
        numSubs = juce::jmin (numSubs + 1, numSubSlots);
        acc = 0.0;
        count = 0;

        const double eM = meanOfLast (4), eS = meanOfLast (numSubSlots);
        momentary = (float) toLufs (eM);
        shortTerm = (float) toLufs (eS);

        if (numSubs >= 4)
        {
            const double l = toLufs (eM);
            if (l >= -70.0)
            {
                const int b = juce::jlimit (0, numBins - 1, (int) std::lround ((l + 70.0) * 10.0));
                hist[(size_t) b].fetch_add (1, std::memory_order_relaxed);
            }
        }
    }

    double meanOfLast (int k) const noexcept
    {
        const int m = juce::jmin (k, numSubs);
        if (m == 0)
            return 0.0;
        double s = 0.0;
        for (int j = 0; j < m; ++j)
            s += subs[(size_t) ((subPos - 1 - j + numSubSlots) % numSubSlots)];
        return s / m;
    }

    static double toLufs (double e) noexcept   { return e > 1.0e-12 ? -0.691 + 10.0 * std::log10 (e) : silence; }
    static double toEnergy (double l) noexcept { return std::pow (10.0, (l + 0.691) / 10.0); }
    static double binLufs (int b) noexcept     { return -70.0 + 0.1 * b; }

    static constexpr int numSubSlots = 30; // 30 x 100 ms = short-term window
    static constexpr int numBins = 751;    // -70 .. +5 LUFS in 0.1 LU steps

    double fs = 48000.0;
    std::array<Biquad, 2> filters;
    double acc = 0.0;
    int count = 0, subLen = 4800;
    std::array<double, numSubSlots> subs {};
    int numSubs = 0, subPos = 0;

    std::atomic<float> momentary { (float) silence }, shortTerm { (float) silence };
    std::array<std::atomic<uint32_t>, numBins> hist {};
};
