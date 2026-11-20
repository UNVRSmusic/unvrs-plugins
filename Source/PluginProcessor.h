#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ConverterEngine.h"

class S2KProcessor final : public juce::AudioProcessor
{
public:
    S2KProcessor();
    ~S2KProcessor() override = default;

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

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<bool> clipIndicator { false };

    static const juce::StringArray rateChoices, interpChoices;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    ConverterEngine::Settings readSettings() const;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    ConverterEngine engine;
    int preparedChannels = 2;
    juce::dsp::DryWetMixer<float> dryWet { 4096 };
    juce::SmoothedValue<float> inGain, outGain;

    std::atomic<float>* pRate = nullptr;   std::atomic<float>* pBits = nullptr;
    std::atomic<float>* pInterp = nullptr; std::atomic<float>* pAA = nullptr;
    std::atomic<float>* pDAC = nullptr;    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pReso = nullptr;   std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pNoise = nullptr;  std::atomic<float>* pIn = nullptr;
    std::atomic<float>* pOut = nullptr;    std::atomic<float>* pMix = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (S2KProcessor)
};
