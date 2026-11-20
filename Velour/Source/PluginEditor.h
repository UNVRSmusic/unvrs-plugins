#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class VelourLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VelourLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class VelourEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VelourEditor (VelourProcessor&);
    ~VelourEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintDisplay (juce::Graphics&);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void setupKnob (Knob&, const juce::String& id, const juce::String& name, const juce::String& tip, int steps = 0);

    VelourProcessor& proc;
    VelourLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    Knob hpf, lowFreq, lowBoost, lowAtten, midFreq, midGain, highFreq, highBoost, highBw, attenFreq, highAtten, iron, drive, output;
    juce::ToggleButton analog { "ANALOG" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> analogAtt;

    struct Section { juce::String title; juce::Rectangle<int> area; };
    std::vector<Section> sections;
    juce::Rectangle<int> header, display, lamp;

    juce::Path curve;
    juce::String lastKey;
    float glow = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelourEditor)
};
