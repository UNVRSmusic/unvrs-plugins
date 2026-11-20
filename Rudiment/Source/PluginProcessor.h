#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DrumVoices.h"
#include "Sequencer.h"

class RudimentProcessor final : public juce::AudioProcessor
{
public:
    RudimentProcessor();
    ~RudimentProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentKit; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // patterns (message thread)
    static juce::StringArray patternNames();
    void loadPattern (int index);

    static juce::String paramId (int element, const char* suffix) { return juce::String (rud::elementSpecs()[(size_t) element].prefix) + suffix; }
    void previewElement (int e) { previewMask.fetch_or (1u << e); }

    juce::AudioProcessorValueTreeState apvts;
    rud::Pattern pattern;
    std::array<std::atomic<int>, rud::numElements> playStep;
    std::array<std::atomic<float>, rud::numElements> meters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    int addMidiEvent (const juce::MidiMessage&, int sample, rud::Event* ev, int count);

    rud::DrumEngine engine;
    std::array<std::array<std::atomic<float>*, 12>, rud::numElements> elemPtr {};
    std::array<std::atomic<float>*, rud::numElements> levelPtr {}, panPtr {}, mutePtr {};
    std::atomic<float> *swingPtr = nullptr, *accentPtr = nullptr, *gluePtr = nullptr, *drivePtr = nullptr, *volumePtr = nullptr, *runPtr = nullptr;

    std::atomic<uint32_t> previewMask { 0 };
    double fs = 48000.0, internalPpq = 0.0;
    bool wasRunning = false;
    int currentKit = 0;
    std::array<rud::Event, 1024> events;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RudimentProcessor)
};
