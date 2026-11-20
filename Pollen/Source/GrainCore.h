#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>
#include "../../Nimbus/Source/ReverbEngine.h"

/*
    Pollen grain engine.

    Source:   mono, loudness-normalised, 16-bit, stored as a 4-level mip-map (each level a halfband-filtered
              2:1 decimation of the previous one). A grain reads the level at which its playback rate
              is <= 1, so pitching up never aliases; reads are 4-point Hermite.
    Grain:    Tukey window (Shape: Hann ... flat-top), own pitch, direction, pan and level.
              Grains on one voice are summed incoherently, so their level is normalised by
              1 / sqrt(overlap * mean window power): Size and Density don't change loudness.
    Voice:    MIDI note -> grain pitch; spawns grains at Density/s (randomised spacing), from
              Position + Scan (playhead moving at a multiple of real time) + Drift (slow wander)
              + Spray (random offset). Positions fold at the ends (ping-pong), never wrap.
    Bus:      tone low-pass -> Nimbus ambient reverb -> volume.
*/
namespace pl
{
constexpr int numLevels = 4;
constexpr int pad = 4;
constexpr double rootHz = 261.6255653;   // built-in sources are tuned to C4 (MIDI 60)
constexpr int overviewBins = 1024;

//==============================================================================
struct Source
{
    double sampleRate = 48000.0;
    int length = 0;                                    // level-0 samples
    std::array<std::vector<int16_t>, numLevels> lv;    // 16-bit, each padded with `pad` zeros on both sides
    float scale = 1.0f;                                // int16 -> loudness-normalised float
    std::vector<float> overview;                       // peak per bin, for the GUI

    float read (int k, double p) const noexcept
    {
        const auto& v = lv[(size_t) k];
        const int n = (int) v.size() - 2 * pad;
        const double fl = std::floor (p);
        const int i = (int) fl;
        if (i < 1 - pad || i + 2 > n + pad - 1)
            return 0.0f;
        const float t = (float) (p - fl);
        const int16_t* d = v.data() + pad + i;
        const float y0 = d[-1], y1 = d[0], y2 = d[1], y3 = d[2];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return (((c3 * t + c2) * t + c1) * t + y1) * scale;
    }

    size_t memoryBytes() const { size_t b = 0; for (auto& v : lv) b += v.size() * sizeof (int16_t); return b; }
};

inline const std::vector<float>& halfbandTaps()
{
    static const std::vector<float> h = []
    {
        constexpr int M = 31;   // 63 taps, Kaiser beta 7: > 70 dB stop band
        const double pi = juce::MathConstants<double>::pi;
        auto i0 = [] (double x)
        {
            double s = 1.0, term = 1.0;
            for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); s += term; }
            return s;
        };
        std::vector<double> t (2 * M + 1);
        double sum = 0.0;
        for (int n = -M; n <= M; ++n)
        {
            const double sinc = n == 0 ? 0.5 : std::sin (pi * n * 0.5) / (pi * n);
            const double r = (double) n / M;
            t[(size_t) (n + M)] = sinc * i0 (7.0 * std::sqrt (1.0 - r * r)) / i0 (7.0);
            sum += t[(size_t) (n + M)];
        }
        std::vector<float> f;
        for (auto v : t) f.push_back ((float) (v / sum));
        return f;
    }();
    return h;
}

/** 2:1 halfband decimation of n samples of any numeric type. */
template <typename T>
std::vector<float> decimate2 (const T* x, int n)
{
    const auto& h = halfbandTaps();
    const int M = ((int) h.size() - 1) / 2;
    std::vector<float> y ((size_t) ((n + 1) / 2));
    for (int j = 0; j < (int) y.size(); ++j)
    {
        const int c = 2 * j;
        double acc = h[(size_t) M] * (float) x[c];
        for (int k = 1; k <= M; k += 2)   // even taps of a halfband are zero
        {
            const float a = c - k >= 0 ? (float) x[c - k] : 0.0f;
            const float b = c + k < n ? (float) x[c + k] : 0.0f;
            acc += h[(size_t) (M + k)] * (a + b);
        }
        y[(size_t) j] = (float) acc;
    }
    return y;
}

/** The same halfband as a stream, so high-rate files can be halved while they are read. */
struct StreamDecimator
{
    std::vector<float> buf;     // input not yet fully used
    int64_t base = 0;           // absolute index of buf[0]
    int64_t centre = 0;         // absolute index of the next output's centre (even)

    void push (const float* x, int n, std::vector<float>& out) { buf.insert (buf.end(), x, x + n); emit (out, false); }
    void finish (std::vector<float>& out) { emit (out, true); }

private:
    void emit (std::vector<float>& out, bool final)
    {
        const auto& h = halfbandTaps();
        const int M = ((int) h.size() - 1) / 2;
        const int64_t end = base + (int64_t) buf.size();
        auto at = [&] (int64_t i) { return i < base || i >= end ? 0.0f : buf[(size_t) (i - base)]; };
        while (final ? centre < end : centre + M < end)
        {
            double acc = h[(size_t) M] * at (centre);
            for (int k = 1; k <= M; k += 2)
                acc += h[(size_t) (M + k)] * (at (centre - k) + at (centre + k));
            out.push_back ((float) acc);
            centre += 2;
        }
        const int64_t keep = juce::jmax (base, centre - M);   // drop what no future output needs
        buf.erase (buf.begin(), buf.begin() + (ptrdiff_t) (keep - base));
        base = keep;
    }
};

