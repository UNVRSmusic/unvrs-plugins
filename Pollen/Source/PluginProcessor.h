#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "GrainCore.h"

class PollenProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    PollenProcessor();
    ~PollenProcessor() override;

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
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- sample handling (message thread) ---------------------------------------------------
    /** Loads a file on a background thread (cancels a load in progress). Progress via getStatus(). */
    void loadSampleAsync (const juce::File&, bool selectSampleSource = true);
    /** Decodes a file on the calling thread and hands it to the audio thread. Returns an error, empty on success. */
    juce::String loadSample (const juce::File&, bool selectSampleSource = true, std::function<bool()> shouldAbort = {});
    bool isLoading() const noexcept { return loading.load(); }
    juce::String getStatus (int& version) const;
    static constexpr double maxSeconds = 30.0 * 60.0;
    juce::File getSampleFile() const;
    bool isSampleMissing() const;
    /** Peak overview of a source (built-in index or pl::numBuiltins for the user sample). */
    std::vector<float> overviewFor (int source) const;
    int sampleVersion() const noexcept { return userVersion.load(); }
    bool isAudioFile (const juce::String& path) const;
    juce::String audioWildcard() const { return formats.getWildcardForAllFormats(); }
    void collectGarbage();

    static juce::StringArray sourceNames();
    static int userSourceIndex() { return pl::numBuiltins; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    pl::Engine& display() noexcept { return engine; }

    pl::Settings readSettings() const;

private:
    void timerCallback() override;
    void setStatus (const juce::String&);
    void selectSampleSource();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    enum Id { p_source, p_position, p_scan, p_spray, p_drift, p_size, p_density, p_shape, p_transpose, p_jitter,
              p_octaves, p_reverse, p_spread, p_attack, p_release, p_tone, p_reverb, p_decay, p_volume, numParams };
    static const char* const ids[numParams];
    std::array<std::atomic<float>*, numParams> ptr {};
    float P (Id i) const { return ptr[(size_t) i]->load (std::memory_order_relaxed); }

    pl::Engine engine;
    int currentProgram = 0;

    // user sample hand-over: message thread fills `pending`; the audio thread swaps it in and
    // parks the previous one in `retired`, which the message thread frees once no grain reads it
    juce::AudioFormatManager formats;
    juce::SpinLock handoff;
    std::unique_ptr<pl::Source> pending, activeUser, retired;
    bool retiredFree = false;

    mutable juce::CriticalSection fileLock;
    juce::File sampleFile;
    bool sampleMissing = false;
    std::vector<float> userOverview;
    std::atomic<int> userVersion { 0 };
    juce::String status;
    int statusVersion = 0;
    std::atomic<bool> loading { false }, selectPending { false };

    juce::ThreadPool loader { 1 };   // last member: destroyed (and its job finished) first

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PollenProcessor)
};
