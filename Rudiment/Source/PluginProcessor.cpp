#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace rud;

namespace
{
struct Kit
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // full parameter id, real value
};

const std::vector<Kit>& kits()
{
    static const std::vector<Kit> list {
        { "Init", {} },
        { "Analog Classic", { { "kTune", 52 }, { "kPitch", 30 }, { "kSweep", 0.05f }, { "kHold", 0.04f }, { "kDecay", 0.8f }, { "kClick", 0.25f }, { "kDrive", 0.2f },
                              { "sSnap", 0.6f }, { "chDecay", 0.06f }, { "glue", 0.25f } } },
        { "Deep Boom", { { "kTune", 44 }, { "kPitch", 24 }, { "kSweep", 0.07f }, { "kHold", 0.08f }, { "kDecay", 2.2f }, { "kClick", 0.12f }, { "kDrive", 0.15f }, { "kTone", 6000 },
                         { "sTune", 175 }, { "sSnapTone", 4000 }, { "sSnapDecay", 0.35f }, { "cDecay", 0.5f }, { "ohDecay", 0.8f }, { "glue", 0.3f } } },
        { "Tight Techno", { { "kTune", 53 }, { "kPitch", 32 }, { "kSweep", 0.03f }, { "kHold", 0.02f }, { "kDecay", 0.45f }, { "kClick", 0.5f }, { "kClickTone", 6000 },
                            { "kDrive", 0.45f }, { "kTone", 8000 },
                            { "sTune", 210 }, { "sSnapTone", 7000 }, { "sBodyDecay", 0.15f }, { "sSnapDecay", 0.2f },
                            { "chDecay", 0.04f }, { "chTone", 10000 }, { "chColor", 0.2f }, { "ohDecay", 0.3f },
                            { "pTune", 900 }, { "pDecay", 0.08f }, { "pWave", 0.2f }, { "pClick", 0.5f }, { "glue", 0.45f }, { "drive", 0.2f } } },
        { "Punchy House", { { "kTune", 55 }, { "kPitch", 26 }, { "kSweep", 0.04f }, { "kHold", 0.05f }, { "kDecay", 0.6f }, { "kClick", 0.3f }, { "kDrive", 0.3f },
                            { "cTone", 1400 }, { "cDecay", 0.4f }, { "cWidth", 0.7f }, { "ohDecay", 0.45f }, { "ohTone", 9000 }, { "glue", 0.35f } } },
        { "Lo-Fi Dusty", { { "kTone", 2500 }, { "kDrive", 0.5f }, { "kClick", 0.15f }, { "sSnapTone", 3500 }, { "sDrive", 0.5f }, { "sRing", 0.5f },
                           { "chTone", 6000 }, { "chColor", 0.6f }, { "ohTone", 6000 }, { "cyTone", 4500 }, { "cTone", 1000 }, { "drive", 0.5f }, { "glue", 0.5f } } },
        { "Hip Hop Knock", { { "kTune", 58 }, { "kPitch", 20 }, { "kSweep", 0.035f }, { "kHold", 0.03f }, { "kDecay", 0.5f }, { "kClick", 0.55f }, { "kClickTone", 3000 },
                             { "kDrive", 0.35f }, { "sTune", 230 }, { "sRing", 0.6f }, { "sSnap", 0.55f }, { "sBodyDecay", 0.3f }, { "sSnapTone", 4500 },
                             { "sDrive", 0.3f }, { "chDecay", 0.05f }, { "glue", 0.4f } } },
        { "Industrial", { { "kDrive", 0.9f }, { "kTone", 4000 }, { "kClick", 0.6f }, { "kDecay", 0.6f }, { "sDrive", 0.8f }, { "sSnap", 0.9f }, { "sSnapTone", 3000 },
                          { "cDrive", 0.6f }, { "cRes", 0.6f }, { "tDrive", 0.7f }, { "pWave", 1.0f }, { "pRatio", 2.3f }, { "drive", 0.6f }, { "glue", 0.7f } } },
        { "Minimal Click", { { "kTune", 60 }, { "kPitch", 20 }, { "kSweep", 0.02f }, { "kDecay", 0.25f }, { "kClick", 0.7f }, { "kClickTone", 9000 }, { "kHold", 0.0f },
                             { "pTune", 1200 }, { "pDecay", 0.06f }, { "pWave", 0.1f }, { "pClick", 0.4f },
                             { "chDecay", 0.03f }, { "chTone", 12000 }, { "chColor", 0.15f }, { "cBursts", 2 }, { "cDecay", 0.15f }, { "cTone", 2000 }, { "sSnap", 0.3f } } },
        { "Trap Sub", { { "kTune", 38 }, { "kPitch", 18 }, { "kSweep", 0.08f }, { "kHold", 0.12f }, { "kDecay", 2.8f }, { "kClick", 0.2f }, { "kDrive", 0.25f },
                        { "sTune", 200 }, { "sSnap", 0.8f }, { "sSnapTone", 6000 }, { "sBodyDecay", 0.12f }, { "cTone", 1600 }, { "cDecay", 0.25f },
                        { "chDecay", 0.04f }, { "chTone", 11000 }, { "glue", 0.3f } } },
    };
    return list;
}