/** Builds a playable source: loudness normalisation, 16-bit mip-map, overview. Not realtime.
    Takes the samples by reference and releases them as soon as level 0 is stored. */
inline std::unique_ptr<Source> makeSource (std::vector<float>& x, double sampleRate)
{
    auto s = std::make_unique<Source>();
    s->sampleRate = sampleRate;
    s->length = (int) x.size();
    if (x.empty())
        return s;

    // loudness: mean power of 20 ms windows within 30 dB of the loudest one (ignores silences)
    const int w = juce::jmax (64, (int) (0.02 * sampleRate));
    std::vector<double> pw;
    float peak = 1.0e-9f;
    for (int i = 0; i < (int) x.size(); i += w)
    {
        const int e = juce::jmin ((int) x.size(), i + w);
        double acc = 0.0;
        for (int j = i; j < e; ++j)
        {
            acc += (double) x[(size_t) j] * x[(size_t) j];
            peak = juce::jmax (peak, std::abs (x[(size_t) j]));
        }
        pw.push_back (acc / (e - i));
    }
    const double maxP = *std::max_element (pw.begin(), pw.end());
    double sum = 0.0;
    int cnt = 0;
    for (auto p : pw) if (p > maxP * 1.0e-3) { sum += p; ++cnt; }
    const double rms = cnt > 0 ? std::sqrt (sum / cnt) : 0.0;
    const float gain = rms > 1.0e-7 ? (float) juce::jmin (0.2 / rms, 40.0) : 1.0f;

    // 16 bit with the peak at 90 % of full scale (headroom for halfband overshoot in the lower levels)
    const float q = 29490.0f / peak;
    s->scale = gain / q;

    s->overview.assign (overviewBins, 0.0f);
    for (int i = 0; i < (int) x.size(); ++i)
    {
        auto& b = s->overview[(size_t) ((int64_t) i * overviewBins / (int64_t) x.size())];
        b = juce::jmax (b, std::abs (x[(size_t) i]) * gain);
    }

    auto store = [&] (int k, const float* d, int n, float mul)
    {
        auto& v = s->lv[(size_t) k];
        v.assign ((size_t) n + 2 * pad, 0);
        for (int i = 0; i < n; ++i)
            v[(size_t) (i + pad)] = (int16_t) juce::jlimit (-32767, 32767, (int) std::lrint (d[i] * mul));
    };
    store (0, x.data(), (int) x.size(), q);
    std::vector<float>().swap (x);   // free the float copy before building the smaller levels

    for (int k = 1; k < numLevels; ++k)
    {
        const auto& prev = s->lv[(size_t) k - 1];
        auto y = decimate2 (prev.data() + pad, (int) prev.size() - 2 * pad);
        store (k, y.data(), (int) y.size(), 1.0f);
    }
    return s;
}

//==============================================================================
// Built-in sources: synthesised once per process, all at C4.
namespace gen
{
constexpr double fs = 48000.0;
constexpr int chunk = 256;   // control rate for vibrato / spectra
constexpr double twoPi = juce::MathConstants<double>::twoPi;

// Bank of complex-rotation oscillators: one complex multiply per partial per sample,
// vectorised across partials. A rotation of magnitude < 1 makes a decaying partial.
struct Bank
{
    int n = 0;
    std::vector<float> re, im, cr, ci, amp;

