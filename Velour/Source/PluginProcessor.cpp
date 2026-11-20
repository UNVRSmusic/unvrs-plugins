#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray VelourProcessor::lowFreqs   { "20", "30", "60", "100", "150" };
const juce::StringArray VelourProcessor::highFreqs  { "3k", "4k", "5k", "8k", "10k", "12k", "16k" };
const juce::StringArray VelourProcessor::attenFreqs { "5k", "10k", "20k" };

VelourProcessor::VelourProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout VelourProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto fmt = [] (std::function<String (float)> f)
    { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
    auto ten = fmt ([] (float v) { return String (v, 1); });
    auto hz  = fmt ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto hpf = fmt ([] (float v) { return v < 20.5f ? String ("OFF") : String (roundToInt (v)) + " Hz"; });
    auto db  = fmt ([] (float v) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    auto pct = fmt ([] (float v) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto bw  = fmt ([] (float v) { return v < 0.33f ? String ("sharp") : v < 0.66f ? String ("medium") : String ("broad"); });

    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };
    auto add = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a)); };
    auto choice = [&] (const char* id, const char* name, const StringArray& c, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };

    add    ("hpf",       "High-pass",       skewed (20.0f, 400.0f, 80.0f), 20.0f, hpf);
    choice ("lowFreq",   "Low Freq",        lowFreqs, 2);
    add    ("lowBoost",  "Low Boost",       NormalisableRange<float> (0.0f, 10.0f, 0.1f), 0.0f, ten);
    add    ("lowAtten",  "Low Atten",       NormalisableRange<float> (0.0f, 10.0f, 0.1f), 0.0f, ten);
    add    ("midFreq",   "Mid Freq",        skewed (150.0f, 8000.0f, 1000.0f), 1000.0f, hz);
    add    ("midGain",   "Mid Gain",        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db);
    choice ("highFreq",  "High Freq",       highFreqs, 4);
    add    ("highBoost", "High Boost",      NormalisableRange<float> (0.0f, 10.0f, 0.1f), 0.0f, ten);
    add    ("highBw",    "High Bandwidth",  NormalisableRange<float> (0.0f, 1.0f), 0.5f, bw);
    choice ("attenFreq", "Atten Freq",      attenFreqs, 1);
    add    ("highAtten", "High Atten",      NormalisableRange<float> (0.0f, 10.0f, 0.1f), 0.0f, ten);
    add    ("iron",      "Iron",            NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct);
    add    ("drive",     "Drive",           NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
    add    ("output",    "Output",          NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, db);
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "analog", 1 }, "Analog", true));
    return l;
}

VelourProcessor::EqSettings VelourProcessor::readEq() const
{
    auto v = [this] (const char* id) { return (double) apvts.getRawParameterValue (id)->load(); };
    EqSettings s;
    s.hpf = v ("hpf") < 20.5 ? 0.0 : v ("hpf");
    s.lowFreq = (int) v ("lowFreq");
    s.lowBoost = v ("lowBoost");
    s.lowAtten = v ("lowAtten");
    s.midFreq = v ("midFreq");
    s.midGain = v ("midGain");
    s.highFreq = (int) v ("highFreq");
    s.highBoost = v ("highBoost");
    s.highBw = v ("highBw");
    s.attenFreq = (int) v ("attenFreq");
    s.highAtten = v ("highAtten");
    return s;
}

//==============================================================================
static VelourProcessor::Coefs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

static VelourProcessor::Coefs peak (double fs, double f, double q, double db)
{
    const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * f / fs;
    const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}

static VelourProcessor::Coefs lowShelf (double fs, double f, double q, double db)
{
    const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * f / fs;
    const double c = std::cos (w), al = std::sin (w) / (2.0 * q), sa = 2.0 * std::sqrt (A) * al;
    return normalise (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                      (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
}

static VelourProcessor::Coefs highShelf (double fs, double f, double q, double db)
{
    const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * f / fs;
    const double c = std::cos (w), al = std::sin (w) / (2.0 * q), sa = 2.0 * std::sqrt (A) * al;
    return normalise (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                      (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
}

static VelourProcessor::Coefs highPass (double fs, double f, double q)
{
    const double w = juce::MathConstants<double>::twoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}

std::vector<VelourProcessor::Coefs> VelourProcessor::designEq (const EqSettings& s, double rate)
{
    // fixed slots (an unused band is a pass-through), so switching bands never resets filter state
    std::vector<Coefs> out (maxBands);
    const double nyq = 0.45 * rate;

    if (s.hpf > 0.0)
        out[0] = highPass (rate, juce::jmin (s.hpf, nyq), 0.6);

    // LOW: boost is a slightly resonant shelf, atten a softer shelf a little higher up,
    // so using both gives the classic bump with a dip just above it.
    const double lf = lowFreqHz (s.lowFreq);
    if (s.lowBoost > 0.0) out[1] = lowShelf (rate, lf, 0.9, s.lowBoost * 1.35);         // up to +13.5 dB
    if (s.lowAtten > 0.0) out[2] = lowShelf (rate, lf * 2.6, 0.45, -s.lowAtten * 1.1);  // up to -11 dB, broad

    // MID: proportional Q - gentle moves are wide, big moves get narrower
    if (std::abs (s.midGain) > 0.01)
        out[3] = peak (rate, juce::jmin (s.midFreq, nyq), 0.55 + 0.11 * std::abs (s.midGain), s.midGain);

    // HIGH boost: bandwidth from sharp (Q 2.4) to broad (Q 0.45)
    if (s.highBoost > 0.0)
        out[4] = peak (rate, juce::jmin (highFreqHz (s.highFreq), nyq), 2.4 * std::pow (0.45 / 2.4, s.highBw), s.highBoost * 1.6);

    // HIGH atten: smooth shelf
    if (s.highAtten > 0.0)
        out[5] = highShelf (rate, juce::jmin (attenFreqHz (s.attenFreq), nyq), 0.5, -s.highAtten * 1.6);

    return out;
}

double VelourProcessor::magnitudeDb (const std::vector<Coefs>& cs, double f, double rate)
{
    const std::complex<double> z = std::polar (1.0, -juce::MathConstants<double>::twoPi * f / rate);
    std::complex<double> h = 1.0;
    for (const auto& c : cs)
        h *= (c.b0 + c.b1 * z + c.b2 * z * z) / (1.0 + c.a1 * z + c.a2 * z * z);
    return 20.0 * std::log10 (std::abs (h) + 1.0e-12);
}

//==============================================================================
bool VelourProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void VelourProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    osRate = fs * 4.0;
    maxBlock = juce::jmax (1, samplesPerBlock);
    numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));

    ironLpCoef = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * 120.0 / osRate);
    dcCoef = 1.0 - juce::MathConstants<double>::twoPi * 5.0 / osRate;

    ironSm.reset (osRate, 0.03);  ironSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("iron")->load());
    driveSm.reset (osRate, 0.03); driveSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("drive")->load());
    outSm.reset (osRate, 0.03);   outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((double) apvts.getRawParameterValue ("output")->load()));

    reset();
}

