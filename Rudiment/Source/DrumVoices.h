#pragma once

// Rudiment drum synthesis. Each element is a monophonic voice pair: a new hit starts in
// the free slot while the previous one fades out in 4 ms, so re-triggers never click.

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <memory>
#include <vector>
#include <type_traits>
#include "Specs.h"

namespace rud
{
constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = 2.0 * pi;

inline double t60Coef (double seconds, double fs) { return std::exp (-6.907755 / (std::max (1e-4, seconds) * fs)); }

struct Rng
{
    uint32_t s = 0x12345678u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
};

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
    float lp (float x) { float l, b, h; tick (x, l, b, h); return l; }
    float hp (float x) { float l, b, h; tick (x, l, b, h); return h; }
    float bpNorm (float x) { float l, b, h; tick (x, l, b, h); return b * k; }   // unity gain at the centre
};

inline float polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

struct BlepSquare
{
    double phase = 0.0, inc = 0.0;
    float tick()
    {
        float v = phase < 0.5 ? 1.0f : -1.0f;
        v += polyBlep (phase, inc);
        double t2 = phase + 0.5; if (t2 >= 1.0) t2 -= 1.0;
        v -= polyBlep (t2, inc);
        phase += inc; if (phase >= 1.0) phase -= 1.0;
        return v;
    }
};

inline float drive (float x, float amount)   // 0..1, unity peak at full scale
{
    if (amount <= 0.001f) return x;
    const float g = 1.0f + 9.0f * amount * amount;
    return std::tanh (g * x) / std::tanh (g);
}

inline float velGain (float vel) { return std::pow (std::clamp (vel, 0.0f, 1.0f), 1.4f); }

//==============================================================================
struct VoiceCommon
{
    bool active = false, fading = false;
    float fade = 1.0f, fadeStep = 0.0f;
    double fs = 48000.0;
    Rng rng;

    void beginFade (double seconds)
    {
        if (active && ! fading) { fading = true; fade = 1.0f; fadeStep = (float) (1.0 / (seconds * fs)); }
    }
    // returns false when the voice just finished fading
    bool fadeGain (float& g)
    {
        if (! fading) { g = 1.0f; return true; }
        fade -= fadeStep;
        if (fade <= 0.0f) { active = false; fading = false; return false; }
        g = fade;
        return true;
    }
    void begin() { active = true; fading = false; fade = 1.0f; }
};

//==============================================================================
struct KickVoice : VoiceCommon
{
    double phase = 0, f0 = 50, semis = 0, sweepCoef = 0;
    double env = 0, dec = 0; int holdLeft = 0;
    float clickEnv = 0, clickDec = 0, clickAmp = 0, amp = 1, drv = 0;
    bool useTone = false;
    Svf clickFilt, toneFilt;

    void start (const float* p, float vel, float pitchOffset)
    {
        begin();
        phase = 0.0;
        f0 = p[kTune] * std::pow (2.0, pitchOffset / 12.0);
        semis = p[kPitch] * (0.75 + 0.25 * vel);                    // harder hit = higher start pitch
        sweepCoef = std::exp (-1.0 / (std::max (0.001f, p[kSweep]) * fs));
        env = 1.0; dec = t60Coef (p[kDecay], fs);
        holdLeft = (int) (p[kHold] * fs);
        clickEnv = 1.0f; clickDec = (float) std::exp (-1.0 / (0.0012 * fs));
        clickAmp = p[kClick] * (0.4f + 0.6f * vel) * 1.6f;
        clickFilt.reset(); clickFilt.set (p[kClickTone] * (0.7 + 0.5 * vel), 0.9, fs);
        useTone = p[kTone] < 19000.0f;
        toneFilt.reset(); toneFilt.set (p[kTone], 0.707, fs);
        drv = p[kDrive];
        amp = velGain (vel) * 0.8f;
    }

    void render (float* L, float* R, int n)
    {
        for (int i = 0; i < n && active; ++i)
        {
            float x = (float) (std::sin (twoPi * phase) * env);
            const double f = f0 * std::exp2 (semis / 12.0);
            semis *= sweepCoef;
            phase += f / fs;
            if (phase >= 1.0) phase -= 1.0;
            if (holdLeft > 0) --holdLeft; else env *= dec;
            if (clickEnv > 1e-4f)
            {
                x += clickFilt.bpNorm (rng.next()) * clickEnv * clickAmp;
                clickEnv *= clickDec;
            }
            x = drive (x, drv);
            if (useTone) x = toneFilt.lp (x);
            float g;
            if (! fadeGain (g)) break;
            const float y = x * amp * g;
            L[i] += y; R[i] += y;
            if (env < 1e-4 && clickEnv < 1e-4f) active = false;
        }
    }
};

