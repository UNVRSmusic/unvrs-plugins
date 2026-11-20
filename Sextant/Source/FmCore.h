#pragma once

// Sextant FM engine: six operators, each one of four kinds (FM oscillator, ring modulator,
// filter, wavefolder), wired by an algorithm (preset or user drawn). Voices run at 2x the
// host rate and are decimated with a linear-phase halfband, which keeps high FM indexes clean.

#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <atomic>

namespace sx
{
constexpr int numOps = 6;
constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = 2.0 * pi;

enum Mode { modeFM = 0, modeRing, modeFilter, modeFold, numModes };
enum Wave { wSine = 0, wTri, wSaw, wSquare, wHalf, wAbs, wDouble, wNoise, numWaves };

//==============================================================================
// Algorithms: bit (src * 6 + dst) set = operator src modulates operator dst.
struct Routing
{
    uint64_t conn = 0;
    uint8_t carriers = 1;

    bool has (int src, int dst) const { return ((conn >> (src * numOps + dst)) & 1u) != 0; }
    bool isCarrier (int op) const { return ((carriers >> op) & 1u) != 0; }
    static uint64_t bit (int src, int dst) { return 1ull << (src * numOps + dst); }
};

struct Algorithm : Routing
{
    std::string name;
};

inline const std::vector<Algorithm>& algorithms()
{
    static const std::vector<Algorithm> list = []
    {
        // "65 54" = op6 -> op5, op5 -> op4 ; carriers listed after '|'
        const char* defs[][2] = {
            { "Stack 6",            "65 54 43 32 21|1" },
            { "Stack 5 + Sine",     "65 54 43 32|12" },
            { "Stack 4 + Pair",     "65 54 43 21|13" },
            { "2 x Stack 3",        "32 21 65 54|14" },
            { "3 x Pair",           "21 43 65|135" },
            { "Pair + 4 Sines",     "21|13456" },
            { "2 Pairs + 2 Sines",  "21 43|1356" },
            { "Stack 3 + 3 Sines",  "65 54|1234" },
            { "Two Into One",       "21 31 54 64|14" },
            { "Three Into One",     "21 31 41 65|15" },
            { "Five Into One",      "21 31 41 51 61|1" },
            { "One To Three",       "21 63 64 65|1345" },
            { "One To Five",        "61 62 63 64 65|12345" },
            { "Two To Four",        "51 52 63 64|1234" },
            { "Diamond",            "42 43 21 31 65|15" },
            { "Fan Stack",          "65 54 53 21|134" },
            { "Deep Pair",          "64 54 43 32 21|1" },
            { "Additive",           "|123456" },
        };
        std::vector<Algorithm> v;
        for (auto& d : defs)
        {
            Algorithm a;
            a.name = d[0];
            a.carriers = 0;
            const std::string s = d[1];
            const auto bar = s.find ('|');
            for (size_t i = 0; i + 1 < bar; ++i)
                if (s[i] >= '1' && s[i] <= '6' && s[i + 1] >= '1' && s[i + 1] <= '6')
                {
                    a.conn |= Algorithm::bit (s[i] - '1', s[i + 1] - '1');
                    ++i;
                }
            for (size_t i = bar + 1; i < s.size(); ++i)
                a.carriers |= (uint8_t) (1u << (s[i] - '1'));
            v.push_back (a);
        }
        return v;
    }();
    return list;
}

//==============================================================================
struct WaveTables
{
    static constexpr int size = 4096;
    std::array<std::array<float, size + 1>, numWaves> t {};

