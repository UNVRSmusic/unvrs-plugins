#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray S2KProcessor::rateChoices   { "44.1 kHz", "32 kHz", "22.05 kHz", "11.025 kHz" };
const juce::StringArray S2KProcessor::interpChoices { "Drop", "Linear", "Hermite" };

static constexpr int rateValues[] { 44100, 32000, 22050, 11025 };

S2KProcessor::S2KProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pRate   = apvts.getRawParameterValue ("rate");
    pBits   = apvts.getRawParameterValue ("bits");
    pInterp = apvts.getRawParameterValue ("interp");
    pAA     = apvts.getRawParameterValue ("aa");
    pDAC    = apvts.getRawParameterValue ("dacfilter");
    pCutoff = apvts.getRawParameterValue ("cutoff");
    pReso   = apvts.getRawParameterValue ("reso");
    pDrive  = apvts.getRawParameterValue ("drive");
    pNoise  = apvts.getRawParameterValue ("noise");
    pIn     = apvts.getRawParameterValue ("input");
    pOut    = apvts.getRawParameterValue ("output");
    pMix    = apvts.getRawParameterValue ("mix");
}

juce::AudioProcessorValueTreeState::ParameterLayout S2KProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; });
    auto db  = AudioParameterFloatAttributes().withLabel ("dB");
    auto hz  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (juce::roundToInt (v)) + " Hz"; });

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Sample Rate", rateChoices, 0));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "bits", 1 }, "ADC Bits", 4, 16, 16));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "interp", 1 }, "Interpolation", interpChoices, 1));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "aa", 1 }, "Anti-Alias Filter", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "dacfilter", 1 }, "DAC Filter", true));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "cutoff", 1 }, "Filter Cutoff",
                                                  NormalisableRange<float> (100.0f, 20000.0f, 0.0f, 0.25f), 20000.0f, hz));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "reso", 1 }, "Filter Resonance",
                                                  NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Analog Drive",
                                                  NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "noise", 1 }, "Noise Floor",
                                                  NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "input", 1 }, "Input Gain",
                                                  NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output Gain",
                                                  NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                                                  NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    return l;
}

bool S2KProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

ConverterEngine::Settings S2KProcessor::readSettings() const
{
    ConverterEngine::Settings s;
    s.recRate   = rateValues[juce::jlimit (0, 3, (int) pRate->load())];
    s.adcBits   = juce::jlimit (4, 16, (int) pBits->load());
    s.interp    = (ConverterEngine::Interp) juce::jlimit (0, 2, (int) pInterp->load());
    s.aaFilter  = pAA->load() > 0.5f;
    s.dacFilter = pDAC->load() > 0.5f;
    s.cutoffHz  = pCutoff->load();
    s.resonance = pReso->load();
    s.drive     = pDrive->load();
    s.noise     = pNoise->load();
    return s;
}

void S2KProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Run the converter model at >= ~176 kHz so the 44.1 kHz domains can be simulated cleanly.
    size_t osLog2 = sampleRate <= 50000.0 ? 2 : (sampleRate <= 100000.0 ? 1 : 0);
    const int osFactor = 1 << osLog2;
    const int numCh = juce::jmax (1, getTotalNumOutputChannels());
    preparedChannels = numCh;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, osLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) samplesPerBlock);

    engine.prepare (juce::roundToInt (sampleRate) * osFactor, osFactor, numCh);
    engine.updateSettings (readSettings());

    const int latency = juce::roundToInt (oversampler->getLatencyInSamples())
                      + engine.getLatencyInOversampledSamples() / osFactor;
    setLatencySamples (latency);

    dryWet.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    dryWet.setWetLatency ((float) latency);

    inGain.reset (sampleRate, 0.03);
    outGain.reset (sampleRate, 0.03);
    inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pIn->load()));
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
}

void S2KProcessor::reset()
{
    if (oversampler != nullptr) oversampler->reset();
    engine.reset();
    dryWet.reset();
}

void S2KProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    for (int c = numIn; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> block (buffer);
    block = block.getSubsetChannelBlock (0, (size_t) juce::jmin ((int) block.getNumChannels(), preparedChannels));

    dryWet.setWetMixProportion (pMix->load());
    dryWet.pushDrySamples (block);

    inGain.setTargetValue (juce::Decibels::decibelsToGain (pIn->load()));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    // input gain
    for (int i = 0; i < (int) block.getNumSamples(); ++i)
    {
        const float g = inGain.getNextValue();
        for (size_t c = 0; c < block.getNumChannels(); ++c)
            block.getChannelPointer (c)[i] *= g;
    }

    engine.updateSettings (readSettings());

    auto osBlock = oversampler->processSamplesUp (block);
    engine.process (osBlock);
    oversampler->processSamplesDown (block);

    if (engine.getAndClearClip())
        clipIndicator = true;

    // output gain
    for (int i = 0; i < (int) block.getNumSamples(); ++i)
    {
        const float g = outGain.getNextValue();
        for (size_t c = 0; c < block.getNumChannels(); ++c)
            block.getChannelPointer (c)[i] *= g;
    }

    dryWet.mixWetSamples (block);
}

void S2KProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void S2KProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* S2KProcessor::createEditor()
{
    return new S2KEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new S2KProcessor();
}
