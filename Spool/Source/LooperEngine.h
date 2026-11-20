#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

/*
    Spool looper core.

    Memory: one preallocated pool per channel (3 minutes of audio). The first
    recording goes to the start of the pool; once the loop length L is known,
    the pool is cut into slots of (L + tail) frames. Every layer lives in its own
    slot, so undo / redo is just switching slots. Starting an overdub copies the
    current slot into a fresh one; the copy runs incrementally ahead of the write
    head (reads from not-yet-copied regions fall back to the source slot), so
    long loops never cause a CPU spike.

    Two heads:
      b  - base head, always 1x; overdubs are written here.
      r  - read head; locked to b unless a time modifier (speed / trim /
           scramble / reverse) is active. Every jump is crossfaded.

    Normal mode: modifiers and stability colour only what you hear.
    Additive mode: overdubbing writes what you hear back into the loop, so
    effects accumulate pass after pass.
    Sampler mode: every recording replaces the loop; trigger restarts it.
*/
class LooperEngine
{
public:
    enum class State   : int { empty = 0, recording, playing, overdubbing, stopped };
    enum class Mode    : int { normal = 0, additive, sampler };
    enum class Command : int { recPlay = 0, stop, oneShot, undo, redo, clear, trigger };

    enum { modSmoothSpeed = 0, modDropper, modTrimmer };     // slot A
    enum { modSteppedSpeed = 0, modScrambler, modFilter };   // slot B

    struct Params
    {
        Mode  mode = Mode::normal;
        float volume = 0.5f, repeats = 1.0f, modA = 0.5f, stability = 0.0f, modB = 0.5f;
        int   modASel = 0, modBSel = 0;
        bool  modAOn = false, modBOn = false, reverse = false;
        bool  dryKill = false, dubAfterRec = false, sampleLoop = true;
    };

    static constexpr int maxLayers = 9; // current + 8 undo steps

    //==============================================================================
    void prepare (double sampleRate)
    {
        const juce::SpinLock::ScopedLockType sl (guiLock);

        fs = sampleRate;
        poolFrames = (int) std::lround (fs * 180.0);
        for (auto& p : pool)
            p.assign ((size_t) poolFrames, 0.0f);

        tail     = juce::jmax (16, (int) std::lround (fs * 0.010));
        fadeLen  = juce::jmax (16, (int) std::lround (fs * 0.008));
        rampLen  = juce::jmax (16, (int) std::lround (fs * 0.005));
        maxLen   = juce::jmin ((int) std::lround (fs * 60.0), poolFrames / 2 - tail);
        minLen   = (int) std::lround (fs * 0.05);

        for (auto& rb : releaseBuf)
            rb.assign ((size_t) fadeLen, 0.0f);
        releaseRemaining = 0;

        kSpeed = onePole (0.06);
        kSmooth = onePole (0.02);
        kDrift = onePole (0.4);
        kDip = onePole (0.012);

        resetAll();
        updateDerived();
    }

    void setParams (const Params& p)
    {
        params = p;
        updateDerived();
    }

    void setForcedLength (int samples) noexcept { forcedLen = samples; }
    int  getRecordedLength() const noexcept     { return recLen; }
    int  getLoopLength() const noexcept         { return L; }
    State getState() const noexcept             { return state; }
    double getSampleRate() const noexcept       { return fs; }

