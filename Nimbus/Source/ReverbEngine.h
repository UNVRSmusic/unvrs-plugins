#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

/*
    Nimbus reverb core.

      in -> predelay -> early reflections (multi-tap) ------------------------------+
                     -> input diffusion (4 allpasses / ch) -> bloom diffusion        |
                     -> 16-line feedback delay network:                              |
                          modulated delay (Hermite) -> allpass -> 3-band decay       |
                          -> Hadamard mix -> back into the lines                     |
                     -> orthogonal stereo taps -----------------------------------> + -> low/high cut -> width -> duck -> mix

    Decay is set per line from its length, so the reverb time is exact and
    independent of size. The 3-band decay filter is built from first-order
    shelves whose gain never exceeds the largest band gain, so the loop is stable
    for any setting (and in freeze every gain is unity).
*/
class ReverbEngine
{
public:
    static constexpr int N = 16;
    enum Character { room = 0, hall, plate, cathedral, ambient, numCharacters };

    struct Params
    {
        int   character   = hall;
        float predelayMs  = 20.0f;
        float size        = 0.55f;   // 0..1
        float decayS      = 2.8f;
        float diffusion   = 0.8f;    // 0..1
        float bloom       = 0.2f;    // 0..1
        float early       = 0.4f;    // 0..1
        float bassMult    = 1.2f;    // low RT = decay * bassMult
        float dampHz      = 5000.0f; // crossover above which trebleMult applies
        float trebleMult  = 0.45f;   // high RT = decay * trebleMult
        float modDepth    = 0.45f;   // 0..1
        float modRate     = 0.6f;    // Hz
        float lowCutHz    = 120.0f;
        float highCutHz   = 12000.0f;
        float width       = 1.0f;    // 0..2
        float duck        = 0.0f;    // 0..1
        float mix         = 0.3f;    // 0..1
        float shimmer     = 0.0f;    // 0..1, share of the feedback that is pitch-shifted
        int   shimmerPitch = 0;      // index into shimmerSemitones
        bool  freeze      = false;
    };

    struct Profile
    {
        float minMs, maxMs;   // delay-line spread at size 1
        float erLevel, erSpanMs;
        float loopAp;         // in-loop allpass gain at full diffusion
        float inDiff;         // input diffusion gain scale
        float modScale;
        float bloomBase;
    };

    static const Profile& profile (int c)
    {
        static const Profile p[numCharacters] {
            { 12.0f,  48.0f, 1.00f,  38.0f, 0.50f, 0.70f, 0.50f, 0.00f }, // Room
            { 31.0f, 113.0f, 0.60f,  70.0f, 0.60f, 0.78f, 1.00f, 0.00f }, // Hall
            {  9.0f,  41.0f, 0.00f,   0.0f, 0.70f, 0.90f, 0.75f, 0.00f }, // Plate
            { 53.0f, 187.0f, 0.50f, 110.0f, 0.62f, 0.78f, 1.10f, 0.20f }, // Cathedral
            { 79.0f, 251.0f, 0.25f, 120.0f, 0.66f, 0.85f, 1.60f, 0.45f }, // Ambient
        };
        return p[juce::jlimit (0, numCharacters - 1, c)];
    }

    static double sizeMultiplier (float size) { return 0.5 + size; }

    static constexpr int numShimmerPitches = 6;
    static double shimmerSemitones (int i)
    {
        static constexpr double st[numShimmerPitches] { 12.0, 7.0, 19.0, 24.0, -12.0, 5.0 };
        return st[juce::jlimit (0, numShimmerPitches - 1, i)];
    }

