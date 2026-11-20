#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class RackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class S2KEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit S2KEditor (S2KProcessor&);
    ~S2KEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void setupCombo (juce::ComboBox&, juce::Label&, const juce::String& paramId, const juce::String& name,
                     const juce::StringArray& items,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);

    S2KProcessor& proc;
    RackLookAndFeel lnf;

    Knob input, bits, cutoff, reso, drive, noise, output, mix;

    juce::ComboBox rateBox, interpBox;
    juce::Label rateLabel, interpLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAtt, interpAtt;

    juce::ToggleButton aaButton { "AA FILTER" }, dacButton { "DAC FILTER" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aaAtt, dacAtt;

    juce::Rectangle<int> lcdArea, ledArea;
    int clipHold = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (S2KEditor)
};
