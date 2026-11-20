#pragma once

// Physical mode tables for the Ictus resonators.
// Every resonator is described as a set of modes: frequency ratio, how strongly the
// strike excites it (mode shape at the hit point), how strongly each "microphone" hears
// it (mode shape at the listening points) and a relative decay time.
// Mode shapes are normalised to unit mean-square over the body so that all types sit
// at comparable levels.

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace ictus
{
constexpr int maxModes = 128;
constexpr int shapeRes = 129;
constexpr double pi = 3.14159265358979323846;

enum ResType { beam = 0, marimba, string, membrane, plate, pipe, bell, bowl, numTypes };

struct ResParams
{
    int type = marimba;
    int modes = 64;
    float material = 0.5f, inharm = 0.0f, shape = 0.0f, hit = 0.3f, listen = 0.3f, width = 0.5f, bright = 0.0f;
    bool operator== (const ResParams&) const = default;
};

struct Layout
{
    int count = 0;
    std::array<float, maxModes> ratio {}, exc {}, outL {}, outR {}, decay {};
};

//==============================================================================
// Euler-Bernoulli beam mode shapes, written so that large beta values do not cancel.
inline double freeFreeShape (double b, double x)
{
    // phi = cosh bx + cos bx - s (sinh bx + sin bx),  s = (cosh b - cos b) / (sinh b - sin b)
    const double den = 0.5 * (std::exp (b * (1.0 - x)) - std::exp (-b * (1.0 + x))) - std::sin (b) * std::exp (-b * x);
    const double oneMinusS_e = (std::cos (b) - std::sin (b) - std::exp (-b)) / den;           // (1-s) e^{bx}
    const double s = 1.0 - (std::cos (b) - std::sin (b) - std::exp (-b)) / (std::sinh (b) - std::sin (b));
    return 0.5 * (oneMinusS_e + (1.0 + s) * std::exp (-b * x)) + std::cos (b * x) - s * std::sin (b * x);
}

inline double clampedFreeShape (double b, double x)
{
    // phi = cosh bx - cos bx - s (sinh bx - sin bx),  s = (cosh b + cos b) / (sinh b + sin b)
    const double den = 0.5 * (std::exp (b * (1.0 - x)) - std::exp (-b * (1.0 + x))) + std::sin (b) * std::exp (-b * x);
    const double num = std::sin (b) - std::cos (b) - std::exp (-b);
    const double oneMinusS_e = num / den;
    const double s = 1.0 - num / (std::sinh (b) + std::sin (b));
    return 0.5 * (oneMinusS_e + (1.0 + s) * std::exp (-b * x)) - std::cos (b * x) + s * std::sin (b * x);
}

// Bessel J_m via its integral representation (trapezoid rule is spectrally accurate here).
inline double besselJ (int m, double x)
{
    const int n = std::max (24, (int) (x + m) + 24);
    double s = 0.0;
    for (int k = 0; k <= n; ++k)
    {
        const double t = pi * k / n;
        s += ((k == 0 || k == n) ? 0.5 : 1.0) * std::cos (m * t - x * std::sin (t));
    }
    return s / n;
}

struct MembraneMode { int m; int k; double j; double ratio; std::array<float, shapeRes> shape; };

struct Tables
{
    static constexpr double ffB0[] { 4.7300407449, 7.8532046241, 10.9956078380, 14.1371654913 };
    static constexpr double cfB0[] { 1.8751040687, 4.6940911330, 7.8547574382, 10.9955407349 };
    std::array<double, maxModes> ffRatio {}, cfRatio {}, ffBeta {}, cfBeta {};
    std::vector<MembraneMode> membrane;                    // sorted by frequency

    Tables()
    {
        // ---- beams (these shape formulas already have unit mean square)
        for (int n = 0; n < maxModes; ++n)
        {
            ffBeta[(size_t) n] = n < 4 ? ffB0[n] : (2 * (n + 1) + 1) * pi * 0.5;
            cfBeta[(size_t) n] = n < 4 ? cfB0[n] : (2 * (n + 1) - 1) * pi * 0.5;
            ffRatio[(size_t) n] = (ffBeta[(size_t) n] / ffB0[0]) * (ffBeta[(size_t) n] / ffB0[0]);
            cfRatio[(size_t) n] = (cfBeta[(size_t) n] / cfB0[0]) * (cfBeta[(size_t) n] / cfB0[0]);
        }

        // ---- circular membrane: zeros of J_m
        std::vector<MembraneMode> all;
        constexpr double xMax = 30.0;
        for (int m = 0; m <= 26; ++m)
        {
            double x0 = m == 0 ? 0.5 : (double) m, f0 = besselJ (m, x0);
            int order = 0;
            for (double x1 = x0 + 0.1; x1 < xMax; x1 += 0.1)
            {
                const double f1 = besselJ (m, x1);
                if ((f0 > 0) != (f1 > 0))
                {
                    double a = x1 - 0.1, b = x1, fa = f0;
                    for (int it = 0; it < 48; ++it)
                    {
                        const double c = 0.5 * (a + b), fc = besselJ (m, c);
                        if ((fc > 0) == (fa > 0)) { a = c; fa = fc; } else b = c;
                    }
                    MembraneMode mm;
                    mm.m = m;
                    mm.k = ++order;
                    mm.j = 0.5 * (a + b);
                    all.push_back (mm);
                }
                f0 = f1;
            }
        }
        std::sort (all.begin(), all.end(), [] (auto& a, auto& b) { return a.j < b.j; });
        all.resize ((size_t) maxModes);
        const double j01 = all[0].j;
        for (auto& mm : all)
        {
            mm.ratio = mm.j / j01;
            const double jn1 = besselJ (mm.m + 1, mm.j);
            const double ms = (mm.m == 0 ? 2.0 : 1.0) * jn1 * jn1 * 0.5;   // mean square over the disk
            const double norm = 1.0 / std::sqrt (ms);
            for (int i = 0; i < shapeRes; ++i)
                mm.shape[(size_t) i] = (float) (besselJ (mm.m, mm.j * i / (shapeRes - 1)) * norm);
        }
        membrane = std::move (all);
    }
};

inline const Tables& tables()
{
    static const Tables t;
    return t;
}

inline float lookup (const float* table, double x)
{
    const double p = std::clamp (x, 0.0, 1.0) * (shapeRes - 1);
    const int i = std::min ((int) p, shapeRes - 2);
    const float f = (float) (p - i);
    return table[i] + f * (table[i + 1] - table[i]);
}

//==============================================================================
struct BellPartial { double ratio; int m; double amp, decay; };

inline const std::vector<BellPartial>& bellPartials()
{
    static const std::vector<BellPartial> list = []
    {
        std::vector<BellPartial> v {
            { 0.500, 2, 0.55, 3.0 }, { 1.000, 2, 0.75, 1.8 }, { 1.200, 2, 0.80, 1.4 }, { 1.500, 3, 0.35, 1.0 },
            { 2.000, 3, 1.00, 0.9 }, { 2.510, 3, 0.55, 0.7 }, { 2.660, 4, 0.60, 0.7 }, { 3.010, 4, 0.45, 0.6 },
            { 3.330, 4, 0.40, 0.55 }, { 4.000, 5, 0.38, 0.5 }, { 4.210, 5, 0.32, 0.45 }, { 4.500, 5, 0.30, 0.42 },
            { 5.000, 6, 0.28, 0.4 }, { 5.330, 6, 0.24, 0.38 }, { 5.950, 6, 0.22, 0.35 }, { 6.650, 7, 0.20, 0.32 } };
        while (v.size() < maxModes / 2)
        {
            const auto& p = v.back();
            const double k = (double) v.size();
            v.push_back ({ p.ratio * (1.075 + 0.02 * std::sin (k * 2.3)), 7 + (int) (k - 15) / 3, p.amp * 0.93, p.decay * 0.96 });
        }
        return v;
    }();
    return list;
}

inline const std::vector<BellPartial>& bowlPartials()
{
    static const std::vector<BellPartial> list = []
    {
        std::vector<BellPartial> v;
        auto g = [] (double n) { return n * (n * n - 1.0) / std::sqrt (n * n + 1.0); };
        for (int n = 2; (int) v.size() < maxModes / 2; ++n)
            v.push_back ({ g (n) / g (2), n, 1.0 / std::pow (n - 1.0, 0.7), 1.0 });
        return v;
    }();
    return list;
}

inline const char* shapeName (int type)
{
    switch (type)
    {
        case beam:     return "CLAMP";
        case marimba:  return "TUNING";
        case string:   return "STIFFNESS";
        case membrane: return "TENSION";
        case plate:    return "ASPECT";
        case pipe:     return "OPENING";
        case bell:     return "THIRD";
        case bowl:     return "WARBLE";
        default:       return "SHAPE";
    }
}

//==============================================================================
// Build the mode layout for one resonator. Allocation free, safe on the audio thread
// (after tables() has been constructed once).
inline void buildLayout (const ResParams& p, Layout& out)
{
    const auto& T = tables();
    const int want = std::clamp (p.modes, 4, maxModes);

    struct M { double ratio, e, l, r, amp, dec; };
    std::array<M, maxModes> md {};
    int n = 0;

    const double hit = std::clamp ((double) p.hit, 0.0, 1.0);
    const double lisL = std::clamp ((double) p.listen - 0.2 * p.width, 0.0, 1.0);
    const double lisR = std::clamp ((double) p.listen + 0.2 * p.width, 0.0, 1.0);
    const double shape = std::clamp ((double) p.shape, 0.0, 1.0);
    const double width = std::clamp ((double) p.width, 0.0, 1.0);

    switch (p.type)
    {
        case beam:
        case marimba:
        {
            // 0 = centre of the bar, 1 = towards the end (free tip for the clamped bar)
            auto xff = [] (double q) { return 0.5 - 0.49 * q; };
            auto xcf = [] (double q) { return 0.5 + 0.49 * q; };
            const double clamp = p.type == beam ? shape : 0.0;
            for (; n < want; ++n)
            {
                const double bf = T.ffBeta[(size_t) n], bc = T.cfBeta[(size_t) n];
                auto sh = [&] (double q)
                {
                    double v = 0.0;
                    if (clamp < 1.0) v += (1.0 - clamp) * freeFreeShape (bf, xff (q));
                    if (clamp > 0.0) v += clamp * clampedFreeShape (bc, xcf (q));
                    return v;
                };
                double ratio = std::exp ((1.0 - clamp) * std::log (T.ffRatio[(size_t) n]) + clamp * std::log (T.cfRatio[(size_t) n]));
                double amp = 1.0, dec = 1.0;
                if (p.type == marimba)
                {
                    // undercut bar: first overtones tuned (xylophone 1:3:6 .. marimba 1:4:10),
                    // higher ones follow the bar series compressed onto the tuned third
                    const double tuned[] { 1.0, 3.0 + shape, 6.0 + 4.0 * shape };
                    if (n < 3) ratio = tuned[n];
                    else       ratio = tuned[2] * std::pow (T.ffRatio[(size_t) n] / T.ffRatio[2], 0.85);
                    if (n == 0) { amp = 1.6; dec = 1.4; }   // resonator tube under the bar
                }
                md[(size_t) n] = { ratio, sh (hit), sh (lisL), sh (lisR), amp, dec };
            }
            break;
        }

        case string:
        case pipe:
        {
            const double B = 0.004 * shape * shape;                 // stiffness (string)
            const double closed = p.type == pipe ? 1.0 - shape : 0.0; // pipe opening
            auto k = [&] (int i) { return (i + 1) - 0.5 * closed; };
            const double k1 = k (0);
            const double f1 = k1 * std::sqrt (1.0 + B * k1 * k1);
            auto pos = [] (double q) { return 0.5 - 0.495 * q; };
            for (; n < want; ++n)
            {
                const double kn = k (n);
                const double ratio = kn * std::sqrt (1.0 + B * kn * kn) / f1;
                auto sh = [&] (double q) { return std::sqrt (2.0) * std::sin (kn * pi * pos (q)); };
                md[(size_t) n] = { ratio, sh (hit), sh (lisL), sh (lisR), p.type == pipe ? 1.0 / std::sqrt (kn / k1) : 1.0, 1.0 };
            }
            break;
        }

        case membrane:
        {
            const double j11 = T.membrane[1].j;   // (1,1) mode
            const double j01 = T.membrane[0].j;
            const double thL = 0.15 * width * pi, thR = -0.45 * width * pi;
            for (; n < want; ++n)
            {
                const auto& mm = T.membrane[(size_t) n];
                double ratio = mm.ratio, dec = 1.0;
                const bool principal = mm.m >= 1 && mm.k == 1;
                if (principal)
                {
                    // air loading pulls the (m,1) family towards a harmonic series (kettle drums, tabla)
                    const double harm = (j11 / j01) * (mm.m + 1) * 0.5;
                    ratio = std::exp ((1.0 - shape) * std::log (ratio) + shape * std::log (harm));
                }
                if (mm.m == 0 && n == 0) dec = 1.0 - 0.6 * shape;    // the breathing mode radiates and dies fast
                const double r = 0.92;
                const double e = lookup (mm.shape.data(), r * hit);
                const double l = lookup (mm.shape.data(), r * lisL) * std::cos (mm.m * thL);
                const double rr = lookup (mm.shape.data(), r * lisR) * std::cos (mm.m * thR);
                md[(size_t) n] = { ratio, e, l, rr, 1.0, dec };
            }
            break;
        }

        case plate:
        {
            // simply supported rectangular plate, sides 1 x A
            const double A = 1.0 + 1.6 * shape;
            struct C { double f; int m, k; };
            std::array<C, 24 * 24> c {};
            int nc = 0;
            for (int m = 1; m <= 24; ++m)
                for (int k = 1; k <= 24; ++k)
                    c[(size_t) nc++] = { m * m + k * k / (A * A), m, k };
            std::partial_sort (c.begin(), c.begin() + want, c.begin() + nc, [] (auto& a, auto& b) { return a.f < b.f; });
            const double f11 = 1.0 + 1.0 / (A * A);
            auto pt = [] (double q, double sx, double sy) { return std::pair<double, double> { 0.5 - 0.45 * q * sx, 0.5 - 0.45 * q * sy }; };
            const auto h = pt (hit, 1.0, 0.618);
            const auto L = pt (lisL, 1.0, 0.382);
            auto R = pt (lisR, 1.0, 0.382);
            R.first = R.first + width * (1.0 - 2.0 * R.first);      // mirror across the plate for width
            for (; n < want; ++n)
            {
                const auto& cc = c[(size_t) n];
                auto sh = [&] (std::pair<double, double> q) { return 2.0 * std::sin (cc.m * pi * q.first) * std::sin (cc.k * pi * q.second); };
                md[(size_t) n] = { cc.f / f11, sh (h), sh (L), sh (R), 1.0, 1.0 };
            }
            break;
        }

        case bell:
        case bowl:
        {
            const auto& list = p.type == bell ? bellPartials() : bowlPartials();
            const double split = p.type == bell ? 0.0012 : 0.0003 + 0.015 * shape * shape;
            const double alpha = 0.31;                          // strike angle against the mode axes
            const double thL = (p.listen * 0.5 - 0.3 * width) * pi;
            const double thR = (p.listen * 0.5 + 0.3 * width) * pi;
            const double tilt = 0.7 * hit - 0.35;               // crown (dark) .. lip (bright)
            for (int i = 0; n + 1 < want && i < (int) list.size(); ++i)
            {
                auto bp = list[(size_t) i];
                if (p.type == bell && i == 2) bp.ratio = 1.2 + 0.05 * shape;   // minor .. major third
                const double amp = bp.amp * std::pow (bp.ratio, tilt);
                for (int twin = 0; twin < 2; ++twin)
                {
                    auto f = [&] (double a) { return twin == 0 ? std::cos (bp.m * a) : std::sin (bp.m * a); };
                    const double ratio = bp.ratio * (1.0 + (twin == 0 ? -0.5 : 0.5) * split);
                    md[(size_t) n++] = { ratio, f (alpha) * 1.41421356, f (thL) * 1.41421356, f (thR) * 1.41421356, amp, bp.decay };
                }
            }
            break;
        }

        default: break;
    }

    // shared transforms: inharmonic warp, material (frequency dependent damping), brightness
    const double warp = 1.0 + 0.3 * std::clamp ((double) p.inharm, -1.0, 1.0);
    const double alphaDecay = 0.05 + 1.4 * std::pow (1.0 - std::clamp ((double) p.material, 0.0, 1.0), 1.5);
    const double tiltAmp = 0.75 * std::clamp ((double) p.bright, -1.0, 1.0) - 0.25;

    for (int i = 0; i < n; ++i)
    {
        auto& m = md[(size_t) i];
        m.ratio = std::pow (m.ratio, warp);
        m.dec *= std::pow (m.ratio, -alphaDecay);
        m.amp *= std::pow (m.ratio, tiltAmp);
    }
    std::sort (md.begin(), md.begin() + n, [] (auto& a, auto& b) { return a.ratio < b.ratio; });

    // level normalisation over the lower modes (independent of hit/listen, so positions still matter)
    double s = 0.0;
    for (int i = 0; i < std::min (n, 24); ++i)
        s += md[(size_t) i].amp * md[(size_t) i].amp;
    // per-type trim so every body sits at a similar loudness (measured, see test suite)
    constexpr double trim[numTypes] { 1.4, 1.18, 1.1, 0.31, 0.59, 1.07, 0.71, 0.82 };
    const double norm = trim[std::clamp (p.type, 0, numTypes - 1)] / std::sqrt (std::max (s, 1e-6));

    out.count = n;
    for (int i = 0; i < maxModes; ++i)
    {
        if (i < n)
        {
            const auto& m = md[(size_t) i];
            out.ratio[(size_t) i] = (float) m.ratio;
            out.exc[(size_t) i]   = (float) (m.e * m.amp);
            out.outL[(size_t) i]  = (float) (m.l * norm);
            out.outR[(size_t) i]  = (float) (m.r * norm);
            out.decay[(size_t) i] = (float) m.dec;
        }
        else
        {
            out.ratio[(size_t) i] = out.exc[(size_t) i] = out.outL[(size_t) i] = out.outR[(size_t) i] = out.decay[(size_t) i] = 0.0f;
        }
    }
}
} // namespace ictus
