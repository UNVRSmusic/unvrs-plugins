#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

/*
    Look-ahead brickwall limiter core. Runs at whatever rate it is prepared for
    (the processor feeds it oversampled audio).

    The gain computer works in the natural-log gain domain (0 = unity, < 0 = attenuation):
      1. required gain   r[n] = min (0, ln ceiling - ln |x[n]|), optionally linked across channels
      2. sliding minimum of r over the attack window H                -> h[n]
      3. dual-stage, program-dependent release envelope               -> e[n] <= h[n]
      4. one or two cascaded moving averages with total support H     -> g[n]
    Every value averaged in step 4 is <= the requirement of the sample the gain is
    applied to, so the output never exceeds the ceiling, while the gain glides into
    each peak instead of jumping. Audio is delayed by a constant D >= H, so the
    latency does not depend on the lookahead setting.
*/
class LimiterEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int numStyles = 4;

    struct Settings
    {
        double ceilingDb   = -1.0;
        double lookaheadMs = 1.5;
        double releaseMs   = 150.0;
        int    style       = 0;
        double link        = 1.0; // 0..1
    };

    void prepare (double sampleRate, int constantDelaySamples)
    {
        fs = sampleRate;
        delay = juce::jmax (1, constantDelaySamples);

        ringSize = 1;
        while (ringSize < 2 * (int64_t) delay + 16)
            ringSize <<= 1;
        mask = ringSize - 1;

        for (auto& c : ch)
        {
            for (auto* v : { &c.r, &c.env, &c.box1, &c.gain, &c.x, &c.dqVal })
                v->assign ((size_t) ringSize, 0.0);
            c.dqIdx.assign ((size_t) ringSize, 0);
        }

        hold = b1 = b2 = -1; // forces reconfiguration on the next updateSettings()
        reset();
    }

    void reset()
    {
        n = 0;
        for (auto& c : ch)
        {
            for (auto* v : { &c.r, &c.env, &c.box1, &c.gain, &c.x, &c.dqVal })
                std::fill (v->begin(), v->end(), 0.0);
            std::fill (c.dqIdx.begin(), c.dqIdx.end(), 0);
            c.dqHead = c.dqTail = 0;
            c.envF = c.envS = 0.0;
            c.sum1 = c.sum2 = 0.0;
        }
    }

    /** Latency in samples at the rate this engine runs at. */
    int getDelay() const noexcept { return delay; }

    void updateSettings (const Settings& s)
    {
        const auto& st = styles[juce::jlimit (0, numStyles - 1, s.style)];

        ceilingLin = std::pow (10.0, s.ceilingDb / 20.0);
        ceilingLog = std::log (ceilingLin);
        link = juce::jlimit (0.0, 1.0, s.link);

        softClip  = st.softClip;
        clipKnee  = ceilingLin * std::pow (10.0, -1.0 / 20.0);
        clipRange = ceilingLin * std::pow (10.0, 4.0 / 20.0) - clipKnee;

        const int look    = juce::jlimit (1, delay, (int) std::lround (s.lookaheadMs * 0.001 * fs));
        const int newHold = juce::jlimit (1, delay, (int) std::lround (look * st.attackFrac));
        int nb1 = newHold, nb2 = 1;
        if (st.sCurve && newHold >= 3)
        {
            nb1 = (newHold + 1) / 2;   // two boxes, b1 + b2 - 1 == hold -> S-shaped attack
            nb2 = newHold + 1 - nb1;
        }

        if (newHold != hold || nb1 != b1 || nb2 != b2)
        {
            hold = newHold; b1 = nb1; b2 = nb2;
            rebuild();
        }

        const double rel = juce::jmax (1.0, s.releaseMs) * 0.001;
        relFast = coef (rel * st.fastRatio);
        relSlow = coef (rel);
        attSlow = coef (st.slowAttackMs * 0.001);
        programDependent = st.slowAttackMs > 0.0;
    }

    /** Processes in place. grOut receives, per host-rate sample, the deepest gain
        reduction in dB (<= 0); it must hold numSamples / factor values and be zeroed. */
    void process (juce::dsp::AudioBlock<float>& block, float* grOut, int factor) noexcept
    {
        const int nch = juce::jmin ((int) block.getNumChannels(), maxChannels);
        const int ns  = (int) block.getNumSamples();

        float* data[maxChannels] {};
        for (int c = 0; c < nch; ++c)
            data[c] = block.getChannelPointer ((size_t) c);

        constexpr double toDb = 20.0 / 2.302585092994046;
        double xs[maxChannels] {}, lr[maxChannels] {};

        for (int i = 0; i < ns; ++i)
        {
            if ((n & 0xffff) == 0)
                for (int c = 0; c < nch; ++c)
                    resyncSums (ch[c]);

            double lmin = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                double x = data[c][i];
                if (softClip)
                    x = clip (x);
                xs[c] = x;

                const double a = std::abs (x);
                lr[c] = a > ceilingLin ? ceilingLog - std::log (a) : 0.0;
                lmin = std::min (lmin, lr[c]);
            }

            if (nch > 1 && link > 0.0)
                for (int c = 0; c < nch; ++c)
                    lr[c] = link * lmin + (1.0 - link) * lr[c];

            const auto w = (size_t) (n & mask);
            double gMin = 0.0;

            for (int c = 0; c < nch; ++c)
            {
                auto& s = ch[c];
                s.r[w] = lr[c];

                // 1) sliding minimum over the last `hold` samples (monotonic deque)
                while (s.dqTail > s.dqHead && s.dqVal[(size_t) ((s.dqTail - 1) & mask)] >= lr[c])
                    --s.dqTail;
                s.dqVal[(size_t) (s.dqTail & mask)] = lr[c];
                s.dqIdx[(size_t) (s.dqTail & mask)] = n;
                ++s.dqTail;
                while (s.dqIdx[(size_t) (s.dqHead & mask)] <= n - hold)
                    ++s.dqHead;
                const double h = s.dqVal[(size_t) (s.dqHead & mask)];

                // 2) release: fast stage + slow stage that only follows sustained reduction
                s.envF = h < s.envF ? h : h + (s.envF - h) * relFast;
                double e = s.envF;
                if (programDependent)
                {
                    s.envS = s.envF + (s.envS - s.envF) * (s.envF < s.envS ? attSlow : relSlow);
                    e = std::min (s.envF, s.envS);
                }
                s.env[w] = e;

                // 3) smoothing: cascaded moving averages
                s.sum1 += e - s.env[(size_t) ((n - b1) & mask)];
                const double g1 = s.sum1 / b1;
                s.box1[w] = g1;
                s.sum2 += g1 - s.box1[(size_t) ((n - b2) & mask)];
                s.gain[w] = std::min (0.0, s.sum2 / b2);
                s.x[w] = xs[c];

                // 4) apply to the delayed audio
                const double ga = s.gain[(size_t) ((n - delay + hold - 1) & mask)];
                double y = s.x[(size_t) ((n - delay) & mask)];
                if (ga < 0.0)
                    y *= std::exp (ga);

                data[c][i] = (float) y;
                gMin = std::min (gMin, ga);
            }

            if (grOut != nullptr)
            {
                auto& dst = grOut[i / factor];
                dst = std::min (dst, (float) (gMin * toDb));
            }

            ++n;
        }
    }

