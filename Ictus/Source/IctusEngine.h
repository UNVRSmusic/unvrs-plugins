#pragma once

// Ictus synthesis engine: exciters (mallet + noise) -> two modal resonators (parallel or serial).
// Each mode is a damped complex rotation (z <- z * r e^{iw} + g x), which keeps tuning exact
// in float, preserves amplitude when the pitch moves, and vectorises cleanly.

#include <juce_dsp/juce_dsp.h>
#include "ModalModels.h"

namespace ictus
{
struct Svf   // Zavalishin TPT state variable filter
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set (double fc, double q, double fs)
    {
        const double g = std::tan (pi * std::clamp (fc, 10.0, 0.45 * fs) / fs);
        k = (float) (1.0 / q);
        a1 = (float) (1.0 / (1.0 + g * (g + k)));
        a2 = (float) (g * a1);
        a3 = (float) (g * a2);
    }
    void reset() { ic1 = ic2 = 0; }
    void tick (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
};

struct ModalBank
{
    using V = juce::dsp::SIMDRegister<float>;
    static constexpr int W = (int) V::SIMDNumElements;

    alignas (32) std::array<float, maxModes> re {}, im {}, cr {}, ci {}, gin {}, gl {}, gr {};
    int active = 0;

    void clear() { re.fill (0.0f); im.fill (0.0f); }

    // f0: frequency of ratio 1 in Hz, t60: decay of ratio 1 in seconds
    void setup (const Layout& L, double f0, double t60, double fs)
    {
        const double lim = std::min (0.45 * fs, 20000.0);
        int cnt = 0;
        for (int i = 0; i < L.count; ++i)
        {
            const double f = f0 * L.ratio[(size_t) i];
            if (f >= lim) break;
            const double taper = std::clamp ((lim - f) / (0.12 * lim), 0.0, 1.0) * std::clamp ((f - 10.0) / 10.0, 0.0, 1.0);
            const double w = 2.0 * pi * f / fs;
            const double t = std::clamp (t60 * L.decay[(size_t) i], 0.002, 90.0);
            const double r = std::exp (-6.907755 / (t * fs));
            cr[(size_t) i] = (float) (r * std::cos (w));
            ci[(size_t) i] = (float) (r * std::sin (w));
            gin[(size_t) i] = L.exc[(size_t) i];
            gl[(size_t) i] = (float) (L.outL[(size_t) i] * taper);
            gr[(size_t) i] = (float) (L.outR[(size_t) i] * taper);
            cnt = i + 1;
        }
        const int newActive = std::min (maxModes, (cnt + W - 1) / W * W);
        for (int i = cnt; i < maxModes; ++i)
        {
            cr[(size_t) i] = ci[(size_t) i] = gin[(size_t) i] = gl[(size_t) i] = gr[(size_t) i] = 0.0f;
            if (i >= newActive) re[(size_t) i] = im[(size_t) i] = 0.0f;
        }
        active = newActive;
    }

    void process (const float* in, float* outL, float* outR, int n) noexcept
    {
        float* pre = re.data(); float* pim = im.data();
        const float* pcr = cr.data(); const float* pci = ci.data();
        const float* pg = gin.data(); const float* pl = gl.data(); const float* pr = gr.data();
        for (int s = 0; s < n; ++s)
        {
            const V x = V::expand (in[s]);
            V accL = V::expand (0.0f), accR = V::expand (0.0f);
            for (int k = 0; k < active; k += W)
            {
                const V r = V::fromRawArray (pre + k), i = V::fromRawArray (pim + k);
                const V c = V::fromRawArray (pcr + k), sn = V::fromRawArray (pci + k);
                const V nr = r * c - i * sn + V::fromRawArray (pg + k) * x;
                const V ni = r * sn + i * c;
                nr.copyToRawArray (pre + k);
                ni.copyToRawArray (pim + k);
                accL += ni * V::fromRawArray (pl + k);
                accR += ni * V::fromRawArray (pr + k);
            }
            outL[s] = accL.sum();
            outR[s] = accR.sum();
        }
    }

    float energy() const noexcept
    {
        float e = 0.0f;
        for (int k = 0; k < active; ++k)
            e += (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]) * (gl[(size_t) k] * gl[(size_t) k] + gr[(size_t) k] * gr[(size_t) k]);
        return e;
    }
};

//==============================================================================
struct Settings
{
    bool malletOn = true;
    float malletVol = 0.8f, malletStiff = 0.5f, malletVelStiff = 0.5f, malletNoise = 0.0f, malletColor = 0.5f;

    bool noiseOn = false;
    int noiseType = 1;   // 0 LP, 1 BP, 2 HP
    float noiseVol = 0.5f, noiseFreq = 2000.0f, noiseRes = 0.3f, noiseKey = 0.0f;
    float noiseA = 0.01f, noiseD = 0.3f, noiseS = 0.0f, noiseR = 0.2f;

