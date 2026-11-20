#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "FmCore.h"

class SextantProcessor final : public juce::AudioProcessor
{
public:
    SextantProcessor();
    ~SextantProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::String opId (int op, const char* name) { return "o" + juce::String (op + 1) + name; }
    static float coarseToRatio (int c) { return c <= 0 ? 0.5f : (float) c; }
    static juce::StringArray algorithmNames();
    static int userAlgorithmIndex() { return (int) sx::algorithms().size(); }

    // the routing currently in effect (preset or user), safe to call from the GUI
    sx::Routing currentRouting() const;
    // GUI edits: copies the current preset routing into the user slot and selects it
    void editRouting (int src, int dst, bool carrierToggle);

    sx::Settings readSettings() const;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    std::atomic<uint64_t> userConn { 0 };
    std::atomic<uint8_t> userCarriers { 1 };
    std::array<std::atomic<float>, sx::numOps>& activity() { return engine.activity; }

    static const juce::StringArray modeNames, waveNames;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct OpPtrs { std::atomic<float> *mode, *wave, *fixed, *coarse, *fine, *detune, *freq, *level, *feedback, *vel, *keyLvl, *A, *D, *S, *R, *fType, *res, *drive; };
    std::array<OpPtrs, sx::numOps> opPtr {};
    enum GlobalId { g_algo, g_voiceMode, g_poly, g_unison, g_uniDetune, g_uniSpread, g_glide, g_bend, g_fType, g_cutoff, g_res, g_fEnv, g_fKey, g_fVel, g_fA, g_fD, g_fS, g_fR, g_lfoWave, g_lfoRate, g_lfoDelay, g_lfoPitch, g_lfoFm, g_lfoAmp, g_lfoFilter, g_mwVib, g_chorus, g_dMix, g_dTime, g_dFb, g_dSync, g_volume, numGlobals };
    static const char* const globalIds[numGlobals];
    std::array<std::atomic<float>*, numGlobals> g {};
    float G (GlobalId i) const { return g[(size_t) i]->load (std::memory_order_relaxed); }

    sx::Engine engine;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SextantProcessor)
};
