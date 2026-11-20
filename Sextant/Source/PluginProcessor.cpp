#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace sx;

const juce::StringArray SextantProcessor::modeNames { "FM", "Ring", "Filter", "Fold" };
const juce::StringArray SextantProcessor::waveNames { "Sine", "Triangle", "Saw", "Square", "Half Sine", "Abs Sine", "Double Sine", "Noise" };

namespace
{
struct Patch
{
    const char* name;
    int algorithm;
    std::vector<std::pair<const char*, float>> values;
};

// algorithm indices (see FmCore.h)
enum { aStack6 = 0, aStack5, aStack4Pair, a2Stack3, a3Pair, aPair4, a2Pair2, aStack3Sines, aTwoIntoOne, aThreeIntoOne,
       aFiveIntoOne, aOneTo3, aOneTo5, aTwoTo4, aDiamond, aFanStack, aDeepPair, aAdditive };

const std::vector<Patch>& patches()
{
    static const std::vector<Patch> list {
        { "Init Sine", a3Pair, { { "volume", -2.5f },} },
        { "Tine Piano", a3Pair, { { "volume", -5.5f },
            { "o1Level", 99 }, { "o1Vel", 0.3f }, { "o1D", 2.5f }, { "o1S", 0.3f }, { "o1R", 0.5f }, { "o1KeyLvl", -0.15f },
            { "o2Level", 75 }, { "o2Vel", 0.8f }, { "o2D", 1.2f }, { "o2S", 0.2f }, { "o2R", 0.4f }, { "o2KeyLvl", -0.3f },
            { "o3Level", 92 }, { "o3Detune", 4 }, { "o3Vel", 0.3f }, { "o3D", 2.5f }, { "o3S", 0.25f }, { "o3R", 0.5f },
            { "o4Level", 62 }, { "o4Coarse", 14 }, { "o4Vel", 0.9f }, { "o4D", 0.25f }, { "o4S", 0.0f }, { "o4R", 0.2f }, { "o4KeyLvl", -0.4f },
            { "chorus", 0.35f } } },
        { "Glass Bell", a3Pair, { { "volume", -3.0f },
            { "o1Level", 99 }, { "o1D", 6.0f }, { "o1S", 0.0f }, { "o1R", 3.0f },
            { "o2Level", 72 }, { "o2Coarse", 3 }, { "o2Fine", 0.1667f }, { "o2D", 4.0f }, { "o2S", 0.0f }, { "o2R", 3.0f }, { "o2Vel", 0.7f },
            { "o3Level", 85 }, { "o3Coarse", 2 }, { "o3D", 5.0f }, { "o3S", 0.0f }, { "o3R", 3.0f },
            { "o4Level", 60 }, { "o4Coarse", 7 }, { "o4Fine", 0.01f }, { "o4D", 3.0f }, { "o4S", 0.0f }, { "o4R", 2.0f },
            { "dMix", 0.2f }, { "dSync", 3 }, { "chorus", 0.2f } } },
        { "FM Bass", a3Pair, { { "volume", 6.0f },
            { "o1Level", 99 }, { "o1A", 0.001f }, { "o1D", 0.8f }, { "o1S", 0.6f }, { "o1R", 0.12f },
            { "o2Level", 80 }, { "o2Feedback", 0.6f }, { "o2D", 0.25f }, { "o2S", 0.25f }, { "o2R", 0.1f }, { "o2Vel", 0.7f },
            { "o3Level", 80 }, { "o3Coarse", 0 }, { "o3D", 1.0f }, { "o3S", 0.8f }, { "o3R", 0.12f },
            { "voiceMode", 2 }, { "glide", 0.03f } } },
        { "Brass Section", a3Pair, { { "volume", -6.0f },
            { "o1Level", 99 }, { "o1A", 0.06f }, { "o1D", 0.4f }, { "o1S", 0.85f }, { "o1R", 0.25f },
            { "o2Level", 78 }, { "o2Feedback", 0.5f }, { "o2A", 0.09f }, { "o2D", 0.5f }, { "o2S", 0.7f }, { "o2R", 0.25f }, { "o2Vel", 0.7f },
            { "o3Level", 97 }, { "o3Detune", -6 }, { "o3A", 0.07f }, { "o3D", 0.4f }, { "o3S", 0.85f }, { "o3R", 0.25f },
            { "o4Level", 76 }, { "o4Feedback", 0.5f }, { "o4A", 0.1f }, { "o4D", 0.5f }, { "o4S", 0.7f }, { "o4R", 0.25f }, { "o4Vel", 0.7f },
            { "lfoDelay", 0.6f }, { "lfoPitch", 15 }, { "chorus", 0.25f } } },
        { "Drawbar Organ", aAdditive, { { "volume", -4.0f },
            { "o1Level", 92 }, { "o1Coarse", 0 }, { "o2Level", 99 }, { "o3Level", 85 }, { "o3Coarse", 1 }, { "o3Fine", 0.5f },
            { "o4Level", 88 }, { "o4Coarse", 2 }, { "o5Level", 80 }, { "o5Coarse", 3 }, { "o6Level", 78 }, { "o6Coarse", 4 },
            { "o1R", 0.05f }, { "o2R", 0.05f }, { "o3R", 0.05f }, { "o4R", 0.05f }, { "o5R", 0.05f }, { "o6R", 0.05f },
            { "o1Vel", 0 }, { "o2Vel", 0 }, { "o3Vel", 0 }, { "o4Vel", 0 }, { "o5Vel", 0 }, { "o6Vel", 0 },
            { "chorus", 0.5f }, { "lfoAmp", 0.1f }, { "lfoRate", 6.0f } } },
        { "Soft Pad", a3Pair, { { "volume", -6.5f },
            { "o1Level", 95 }, { "o1A", 1.2f }, { "o1D", 2.0f }, { "o1S", 0.8f }, { "o1R", 2.0f },
            { "o2Level", 62 }, { "o2Coarse", 2 }, { "o2A", 1.5f }, { "o2D", 3.0f }, { "o2S", 0.6f }, { "o2R", 2.0f },
            { "o3Level", 95 }, { "o3Detune", 7 }, { "o3A", 1.2f }, { "o3D", 2.0f }, { "o3S", 0.8f }, { "o3R", 2.0f },
            { "o4Level", 58 }, { "o4A", 1.8f }, { "o4D", 3.0f }, { "o4S", 0.6f }, { "o4R", 2.0f },
            { "o5Level", 90 }, { "o5Coarse", 2 }, { "o5Detune", -7 }, { "o5A", 1.4f }, { "o5D", 2.0f }, { "o5S", 0.8f }, { "o5R", 2.0f },
            { "o6Level", 50 }, { "o6Coarse", 3 }, { "o6A", 2.0f }, { "o6D", 3.0f }, { "o6S", 0.5f }, { "o6R", 2.0f },
            { "unison", 2 }, { "uniDetune", 10 }, { "chorus", 0.6f }, { "fType", 1 }, { "cutoff", 4000 } } },
        { "Sync Lead", a3Pair, { { "volume", 4.0f },
            { "o1Level", 99 }, { "o2Level", 80 }, { "o2Feedback", 0.8f }, { "o2Vel", 0.6f },
            { "o3Level", 70 }, { "o3Coarse", 2 }, { "o3Detune", 3 },
            { "voiceMode", 2 }, { "glide", 0.08f }, { "lfoDelay", 0.4f }, { "lfoPitch", 20 }, { "dMix", 0.25f }, { "dSync", 3 } } },
        { "Pluck", a3Pair, { { "volume", -8.0f },
            { "o1Level", 99 }, { "o1D", 0.9f }, { "o1S", 0.0f }, { "o1R", 0.3f },
            { "o2Level", 82 }, { "o2Coarse", 3 }, { "o2Vel", 0.8f }, { "o2D", 0.15f }, { "o2S", 0.0f },
            { "o3Level", 90 }, { "o3Detune", 5 }, { "o3D", 0.9f }, { "o3S", 0.0f }, { "o3R", 0.3f },
            { "o4Level", 70 }, { "o4D", 0.2f }, { "o4S", 0.0f }, { "dMix", 0.2f }, { "dSync", 2 } } },
        { "Marimba", a3Pair, { { "volume", -3.0f },
            { "o1Level", 99 }, { "o1D", 0.6f }, { "o1S", 0.0f }, { "o1R", 0.4f },
            { "o2Level", 66 }, { "o2Coarse", 10 }, { "o2D", 0.04f }, { "o2S", 0.0f }, { "o2Vel", 0.8f },
            { "o3Level", 72 }, { "o3Coarse", 4 }, { "o3D", 0.25f }, { "o3S", 0.0f }, { "o3R", 0.2f } } },
        { "Clav", a3Pair, { { "volume", -6.0f },
            { "o1Level", 99 }, { "o1D", 0.5f }, { "o1S", 0.2f }, { "o1R", 0.05f },
            { "o2Level", 80 }, { "o2Coarse", 3 }, { "o2Wave", 3 }, { "o2Feedback", 0.3f }, { "o2Vel", 0.9f }, { "o2D", 0.3f }, { "o2S", 0.3f }, { "o2R", 0.05f } } },
        { "Ring Metal", a3Pair, { { "volume", -10.0f },
            { "o1Mode", 1 }, { "o1Level", 99 }, { "o1D", 3.0f }, { "o1S", 0.0f }, { "o1R", 2.0f },
            { "o2Level", 99 }, { "o2Coarse", 2 }, { "o2Fine", 0.205f }, { "o2Vel", 0 },
            { "o3Level", 70 }, { "o3D", 2.0f }, { "o3S", 0.0f }, { "o3R", 1.5f }, { "dMix", 0.25f }, { "dSync", 4 } } },
        { "Saw Through Filter Op", a3Pair, { { "volume", -4.0f },
            { "o1Mode", 2 }, { "o1Level", 99 }, { "o1Coarse", 2 }, { "o1Res", 0.75f }, { "o1D", 0.4f }, { "o1S", 0.4f }, { "o1R", 0.15f },
            { "o2Level", 99 }, { "o2Feedback", 1.0f }, { "o2Vel", 0 },
            { "fType", 2 }, { "cutoff", 600 }, { "fEnv", 0.6f }, { "fD", 0.3f }, { "fS", 0.2f }, { "voiceMode", 2 }, { "glide", 0.05f } } },
        { "Fold Bass", a3Pair, { { "volume", 6.5f },
            { "o1Mode", 3 }, { "o1Level", 99 }, { "o1Drive", 0.6f }, { "o1D", 0.5f }, { "o1S", 0.7f }, { "o1R", 0.1f },
            { "o2Level", 60 }, { "o2D", 0.3f }, { "o2S", 0.2f }, { "o2Vel", 0.8f }, { "voiceMode", 2 } } },
        { "Sub Bass", a3Pair, { { "volume", 6.5f },
            { "o1Level", 99 }, { "o1R", 0.1f }, { "o2Level", 40 }, { "o2D", 0.2f }, { "o2S", 0.0f }, { "voiceMode", 2 }, { "glide", 0.05f } } },
        { "Digital Choir", aOneTo5, { { "volume", 4.0f },
            { "o6Level", 55 }, { "o6A", 0.8f }, { "o6D", 2.0f }, { "o6S", 0.7f }, { "o6R", 1.5f },
            { "o1Level", 90 }, { "o2Level", 80 }, { "o2Coarse", 2 }, { "o2Detune", 5 }, { "o3Level", 70 }, { "o3Coarse", 3 },
            { "o4Level", 65 }, { "o4Coarse", 4 }, { "o5Level", 90 }, { "o5Detune", -5 },
            { "o1A", 0.6f }, { "o2A", 0.6f }, { "o3A", 0.6f }, { "o4A", 0.6f }, { "o5A", 0.6f },
            { "o1R", 1.5f }, { "o2R", 1.5f }, { "o3R", 1.5f }, { "o4R", 1.5f }, { "o5R", 1.5f },
            { "chorus", 0.6f }, { "lfoPitch", 8 } } },
    };
    return list;
}
} // namespace

