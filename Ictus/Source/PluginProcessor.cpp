#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray IctusProcessor::typeChoices      { "Beam", "Marimba", "String", "Membrane", "Plate", "Pipe", "Bell", "Bowl" };
const juce::StringArray IctusProcessor::modeCountChoices { "32 modes", "64 modes", "96 modes", "128 modes" };
const juce::StringArray IctusProcessor::structureChoices { "Parallel", "Serial 1 > 2" };
const juce::StringArray IctusProcessor::noiseTypeChoices { "Low Pass", "Band Pass", "High Pass" };

//==============================================================================
namespace
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // real units, choices as index
};

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Init", { { "volume", -2.0f } } },
        { "Marimba", { { "volume", -4.0f }, { "r1Type", 1 }, { "r1Shape", 1.0f }, { "r1Decay", 1.2f }, { "r1Material", 0.3f }, { "r1Hit", 0.25f }, { "r1Bright", -0.1f }, { "r1Damp", 0.85f },
                       { "malletStiff", 0.4f }, { "malletVelStiff", 0.6f },
                       { "r2On", 1 }, { "r2Type", 5 }, { "r2Shape", 0.0f }, { "r2Decay", 0.5f }, { "r2Material", 0.4f }, { "r2Gain", -12.0f } } },
        { "Xylophone", { { "volume", -1.5f }, { "r1Type", 1 }, { "r1Shape", 0.0f }, { "r1Tune", 12 }, { "r1Decay", 0.6f }, { "r1Material", 0.55f }, { "r1Bright", 0.2f }, { "r1Damp", 0.90f },
                         { "malletStiff", 0.85f } } },
        { "Vibraphone", { { "volume", -2.5f }, { "r1Type", 1 }, { "r1Shape", 1.0f }, { "r1Decay", 5.0f }, { "r1Material", 0.85f }, { "r1Damp", 0.15f }, { "r1Bright", -0.2f }, { "r1Width", 0.7f },
                          { "malletStiff", 0.35f } } },
        { "Glockenspiel", { { "volume", -0.5f }, { "r1Type", 0 }, { "r1Tune", 24 }, { "r1Decay", 3.5f }, { "r1Material", 0.92f }, { "r1Hit", 0.6f }, { "r1Bright", 0.1f }, { "r1Damp", 0.80f },
                            { "malletStiff", 0.95f } } },
        { "Kalimba", { { "volume", -11.5f }, { "r1Type", 0 }, { "r1Shape", 1.0f }, { "r1Decay", 1.6f }, { "r1Material", 0.7f }, { "r1Hit", 0.85f }, { "r1Listen", 0.6f },
                       { "malletStiff", 0.6f }, { "malletNoise", 0.15f }, { "malletColor", 0.6f },
                       { "r2On", 1 }, { "r2Type", 4 }, { "r2Key", 0.3f }, { "r2Decay", 0.25f }, { "r2Material", 0.4f }, { "r2Gain", -14.0f } } },
        { "Music Box", { { "volume", -6.0f }, { "r1Type", 0 }, { "r1Shape", 1.0f }, { "r1Tune", 24 }, { "r1Decay", 2.5f }, { "r1Material", 0.95f }, { "r1Hit", 1.0f }, { "r1Damp", 1.00f }, { "r1Bright", 0.2f },
                         { "malletStiff", 1.0f } } },
        { "Nylon Pluck", { { "volume", -10.0f }, { "r1Type", 2 }, { "r1Shape", 0.05f }, { "r1Decay", 2.5f }, { "r1Material", 0.35f }, { "r1Hit", 0.75f }, { "r1Listen", 0.6f }, { "r1Width", 0.6f }, { "r1Bright", -0.2f },
                           { "malletStiff", 0.55f }, { "malletNoise", 0.1f } } },
        { "Steel Harp", { { "volume", -4.5f }, { "r1Type", 2 }, { "r1Shape", 0.25f }, { "r1Decay", 5.0f }, { "r1Material", 0.75f }, { "r1Hit", 0.8f }, { "r1Damp", 0.70f }, { "malletStiff", 0.8f } } },
        { "Hammer String", { { "volume", 0.0f }, { "r1Type", 2 }, { "r1Shape", 0.35f }, { "r1Decay", 6.0f }, { "r1Material", 0.55f }, { "r1Hit", 0.75f }, { "r1Damp", 0.15f }, { "r1Gain", -3.0f },
                             { "malletStiff", 0.4f }, { "malletVelStiff", 1.0f },
                             { "structure", 1 }, { "r2On", 1 }, { "r2Type", 4 }, { "r2Decay", 0.6f }, { "r2Key", 0.5f }, { "r2Material", 0.6f }, { "r2Gain", -6.0f } } },
        { "Tom", { { "volume", -16.0f }, { "r1Type", 3 }, { "r1Key", 0.5f }, { "r1Tune", -12 }, { "r1Decay", 0.8f }, { "r1Material", 0.45f }, { "r1Hit", 0.35f }, { "r1Listen", 0.4f }, { "r1Shape", 0.2f },
                   { "penvAmt", 5.0f }, { "penvTime", 0.06f }, { "malletNoise", 0.25f },
                   { "r2On", 1 }, { "r2Type", 5 }, { "r2Key", 0.5f }, { "r2Tune", -12 }, { "r2Decay", 0.3f }, { "r2Gain", -12.0f } } },
        { "Tabla", { { "volume", 0.0f }, { "r1Type", 3 }, { "r1Shape", 0.9f }, { "r1Decay", 1.2f }, { "r1Material", 0.55f }, { "r1Hit", 0.75f }, { "malletStiff", 0.6f } } },
        { "Kettle Drum", { { "volume", 4.0f }, { "r1Type", 3 }, { "r1Shape", 0.75f }, { "r1Tune", -12 }, { "r1Decay", 3.0f }, { "r1Material", 0.6f }, { "r1Hit", 0.7f }, { "r1Width", 0.8f },
                           { "malletStiff", 0.25f } } },
        { "Steel Plate", { { "volume", -8.0f }, { "r1Type", 4 }, { "r1Shape", 0.35f }, { "r1Tune", -12 }, { "r1Decay", 6.0f }, { "r1Material", 0.92f }, { "r1Hit", 0.6f }, { "r1Listen", 0.7f }, { "r1Width", 0.8f },
                           { "malletStiff", 0.75f } } },
        { "Gong", { { "volume", -1.5f }, { "r1Type", 4 }, { "r1Tune", -24 }, { "r1Decay", 14.0f }, { "r1Material", 0.97f }, { "r1Inharm", 0.25f }, { "r1Hit", 0.3f }, { "r1Width", 1.0f }, { "r1Damp", 1.00f },
                    { "malletStiff", 0.15f }, { "malletNoise", 0.1f }, { "r1Modes", 3 } } },
        { "Church Bell", { { "volume", -7.5f }, { "r1Type", 6 }, { "r1Tune", -12 }, { "r1Decay", 9.0f }, { "r1Material", 0.9f }, { "r1Hit", 0.6f }, { "r1Width", 0.8f }, { "r1Damp", 1.00f },
                           { "malletStiff", 0.85f } } },
        { "Bright Bell", { { "volume", -7.0f }, { "r1Type", 6 }, { "r1Shape", 1.0f }, { "r1Decay", 5.0f }, { "r1Material", 0.95f }, { "r1Hit", 0.8f }, { "malletStiff", 0.95f } } },
        { "Singing Bowl", { { "volume", 0.5f }, { "r1Type", 7 }, { "r1Shape", 0.5f }, { "r1Tune", -12 }, { "r1Decay", 16.0f }, { "r1Material", 0.97f }, { "r1Hit", 0.4f }, { "r1Width", 0.9f }, { "r1Damp", 1.00f },
                            { "malletStiff", 0.1f }, { "malletNoise", 0.05f } } },
        { "Bowed Glass", { { "volume", -8.0f }, { "r1Type", 7 }, { "r1Shape", 0.3f }, { "r1Decay", 6.0f }, { "r1Material", 0.95f }, { "r1Damp", 0.80f },
                           { "malletOn", 0 }, { "noiseOn", 1 }, { "noiseType", 1 }, { "noiseFreq", 2500.0f }, { "noiseKey", 1.0f }, { "noiseRes", 0.4f },
                           { "noiseA", 0.4f }, { "noiseD", 1.0f }, { "noiseS", 0.8f }, { "noiseR", 1.2f } } },
        { "Breath Flute", { { "volume", -12.0f }, { "r1Type", 5 }, { "r1Shape", 1.0f }, { "r1Decay", 0.35f }, { "r1Damp", 0.30f }, { "r1Material", 0.6f }, { "bleed", 0.15f },
                            { "malletOn", 0 }, { "noiseOn", 1 }, { "noiseType", 1 }, { "noiseFreq", 1000.0f }, { "noiseKey", 1.0f }, { "noiseRes", 0.15f },
                            { "noiseA", 0.08f }, { "noiseD", 0.4f }, { "noiseS", 0.7f }, { "noiseR", 0.15f } } },
        { "Pan Pipe", { { "volume", -8.5f }, { "r1Type", 5 }, { "r1Shape", 0.0f }, { "r1Decay", 0.5f }, { "r1Damp", 0.30f }, { "r1Material", 0.6f }, { "bleed", 0.1f },
                        { "malletOn", 0 }, { "noiseOn", 1 }, { "noiseType", 1 }, { "noiseFreq", 1200.0f }, { "noiseKey", 1.0f }, { "noiseRes", 0.2f },
                        { "noiseA", 0.05f }, { "noiseD", 0.3f }, { "noiseS", 0.6f }, { "noiseR", 0.12f } } },
        { "Wood Block", { { "volume", -5.5f }, { "r1Type", 0 }, { "r1Tune", 12 }, { "r1Decay", 0.25f }, { "r1Material", 0.1f }, { "r1Hit", 0.4f }, { "malletStiff", 0.9f } } },
        { "Plucked Plate", { { "volume", -12.0f }, { "r1Type", 2 }, { "r1Decay", 3.0f }, { "r1Hit", 0.7f }, { "r1Gain", -6.0f }, { "malletStiff", 0.6f },
                             { "structure", 1 }, { "r2On", 1 }, { "r2Type", 4 }, { "r2Decay", 2.0f }, { "r2Material", 0.85f }, { "r2Shape", 0.4f } } },
        { "Crystal Keys", { { "volume", -5.0f }, { "r1Type", 7 }, { "r1Shape", 0.2f }, { "r1Decay", 4.0f }, { "r1Material", 0.9f }, { "malletStiff", 0.7f },
                            { "r2On", 1 }, { "r2Type", 0 }, { "r2Tune", 12 }, { "r2Decay", 2.0f }, { "r2Material", 0.85f }, { "r2Gain", -6.0f } } },
    };
    return list;
}
} // namespace