    void init (int count, juce::Random& r)
    {
        n = count;
        re.resize ((size_t) n); im.resize ((size_t) n); cr.assign ((size_t) n, 1.0f); ci.assign ((size_t) n, 0.0f); amp.assign ((size_t) n, 0.0f);
        for (int k = 0; k < n; ++k)
        {
            const double ph = r.nextDouble() * twoPi;
            re[(size_t) k] = (float) std::cos (ph);
            im[(size_t) k] = (float) std::sin (ph);
        }
    }
    void setFreq (int k, double hz, double decayPerSample = 1.0)
    {
        cr[(size_t) k] = (float) (decayPerSample * std::cos (twoPi * hz / fs));
        ci[(size_t) k] = (float) (decayPerSample * std::sin (twoPi * hz / fs));
    }
    void setHarmonics (double f0)   // partial k = (k+1) * f0, via powers of one rotation
    {
        const std::complex<double> w1 = std::polar (1.0, twoPi * f0 / fs);
        std::complex<double> w = w1;
        for (int k = 0; k < n; ++k, w *= w1)
        {
            cr[(size_t) k] = (float) w.real();
            ci[(size_t) k] = (float) w.imag();
        }
    }
    void renorm()
    {
        for (int k = 0; k < n; ++k)
        {
            const float m = 1.0f / std::sqrt (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);
            re[(size_t) k] *= m;
            im[(size_t) k] *= m;
        }
    }
    float tick() noexcept
    {
#if defined (__clang__)
#pragma clang fp reassociate(on)
#endif
        float* __restrict a = re.data();
        float* __restrict b = im.data();
        const float* __restrict c = cr.data();
        const float* __restrict s = ci.data();
        const float* __restrict g = amp.data();
        float acc = 0.0f;
        for (int k = 0; k < n; ++k)
        {
            const float r = a[k] * c[k] - b[k] * s[k];
            const float i = a[k] * s[k] + b[k] * c[k];
            a[k] = r;
            b[k] = i;
            acc += g[k] * i;
        }
        return acc;
    }
};

inline float edges (double t, double len, double in, double out)
{
    return (float) juce::jlimit (0.0, 1.0, juce::jmin (t / in, (len - t) / out));
}

// sustained glass: each harmonic is an unequal pair beating slowly (the pairs never all cancel
// at once), brightness breathing, two faint inharmonic partials
inline std::vector<float> glass()
{
    const double len = 6.0;
    const int n = (int) (fs * len);
    std::vector<float> y ((size_t) n);
    juce::Random rnd (5);
    constexpr int K = 8;
    Bank b;
    b.init (2 * K + 2, rnd);
    float base[2 * K + 2] {};
    for (int k = 0; k < K; ++k)
    {
        const double beat = 0.06 + 0.14 * rnd.nextDouble();
        b.setFreq (2 * k,     rootHz * (k + 1) - 0.5 * beat);
        b.setFreq (2 * k + 1, rootHz * (k + 1) + 0.5 * beat);
        base[2 * k] = 0.62f;
        base[2 * k + 1] = 0.38f;
    }
    b.setFreq (2 * K, rootHz * 2.756);
    b.setFreq (2 * K + 1, rootHz * 5.404);
    for (int c = 0; c < n; c += chunk)
    {
        const double t = c / fs;
        const double bright = 0.5 + 0.5 * std::sin (twoPi * 0.13 * t);
        for (int k = 0; k < K; ++k)
        {
            const float a = (float) (std::pow (k + 1.0, -1.3) * ((k & 1) ? 0.55 : 1.0) * std::exp (-k * (0.45 - 0.25 * bright)));
            b.amp[(size_t) (2 * k)] = a * base[2 * k];
            b.amp[(size_t) (2 * k + 1)] = a * base[2 * k + 1];
        }
        b.amp[2 * K] = 0.05f * (float) (0.6 + 0.4 * std::sin (twoPi * 0.3 * t));
        b.amp[2 * K + 1] = 0.025f * (float) (0.6 + 0.4 * std::sin (twoPi * 0.5 * t));
        const float swell = (float) (0.85 + 0.15 * std::sin (twoPi * 0.21 * t + 1.0));
        for (int i = c; i < juce::jmin (n, c + chunk); ++i)
            y[(size_t) i] = b.tick() * swell * edges (i / fs, len, 0.08, 0.3);
        b.renorm();
    }
    return y;
}

// four voices singing a vowel that moves a -> o -> a; additive, formant envelope per harmonic
inline std::vector<float> choir()
{
    const double len = 6.0;
    const int n = (int) (fs * len);
    std::vector<float> y ((size_t) n, 0.0f);
    struct Formant { double f, bw, db; };
    const Formant A[4] { { 800, 80, 0 }, { 1150, 90, -6 }, { 2900, 120, -32 }, { 3900, 130, -20 } };
    const Formant O[4] { { 450, 70, 0 }, { 800, 80, -11 }, { 2830, 100, -22 }, { 3800, 130, -22 } };
    const double cents[4] { -9.0, 0.0, 8.0, -3.0 }, vibRate[4] { 5.1, 5.6, 4.8, 5.35 }, vibPh[4] { 0.0, 1.7, 3.1, 4.4 };
    constexpr int K = 34;
    juce::Random rnd (7);
    std::array<Bank, 4> banks;
    for (auto& b : banks) b.init (K, rnd);
    float b1 = 0.0f, b2 = 0.0f;
    for (int c = 0; c < n; c += chunk)
    {
        const double t = (c + 0.5 * chunk) / fs;
        const double m = 0.5 - 0.5 * std::cos (twoPi * t / len);
        double fF[4], fB[4], fG[4];
        for (int i = 0; i < 4; ++i)
        {
            fF[i] = A[i].f + (O[i].f - A[i].f) * m;
            fB[i] = A[i].bw + (O[i].bw - A[i].bw) * m;
            fG[i] = std::pow (10.0, (A[i].db + (O[i].db - A[i].db) * m) / 20.0);
        }
        for (int v = 0; v < 4; ++v)
        {
            const double vib = 16.0 * std::sin (twoPi * vibRate[v] * t + vibPh[v]) * juce::jmin (1.0, t / 0.6);
            const double f0 = rootHz * std::pow (2.0, (cents[v] + vib) / 1200.0);
            auto& b = banks[(size_t) v];
            b.setHarmonics (f0);
            for (int k = 0; k < K; ++k)
            {
                const double f = f0 * (k + 1);
                double e = 0.0;
                for (int i = 0; i < 4; ++i)
                {
                    const double x = (f - fF[i]) / (0.5 * fB[i]);
                    e += fG[i] / std::sqrt (1.0 + x * x);
                }
                b.amp[(size_t) k] = f < 9000.0 ? (float) (0.25 * e / (k + 1)) : 0.0f;
            }
        }
        for (int i = c; i < juce::jmin (n, c + chunk); ++i)
        {
            float acc = 0.0f;
            for (auto& b : banks) acc += b.tick();
            // a little breath: band-passed noise around 3 kHz
            const float w = rnd.nextFloat() * 2.0f - 1.0f;
            b1 += 0.35f * (w - b1);
            b2 += 0.35f * (b1 - b2);
            y[(size_t) i] = (acc + 0.02f * (b1 - b2)) * edges (i / fs, len, 0.15, 0.3);
        }
        for (auto& b : banks) b.renorm();
    }
    return y;
}

// a bowed ensemble: seven detuned saws plus two an octave below, gentle vibrato
inline std::vector<float> strings()
{
    const double len = 6.0;
    const int n = (int) (fs * len);
    std::vector<float> y ((size_t) n, 0.0f);
    struct Player { double cents, oct, rate, phase, amp; };
    std::vector<Player> pl;
    juce::Random rnd (3);
    for (int i = 0; i < 7; ++i)
        pl.push_back ({ -14.0 + 28.0 * i / 6.0, 1.0, 4.4 + 1.1 * rnd.nextDouble(), 6.28 * rnd.nextDouble(), 1.0 });
    pl.push_back ({ -5.0, 0.5, 4.7, 1.0, 0.45 });
    pl.push_back ({ 6.0, 0.5, 5.2, 2.0, 0.45 });
    constexpr int K = 32;
    std::vector<Bank> banks (pl.size());
    for (auto& b : banks) b.init (K, rnd);
    for (int c = 0; c < n; c += chunk)
    {
        const double t = (c + 0.5 * chunk) / fs;
        for (size_t v = 0; v < pl.size(); ++v)
        {
            const auto& p = pl[v];
            const double vib = 6.0 * std::sin (twoPi * p.rate * t + p.phase);
            const double f0 = rootHz * p.oct * std::pow (2.0, (p.cents + vib) / 1200.0);
            auto& b = banks[v];
            b.setHarmonics (f0);
            for (int k = 0; k < K; ++k)
            {
                const double f = f0 * (k + 1);
                b.amp[(size_t) k] = f < 8500.0 ? (float) (0.15 * p.amp / (k + 1) / std::sqrt (1.0 + (f / 2200.0) * (f / 2200.0))) : 0.0f;
            }
        }
        for (int i = c; i < juce::jmin (n, c + chunk); ++i)
        {
            float acc = 0.0f;
            for (auto& b : banks) acc += b.tick();
            y[(size_t) i] = acc * edges (i / fs, len, 0.6, 0.4) * (float) (0.9 + 0.1 * std::sin (0.9 * i / fs));
        }
        for (auto& b : banks) b.renorm();
    }
    return y;
}

// wind in a pipe: noise through narrow resonances at the harmonics of C4, plus air
inline std::vector<float> breath()
{
    const double len = 6.0;
    const int n = (int) (fs * len);
    std::vector<float> y ((size_t) n, 0.0f);
    juce::Random rnd (21);
    struct Bp { float b0, a1, a2, z1 = 0, z2 = 0, g; };
    std::vector<Bp> f;
    for (int k = 1; k <= 8; ++k)
    {
        const double w0 = twoPi * rootHz * k / fs;
        const double q = 90.0 / std::pow (k, 0.3);
        const double alpha = std::sin (w0) / (2.0 * q), a0 = 1.0 + alpha;
        f.push_back ({ (float) (alpha / a0), (float) (-2.0 * std::cos (w0) / a0), (float) ((1.0 - alpha) / a0), 0, 0, (float) std::pow (k, -0.8) });
    }
    float air = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float w = rnd.nextFloat() * 2.0f - 1.0f;
        float acc = 0.0f;
        for (auto& b : f)
        {
            const float out = b.b0 * w + b.z1;          // transposed direct form II, b1 = 0, b2 = -b0
            b.z1 = -b.a1 * out + b.z2;
            b.z2 = -b.b0 * w - b.a2 * out;
            acc += out * b.g;
        }
        air += 0.12f * (w - air);
        const double t = i / fs;
        const float swell = (float) (0.7 + 0.3 * std::sin (twoPi * 0.23 * t) * std::sin (twoPi * 0.07 * t + 0.5));
        y[(size_t) i] = (acc * 6.0f + air * 0.25f) * swell * edges (t, len, 0.4, 0.4);
    }
    return y;
}