    WaveTables()
    {
        for (int i = 0; i <= size; ++i)
        {
            const double x = (double) (i % size) / size;
            const double s = std::sin (twoPi * x);
            double saw = 0, sq = 0;
            for (int h = 1; h <= 24; ++h)
            {
                const double sigma = std::sin (pi * h / 25.0) / (pi * h / 25.0);   // Lanczos: no Gibbs ringing
                saw += sigma * std::sin (twoPi * h * x) / h;
                if (h % 2 == 1) sq += sigma * std::sin (twoPi * h * x) / h;
            }
            t[wSine][(size_t) i]   = (float) s;
            t[wTri][(size_t) i]    = (float) (x < 0.25 ? 4 * x : (x < 0.75 ? 2 - 4 * x : 4 * x - 4));
            t[wSaw][(size_t) i]    = (float) (saw * 2.0 / pi);
            t[wSquare][(size_t) i] = (float) (sq * 4.0 / pi * 0.85);
            t[wHalf][(size_t) i]   = (float) (x < 0.5 ? s : 0.0);
            t[wAbs][(size_t) i]    = (float) (std::abs (s) * 2.0 - 1.0);
            t[wDouble][(size_t) i] = (float) (x < 0.5 ? std::sin (2.0 * twoPi * x) : 0.0);
            t[wNoise][(size_t) i]  = 0.0f;
        }
    }
};

inline const WaveTables& waves() { static const WaveTables w; return w; }

inline float lookup (const std::array<float, WaveTables::size + 1>& tab, double phase)
{
    phase -= std::floor (phase);
    const double p = phase * WaveTables::size;
    const int i = (int) p;
    const float f = (float) (p - i);
    return tab[(size_t) i] + f * (tab[(size_t) i + 1] - tab[(size_t) i]);
}

//==============================================================================
struct Svf   // Zavalishin TPT state variable filter
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set (double fc, double q, double fs)
    {
        const double g = std::tan (pi * std::clamp (fc, 8.0, 0.45 * fs) / fs);
        k = (float) (1.0 / q);
        a1 = (float) (1.0 / (1.0 + g * (g + k)));
        a2 = (float) (g * a1);
        a3 = (float) (g * a2);
    }
    void reset() { ic1 = ic2 = 0; }
    float tick (float v0, int type)   // 0 lp, 1 bp, 2 hp
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        return type == 0 ? v2 : (type == 1 ? v1 * k : v0 - k * v1 - v2);
    }
};

struct Env
{
    enum Stage { idle, attack, decay, release };
    Stage stage = idle;
    float v = 0.0f, a = 0.0f, d = 0.0f, s = 1.0f, r = 0.0f;

    void set (float A, float D, float S, float R, double fs)
    {
        a = 1.0f - (float) std::exp (-1.609 / (std::max (0.0005f, A) * fs));   // overshoot target 1.25 reaches 1 in A
        d = 1.0f - (float) std::exp (-4.6 / (std::max (0.001f, D) * fs));
        s = S;
        r = 1.0f - (float) std::exp (-6.9 / (std::max (0.002f, R) * fs));
    }
    void gate (bool on) { stage = on ? attack : (stage == idle ? idle : release); }
    float tick()
    {
        switch (stage)
        {
            case attack:  v += (1.25f - v) * a; if (v >= 1.0f) { v = 1.0f; stage = decay; } break;
            case decay:   v += (s - v) * d; break;
            case release: v -= v * r; if (v < 1e-5f) { v = 0.0f; stage = idle; } break;
            default: break;
        }
        return v;
    }
};

//==============================================================================
struct OpParams
{
    int mode = modeFM, wave = wSine, filterType = 0;
    bool fixed = false;
    float ratio = 1.0f, detune = 0.0f, freq = 440.0f, level = 0.0f, feedback = 0.0f, velocity = 0.5f, keyLevel = 0.0f;
    float A = 0.002f, D = 0.5f, S = 1.0f, R = 0.3f, res = 0.2f, drive = 0.3f;
};

struct Settings
{
    std::array<OpParams, numOps> op;
    Routing algo = algorithms()[0];   // routing only: copying settings never allocates

    int voiceMode = 0;            // 0 poly, 1 mono, 2 legato
    int polyphony = 8, unison = 1;
    float uniDetune = 12.0f, uniSpread = 0.6f, glide = 0.0f, bendRange = 2.0f;