//==============================================================================
// Drum head made of the first circular-membrane modes (Bessel zero ratios).
template <int N>
struct Membrane
{
    static constexpr double ratio[6] { 1.0, 1.5933, 2.1355, 2.2954, 2.6531, 2.9173 };
    double phase[N] {}, env[N] {}, dec[N] {}; float amp[N] {};
    double f0 = 100, semis = 0, sweepCoef = 0;

    void start (double freq, double bendSemis, double sweepSec, double decay, float ring, double fs)
    {
        static constexpr double decMul[6] { 1.0, 0.75, 0.55, 0.5, 0.42, 0.38 };
        static constexpr float ampMul[6] { 1.0f, 0.8f, 0.55f, 0.45f, 0.35f, 0.3f };
        f0 = freq; semis = bendSemis; sweepCoef = std::exp (-1.0 / (std::max (0.002, sweepSec) * fs));
        for (int k = 0; k < N; ++k)
        {
            phase[k] = 0.0; env[k] = 1.0;
            dec[k] = t60Coef (decay * decMul[k], fs);
            amp[k] = k == 0 ? 1.0f : ring * ampMul[k];
        }
    }
    float tick (double fs)
    {
        const double f = f0 * std::exp2 (semis / 12.0);
        semis *= sweepCoef;
        float s = 0.0f;
        for (int k = 0; k < N; ++k)
        {
            phase[k] += f * ratio[k] / fs;
            if (phase[k] >= 1.0) phase[k] -= 1.0;
            s += (float) (std::sin (twoPi * phase[k]) * env[k]) * amp[k];
            env[k] *= dec[k];
        }
        return s;
    }
    bool alive() const { return env[0] > 1e-4; }
};

struct SnareVoice : VoiceCommon
{
    Membrane<4> head;
    Svf hpF, lpF;
    float snapEnv = 0, snapDec = 0, snapAmp = 0, bodyAmp = 1, amp = 1, drv = 0;

    void start (const float* p, float vel, float pitchOffset)
    {
        begin();
        head.start (p[sTune] * std::pow (2.0, pitchOffset / 12.0), p[sPitch] * (0.6 + 0.4 * vel), 0.018, p[sBodyDecay], p[sRing], fs);
        const double tone = p[sSnapTone] * (0.75 + 0.35 * vel);
        hpF.reset(); hpF.set (tone * 0.35, 0.7, fs);
        lpF.reset(); lpF.set (tone * 1.6, 0.7, fs);
        snapEnv = 1.0f; snapDec = (float) t60Coef (p[sSnapDecay], fs);
        snapAmp = p[sSnap] * 1.6f;
        bodyAmp = 0.75f;
        drv = p[sDrive];
        amp = velGain (vel) * 0.7f;
    }

    void render (float* L, float* R, int n)
    {
        for (int i = 0; i < n && active; ++i)
        {
            float x = head.tick (fs) * bodyAmp;
            x += lpF.lp (hpF.hp (rng.next())) * snapEnv * snapAmp;
            snapEnv *= snapDec;
            x = drive (x, drv);
            float g;
            if (! fadeGain (g)) break;
            const float y = x * amp * g;
            L[i] += y; R[i] += y;
            if (! head.alive() && snapEnv < 1e-4f) active = false;
        }
    }
};

struct TomVoice : VoiceCommon
{
    Membrane<3> head;
    Svf stickF;
    float stickEnv = 0, stickDec = 0, stickAmp = 0, amp = 1, drv = 0;

    void start (const float* p, float vel, float pitchOffset)
    {
        begin();
        head.start (p[tTune] * std::pow (2.0, pitchOffset / 12.0), p[tPitch] * (0.6 + 0.4 * vel), p[tSweep], p[tDecay], p[tRing], fs);
        stickF.reset(); stickF.set (2500.0, 0.7, fs);
        stickEnv = 1.0f; stickDec = (float) std::exp (-1.0 / (0.004 * fs));
        stickAmp = p[tStick] * 1.2f * (0.5f + 0.5f * vel);
        drv = p[tDrive];
        amp = velGain (vel) * 0.85f;
    }