    //==============================================================================
    void prepare (double sampleRate)
    {
        fs = sampleRate;

        for (auto& l : lines) l.init ((int) (fs * 0.6));
        for (auto& a : loopAp) a.init ((int) (fs * 0.012));
        for (auto& ch : inAp) for (auto& a : ch) a.init ((int) (fs * 0.016));
        for (auto& ch : bloomAp) for (auto& a : ch) a.init ((int) (fs * 0.08));
        for (auto& p : pre) p.init ((int) (fs * 0.7));
        for (auto& sh : shifters) sh.buf.init ((int) (fs * 0.1));

        static constexpr float inMs[2][4]    { { 4.771f, 3.595f, 12.73f, 9.307f }, { 4.919f, 3.371f, 13.11f, 8.893f } };
        static constexpr float bloomMs[2][3] { { 31.3f, 47.9f, 71.1f }, { 37.1f, 53.3f, 66.7f } };
        for (int c = 0; c < 2; ++c)
        {
            for (int j = 0; j < 4; ++j) inLen[c][j] = ms (inMs[c][j]);
            for (int j = 0; j < 3; ++j) bloomLen[c][j] = ms (bloomMs[c][j]);
        }

        static constexpr int perm[N] { 0, 9, 4, 13, 2, 11, 6, 15, 1, 8, 5, 12, 3, 10, 7, 14 };
        juce::Random rnd (0x5eed);
        for (int i = 0; i < N; ++i)
        {
            spread[i]  = (float) perm[i] / (N - 1);
            jitter[i]  = 1.0f + 0.04f * (rnd.nextFloat() - 0.5f);
            loopApLen[i] = ms (2.3f * std::pow (9.1f / 2.3f, (float) perm[(i + 5) % N] / (N - 1)));
            injSign[i] = rnd.nextBool() ? 1.0f : -1.0f;
            lfoPhase0[i] = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
            lfoRateMul[i] = 0.62f + 0.76f * (float) perm[(i + 3) % N] / (N - 1);
            shifters[(size_t) i].window = (int) std::lround ((0.05 + 0.03 * (double) perm[(i + 7) % N] / (N - 1)) * fs);
            shifters[(size_t) i].phase = (double) perm[(i + 11) % N] / N;
        }

        for (int c = 0; c < 2; ++c)
        {
            float energy = 0.0f;
            for (int k = 0; k < numEr; ++k)
            {
                erFrac[c][k] = std::pow ((k + 1.0f) / numEr, 1.35f) * (0.9f + 0.2f * rnd.nextFloat());
                erGain[c][k] = (1.0f - 0.75f * (float) k / numEr) * (rnd.nextBool() ? 1.0f : -1.0f);
                energy += erGain[c][k] * erGain[c][k];
            }
            for (int k = 0; k < numEr; ++k)
                erGain[c][k] /= std::sqrt (energy);
        }

        kSmooth = 1.0 - std::exp (-1.0 / (0.02 * fs));
        kGlide  = 1.0 - std::exp (-1.0 / (0.25 * fs));
        kFade   = 1.0f / (float) (0.02 * fs);
        lowXover = tptCoef (250.0);

        currentCharacter = params.character;
        pendingCharacter = -1;
        fade = 1.0f;
        applyCharacter();
        reset();
        updateTargets();
        snapSmoothers();
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& a : loopAp) a.clear();
        for (auto& ch : inAp) for (auto& a : ch) a.clear();
        for (auto& ch : bloomAp) for (auto& a : ch) a.clear();
        for (auto& p : pre) p.clear();
        for (auto& sh : shifters) { sh.buf.clear(); sh.hp = sh.lp = 0.0f; }
        for (auto& s : dampState) s = {};
        for (auto& s : eqState) s = {};
        erLp = { 0.0f, 0.0f };
        duckEnv = 0.0f;
        for (int i = 0; i < N; ++i)
            osc[i] = std::polar (1.0, (double) lfoPhase0[i]);
    }

    void setParams (const Params& p)
    {
        params = p;
        updateTargets();
    }

    /** RT60 the network produces at a given frequency (for the GUI curve). */
    static double rtAt (double f, const Params& p)
    {
        if (p.freeze)
            return 1.0e9;

        const auto& pr = profile (p.character);
        const double de = 0.5 * (pr.minMs + pr.maxMs) * sizeMultiplier (p.size) * 0.001 + 0.005;
        const double gM = std::pow (10.0, -3.0 * de / p.decayS);
        const double gL = std::pow (10.0, -3.0 * de / (p.decayS * p.bassMult));
        const double gH = std::pow (10.0, -3.0 * de / (p.decayS * p.trebleMult));

        const std::complex<double> jl (0.0, f / 250.0), jh (0.0, f / p.dampHz);
        const auto low  = gM + (gL - gM) / (1.0 + jl);
        const auto high = 1.0 + (gH / gM - 1.0) * jh / (1.0 + jh);
        const double mag = std::abs (low * high);
        return -3.0 * de / std::log10 (juce::jlimit (1.0e-9, 0.999999, mag));
    }

    /** Processes a stereo pair in place. */
    void process (float* L, float* R, int n) noexcept
    {
        const auto& pr = profile (currentCharacter);

        for (int s = 0; s < n; ++s)
        {
            const float dl = L[s], dr = R[s];

            // ---- smoothed controls -------------------------------------------------------------
            mixCur  += (mixTarget - mixCur) * (float) kSmooth;
            injCur  += (injTarget - injCur) * (float) kSmooth;
            erCur   += (erTarget - erCur) * (float) kSmooth;
            bloomCur += (bloomTarget - bloomCur) * (float) kSmooth;
            shimCur += (shimTarget - shimCur) * (float) kSmooth;
            // power-complementary crossfade: the shifted copy is uncorrelated with the tail, so
            // cos/sin keeps the energy; 1.1 makes up most of the Hann overlap loss (energy 0.75)
            const float shimCos = std::cos (shimCur), shimSin = std::sin (shimCur) * 1.1f;
            widthCur += (widthTarget - widthCur) * (float) kSmooth;
            predCur += (predTarget - predCur) * kGlide;
            modDepthSamples += (modTarget - modDepthSamples) * kGlide;
            if (modTarget == 0.0 && modDepthSamples < 1.0e-6) modDepthSamples = 0.0;
            sizeCur += (sizeTarget - sizeCur) * kGlide;

            // character change: fade the wet path out, rebuild, fade back in
            if (pendingCharacter >= 0)
            {
                fade = juce::jmax (0.0f, fade - kFade);
                if (fade <= 0.0f)
                {
                    currentCharacter = pendingCharacter;
                    pendingCharacter = -1;
                    applyCharacter();
                    reset();
                    updateTargets();
                    snapSmoothers();
                }
            }
            else
                fade = juce::jmin (1.0f, fade + kFade);

            // ---- predelay + early reflections ---------------------------------------------------
            pre[0].push (dl);
            pre[1].push (dr);
            const float pl = pre[0].tapLinear (predCur + 1.0);
            const float prr = pre[1].tapLinear (predCur + 1.0);

            float er[2] { 0.0f, 0.0f };
            if (erCur > 1.0e-4f && pr.erSpanMs > 0.0f)
            {
                const double span = pr.erSpanMs * 0.001 * fs * sizeCur;
                for (int c = 0; c < 2; ++c)
                {
                    float acc = 0.0f;
                    for (int k = 0; k < numEr; ++k)
                        acc += erGain[c][k] * pre[c].tapLinear (predCur + 1.0 + erFrac[c][k] * span);
                    erLp[c] += (acc - erLp[c]) * erLpCoef;
                    er[c] = erLp[c] * erCur;
                }
            }

            // ---- input diffusion + bloom --------------------------------------------------------
            float x[2] { pl, prr };
            for (int c = 0; c < 2; ++c)
            {
                for (int j = 0; j < 4; ++j)
                    x[c] = inAp[c][j].process (x[c], inLen[c][j], j < 2 ? inG1 : inG2);

                if (bloomCur > 1.0e-4f)
                {
                    float b = x[c];
                    for (int j = 0; j < 3; ++j)
                        b = bloomAp[c][j].process (b, bloomLen[c][j], 0.7f);
                    x[c] += (b - x[c]) * bloomCur;
                }
            }

            // ---- feedback delay network ------------------------------------------------------------
            float y[N];
            float wet[2] { 0.0f, 0.0f };
            for (int i = 0; i < N; ++i)
            {
                dCur[i] += (dTarget[i] - dCur[i]) * kGlide;
                osc[i] *= oscStep[i];
                const double d = dCur[i] + modDepthSamples * osc[i].imag();
                y[i] = lines[i].tapHermite (d);
                wet[0] += outL[i] * y[i];
                wet[1] += outR[i] * y[i];
            }

            if ((++renorm & 1023) == 0)
                for (auto& o : osc) o /= std::abs (o);

            for (int i = 0; i < N; ++i)
            {
                float v = loopAp[i].process (y[i], loopApLen[i], loopApG);

                // 3-band decay: gM*x + (gL-gM)*LP_low(x), then a high shelf of ratio gH/gM
                auto& st = dampState[i];
                const float lowLp = tpt (st.low, v, lowXover);
                v = gMid[i] * v + gLowDelta[i] * lowLp;
                const float hiLp = tpt (st.high, v, highXover);
                v += gHighRatio[i] * (v - hiLp);

                // shimmer: crossfade part of the feedback with a pitch-shifted copy of itself.
                // Two Hann-windowed read heads 180 degrees apart; each stays <= the input level,
                // and the band-limiting filters only remove energy, so the loop can never gain.
                auto& sh = shifters[(size_t) i];
                sh.buf.push (v);
                if (shimCur > 1.0e-4f)
                {
                    sh.phase += shimPhaseInc / sh.window;
                    sh.phase -= std::floor (sh.phase);
                    float shifted = 0.0f;
                    for (int k = 0; k < 2; ++k)
                    {
                        double ph = sh.phase + 0.5 * k;
                        if (ph >= 1.0) ph -= 1.0;
                        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) ph);
                        shifted += w * sh.buf.tapHermite (2.0 + ph * sh.window);
                    }
                    sh.hp += (shifted - sh.hp) * shimHpCoef;      // one-pole high-pass (shifted - hp)
                    shifted -= sh.hp;
                    sh.lp += (shifted - sh.lp) * shimLpCoef;      // one-pole low-pass
                    v = shimCos * v + shimSin * sh.lp;
                }
                y[i] = v;
            }

            hadamard (y);

            const float inj[2] { x[0] * injCur * injScale, x[1] * injCur * injScale };
            for (int i = 0; i < N; ++i)
                lines[i].push (y[i] + injSign[i] * inj[i & 1]);

            // ---- wet post-processing ---------------------------------------------------------------
            for (int c = 0; c < 2; ++c)
            {
                float w = wet[c] * outScale + er[c];
                w = svf (eqState[c * 2],     w, lowCutG,  false); // high-pass
                w = svf (eqState[c * 2 + 1], w, highCutG, true);  // low-pass
                wet[c] = w;
            }

            const float mid = 0.5f * (wet[0] + wet[1]);
            const float side = 0.5f * (wet[0] - wet[1]) * widthCur;
            wet[0] = mid + side;
            wet[1] = mid - side;

            if (duckAmount > 0.0f)
            {
                const float lvl = juce::jmax (std::abs (dl), std::abs (dr));
                duckEnv += (lvl - duckEnv) * (lvl > duckEnv ? duckAtt : duckRel);
                const float g = 1.0f / (1.0f + duckAmount * 12.0f * duckEnv);
                wet[0] *= g;
                wet[1] *= g;
            }

            const float a = mixCur * juce::MathConstants<float>::halfPi;
            const float wetG = std::sin (a) * fade, dryG = std::cos (a);
            L[s] = dl * dryG + wet[0] * wetG;
            R[s] = dr * dryG + wet[1] * wetG;
        }
    }