//==============================================================================
const char* const SextantProcessor::globalIds[numGlobals] { "algo", "voiceMode", "poly", "unison", "uniDetune", "uniSpread", "glide", "bend", "fType", "cutoff", "res", "fEnv", "fKey", "fVel", "fA", "fD", "fS", "fR", "lfoWave", "lfoRate", "lfoDelay", "lfoPitch", "lfoFm", "lfoAmp", "lfoFilter", "mwVib", "chorus", "dMix", "dTime", "dFb", "dSync", "volume" };

juce::StringArray SextantProcessor::algorithmNames()
{
    juce::StringArray s;
    for (auto& a : algorithms()) s.add (a.name);
    s.add ("User");
    return s;
}

SextantProcessor::SextantProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int k = 0; k < numOps; ++k)
    {
        auto P = [&] (const char* n) { return apvts.getRawParameterValue (opId (k, n)); };
        opPtr[(size_t) k] = { P ("Mode"), P ("Wave"), P ("Fixed"), P ("Coarse"), P ("Fine"), P ("Detune"), P ("Freq"), P ("Level"), P ("Feedback"),
                              P ("Vel"), P ("KeyLvl"), P ("A"), P ("D"), P ("S"), P ("R"), P ("FiltType"), P ("Res"), P ("Drive") };
    }
    for (int i = 0; i < numGlobals; ++i)
        g[(size_t) i] = apvts.getRawParameterValue (globalIds[i]);
    setLatencySamples (Engine::latency);
}

