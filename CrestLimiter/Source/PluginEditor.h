#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class CrestLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CrestLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
};

class CrestEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CrestEditor (CrestProcessor&);
    ~CrestEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintDisplay (juce::Graphics&);
    void paintMeters (juce::Graphics&);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void setupChoice (Choice&, const juce::String& paramId, const juce::String& name, const juce::StringArray& items);

    CrestProcessor& proc;
    CrestLookAndFeel lnf;

    Knob gain, ceiling, lookahead, release, link;
    Choice style, oversampling, dither;
    juce::ToggleButton unityButton { "UNITY GAIN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> unityAtt;
    juce::TextButton resetButton { "RESET" };

    // history ring (newest at writePos - 1)
    static constexpr int histSize = 2048;
    std::array<CrestProcessor::HistPoint, histSize> hist {};
    int histWrite = 0, histCount = 0;

    // meter ballistics (dB)
    float inLvl[2] { -100, -100 }, outLvl[2] { -100, -100 }, grLvl = 0;
    float inHold = -100, outHold = -100, grHold = 0;

    juce::Rectangle<int> displayArea, meterArea, headerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrestEditor)
};