    int filterType = 0;           // 0 off, 1 LP12, 2 LP24, 3 BP, 4 HP
    float cutoff = 8000.0f, res = 0.2f, filterEnv = 0.0f, filterKey = 0.5f, filterVel = 0.3f;
    float fA = 0.005f, fD = 0.4f, fS = 0.0f, fR = 0.4f;

    int lfoWave = 0;              // 0 sine, 1 tri, 2 square, 3 saw, 4 s&h
    float lfoRate = 5.0f, lfoDelay = 0.0f, lfoPitch = 0.0f, lfoFm = 0.0f, lfoAmp = 0.0f, lfoFilter = 0.0f, mwVibrato = 0.4f;

    float chorus = 0.0f, delayMix = 0.0f, delayTime = 0.375f, delayFb = 0.35f;
    int delaySync = 0;            // 0 free, 1 1/16, 2 1/8, 3 1/8 dotted, 4 1/4, 5 1/4 dotted, 6 1/2
    float volume = 0.7f;
};

//==============================================================================
struct Halfband
{
    static constexpr int taps = 79, centre = 39;
    static const std::array<float, taps>& coefs()
    {
        static const std::array<float, taps> h = []
        {
            std::array<float, taps> c {};
            auto bessel0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
            for (int i = 0; i < taps; ++i)
            {
                const int k = i - centre;
                const double sinc = k == 0 ? 0.5 : std::sin (pi * k / 2.0) / (pi * k);
                const double r = (double) k / centre;
                c[(size_t) i] = (float) (sinc * bessel0 (7.0 * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (7.0));
            }
            return c;
        }();
        return h;
    }
};

struct Downsampler2x
{
    std::array<float, 128> hist {}; int pos = 0;
    void reset() { hist.fill (0); pos = 0; }
    float process (float w0, float w1)
    {
        const auto& h = Halfband::coefs();
        pos = (pos + 1) & 127; hist[(size_t) pos] = w0;
        float acc = 0.0f;
        for (int i = 0; i < Halfband::taps; ++i) acc += h[(size_t) i] * hist[(size_t) ((pos - i) & 127)];
        pos = (pos + 1) & 127; hist[(size_t) pos] = w1;
        return acc;
    }
};

//==============================================================================
class Engine
{
public:
    static constexpr int maxPoly = 16, poolSize = 24, ctrl = 16;    // ctrl: samples at 2x per control update
    static constexpr int latency = 20;                               // halfband delay in host samples (19.5)
    static constexpr float modScale = 6.0f;                          // radians of phase deviation at full level
    static constexpr float fbScale = 0.8f;                           // 100 % feedback = clean saw (beyond it turns chaotic)

    Engine() { waves(); heldStack.reserve (256); }

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate; fs2 = 2.0 * fs;
        osL.assign ((size_t) (2 * maxBlock + 64), 0.0f);
        osR.assign (osL.size(), 0.0f);
        for (auto& v : voices) v = Voice {};
        downL.reset(); downR.reset();
        chorusL.assign ((size_t) (0.06 * fs) + 4, 0.0f); chorusR.assign (chorusL.size(), 0.0f);
        delayL.assign ((size_t) (2.1 * fs), 0.0f); delayR.assign (delayL.size(), 0.0f);
        cPos = dPos = 0; delaySmooth = 0.0;
        lfoPhase = 0.0; lfoHeld = 0.0f;
    }

    void setSettings (const Settings& s)
    {
        for (int k = 0; k < numOps; ++k)
        {
            const auto& a = s.op[(size_t) k]; const auto& b = settings.op[(size_t) k];
            if (a.A != b.A || a.D != b.D || a.S != b.S || a.R != b.R) envDirty = true;
        }
        if (s.fA != settings.fA || s.fD != settings.fD || s.fS != settings.fS || s.fR != settings.fR) envDirty = true;
        settings = s;
    }
    void setTempo (double bpm) { tempo = bpm > 0 ? bpm : 120.0; }

    void pitchBend (float v) { bend = v; }           // -1..1
    void modWheel (float v) { wheel = v; }           // 0..1
    void sustain (bool down)
    {
        sustainDown = down;
        if (! down)
            for (auto& v : voices)
                if (v.active && v.sustained) { v.sustained = false; releaseVoice (v); }
    }

