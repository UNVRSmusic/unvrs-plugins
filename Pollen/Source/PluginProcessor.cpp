#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

// source: 0 Glass, 1 Choir, 2 Strings, 3 Breath, 4 Music Box
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Init", { { "volume", -5.0f } } },
        { "First Light", { { "source", 0 }, { "position", 0.2f }, { "scan", 0.04f }, { "spray", 0.2f }, { "drift", 0.4f }, { "size", 260 },
                           { "density", 22 }, { "shape", 0.2f }, { "octaves", 0.22f }, { "jitter", 5 }, { "spread", 0.85f },
                           { "attack", 1.2f }, { "release", 3.5f }, { "tone", 0.82f }, { "reverb", 0.45f }, { "decay", 8 }, { "volume", -6.0f } } },
        { "Dust Choir", { { "source", 1 }, { "position", 0.4f }, { "scan", 0.02f }, { "spray", 0.35f }, { "drift", 0.5f }, { "size", 320 },
                          { "density", 26 }, { "jitter", 12 }, { "spread", 0.9f }, { "attack", 0.8f }, { "release", 3.0f },
                          { "tone", 0.75f }, { "reverb", 0.4f }, { "decay", 7 }, { "volume", -7.0f } } },
        { "Slow Tide", { { "source", 2 }, { "position", 0.3f }, { "scan", 0.08f }, { "spray", 0.15f }, { "drift", 0.35f }, { "size", 420 },
                         { "density", 14 }, { "shape", 0.15f }, { "jitter", 6 }, { "spread", 0.8f }, { "attack", 2.0f }, { "release", 4.0f },
                         { "tone", 0.7f }, { "reverb", 0.5f }, { "decay", 10 }, { "volume", -6.5f } } },
        { "Wind Organ", { { "source", 3 }, { "position", 0.5f }, { "spray", 0.5f }, { "drift", 0.6f }, { "size", 500 }, { "density", 18 },
                          { "jitter", 3 }, { "spread", 1.0f }, { "attack", 1.5f }, { "release", 4.0f }, { "tone", 0.85f },
                          { "reverb", 0.45f }, { "decay", 9 }, { "volume", -6.5f } } },
        { "Music Box Cloud", { { "source", 4 }, { "position", 0.3f }, { "scan", 0.12f }, { "spray", 0.4f }, { "drift", 0.3f }, { "size", 120 },
                               { "density", 24 }, { "shape", 0.35f }, { "jitter", 4 }, { "spread", 0.9f }, { "attack", 0.3f },
                               { "release", 3.0f }, { "reverb", 0.5f }, { "decay", 8 }, { "volume", -6.5f } } },
        { "Frozen Bell", { { "source", 0 }, { "position", 0.08f }, { "spray", 0.02f }, { "drift", 0.15f }, { "size", 700 }, { "density", 9 },
                           { "shape", 0.1f }, { "jitter", 2 }, { "spread", 0.6f }, { "attack", 0.6f }, { "release", 5.0f },
                           { "tone", 0.9f }, { "reverb", 0.4f }, { "decay", 12 }, { "volume", -4.5f } } },
        { "Shimmer Veil", { { "source", 2 }, { "position", 0.5f }, { "scan", 0.05f }, { "spray", 0.3f }, { "drift", 0.4f }, { "size", 380 },
                            { "density", 30 }, { "octaves", 0.45f }, { "jitter", 7 }, { "spread", 1.0f }, { "attack", 2.5f },
                            { "release", 6.0f }, { "tone", 0.88f }, { "reverb", 0.55f }, { "decay", 14 }, { "volume", -6.5f } } },
        { "Granite", { { "source", 1 }, { "position", 0.5f }, { "scan", 0.25f }, { "spray", 0.6f }, { "size", 22 }, { "density", 90 },
                       { "shape", 0.75f }, { "jitter", 25 }, { "spread", 1.0f }, { "attack", 0.05f }, { "release", 1.0f },
                       { "tone", 0.65f }, { "reverb", 0.25f }, { "decay", 3 }, { "volume", -8.5f } } },
        { "Reverse Halo", { { "source", 0 }, { "position", 0.4f }, { "spray", 0.25f }, { "size", 450 }, { "density", 16 }, { "reverse", 0.7f },
                            { "jitter", 4 }, { "spread", 0.9f }, { "attack", 1.0f }, { "release", 4.0f }, { "reverb", 0.5f },
                            { "decay", 10 }, { "volume", -4.5f } } },
        { "Underwater", { { "source", 2 }, { "transpose", -12 }, { "position", 0.6f }, { "spray", 0.3f }, { "drift", 0.7f }, { "size", 600 },
                          { "density", 12 }, { "jitter", 15 }, { "tone", 0.45f }, { "reverb", 0.55f }, { "decay", 12 },
                          { "attack", 1.5f }, { "release", 4.0f }, { "volume", -5.5f } } },
        { "Glass Rain", { { "source", 4 }, { "position", 0.5f }, { "scan", 0.3f }, { "spray", 0.9f }, { "size", 45 }, { "density", 40 },
                          { "shape", 0.5f }, { "jitter", 10 }, { "octaves", 0.25f }, { "spread", 1.0f }, { "attack", 0.01f },
                          { "release", 2.5f }, { "reverb", 0.5f }, { "decay", 9 }, { "tone", 0.9f }, { "volume", -6.5f } } },
        { "Long Exhale", { { "source", 3 }, { "position", 0.3f }, { "scan", 0.03f }, { "spray", 0.25f }, { "size", 900 }, { "density", 10 },
                           { "drift", 0.5f }, { "attack", 4.0f }, { "release", 8.0f }, { "reverb", 0.6f }, { "decay", 16 }, { "volume", -4.5f } } },
        { "Choir Swell", { { "source", 1 }, { "octaves", 0.3f }, { "size", 250 }, { "density", 30 }, { "attack", 2.0f }, { "release", 5.0f },
                           { "reverb", 0.5f }, { "decay", 12 }, { "spread", 1.0f }, { "spray", 0.25f }, { "jitter", 9 }, { "volume", -6.0f } } },
    };
    return list;
}
} // namespace