// a music box playing a pentatonic line in C
inline std::vector<float> musicBox()
{
    const int notes[] { 0, 7, 12, 4, 16, 9, 19, 14, 24, 12, 7, 2, 9, 14, 4, 19, 0, 12 };
    const double step = 0.36, len = step * (double) std::size (notes) + 2.2;
    const int n = (int) (fs * len);
    std::vector<float> y ((size_t) n, 0.0f);
    struct Partial { double ratio, amp, tau; };
    const Partial parts[] { { 1.0, 1.0, 1.7 }, { 2.0, 0.12, 0.9 }, { 3.01, 0.16, 0.45 }, { 5.93, 0.07, 0.22 }, { 9.1, 0.03, 0.1 } };
    juce::Random rnd (9);
    for (size_t k = 0; k < std::size (notes); ++k)
    {
        const int start = (int) (k * step * fs);
        const double f0 = rootHz * std::pow (2.0, notes[k] / 12.0);
        const double vel = 0.75 + 0.25 * (double) ((k * 7) % 5) / 4.0;
        Bank b;
        b.init ((int) std::size (parts), rnd);
        for (int p = 0; p < (int) std::size (parts); ++p)
        {
            b.re[(size_t) p] = 1.0f; b.im[(size_t) p] = 0.0f;   // sine phase: starts from zero
            b.setFreq (p, f0 * parts[p].ratio, std::exp (-1.0 / (parts[p].tau * fs)));
            b.amp[(size_t) p] = f0 * parts[p].ratio < 16000.0 ? (float) (vel * parts[p].amp) : 0.0f;
        }
        const int dur = juce::jmin (n - start, (int) (5.0 * parts[0].tau * fs));
        const int atk = (int) (0.002 * fs);
        for (int i = 0; i < dur; ++i)
            y[(size_t) (start + i)] += b.tick() * (i < atk ? (float) i / atk : 1.0f);
    }
    for (int i = 0; i < n; ++i)
        y[(size_t) i] *= edges (i / fs, len, 0.001, 0.5);
    return y;
}
} // namespace gen

