#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "IctusEngine.h"

class IctusProcessor final : public juce::AudioProcessor
{
public:
    IctusProcessor();
    ~IctusProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // GUI helpers
    ictus::Settings readSettings() const;
    static juce::String resId (int r, const char* name) { return "r" + juce::String (r + 1) + name; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    std::atomic<int> voicesPlaying { 0 };

    static const juce::StringArray typeChoices, modeCountChoices, structureChoices, noiseTypeChoices;
    static constexpr int modeCounts[] { 32, 64, 96, 128 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct ResPtrs { std::atomic<float> *on, *type, *modes, *tune, *fine, *key, *decay, *damp, *material, *shape, *inharm, *bright, *hit, *listen, *width, *gain; };
    struct Ptrs
    {
        std::atomic<float> *malletOn, *malletVol, *malletStiff, *malletVelStiff, *malletNoise, *malletColor;
        std::atomic<float> *noiseOn, *noiseType, *noiseVol, *noiseFreq, *noiseRes, *noiseKey, *noiseA, *noiseD, *noiseS, *noiseR;
        std::atomic<float> *structure, *voices, *velocity, *penvAmt, *penvTime, *bleed, *volume;
        ResPtrs res[2];
    } ptrs {};

    ictus::Engine engine;
    juce::AudioBuffer<float> scratch;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IctusProcessor)
};