    void render (float* L, float* R, int n)
    {
        for (int i = 0; i < n && active; ++i)
        {
            float x = head.tick (fs);
            if (stickEnv > 1e-4f) { x += stickF.hp (rng.next()) * stickEnv * stickAmp; stickEnv *= stickDec; }
            x = drive (x, drv);
            float g;
            if (! fadeGain (g)) break;
            const float y = x * amp * g;
            L[i] += y; R[i] += y;
            if (! head.alive() && stickEnv < 1e-4f) active = false;
        }
    }
};

//==============================================================================
struct ClapVoice : VoiceCommon
{
    Svf bpL, bpR;
    Rng rng2;
    int bursts = 4, burstIdx = 0; double nextBurst = 0, t = 0, spread = 0.011;
    double env = 0, fastCoef = 0, tailCoef = 0;
    float width = 0.5f, amp = 1, drv = 0, norm = 1;
    double jitter[6] {};

    void start (const float* p, float vel, float)
    {
        begin();
        const double q = 0.8 + 3.2 * p[cRes];
        const double tone = p[cTone] * (0.85 + 0.25 * vel);
        bpL.reset(); bpR.reset(); bpL.set (tone, q, fs); bpR.set (tone * 1.03, q, fs);
        norm = (float) std::sqrt (q) * 1.4f;
        bursts = std::clamp ((int) std::lround (p[cBursts]), 1, 6);
        spread = p[cSpread];
        for (int i = 0; i < 6; ++i) jitter[i] = 1.0 + 0.18 * rng.next();
        burstIdx = 0; nextBurst = 0; t = 0; env = 0;
        fastCoef = std::exp (-1.0 / (0.0028 * fs));
        tailCoef = t60Coef (p[cDecay], fs);
        width = p[cWidth];
        drv = p[cDrive];
        amp = velGain (vel) * 0.9f;
        rng2.s = rng.s ^ 0x5bd1e995u;
    }

    void render (float* L, float* R, int n)
    {
        for (int i = 0; i < n && active; ++i)
        {
            if (burstIdx < bursts && t >= nextBurst)
            {
                env = 1.0;
                ++burstIdx;
                nextBurst += spread * jitter[burstIdx % 6];
            }
            const double coef = burstIdx >= bursts ? tailCoef : fastCoef;
            const float a = rng.next(), b = rng2.next();
            const float nl = a, nr = a * (1.0f - width) + b * width;
            float l = bpL.bpNorm (nl) * norm * (float) env;
            float r = bpR.bpNorm (nr) * norm * (float) env;
            env *= coef;
            t += 1.0 / fs;
            l = drive (l, drv); r = drive (r, drv);
            float g;
            if (! fadeGain (g)) break;
            L[i] += l * amp * g; R[i] += r * amp * g;
            if (burstIdx >= bursts && env < 1e-4) active = false;
        }
    }
};

//==============================================================================
// Six band-limited squares at inharmonic ratios: the classic analog metal source.
struct Metal
{
    static constexpr double ratio[6] { 1.0, 1.4826, 1.8004, 2.5459, 2.6303, 3.8968 };
    BlepSquare osc[6];
    void set (double base, double fs) { for (int k = 0; k < 6; ++k) osc[k].inc = base * ratio[k] / fs; }
    float tick() { float s = 0; for (auto& o : osc) s += o.tick(); return s * (1.0f / 6.0f); }
};

struct HatVoice : VoiceCommon
{
    Metal metal;
    Svf bp, hp;
    double env = 0, dec = 0;
    float color = 0.35f, amp = 1, norm = 1;
    double baseFreq = 405.0;

    void start (const float* p, float vel, float)
    {
        begin();
        metal.set (baseFreq * std::pow (2.0, p[hTune] / 12.0), fs);
        const double tone = p[hTone] * (0.85 + 0.2 * vel);
        const double q = 0.6 + 3.0 * p[hRes];
        bp.set (tone, q, fs); hp.set (tone * 0.55, 0.7, fs);
        norm = (float) std::sqrt (q) * 1.3f;
        color = p[hColor];
        env = 1.0; dec = t60Coef (p[hDecay], fs);
        amp = velGain (vel) * 0.9f;
    }

