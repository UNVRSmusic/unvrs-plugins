#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class EmberLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    EmberLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

/** X = band centre (log), Y = band width (top narrow, bottom wide). */
class FilterPad final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FilterPad (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (12.0f, 10.0f).withTrimmedBottom (14.0f); }
    void setFromMouse (juce::Point<float>);

    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter& freq;
    juce::RangedAudioParameter& width;
};

class EmberEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EmberEditor (EmberProcessor&);
    ~EmberEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintTransfer (juce::Graphics&);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void setupKnob (Knob&, const juce::String& id, const juce::String& name, const juce::String& tip);

    EmberProcessor& proc;
    EmberLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    FilterPad pad;

    Knob freq, width, drive, tone, dynamics, mix, output;
    juce::Rectangle<int> header, padArea, transferArea, driveArea, bottomArea;
    float heat = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberEditor)
};
