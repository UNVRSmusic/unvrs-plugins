#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "LooperEngine.h"

class SpoolProcessor final : public juce::AudioProcessor
{
public:
    using Command = LooperEngine::Command;

    SpoolProcessor();
    ~SpoolProcessor() override = default;

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
    LooperEngine engine;

    static const juce::StringArray modeChoices, modAChoices, modBChoices, syncChoices, rampModeChoices, rampDirChoices;

    /** Message thread: queue a footswitch-style command for the audio thread. */
    void sendCommand (Command c);
    void retriggerRamp() { rampRetrigger = true; }

    /** Writes the current layer to ~/Music/Spool as a 24-bit WAV. */
    bool exportLoop (juce::File& written);

    // effective (ramped) values for the GUI: volume, repeats, modA, stability, modB
    std::array<std::atomic<float>, 5> modulated {};
    std::atomic<bool> syncWaiting { false };

    // MIDI note map (C1 = 36)
    static constexpr int noteBase = 36;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    LooperEngine::Params readParams (int numSamples);
    void toggleParam (const juce::String& id);

    juce::AbstractFifo cmdFifo { 64 };
    std::array<int, 64> cmdData {};

    bool hasPending = false;
    Command pendingCmd = Command::recPlay;
    double pendingPpq = 0.0;

    double rampPhase = 0.0;
    int lastRampMode = 0;
    std::atomic<bool> rampRetrigger { false };

    int64_t sampleCounter = 0;
    std::array<int64_t, 128> noteOnAt {};

    double fs = 48000.0;

    std::atomic<float>* pMode = nullptr;     std::atomic<float>* pVolume = nullptr;
    std::atomic<float>* pRepeats = nullptr;  std::atomic<float>* pModA = nullptr;
    std::atomic<float>* pModASel = nullptr;  std::atomic<float>* pModAOn = nullptr;
    std::atomic<float>* pStab = nullptr;     std::atomic<float>* pModB = nullptr;
    std::atomic<float>* pModBSel = nullptr;  std::atomic<float>* pModBOn = nullptr;
    std::atomic<float>* pReverse = nullptr;  std::atomic<float>* pDryKill = nullptr;
    std::atomic<float>* pDubAfter = nullptr; std::atomic<float>* pSampleLoop = nullptr;
    std::atomic<float>* pSync = nullptr;     std::atomic<float>* pRampMode = nullptr;
    std::atomic<float>* pRampRate = nullptr; std::atomic<float>* pRampDir = nullptr;
    std::atomic<float>* pRampSync = nullptr;
    std::array<std::atomic<float>*, 5> pRampTarget {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpoolProcessor)
};
