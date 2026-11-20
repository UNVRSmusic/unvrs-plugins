#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../../shared/AnalogEqDsp.h"

/*
    Velour P: parametric coloured EQ.

      in -> transformer (IRON) -> HPF -> 6 parametric bands -> LPF -> tube (DRIVE) -> output
    4x oversampled; coefficients are redesigned every 32 host samples from smoothed
    parameters, so fast automation does not zipper.
*/
class VelourPProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 6;
    enum BandType { bell = 0, lowShelf, highShelf };

    VelourPProcessor();
    ~VelourPProcessor() override = default;

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
    static const juce::StringArray typeChoices, slopeChoices;
    static juce::String bandId (int band, const char* what) { return "b" + juce::String (band + 1) + what; }

    struct Band { bool on = true; int type = bell; double freq = 1000, gain = 0, q = 0.7; };
    struct Settings
    {
        std::array<Band, numBands> bands;
        double hpf = 0, lpf = 0;      // 0 = off
        bool hpf24 = false, lpf24 = false, consoleQ = true;
    };

    Settings readSettings() const;

    /** Slot layout: 2 x HPF, 6 bands, 2 x LPF (unused slots are pass-through). */
    static constexpr int numSlots = 2 + numBands + 2;
    static std::array<analogeq::Coefs, numSlots> design (const Settings&, double fs);
    static analogeq::Coefs designBand (const Band&, bool consoleQ, double fs);

    // spectrum feed for the editor (mono, post-output)
    juce::AbstractFifo analyserFifo { 32768 };
    std::vector<float> analyserData = std::vector<float> (32768);
    std::atomic<float> driveActivity { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::dsp::AudioBlock<float> block);

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    double fs = 48000.0, osRate = 192000.0;
    int maxBlock = 512, numCh = 2;

    std::array<std::array<analogeq::Biquad, numSlots>, 2> filters {};
    std::array<analogeq::Character, 2> character {};

    // smoothed band parameters (log-frequency, dB, log-Q) + filters
    struct Smooth { double cur = 0, target = 0; };
    std::array<Smooth, numBands> sFreq, sGain, sQ;
    Smooth sHpf, sLpf, sIron, sDrive, sOut;
    bool firstBlock = true;
    float activity = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelourPProcessor)
};