constexpr int numBuiltins = 5;
inline const char* builtinName (int i)
{
    static const char* names[numBuiltins] { "Glass", "Choir", "Strings", "Breath", "Music Box" };
    return names[juce::jlimit (0, numBuiltins - 1, i)];
}

struct Builtins
{
    std::array<std::unique_ptr<Source>, numBuiltins> src;
    Builtins()
    {
        auto mk = [] (std::vector<float> v) { return makeSource (v, gen::fs); };
        src[0] = mk (gen::glass());
        src[1] = mk (gen::choir());
        src[2] = mk (gen::strings());
        src[3] = mk (gen::breath());
        src[4] = mk (gen::musicBox());
    }
};
inline const Builtins& builtins() { static const Builtins b; return b; }

//==============================================================================
struct Settings
{
    int source = 0;               // 0..numBuiltins-1 built-in, numBuiltins = user sample
    float position = 0.35f, scan = 0.0f, spray = 0.12f, drift = 0.3f;
    float sizeMs = 160.0f, density = 18.0f, shape = 0.25f;
    float transpose = 0.0f, jitter = 6.0f, octaves = 0.0f, reverse = 0.0f;
    float spread = 0.7f, attack = 0.25f, release = 1.8f;
    float tone = 0.8f, reverbMix = 0.35f, reverbDecay = 6.0f, volume = 0.7f;
};

class Engine
{
public:
    static constexpr int maxVoices = 16, maxPoly = 12, maxGrains = 40, blockSize = 64, maxDisplay = 128;

