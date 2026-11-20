#pragma once

// 16-step pattern per element with per-track length (polymeter) and swing.
// Step positions are derived from the song position, so the pattern is always in
// the same place relative to the bar no matter where playback starts or loops.

#include <atomic>
#include <array>
#include <cmath>
#include <string>
#include "Specs.h"

namespace rud
{
constexpr int numSteps = 16;

struct Pattern
{
    std::array<std::atomic<uint8_t>, numElements * numSteps> steps;   // 0 off, 1 on, 2 accent
    std::array<std::atomic<int>, numElements> length;

    Pattern() { clear(); }

    void clear()
    {
        for (auto& s : steps) s = 0;
        for (auto& l : length) l = numSteps;
    }

    uint8_t get (int e, int s) const { return steps[(size_t) (e * numSteps + s)].load (std::memory_order_relaxed); }
    void set (int e, int s, uint8_t v) { steps[(size_t) (e * numSteps + s)].store (v, std::memory_order_relaxed); }
    int len (int e) const { return std::clamp (length[(size_t) e].load (std::memory_order_relaxed), 1, numSteps); }

    // '.' off, 'x' on, 'X' accent
    std::string rowString (int e) const
    {
        std::string r;
        for (int s = 0; s < numSteps; ++s) r += get (e, s) == 2 ? 'X' : (get (e, s) == 1 ? 'x' : '.');
        return r;
    }
    void setRow (int e, const std::string& r)
    {
        for (int s = 0; s < numSteps; ++s)
        {
            const char c = s < (int) r.size() ? r[(size_t) s] : '.';
            set (e, s, c == 'X' ? 2 : (c == 'x' ? 1 : 0));
        }
    }
};

struct StepClock
{
    static constexpr double stepLen = 0.25;   // 1/16 note in quarter notes

    static double stepTime (long k, double swing)   // swing 0.5 straight .. 0.75 hard
    {
        return (double) k * stepLen + ((k & 1) != 0 ? (swing - 0.5) * 2.0 * stepLen : 0.0);
    }

    template <class F>
    static void forEach (double ppqStart, double ppqEnd, double swing, F&& f)
    {
        const long k0 = (long) std::floor (ppqStart / stepLen) - 2;
        const long k1 = (long) std::ceil (ppqEnd / stepLen) + 1;
        for (long k = k0; k <= k1; ++k)
        {
            const double t = stepTime (k, swing);
            if (t >= ppqStart && t < ppqEnd)
                f (k, t);
        }
    }

    static int wrap (long k, int len)
    {
        long m = k % len;
        if (m < 0) m += len;
        return (int) m;
    }
};
} // namespace rud