    void command (Command c)
    {
        const bool sampler = params.mode == Mode::sampler;

        switch (c)
        {
            case Command::recPlay:
                if (sampler)
                {
                    if (state == State::recording) closeRecording();
                    else                           startRecording();
                    break;
                }
                switch (state)
                {
                    case State::empty:       startRecording(); break;
                    case State::recording:   closeRecording(); break;
                    case State::playing:     startOverdub (0); break;
                    case State::overdubbing: endOverdub(); break;
                    case State::stopped:     restart(); break;
                }
                break;

            case Command::stop:
                if (state == State::recording)
                {
                    closeRecording();
                    if (sealing) bakeSealSilent();
                }
                if (state == State::overdubbing) endOverdub();
                if (state == State::playing)     { state = State::stopped; playTarget = 0.0f; }
                pendingDub = false;
                break;

            case Command::oneShot:
                if (sampler) break;
                if (state == State::stopped) restart();
                if (state == State::playing) startOverdub (L);
                break;

            case Command::undo:
                if (state == State::recording) { clearNow(); break; }
                if (state == State::overdubbing) { endOverdub(); dubAmt = 0.0f; }
                if (L > 0 && cur > 0) switchLayer (cur - 1);
                break;

            case Command::redo:
                if (state == State::recording) break;
                if (state == State::overdubbing) { endOverdub(); dubAmt = 0.0f; }
                if (L > 0 && cur < count - 1) switchLayer (cur + 1);
                break;

            case Command::clear:
                if (state != State::recording && state != State::empty && playAmt > 0.001f)
                {
                    pendingClear = true;
                    state = State::stopped;
                    playTarget = 0.0f;
                    dubTarget = 0.0f;
                }
                else
                    clearNow();
                break;

            case Command::trigger:
                if (L > 0 && state != State::recording)
                    restart();
                break;
        }

        publish();
    }

    /** In-place: io = dry (unless dry kill) + loop. Mono or stereo. */
    void process (float* const* io, int nch, int start, int len) noexcept
    {
        if (len <= 0)
            return;

        advanceCopy (juce::jmax (65536, len * 4));

        for (int i = start; i < start + len; ++i)
        {
            const float in[2] { io[0][i], nch > 1 ? io[1][i] : io[0][i] };
            float loop[2] { 0.0f, 0.0f };

            volume += (volumeTarget - volume) * kSmooth;

            if (state == State::recording)
            {
                pool[0][(size_t) recLen] = in[0];
                pool[1][(size_t) recLen] = in[1];
                if (++recLen >= maxLen)
                    closeRecording();
            }
            else if (L > 0)
            {
                playAmt = approach (playAmt, playTarget);
                dubAmt  = approach (dubAmt, dubTarget);

                if (state == State::stopped && playAmt <= 0.0f && dubAmt <= 0.0f)
                {
                    if (pendingClear)
                        clearNow();
                }
                else
                {
                    float heard[2];
                    tick (in, heard);
                    loop[0] = heard[0] * playAmt * volume;
                    loop[1] = heard[1] * playAmt * volume;
                }
            }

            if (releaseRemaining > 0)
            {
                const auto k = (size_t) (fadeLen - releaseRemaining);
                loop[0] += releaseBuf[0][k];
                loop[1] += releaseBuf[1][k];
                --releaseRemaining;
            }

            const float dry = params.dryKill ? 0.0f : 1.0f;
            if (nch > 1)
            {
                io[0][i] = dry * in[0] + loop[0];
                io[1][i] = dry * in[1] + loop[1];
            }
            else
                io[0][i] = dry * in[0] + 0.5f * (loop[0] + loop[1]);
        }

        publish();
    }

    //==============================================================================
    // GUI side (message thread)
    struct Info
    {
        State state = State::empty;
        int len = 0, pos = 0, cur = 0, count = 0, slots = 0;
        double readPos = 0.0, regionStart = 0.0, regionEnd = 0.0;
        bool locked = true;
    };

    Info getInfo() const noexcept
    {
        Info in;
        in.state = (State) aState.load();
        in.len = aLen.load();
        in.pos = aPos.load();
        in.cur = aCur.load();
        in.count = aCount.load();
        in.slots = aSlots.load();
        in.readPos = aRead.load();
        in.regionStart = aRegionStart.load();
        in.regionEnd = aRegionEnd.load();
        in.locked = aLocked.load();
        return in;
    }