const char* const PollenProcessor::ids[numParams] { "source", "position", "scan", "spray", "drift", "size", "density", "shape", "transpose", "jitter",
                                                    "octaves", "reverse", "spread", "attack", "release", "tone", "reverb", "decay", "volume" };

juce::StringArray PollenProcessor::sourceNames()
{
    juce::StringArray s;
    for (int i = 0; i < pl::numBuiltins; ++i) s.add (pl::builtinName (i));
    s.add ("Sample");
    return s;
}

PollenProcessor::PollenProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formats.registerBasicFormats();
    for (int i = 0; i < numParams; ++i)
        ptr[(size_t) i] = apvts.getRawParameterValue (ids[i]);
    pl::builtins();   // synthesise the built-in sources now rather than on the audio thread
    startTimerHz (4);
}

PollenProcessor::~PollenProcessor()
{
    stopTimer();
    loader.removeAllJobs (true, 10000);
}

juce::AudioProcessorValueTreeState::ParameterLayout PollenProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto fmt = [] (std::function<String (float)> f) { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
    auto pct = fmt ([] (float v) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto bip = fmt ([] (float v) { return std::abs (v) < 0.005f ? String ("0 %") : (v > 0 ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; });
    auto sec = fmt ([] (float v) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, v < 10 ? 2 : 1) + " s"; });
    auto ms  = fmt ([] (float v) { return v < 1000.0f ? String (roundToInt (v)) + " ms" : String (v / 1000.0f, 2) + " s"; });
    auto per = fmt ([] (float v) { return String (v, v < 10 ? 1 : 0) + " /s"; });
    auto st  = fmt ([] (float v) { return (v > 0.5f ? "+" : "") + String (roundToInt (v)) + " st"; });
    auto ct  = fmt ([] (float v) { return String (roundToInt (v)) + " ct"; });
    auto db  = fmt ([] (float v) { return String (v, 1) + " dB"; });
    auto hz  = fmt ([] (float v) { const float f = 250.0f * std::pow (80.0f, v); return f >= 1000.0f ? String (f / 1000.0f, 1) + " kHz" : String (roundToInt (f)) + " Hz"; });
    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };
    auto add = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a));
    };

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 }, "Source", sourceNames(), 0));
    add ("position",  "Position",  { 0.0f, 1.0f }, 0.35f, pct);
    add ("scan",      "Scan",      { -2.0f, 2.0f, 0.0f, 0.4f, true }, 0.0f, bip);
    add ("spray",     "Spray",     { 0.0f, 1.0f, 0.0f, 0.5f }, 0.12f, pct);
    add ("drift",     "Drift",     { 0.0f, 1.0f }, 0.3f, pct);
    add ("size",      "Size",      skewed (5.0f, 2000.0f, 150.0f), 160.0f, ms);
    add ("density",   "Density",   skewed (0.5f, 120.0f, 15.0f), 18.0f, per);
    add ("shape",     "Shape",     { 0.0f, 1.0f }, 0.25f, pct);
    add ("transpose", "Transpose", { -24.0f, 24.0f, 1.0f }, 0.0f, st);
    add ("jitter",    "Jitter",    { 0.0f, 100.0f, 0.0f, 0.5f }, 6.0f, ct);
    add ("octaves",   "Octaves",   { 0.0f, 1.0f }, 0.0f, pct);
    add ("reverse",   "Reverse",   { 0.0f, 1.0f }, 0.0f, pct);
    add ("spread",    "Spread",    { 0.0f, 1.0f }, 0.7f, pct);
    add ("attack",    "Attack",    skewed (0.001f, 10.0f, 0.5f), 0.25f, sec);
    add ("release",   "Release",   skewed (0.02f, 20.0f, 2.0f), 1.8f, sec);
    add ("tone",      "Tone",      { 0.0f, 1.0f }, 0.8f, hz);
    add ("reverb",    "Reverb",    { 0.0f, 1.0f }, 0.35f, pct);
    add ("decay",     "Decay",     skewed (0.5f, 30.0f, 5.0f), 6.0f, sec);
    add ("volume",    "Volume",    { -24.0f, 12.0f, 0.1f }, -5.0f, db);
    return l;
}

