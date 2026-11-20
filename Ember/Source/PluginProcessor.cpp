#include "PluginProcessor.h"
#include "PluginEditor.h"

EmberProcessor::EmberProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pFreq  = apvts.getRawParameterValue ("freq");
    pWidth = apvts.getRawParameterValue ("width");
    pDrive = apvts.getRawParameterValue ("drive");
    pTone  = apvts.getRawParameterValue ("tone");
    pDyn   = apvts.getRawParameterValue ("dynamics");
    pMix   = apvts.getRawParameterValue ("mix");
    pOut   = apvts.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout EmberProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto fmt = [] (std::function<String (float)> f)
    { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
    auto pct = fmt ([] (float v) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto hz  = fmt ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto oct = fmt ([] (float v) { return String (v, 1) + " oct"; });
    auto db  = fmt ([] (float v) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; });

    NormalisableRange<float> freqRange (50.0f, 18000.0f);
    freqRange.setSkewForCentre (1000.0f);

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "freq", 1 },     "Filter Freq",  freqRange, 1000.0f, hz));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 },    "Filter Width", NormalisableRange<float> (0.5f, 9.0f), 6.5f, oct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 },    "Drive",        NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 },     "Tone",         NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "dynamics", 1 }, "Dynamics",     NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 },      "Dry/Wet",      NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 },   "Output",       NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db));
    return l;
}

bool EmberProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void EmberProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    const int latency = juce::roundToInt (oversampler->getLatencyInSamples());
    setLatencySamples (latency);

    dryWet.prepare ({ sampleRate, (juce::uint32) maxBlock, (juce::uint32) numCh });
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    dryWet.setWetLatency ((float) latency);

    envAtt  = 1.0f - std::exp (-1.0f / (0.003f * (float) fs));
    envRel  = 1.0f - std::exp (-1.0f / (0.080f * (float) fs));
    kSmooth = 1.0f - std::exp (-1.0f / (0.010f * (float) fs));
    dcCoef  = 1.0f - (float) (2.0 * juce::MathConstants<double>::pi * 10.0 / fs);

    const double osRate = fs * 4.0;
    driveSm.reset (osRate, 0.02);
    driveSm.setCurrentAndTargetValue (driveGain (pDrive->load()));
    dynSm.reset (fs, 0.02);
    dynSm.setCurrentAndTargetValue (pDyn->load());
    outSm.reset (fs, 0.02);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    envInBuf.assign ((size_t) maxBlock, 0.0f);
    reset();
}

void EmberProcessor::reset()
{
    if (oversampler != nullptr) oversampler->reset();
    dryWet.reset();
    hp = {}; lp = {}; tone = {};
    envIn = {}; envOut = {}; dcX = {}; dcY = {};
}

void EmberProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    auto block = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, (size_t) juce::jmin (numCh, buffer.getNumChannels()));
    for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
        processChunk (block.getSubBlock ((size_t) start, (size_t) juce::jmin (maxBlock, buffer.getNumSamples() - start)));
}

void EmberProcessor::processChunk (juce::dsp::AudioBlock<float> block)
{
    const int n = (int) block.getNumSamples();
    const int nch = (int) block.getNumChannels();

    dryWet.setWetMixProportion (pMix->load());
    dryWet.pushDrySamples (block);

    // ---- band-pass before the drive: HP at f / 2^(w/2), LP at f * 2^(w/2) ----------------------
    const double f = pFreq->load(), halfWidth = std::pow (2.0, pWidth->load() * 0.5);
    const float gHpT = gFor (f / halfWidth), gLpT = gFor (f * halfWidth);
    const float gToneT = gFor (500.0 * std::pow (44.0, (double) pTone->load()));

    for (int i = 0; i < n; ++i)
    {
        gHp += (gHpT - gHp) * kSmooth;
        gLp += (gLpT - gLp) * kSmooth;
        float env = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            float* d = block.getChannelPointer ((size_t) c);
            float x = svfProcess (hp[(size_t) c], d[i], gHp, true);
            x = svfProcess (lp[(size_t) c], x, gLp, false);
            d[i] = x;

            const float a = std::abs (x);
            auto& e = envIn[(size_t) c];
            e += (a - e) * (a > e ? envAtt : envRel);
            env = juce::jmax (env, e);
        }
        envInBuf[(size_t) i] = env;
    }

    // ---- drive at 4x ------------------------------------------------------------------------------
    driveSm.setTargetValue (driveGain (pDrive->load()));
    auto up = oversampler->processSamplesUp (block);
    const int un = (int) up.getNumSamples();
    const float off = clip (bias);
    float hard = 0.0f;
    for (int i = 0; i < un; ++i)
    {
        const float g = driveSm.getNextValue();
        for (int c = 0; c < nch; ++c)
        {
            float* d = up.getChannelPointer ((size_t) c);
            const float u = g * d[i] + bias;
            float y = clip (u) - off;                       // asymmetric soft clip -> even harmonics
            y = std::tanh (1.3f * y) * (1.0f / 0.8617f);    // second, rounder stage (tanh(1.3) = 0.8617)
            d[i] = y;
            hard = juce::jmax (hard, std::abs (u));
        }
    }
    oversampler->processSamplesDown (block);
    meterAcc = juce::jmax (meterAcc * 0.9f, juce::jlimit (0.0f, 1.0f, (hard - 0.5f) / 6.0f));
    driveMeter = meterAcc;

    // ---- DC block, dynamics, tone, output -------------------------------------------------------
    dynSm.setTargetValue (pDyn->load());
    outSm.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    constexpr float wetScale = 0.6f;

    for (int i = 0; i < n; ++i)
    {
        gTone += (gToneT - gTone) * kSmooth;
        const float dyn = dynSm.getNextValue();
        const float outG = outSm.getNextValue();

        float envO = 0.0f;
        float y[2] {};
        for (int c = 0; c < nch; ++c)
        {
            float* d = block.getChannelPointer ((size_t) c);
            const float x = d[i];
            float v = x - dcX[(size_t) c] + dcCoef * dcY[(size_t) c];
            dcX[(size_t) c] = x;
            dcY[(size_t) c] = v;
            y[c] = v;

            const float a = std::abs (v);
            auto& e = envOut[(size_t) c];
            e += (a - e) * (a > e ? envAtt : envRel);
            envO = juce::jmax (envO, e);
        }

        // Dynamics: 0 = keep the clipper's squashed, made-up level; 1 = follow the input envelope
        const float ratio = juce::jlimit (0.0f, 2.0f, envInBuf[(size_t) i] / juce::jmax (envO * wetScale, 1.0e-5f));
        const float dynGain = wetScale * std::pow (ratio, dyn);

        for (int c = 0; c < nch; ++c)
        {
            float v = svfProcess (tone[(size_t) c], y[c] * dynGain, gTone, false);
            block.getChannelPointer ((size_t) c)[i] = v * outG;
        }
    }

    dryWet.mixWetSamples (block);
}

void EmberProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EmberProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* EmberProcessor::createEditor()
{
    return new EmberEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EmberProcessor();
}