private:
    //==============================================================================
    struct Delay
    {
        std::vector<float> buf;
        int mask = 0, w = 0;

        void init (int minSize)
        {
            int s = 1;
            while (s < minSize + 8) s <<= 1;
            buf.assign ((size_t) s, 0.0f);
            mask = s - 1;
            w = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
        float tap (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }

        float tapLinear (double d) const noexcept
        {
            const int i = (int) d;
            const float t = (float) (d - i);
            const float a = tap (i), b = tap (i + 1);
            return a + t * (b - a);
        }

        float tapHermite (double d) const noexcept
        {
            const int i = (int) d;
            const float t = (float) (d - i);
            const float y0 = tap (i - 1), y1 = tap (i), y2 = tap (i + 1), y3 = tap (i + 2);
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * t + c2) * t + c1) * t + y1;
        }
    };

    struct Allpass
    {
        Delay d;
        void init (int maxLen) { d.init (maxLen); }
        void clear() { d.clear(); }
        float process (float x, int len, float g) noexcept
        {
            const float out = d.tap (len);
            const float w = x + g * out;
            d.push (w);
            return out - g * w;
        }
    };

    struct OnePole { float low = 0.0f, high = 0.0f; };
    struct SvfState { float s1 = 0.0f, s2 = 0.0f; };

    static float tpt (float& z, float x, float a) noexcept
    {
        const float v = (x - z) * a;
        const float lp = v + z;
        z = lp + v;
        return lp;
    }

    float tptCoef (double hz) const
    {
        const double g = std::tan (juce::MathConstants<double>::pi * juce::jmin (hz, 0.45 * fs) / fs);
        return (float) (g / (1.0 + g));
    }

    static float svf (SvfState& st, float x, float g, bool lowpass) noexcept
    {
        constexpr float k = 1.4142135f;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        const float v3 = x - st.s2;
        const float v1 = a1 * st.s1 + a2 * v3;
        const float v2 = st.s2 + a2 * st.s1 + a3 * v3;
        st.s1 = 2.0f * v1 - st.s1;
        st.s2 = 2.0f * v2 - st.s2;
        return lowpass ? v2 : x - k * v1 - v2;
    }

    static void hadamard (float* x) noexcept
    {
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }
        for (int i = 0; i < N; ++i)
            x[i] *= 0.25f; // 1 / sqrt(16): orthonormal
    }

    int ms (float milliseconds) const { return juce::jmax (1, (int) std::lround (milliseconds * 0.001 * fs)); }

    void applyCharacter()
    {
        const auto& pr = profile (currentCharacter);
        for (int i = 0; i < N; ++i)
            baseMs[i] = pr.minMs * std::pow (pr.maxMs / pr.minMs, spread[i]) * jitter[i];

        for (int i = 0; i < N; ++i)
        {
            const float sl = (i & 1) ? -1.0f : 1.0f;
            const float sr = ((i >> 1) & 1) ^ ((i >> 2) & 1) ? -1.0f : 1.0f;
            outL[i] = sl;
            outR[i] = sr;
        }
    }

    void updateTargets()
    {
        if (params.character != currentCharacter && pendingCharacter < 0)
            pendingCharacter = params.character;

        const auto& pr = profile (currentCharacter);
        const double sm = sizeMultiplier (params.size);
        sizeTarget = sm;
        predTarget = juce::jlimit (0.0, 0.26 * fs, params.predelayMs * 0.001 * fs);

        for (int i = 0; i < N; ++i)
            dTarget[i] = std::round (juce::jlimit (4.0, fs * 0.55, baseMs[i] * sm * 0.001 * fs));

        // decay per line (uses the target length; the glide is slow enough not to matter)
        for (int i = 0; i < N; ++i)
        {
            if (params.freeze)
            {
                gMid[i] = 1.0f; gLowDelta[i] = 0.0f; gHighRatio[i] = 0.0f;
                continue;
            }
            const double de = (dTarget[i] + loopApLen[i]) / fs;
            const double rt = params.decayS * rtCompensation; // modulation & interpolation losses shorten the tail slightly
            const double gM = std::pow (10.0, -3.0 * de / rt);
            const double gL = std::pow (10.0, -3.0 * de / (rt * params.bassMult));
            const double gH = std::pow (10.0, -3.0 * de / (rt * params.trebleMult));
            gMid[i] = (float) gM;
            gLowDelta[i] = (float) (gL - gM);
            gHighRatio[i] = (float) (gH / gM - 1.0);
        }
        highXover = tptCoef (params.dampHz);

        // in freeze the modulation winds down so the (integer-length) loop is lossless
        modTarget = params.freeze ? 0.0 : params.modDepth * pr.modScale * 0.0011 * fs * juce::jmin (1.0, sm);
        for (int i = 0; i < N; ++i)
            oscStep[i] = std::polar (1.0, juce::MathConstants<double>::twoPi * params.modRate * lfoRateMul[i] / fs);

        const float diff = params.diffusion;
        inG1 = 0.75f * diff * pr.inDiff / 0.78f;
        inG2 = 0.625f * diff * pr.inDiff / 0.78f;
        inG1 = juce::jmin (inG1, 0.8f);
        inG2 = juce::jmin (inG2, 0.7f);
        loopApG = pr.loopAp * diff;

        bloomTarget = juce::jlimit (0.0f, 1.0f, params.bloom + pr.bloomBase);
        erTarget = params.early * pr.erLevel * 1.4f;
        erLpCoef = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 7000.0 / fs));

        lowCutG  = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, 0.45 * fs, (double) params.lowCutHz) / fs);
        highCutG = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (100.0, 0.45 * fs, (double) params.highCutHz) / fs);
        widthTarget = params.width;

        duckAmount = params.duck;
        duckAtt = (float) (1.0 - std::exp (-1.0 / (0.005 * fs)));
        duckRel = (float) (1.0 - std::exp (-1.0 / (0.30 * fs)));

        mixTarget = params.mix;
        shimTarget = juce::jlimit (0.0f, 1.0f, params.shimmer) * juce::MathConstants<float>::pi * 0.25f; // crossfade angle, max 45 deg
        shimPhaseInc = 1.0 - std::pow (2.0, shimmerSemitones (params.shimmerPitch) / 12.0);
        shimHpCoef = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 180.0 / fs));
        shimLpCoef = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 7500.0 / fs));
        injTarget = params.freeze ? 0.0f : 1.0f;
    }

    void snapSmoothers()
    {
        mixCur = mixTarget; shimCur = shimTarget; injCur = injTarget; erCur = erTarget; bloomCur = bloomTarget;
        widthCur = widthTarget; predCur = predTarget; sizeCur = sizeTarget; modDepthSamples = modTarget;
        for (int i = 0; i < N; ++i) dCur[i] = dTarget[i];
    }

    //==============================================================================
    static constexpr int numEr = 12;
    static constexpr double rtCompensation = 1.075;

    double fs = 48000.0;
    Params params;
    int currentCharacter = hall, pendingCharacter = -1;
    float fade = 1.0f, kFade = 0.001f;

    std::array<Delay, N> lines;
    std::array<Allpass, N> loopAp;
    std::array<std::array<Allpass, 4>, 2> inAp;
    std::array<std::array<Allpass, 3>, 2> bloomAp;
    std::array<Delay, 2> pre;

    struct Shifter { Delay buf; double phase = 0.0; int window = 2400; float hp = 0.0f, lp = 0.0f; };
    std::array<Shifter, N> shifters;
    float shimCur = 0.0f, shimTarget = 0.0f, shimHpCoef = 0.01f, shimLpCoef = 0.5f;
    double shimPhaseInc = -1.0;

    int inLen[2][4] {}, bloomLen[2][3] {}, loopApLen[N] {};
    float spread[N] {}, jitter[N] {}, baseMs[N] {}, injSign[N] {}, lfoPhase0[N] {}, lfoRateMul[N] {};
    float outL[N] {}, outR[N] {};
    float erFrac[2][numEr] {}, erGain[2][numEr] {};
    std::array<float, 2> erLp {};
    float erLpCoef = 0.5f;

    double dCur[N] {}, dTarget[N] {};
    std::complex<double> osc[N], oscStep[N];
    double modDepthSamples = 0.0, modTarget = 0.0;
    int renorm = 0;

    float gMid[N] {}, gLowDelta[N] {}, gHighRatio[N] {};
    float lowXover = 0.1f, highXover = 0.3f;
    std::array<OnePole, N> dampState {};

    float inG1 = 0.75f, inG2 = 0.625f, loopApG = 0.5f;
    float lowCutG = 0.01f, highCutG = 1.0f;
    std::array<SvfState, 4> eqState {};

    float duckAmount = 0.0f, duckEnv = 0.0f, duckAtt = 0.01f, duckRel = 0.001f;

    // injection / output scaling (orthonormal network: unit-energy taps)
    static constexpr float injScale = 0.35f;
    static constexpr float outScale = 0.25f;

    double kSmooth = 0.001, kGlide = 0.0001;
    float mixCur = 0.3f, mixTarget = 0.3f, injCur = 1.0f, injTarget = 1.0f;
    float erCur = 0.0f, erTarget = 0.0f, bloomCur = 0.0f, bloomTarget = 0.0f;
    float widthCur = 1.0f, widthTarget = 1.0f;
    double predCur = 0.0, predTarget = 0.0, sizeCur = 1.0, sizeTarget = 1.0;
};
