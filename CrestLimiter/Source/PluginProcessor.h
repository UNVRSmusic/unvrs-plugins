#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "LimiterEngine.h"
#include "LoudnessMeter.h"

class CrestProcessor final : public juce::AudioProcessor
{
public:
    CrestProcessor();
    ~CrestProcessor() override = default;

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
    static const juce::StringArray styleChoices, osChoices, ditherChoices;

    // display history: one point per 10 ms, written by the audio thread, read by the editor
    struct HistPoint { float inPk[2]; float outPk[2]; float grDb; };
    static constexpr int histFifoSize = 2048;
    static constexpr int histPointsPerSecond = 100;
    juce::AbstractFifo histFifo { histFifoSize };
    std::array<HistPoint, histFifoSize> histData {};

    LoudnessMeter loudness;
    std::atomic<float> truePeakMax { 0.0f }; // linear
    void resetMeters();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::AudioBuffer<float>&, int start, int len);

    static constexpr int numOs = 4; // 1x, 2x, 4x, 8x
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, numOs> oversamplers;
    std::array<LimiterEngine, numOs> engines;
    std::array<int, numOs> latencies {};
    int maxLatency = 0, osIndex = -1;

    std::unique_ptr<juce::dsp::Oversampling<float>> meterOs; // 4x, true-peak metering
    juce::AudioBuffer<float> meterBuf, padBuf;
    int padPos = 0;

    std::vector<float> grBuf, gainBuf;
    std::vector<std::array<float, 2>> inPkBuf;
    int maxBlock = 512, preparedChannels = 2;

    juce::SmoothedValue<float> inGain;
    juce::Random rng;

    int chunkLen = 441, chunkCount = 0;
    HistPoint chunkAcc {};

    std::atomic<float>* pGain = nullptr;    std::atomic<float>* pCeiling = nullptr;
    std::atomic<float>* pLook = nullptr;    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pLink = nullptr;    std::atomic<float>* pStyle = nullptr;
    std::atomic<float>* pOs = nullptr;      std::atomic<float>* pDither = nullptr;
    std::atomic<float>* pUnity = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrestProcessor)
};
