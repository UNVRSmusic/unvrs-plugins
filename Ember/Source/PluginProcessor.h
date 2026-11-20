#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

/*
    Ember: stomp-box style overdrive.

      in -> band-pass (HP + LP, centre / width in octaves)
         -> 4x oversampled drive: gain -> asymmetric soft clipper -> rounding stage
         -> DC block -> Dynamics (restores the input envelope by a chosen amount)
         -> Tone (post low-pass sweep) -> Output -> Dry/Wet (latency-compensated)
*/
class EmberProcessor final : public juce::AudioProcessor
{
public:
    EmberProcessor();
    ~EmberProcessor() override = default;

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
    std::atomic<float> driveMeter { 0.0f }; // 0..1 how hard the clipper is working, for the GUI

    /** The static waveshaper (for drawing the transfer curve too). */
    static float shape (float u) noexcept
    {
        constexpr float p = 2.5f;
        return u / std::pow (1.0f + std::pow (std::abs (u), p), 1.0f / p);
    }
    /** Asymmetric clipper: the negative half clips lower, so even harmonics survive at any drive. */
    static float clip (float u) noexcept { return u >= 0.0f ? shape (u) : 0.82f * shape (u / 0.82f); }
    static float driveGain (float drive) { return juce::Decibels::decibelsToGain (4.0f + 42.0f * drive); }
    static constexpr float bias = 0.12f;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::dsp::AudioBlock<float> block);

    struct Svf { float s1 = 0.0f, s2 = 0.0f; };
    static float svfProcess (Svf& st, float x, float g, bool highpass) noexcept
    {
        constexpr float k = 1.4142135f;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        const float v3 = x - st.s2;
        const float v1 = a1 * st.s1 + a2 * v3;
        const float v2 = st.s2 + a2 * st.s1 + a3 * v3;
        st.s1 = 2.0f * v1 - st.s1;
        st.s2 = 2.0f * v2 - st.s2;
        return highpass ? x - k * v1 - v2 : v2;
    }
    float gFor (double hz) const { return (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, 0.45 * fs, hz) / fs); }

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::DryWetMixer<float> dryWet { 1024 };
    double fs = 48000.0;
    int maxBlock = 512, numCh = 2;

    std::array<Svf, 2> hp {}, lp {}, tone {};
    std::array<float, 2> envIn {}, envOut {}, dcX {}, dcY {};
    float gHp = 0.1f, gLp = 0.5f, gTone = 0.5f;           // smoothed coefficients
    float envAtt = 0.01f, envRel = 0.001f, kSmooth = 0.001f, dcCoef = 0.999f;
    juce::SmoothedValue<float> driveSm, dynSm, outSm;
    std::vector<float> envInBuf;                           // per-sample input envelope (max of channels)
    float meterAcc = 0.0f;

    std::atomic<float>* pFreq = nullptr;  std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pDrive = nullptr; std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pDyn = nullptr;   std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOut = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberProcessor)
};