    Engine() { for (auto& p : dispPos) p = 0.0f; for (auto& a : dispAmp) a = 0.0f; for (auto& i : dispId) i = 0; for (auto& h : dispHead) h = -1.0f; }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i <= riseRes; ++i)
            riseTab[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) i / riseRes);
        lastRevMix = lastRevDecay = -1.0f;
        setSettings (set);        // hands the reverb its character before it is built
        reverb.prepare (fs);
        int seed = 1;
        for (auto& v : voices) { v = Voice(); v.rng.setSeed (0x9e3779b9LL * seed++); }
        svf = {};
        volCur = set.volume;
        bendCur = bendTarget;
        kVol = 1.0f - std::exp (-1.0f / (0.03f * (float) fs));
        toneCur = toneTarget;
    }

    void setSettings (const Settings& s)
    {
        set = s;
        // incoherent sum of `overlap` grains: normalise by sqrt of the overlapping window energy
        const float a = 1.0f - 0.85f * juce::jlimit (0.0f, 1.0f, s.shape);
        const float meanW2 = (1.0f - a) + a * 0.375f;
        const float overlap = juce::jmin ((float) maxGrains, s.density * s.sizeMs * 0.001f);   // the pool caps it
        normGain = 1.0f / std::sqrt (juce::jmax (1.0f, overlap * meanW2));
        tukey = a;
        toneTarget = (float) juce::jmin (0.45 * fs, 250.0 * std::pow (80.0, (double) juce::jlimit (0.0f, 1.0f, s.tone)));
        if (s.reverbMix != lastRevMix || s.reverbDecay != lastRevDecay)
        {
            lastRevMix = s.reverbMix;
            lastRevDecay = s.reverbDecay;
            ReverbEngine::Params p;
            p.character = ReverbEngine::ambient;
            p.predelayMs = 18.0f;
            p.size = juce::jlimit (0.2f, 0.9f, 0.2f + 0.7f * s.reverbDecay / 20.0f);
            p.decayS = s.reverbDecay;
            p.diffusion = 0.85f;
            p.bloom = 0.35f;
            p.early = 0.15f;
            p.bassMult = 1.0f;
            p.dampHz = 4200.0f;
            p.trebleMult = 0.45f;
            p.modDepth = 0.55f;
            p.modRate = 0.45f;
            p.lowCutHz = 140.0f;
            p.highCutHz = 11000.0f;
            p.width = 1.15f;
            p.mix = s.reverbMix;
            reverb.setParams (p);
        }
    }

    void setUserSource (const Source* s) noexcept { user = s; }
    const Source* currentSource() const noexcept { return sourceFor (set.source); }

    bool uses (const Source* s) const noexcept
    {
        for (auto& v : voices)
            if (v.active)
                for (int i = 0; i < v.numGrains; ++i)
                    if (v.grains[(size_t) i].src == s)
                        return true;
        return false;
    }

    //==============================================================================
    void noteOn (int note, float velocity)
    {
        // a held note re-struck: let the old voice ring out
        for (auto& v : voices)
            if (v.active && v.note == note && ! v.released)
                release (v);

        int sounding = 0;
        Voice* freeV = nullptr;
        for (auto& v : voices)
        {
            if (v.active && ! v.stealing) ++sounding;
            if (! v.active && freeV == nullptr) freeV = &v;
        }
        if (sounding >= maxPoly || freeV == nullptr)
        {
            Voice* victim = nullptr;
            for (int pass = 0; pass < 2 && victim == nullptr; ++pass)
                for (auto& v : voices)
                    if (v.active && ! v.stealing && (pass == 1 || v.released) && (victim == nullptr || v.order < victim->order))
                        victim = &v;
            if (victim != nullptr)
            {
                victim->stealing = true;
                victim->released = true;
                victim->stage = 2;
                victim->relMul = std::exp (std::log (1.0e-4f) / (0.008f * (float) fs));
            }
        }
        if (freeV == nullptr)   // all slots busy fading: take the oldest outright
        {
            freeV = &voices[0];
            for (auto& v : voices)
                if (v.order < freeV->order) freeV = &v;
        }

        auto& v = *freeV;
        v.active = true;
        v.released = v.held = v.stealing = false;
        v.note = note;
        v.vel = 0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, velocity);
        v.env = 0.0f;
        v.stage = 0;
        v.scan = 0.0;
        v.countdown = 0.0;
        v.numGrains = 0;
        v.order = ++orderCounter;
        v.nPos.reset (v.rng);
        v.nPitch.reset (v.rng);
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && ! v.released)
            {
                if (sustainOn) v.held = true;
                else release (v);
            }
    }

    void sustain (bool on)
    {
        sustainOn = on;
        if (! on)
            for (auto& v : voices)
                if (v.active && v.held && ! v.released)
                    release (v);
    }

    void pitchBend (float x) noexcept { bendTarget = 2.0f * juce::jlimit (-1.0f, 1.0f, x); }
    void modWheel (float x) noexcept { mod = juce::jlimit (0.0f, 1.0f, x); }
    void allNotesOff() { for (auto& v : voices) if (v.active && ! v.released) release (v); sustainOn = false; }

    //==============================================================================
    void render (float* L, float* R, int n) noexcept
    {
        for (int done = 0; done < n;)
        {
            const int m = juce::jmin (blockSize, n - done);
            renderBlock (L + done, R + done, m);
            done += m;
        }
        reverb.process (L, R, n);
        for (int i = 0; i < n; ++i)
        {
            volCur += (set.volume - volCur) * kVol;
            L[i] *= volCur;
            R[i] *= volCur;
        }
        publishDisplay();
    }

    // GUI snapshot (written by the audio thread once per block)
    std::array<std::atomic<float>, maxDisplay> dispPos, dispAmp;
    std::array<std::atomic<uint32_t>, maxDisplay> dispId;
    std::atomic<int> dispCount { 0 };
    std::array<std::atomic<float>, maxVoices> dispHead;   // per-voice playhead (0..1) or -1

