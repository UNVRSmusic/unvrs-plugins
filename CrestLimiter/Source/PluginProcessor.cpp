#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray CrestProcessor::styleChoices  { "Transparent", "Punchy", "Dense", "Safe" };
const juce::StringArray CrestProcessor::osChoices     { "Off", "2x", "4x", "8x" };
const juce::StringArray CrestProcessor::ditherChoices { "Off", "24 bit", "20 bit", "16 bit" };

static constexpr double maxLookaheadSeconds = 0.005;

CrestProcessor::CrestProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pGain    = apvts.getRawParameterValue ("gain");
    pCeiling = apvts.getRawParameterValue ("ceiling");
    pLook    = apvts.getRawParameterValue ("lookahead");
    pRelease = apvts.getRawParameterValue ("release");
    pLink    = apvts.getRawParameterValue ("link");
    pStyle   = apvts.getRawParameterValue ("style");
    pOs      = apvts.getRawParameterValue ("oversampling");
    pDither  = apvts.getRawParameterValue ("dither");
    pUnity   = apvts.getRawParameterValue ("unity");
}

juce::AudioProcessorValueTreeState::ParameterLayout CrestProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto db  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   { return (v > 0.0f ? "+" : "") + String (v, 2) + " dB"; });
    auto ms  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   { return v < 10.0f ? String (v, 2) + " ms" : String (roundToInt (v)) + " ms"; });
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   { return String (roundToInt (v * 100.0f)) + " %"; });

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Gain",
                                                  NormalisableRange<float> (0.0f, 30.0f, 0.01f), 0.0f, db));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Ceiling",
                                                  NormalisableRange<float> (-20.0f, 0.0f, 0.01f, 2.0f), -1.0f, db));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "lookahead", 1 }, "Lookahead",
                                                  NormalisableRange<float> (0.1f, 5.0f, 0.01f, 0.5f), 1.5f, ms));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release",
                                                  NormalisableRange<float> (1.0f, 1000.0f, 0.1f, 0.3f), 150.0f, ms));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "link", 1 }, "Stereo Link",
                                                  NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", styleChoices, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "oversampling", 1 }, "Oversampling", osChoices, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "dither", 1 }, "Dither", ditherChoices, 0));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "unity", 1 }, "Unity Gain Listen", false));
    return l;
}

bool CrestProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CrestProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (1, samplesPerBlock);
    preparedChannels = juce::jlimit (1, 2, getTotalNumOutputChannels());
    const auto nch = (size_t) preparedChannels;

    const int hostLook = (int) std::ceil (maxLookaheadSeconds * sampleRate);

    maxLatency = 0;
    for (int k = 0; k < numOs; ++k)
    {
        const int factor = 1 << k;
        int osLatency = 0;

        if (k > 0)
        {
            oversamplers[(size_t) k] = std::make_unique<juce::dsp::Oversampling<float>> (
                nch, (size_t) k, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            oversamplers[(size_t) k]->initProcessing ((size_t) maxBlock);
            osLatency = juce::roundToInt (oversamplers[(size_t) k]->getLatencyInSamples());
        }

        engines[(size_t) k].prepare (sampleRate * factor, hostLook * factor);
        latencies[(size_t) k] = osLatency + hostLook;
        maxLatency = juce::jmax (maxLatency, latencies[(size_t) k]);
    }

    // every oversampling mode is padded to the same latency, so switching never shifts the track
    setLatencySamples (maxLatency);
    padBuf.setSize ((int) nch, maxLatency + 1);
    padBuf.clear();
    padPos = 0;

    meterOs = std::make_unique<juce::dsp::Oversampling<float>> (
        nch, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, false, false);
    meterOs->initProcessing ((size_t) maxBlock);
    meterBuf.setSize ((int) nch, maxBlock);

    grBuf.assign ((size_t) maxBlock, 0.0f);
    gainBuf.assign ((size_t) maxBlock, 1.0f);
    inPkBuf.assign ((size_t) maxBlock, { 0.0f, 0.0f });

    loudness.prepare (sampleRate);

    inGain.reset (sampleRate, 0.02);
    inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGain->load()));

    chunkLen = juce::jmax (1, juce::roundToInt (sampleRate / histPointsPerSecond));
    chunkCount = 0;
    chunkAcc = { { 0, 0 }, { 0, 0 }, 0 };

    osIndex = -1;
}

void CrestProcessor::reset()
{
    for (auto& o : oversamplers)
        if (o != nullptr) o->reset();
    for (auto& e : engines)
        e.reset();
    if (meterOs != nullptr)
        meterOs->reset();
    padBuf.clear();
    loudness.resetState();
}

void CrestProcessor::resetMeters()
{
    loudness.resetIntegrated();
    truePeakMax = 0.0f;
}

void CrestProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    // hosts may send blocks larger than announced; the oversamplers are sized for maxBlock
    for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, buffer.getNumSamples() - start));
}

void CrestProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int len)
{
    const int nch = juce::jmin (buffer.getNumChannels(), preparedChannels);
    if (nch <= 0 || len <= 0)
        return;

    auto block = juce::dsp::AudioBlock<float> (buffer)
                     .getSubsetChannelBlock (0, (size_t) nch)
                     .getSubBlock ((size_t) start, (size_t) len);

    float* d[2] { block.getChannelPointer (0), nch > 1 ? block.getChannelPointer (1) : nullptr };

    // ---- settings ---------------------------------------------------------------
    const int k = juce::jlimit (0, numOs - 1, (int) pOs->load());
    if (k != osIndex)
    {
        osIndex = k;
        engines[(size_t) k].reset();
        if (oversamplers[(size_t) k] != nullptr)
            oversamplers[(size_t) k]->reset();
        padBuf.clear();
    }

    LimiterEngine::Settings s;
    s.ceilingDb   = pCeiling->load();
    s.lookaheadMs = pLook->load();
    s.releaseMs   = pRelease->load();
    s.style       = (int) pStyle->load();
    s.link        = pLink->load();
    engines[(size_t) k].updateSettings (s);
    const float ceilingLin = juce::Decibels::decibelsToGain ((float) s.ceilingDb);

    // ---- input gain + input metering ---------------------------------------------
    inGain.setTargetValue (juce::Decibels::decibelsToGain (pGain->load()));
    for (int i = 0; i < len; ++i)
    {
        const float g = inGain.getNextValue();
        gainBuf[(size_t) i] = g;
        for (int c = 0; c < nch; ++c)
        {
            d[c][i] *= g;
            inPkBuf[(size_t) i][(size_t) c] = std::abs (d[c][i]);
        }
    }

    // ---- limiter (optionally oversampled) -------------------------------------------
    std::fill (grBuf.begin(), grBuf.begin() + len, 0.0f);
    if (k == 0)
    {
        engines[0].process (block, grBuf.data(), 1);
    }
    else
    {
        auto& os = *oversamplers[(size_t) k];
        auto up = os.processSamplesUp (block);
        engines[(size_t) k].process (up, grBuf.data(), 1 << k);
        os.processSamplesDown (block);
    }

    // ---- latency padding --------------------------------------------------------------
    const int padDelay = maxLatency - latencies[(size_t) k];
    if (padDelay > 0)
    {
        const int cap = padBuf.getNumSamples();
        for (int i = 0; i < len; ++i)
        {
            const int rd = (padPos - padDelay + cap) % cap;
            for (int c = 0; c < nch; ++c)
            {
                auto* p = padBuf.getWritePointer (c);
                p[padPos] = d[c][i];
                d[c][i] = p[rd];
            }
            padPos = (padPos + 1) % cap;
        }
    }

    // ---- safety clip: catches the tiny overshoot of the downsampling filter ---------
    for (int c = 0; c < nch; ++c)
        juce::FloatVectorOperations::clip (d[c], d[c], -ceilingLin, ceilingLin, len);

    // ---- output metering ----------------------------------------------------------------
    {
        auto mb = juce::dsp::AudioBlock<float> (meterBuf).getSubsetChannelBlock (0, (size_t) nch)
                                                        .getSubBlock (0, (size_t) len);
        mb.copyFrom (block);
        auto up = meterOs->processSamplesUp (mb);
        float tp = 0.0f;
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            const auto r = juce::FloatVectorOperations::findMinAndMax (up.getChannelPointer (c), (int) up.getNumSamples());
            tp = juce::jmax (tp, std::abs (r.getStart()), std::abs (r.getEnd()));
        }
        if (tp > truePeakMax.load())
            truePeakMax = tp;

        loudness.process (d, nch, len);
    }

    for (int i = 0; i < len; ++i)
    {
        for (int c = 0; c < 2; ++c)
        {
            const int src = juce::jmin (c, nch - 1);
            chunkAcc.inPk[c]  = juce::jmax (chunkAcc.inPk[c], inPkBuf[(size_t) i][(size_t) src]);
            chunkAcc.outPk[c] = juce::jmax (chunkAcc.outPk[c], std::abs (d[src][i]));
        }
        chunkAcc.grDb = juce::jmin (chunkAcc.grDb, grBuf[(size_t) i]);

        if (++chunkCount >= chunkLen)
        {
            const auto scope = histFifo.write (1);
            if (scope.blockSize1 > 0)
                histData[(size_t) scope.startIndex1] = chunkAcc;
            chunkAcc = { { 0, 0 }, { 0, 0 }, 0 };
            chunkCount = 0;
        }
    }

    // ---- unity-gain listening: removes the input gain so you hear only the "sound" ---
    if (pUnity->load() > 0.5f)
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < len; ++i)
                d[c][i] /= gainBuf[(size_t) i];

    // ---- TPDF dither ----------------------------------------------------------------------
    static constexpr int ditherBits[] { 0, 24, 20, 16 };
    if (const int bits = ditherBits[juce::jlimit (0, 3, (int) pDither->load())]; bits > 0)
    {
        const float scale = (float) (1 << (bits - 1));
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < len; ++i)
            {
                const float tpdf = rng.nextFloat() - rng.nextFloat();
                d[c][i] = std::round (d[c][i] * scale + tpdf) / scale;
            }
    }
}

void CrestProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CrestProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CrestProcessor::createEditor()
{
    return new CrestEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrestProcessor();
}
