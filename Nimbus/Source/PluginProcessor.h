#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ReverbEngine.h"

class NimbusProcessor final : public juce::AudioProcessor
{
public:
    NimbusProcessor();
    ~NimbusProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static const juce::StringArray characterChoices, shimmerChoices;

    ReverbEngine::Params readParams() const;
    std::atomic<float> outLevel { 0.0f }; // for the display glow

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    ReverbEngine engine;
    std::vector<float> monoR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NimbusProcessor)
};
