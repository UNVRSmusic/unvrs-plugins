#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray PoiseProcessor::targetChoices { "Pink", "Mix", "Warm", "Bright", "Learned" };
const juce::StringArray PoiseProcessor::modeChoices   { "Cut + Boost", "Cut Only", "Boost Only" };

PoiseProcessor::PoiseProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PoiseProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto fmt = [] (std::function<String (float)> f)
    { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
    auto pct  = fmt ([] (float v) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto sec  = fmt ([] (float v) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; });
    auto oct  = fmt ([] (float v) { return String (v, 2) + " oct"; });
    auto db   = fmt ([] (float v) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    auto rng  = fmt ([] (float v) { return String::charToString (0xb1) + String (v, 1) + " dB"; });
    auto tilt = fmt ([] (float v) { return (v > 0.005f ? "+" : "") + String (v, 2) + " dB/oct"; });
    auto hz   = fmt ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });

    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };
    auto add = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a)); };

    add ("amount",     "Amount",          NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    add ("keep",       "Keep Character",  NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct);
    add ("speed",      "Speed",           skewed (0.05f, 3.0f, 0.6f), 0.6f, sec);
    add ("resolution", "Resolution",      skewed (0.33f, 2.0f, 1.0f), 1.0f, oct);
    add ("range",      "Range",           NormalisableRange<float> (1.0f, 18.0f, 0.1f), 6.0f, rng);
    add ("tilt",       "Tilt",            NormalisableRange<float> (-3.0f, 3.0f, 0.01f), 0.0f, tilt);
    add ("lowLimit",   "Low Limit",       skewed (20.0f, 500.0f, 80.0f), 30.0f, hz);
    add ("highLimit",  "High Limit",      skewed (2000.0f, 20000.0f, 8000.0f), 16000.0f, hz);
    add ("mix",        "Mix",             NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct);
    add ("output",     "Output",          NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "target", 1 }, "Target", targetChoices, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modeChoices, 0));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "autoGain", 1 }, "Auto Gain", true));
    return l;
}

bool PoiseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void PoiseProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatency());
}

void PoiseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    RebalanceEngine::Params p;
    p.amount = v ("amount");
    p.keep = v ("keep");
    p.speed = v ("speed");
    p.resolution = v ("resolution");
    p.range = v ("range");
    p.tilt = v ("tilt");
    p.lowLimit = v ("lowLimit");
    p.highLimit = v ("highLimit");
    p.mix = v ("mix");
    p.outputDb = v ("output");
    p.target = (int) v ("target");
    p.mode = (int) v ("mode");
    p.autoGain = v ("autoGain") > 0.5f;
    engine.setParams (p);

    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (nch > 0)
        engine.process (buffer.getArrayOfWritePointers(), nch, buffer.getNumSamples());
}

void PoiseProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (engine.hasLearned.load())
    {
        juce::StringArray values;
        for (auto& v : engine.learnedCurve)
            values.add (juce::String (v.load(), 3));
        state.setProperty ("learned", values.joinIntoString (","), nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PoiseProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const auto learned = juce::StringArray::fromTokens (tree.getProperty ("learned").toString(), ",", {});
            if (learned.size() > 0)
            {
                for (int i = 0; i < (int) engine.learnedCurve.size(); ++i)
                    engine.learnedCurve[(size_t) i] = i < learned.size() ? learned[i].getFloatValue() : 0.0f;
                engine.hasLearned = true;
            }
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* PoiseProcessor::createEditor()
{
    return new PoiseEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PoiseProcessor();
}