    /** Peak overview of the current layer (or the recording in progress). */
    void getOverview (std::vector<float>& peaks, int bins) const
    {
        const juce::SpinLock::ScopedLockType sl (guiLock);
        peaks.assign ((size_t) bins, 0.0f);

        const int len = aLen.load(), base = aBase.load();
        if (len <= 0 || pool[0].empty() || base + len > poolFrames)
            return;

        for (int bIdx = 0; bIdx < bins; ++bIdx)
        {
            const int s0 = (int) ((int64_t) bIdx * len / bins);
            const int s1 = juce::jmax (s0 + 1, (int) ((int64_t) (bIdx + 1) * len / bins));
            const int step = juce::jmax (1, (s1 - s0) / 48);
            float m = 0.0f;
            for (int s = s0; s < s1; s += step)
                m = juce::jmax (m, std::abs (pool[0][(size_t) (base + s)]), std::abs (pool[1][(size_t) (base + s)]));
            peaks[(size_t) bIdx] = m;
        }
    }

    /** Copies the current layer for export; returns false if there is no loop. */
    bool copyCurrentLayer (juce::AudioBuffer<float>& dst) const
    {
        const juce::SpinLock::ScopedLockType sl (guiLock);
        const auto st = (State) aState.load();
        const int len = aLen.load(), base = aBase.load();
        if (st == State::empty || st == State::recording || len <= 0 || base + len > poolFrames)
            return false;

        dst.setSize (2, len);
        for (int c = 0; c < 2; ++c)
            dst.copyFrom (c, 0, pool[(size_t) c].data() + base, len);
        return true;
    }

private:
    //==============================================================================
    double onePole (double seconds) const { return 1.0 - std::exp (-1.0 / (seconds * fs)); }

    float approach (float v, float target) const noexcept
    {
        const float step = 1.0f / (float) rampLen;
        return v < target ? juce::jmin (target, v + step) : juce::jmax (target, v - step);
    }

    static uint32_t hash (uint32_t x) noexcept
    {
        x ^= x >> 16; x *= 0x7feb352du;
        x ^= x >> 15; x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }
    static float h01 (uint32_t x) noexcept { return (float) (hash (x) >> 8) * (1.0f / 16777216.0f); }

    int slotOf (int layer) const noexcept { return (first + layer) % numSlots; }

    double wrapLoop (double p) const noexcept
    {
        if (p >= L || p < 0.0)
        {
            p = std::fmod (p, (double) L);
            if (p < 0.0) p += L;
        }
        return p;
    }

    bool isCopied (int idx) const noexcept
    {
        int d = idx - copyStart;
        if (d < 0) d += L;
        return d < copied;
    }

    float rd (int c, int slot, int idx) const noexcept
    {
        if (copying && slot == copyDst && ! isCopied (idx))
            slot = copySrc;

        const float* w = pool[(size_t) c].data() + (size_t) slot * (size_t) stride;

        if (sealing && slot == 0 && idx < tail)
        {
            const float fi = sealFade (idx);
            return w[idx] * fi + w[L + idx] * (1.0f - fi);
        }
        return w[idx];
    }