private:
    struct Grain
    {
        const Source* src = nullptr;
        double pos = 0.0, inc = 0.0;   // in samples of mip level `lvl`
        int lvl = 0, age = 0, total = 0, delay = 0;
        float rise = 1.0f, invRise = 1.0f, gl = 0.0f, gr = 0.0f;
        uint32_t id = 0;
    };

    struct ValueNoise   // smooth random wander in -1..1
    {
        float a = 0.0f, b = 0.0f, t = 0.0f;
        void reset (juce::Random& r) { a = r.nextFloat() * 2.0f - 1.0f; b = r.nextFloat() * 2.0f - 1.0f; t = r.nextFloat(); }
        float step (juce::Random& r, float dt) noexcept
        {
            t += dt;
            if (t >= 1.0f) { t -= 1.0f; a = b; b = r.nextFloat() * 2.0f - 1.0f; }
            const float s = t * t * (3.0f - 2.0f * t);
            return a + (b - a) * s;
        }
    };

    struct Voice
    {
        bool active = false, released = false, held = false, stealing = false;
        int note = 60, stage = 0;
        float vel = 1.0f, env = 0.0f, relMul = 0.999f;
        double scan = 0.0, countdown = 0.0, head = 0.0;
        uint64_t order = 0;
        juce::Random rng;
        ValueNoise nPos, nPitch;
        float driftPos = 0.0f, driftPitch = 0.0f;
        std::array<Grain, maxGrains> grains {};
        int numGrains = 0;
    };

    const Source* sourceFor (int i) const noexcept
    {
        if (i >= 0 && i < numBuiltins) return builtins().src[(size_t) i].get();
        return user;
    }

    void release (Voice& v) noexcept
    {
        v.released = true;
        v.held = false;
        v.stage = 2;
        v.relMul = std::exp (std::log (1.0e-4f) / (juce::jmax (0.01f, set.release) * (float) fs));
    }

    static double fold (double f) noexcept
    {
        f = std::fmod (f, 2.0);
        if (f < 0.0) f += 2.0;
        return f > 1.0 ? 2.0 - f : f;
    }

    void spawn (Voice& v, int offset) noexcept
    {
        const Source* s = currentSource();
        if (s == nullptr || s->length < 256 || v.numGrains >= maxGrains)
            return;
        auto& rng = v.rng;
        const double len = s->length;

        double semis = v.note - 60 + set.transpose + set.jitter * 0.01 * (rng.nextFloat() - rng.nextFloat())
                       + 0.12 * set.drift * v.driftPitch;
        if (set.octaves > 0.0f && rng.nextFloat() < set.octaves)
        {
            const float r = rng.nextFloat();
            semis += r < 0.6f ? 12.0 : (r < 0.85f ? 19.0 : -12.0);
        }
        const double rate = std::exp2 (semis / 12.0) * s->sampleRate / fs;

        int total = (int) (set.sizeMs * 0.001 * fs);
        const double margin = 1.13;   // pitch bend may stretch the grain's span while it plays
        total = juce::jmin (total, (int) ((len - 8.0) / (rate * margin)));
        if (total < 16)
            return;
        const double span = total * rate * margin;

        const double spray = juce::jmin (1.0, (double) set.spray + 0.5 * mod);
        const double f = fold (v.head + spray * (rng.nextFloat() * 2.0 - 1.0) * 0.5);
        const double start = 2.0 + f * (len - span - 4.0);
        const bool rev = set.reverse > 0.0f && rng.nextFloat() < set.reverse;

        int k = 0;
        double r = rate;
        while (r > 1.0 && k < numLevels - 1) { r *= 0.5; ++k; }
        const double scale = 1.0 / (double) (1 << k);

        auto& g = v.grains[(size_t) v.numGrains++];
        g.src = s;
        g.lvl = k;
        g.inc = rev ? -r : r;
        g.pos = (rev ? start + total * rate : start) * scale;
        g.age = 0;
        g.total = total;
        g.delay = offset;
        g.rise = juce::jmax (1.0f, tukey * (float) total * 0.5f);
        g.invRise = 1.0f / g.rise;
        const float pan = set.spread * (rng.nextFloat() * 2.0f - 1.0f);
        const float ang = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float amp = v.vel * normGain * juce::MathConstants<float>::sqrt2;
        g.gl = amp * std::cos (ang);
        g.gr = amp * std::sin (ang);
        g.id = ++grainCounter;
    }

    float window (const Grain& g) const noexcept
    {
        const float a = (float) g.age;
        float u;
        if (a < g.rise) u = a * g.invRise;
        else
        {
            const float b = (float) (g.total - g.age);
            if (b >= g.rise) return 1.0f;
            u = b * g.invRise;
        }
        const float x = juce::jmin (1.0f, u) * (float) riseRes;
        const int i = juce::jmin (riseRes - 1, (int) x);
        return riseTab[(size_t) i] + (x - (float) i) * (riseTab[(size_t) i + 1] - riseTab[(size_t) i]);
    }

    void renderBlock (float* L, float* R, int m) noexcept
    {
        std::fill (L, L + m, 0.0f);
        std::fill (R, R + m, 0.0f);
        bendCur += (bendTarget - bendCur) * 0.25f;
        const double bendMul = std::exp2 (bendCur / 12.0);
        const float dt = (float) m / (float) fs;

        const Source* s = currentSource();
        const double interval = fs / juce::jmax (0.1f, set.density);

        for (auto& v : voices)
        {
            if (! v.active) continue;

            // playhead: Position + Scan (multiple of real time through the source) + Drift
            v.driftPos = v.nPos.step (v.rng, dt * 0.35f);
            v.driftPitch = v.nPitch.step (v.rng, dt * 0.5f);
            if (s != nullptr && s->length > 0)
                v.scan += set.scan * m * (s->sampleRate / fs) / s->length;
            v.head = fold (set.position + v.scan + 0.08 * set.drift * v.driftPos);

            while (v.countdown < m)
            {
                spawn (v, juce::jmax (0, (int) v.countdown));
                v.countdown += interval * (0.4 + 1.2 * v.rng.nextDouble());
            }
            v.countdown -= m;

            std::fill (tmpL.begin(), tmpL.begin() + m, 0.0f);
            std::fill (tmpR.begin(), tmpR.begin() + m, 0.0f);
            for (int gi = 0; gi < v.numGrains;)
            {
                auto& g = v.grains[(size_t) gi];
                const Source& src = *g.src;
                const double inc = g.inc * bendMul;
                for (int i = g.delay; i < m && g.age < g.total; ++i, ++g.age)
                {
                    const float x = src.read (g.lvl, g.pos) * window (g);
                    g.pos += inc;
                    tmpL[(size_t) i] += x * g.gl;
                    tmpR[(size_t) i] += x * g.gr;
                }
                g.delay = 0;
                if (g.age >= g.total)
                    g = v.grains[(size_t) --v.numGrains];
                else
                    ++gi;
            }

            const float atkInc = 1.0f / (juce::jmax (0.001f, set.attack) * (float) fs);
            for (int i = 0; i < m; ++i)
            {
                if (v.stage == 0)
                {
                    v.env += atkInc;
                    if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
                }
                else if (v.stage == 2)
                    v.env *= v.relMul;
                L[i] += tmpL[(size_t) i] * v.env;
                R[i] += tmpR[(size_t) i] * v.env;
            }
            if (v.stage == 2 && v.env < 1.0e-4f)
            {
                v.active = false;
                v.numGrains = 0;
            }
        }

        // tone: 12 dB/oct low-pass (TPT SVF)
        toneCur += (toneTarget - toneCur) * 0.2f;
        const float gC = (float) std::tan (juce::MathConstants<double>::pi * toneCur / fs);
        constexpr float k = 1.4142135f;
        const float a1 = 1.0f / (1.0f + gC * (gC + k)), a2 = gC * a1, a3 = gC * a2;
        for (int c = 0; c < 2; ++c)
        {
            float* x = c == 0 ? L : R;
            auto& st = svf[(size_t) c];
            for (int i = 0; i < m; ++i)
            {
                const float v3 = x[i] - st[1];
                const float v1 = a1 * st[0] + a2 * v3;
                const float v2 = st[1] + a2 * st[0] + a3 * v3;
                st[0] = 2.0f * v1 - st[0];
                st[1] = 2.0f * v2 - st[1];
                x[i] = v2;
            }
        }
    }

    void publishDisplay() noexcept
    {
        const Source* s = currentSource();
        int c = 0;
        for (size_t vi = 0; vi < voices.size(); ++vi)
        {
            const auto& v = voices[vi];
            dispHead[vi].store (v.active && ! v.released ? (float) v.head : -1.0f, std::memory_order_relaxed);
            if (! v.active || s == nullptr) continue;
            for (int i = 0; i < v.numGrains && c < maxDisplay; ++i)
            {
                const auto& g = v.grains[(size_t) i];
                if (g.src != s || g.delay > 0) continue;
                dispPos[(size_t) c].store ((float) (g.pos * (1 << g.lvl) / s->length), std::memory_order_relaxed);
                dispAmp[(size_t) c].store (window (g) * v.env * v.vel, std::memory_order_relaxed);
                dispId[(size_t) c].store (g.id, std::memory_order_relaxed);
                ++c;
            }
        }
        dispCount.store (c, std::memory_order_release);
    }

    static constexpr int riseRes = 1024;
    std::array<float, riseRes + 1> riseTab {};

    double fs = 48000.0;
    Settings set;
    const Source* user = nullptr;
    std::array<Voice, maxVoices> voices;
    std::array<float, blockSize> tmpL {}, tmpR {};
    std::array<std::array<float, 2>, 2> svf {};
    ReverbEngine reverb;
    float lastRevMix = -1.0f, lastRevDecay = -1.0f;
    float normGain = 1.0f, tukey = 1.0f, toneTarget = 8000.0f, toneCur = 8000.0f;
    float volCur = 0.7f, kVol = 0.001f, bendTarget = 0.0f, bendCur = 0.0f, mod = 0.0f;
    bool sustainOn = false;
    uint64_t orderCounter = 0;
    uint32_t grainCounter = 0;
};
} // namespace pl
