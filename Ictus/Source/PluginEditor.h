#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class IctusLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    IctusLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

class IctusEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit IctusEditor (IctusProcessor&);
    ~IctusEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintModes (juce::Graphics&);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void setupKnob (Knob&, const juce::String& id, const juce::String& name, const juce::String& tip, bool centred = false);
    void setupCombo (juce::ComboBox&, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&,
                     const juce::String& id, const juce::StringArray& items, const juce::String& tip);
    void setupToggle (juce::ToggleButton&, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>&,
                      const juce::String& id, const juce::String& tip);
    void layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> knobs, int knobWidth);

    IctusProcessor& proc;
    IctusLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    using ComboAtt = std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>;
    using ButtonAtt = std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>;

    juce::ComboBox preset;

    juce::ToggleButton malletOn { "MALLET" }, noiseOn { "NOISE" };
    ButtonAtt malletOnAtt, noiseOnAtt;
    Knob malletVol, malletStiff, malletVelStiff, malletNoise, malletColor;
    juce::ComboBox noiseType; ComboAtt noiseTypeAtt;
    Knob noiseVol, noiseFreq, noiseRes, noiseKey, noiseA, noiseD, noiseS, noiseR;

    struct ResUI
    {
        juce::ToggleButton on;
        ButtonAtt onAtt;
        juce::ComboBox type, modes;
        ComboAtt typeAtt, modesAtt;
        Knob tune, fine, key, decay, damp, material, shape, inharm, bright, hit, listen, width, gain;
    } res[2];

    juce::ComboBox structure; ComboAtt structureAtt;
    Knob voices, velocity, penvAmt, penvTime, bleed, volume;

    juce::MidiKeyboardComponent keyboard;

    juce::Rectangle<int> header, modeArea, excRow, res1Row, res2Row, globalRow;
    juce::Rectangle<int> malletPanel, noisePanel, globalPanel;
    std::array<ictus::Layout, 2> layouts;
    std::array<ictus::ResParams, 2> shownParams;
    std::array<bool, 2> shownValid { false, false };
    int lastProgram = -1, lastVoices = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IctusEditor)
};