    struct Res
    {
        bool on = true;
        ResParams rp;
        float tune = 0.0f, fine = 0.0f, key = 1.0f, decay = 1.5f, damp = 0.5f, gain = 1.0f;
    } res[2];

    int structure = 0;   // 0 parallel, 1 serial (1 -> 2)
    int voices = 12;
    float velocity = 0.6f, penvAmt = 0.0f, penvTime = 0.08f, bleed = 0.0f, volume = 0.5f;
};

class Engine
{
public:
    static constexpr int maxVoices = 16;
    static constexpr int chunk = 32;
    static constexpr float malletScale = 0.35f, noiseScale = 0.07f, serialScale = 0.0018f;

    Engine() { tables(); }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& v : voices) v = Voice {};
        volumeSmoothed = settings.volume;
    }

    void setSettings (const Settings& s)
    {
        for (int r = 0; r < 2; ++r)
            if (! (s.res[r].rp == settings.res[r].rp) || layoutVersion[r] == 0)
            {
                buildLayout (s.res[r].rp, layouts[r]);
                ++layoutVersion[r];
            }
        settings = s;
    }

    void reset()
    {
        for (auto& v : voices) v = Voice {};
        sustainPedal = false;
    }

    void pitchBend (float semis) { bend = semis; }

    void sustain (bool down)
    {
        sustainPedal = down;
        if (! down)
            for (auto& v : voices)
                if (v.active && v.sustained) { v.sustained = false; release (v); }
    }

    void noteOn (int note, float velocity)
    {
        const int limit = std::clamp (settings.voices, 1, maxVoices);
        ++clock;

        for (auto& v : voices)   // re-striking a ringing note adds to its vibration, like the real thing
            if (v.active && v.note == note && v.fadeLeft == 0) { trigger (v, note, velocity, false); return; }

        if (limit == 1)
        {
            auto& v = voices[0];
            for (int i = 1; i < maxVoices; ++i) voices[(size_t) i].active = false;
            trigger (v, note, velocity, ! v.active);
            return;
        }

        int busy = 0;
        for (auto& v : voices) busy += v.active ? 1 : 0;
        if (busy < limit)
            for (auto& v : voices)
                if (! v.active) { trigger (v, note, velocity, true); return; }

        // steal: oldest released voice, else oldest held one; fade it out quickly first
        Voice* victim = nullptr;
        for (auto& v : voices)
            if (v.active && v.fadeLeft == 0 && ! v.held && (victim == nullptr || v.age < victim->age)) victim = &v;
        if (victim == nullptr)
            for (auto& v : voices)
                if (v.active && v.fadeLeft == 0 && (victim == nullptr || v.age < victim->age)) victim = &v;
        if (victim == nullptr) return;
        victim->fadeLeft = fadeLen;
        victim->pendingNote = note;
        victim->pendingVel = velocity;
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
        {
            if (v.active && v.pendingNote == note) v.pendingHeld = false;
            if (v.active && v.note == note && v.held)
            {
                v.held = false;
                if (sustainPedal) v.sustained = true;
                else release (v);
            }
        }
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active && v.held) { v.held = false; release (v); }
    }

    // replaces the contents of L/R
    void render (float* L, float* R, int n)
    {
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        for (int off = 0; off < n; off += chunk)
        {
            const int len = std::min (chunk, n - off);
            for (auto& v : voices)
                if (v.active)
                    renderVoice (v, L + off, R + off, len);
        }
        const float target = settings.volume;
        const float step = (target - volumeSmoothed) / (float) std::max (1, n);
        for (int i = 0; i < n; ++i)
        {
            volumeSmoothed += step;
            L[i] = softClip (L[i] * volumeSmoothed);
            R[i] = softClip (R[i] * volumeSmoothed);
        }
    }

    int activeVoices() const
    {
        int n = 0;
        for (auto& v : voices) n += v.active ? 1 : 0;
        return n;
    }

    const Layout& layout (int r) const { return layouts[r]; }