pl::Settings PollenProcessor::readSettings() const
{
    pl::Settings s;
    s.source = (int) P (p_source);
    s.position = P (p_position); s.scan = P (p_scan); s.spray = P (p_spray); s.drift = P (p_drift);
    s.sizeMs = P (p_size); s.density = P (p_density); s.shape = P (p_shape);
    s.transpose = P (p_transpose); s.jitter = P (p_jitter); s.octaves = P (p_octaves); s.reverse = P (p_reverse);
    s.spread = P (p_spread); s.attack = P (p_attack); s.release = P (p_release);
    s.tone = P (p_tone); s.reverbMix = P (p_reverb); s.reverbDecay = P (p_decay);
    s.volume = juce::Decibels::decibelsToGain (P (p_volume));
    return s;
}

bool PollenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PollenProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setSettings (readSettings());
    engine.prepare (sampleRate);
    keyboardState.reset();
}

void PollenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    {
        const juce::SpinLock::ScopedTryLockType lock (handoff);
        if (lock.isLocked())
        {
            if (pending != nullptr && retired == nullptr)
            {
                retired = std::move (activeUser);
                activeUser = std::move (pending);
            }
            retiredFree = retired != nullptr && ! engine.uses (retired.get());
        }
    }
    engine.setUserSource (activeUser.get());
    engine.setSettings (readSettings());

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
bool PollenProcessor::isAudioFile (const juce::String& path) const
{
    return formats.findFormatForFileExtension (juce::File (path).getFileExtension()) != nullptr;
}

void PollenProcessor::setStatus (const juce::String& s)
{
    const juce::ScopedLock sl (fileLock);
    status = s;
    ++statusVersion;
}

juce::String PollenProcessor::getStatus (int& version) const
{
    const juce::ScopedLock sl (fileLock);
    version = statusVersion;
    return status;
}

void PollenProcessor::selectSampleSource()
{
    if (auto* p = apvts.getParameter ("source"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) userSourceIndex()));
        p->endChangeGesture();
    }
}

void PollenProcessor::timerCallback()
{
    collectGarbage();
    if (selectPending.exchange (false))
        selectSampleSource();
}

void PollenProcessor::loadSampleAsync (const juce::File& file, bool select)
{
    loader.removeAllJobs (true, 5000);   // cancels a load still in progress
    loading = true;
    setStatus ("Loading " + file.getFileName() + " ...");
    loader.addJob ([this, file, select]
    {
        auto* job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
        const auto err = loadSample (file, select, [job] { return job != nullptr && job->shouldExit(); });
        if (err != "cancelled")
            loading = false;
    });
}