private:
    struct Style
    {
        double attackFrac;   // portion of the lookahead used for the gain ramp
        bool   sCurve;       // two cascaded averages (smooth) or one (linear ramp)
        double fastRatio;    // fast release = release * fastRatio
        double slowAttackMs; // how quickly the slow stage picks up sustained reduction (0 = off)
        bool   softClip;     // soft-saturate peaks before the limiter
    };

    static constexpr Style styles[numStyles] {
        { 1.00, true,  0.30,  80.0, false }, // Transparent
        { 0.50, true,  0.15, 150.0, false }, // Punchy
        { 0.30, false, 0.10, 300.0, true  }, // Dense
        { 1.00, true,  1.00,   0.0, false }, // Safe
    };

    struct Channel
    {
        std::vector<double> r, env, box1, gain, x, dqVal;
        std::vector<int64_t> dqIdx;
        int64_t dqHead = 0, dqTail = 0;
        double envF = 0.0, envS = 0.0, sum1 = 0.0, sum2 = 0.0;
    };

    double coef (double seconds) const noexcept
    {
        return std::exp (-1.0 / (juce::jmax (1.0e-5, seconds) * fs));
    }

    double clip (double x) const noexcept
    {
        const double a = std::abs (x);
        if (a <= clipKnee)
            return x;
        const double y = clipKnee + clipRange * std::tanh ((a - clipKnee) / clipRange);
        return x < 0.0 ? -y : y;
    }

    void resyncSums (Channel& s) noexcept
    {
        s.sum1 = 0.0;
        for (int j = 1; j <= b1; ++j) s.sum1 += s.env[(size_t) ((n - j) & mask)];
        s.sum2 = 0.0;
        for (int j = 1; j <= b2; ++j) s.sum2 += s.box1[(size_t) ((n - j) & mask)];
    }

    void rebuild() noexcept
    {
        for (auto& s : ch)
        {
            s.dqHead = s.dqTail = 0;
            for (int64_t k = n - hold + 1; k < n; ++k)
            {
                const double v = s.r[(size_t) (k & mask)];
                while (s.dqTail > s.dqHead && s.dqVal[(size_t) ((s.dqTail - 1) & mask)] >= v)
                    --s.dqTail;
                s.dqVal[(size_t) (s.dqTail & mask)] = v;
                s.dqIdx[(size_t) (s.dqTail & mask)] = k;
                ++s.dqTail;
            }
            resyncSums (s);
        }
    }

    double fs = 44100.0;
    int delay = 1, hold = 1, b1 = 1, b2 = 1;
    int64_t ringSize = 1, mask = 0, n = 0;
    std::array<Channel, maxChannels> ch;

    double ceilingLin = 1.0, ceilingLog = 0.0, link = 1.0;
    double relFast = 0.0, relSlow = 0.0, attSlow = 0.0;
    bool programDependent = true, softClip = false;
    double clipKnee = 1.0, clipRange = 1.0;
};
