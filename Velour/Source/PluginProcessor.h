#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <complex>

/*
    Velour: coloured EQ.

      in -> input transformer (IRON: low-frequency core saturation + soft HF loss)
         -> passive-style EQ: HPF, LOW boost + LOW atten (same knob frequency, interacting),
            MID bell with proportional Q, HIGH boost (bandwidth), HIGH atten shelf
         -> tube make-up stage (DRIVE: asymmetric, even harmonics; sits after the EQ, so what
            you boost drives it harder)
         -> DC block -> output trim
    Everything after the input runs at 4x, filters in double precision.
*/
class VelourProcessor final : public juce::AudioProcessor
{
public:
    VelourProcessor();
    ~VelourProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    static const juce::StringArray lowFreqs, highFreqs, attenFreqs;
    static double lowFreqHz (int i)   { static const double v[] { 20, 30, 60, 100, 150 };        return v[juce::jlimit (0, 4, i)]; }
    static double highFreqHz (int i)  { static const double v[] { 3000, 4000, 5000, 8000, 10000, 12000, 16000 }; return v[juce::jlimit (0, 6, i)]; }
    static double attenFreqHz (int i) { static const double v[] { 5000, 10000, 20000 };          return v[juce::jlimit (0, 2, i)]; }

    struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    static constexpr int maxBands = 6; // HPF, low boost, low atten, mid, high boost, high atten

    struct EqSettings
    {
        double hpf = 0;                 // Hz, 0 = off
        int lowFreq = 2; double lowBoost = 0, lowAtten = 0;
        double midFreq = 1000, midGain = 0;
        int highFreq = 4; double highBoost = 0, highBw = 0.5;
        int attenFreq = 1; double highAtten = 0;
    };

    /** The EQ section as a list of biquads (shared by the audio path and the curve display). */
    static std::vector<Coefs> designEq (const EqSettings&, double fs);
    static double magnitudeDb (const std::vector<Coefs>&, double f, double fs);
    EqSettings readEq() const;

    std::atomic<float> driveActivity { 0.0f }; // for the GUI lamp

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::dsp::AudioBlock<float> block);

    struct Biquad
    {
        Coefs c;
        double z1 = 0, z2 = 0;
        double process (double x) noexcept
        {
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return y;
        }
    };

    std::array<std::array<Biquad, maxBands>, 2> eq {};
    int numBands = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    double fs = 48000.0, osRate = 192000.0;
    int maxBlock = 512, numCh = 2;

    std::array<double, 2> ironLp {}, ironHf {}, dcX {}, dcY {};
    double ironLpCoef = 0.0, dcCoef = 0.999;
    juce::SmoothedValue<double> ironSm, driveSm, outSm;
    float activity = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelourProcessor)
};