    void render (float* L, float* R, int n)
    {
        for (int i = 0; i < n && active; ++i)
        {
            const float src = metal.tick() * (1.0f - color) + rng.next() * color * 0.9f;
            float x = hp.hp (bp.bpNorm (src)) * norm * (float) env;
            env *= dec;
            float g;
            if (! fadeGain (g)) break;
            const float y = x * amp * g;
            L[i] += y; R[i] += y;
            if (env < 1e-4) active = false;
        }
    }
};

//==============================================================================
struct PercVoice : VoiceCommon
{
    double ph1 = 0, ph2 = 0, inc1 = 0, inc2 = 0;
    BlepSquare sq1, sq2;
    Svf lp, hp, clickF;
    double env = 0, dec = 0, envFast = 0, decFast = 0;
    float wave = 0.6f, amp = 1, clickEnv = 0, clickDec = 0, clickAmp = 0;

    void start (const float* p, float vel, float pitchOffset)
    {
        begin();
        const double f = p[pTune] * std::pow (2.0, pitchOffset / 12.0);
        inc1 = f / fs; inc2 = f * p[pRatio] / fs;
        sq1.inc = inc1; sq2.inc = inc2;
        ph1 = ph2 = 0.0; sq1.phase = 0.0; sq2.phase = 0.0;
        wave = p[pWave];
        lp.reset(); lp.set (p[pTone] * (0.8 + 0.3 * vel), 0.8, fs);
        hp.reset(); hp.set (f * 0.5, 0.7, fs);
        clickF.reset(); clickF.set (3000.0, 0.7, fs);
        env = 1.0; dec = t60Coef (p[pDecay], fs);
        envFast = 1.0; decFast = t60Coef (std::min (0.04f, p[pDecay] * 0.3f), fs);
        clickEnv = 1.0f; clickDec = (float) std::exp (-1.0 / (0.0015 * fs)); clickAmp = p[pClick] * 1.5f;
        amp = velGain (vel) * 0.6f;
    }

    void render (float* L, float* R, int n)
    {
        for (int i = 0; i < n && active; ++i)
        {
            const float s1 = (float) std::sin (twoPi * ph1), s2 = (float) std::sin (twoPi * ph2);
            ph1 += inc1; if (ph1 >= 1.0) ph1 -= 1.0;
            ph2 += inc2; if (ph2 >= 1.0) ph2 -= 1.0;
            const float q1 = sq1.tick(), q2 = sq2.tick();
            const float osc = ((1.0f - wave) * (s1 + s2) + wave * 0.7f * (q1 + q2)) * 0.5f;
            float x = hp.hp (lp.lp (osc)) * (float) (0.7 * env + 0.5 * envFast);
            env *= dec; envFast *= decFast;
            if (clickEnv > 1e-4f) { x += clickF.hp (rng.next()) * clickEnv * clickAmp; clickEnv *= clickDec; }
            float g;
            if (! fadeGain (g)) break;
            const float y = x * amp * g;
            L[i] += y; R[i] += y;
            if (env < 1e-4 && clickEnv < 1e-4f) active = false;
        }
    }
};

//==============================================================================
struct CymbalVoice : VoiceCommon
{
    Metal metL, metR;
    Svf bpL, bpR, hpL, hpR;
    Rng rng2;
    double e1 = 0, d1 = 0, e2 = 0, d2 = 0, t = 0;
    float color = 0.5f, width = 0.6f, bloom = 0.15f, amp = 1, norm = 1;

    void start (const float* p, float vel, float)
    {
        begin();
        const double base = 330.0 * std::pow (2.0, p[yTune] / 12.0);
        metL.set (base, fs); metR.set (base * 1.011, fs);
        const double tone = p[yTone] * (0.85 + 0.2 * vel);
        for (auto* f : { &bpL, &bpR }) f->set (tone, 0.9, fs);
        for (auto* f : { &hpL, &hpR }) f->set (tone * 0.45, 0.7, fs);
        norm = 1.5f;
        color = p[yColor]; width = p[yWidth]; bloom = p[yBloom];
        e1 = 1.0; d1 = t60Coef (p[yDecay] * 0.2, fs);
        e2 = 1.0; d2 = t60Coef (p[yDecay], fs);
        t = 0.0;
        amp = velGain (vel) * 0.8f;
        rng2.s = rng.s ^ 0x27d4eb2du;
    }