private:
    struct Voice
    {
        bool active = false, held = false, sustained = false, released = false;
        int note = 60; float vel = 1.0f; uint64_t age = 0;
        int fadeLeft = 0, pendingNote = -1; float pendingVel = 0; bool pendingHeld = true;

        int pulsePos = 0, pulseLen = 0; float pulseAmp = 0;
        float mnEnv = 0, mnDecay = 0, mnAmp = 0; Svf mnFilt;
        int noiseStage = 0; float noiseEnv = 0; Svf noiseFilt;   // 0 idle 1 attack 2 decay/sustain 3 release
        uint32_t rng = 0x9e3779b9u;
        float penv = 0.0f;
        int quiet = 0;

        ModalBank bank[2];
        uint32_t ver[2] { 0, 0 };
        double fCache[2] { 0, 0 }, tCache[2] { 0, 0 };

        float white() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) ((int32_t) rng) * (1.0f / 2147483648.0f); }
    };

    static float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
    }

    void release (Voice& v)
    {
        v.released = true;
        if (v.noiseStage != 0) v.noiseStage = 3;
    }

    void trigger (Voice& v, int note, float velocity, bool fresh)
    {
        if (fresh)
        {
            const auto seed = v.rng;
            v = Voice {};
            v.rng = seed * 1664525u + 1013904223u + (uint32_t) note;
        }
        v.active = true; v.held = true; v.sustained = false; v.released = false;
        v.note = note; v.vel = std::clamp (velocity, 0.0f, 1.0f); v.age = clock; v.quiet = 0;
        v.fadeLeft = 0; v.pendingNote = -1;
        v.penv = 1.0f;

        const auto& s = settings;
        const float vg = std::pow (std::max (v.vel, 0.01f), 2.0f * s.velocity);

        if (s.malletOn)
        {
            const float stiff = std::clamp (s.malletStiff + s.malletVelStiff * (v.vel - 0.6f) * 0.9f, 0.0f, 1.0f);
            const double wsec = 0.006 * std::pow (0.08 / 6.0, (double) stiff);       // contact time 6 ms .. 0.08 ms
            v.pulseLen = std::max (1, (int) std::lround (wsec * fs));
            const float area = s.malletVol * vg * malletScale;
            v.pulseAmp = area * (float) (pi * 0.5) / (float) v.pulseLen;
            v.pulsePos = 0;

            if (s.malletNoise > 0.0f)
            {
                const double tau = wsec * 0.5 + 0.002;
                const double fc = 300.0 * std::pow (2.0, 6.0 * s.malletColor);
                v.mnFilt.set (fc, 0.707, fs);
                v.mnDecay = (float) std::exp (-1.0 / (tau * fs));
                v.mnEnv = 1.0f;
                v.mnAmp = (float) (s.malletNoise * area * std::sqrt (3.0 / (tau * fs)) * std::pow (0.45 * fs / fc, 0.25));
            }
        }
        else v.pulseLen = 0;

        if (s.noiseOn)
        {
            v.noiseStage = 1;
            if (fresh) v.noiseEnv = 0.0f;
        }
        else v.noiseStage = 0;
    }

    double resF0 (const Voice& v, int r) const
    {
        const auto& rs = settings.res[r];
        const double semis = (v.note - 60) * rs.key + rs.tune + rs.fine * 0.01 + bend + settings.penvAmt * v.penv;
        return 261.6255653 * std::pow (2.0, semis / 12.0);
    }

    double resT60 (const Voice& v, int r, double f0) const
    {
        const auto& rs = settings.res[r];
        double t = std::clamp ((double) rs.decay * std::pow (f0 / 261.6255653, -0.3), 0.005, 60.0);
        if (v.released)
        {
            const double off = std::min (t, 0.04);
            t = std::exp ((1.0 - rs.damp) * std::log (t) + rs.damp * std::log (off));
        }
        return t;
    }

    void renderVoice (Voice& v, float* L, float* R, int n)
    {
        const auto& s = settings;
        const bool serial = s.structure == 1;

        // ---- control rate
        int refRes = -1;
        double refT60 = 1.0;
        for (int r = 0; r < 2; ++r)
        {
            if (! s.res[r].on) continue;
            const double f0 = resF0 (v, r), t60 = resT60 (v, r, f0);
            if (refRes < 0) { refRes = r; refT60 = t60; }
            if (v.ver[r] != layoutVersion[r] || std::abs (f0 / v.fCache[r] - 1.0) > 1e-7 || std::abs (t60 / v.tCache[r] - 1.0) > 1e-4)
            {
                v.bank[r].setup (layouts[r], f0, t60, fs);
                v.ver[r] = layoutVersion[r]; v.fCache[r] = f0; v.tCache[r] = t60;
            }
        }
        if (s.penvAmt != 0.0f)
            v.penv *= (float) std::exp (-(double) n / (std::max (0.001f, s.penvTime) * fs));
        else
            v.penv = 0.0f;

        // ---- exciters
        float exc[chunk];
        const float vg = std::pow (std::max (v.vel, 0.01f), 2.0f * s.velocity);
        float noiseGain = 0.0f;
        if (v.noiseStage != 0)
        {
            const double f0 = 261.6255653 * std::pow (2.0, (v.note - 60) / 12.0);
            const double fc = s.noiseFreq * std::pow (f0 / 261.6255653, (double) s.noiseKey);
            v.noiseFilt.set (fc, 0.5 + 19.5 * s.noiseRes * s.noiseRes, fs);
            noiseGain = s.noiseVol * vg * noiseScale * (float) std::sqrt (2.0 * 6.907755 / (refT60 * fs)) * 60.0f;
        }
        const float atk = 1.0f / (float) (std::max (0.001f, s.noiseA) * fs);
        const float dk = 1.0f - (float) std::exp (-4.0 / (std::max (0.005f, s.noiseD) * fs));
        const float rk = 1.0f - (float) std::exp (-4.0 / (std::max (0.005f, s.noiseR) * fs));
        const float bpScale = std::sqrt (0.5f + 19.5f * s.noiseRes * s.noiseRes);

        bool excActive = false;
        for (int i = 0; i < n; ++i)
        {
            float e = 0.0f;
            if (v.pulsePos < v.pulseLen)
            {
                e += v.pulseAmp * std::sin ((float) pi * ((float) v.pulsePos + 0.5f) / (float) v.pulseLen);
                ++v.pulsePos;
            }
            if (v.mnEnv > 1e-4f)
            {
                float lp, bp, hp;
                v.mnFilt.tick (v.white(), lp, bp, hp);
                e += v.mnAmp * v.mnEnv * lp;
                v.mnEnv *= v.mnDecay;
            }
            if (v.noiseStage != 0)
            {
                switch (v.noiseStage)
                {
                    case 1: v.noiseEnv += atk; if (v.noiseEnv >= 1.0f) { v.noiseEnv = 1.0f; v.noiseStage = 2; } break;
                    case 2: v.noiseEnv += (s.noiseS - v.noiseEnv) * dk; if (s.noiseS < 1e-4f && v.noiseEnv < 1e-5f) v.noiseStage = 0; break;
                    case 3: v.noiseEnv -= v.noiseEnv * rk; if (v.noiseEnv < 1e-5f) v.noiseStage = 0; break;
                    default: break;
                }
                float lp, bp, hp;
                v.noiseFilt.tick (v.white(), lp, bp, hp);
                const float filtered = s.noiseType == 0 ? lp : (s.noiseType == 2 ? hp : bp * v.noiseFilt.k * bpScale);
                e += filtered * v.noiseEnv * noiseGain;
            }
            exc[i] = e;
        }
        excActive = v.pulsePos < v.pulseLen || v.mnEnv > 1e-4f || v.noiseStage != 0;

        // ---- resonators
        float o1L[chunk], o1R[chunk], o2L[chunk], o2R[chunk], in2[chunk];
        const bool on1 = s.res[0].on, on2 = s.res[1].on;
        if (on1) v.bank[0].process (exc, o1L, o1R, n);
        if (on2)
        {
            if (serial && on1)
                for (int i = 0; i < n; ++i) in2[i] = serialScale * (o1L[i] + o1R[i]);
            else
                std::copy (exc, exc + n, in2);
            v.bank[1].process (in2, o2L, o2R, n);
        }

        const float g1 = on1 ? s.res[0].gain : 0.0f, g2 = on2 ? s.res[1].gain : 0.0f;
        const float bl = s.bleed * 0.5f;
        for (int i = 0; i < n; ++i)
        {
            float l = bl * exc[i], r = bl * exc[i];
            if (on1) { l += g1 * o1L[i]; r += g1 * o1R[i]; }
            if (on2) { l += g2 * o2L[i]; r += g2 * o2R[i]; }
            if (v.fadeLeft > 0)
            {
                const float f = (float) v.fadeLeft / (float) fadeLen;
                l *= f; r *= f;
                if (v.fadeLeft > 1) --v.fadeLeft;
            }
            L[i] += l; R[i] += r;
        }

        // ---- stealing / end of life
        if (v.fadeLeft == 1)
        {
            const int note = v.pendingNote; const float vel = v.pendingVel; const bool held = v.pendingHeld;
            trigger (v, note, vel, true);
            if (! held) { v.held = false; if (sustainPedal) v.sustained = true; else release (v); }
            return;
        }

        const float en = (on1 ? v.bank[0].energy() : 0.0f) + (on2 ? v.bank[1].energy() : 0.0f);
        if (! excActive && en < 1e-11f)
        {
            if (++v.quiet > 4) v.active = false;
        }
        else v.quiet = 0;
    }

    double fs = 48000.0;
    Settings settings;
    std::array<Layout, 2> layouts;
    uint32_t layoutVersion[2] { 0, 0 };
    std::array<Voice, maxVoices> voices;
    float bend = 0.0f, volumeSmoothed = 0.5f;
    bool sustainPedal = false;
    uint64_t clock = 0;
    static constexpr int fadeLen = 96;
};
} // namespace ictus