//==============================================================================
IctusProcessor::IctusProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto P = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    ptrs.malletOn = P ("malletOn"); ptrs.malletVol = P ("malletVol"); ptrs.malletStiff = P ("malletStiff");
    ptrs.malletVelStiff = P ("malletVelStiff"); ptrs.malletNoise = P ("malletNoise"); ptrs.malletColor = P ("malletColor");
    ptrs.noiseOn = P ("noiseOn"); ptrs.noiseType = P ("noiseType"); ptrs.noiseVol = P ("noiseVol"); ptrs.noiseFreq = P ("noiseFreq");
    ptrs.noiseRes = P ("noiseRes"); ptrs.noiseKey = P ("noiseKey"); ptrs.noiseA = P ("noiseA"); ptrs.noiseD = P ("noiseD");
    ptrs.noiseS = P ("noiseS"); ptrs.noiseR = P ("noiseR");
    ptrs.structure = P ("structure"); ptrs.voices = P ("voices"); ptrs.velocity = P ("velocity"); ptrs.penvAmt = P ("penvAmt");
    ptrs.penvTime = P ("penvTime"); ptrs.bleed = P ("bleed"); ptrs.volume = P ("volume");
    for (int r = 0; r < 2; ++r)
    {
        auto& q = ptrs.res[r];
        auto R = [&] (const char* n) { return P (resId (r, n)); };
        q.on = R ("On"); q.type = R ("Type"); q.modes = R ("Modes"); q.tune = R ("Tune"); q.fine = R ("Fine"); q.key = R ("Key");
        q.decay = R ("Decay"); q.damp = R ("Damp"); q.material = R ("Material"); q.shape = R ("Shape"); q.inharm = R ("Inharm");
        q.bright = R ("Bright"); q.hit = R ("Hit"); q.listen = R ("Listen"); q.width = R ("Width"); q.gain = R ("Gain");
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout IctusProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto fmt = [] (std::function<String (float)> f)
    { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
    auto pct  = fmt ([] (float v) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto bip  = fmt ([] (float v) { return (v > 0.005f ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; });
    auto sec  = fmt ([] (float v) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; });
    auto db   = fmt ([] (float v) { return v <= -35.9f ? String ("-inf") : (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    auto st   = fmt ([] (float v) { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; });
    auto ct   = fmt ([] (float v) { return (v > 0.5f ? "+" : "") + String (roundToInt (v)) + " ct"; });
    auto hz   = fmt ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });

    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };
    auto add = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a)); };
    auto addBool = [&] (const String& id, const String& name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto addChoice = [&] (const String& id, const String& name, const StringArray& c, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };

    // exciters
    addBool ("malletOn", "Mallet On", true);
    add ("malletVol",      "Mallet Volume",    NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct);
    add ("malletStiff",    "Mallet Stiffness", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    add ("malletVelStiff", "Mallet Vel > Stiffness", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    add ("malletNoise",    "Mallet Noise",     NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("malletColor",    "Mallet Noise Color", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);

    addBool ("noiseOn", "Noise On", false);
    addChoice ("noiseType", "Noise Filter", noiseTypeChoices, 1);
    add ("noiseVol",  "Noise Volume",   NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    add ("noiseFreq", "Noise Freq",     skewed (20.0f, 20000.0f, 1500.0f), 2000.0f, hz);
    add ("noiseRes",  "Noise Res",      NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
    add ("noiseKey",  "Noise Key",      NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("noiseA",    "Noise Attack",   skewed (0.001f, 5.0f, 0.2f), 0.01f, sec);
    add ("noiseD",    "Noise Decay",    skewed (0.005f, 10.0f, 0.6f), 0.3f, sec);
    add ("noiseS",    "Noise Sustain",  NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("noiseR",    "Noise Release",  skewed (0.005f, 10.0f, 0.6f), 0.2f, sec);

    // resonators
    for (int r = 0; r < 2; ++r)
    {
        const String p = "Res " + String (r + 1) + " ";
        addBool (resId (r, "On"), p + "On", r == 0);
        addChoice (resId (r, "Type"), p + "Type", typeChoices, r == 0 ? 1 : 5);
        addChoice (resId (r, "Modes"), p + "Quality", modeCountChoices, 1);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { resId (r, "Tune"), 1 }, p + "Tune", NormalisableRange<float> (-48.0f, 48.0f, 1.0f), 0.0f, st));
        add (resId (r, "Fine"),     p + "Fine",      NormalisableRange<float> (-50.0f, 50.0f), 0.0f, ct);
        add (resId (r, "Key"),      p + "Key Track", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct);
        add (resId (r, "Decay"),    p + "Decay",     skewed (0.02f, 30.0f, 2.0f), 1.5f, sec);
        add (resId (r, "Damp"),     p + "Release", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
        add (resId (r, "Material"), p + "Material",  NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
        add (resId (r, "Shape"),    p + "Shape",     NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        add (resId (r, "Inharm"),   p + "Inharmonics", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bip);
        add (resId (r, "Bright"),   p + "Brightness", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bip);
        add (resId (r, "Hit"),      p + "Hit Position", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
        add (resId (r, "Listen"),   p + "Listen Position", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
        add (resId (r, "Width"),    p + "Stereo Width", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
        add (resId (r, "Gain"),     p + "Gain",      NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f, db);
    }

    // global
    addChoice ("structure", "Structure", structureChoices, 0);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "voices", 1 }, "Voices", 1, ictus::Engine::maxVoices, 12));
    add ("velocity", "Velocity Sens", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct);
    add ("penvAmt",  "Pitch Env Amount", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, st);
    add ("penvTime", "Pitch Env Time", skewed (0.005f, 2.0f, 0.15f), 0.08f, sec);
    add ("bleed",    "Exciter Bleed", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("volume",   "Volume",        NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -2.0f, db);
    return l;
}

ictus::Settings IctusProcessor::readSettings() const
{
    auto v = [] (const std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };
    const auto& q = ptrs;
    ictus::Settings s;
    s.malletOn = v (q.malletOn) > 0.5f;
    s.malletVol = v (q.malletVol); s.malletStiff = v (q.malletStiff); s.malletVelStiff = v (q.malletVelStiff);
    s.malletNoise = v (q.malletNoise); s.malletColor = v (q.malletColor);
    s.noiseOn = v (q.noiseOn) > 0.5f; s.noiseType = (int) v (q.noiseType);
    s.noiseVol = v (q.noiseVol); s.noiseFreq = v (q.noiseFreq); s.noiseRes = v (q.noiseRes); s.noiseKey = v (q.noiseKey);
    s.noiseA = v (q.noiseA); s.noiseD = v (q.noiseD); s.noiseS = v (q.noiseS); s.noiseR = v (q.noiseR);
    for (int r = 0; r < 2; ++r)
    {
        auto& rs = s.res[r];
        const auto& p = q.res[r];
        rs.on = v (p.on) > 0.5f;
        rs.rp.type = (int) v (p.type);
        rs.rp.modes = modeCounts[juce::jlimit (0, 3, (int) v (p.modes))];
        rs.rp.material = v (p.material); rs.rp.inharm = v (p.inharm); rs.rp.shape = v (p.shape);
        rs.rp.hit = v (p.hit); rs.rp.listen = v (p.listen); rs.rp.width = v (p.width); rs.rp.bright = v (p.bright);
        rs.tune = v (p.tune); rs.fine = v (p.fine); rs.key = v (p.key); rs.decay = v (p.decay); rs.damp = 1.0f - v (p.damp);   // knob: 100 % = rings on, engine: 1 = fully damped
        rs.gain = v (p.gain) <= -35.9f ? 0.0f : juce::Decibels::decibelsToGain (v (p.gain));
    }
    s.structure = (int) v (q.structure);
    s.voices = (int) v (q.voices);
    s.velocity = v (q.velocity); s.penvAmt = v (q.penvAmt); s.penvTime = v (q.penvTime); s.bleed = v (q.bleed);
    s.volume = juce::Decibels::decibelsToGain (v (q.volume), -36.0f);
    return s;
}

bool IctusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void IctusProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setSettings (readSettings());
    scratch.setSize (2, juce::jmax (512, samplesPerBlock));
    keyboardState.reset();
}

void IctusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    engine.setSettings (readSettings());

    if (scratch.getNumSamples() < n)
        scratch.setSize (2, n, false, false, true);
    float* L = scratch.getWritePointer (0);
    float* R = scratch.getWritePointer (1);

    int pos = 0;
    auto renderTo = [&] (int end) { if (end > pos) { engine.render (L + pos, R + pos, end - pos); pos = end; } };

    for (const auto meta : midi)
    {
        renderTo (juce::jlimit (0, n, meta.samplePosition));
        const auto m = meta.getMessage();
        if (m.isNoteOn())                         engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())                   engine.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel())                engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
        else if (m.isSustainPedalOn())            engine.sustain (true);
        else if (m.isSustainPedalOff())           engine.sustain (false);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    renderTo (n);

    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, L, n);
        buffer.copyFrom (1, 0, R, n);
        for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFrom (0, 0, L, n, 0.5f);
        buffer.addFrom (0, 0, R, n, 0.5f);
    }
    voicesPlaying = engine.activeVoices();
}

//==============================================================================
int IctusProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String IctusProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) presets().size()) ? juce::String (presets()[(size_t) i].name) : juce::String();
}

void IctusProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size()))
        return;
    currentProgram = index;
    const auto& pr = presets()[(size_t) index];
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            float value = rp->getDefaultValue();
            for (const auto& [id, v] : pr.values)
                if (rp->getParameterID() == id)
                    value = rp->convertTo0to1 (v);
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (value);
            rp->endChangeGesture();
        }
}

void IctusProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void IctusProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* IctusProcessor::createEditor() { return new IctusEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IctusProcessor(); }