    void noteOn (int note, float vel)
    {
        ++clock;
        heldStack.erase (std::remove (heldStack.begin(), heldStack.end(), note), heldStack.end());
        heldStack.push_back (note);
        if (heldCount() == 1) lfoFade = 0.0f;

        const int uni = std::clamp (settings.unison, 1, 4);
        if (settings.voiceMode != 0)
        {
            // mono / legato: one group of unison voices, glide between notes
            const bool legato = settings.voiceMode == 2 && heldStack.size() > 1;
            bool any = false;
            for (int u = 0; u < uni; ++u)
            {
                auto& v = voices[(size_t) u];
                any |= v.active;
                startVoice (v, note, vel, u, uni, legato && v.active, ! v.active);
            }
            for (int i = uni; i < poolSize; ++i) if (voices[(size_t) i].active && ! voices[(size_t) i].fading) beginFade (voices[(size_t) i]);
            (void) any;
            return;
        }

        // poly: keep the polyphony limit (counted in notes), fade the oldest when needed
        const int limit = std::clamp (settings.polyphony, 1, maxPoly) * uni;
        for (int u = 0; u < uni; ++u)
        {
            int busy = 0;
            for (auto& v : voices) busy += (v.active && ! v.fading) ? 1 : 0;
            if (busy >= limit)
            {
                Voice* victim = nullptr;
                for (auto& v : voices)
                    if (v.active && ! v.fading && (victim == nullptr || (! v.held && victim->held) || (v.held == victim->held && v.age < victim->age)))
                        victim = &v;
                if (victim) beginFade (*victim);
            }
            Voice* slot = nullptr;
            for (auto& v : voices) if (! v.active) { slot = &v; break; }
            if (slot == nullptr)
                for (auto& v : voices) if (v.fading && (slot == nullptr || v.fade < slot->fade)) slot = &v;
            if (slot == nullptr) return;
            *slot = Voice {};
            startVoice (*slot, note, vel, u, uni, false, true);
        }
    }

    void noteOff (int note)
    {
        heldStack.erase (std::remove (heldStack.begin(), heldStack.end(), note), heldStack.end());
        if (settings.voiceMode != 0)
        {
            const int uni = std::clamp (settings.unison, 1, 4);
            if (! heldStack.empty())
            {
                // return to the previous held note (last-note priority), legato
                const int back = heldStack.back();
                for (int u = 0; u < uni; ++u)
                    if (voices[(size_t) u].active && voices[(size_t) u].note == note)
                        startVoice (voices[(size_t) u], back, voices[(size_t) u].vel, u, uni, true, false);
                return;
            }
        }
        for (auto& v : voices)
            if (v.active && v.held && v.note == note)
            {
                v.held = false;
                if (sustainDown) v.sustained = true; else releaseVoice (v);
            }
    }

    void allNotesOff()
    {
        heldStack.clear();
        for (auto& v : voices) if (v.active) { v.held = false; releaseVoice (v); }
    }

    void render (float* L, float* R, int n)
    {
        const int n2 = 2 * n;
        std::fill (osL.begin(), osL.begin() + n2, 0.0f);
        std::fill (osR.begin(), osR.begin() + n2, 0.0f);
        prepareRouting();
        if (envDirty)
        {
            envDirty = false;
            for (auto& v : voices)
                if (v.active)
                {
                    for (int k = 0; k < numOps; ++k)
                    {
                        const auto& p = settings.op[(size_t) k];
                        v.ops[(size_t) k].env.set (p.A, p.D, p.S, p.R, fs2);
                    }
                    v.fenv.set (settings.fA, settings.fD, settings.fS, settings.fR, fs2);
                }
        }

        for (int off = 0; off < n2; off += ctrl)
        {
            const int len = std::min (ctrl, n2 - off);
            updateLfo (len);
            for (auto& v : voices)
                if (v.active)
                    renderVoice (v, osL.data() + off, osR.data() + off, len);
        }

        for (int i = 0; i < n; ++i)
        {
            L[i] = downL.process (osL[(size_t) (2 * i)], osL[(size_t) (2 * i + 1)]);
            R[i] = downR.process (osR[(size_t) (2 * i)], osR[(size_t) (2 * i + 1)]);
        }
        effects (L, R, n);

        // GUI activity: most recent voice
        const Voice* latest = nullptr;
        for (auto& v : voices) if (v.active && (latest == nullptr || v.age > latest->age)) latest = &v;
        for (int k = 0; k < numOps; ++k)
            activity[(size_t) k].store (latest ? latest->ops[(size_t) k].env.v * latest->ops[(size_t) k].amp : 0.0f, std::memory_order_relaxed);
    }

