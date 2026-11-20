#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class SpoolLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SpoolLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
};

/** TextButton that reports press and release separately (for hold gestures). */
class PressButton final : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    std::function<void()> onPress, onRelease;

    void mouseDown (const juce::MouseEvent& e) override
    {
        juce::TextButton::mouseDown (e);
        if (onPress) onPress();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        juce::TextButton::mouseUp (e);
        if (onRelease) onRelease();
    }
};

class SpoolEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpoolEditor (SpoolProcessor&);
    ~SpoolEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintReel (juce::Graphics&);
    void paintSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void setupChoice (Choice&, const juce::String& paramId, const juce::StringArray& items);
    void setupToggle (Toggle&, const juce::String& paramId, const juce::String& text);
    void setupTransport (PressButton&, const juce::String& tooltip);

    SpoolProcessor& proc;
    SpoolLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    Knob volume, repeats, stability, modA, modB, rampRate;
    Choice mode, sync, modASel, modBSel, rampMode, rampDir;
    Toggle modAOn, modBOn, reverse, dryKill, dubAfter, sampleLoop, rampSync;
    Toggle rampVol, rampRep, rampA, rampStab, rampB;

    PressButton recButton { "REC / PLAY" }, stopButton { "STOP" }, oneShotButton { "ONE-SHOT" },
                undoButton { "UNDO" }, redoButton { "REDO" }, triggerButton { "TRIGGER" },
                clearButton { "CLEAR" }, retrigButton { "RETRIG" }, exportButton { "EXPORT WAV" };
    juce::Label statusLabel;

    juce::Rectangle<int> headerArea, reelArea, loopSection, modSection, rampSection, transportArea;

    double recPressTime = 0.0;
    std::vector<float> overview;
    int overviewCountdown = 0;
    LooperEngine::Info info;
    float pulse = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpoolEditor)
};