struct PatternPreset { const char* name; std::array<const char*, numElements> rows; };

const std::vector<PatternPreset>& patterns()
{
    //                                  kick                snare               clap                tom                 closed hat          open hat            perc                cymbal
    static const std::vector<PatternPreset> list {
        { "Clear",             { "", "", "", "", "", "", "", "" } },
        { "Four on the Floor", { "X...x...X...x...", "................", "....x.......x...", "................", "..x...x...x...x.", "................", "................", "X..............." } },
        { "House",             { "X...x...X...x...", "................", "....x.......x...", "................", "x.x...x.x.x...x.", "..x...x...x...x.", "......x.......x.", "................" } },
        { "Techno",            { "X...x...X...x...", "................", "................", "................", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "...x..x....x..x.", "................" } },
        { "Breakbeat",         { "X.x.......X.....", "....X..x.x..X...", "................", "................", "x.x.x.x.x.x.x.x.", ".............x..", "................", "X..............." } },
        { "Hip Hop",           { "X......x..X.....", "....X.......X...", "................", "................", "x.x.x.x.x.x.x.x.", "................", "...........x....", "................" } },
        { "Trap",              { "X......x.x......", "........X.......", "........x.......", "................", "x.x.x.x.xxx.x.xx", "................", "................", "................" } },
        { "Electro",           { "X.....x...X.....", "....X.......X...", "................", "..........x..x..", "x.x.x.x.x.x.x.x.", "................", "..x..x....x..x..", "................" } },
        { "Disco",             { "X...x...X...x...", "....x.......x...", "................", "................", "x...x...x...x...", "..x...x...x...x.", "................", "X..............." } },
        { "Polymeter",         { "X...x...X...x...", "....x.......x...", "................", "x..x..x..x..x...", "x.x.x.x.x.x.x.x.", "................", "x..x..x.x.......", "................" } },
    };
    return list;
}
} // namespace

//==============================================================================
RudimentProcessor::RudimentProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    const auto& specs = elementSpecs();
    for (int e = 0; e < numElements; ++e)
    {
        for (size_t i = 0; i < specs[(size_t) e].params.size(); ++i)
            elemPtr[(size_t) e][i] = apvts.getRawParameterValue (paramId (e, specs[(size_t) e].params[i].id));
        levelPtr[(size_t) e] = apvts.getRawParameterValue (paramId (e, "Level"));
        panPtr[(size_t) e]   = apvts.getRawParameterValue (paramId (e, "Pan"));
        mutePtr[(size_t) e]  = apvts.getRawParameterValue (paramId (e, "Mute"));
        playStep[(size_t) e] = -1;
        meters[(size_t) e] = 0.0f;
    }
    swingPtr = apvts.getRawParameterValue ("swing");
    accentPtr = apvts.getRawParameterValue ("accent");
    gluePtr = apvts.getRawParameterValue ("glue");
    drivePtr = apvts.getRawParameterValue ("drive");
    volumePtr = apvts.getRawParameterValue ("volume");
    runPtr = apvts.getRawParameterValue ("run");
    loadPattern (1);
    setLatencySamples (MasterBus::latency);
}

