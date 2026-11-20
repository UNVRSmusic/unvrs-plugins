#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray SpoolProcessor::modeChoices     { "Normal", "Additive", "Sampler" };
const juce::StringArray SpoolProcessor::modAChoices     { "Smooth Speed", "Dropper", "Trimmer" };
const juce::StringArray SpoolProcessor::modBChoices     { "Stepped Speed", "Scrambler", "Filter" };
const juce::StringArray SpoolProcessor::syncChoices     { "Free", "Beat", "Bar" };
const juce::StringArray SpoolProcessor::rampModeChoices { "Off", "Ramp", "Bounce" };
const juce::StringArray SpoolProcessor::rampDirChoices  { "Up", "Down" };

static const char* const rampTargetIds[] { "rampVolume", "rampRepeats", "rampModA", "rampStability", "rampModB" };

SpoolProcessor::SpoolProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pMode       = apvts.getRawParameterValue ("mode");
    pVolume     = apvts.getRawParameterValue ("volume");
    pRepeats    = apvts.getRawParameterValue ("repeats");
    pModA       = apvts.getRawParameterValue ("modA");
    pModASel    = apvts.getRawParameterValue ("modASel");
    pModAOn     = apvts.getRawParameterValue ("modAOn");
    pStab       = apvts.getRawParameterValue ("stability");
    pModB       = apvts.getRawParameterValue ("modB");
    pModBSel    = apvts.getRawParameterValue ("modBSel");
    pModBOn     = apvts.getRawParameterValue ("modBOn");
    pReverse    = apvts.getRawParameterValue ("reverse");
    pDryKill    = apvts.getRawParameterValue ("dryKill");
    pDubAfter   = apvts.getRawParameterValue ("dubAfterRec");
    pSampleLoop = apvts.getRawParameterValue ("sampleLoop");
    pSync       = apvts.getRawParameterValue ("sync");
    pRampMode   = apvts.getRawParameterValue ("rampMode");
    pRampRate   = apvts.getRawParameterValue ("rampRate");
    pRampDir    = apvts.getRawParameterValue ("rampDir");
    pRampSync   = apvts.getRawParameterValue ("rampSync");
    for (size_t i = 0; i < pRampTarget.size(); ++i)
        pRampTarget[i] = apvts.getRawParameterValue (rampTargetIds[i]);
}

juce::AudioProcessorValueTreeState::ParameterLayout SpoolProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   { return String (roundToInt (v * 100.0f)) + " %"; });
    auto centred = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   {
                       const int k = roundToInt ((v - 0.5f) * 200.0f);
                       return k == 0 ? String ("noon") : (k > 0 ? "+" : "") + String (k);
                   });
    auto vol = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   {
                       const float g = v <= 0.5f ? (2.0f * v) * (2.0f * v) : 1.0f + 2.0f * (v - 0.5f);
                       return g <= 0.0001f ? String ("-inf") : String (Decibels::gainToDecibels (g), 1) + " dB";
                   });

    auto addFloat = [&] (const char* id, const char* name, float def, AudioParameterFloatAttributes attr)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def, attr));
    };
    auto addBool = [&] (const char* id, const char* name, bool def)
    {
        l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto addChoice = [&] (const char* id, const char* name, const StringArray& c, int def)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def));
    };

    addChoice ("mode", "Mode", modeChoices, 0);
    addFloat  ("volume", "Volume", 0.5f, vol);
    addFloat  ("repeats", "Repeats", 1.0f, pct);
    addFloat  ("stability", "Stability", 0.0f, pct);

    addFloat  ("modA", "Mod A", 0.5f, centred);
    addChoice ("modASel", "Mod A Type", modAChoices, 0);
    addBool   ("modAOn", "Mod A On", false);
    addFloat  ("modB", "Mod B", 0.5f, centred);
    addChoice ("modBSel", "Mod B Type", modBChoices, 0);
    addBool   ("modBOn", "Mod B On", false);

    addBool   ("reverse", "Reverse", false);
    addBool   ("dryKill", "Dry Kill", false);
    addBool   ("dubAfterRec", "Dub After Record", false);
    addBool   ("sampleLoop", "Sampler Loop", true);
    addChoice ("sync", "Sync", syncChoices, 0);

    addChoice ("rampMode", "Ramp Mode", rampModeChoices, 0);
    addFloat  ("rampRate", "Ramp Rate", 0.4f, AudioParameterFloatAttributes());
    addChoice ("rampDir", "Ramp Direction", rampDirChoices, 0);
    addBool   ("rampSync", "Ramp Sync To Loop", false);
    addBool   ("rampVolume", "Ramp Volume", false);
    addBool   ("rampRepeats", "Ramp Repeats", false);
    addBool   ("rampModA", "Ramp Mod A", false);
    addBool   ("rampStability", "Ramp Stability", false);
    addBool   ("rampModB", "Ramp Mod B", false);
    return l;
}