    void render (float* L, float* R, int n)
    {
        for (int i = 0; i < n && active; ++i)
        {
            const float mL = metL.tick(), mR = metR.tick();
            const float nL = rng.next(), nR = rng2.next();
            const float sL = mL * (1.0f - color) + nL * color * 0.9f;
            const float sR0 = mR * (1.0f - color) + nR * color * 0.9f;
            const float sR = sL * (1.0f - width) + sR0 * width;
            const float swell = (float) ((1.0 - std::exp (-t / 0.05)) * e2) * bloom * 1.2f;
            const float envAmp = (float) (0.55 * e1 + 0.45 * e2);
            float l = hpL.hp (bpL.bpNorm (sL)) * norm * envAmp;
            float r = hpR.hp (bpR.bpNorm (sR)) * norm * envAmp;
            l += nL * swell * 0.25f; r += (nL * (1.0f - width) + nR * width) * swell * 0.25f;
            e1 *= d1; e2 *= d2; t += 1.0 / fs;
            float g;
            if (! fadeGain (g)) break;
            L[i] += l * amp * g; R[i] += r * amp * g;
            if (e2 < 1e-4) active = false;
        }
    }
};

//==============================================================================
class ElementBase
{
public:
    virtual ~ElementBase() = default;
    virtual void prepare (double fs) = 0;
    virtual void trigger (float vel, float pitchOffset) = 0;
    virtual void choke() = 0;
    virtual void render (float* L, float* R, int n) = 0;   // adds
    virtual bool isActive() const = 0;
    std::array<float, 12> p {};
};

template <class V>
class ElementImpl final : public ElementBase
{
public:
    explicit ElementImpl (uint32_t seed, double baseFreq = 0.0)
    {
        for (int k = 0; k < 2; ++k)
        {
            slots[k].rng.s = seed * 2654435761u + (uint32_t) k * 40503u + 1u;
            if constexpr (std::is_same_v<V, HatVoice>) if (baseFreq > 0) slots[k].baseFreq = baseFreq;
        }
    }
    void prepare (double fs) override { for (auto& s : slots) { s.fs = fs; s.active = false; s.fading = false; } }
    void trigger (float vel, float pitchOffset) override
    {
        slots[cur].beginFade (0.004);
        cur ^= 1;
        slots[cur].active = false;
        slots[cur].start (p.data(), vel, pitchOffset);
    }
    void choke() override { for (auto& s : slots) s.beginFade (0.012); }
    void render (float* L, float* R, int n) override { for (auto& s : slots) if (s.active) s.render (L, R, n); }
    bool isActive() const override { return slots[0].active || slots[1].active; }
private:
    V slots[2];
    int cur = 0;
};

inline std::unique_ptr<ElementBase> makeElement (int e)
{
    switch (e)
    {
        case kick:      return std::make_unique<ElementImpl<KickVoice>> (11u);
        case snare:     return std::make_unique<ElementImpl<SnareVoice>> (23u);
        case clap:      return std::make_unique<ElementImpl<ClapVoice>> (37u);
        case tom:       return std::make_unique<ElementImpl<TomVoice>> (41u);
        case closedHat: return std::make_unique<ElementImpl<HatVoice>> (53u, 405.0);
        case openHat:   return std::make_unique<ElementImpl<HatVoice>> (67u, 405.0);
        case perc:      return std::make_unique<ElementImpl<PercVoice>> (71u);
        default:        return std::make_unique<ElementImpl<CymbalVoice>> (83u);
    }
}

//==============================================================================
// Linear-phase halfband FIR for 2x oversampling (79 taps, Kaiser window, flat to ~20 kHz).
struct Halfband
{
    static constexpr int taps = 79, centre = 39;
    static const std::array<float, taps>& coefs()
    {
        static const std::array<float, taps> h = []
        {
            std::array<float, taps> c {};
            auto bessel0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
            const double beta = 7.0;
            for (int i = 0; i < taps; ++i)
            {
                const int k = i - centre;
                const double sinc = k == 0 ? 0.5 : std::sin (pi * k / 2.0) / (pi * k);
                const double r = (double) k / centre;
                c[(size_t) i] = (float) (sinc * bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (beta));
            }
            return c;
        }();
        return h;
    }
};

