#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class NimbusLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    NimbusLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
};

class NimbusEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit NimbusEditor (NimbusProcessor&);
    ~NimbusEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void parameterChanged (const juce::String&, float) override { curveDirty = true; }
    void paintDisplay (juce::Graphics&);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, const juce::String& tip);

    NimbusProcessor& proc;
    NimbusLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    Knob predelay, size, decay, diffusion, bloom, early;
    Knob bassMult, dampHz, trebleMult, modDepth, modRate, shimmer;
    Knob lowCut, highCut, width, duck, mix;

    juce::ComboBox character;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> characterAtt;
    juce::ComboBox shimmerPitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shimmerPitchAtt;
    juce::ToggleButton freeze { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAtt;

    juce::Rectangle<int> header, display;
    std::array<juce::Rectangle<int>, 3> rows;

    bool curveDirty = true;
    juce::Path curve, curveFill;
    float glow = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NimbusEditor)
};
