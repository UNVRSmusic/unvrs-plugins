#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class PoiseLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PoiseLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

class PoiseEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PoiseEditor (PoiseProcessor&);
    ~PoiseEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintBalance (juce::Graphics&);
    void paintCorrection (juce::Graphics&);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void setupKnob (Knob&, const juce::String& id, const juce::String& name, const juce::String& tip, bool centred = false);

    PoiseProcessor& proc;
    PoiseLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    Knob amount, keep, speed, resolution, range, tilt, lowLimit, highLimit, mix, output;
    juce::ComboBox target, mode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAtt, modeAtt;
    juce::ToggleButton autoGain { "AUTO GAIN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAtt;
    juce::TextButton learn { "LEARN" };

    juce::Rectangle<int> header, balanceArea, correctionArea, knobArea;
    std::array<float, RebalanceEngine::maxBands> level {}, targetCurve {}, gain {};
    float blink = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PoiseEditor)
};