struct Upsampler2x   // y0, y1 at twice the rate
{
    std::array<float, 64> hist {}; int pos = 0;
    void reset() { hist.fill (0); pos = 0; }
    void process (float x, float& y0, float& y1)
    {
        const auto& h = Halfband::coefs();
        pos = (pos + 1) & 63; hist[(size_t) pos] = x;
        // zero-stuffed input: even outputs use the even taps, odd outputs only the centre tap
        float acc = 0.0f;
        for (int m = 0; m <= Halfband::taps / 2; ++m) acc += h[(size_t) (2 * m)] * hist[(size_t) ((pos - m) & 63)];
        y0 = 2.0f * acc;
        y1 = 2.0f * h[Halfband::centre] * hist[(size_t) ((pos - (Halfband::centre - 1) / 2) & 63)];
    }
};

struct Downsampler2x
{
    std::array<float, 128> hist {}; int pos = 0;
    void reset() { hist.fill (0); pos = 0; }
    float process (float w0, float w1)   // output aligned to the even phase
    {
        const auto& h = Halfband::coefs();
        pos = (pos + 1) & 127; hist[(size_t) pos] = w0;
        float acc = 0.0f;
        for (int i = 0; i < Halfband::taps; ++i) acc += h[(size_t) i] * hist[(size_t) ((pos - i) & 127)];
        pos = (pos + 1) & 127; hist[(size_t) pos] = w1;
        return acc;
    }
};

// Bus: glue compressor + saturation (2x oversampled, first-order ADAA tanh).
// Constant latency (reported to the host) so the timing stays sample accurate.
struct MasterBus
{
    static constexpr int latency = Halfband::centre;
    double fs = 48000.0;
    float env = 0.0f, prevL = 0.0f, prevR = 0.0f;
    Upsampler2x upL, upR;
    Downsampler2x downL, downR;

    static double logCosh (double x) { const double a = std::abs (x); return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453; }
    static float adaa (float x, float& prev)
    {
        const double d = (double) x - prev;
        float y;
        if (std::abs (d) < 1e-5) y = std::tanh (0.5f * (x + prev));
        else y = (float) ((logCosh (x) - logCosh (prev)) / d);
        prev = x;
        return y;
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate; env = 0; prevL = prevR = 0;
        upL.reset(); upR.reset(); downL.reset(); downR.reset();
    }

    void process (float* L, float* R, int n, float glue, float drv, float volume)
    {
        const float atk = 1.0f - (float) std::exp (-1.0 / (0.008 * fs));
        const float rel = 1.0f - (float) std::exp (-1.0 / (0.15 * fs));
        const float thr = -4.0f - 22.0f * glue, ratio = 2.0f + 4.0f * glue, knee = 6.0f;
        const float makeup = glue > 0.001f ? std::pow (10.0f, (-thr * (1.0f - 1.0f / ratio) * 0.18f) / 20.0f) : 1.0f;
        const float g = 1.0f + 6.0f * drv * drv;
        const float dMake = 0.5f / std::tanh (0.5f * g);   // -6 dBFS peaks keep their level, quieter parts get denser
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (glue > 0.001f)
            {
                const float x = std::max (std::abs (l), std::abs (r));
                env += (x - env) * (x > env ? atk : rel);
                const float lvl = 20.0f * std::log10 (env + 1e-9f);
                const float over = lvl - thr;
                float grDb = 0.0f;
                if (over > knee * 0.5f) grDb = -over * (1.0f - 1.0f / ratio);
                else if (over > -knee * 0.5f) { const float o = over + knee * 0.5f; grDb = -(1.0f - 1.0f / ratio) * o * o / (2.0f * knee); }
                const float gain = std::pow (10.0f, grDb / 20.0f) * makeup;
                l *= gain; r *= gain;
            }
            {
                float a0, a1, b0, b1;
                upL.process (l, a0, a1);
                upR.process (r, b0, b1);
                if (drv > 0.001f)
                {
                    a0 = adaa (a0 * g, prevL) * dMake; a1 = adaa (a1 * g, prevL) * dMake;
                    b0 = adaa (b0 * g, prevR) * dMake; b1 = adaa (b1 * g, prevR) * dMake;
                }
                l = downL.process (a0, a1);
                r = downR.process (b0, b1);
            }
            l *= volume; r *= volume;
            L[i] = softClip (l); R[i] = softClip (r);
        }
    }

    static float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.85f) return x;
        return std::copysign (0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f), x);
    }
};