void VelourProcessor::reset()
{
    if (oversampler != nullptr) oversampler->reset();
    for (auto& ch : eq) for (auto& b : ch) b.z1 = b.z2 = 0.0;
    ironLp = {}; ironHf = {}; dcX = {}; dcY = {};
}

void VelourProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    auto block = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, (size_t) juce::jmin (numCh, buffer.getNumChannels()));
    for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
        processChunk (block.getSubBlock ((size_t) start, (size_t) juce::jmin (maxBlock, buffer.getNumSamples() - start)));
}

void VelourProcessor::processChunk (juce::dsp::AudioBlock<float> block)
{
    // coefficients: redesign each block, keep the filter states (bands keep their slots)
    const auto coefs = designEq (readEq(), osRate);
    numBands = maxBands;
    for (auto& ch : eq)
        for (int b = 0; b < numBands; ++b)
            ch[(size_t) b].c = coefs[(size_t) b];

    const bool analog = apvts.getRawParameterValue ("analog")->load() > 0.5f;
    ironSm.setTargetValue (apvts.getRawParameterValue ("iron")->load());
    driveSm.setTargetValue (apvts.getRawParameterValue ("drive")->load());
    outSm.setTargetValue (juce::Decibels::decibelsToGain ((double) apvts.getRawParameterValue ("output")->load()));

    auto up = oversampler->processSamplesUp (block);
    const int n = (int) up.getNumSamples();
    const int nch = (int) up.getNumChannels();
    float hot = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const double iron = ironSm.getNextValue();
        const double drive = driveSm.getNextValue();
        const double outG = outSm.getNextValue();

        // transformer: low band saturates (core), HF gently rolls off with more iron
        const double k = 0.3 + 5.0 * iron * iron;
        const double hfCoef = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * (40000.0 - 22000.0 * iron) / osRate);
        // tube: pre-gain 1..5x into an asymmetric curve, normalised to unity small-signal gain
        const double g = std::pow (10.0, 0.7 * drive);
        // asymmetry grows with drive, so the 2nd harmonic stays on top like a single-ended stage
        const double bias = 0.22 + 0.5 * drive;
        const double tb = std::tanh (bias), slope = 1.0 - tb * tb;

        for (int c = 0; c < nch; ++c)
        {
            float* d = up.getChannelPointer ((size_t) c);
            double x = d[i];

            if (analog)
            {
                ironLp[(size_t) c] += (x - ironLp[(size_t) c]) * ironLpCoef;
                const double lo = ironLp[(size_t) c];
                x = (x - lo) + std::tanh (k * lo) / k;
                ironHf[(size_t) c] += (x - ironHf[(size_t) c]) * hfCoef;
                x = ironHf[(size_t) c];
            }

            for (int b = 0; b < numBands; ++b)
                x = eq[(size_t) c][(size_t) b].process (x);

            if (analog)
            {
                const double u = g * x;
                x = (std::tanh (u + bias) - tb) / (slope * g);
                hot = juce::jmax (hot, (float) std::abs (u));

                const double y = x - dcX[(size_t) c] + dcCoef * dcY[(size_t) c]; // remove the bias offset
                dcX[(size_t) c] = x;
                dcY[(size_t) c] = y;
                x = y;
            }

            d[i] = (float) (x * outG);
        }
    }

    oversampler->processSamplesDown (block);
    activity = juce::jmax (activity * 0.85f, juce::jlimit (0.0f, 1.0f, hot / 1.5f));
    driveActivity = activity;
}

void VelourProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VelourProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VelourProcessor::createEditor()
{
    return new VelourEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VelourProcessor();
}