juce::AudioProcessorValueTreeState::ParameterLayout RudimentProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto attrs = [] (Unit u)
    {
        return AudioParameterFloatAttributes().withStringFromValueFunction ([u] (float v, int) -> String
        {
            switch (u)
            {
                case Unit::hz:    return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
                case Unit::sec:   return v < 1.0f ? String (v * 1000.0f, v < 0.01f ? 1 : 0) + " ms" : String (v, 2) + " s";
                case Unit::semis: return (v > 0.05f ? "+" : "") + String (v, 1) + " st";
                case Unit::ratio: return String (v, 2) + "x";
                case Unit::count: return String (roundToInt (v));
                case Unit::pct:
                default:          return String (roundToInt (v * 100.0f)) + " %";
            }
        });
    };

    for (int e = 0; e < numElements; ++e)
    {
        const auto& es = elementSpecs()[(size_t) e];
        const String nm = String (es.name).toLowerCase().replace (" ", "-");
        auto title = [&] (const String& s) { return String (es.name).substring (0, 1) + String (es.name).substring (1).toLowerCase() + " " + s; };
        for (const auto& ps : es.params)
        {
            NormalisableRange<float> r (ps.min, ps.max, ps.unit == Unit::count ? 1.0f : 0.0f);
            if (ps.centre > 0.0f) r.setSkewForCentre (ps.centre);
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { paramId (e, ps.id), 1 }, title (String (ps.label).toLowerCase()), r, ps.def, attrs (ps.unit)));
        }
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { paramId (e, "Level"), 1 }, title ("level"), NormalisableRange<float> (-36.0f, 6.0f, 0.1f), 0.0f,
                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= -35.9f ? String ("-inf") : (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { paramId (e, "Pan"), 1 }, title ("pan"), NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return std::abs (v) < 0.01f ? String ("C") : (v < 0 ? "L" : "R") + String (roundToInt (std::abs (v) * 100.0f)); })));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { paramId (e, "Mute"), 1 }, title ("mute"), false));
    }

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", NormalisableRange<float> (0.5f, 0.75f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "accent", 1 }, "Accent", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "glue", 1 }, "Glue", NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -3.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "run", 1 }, "Run", false));
    return l;
}

bool RudimentProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RudimentProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    engine.prepare (sampleRate, juce::jmax (512, samplesPerBlock));
    setLatencySamples (MasterBus::latency);
}

// General MIDI drum map (plus the toms spread around the TOM tune)
int RudimentProcessor::addMidiEvent (const juce::MidiMessage& m, int sample, Event* ev, int count)
{
    if (! m.isNoteOn() || count >= (int) events.size()) return count;
    int e = -1; float pitch = 0.0f;
    switch (m.getNoteNumber())
    {
        case 35: case 36:                     e = kick; break;
        case 38: case 40:                     e = snare; break;
        case 39:                              e = clap; break;
        case 41: e = tom; pitch = -5; break;
        case 43: e = tom; pitch = -3; break;
        case 45: e = tom; pitch = 0; break;
        case 47: e = tom; pitch = 3; break;
        case 48: e = tom; pitch = 5; break;
        case 50: e = tom; pitch = 7; break;
        case 42: case 44:                     e = closedHat; break;
        case 46:                              e = openHat; break;
        case 37: case 56: case 54: case 75:   e = perc; break;
        case 49: case 51: case 52: case 53: case 55: case 57: case 59: e = cymbal; break;
        default: break;
    }
    if (e < 0) return count;
    ev[count] = { sample, e, m.getFloatVelocity(), pitch };
    return count + 1;
}

void RudimentProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const auto& specs = elementSpecs();

    // ---- parameters
    for (int e = 0; e < numElements; ++e)
    {
        auto& el = engine.element (e);
        for (size_t i = 0; i < specs[(size_t) e].params.size(); ++i)
            el.p[i] = elemPtr[(size_t) e][i]->load (std::memory_order_relaxed);
        auto& mx = engine.mix[(size_t) e];
        const float lv = levelPtr[(size_t) e]->load();
        mx.level = lv <= -35.9f ? 0.0f : juce::Decibels::decibelsToGain (lv);
        mx.pan = panPtr[(size_t) e]->load();
        mx.mute = mutePtr[(size_t) e]->load() > 0.5f;
    }
    engine.glue = gluePtr->load();
    engine.driveAmt = drivePtr->load();
    engine.volume = juce::Decibels::decibelsToGain (volumePtr->load());

    // ---- events: preview, MIDI, sequencer
    int ne = 0;
    const uint32_t mask = previewMask.exchange (0);
    for (int e = 0; e < numElements; ++e)
        if ((mask & (1u << e)) != 0) events[(size_t) ne++] = { 0, e, 0.9f, 0.0f };
    for (const auto meta : midi)
        ne = addMidiEvent (meta.getMessage(), juce::jlimit (0, n - 1, meta.samplePosition), events.data(), ne);

    double ppqStart = 0.0, bpm = 120.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppqStart = *p; else hostPlaying = false;
        }
    const bool run = runPtr->load() > 0.5f;
    const double ppqPerSample = bpm / (60.0 * fs);
    bool seqActive = true;
    if (hostPlaying)
        internalPpq = ppqStart + n * ppqPerSample;
    else if (run)
    {
        if (! wasRunning) internalPpq = -1e-9;
        ppqStart = internalPpq;
        internalPpq += n * ppqPerSample;
    }
    else seqActive = false;
    wasRunning = run && ! hostPlaying;

    if (seqActive)
    {
        const double swing = swingPtr->load(), accent = accentPtr->load();
        const double ppqEnd = ppqStart + n * ppqPerSample;
        StepClock::forEach (ppqStart, ppqEnd, swing, [&] (long k, double t)
        {
            const int sample = juce::jlimit (0, n - 1, (int) ((t - ppqStart) / ppqPerSample));
            for (int e = 0; e < numElements; ++e)
            {
                const int s = StepClock::wrap (k, pattern.len (e));
                playStep[(size_t) e] = s;
                const uint8_t st = pattern.get (e, s);
                if (st != 0 && ne < (int) events.size())
                    events[(size_t) ne++] = { sample, e, st == 2 ? 1.0f : (float) (1.0 - 0.45 * accent), 0.0f };
            }
        });
    }
    else
        for (auto& p : playStep) p = -1;

    std::stable_sort (events.begin(), events.begin() + ne, [] (const Event& a, const Event& b) { return a.sample < b.sample; });

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    engine.render (L, R, n, events.data(), ne);
    for (int e = 0; e < numElements; ++e)
        meters[(size_t) e] = engine.meter[(size_t) e];
}

//==============================================================================
int RudimentProcessor::getNumPrograms() { return (int) kits().size(); }

const juce::String RudimentProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) kits().size()) ? juce::String (kits()[(size_t) i].name) : juce::String();
}

void RudimentProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) kits().size()))
        return;
    currentKit = index;
    const auto& kit = kits()[(size_t) index];
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id == "run" || id == "swing" || id.endsWith ("Mute") || id.endsWith ("Level") || id.endsWith ("Pan") || id == "volume")
                continue;   // kits change sounds, not the mix or transport
            float value = rp->getDefaultValue();
            for (const auto& [pid, v] : kit.values)
                if (id == pid) value = rp->convertTo0to1 (v);
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (value);
            rp->endChangeGesture();
        }
}

juce::StringArray RudimentProcessor::patternNames()
{
    juce::StringArray s;
    for (auto& p : patterns()) s.add (p.name);
    return s;
}

void RudimentProcessor::loadPattern (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) patterns().size())) return;
    const auto& p = patterns()[(size_t) index];
    for (int e = 0; e < numElements; ++e)
    {
        pattern.setRow (e, p.rows[(size_t) e]);
        pattern.length[(size_t) e] = numSteps;
    }
    if (juce::String (p.name) == "Polymeter")
    {
        pattern.length[tom] = 12;
        pattern.length[perc] = 10;
    }
}

void RudimentProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("kit", currentKit, nullptr);
    juce::ValueTree pat ("PATTERN");
    for (int e = 0; e < numElements; ++e)
    {
        pat.setProperty ("row" + juce::String (e), juce::String (pattern.rowString (e)), nullptr);
        pat.setProperty ("len" + juce::String (e), pattern.len (e), nullptr);
    }
    state.removeChild (state.getChildWithName ("PATTERN"), nullptr);
    state.appendChild (pat, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RudimentProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentKit = (int) tree.getProperty ("kit", 0);
            const auto pat = tree.getChildWithName ("PATTERN");
            if (pat.isValid())
                for (int e = 0; e < numElements; ++e)
                {
                    pattern.setRow (e, pat.getProperty ("row" + juce::String (e)).toString().toStdString());
                    pattern.length[(size_t) e] = juce::jlimit (1, numSteps, (int) pat.getProperty ("len" + juce::String (e), numSteps));
                }
            tree.removeChild (pat, nullptr);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* RudimentProcessor::createEditor() { return new RudimentEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RudimentProcessor(); }