//==============================================================================
struct Event { int sample; int element; float vel; float pitch; };

class DrumEngine
{
public:
    DrumEngine() { for (int e = 0; e < numElements; ++e) elements[(size_t) e] = makeElement (e); }

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        for (auto& e : elements) e->prepare (fs);
        for (auto& b : bufL) b.assign ((size_t) maxBlock, 0.0f);
        for (auto& b : bufR) b.assign ((size_t) maxBlock, 0.0f);
        master.prepare (fs);
    }

    ElementBase& element (int e) { return *elements[(size_t) e]; }

    void trigger (int e, float vel, float pitch = 0.0f)
    {
        if (e == closedHat) elements[openHat]->choke();
        elements[(size_t) e]->trigger (vel, pitch);
    }

    // events must be sorted by sample; any block length works (split into internal chunks)
    void render (float* L, float* R, int n, const Event* events, int numEvents)
    {
        const int cap = (int) bufL[0].size();
        int done = 0, ev = 0;
        while (done < n)
        {
            const int len = std::min (cap, n - done);
            int evEnd = ev;
            while (evEnd < numEvents && (events[evEnd].sample < done + len || done + len >= n)) ++evEnd;
            renderChunk (L + done, R + done, len, events + ev, evEnd - ev, done);
            ev = evEnd;
            done += len;
        }
    }

private:
    void renderChunk (float* L, float* R, int n, const Event* events, int numEvents, int offset)
    {
        for (int e = 0; e < numElements; ++e)
        {
            std::fill (bufL[(size_t) e].begin(), bufL[(size_t) e].begin() + n, 0.0f);
            std::fill (bufR[(size_t) e].begin(), bufR[(size_t) e].begin() + n, 0.0f);
        }
        int pos = 0, ev = 0;
        while (pos < n)
        {
            while (ev < numEvents && events[ev].sample - offset <= pos) { trigger (events[ev].element, events[ev].vel, events[ev].pitch); ++ev; }
            const int end = ev < numEvents ? std::min (n, std::max (pos + 1, events[ev].sample - offset)) : n;
            for (int e = 0; e < numElements; ++e)
                if (elements[(size_t) e]->isActive())
                    elements[(size_t) e]->render (bufL[(size_t) e].data() + pos, bufR[(size_t) e].data() + pos, end - pos);
            pos = end;
        }
        while (ev < numEvents) { trigger (events[ev].element, events[ev].vel, events[ev].pitch); ++ev; }

        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        for (int e = 0; e < numElements; ++e)
        {
            const auto& m = mix[(size_t) e];
            if (m.mute) continue;
            const float a = (float) ((m.pan + 1.0) * 0.25 * pi);
            const float gl = m.level * std::cos (a) * 1.41421356f * elementHeadroom, gr = m.level * std::sin (a) * 1.41421356f * elementHeadroom;
            const float* bl = bufL[(size_t) e].data(); const float* br = bufR[(size_t) e].data();
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)   // equal-power pan (acts as balance on stereo elements)
            {
                const float l = bl[i] * gl, r = br[i] * gr;
                L[i] += l; R[i] += r;
                pk = std::max (pk, std::max (std::abs (l), std::abs (r)));
            }
            meter[(size_t) e] = std::max (meter[(size_t) e] * 0.8f, pk);
        }
        master.process (L, R, n, glue, driveAmt, volume);
    }

public:
    struct Mix { float level = 1.0f, pan = 0.0f; bool mute = false; };
    std::array<Mix, numElements> mix;
    std::array<float, numElements> meter {};
    float glue = 0.0f, driveAmt = 0.0f, volume = 0.7f;
    static constexpr float elementHeadroom = 0.5f;   // single hits around -9 dBFS, a full kit stays clear of the clipper

private:
    double fs = 48000.0;
    std::array<std::unique_ptr<ElementBase>, numElements> elements;
    std::array<std::vector<float>, numElements> bufL, bufR;
    MasterBus master;
};
} // namespace rud