bool SpoolProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SpoolProcessor::prepareToPlay (double sampleRate, int)
{
    if (sampleRate != fs || engine.getLoopLength() == 0)
    {
        fs = sampleRate;
        engine.prepare (sampleRate);
    }
    hasPending = false;
    syncWaiting = false;
}

void SpoolProcessor::sendCommand (Command c)
{
    const auto scope = cmdFifo.write (1);
    if (scope.blockSize1 > 0)
        cmdData[(size_t) scope.startIndex1] = (int) c;
}

void SpoolProcessor::toggleParam (const juce::String& id)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
}

LooperEngine::Params SpoolProcessor::readParams (int numSamples)
{
    LooperEngine::Params p;
    p.mode        = (LooperEngine::Mode) juce::jlimit (0, 2, (int) pMode->load());
    p.modASel     = (int) pModASel->load();
    p.modBSel     = (int) pModBSel->load();
    p.modAOn      = pModAOn->load() > 0.5f;
    p.modBOn      = pModBOn->load() > 0.5f;
    p.reverse     = pReverse->load() > 0.5f;
    p.dryKill     = pDryKill->load() > 0.5f;
    p.dubAfterRec = pDubAfter->load() > 0.5f;
    p.sampleLoop  = pSampleLoop->load() > 0.5f;

    // ---- ramp modulator --------------------------------------------------------------
    const int rampMode = (int) pRampMode->load();
    if (rampMode != lastRampMode || rampRetrigger.exchange (false))
    {
        rampPhase = 0.0;
        lastRampMode = rampMode;
    }

    double m = 0.0;
    if (rampMode > 0)
    {
        const float rate = pRampRate->load();
        double period;
        const int L = engine.getLoopLength();
        if (pRampSync->load() > 0.5f && L > 0)
        {
            static constexpr double mults[] { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };
            period = mults[juce::jlimit (0, 5, (int) std::lround (rate * 5.0f))] * L / fs;
        }
        else
            period = 0.2 * std::pow (300.0, (double) rate); // 0.2 s .. 60 s

        rampPhase += numSamples / fs / period;

        if (rampMode == 1)
            m = juce::jmin (1.0, rampPhase);
        else
        {
            const double f = rampPhase - std::floor (rampPhase);
            m = f < 0.5 ? 2.0 * f : 2.0 - 2.0 * f;
        }
    }

    const float extreme = pRampDir->load() > 0.5f ? 0.0f : 1.0f;
    const float raw[5] { pVolume->load(), pRepeats->load(), pModA->load(), pStab->load(), pModB->load() };
    float eff[5];
    for (size_t i = 0; i < 5; ++i)
    {
        const bool on = pRampTarget[i]->load() > 0.5f;
        eff[i] = on ? raw[i] + (float) m * (extreme - raw[i]) : raw[i];
        modulated[i] = eff[i];
    }

    p.volume = eff[0];
    p.repeats = eff[1];
    p.modA = eff[2];
    p.stability = eff[3];
    p.modB = eff[4];
    return p;
}

void SpoolProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (nch == 0 || n == 0)
        return;

    engine.setParams (readParams (n));

    // ---- host transport for quantised recording ------------------------------------------
    bool hostPlaying = false;
    double ppq = 0.0, bpm = 120.0, barStart = 0.0, barLen = 4.0;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (const auto p = pos->getPpqPosition()) ppq = *p; else hostPlaying = false;
            if (const auto b = pos->getBpm()) bpm = *b;
            if (const auto ts = pos->getTimeSignature()) barLen = ts->numerator * 4.0 / juce::jmax (1, ts->denominator);
            if (const auto lb = pos->getPpqPositionOfLastBarStart()) barStart = *lb;
        }

    const int syncMode = (int) pSync->load();
    const bool syncOn = syncMode > 0 && hostPlaying && bpm > 0.0;
    const double qps = bpm / 60.0 / fs;                       // quarter notes per sample
    const double grid = syncMode == 2 ? barLen : 1.0;
    const double gridOrigin = syncMode == 2 ? barStart : 0.0;

    struct Event { int offset; Command cmd; bool synced; };
    std::array<Event, 96> events;
    int numEvents = 0;

    auto push = [&] (int offset, Command c, bool synced)
    {
        if (numEvents < (int) events.size())
            events[(size_t) numEvents++] = { juce::jlimit (0, n, offset), c, synced };
    };

    auto quantisable = [] (Command c)
    {
        return c == Command::recPlay || c == Command::oneShot || c == Command::trigger;
    };

    auto addCmd = [&] (Command c, int offset)
    {
        if (syncOn && quantisable (c))
        {
            const double at = ppq + offset * qps;
            const double k = std::ceil ((at - gridOrigin) / grid - 1.0e-4);
            pendingPpq = gridOrigin + k * grid;
            pendingCmd = c;
            hasPending = true;
        }
        else
            push (offset, c, false);
    };

    // GUI commands
    {
        const auto scope = cmdFifo.read (cmdFifo.getNumReady());
        scope.forEach ([&] (int idx) { addCmd ((Command) cmdData[(size_t) idx], 0); });
    }

    // MIDI notes
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int note = msg.getNoteNumber() - noteBase;

        if (msg.isNoteOn())
        {
            noteOnAt[(size_t) msg.getNoteNumber()] = sampleCounter + meta.samplePosition;
            switch (note)
            {
                case 0: addCmd (Command::recPlay, meta.samplePosition); break;
                case 1: addCmd (Command::stop,    meta.samplePosition); break;
                case 2: addCmd (Command::oneShot, meta.samplePosition); break;
                case 3: addCmd (Command::undo,    meta.samplePosition); break;
                case 4: addCmd (Command::redo,    meta.samplePosition); break;
                case 5: addCmd (Command::clear,   meta.samplePosition); break;
                case 6: addCmd (Command::trigger, meta.samplePosition); break;
                case 7: toggleParam ("modAOn");  break;
                case 8: toggleParam ("modBOn");  break;
                case 9: toggleParam ("reverse"); break;
                default: break;
            }
        }
        else if (msg.isNoteOff() && note == 0)
        {
            // sampler: holding the record note longer than ~0.35 s records only while held
            const auto held = sampleCounter + meta.samplePosition - noteOnAt[(size_t) msg.getNoteNumber()];
            if ((int) pMode->load() == 2 && engine.getState() == LooperEngine::State::recording && held > (int64_t) (0.35 * fs))
                addCmd (Command::recPlay, meta.samplePosition);
        }
    }

    // quantised command due in this block?
    if (hasPending)
    {
        if (! syncOn)
        {
            push (0, pendingCmd, false);
            hasPending = false;
        }
        else
        {
            const double off = (pendingPpq - ppq) / qps;
            if (off < n || off > (grid / qps) * 1.05 + n) // due now, or the host transport jumped
            {
                push ((int) std::lround (juce::jmax (0.0, off)), pendingCmd, true);
                hasPending = false;
            }
        }
    }
    syncWaiting = hasPending;

    std::stable_sort (events.begin(), events.begin() + numEvents,
                      [] (const Event& a, const Event& b) { return a.offset < b.offset; });

    // ---- render, splitting the block at each command ------------------------------------------
    float* io[2] { buffer.getWritePointer (0), nch > 1 ? buffer.getWritePointer (1) : nullptr };
    int pos = 0;
    for (int e = 0; e < numEvents; ++e)
    {
        const auto& ev = events[(size_t) e];
        engine.process (io, nch, pos, ev.offset - pos);
        pos = ev.offset;

        if (ev.synced && ev.cmd == Command::recPlay && engine.getState() == LooperEngine::State::recording)
        {
            // lock the loop to a whole number of beats / bars so it never drifts against the host
            const double perGrid = grid / qps;
            const double grids = juce::jmax (1.0, std::round (engine.getRecordedLength() / perGrid));
            engine.setForcedLength ((int) std::lround (grids * perGrid));
        }

        engine.command (ev.cmd);
    }
    engine.process (io, nch, pos, n - pos);

    sampleCounter += n;
}

bool SpoolProcessor::exportLoop (juce::File& written)
{
    juce::AudioBuffer<float> buf;
    if (! engine.copyCurrentLayer (buf))
        return false;

    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Spool");
    if (! dir.createDirectory())
        return false;

    written = dir.getNonexistentChildFile ("Spool " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"), ".wav");

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (written);
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (engine.getSampleRate())
                                                                              .withNumChannels (2)
                                                                              .withBitsPerSample (24));
    if (writer == nullptr)
        return false;

    return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

void SpoolProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SpoolProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SpoolProcessor::createEditor()
{
    return new SpoolEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpoolProcessor();
}
