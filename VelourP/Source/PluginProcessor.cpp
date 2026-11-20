#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace analogeq;

const juce::StringArray VelourPProcessor::typeChoices  { "Bell", "Low Shelf", "High Shelf" };
const juce::StringArray VelourPProcessor::slopeChoices { "12 dB", "24 dB" };

static constexpr double hpfOff = 10.0, lpfOff = 22000.0;
static constexpr double defaultFreq[] { 80, 250, 800, 2500, 6000, 12000 };
static constexpr double defaultQ[]    { 0.7, 1.0, 1.0, 1.0, 1.0, 0.7 };

VelourPProcessor::VelourPProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout VelourPProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto fmt = [] (std::function<String (float)> f)
    { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
    auto hz  = fmt ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto db  = fmt ([] (float v) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    auto q   = fmt ([] (float v) { return String (v, 2); });
    auto pct = fmt ([] (float v) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto hpf = fmt ([] (float v) { return v <= hpfOff + 0.01 ? String ("OFF") : String (roundToInt (v)) + " Hz"; });
    auto lpf = fmt ([] (float v) { return v >= lpfOff - 1.0 ? String ("OFF") : String (v / 1000.0f, 1) + " kHz"; });

    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    for (int b = 0; b < numBands; ++b)
    {
        const auto n = "Band " + String (b + 1) + " ";
        const int defType = b == 0 ? lowShelf : (b == numBands - 1 ? highShelf : bell);
        l.add (std::make_unique<AudioParameterBool>   (ParameterID { bandId (b, "On"), 1 },   n + "On", true));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { bandId (b, "Type"), 1 }, n + "Type", typeChoices, defType));
        l.add (std::make_unique<AudioParameterFloat>  (ParameterID { bandId (b, "Freq"), 1 }, n + "Freq", skewed (20.0f, 20000.0f, 1000.0f), (float) defaultFreq[b], hz));
        l.add (std::make_unique<AudioParameterFloat>  (ParameterID { bandId (b, "Gain"), 1 }, n + "Gain", NormalisableRange<float> (-18.0f, 18.0f, 0.01f), 0.0f, db));
        l.add (std::make_unique<AudioParameterFloat>  (ParameterID { bandId (b, "Q"), 1 },    n + "Q", skewed (0.1f, 10.0f, 1.0f), (float) defaultQ[b], q));
    }

    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "hpf", 1 }, "High-pass", skewed ((float) hpfOff, 1000.0f, 80.0f), (float) hpfOff, hpf));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "hpfSlope", 1 }, "High-pass Slope", slopeChoices, 0));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "lpf", 1 }, "Low-pass", skewed (1000.0f, (float) lpfOff, 8000.0f), (float) lpfOff, lpf));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "lpfSlope", 1 }, "Low-pass Slope", slopeChoices, 0));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "iron", 1 },   "Iron",   NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 },  "Drive",  NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "analog", 1 },   "Analog", true));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "consoleQ", 1 }, "Console Q", true));
    return l;
}

VelourPProcessor::Settings VelourPProcessor::readSettings() const
{
    auto v = [this] (const juce::String& id) { return (double) apvts.getRawParameterValue (id)->load(); };
    Settings s;
    for (int b = 0; b < numBands; ++b)
    {
        auto& bd = s.bands[(size_t) b];
        bd.on = v (bandId (b, "On")) > 0.5;
        bd.type = (int) v (bandId (b, "Type"));
        bd.freq = v (bandId (b, "Freq"));
        bd.gain = v (bandId (b, "Gain"));
        bd.q = v (bandId (b, "Q"));
    }
    s.hpf = v ("hpf") <= hpfOff + 0.01 ? 0.0 : v ("hpf");
    s.lpf = v ("lpf") >= lpfOff - 1.0 ? 0.0 : v ("lpf");
    s.hpf24 = v ("hpfSlope") > 0.5;
    s.lpf24 = v ("lpfSlope") > 0.5;
    s.consoleQ = v ("consoleQ") > 0.5;
    return s;
}

Coefs VelourPProcessor::designBand (const Band& b, bool consoleQ, double rate)
{
    const double gain = b.on ? b.gain : 0.0;
    if (std::abs (gain) < 0.005)
        return {};

    switch (b.type)
    {
        case lowShelf:  return analogeq::lowShelf  (rate, b.freq, juce::jlimit (0.3, 2.0, b.q), gain);
        case highShelf: return analogeq::highShelf (rate, b.freq, juce::jlimit (0.3, 2.0, b.q), gain);
        default:
        {
            // console behaviour: small moves are wider, big moves narrower (Q as set at 6 dB)
            const double q = consoleQ ? b.q * std::pow (2.0, (std::abs (gain) - 6.0) / 12.0) : b.q;
            return analogeq::peak (rate, b.freq, juce::jlimit (0.05, 20.0, q), gain);
        }
    }
}

std::array<Coefs, VelourPProcessor::numSlots> VelourPProcessor::design (const Settings& s, double rate)
{
    std::array<Coefs, numSlots> out {};
    // Butterworth: 2nd order Q 0.707, 4th order as Q 0.541 + Q 1.307
    if (s.hpf > 0.0)
    {
        out[0] = highPass (rate, s.hpf, s.hpf24 ? 0.5412 : 0.7071);
        if (s.hpf24) out[1] = highPass (rate, s.hpf, 1.3066);
    }
    for (int b = 0; b < numBands; ++b)
        out[(size_t) (2 + b)] = designBand (s.bands[(size_t) b], s.consoleQ, rate);
    if (s.lpf > 0.0)
    {
        out[2 + numBands] = lowPass (rate, s.lpf, s.lpf24 ? 0.5412 : 0.7071);
        if (s.lpf24) out[3 + numBands] = lowPass (rate, s.lpf, 1.3066);
    }
    return out;
}