    int activeVoices() const { int c = 0; for (auto& v : voices) c += v.active ? 1 : 0; return c; }

    std::array<std::atomic<float>, numOps> activity {};

private:
    struct OpState
    {
        double phase = 0.0, inc = 0.0;
        float fb1 = 0.0f, fb2 = 0.0f, amp = 0.0f;
        Env env;
        Svf svf;
    };

    struct Voice
    {
        bool active = false, held = false, sustained = false, fading = false;
        int note = 60; float vel = 1.0f; uint64_t age = 0;
        float fade = 1.0f;
        double pitch = 60.0, target = 60.0, uniOffset = 0.0;
        float panL = 1.0f, panR = 1.0f;
        std::array<OpState, numOps> ops;
        std::array<float, numOps> prevOut {};
        Env fenv;
        Svf f1, f2;
        uint32_t rng = 0x2545F491u;
        float noise() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (int32_t) rng * (1.0f / 2147483648.0f); }
    };

    int heldCount() const { return (int) heldStack.size(); }

    void beginFade (Voice& v) { v.fading = true; v.fade = 1.0f; }

    void releaseVoice (Voice& v)
    {
        for (auto& o : v.ops) o.env.gate (false);
        v.fenv.gate (false);
    }

    void startVoice (Voice& v, int note, float vel, int uniIndex, int uni, bool legato, bool fresh)
    {
        const auto& s = settings;
        const bool glideFromLast = s.voiceMode != 0 && s.glide > 0.0f && lastMonoPitch >= 0.0;
        if (fresh)
        {
            v.rng = 0x9E3779B9u * (uint32_t) (clock + 1) + (uint32_t) uniIndex * 7919u;
            for (auto& o : v.ops) { o.phase = 0.0; o.fb1 = o.fb2 = 0.0f; o.env.v = 0.0f; o.env.stage = Env::idle; o.svf.reset(); }
            v.prevOut.fill (0.0f);
            v.f1.reset(); v.f2.reset();
            v.fenv.v = 0.0f; v.fenv.stage = Env::idle;
            v.pitch = glideFromLast ? lastMonoPitch : note;
        }
        else if (! glideFromLast)
            v.pitch = note;      // mono without glide jumps; with glide the pitch keeps sliding from where it is

        v.active = true; v.held = true; v.sustained = false; v.fading = false; v.fade = 1.0f;
        v.note = note; v.target = note; v.age = clock;
        if (! legato) v.vel = vel;
        const double spreadPos = uni > 1 ? (2.0 * uniIndex / (uni - 1) - 1.0) : 0.0;
        v.uniOffset = spreadPos * s.uniDetune * 0.01;
        const double pan = spreadPos * s.uniSpread;
        v.panL = (float) std::cos ((pan + 1.0) * pi * 0.25) * 1.41421356f;
        v.panR = (float) std::sin ((pan + 1.0) * pi * 0.25) * 1.41421356f;
        if (s.voiceMode != 0) lastMonoPitch = note;

        for (int k = 0; k < numOps; ++k)
        {
            auto& o = v.ops[(size_t) k];
            const auto& p = s.op[(size_t) k];
            o.env.set (p.A, p.D, p.S, p.R, fs2);
            if (! legato) o.env.gate (true);
        }
        v.fenv.set (s.fA, s.fD, s.fS, s.fR, fs2);
        if (! legato) v.fenv.gate (true);
    }

    // Which operators matter (reach a carrier) and the source lists per operator.
    void prepareRouting()
    {
        const auto& a = settings.algo;
        uint8_t reach = a.carriers;
        for (int pass = 0; pass < numOps; ++pass)
            for (int src = 0; src < numOps; ++src)
                for (int dst = 0; dst < numOps; ++dst)
                    if (a.has (src, dst) && ((reach >> dst) & 1u)) reach |= (uint8_t) (1u << src);
        activeMask = reach;
        int nc = 0;
        for (int k = 0; k < numOps; ++k)
        {
            srcCount[(size_t) k] = 0;
            for (int src = 0; src < numOps; ++src)
                if (a.has (src, k) && ((reach >> src) & 1u))
                    srcList[(size_t) k][(size_t) srcCount[(size_t) k]++] = src;
            nc += a.isCarrier (k) ? 1 : 0;
        }
        carrierNorm = 1.0f / std::sqrt ((float) std::max (1, nc));
    }

    void updateLfo (int len)
    {
        const auto& s = settings;
        const double ph0 = lfoPhase;
        lfoPhase += s.lfoRate * len / fs2;
        if (lfoPhase >= 1.0) { lfoPhase -= std::floor (lfoPhase); lfoHeld = (float) (((clock * 1103515245u + (uint64_t) (ph0 * 1e6)) >> 8) % 2001) / 1000.0f - 1.0f; }
        const double x = lfoPhase;
        switch (s.lfoWave)
        {
            case 1:  lfoValue = (float) (x < 0.5 ? 4 * x - 1 : 3 - 4 * x); break;
            case 2:  lfoValue = x < 0.5 ? 1.0f : -1.0f; break;
            case 3:  lfoValue = (float) (2 * x - 1); break;
            case 4:  lfoValue = lfoHeld; break;
            default: lfoValue = (float) std::sin (twoPi * x); break;
        }
        if (s.lfoDelay > 0.0f) lfoFade = std::min (1.0f, lfoFade + (float) (len / (s.lfoDelay * fs2)));
        else lfoFade = 1.0f;
    }

    void renderVoice (Voice& v, float* outL, float* outR, int n)
    {
        const auto& s = settings;
        const auto& wt = waves().t;

        // ---- control rate
        if (s.glide > 0.0f && s.voiceMode != 0)
            v.pitch += (v.target - v.pitch) * (1.0 - std::exp (-(double) n / (s.glide * fs2 * 0.3)));
        else v.pitch = v.target;
        const float lfo = lfoValue * lfoFade;
        const double vib = (s.lfoPitch + wheel * s.mwVibrato * 50.0f) * 0.01 * lfo;   // semitones
        const double semis = v.pitch + bend * s.bendRange + v.uniOffset + vib;
        const double noteHz = 440.0 * std::pow (2.0, (semis - 69.0) / 12.0);
        const float fmScale = 1.0f + s.lfoFm * lfo;
        const float keyOct = (float) ((v.note - 60) / 12.0);

        float level[numOps];
        for (int k = 0; k < numOps; ++k)
        {
            auto& o = v.ops[(size_t) k];
            const auto& p = s.op[(size_t) k];
            const double f = (p.fixed ? (double) p.freq : noteHz * p.ratio) * std::pow (2.0, p.detune / 1200.0);
            o.inc = f / fs2;
            float amp = p.level <= 0.0f ? 0.0f : std::pow (10.0f, -(99.0f - p.level) * 0.75f / 20.0f);
            amp *= (1.0f - p.velocity) + p.velocity * std::pow (v.vel, 1.5f);
            amp *= std::pow (2.0f, p.keyLevel * keyOct);                 // +-6 dB per octave
            if (! s.algo.isCarrier (k)) amp *= fmScale;
            o.amp = amp;
            level[k] = amp;
            if (p.mode == modeFilter)
                o.svf.set (f, 0.5 + 15.0 * p.res * p.res, fs2);
        }

        const bool useFilter = s.filterType != 0;
        if (useFilter)
        {
            const double env = v.fenv.v;
            double oct = s.filterEnv * 6.0 * env * (1.0 - s.filterVel + s.filterVel * v.vel) + s.lfoFilter * lfo;
            oct += s.filterKey * keyOct;
            const double fc = s.cutoff * std::pow (2.0, oct);
            const double q = 0.5 + 12.0 * s.res * s.res;
            v.f1.set (fc, s.filterType == 2 ? q * 0.7 : q, fs2);
            v.f2.set (fc, 0.54, fs2);
        }
        const int ftype = s.filterType == 3 ? 1 : (s.filterType == 4 ? 2 : 0);
        const float ampMod = 1.0f - s.lfoAmp * 0.5f * (1.0f - lfo);
        const float fadeStep = 1.0f / (0.003f * (float) fs2);

        // ---- audio rate
        float cur[numOps] {};
        for (int i = 0; i < n; ++i)
        {
            for (int k = numOps - 1; k >= 0; --k)
            {
                if (((activeMask >> k) & 1u) == 0) { cur[k] = 0.0f; continue; }
                auto& o = v.ops[(size_t) k];
                const auto& p = s.op[(size_t) k];
                float in = 0.0f;
                for (int j = 0; j < srcCount[(size_t) k]; ++j)
                {
                    const int src = srcList[(size_t) k][(size_t) j];
                    in += src > k ? cur[src] : v.prevOut[(size_t) src];
                }
                const float e = o.env.tick() * level[k];
                float y;
                switch (p.mode)
                {
                    case modeRing:
                    {
                        const float osc = p.wave == wNoise ? v.noise() : lookup (wt[(size_t) p.wave], o.phase);
                        y = osc * in * e * 2.0f;
                        break;
                    }
                    case modeFilter:
                        y = o.svf.tick (in, std::clamp (p.filterType, 0, 2)) * e;
                        break;
                    case modeFold:
                    {
                        const float osc = p.wave == wNoise ? v.noise() : lookup (wt[(size_t) p.wave], o.phase);
                        const float drive = 1.0f + 11.0f * p.drive * p.drive;
                        y = lookup (wt[wSine], (osc + in) * drive * 0.25) * e;
                        break;
                    }
                    default:
                    {
                        if (p.wave == wNoise) { y = v.noise() * e; break; }
                        const double pm = (in * modScale + p.feedback * fbScale * (o.fb1 + o.fb2)) / twoPi;
                        y = lookup (wt[(size_t) p.wave], o.phase + pm) * e;
                        o.fb2 = o.fb1; o.fb1 = y;
                        break;
                    }
                }
                o.phase += o.inc;
                if (o.phase >= 1.0) o.phase -= 1.0;
                cur[k] = y;
            }
            float out = 0.0f;
            for (int k = 0; k < numOps; ++k)
            {
                v.prevOut[(size_t) k] = cur[k];
                if (s.algo.isCarrier (k)) out += cur[k];
            }
            out *= carrierNorm * 0.35f * ampMod;
            v.fenv.tick();
            if (useFilter)
            {
                out = v.f1.tick (out, ftype);
                if (s.filterType == 2) out = v.f2.tick (out, 0);
            }
            if (v.fading)
            {
                v.fade -= fadeStep;
                if (v.fade <= 0.0f) { v.active = false; return; }
                out *= v.fade;
            }
            outL[i] += out * v.panL;
            outR[i] += out * v.panR;
        }

        // ---- end of life: all carriers silent
        bool alive = false;
        for (int k = 0; k < numOps; ++k)
            if (s.algo.isCarrier (k) && ((activeMask >> k) & 1u) && v.ops[(size_t) k].env.stage != Env::idle && level[k] > 0.0f)
                alive = true;
        if (! alive) v.active = false;
    }

    void effects (float* L, float* R, int n)
    {
        const auto& s = settings;
        // chorus: two modulated taps in quadrature
        if (s.chorus > 0.001f)
        {
            const int size = (int) chorusL.size();
            for (int i = 0; i < n; ++i)
            {
                chorusL[(size_t) cPos] = L[i]; chorusR[(size_t) cPos] = R[i];
                chorusPhase += 0.45 / fs; if (chorusPhase >= 1.0) chorusPhase -= 1.0;
                auto tap = [&] (const std::vector<float>& b, double ph)
                {
                    const double d = (0.011 + 0.0035 * std::sin (twoPi * ph)) * fs;
                    double rp = cPos - d; while (rp < 0) rp += size;
                    const int i0 = (int) rp; const float f = (float) (rp - i0);
                    return b[(size_t) i0] + f * (b[(size_t) ((i0 + 1) % size)] - b[(size_t) i0]);
                };
                const float wl = tap (chorusL, chorusPhase), wr = tap (chorusR, chorusPhase + 0.25);
                L[i] = L[i] * (1.0f - 0.4f * s.chorus) + wl * 0.8f * s.chorus;
                R[i] = R[i] * (1.0f - 0.4f * s.chorus) + wr * 0.8f * s.chorus;
                cPos = (cPos + 1) % size;
            }
        }
        // ping-pong delay, darkening feedback
        if (s.delayMix > 0.001f)
        {
            static constexpr double beats[] { 0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
            double t = s.delaySync > 0 ? beats[std::clamp (s.delaySync, 1, 6)] * 60.0 / tempo : s.delayTime;
            const double target = std::clamp (t * fs, 1.0, (double) delayL.size() - 4);
            if (delaySmooth <= 0.0) delaySmooth = target;
            const int size = (int) delayL.size();
            const float lpc = 1.0f - (float) std::exp (-twoPi * 5000.0 / fs);
            for (int i = 0; i < n; ++i)
            {
                delaySmooth += (target - delaySmooth) * 0.0005;
                double rp = dPos - delaySmooth; while (rp < 0) rp += size;
                const int i0 = (int) rp; const float f = (float) (rp - i0);
                const float dl = delayL[(size_t) i0] + f * (delayL[(size_t) ((i0 + 1) % size)] - delayL[(size_t) i0]);
                const float dr = delayR[(size_t) i0] + f * (delayR[(size_t) ((i0 + 1) % size)] - delayR[(size_t) i0]);
                lpL += (dl - lpL) * lpc; lpR += (dr - lpR) * lpc;
                delayL[(size_t) dPos] = 0.5f * (L[i] + R[i]) + s.delayFb * lpR;
                delayR[(size_t) dPos] = s.delayFb * lpL;
                L[i] += dl * s.delayMix; R[i] += dr * s.delayMix;
                dPos = (dPos + 1) % size;
            }
        }
        for (int i = 0; i < n; ++i)
        {
            L[i] = softClip (L[i] * s.volume);
            R[i] = softClip (R[i] * s.volume);
        }
    }

    static float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.85f) return x;
        return std::copysign (0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f), x);
    }

    double fs = 48000.0, fs2 = 96000.0, tempo = 120.0;
    Settings settings;
    std::array<Voice, poolSize> voices;
    std::vector<int> heldStack;
    std::vector<float> osL, osR, chorusL, chorusR, delayL, delayR;
    Downsampler2x downL, downR;
    int cPos = 0, dPos = 0;
    double chorusPhase = 0.0, delaySmooth = 0.0, lfoPhase = 0.0, lastMonoPitch = -1.0;
    float lpL = 0.0f, lpR = 0.0f, lfoValue = 0.0f, lfoHeld = 0.0f, lfoFade = 1.0f;
    float bend = 0.0f, wheel = 0.0f, carrierNorm = 1.0f;
    bool sustainDown = false, envDirty = false;
    uint64_t clock = 0;
    uint8_t activeMask = 0x3f;
    std::array<std::array<int, numOps>, numOps> srcList {};
    std::array<int, numOps> srcCount {};
};
} // namespace sx