juce::String PollenProcessor::loadSample (const juce::File& file, bool select, std::function<bool()> shouldAbort)
{
    collectGarbage();   // free the slot for the sample this one replaces, if no grain needs it any more
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        const juce::String err = file.existsAsFile() ? "Unsupported audio file" : "File not found: " + file.getFileName();
        {
            const juce::ScopedLock sl (fileLock);
            if (! file.existsAsFile()) { sampleFile = file; sampleMissing = true; }
        }
        ++userVersion;
        setStatus (err);
        return err;
    }

    const double fileRate = reader->sampleRate;
    const auto maxFrames = (juce::int64) (fileRate * maxSeconds);
    const juce::int64 total = juce::jmin (reader->lengthInSamples, maxFrames);
    if (total < 256 || fileRate < 8000.0)
    {
        setStatus ("The file is too short");
        return "The file is too short";
    }

    // read in chunks, mix to mono and halve rates above 50 kHz on the fly (keeps memory at 2 x 16 bit)
    int halvings = 0;
    while (fileRate / (1 << halvings) > 50000.0 && halvings < 2) ++halvings;
    std::vector<pl::StreamDecimator> dec ((size_t) halvings);
    std::vector<float> mono;
    mono.reserve ((size_t) (total >> halvings) + 64);
    auto feed = [&] (std::vector<float> data, bool final)
    {
        for (auto& d : dec)
        {
            std::vector<float> o;
            d.push (data.data(), (int) data.size(), o);
            if (final) d.finish (o);
            data = std::move (o);
        }
        mono.insert (mono.end(), data.begin(), data.end());
    };

    constexpr int chunk = 1 << 16;
    const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);
    juce::AudioBuffer<float> buf (chans, chunk);
    for (juce::int64 pos = 0; pos < total; pos += chunk)
    {
        if (shouldAbort && shouldAbort())
            return "cancelled";
        const int n = (int) juce::jmin ((juce::int64) chunk, total - pos);
        reader->read (&buf, 0, n, pos, true, chans > 1);
        std::vector<float> c ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            float v = 0.0f;
            for (int ch = 0; ch < chans; ++ch) v += buf.getSample (ch, i);
            c[(size_t) i] = v / (float) chans;
        }
        feed (std::move (c), false);
    }
    feed ({}, true);

    double mean = 0.0;
    for (auto v : mono) mean += v;
    mean /= (double) mono.size();
    for (auto& v : mono) v -= (float) mean;   // DC would turn every grain window into a thump

    if (shouldAbort && shouldAbort())
        return "cancelled";
    auto src = pl::makeSource (mono, fileRate / (1 << halvings));
    const double seconds = src->length / src->sampleRate;
    auto overview = src->overview;
    std::unique_ptr<pl::Source> old;
    {
        const juce::SpinLock::ScopedLockType lock (handoff);
        old = std::move (pending);   // never seen by the audio thread, safe to drop here
        pending = std::move (src);
    }
    old.reset();
    {
        const juce::ScopedLock sl (fileLock);
        sampleFile = file;
        sampleMissing = false;
        userOverview = std::move (overview);
    }
    ++userVersion;

    const int secs = (int) std::lround (seconds);
    juce::String msg = "Loaded " + file.getFileName() + "  (" + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2);
    msg += reader->lengthInSamples > maxFrames ? ", first " + juce::String ((int) (maxSeconds / 60)) + " min)" : ")";
    setStatus (msg);

    if (select)
    {
        if (juce::MessageManager::existsAndIsCurrentThread()) selectSampleSource();
        else selectPending = true;
    }
    return {};
}

void PollenProcessor::collectGarbage()
{
    std::unique_ptr<pl::Source> dead;
    {
        const juce::SpinLock::ScopedLockType lock (handoff);
        if (retired != nullptr && retiredFree)
        {
            dead = std::move (retired);
            retiredFree = false;
        }
    }
}

juce::File PollenProcessor::getSampleFile() const { const juce::ScopedLock sl (fileLock); return sampleFile; }
bool PollenProcessor::isSampleMissing() const { const juce::ScopedLock sl (fileLock); return sampleMissing; }

std::vector<float> PollenProcessor::overviewFor (int source) const
{
    if (source >= 0 && source < pl::numBuiltins)
        return pl::builtins().src[(size_t) source]->overview;
    const juce::ScopedLock sl (fileLock);
    return userOverview;
}

//==============================================================================
int PollenProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String PollenProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) presets().size()) ? juce::String (presets()[(size_t) i].name) : juce::String();
}

void PollenProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    const auto& pr = presets()[(size_t) index];
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            float value = rp->getDefaultValue();
            for (const auto& [pid, v] : pr.values)
                if (id == pid) value = rp->convertTo0to1 (v);
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (value);
            rp->endChangeGesture();
        }
}

void PollenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("sample", getSampleFile().getFullPathName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PollenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            const auto path = tree.getProperty ("sample", "").toString();
            tree.removeProperty ("sample", nullptr);
            apvts.replaceState (tree);
            if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path) != getSampleFile())
                loadSampleAsync (juce::File (path), false);
        }
}

juce::AudioProcessorEditor* PollenProcessor::createEditor() { return new PollenEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PollenProcessor(); }
