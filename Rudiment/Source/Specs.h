#pragma once

// Parameter specification shared by the engine, the processor and the editor.
// The order of each element's params is the index order the voice code reads.

#include <array>
#include <vector>

namespace rud
{
enum Element { kick = 0, snare, clap, tom, closedHat, openHat, perc, cymbal, numElements };
enum class Unit { hz, sec, semis, pct, ratio, count };

struct ParamSpec
{
    const char* id;      // suffix, full id = prefix + id
    const char* label;
    float min, max, def, centre;   // centre > 0: skewed range
    Unit unit;
    const char* tip;
};

struct ElementSpec
{
    const char* name;
    const char* prefix;
    std::vector<ParamSpec> params;
};

// Kick
enum { kTune, kPitch, kSweep, kHold, kDecay, kClick, kClickTone, kDrive, kTone };
// Snare
enum { sTune, sPitch, sBodyDecay, sRing, sSnap, sSnapDecay, sSnapTone, sDrive };
// Clap
enum { cTone, cRes, cSpread, cBursts, cDecay, cWidth, cDrive };
// Tom
enum { tTune, tPitch, tSweep, tDecay, tRing, tStick, tDrive };
// Hats (closed + open)
enum { hTune, hColor, hTone, hRes, hDecay };
// Perc
enum { pTune, pRatio, pWave, pDecay, pTone, pClick };
// Cymbal
enum { yTune, yColor, yTone, yDecay, yWidth, yBloom };

inline const std::array<ElementSpec, numElements>& elementSpecs()
{
    static const std::array<ElementSpec, numElements> specs { {
        { "KICK", "k", {
            { "Tune",      "TUNE",       30.0f, 150.0f, 50.0f, 60.0f,  Unit::hz,    "Pitch the body settles to." },
            { "Pitch",     "PITCH",      0.0f,  60.0f,  28.0f, 0.0f,   Unit::semis, "How far above the body the sweep starts. More = more punch and knock." },
            { "Sweep",     "SWEEP",      0.002f, 0.3f,  0.045f, 0.04f, Unit::sec,   "Speed of the pitch drop. Short = tight click, long = laser / boing." },
            { "Hold",      "HOLD",       0.0f,  0.3f,   0.03f, 0.06f,  Unit::sec,   "Time at full level before the decay starts. Adds weight and punch." },
            { "Decay",     "DECAY",      0.05f, 4.0f,   0.7f,  0.6f,   Unit::sec,   "Length of the body (to -60 dB). Long = sub / boom." },
            { "Click",     "CLICK",      0.0f,  1.0f,   0.35f, 0.0f,   Unit::pct,   "Level of the beater transient." },
            { "ClickTone", "CLICK TONE", 1000.0f, 14000.0f, 4000.0f, 4000.0f, Unit::hz, "Colour of the transient: dull felt to hard plastic." },
            { "Drive",     "DRIVE",      0.0f,  1.0f,   0.25f, 0.0f,   Unit::pct,   "Saturation. Rounds the sine towards a square, adds harmonics that cut through on small speakers." },
            { "Tone",      "TONE",       100.0f, 20000.0f, 20000.0f, 2000.0f, Unit::hz, "Low-pass after the drive. Tames the harmonics." } } },

        { "SNARE", "s", {
            { "Tune",      "TUNE",       100.0f, 400.0f, 185.0f, 200.0f, Unit::hz,  "Pitch of the drum head." },
            { "Pitch",     "PITCH",      0.0f,  24.0f,  6.0f,  0.0f,   Unit::semis, "Pitch bend of the head at the hit." },
            { "BodyDecay", "BODY",       0.03f, 1.5f,   0.22f, 0.3f,   Unit::sec,   "Ring time of the head." },
            { "Ring",      "RING",       0.0f,  1.0f,   0.35f, 0.0f,   Unit::pct,   "Level of the head's overtones (real membrane mode ratios)." },
            { "Snap",      "SNAP",       0.0f,  1.0f,   0.65f, 0.0f,   Unit::pct,   "Level of the snare wires." },
            { "SnapDecay", "SNAP DECAY", 0.03f, 1.5f,   0.28f, 0.3f,   Unit::sec,   "How long the wires buzz." },
            { "SnapTone",  "SNAP TONE",  1000.0f, 14000.0f, 5500.0f, 4500.0f, Unit::hz, "Colour of the wires: dark and fat to bright and crisp." },
            { "Drive",     "DRIVE",      0.0f,  1.0f,   0.2f,  0.0f,   Unit::pct,   "Saturation." } } },

        { "CLAP", "c", {
            { "Tone",      "TONE",       500.0f, 6000.0f, 1300.0f, 1500.0f, Unit::hz, "Centre of the hand clap noise band." },
            { "Res",       "RES",        0.0f,  1.0f,   0.3f,  0.0f,   Unit::pct,   "Narrower band: more tonal, more 'room'." },
            { "Spread",    "SPREAD",     0.003f, 0.03f, 0.011f, 0.01f, Unit::sec,   "Time between the individual claps." },
            { "Bursts",    "CLAPS",      1.0f,  6.0f,   4.0f,  0.0f,   Unit::count, "Number of hands. 1 = a single slap." },
            { "Decay",     "DECAY",      0.05f, 2.0f,   0.35f, 0.4f,   Unit::sec,   "Length of the tail." },
            { "Width",     "WIDTH",      0.0f,  1.0f,   0.5f,  0.0f,   Unit::pct,   "Stereo spread of the claps." },
            { "Drive",     "DRIVE",      0.0f,  1.0f,   0.1f,  0.0f,   Unit::pct,   "Saturation." } } },

        { "TOM", "t", {
            { "Tune",      "TUNE",       50.0f, 400.0f, 110.0f, 140.0f, Unit::hz,   "Pitch of the tom (MIDI notes 41-50 play a tom set around it)." },
            { "Pitch",     "PITCH",      0.0f,  24.0f,  7.0f,  0.0f,   Unit::semis, "Pitch drop after the hit." },
            { "Sweep",     "SWEEP",      0.01f, 0.5f,   0.09f, 0.1f,   Unit::sec,   "Speed of the pitch drop." },
            { "Decay",     "DECAY",      0.05f, 3.0f,   0.55f, 0.6f,   Unit::sec,   "Ring time." },
            { "Ring",      "RING",       0.0f,  1.0f,   0.3f,  0.0f,   Unit::pct,   "Overtones of the head." },
            { "Stick",     "STICK",      0.0f,  1.0f,   0.25f, 0.0f,   Unit::pct,   "Stick attack noise." },
            { "Drive",     "DRIVE",      0.0f,  1.0f,   0.15f, 0.0f,   Unit::pct,   "Saturation." } } },

        { "CLOSED HAT", "ch", {
            { "Tune",      "TUNE",       -12.0f, 12.0f, 0.0f,  0.0f,   Unit::semis, "Pitch of the metal oscillators." },
            { "Color",     "NOISE",      0.0f,  1.0f,   0.35f, 0.0f,   Unit::pct,   "Metallic (left) to noisy (right)." },
            { "Tone",      "TONE",       3000.0f, 16000.0f, 8500.0f, 8000.0f, Unit::hz, "Filter frequency." },
            { "Res",       "RES",        0.0f,  1.0f,   0.2f,  0.0f,   Unit::pct,   "Filter resonance: more 'sizzle' at the tone frequency." },
            { "Decay",     "DECAY",      0.01f, 0.6f,   0.07f, 0.1f,   Unit::sec,   "Length." } } },

        { "OPEN HAT", "oh", {
            { "Tune",      "TUNE",       -12.0f, 12.0f, 0.0f,  0.0f,   Unit::semis, "Pitch of the metal oscillators." },
            { "Color",     "NOISE",      0.0f,  1.0f,   0.35f, 0.0f,   Unit::pct,   "Metallic (left) to noisy (right)." },
            { "Tone",      "TONE",       3000.0f, 16000.0f, 8000.0f, 8000.0f, Unit::hz, "Filter frequency." },
            { "Res",       "RES",        0.0f,  1.0f,   0.2f,  0.0f,   Unit::pct,   "Filter resonance." },
            { "Decay",     "DECAY",      0.1f,  3.0f,   0.55f, 0.6f,   Unit::sec,   "Length. The closed hat chokes it." } } },

        { "PERC", "p", {
            { "Tune",      "TUNE",       100.0f, 2500.0f, 560.0f, 600.0f, Unit::hz, "Pitch: low conga, rim, clave, cowbell..." },
            { "Ratio",     "RATIO",      1.0f,  3.0f,   1.48f, 0.0f,   Unit::ratio, "Interval of the second oscillator (1.48 = classic cowbell)." },
            { "Wave",      "WAVE",       0.0f,  1.0f,   0.6f,  0.0f,   Unit::pct,   "Sine (wood, conga) to square (metal, cowbell)." },
            { "Decay",     "DECAY",      0.02f, 1.5f,   0.22f, 0.3f,   Unit::sec,   "Length." },
            { "Tone",      "TONE",       500.0f, 14000.0f, 4000.0f, 3000.0f, Unit::hz, "Low-pass brightness." },
            { "Click",     "CLICK",      0.0f,  1.0f,   0.2f,  0.0f,   Unit::pct,   "Attack click (rim shot, clave)." } } },

        { "CYMBAL", "cy", {
            { "Tune",      "TUNE",       -12.0f, 12.0f, 0.0f,  0.0f,   Unit::semis, "Pitch of the metal." },
            { "Color",     "NOISE",      0.0f,  1.0f,   0.5f,  0.0f,   Unit::pct,   "Metallic ride (left) to washy crash (right)." },
            { "Tone",      "TONE",       2000.0f, 15000.0f, 6500.0f, 6000.0f, Unit::hz, "Filter frequency." },
            { "Decay",     "DECAY",      0.2f,  8.0f,   2.2f,  2.0f,   Unit::sec,   "Length." },
            { "Width",     "WIDTH",      0.0f,  1.0f,   0.6f,  0.0f,   Unit::pct,   "Stereo width (two slightly different metals left and right)." },
            { "Bloom",     "BLOOM",      0.0f,  1.0f,   0.15f, 0.0f,   Unit::pct,   "Wash that swells after the hit, like a crash opening up." } } },
    } };
    return specs;
}
} // namespace rud
