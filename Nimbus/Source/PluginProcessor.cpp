#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray NimbusProcessor::characterChoices { "Room", "Hall", "Plate", "Cathedral", "Ambient" };
const juce::StringArray NimbusProcessor::shimmerChoices   { "+12 Octave", "+7 Fifth", "+19 Oct + Fifth", "+24 Two Octaves", "-12 Octave Down", "+5 Fourth" };

NimbusProcessor::NimbusProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout NimbusProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto fmt = [] (std::function<String (float)> f)
    {
        return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); });
    };
    auto ms  = fmt ([] (float v) { return String (roundToInt (v)) + " ms"; });
    auto sec = fmt ([] (float v) { return v < 10.0f ? String (v, 2) + " s" : String (v, 1) + " s"; });
    auto pct = fmt ([] (float v) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto hz  = fmt ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto mul = fmt ([] (float v) { return String (v, 2) + "x"; });
    auto rate = fmt ([] (float v) { return String (v, 2) + " Hz"; });

    auto add = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "character", 1 }, "Character", characterChoices, 1));
    add ("predelay",   "Predelay",       NormalisableRange<float> (0.0f, 250.0f, 0.0f, 0.5f), 20.0f, ms);
    add ("size",       "Size",           NormalisableRange<float> (0.0f, 1.0f), 0.55f, pct);
    add ("decay",      "Decay",          skewed (0.2f, 30.0f, 3.0f), 2.8f, sec);
    add ("diffusion",  "Diffusion",      NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct);
    add ("bloom",      "Bloom",          NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct);
    add ("early",      "Early",          NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct);
    add ("bassMult",   "Bass Decay",     skewed (0.5f, 2.0f, 1.0f), 1.2f, mul);
    add ("dampHz",     "Damping Freq",   skewed (1000.0f, 16000.0f, 4000.0f), 5000.0f, hz);
    add ("trebleMult", "Treble Decay",   skewed (0.1f, 1.0f, 0.4f), 0.45f, mul);
    add ("modDepth",   "Mod Depth",      NormalisableRange<float> (0.0f, 1.0f), 0.45f, pct);
    add ("modRate",    "Mod Rate",       skewed (0.05f, 4.0f, 0.6f), 0.6f, rate);
    add ("lowCut",     "Low Cut",        skewed (20.0f, 1000.0f, 150.0f), 120.0f, hz);
    add ("highCut",    "High Cut",       skewed (1000.0f, 20000.0f, 6000.0f), 12000.0f, hz);
    add ("width",      "Width",          NormalisableRange<float> (0.0f, 2.0f), 1.0f, pct);
    add ("duck",       "Ducking",        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    add ("mix",        "Mix",            NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
    add ("shimmer",    "Shimmer",        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "shimmerPitch", 1 }, "Shimmer Pitch", shimmerChoices, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze", false));
    return l;
}

ReverbEngine::Params NimbusProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    ReverbEngine::Params p;
    p.character  = (int) v ("character");
    p.predelayMs = v ("predelay");
    p.size       = v ("size");
    p.decayS     = v ("decay");
    p.diffusion  = v ("diffusion");
    p.bloom      = v ("bloom");
    p.early      = v ("early");
    p.bassMult   = v ("bassMult");
    p.dampHz     = v ("dampHz");
    p.trebleMult = v ("trebleMult");
    p.modDepth   = v ("modDepth");
    p.modRate    = v ("modRate");
    p.lowCutHz   = v ("lowCut");
    p.highCutHz  = v ("highCut");
    p.width      = v ("width");
    p.duck       = v ("duck");
    p.mix        = v ("mix");
    p.shimmer    = v ("shimmer");
    p.shimmerPitch = (int) v ("shimmerPitch");
    p.freeze     = v ("freeze") > 0.5f;
    return p;
}

bool NimbusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void NimbusProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate);
    monoR.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
}

void NimbusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();

    // mono in -> stereo out: duplicate the input
    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int c = juce::jmax (numIn, numIn == 1 && numOut > 1 ? 2 : numIn); c < numOut; ++c)
        buffer.clear (c, 0, n);

    engine.setParams (readParams());

    float* L = buffer.getWritePointer (0);
    if (buffer.getNumChannels() > 1)
    {
        engine.process (L, buffer.getWritePointer (1), n);
    }
    else
    {
        if ((int) monoR.size() < n) monoR.resize ((size_t) n); // only if the host breaks its block size promise
        std::copy (L, L + n, monoR.begin());
        engine.process (L, monoR.data(), n);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + monoR[(size_t) i]);
    }

    outLevel = juce::jmax (outLevel.load() * 0.8f, buffer.getMagnitude (0, n));
}

void NimbusProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NimbusProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* NimbusProcessor::createEditor()
{
    return new NimbusEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NimbusProcessor();
}