juce::AudioProcessorValueTreeState::ParameterLayout SextantProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto fmt = [] (std::function<String (float)> f) { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
    auto pct  = fmt ([] (float v) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto bip  = fmt ([] (float v) { return (v > 0.005f ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; });
    auto sec  = fmt ([] (float v) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; });
    auto hz   = fmt ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (v, v < 100 ? 1 : 0) + " Hz"; });
    auto ct   = fmt ([] (float v) { return (v > 0.5f ? "+" : "") + String (roundToInt (v)) + " ct"; });
    auto lvl  = fmt ([] (float v) { return String (roundToInt (v)); });
    auto skewed = [] (float lo, float hi, float c) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (c); return r; };
    auto add = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a)); };
    auto choice = [&] (const String& id, const String& name, const StringArray& c, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };

    for (int k = 0; k < numOps; ++k)
    {
        const String p = "Op " + String (k + 1) + " ";
        auto id = [k] (const char* n) { return opId (k, n); };
        choice (id ("Mode"), p + "Mode", modeNames, 0);
        choice (id ("Wave"), p + "Wave", waveNames, 0);
        l.add (std::make_unique<AudioParameterBool> (ParameterID { id ("Fixed"), 1 }, p + "Fixed", false));
        l.add (std::make_unique<AudioParameterInt> (ParameterID { id ("Coarse"), 1 }, p + "Coarse", 0, 32, 1,
                   AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 0 ? String ("0.5") : String (v); })));
        add (id ("Fine"),     p + "Fine",      NormalisableRange<float> (0.0f, 1.0f), 0.0f, fmt ([] (float v) { return "x" + String (1.0f + v, 3); }));
        add (id ("Detune"),   p + "Detune",    NormalisableRange<float> (-50.0f, 50.0f), 0.0f, ct);
        add (id ("Freq"),     p + "Fixed Freq", skewed (1.0f, 9000.0f, 440.0f), 440.0f, hz);
        add (id ("Level"),    p + "Level",     NormalisableRange<float> (0.0f, 99.0f), k == 0 ? 99.0f : 0.0f, lvl);
        add (id ("Feedback"), p + "Feedback",  NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        add (id ("Vel"),      p + "Velocity",  NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
        add (id ("KeyLvl"),   p + "Key Level", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bip);
        add (id ("A"),        p + "Attack",    skewed (0.0005f, 10.0f, 0.3f), 0.002f, sec);
        add (id ("D"),        p + "Decay",     skewed (0.001f, 20.0f, 1.0f), 0.5f, sec);
        add (id ("S"),        p + "Sustain",   NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct);
        add (id ("R"),        p + "Release",   skewed (0.002f, 20.0f, 0.8f), 0.3f, sec);
        choice (id ("FiltType"), p + "Filter Type", StringArray { "Low Pass", "Band Pass", "High Pass" }, 0);
        add (id ("Res"),      p + "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct);
        add (id ("Drive"),    p + "Fold Drive", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
    }

    choice ("algo", "Algorithm", algorithmNames(), 4);
    choice ("voiceMode", "Voice Mode", StringArray { "Poly", "Mono", "Legato" }, 0);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "poly", 1 }, "Polyphony", 1, Engine::maxPoly, 8));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "unison", 1 }, "Unison", 1, 4, 1));
    add ("uniDetune", "Unison Detune", NormalisableRange<float> (0.0f, 50.0f), 12.0f, ct);
    add ("uniSpread", "Unison Spread", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct);
    add ("glide", "Glide", skewed (0.0f, 2.0f, 0.2f), 0.0f, sec);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "bend", 1 }, "Bend Range", 1, 12, 2));

    choice ("fType", "Filter Type", StringArray { "Off", "LP 12", "LP 24", "Band Pass", "High Pass" }, 0);
    add ("cutoff", "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 8000.0f, hz);
    add ("res", "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct);
    add ("fEnv", "Filter Env", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bip);
    add ("fKey", "Filter Key", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    add ("fVel", "Filter Velocity", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
    add ("fA", "Filter Attack", skewed (0.0005f, 10.0f, 0.3f), 0.005f, sec);
    add ("fD", "Filter Decay", skewed (0.001f, 20.0f, 1.0f), 0.4f, sec);
    add ("fS", "Filter Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("fR", "Filter Release", skewed (0.002f, 20.0f, 0.8f), 0.4f, sec);

    choice ("lfoWave", "LFO Wave", StringArray { "Sine", "Triangle", "Square", "Saw", "S&H" }, 0);
    add ("lfoRate", "LFO Rate", skewed (0.05f, 20.0f, 3.0f), 5.0f, hz);
    add ("lfoDelay", "LFO Delay", NormalisableRange<float> (0.0f, 5.0f), 0.0f, sec);
    add ("lfoPitch", "LFO > Pitch", NormalisableRange<float> (0.0f, 100.0f), 0.0f, ct);
    add ("lfoFm", "LFO > FM", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("lfoAmp", "LFO > Amp", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("lfoFilter", "LFO > Filter", NormalisableRange<float> (0.0f, 4.0f), 0.0f, fmt ([] (float v) { return String (v, 2) + " oct"; }));
    add ("mwVib", "Wheel > Vibrato", NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct);

    add ("chorus", "Chorus", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("dMix", "Delay Mix", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("dTime", "Delay Time", skewed (0.02f, 2.0f, 0.4f), 0.375f, sec);
    add ("dFb", "Delay Feedback", NormalisableRange<float> (0.0f, 0.9f), 0.35f, pct);
    choice ("dSync", "Delay Sync", StringArray { "Free", "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2" }, 0);
    add ("volume", "Volume", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), -3.0f, fmt ([] (float v) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }));
    return l;
}

Routing SextantProcessor::currentRouting() const
{
    const int a = (int) G (g_algo);
    if (a >= 0 && a < (int) algorithms().size()) return algorithms()[(size_t) a];
    Routing r;
    r.conn = userConn.load();
    r.carriers = userCarriers.load();
    return r;
}

void SextantProcessor::editRouting (int src, int dst, bool carrierToggle)
{
    auto r = currentRouting();
    if (carrierToggle) r.carriers ^= (uint8_t) (1u << src);
    else if (src != dst) r.conn ^= Routing::bit (src, dst);
    userConn = r.conn;
    userCarriers = r.carriers;
    if (auto* p = apvts.getParameter ("algo"))
        if ((int) G (g_algo) != userAlgorithmIndex())
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) userAlgorithmIndex()));
            p->endChangeGesture();
        }
}

Settings SextantProcessor::readSettings() const
{
    Settings s;
    auto v = [] (const std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };
    for (int k = 0; k < numOps; ++k)
    {
        const auto& q = opPtr[(size_t) k];
        auto& o = s.op[(size_t) k];
        o.mode = (int) v (q.mode); o.wave = (int) v (q.wave); o.fixed = v (q.fixed) > 0.5f;
        o.ratio = coarseToRatio ((int) v (q.coarse)) * (1.0f + v (q.fine));
        o.detune = v (q.detune); o.freq = v (q.freq); o.level = v (q.level); o.feedback = v (q.feedback);
        o.velocity = v (q.vel); o.keyLevel = v (q.keyLvl);
        o.A = v (q.A); o.D = v (q.D); o.S = v (q.S); o.R = v (q.R);
        o.filterType = (int) v (q.fType); o.res = v (q.res); o.drive = v (q.drive);
    }
    s.algo = currentRouting();
    s.voiceMode = (int) G (g_voiceMode); s.polyphony = (int) G (g_poly); s.unison = (int) G (g_unison);
    s.uniDetune = G (g_uniDetune); s.uniSpread = G (g_uniSpread); s.glide = G (g_glide); s.bendRange = G (g_bend);
    s.filterType = (int) G (g_fType); s.cutoff = G (g_cutoff); s.res = G (g_res); s.filterEnv = G (g_fEnv); s.filterKey = G (g_fKey); s.filterVel = G (g_fVel);
    s.fA = G (g_fA); s.fD = G (g_fD); s.fS = G (g_fS); s.fR = G (g_fR);
    s.lfoWave = (int) G (g_lfoWave); s.lfoRate = G (g_lfoRate); s.lfoDelay = G (g_lfoDelay); s.lfoPitch = G (g_lfoPitch);
    s.lfoFm = G (g_lfoFm); s.lfoAmp = G (g_lfoAmp); s.lfoFilter = G (g_lfoFilter); s.mwVibrato = G (g_mwVib);
    s.chorus = G (g_chorus); s.delayMix = G (g_dMix); s.delayTime = G (g_dTime); s.delayFb = G (g_dFb); s.delaySync = (int) G (g_dSync);
    s.volume = juce::Decibels::decibelsToGain (G (g_volume));
    return s;
}

bool SextantProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SextantProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (512, samplesPerBlock));
    engine.setSettings (readSettings());
    setLatencySamples (Engine::latency);
    keyboardState.reset();
}

void SextantProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    engine.setSettings (readSettings());
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                engine.setTempo (*bpm);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int pos = 0;
    auto renderTo = [&] (int end) { if (end > pos) { engine.render (L + pos, R + pos, end - pos); pos = end; } };
    for (const auto meta : midi)
    {
        renderTo (juce::jlimit (0, n, meta.samplePosition));
        const auto m = meta.getMessage();
        if (m.isNoteOn())                  engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())            engine.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel())         engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isControllerOfType (1)) engine.modWheel ((float) m.getControllerValue() / 127.0f);
        else if (m.isSustainPedalOn())     engine.sustain (true);
        else if (m.isSustainPedalOff())    engine.sustain (false);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    renderTo (n);
}

//==============================================================================
int SextantProcessor::getNumPrograms() { return (int) patches().size(); }

const juce::String SextantProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) patches().size()) ? juce::String (patches()[(size_t) i].name) : juce::String();
}

void SextantProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) patches().size())) return;
    currentProgram = index;
    const auto& pt = patches()[(size_t) index];
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            float value = rp->getDefaultValue();
            if (id == "algo") value = rp->convertTo0to1 ((float) pt.algorithm);
            for (const auto& [pid, v] : pt.values)
                if (id == pid) value = rp->convertTo0to1 (v);
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (value);
            rp->endChangeGesture();
        }
}

void SextantProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("userConn", juce::String::toHexString ((juce::int64) userConn.load()), nullptr);
    state.setProperty ("userCarriers", (int) userCarriers.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SextantProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            userConn = (uint64_t) tree.getProperty ("userConn", "0").toString().getHexValue64();
            userCarriers = (uint8_t) (int) tree.getProperty ("userCarriers", 1);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* SextantProcessor::createEditor() { return new SextantEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SextantProcessor(); }