    float sealFade (int idx) const noexcept
    {
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) idx / (float) tail);
    }

    float readInterp (int c, int slot, double pos) const noexcept
    {
        const int i = (int) pos;
        const float t = (float) (pos - i);
        auto at = [&] (int k) { k %= L; if (k < 0) k += L; return rd (c, slot, k); };
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    //==============================================================================
    void resetAll()
    {
        state = State::empty;
        L = recLen = b = 0;
        r = lastSrc = 0.0;
        stride = poolFrames;
        numSlots = 1; first = 0; count = 1; cur = 0;
        copying = sealing = pendingDub = pendingClear = false;
        pendingOneShot = oneShotRemaining = 0;
        playAmt = playTarget = dubAmt = dubTarget = 0.0f;
        fadeRemaining = 0; // releaseRemaining is left alone: a pending fade-out must survive a reset
        locked = true;
        curSpeed = 1.0;
        scrSlice = -1; scrOffset = 0.0;
        dropGain = dropTarget = 1.0f; dropSeg = -1; dropCounter = 0;
        for (auto& f : svf) f = {};
        for (auto& l : stabLp) l = 0.0f;
        dipGain = dipTarget = 1.0f; dipTimer = 0;
        forcedLen = 0;
        publish();
    }

    void updateDerived()
    {
        auto centred = [] (float v)
        {
            const float k = (v - 0.5f) * 2.0f;
            if (std::abs (k) < 0.04f) return 0.0f;
            return (k - std::copysign (0.04f, k)) / 0.96f;
        };

        const float kA = params.modAOn ? centred (params.modA) : 0.0f;
        const float kB = params.modBOn ? centred (params.modB) : 0.0f;

        double speed = 1.0;
        if (params.modASel == modSmoothSpeed)  speed *= 1.0 + kA;           // 0x .. 2x
        if (params.modBSel == modSteppedSpeed) speed *= steppedSpeed (kB);  // octaves & fifths
        if (params.reverse) speed = -speed;
        targetSpeed = speed;

        trimK = params.modASel == modTrimmer   ? kA : 0.0f;
        dropK = params.modASel == modDropper   ? kA : 0.0f;
        scrK  = params.modBSel == modScrambler ? kB : 0.0f;
        filtK = params.modBSel == modFilter    ? kB : 0.0f;

        timeMods = std::abs (targetSpeed - 1.0) > 1.0e-6 || trimK != 0.0f || scrK != 0.0f;

        // trimmer region
        rs = 0.0; re = (double) L;
        if (L > 0 && trimK != 0.0f)
        {
            const double cut = 0.95 * std::abs (trimK) * L;
            if (trimK < 0.0f) re = L - cut;
            else              rs = cut;
            if (re - rs < fs * 0.02) re = juce::jmin ((double) L, rs + fs * 0.02);
        }

        // scrambler
        const float sa = std::abs (scrK);
        scrSlices = sa < 0.4f ? 8 : (sa < 0.75f ? 16 : 32);
        scrAmount = sa;
        scrPattern = scrK > 0.0f;

        // dropper
        const float da = std::abs (dropK);
        dropAmount = da;
        dropPattern = dropK > 0.0f;
        dropSegLen = fs * (0.16 - 0.145 * std::pow ((double) da, 0.7));

        // filter
        if (filtK > 0.0f)      filterG = std::tan (juce::MathConstants<double>::pi * juce::jmin (20.0 * std::pow (150.0, (double) filtK), 0.45 * fs) / fs);
        else if (filtK < 0.0f) filterG = std::tan (juce::MathConstants<double>::pi * juce::jmin (20000.0 * std::pow (0.0125, (double) -filtK), 0.45 * fs) / fs);
        filterHp = filtK > 0.0f;

        // stability
        const double s = params.stability;
        stab = s;
        wowDepth  = std::pow (s, 1.5) * 0.004 * fs;
        flutDepth = s * s * 0.0002 * fs;
        stabLpCoef = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 18000.0 * std::pow (0.18, s) / fs));
        stabDrive = (float) (1.0 + 3.0 * s * s);
        hissAmp = (float) (0.002 * s * s);
        dipProb = (float) juce::jmax (0.0, (s - 0.5) * 0.35);

        // output
        const float v = params.volume;
        volumeTarget = v <= 0.5f ? (2.0f * v) * (2.0f * v) : 1.0f + 2.0f * (v - 0.5f);
        feedback = params.repeats;
    }

    static double steppedSpeed (float k) noexcept
    {
        static constexpr double up[]   { 1.0, 1.5, 2.0, 3.0, 4.0 };
        static constexpr double down[] { 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25 };
        const int idx = juce::jlimit (0, 4, (int) std::lround (std::abs (k) * 4.0f));
        return k >= 0.0f ? up[idx] : down[idx];
    }

    //==============================================================================
    void startRecording()
    {
        renderRelease();
        resetAll();
        state = State::recording;
        recLen = 0;
    }

    void closeRecording()
    {
        if (recLen < minLen)
        {
            clearNow();
            return;
        }

        if (forcedLen > 0 && std::abs (forcedLen - recLen) < (int) (fs * 0.08) && forcedLen < maxLen)
        {
            for (int s = recLen; s < forcedLen; ++s)
                pool[0][(size_t) s] = pool[1][(size_t) s] = 0.0f;
            recLen = forcedLen;
        }
        forcedLen = 0;

        L = recLen;
        stride = L + tail;
        numSlots = juce::jlimit (2, maxLayers, poolFrames / stride);
        first = 0; count = 1; cur = 0;

        b = 0; r = 0.0; lastSrc = 0.0;
        locked = true; curSpeed = 1.0;
        scrSlice = -1; scrOffset = 0.0;
        scrSeed = (uint32_t) juce::Random::getSystemRandom().nextInt();

        sealing = true;
        state = State::playing;
        playAmt = 0.0f;
        playTarget = 1.0f;

        if (params.dubAfterRec && params.mode != Mode::sampler)
        {
            pendingDub = true;
            pendingOneShot = 0;
        }

        updateDerived();
    }

    /** Crossfade the loop seam with the audio recorded just after closing. */
    void bakeSeal() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            float* w = pool[(size_t) c].data();
            for (int k = 0; k < tail; ++k)
            {
                const float fi = sealFade (k);
                w[k] = w[k] * fi + w[L + k] * (1.0f - fi);
            }
        }
        sealing = false;

        if (pendingDub)
        {
            pendingDub = false;
            startOverdub (pendingOneShot);
        }
    }

    /** No audio after the end point (stopped immediately): short fades instead. */
    void bakeSealSilent() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            float* w = pool[(size_t) c].data();
            for (int k = 0; k < tail; ++k)
            {
                const float fi = sealFade (k);
                w[k] *= fi;
                w[L - tail + k] *= 1.0f - fi;
            }
        }
        sealing = false;
        pendingDub = false;
    }

    void startOverdub (int passLength)
    {
        if (sealing)
        {
            pendingDub = true;
            pendingOneShot = passLength;
            return;
        }

        finishCopy();

        const int src = slotOf (cur);
        count = cur + 1;                       // overdubbing discards the redo history
        if (count == numSlots)
        {
            first = (first + 1) % numSlots;    // forget the oldest layer
            --count;
            --cur;
        }
        cur = count;
        ++count;

        copySrc = src;
        copyDst = slotOf (cur);
        copyStart = b;
        copied = 0;
        copying = true;
        advanceCopy (juce::jmax (4096, rampLen * 4));

        state = State::overdubbing;
        dubTarget = 1.0f;
        oneShotRemaining = passLength;
    }

    void endOverdub()
    {
        if (state == State::overdubbing)
            state = State::playing;
        dubTarget = 0.0f;
        oneShotRemaining = 0;
    }

    void switchLayer (int layer)
    {
        startFade (slotOf (cur));
        cur = layer;
    }

    void restart()
    {
        if (playAmt > 0.0f)
            startFade (slotOf (cur));
        b = 0;
        r = locked ? 0.0 : rs;
        scrSlice = -1;
        state = State::playing;
        playTarget = 1.0f;
    }

    void clearNow()
    {
        renderRelease();
        resetAll();
    }

    void advanceCopy (int frames) noexcept
    {
        if (! copying)
            return;

        int todo = juce::jmin (frames, L - copied);
        while (todo > 0)
        {
            const int pos = (copyStart + copied) % L;
            const int n = juce::jmin (todo, L - pos);
            for (int c = 0; c < 2; ++c)
            {
                float* base = pool[(size_t) c].data();
                std::memcpy (base + (size_t) copyDst * (size_t) stride + pos,
                             base + (size_t) copySrc * (size_t) stride + pos, sizeof (float) * (size_t) n);
            }
            copied += n;
            todo -= n;
        }

        if (copied >= L)
            copying = false;
    }

    void finishCopy() noexcept { advanceCopy (L); }

    void startFade (int slot) noexcept
    {
        fadeSlot = slot;
        fadePos = wrapLoop (lastSrc + curSpeed);
        fadeRemaining = fadeLen;
    }

    /** Pre-renders a short fade-out of what is currently audible, so clearing or
        re-recording over a playing loop never clicks. */
    void renderRelease() noexcept
    {
        releaseRemaining = 0;
        if (L <= 0 || state == State::recording || playAmt <= 0.0f)
            return;

        const int slot = slotOf (cur);
        for (int k = 0; k < fadeLen; ++k)
        {
            const float g = (1.0f - (float) k / (float) fadeLen) * playAmt * volume;
            const double p = wrapLoop (lastSrc + k * curSpeed);
            for (int c = 0; c < 2; ++c)
                releaseBuf[(size_t) c][(size_t) k] = readInterp (c, slot, p) * g;
        }
        releaseRemaining = fadeLen;
    }

    //==============================================================================
    void tick (const float* in, float* heard) noexcept
    {
        const int slot = slotOf (cur);

        // ---- read head ------------------------------------------------------------------
        if (locked && timeMods)
        {
            locked = false;
            r = b;
        }
        else if (! locked && ! timeMods)
        {
            startFade (slot);
            locked = true;
            curSpeed = 1.0;
            scrOffset = 0.0;
            scrSlice = -1;
        }

        if (locked)
        {
            r = b;
        }
        else
        {
            curSpeed += (targetSpeed - curSpeed) * kSpeed;
            r += curSpeed;

            if (r >= re || r < rs)
            {
                const double rl = re - rs;
                if (rs > 0.0 || re < L)
                    startFade (slot);
                r = rs + std::fmod (r - rs, rl);
                if (r < rs) r += rl;
            }
        }

        // ---- scrambler -----------------------------------------------------------------------
        if (! locked && scrK != 0.0f)
        {
            const double sliceLen = (re - rs) / scrSlices;
            const int idx = juce::jlimit (0, scrSlices - 1, (int) ((r - rs) / sliceLen));
            if (idx != scrSlice)
            {
                scrSlice = idx;
                const float u = scrPattern ? h01 ((uint32_t) idx * 2u + scrSeed)      : rng.nextFloat();
                const float v = scrPattern ? h01 ((uint32_t) idx * 2u + 1u + scrSeed) : rng.nextFloat();
                const int target = u < scrAmount ? juce::jlimit (0, scrSlices - 1, (int) (v * scrSlices)) : idx;
                const double off = (target - idx) * sliceLen;
                if (off != scrOffset)
                {
                    startFade (slot);
                    scrOffset = off;
                }
            }
        }
        else if (scrOffset != 0.0)
        {
            startFade (slot);
            scrOffset = 0.0;
            scrSlice = -1;
        }

        double src = r + scrOffset;
        if (! locked && (src >= re || src < rs))
        {
            const double rl = re - rs;
            src = rs + std::fmod (src - rs, rl);
            if (src < rs) src += rl;
        }

        // ---- stability: wow & flutter as a (positive) delay of the read point -----------
        if (stab > 0.005)
        {
            wowPhase  += 2.0 * juce::MathConstants<double>::pi * 0.55 / fs;
            flutPhase += 2.0 * juce::MathConstants<double>::pi * 8.7 / fs;
            if (wowPhase > 6.283185307179586)  wowPhase  -= 6.283185307179586;
            if (flutPhase > 6.283185307179586) flutPhase -= 6.283185307179586;

            if (--driftTimer <= 0)
            {
                driftTarget = rng.nextFloat();
                driftTimer = (int) (fs * (0.3 + 0.5 * rng.nextFloat()));
            }
            drift += (driftTarget - drift) * (float) kDrift;

            const double wow = wowDepth * (0.5 + 0.5 * std::sin (wowPhase)) * (0.55 + 0.45 * drift);
            const double flut = flutDepth * (0.5 + 0.5 * std::sin (flutPhase));
            src -= wow + flut;
        }

        src = wrapLoop (src);
        lastSrc = src;

        float v[2];
        for (int c = 0; c < 2; ++c)
            v[c] = readInterp (c, slot, src);

        if (fadeRemaining > 0)
        {
            const float g = (float) fadeRemaining / (float) fadeLen;
            for (int c = 0; c < 2; ++c)
                v[c] = v[c] * (1.0f - g) + readInterp (c, fadeSlot, fadePos) * g;
            fadePos = wrapLoop (fadePos + curSpeed);
            --fadeRemaining;
        }

        // near-zero tape speed fades out instead of freezing on one sample
        if (! locked && std::abs (curSpeed) < 0.1)
        {
            const float g = (float) (std::abs (curSpeed) / 0.1);
            v[0] *= g; v[1] *= g;
        }

        // ---- dropper -------------------------------------------------------------------------
        if (dropK != 0.0f)
        {
            const int64_t seg = dropPattern ? (int64_t) (src / dropSegLen)
                                            : (int64_t) (dropCounter++ / (int64_t) juce::jmax (1.0, dropSegLen));
            if (seg != dropSeg)
            {
                dropSeg = seg;
                const uint32_t key = (uint32_t) seg * 3u + scrSeed;
                const float u  = dropPattern ? h01 (key)      : rng.nextFloat();
                const float dv = dropPattern ? h01 (key + 1u) : rng.nextFloat();
                const float fv = dropPattern ? h01 (key + 2u) : rng.nextFloat();
                const bool drop = u < dropAmount * 0.85f;
                dropTarget = drop ? 1.0f - (0.45f + 0.55f * dv) : 1.0f;
                dropCoef = (float) (1.0 - std::exp (-1.0 / ((0.001 + 0.04 * fv * fv) * fs)));
            }
        }
        else
        {
            dropTarget = 1.0f;
            dropCoef = (float) kSmooth;
        }
        dropGain += (dropTarget - dropGain) * dropCoef;
        v[0] *= dropGain; v[1] *= dropGain;

        // ---- filter (non-resonant SVF; LP left of noon, HP right of noon) ----------------------
        if (filtK != 0.0f)
        {
            curG += (filterG - curG) * kSmooth;
            constexpr double k = 1.4142135623730951;
            const double a1 = 1.0 / (1.0 + curG * (curG + k));
            const double a2 = curG * a1, a3 = curG * a2;
            for (int c = 0; c < 2; ++c)
            {
                auto& f = svf[(size_t) c];
                const double x = v[c];
                const double v3 = x - f.s2;
                const double v1 = a1 * f.s1 + a2 * v3;
                const double v2 = f.s2 + a2 * f.s1 + a3 * v3;
                f.s1 = 2.0 * v1 - f.s1;
                f.s2 = 2.0 * v2 - f.s2;
                v[c] = (float) (filterHp ? x - k * v1 - v2 : v2);
            }
        }
        else
        {
            curG = filterG;
            svf[0] = {}; svf[1] = {};
        }

        // ---- stability: tone, saturation, hiss, tape dropouts --------------------------------
        if (stab > 0.005)
        {
            if (--dipTimer <= 0)
            {
                dipTimer = (int) (fs * 0.05);
                if (dipTarget < 1.0f)            dipTarget = 1.0f;
                else if (rng.nextFloat() < dipProb) dipTarget = juce::Decibels::decibelsToGain (-(3.0f + 7.0f * rng.nextFloat()));
            }
            dipGain += (dipTarget - dipGain) * (float) kDip;

            for (int c = 0; c < 2; ++c)
            {
                stabLp[(size_t) c] += (v[c] - stabLp[(size_t) c]) * stabLpCoef;
                float y = std::tanh (stabDrive * stabLp[(size_t) c]) / stabDrive;
                y += hissAmp * (rng.nextFloat() * 2.0f - 1.0f);
                v[c] = y * dipGain;
            }
        }

        heard[0] = v[0];
        heard[1] = v[1];

        // ---- write (overdub) -----------------------------------------------------------------
        if (dubAmt > 0.0f)
        {
            const bool additive = params.mode == Mode::additive;
            for (int c = 0; c < 2; ++c)
            {
                float* w = pool[(size_t) c].data() + (size_t) slot * (size_t) stride;
                const float old = rd (c, slot, b);
                const float basis = additive ? heard[c] : old;
                w[b] = old + dubAmt * (feedback * basis + in[c] - old);
            }
        }

        if (sealing)
        {
            pool[0][(size_t) (L + b)] = in[0];
            pool[1][(size_t) (L + b)] = in[1];
        }

        // ---- advance ---------------------------------------------------------------------------
        ++b;
        if (sealing && b >= tail)
            bakeSeal();
        if (b >= L)
            b = 0;

        if (oneShotRemaining > 0 && --oneShotRemaining == 0)
            endOverdub();

        if (params.mode == Mode::sampler && ! params.sampleLoop && state == State::playing
            && L > rampLen && b == L - rampLen)
        {
            state = State::stopped;
            playTarget = 0.0f;
        }
    }

    void publish() noexcept
    {
        aState = (int) state;
        aLen = state == State::recording ? recLen : L;
        aPos = state == State::recording ? recLen : b;
        aCur = cur;
        aCount = state == State::empty || state == State::recording ? 0 : count;
        aSlots = numSlots;
        aBase = state == State::recording || L == 0 ? 0 : slotOf (cur) * stride;
        aRead = lastSrc;
        aRegionStart = rs;
        aRegionEnd = re;
        aLocked = locked;
    }

    //==============================================================================
    double fs = 48000.0;
    std::array<std::vector<float>, 2> pool;
    int poolFrames = 0, tail = 480, fadeLen = 384, rampLen = 240, maxLen = 0, minLen = 0;

    Params params;
    State state = State::empty;

    int L = 0, recLen = 0, b = 0, stride = 0, forcedLen = 0;
    int numSlots = 1, first = 0, count = 1, cur = 0;

    bool copying = false;
    int copySrc = 0, copyDst = 0, copyStart = 0, copied = 0;

    bool sealing = false, pendingDub = false, pendingClear = false;
    int pendingOneShot = 0, oneShotRemaining = 0;

    float playAmt = 0, playTarget = 0, dubAmt = 0, dubTarget = 0;
    float volume = 1.0f, volumeTarget = 1.0f, feedback = 1.0f;

    // read head
    bool locked = true, timeMods = false;
    double r = 0.0, lastSrc = 0.0, curSpeed = 1.0, targetSpeed = 1.0;
    double rs = 0.0, re = 0.0;
    int fadeSlot = 0, fadeRemaining = 0;
    double fadePos = 0.0;

    // modifiers
    float trimK = 0, dropK = 0, scrK = 0, filtK = 0;
    int scrSlices = 8, scrSlice = -1;
    float scrAmount = 0.0f;
    bool scrPattern = true;
    double scrOffset = 0.0;
    uint32_t scrSeed = 1;

    float dropAmount = 0.0f, dropGain = 1.0f, dropTarget = 1.0f, dropCoef = 0.01f;
    bool dropPattern = true;
    double dropSegLen = 4800.0;
    int64_t dropSeg = -1, dropCounter = 0;

    struct SvfState { double s1 = 0.0, s2 = 0.0; };
    std::array<SvfState, 2> svf;
    double filterG = 1.0, curG = 1.0;
    bool filterHp = false;

    // stability
    double stab = 0.0, wowDepth = 0.0, flutDepth = 0.0, wowPhase = 0.0, flutPhase = 0.0;
    float drift = 0.5f, driftTarget = 0.5f;
    int driftTimer = 0;
    std::array<float, 2> stabLp {};
    float stabLpCoef = 1.0f, stabDrive = 1.0f, hissAmp = 0.0f, dipProb = 0.0f;
    float dipGain = 1.0f, dipTarget = 1.0f;
    int dipTimer = 0;

    double kSpeed = 0.001, kSmooth = 0.001, kDrift = 0.001, kDip = 0.001;

    std::array<std::vector<float>, 2> releaseBuf;
    int releaseRemaining = 0;

    juce::Random rng;

    // published for the GUI
    std::atomic<int> aState { 0 }, aLen { 0 }, aPos { 0 }, aCur { 0 }, aCount { 0 }, aSlots { 1 }, aBase { 0 };
    std::atomic<double> aRead { 0.0 }, aRegionStart { 0.0 }, aRegionEnd { 0.0 };
    std::atomic<bool> aLocked { true };
    mutable juce::SpinLock guiLock;
};