//==============================================================================
bool VelourPProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void VelourPProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    osRate = fs * 4.0;
    maxBlock = juce::jmax (1, samplesPerBlock);
    numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));

    for (auto& c : character)
        c.prepare (osRate);

    firstBlock = true;
    reset();
}

void VelourPProcessor::reset()
{
    if (oversampler != nullptr) oversampler->reset();
    for (auto& ch : filters) for (auto& f : ch) f.reset();
    for (auto& c : character) c.reset();
}

void VelourPProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    auto block = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, (size_t) juce::jmin (numCh, buffer.getNumChannels()));
    for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
        processChunk (block.getSubBlock ((size_t) start, (size_t) juce::jmin (maxBlock, buffer.getNumSamples() - start)));

    // analyser feed
    const int n = buffer.getNumSamples();
    const auto scope = analyserFifo.write (n);
    auto feed = [&] (int start, int count, int offset)
    {
        for (int i = 0; i < count; ++i)
        {
            float v = buffer.getSample (0, offset + i);
            if (numCh > 1) v = 0.5f * (v + buffer.getSample (1, offset + i));
            analyserData[(size_t) (start + i)] = v;
        }
    };
    feed (scope.startIndex1, scope.blockSize1, 0);
    feed (scope.startIndex2, scope.blockSize2, scope.blockSize1);
}

void VelourPProcessor::processChunk (juce::dsp::AudioBlock<float> block)
{
    const auto s = readSettings();
    const bool analog = apvts.getRawParameterValue ("analog")->load() > 0.5f;

    // targets (frequencies and Q smoothed in the log domain, gains in dB)
    for (int b = 0; b < numBands; ++b)
    {
        const auto& bd = s.bands[(size_t) b];
        sFreq[(size_t) b].target = std::log (bd.freq);
        sGain[(size_t) b].target = bd.on ? bd.gain : 0.0;
        sQ[(size_t) b].target = std::log (bd.q);
    }
    sHpf.target = std::log (s.hpf > 0 ? s.hpf : hpfOff);
    sLpf.target = std::log (s.lpf > 0 ? s.lpf : lpfOff);
    sIron.target = apvts.getRawParameterValue ("iron")->load();
    sDrive.target = apvts.getRawParameterValue ("drive")->load();
    sOut.target = apvts.getRawParameterValue ("output")->load();

    auto all = [&] (auto fn)
    {
        for (int b = 0; b < numBands; ++b) { fn (sFreq[(size_t) b]); fn (sGain[(size_t) b]); fn (sQ[(size_t) b]); }
        fn (sHpf); fn (sLpf); fn (sIron); fn (sDrive); fn (sOut);
    };
    if (firstBlock)
    {
        all ([] (Smooth& sm) { sm.cur = sm.target; });
        firstBlock = false;
    }

    auto up = oversampler->processSamplesUp (block);
    const int n = (int) up.getNumSamples();
    const int nch = (int) up.getNumChannels();
    constexpr int segment = 128; // 32 host samples at 4x
    const double k = 1.0 - std::exp (-segment / (0.03 * osRate));
    float hot = 0.0f;

    for (int start = 0; start < n; start += segment)
    {
        all ([k] (Smooth& sm) { sm.cur += (sm.target - sm.cur) * k; });

        Settings cur = s;
        for (int b = 0; b < numBands; ++b)
        {
            auto& bd = cur.bands[(size_t) b];
            bd.on = true;                               // on/off is handled by the gain ramp
            bd.freq = std::exp (sFreq[(size_t) b].cur);
            bd.gain = sGain[(size_t) b].cur;
            bd.q = std::exp (sQ[(size_t) b].cur);
        }
        cur.hpf = s.hpf > 0 || sHpf.cur > std::log (hpfOff) + 0.01 ? std::exp (sHpf.cur) : 0.0;
        cur.lpf = s.lpf > 0 || sLpf.cur < std::log (lpfOff) - 0.01 ? std::exp (sLpf.cur) : 0.0;

        const auto coefs = design (cur, osRate);
        for (auto& ch : filters)
            for (int i = 0; i < numSlots; ++i)
                ch[(size_t) i].c = coefs[(size_t) i];

        const double iron = sIron.cur, drive = sDrive.cur;
        const double outG = juce::Decibels::decibelsToGain (sOut.cur);
        const int end = juce::jmin (n, start + segment);

        for (int c = 0; c < nch; ++c)
        {
            float* d = up.getChannelPointer ((size_t) c);
            auto& ch = filters[(size_t) c];
            auto& chr = character[(size_t) c];
            for (int i = start; i < end; ++i)
            {
                double x = d[i];
                if (analog) x = chr.iron (x, iron);
                for (auto& f : ch) x = f.process (x);
                if (analog)
                {
                    double lvl = 0.0;
                    x = chr.tube (x, drive, lvl);
                    hot = juce::jmax (hot, (float) lvl);
                }
                d[i] = (float) (x * outG);
            }
        }
    }

    oversampler->processSamplesDown (block);
    activity = juce::jmax (activity * 0.85f, juce::jlimit (0.0f, 1.0f, hot / 1.5f));
    driveActivity = activity;
}

void VelourPProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VelourPProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VelourPProcessor::createEditor()
{
    return new VelourPEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VelourPProcessor();
}
