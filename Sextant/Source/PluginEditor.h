#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class SextantLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SextantLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

//==============================================================================
// Algorithm diagram; in edit mode a routing matrix (rows modulate columns, bottom row = output).
class AlgoView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit AlgoView (SextantProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    bool editing = false;
    int selected = 0;
    std::function<void (int)> onSelect;

private:
    void paintDiagram (juce::Graphics&, const sx::Routing&);
    void paintMatrix (juce::Graphics&, const sx::Routing&);
    juce::Rectangle<float> matrixCell (int row, int col) const;
    std::array<juce::Rectangle<float>, sx::numOps> boxes;
    SextantProcessor& proc;
};

//==============================================================================
class SextantEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SextantEditor (SextantProcessor&);
    ~SextantEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectOp (int op);
    void showTab (int tab);
    void paintEnvelope (juce::Graphics&);
    void paintWave (juce::Graphics&);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    using KnobList = std::vector<std::unique_ptr<Knob>>;
    Knob* makeKnob (KnobList&, const juce::String& id, const juce::String& name, const juce::String& tip, bool centred = false);
    struct Combo { juce::ComboBox box; juce::Label label; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att; };
    void setupCombo (Combo&, const juce::String& id, const juce::String& name, const juce::String& tip);
    void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, const std::vector<juce::Label*>& labels, int width);

    SextantProcessor& proc;
    SextantLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // header
    juce::ComboBox preset;
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, algoPrev { "<" }, algoNext { ">" };
    juce::ToggleButton editAlgo { "EDIT" };
    juce::ComboBox algoBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algoAtt;

    // operator mixer
    struct Strip
    {
        juce::TextButton select;
        juce::Slider level;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    std::array<Strip, sx::numOps> strips;
    AlgoView algoView;

    // operator editor: one set per operator, only the selected one visible
    struct OpUI
    {
        std::array<juce::TextButton, sx::numModes> modeButtons;
        Combo wave, filterType;
        juce::ToggleButton fixed { "FIXED" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fixedAtt;
        KnobList knobs;   // coarse fine detune freq feedback vel keylvl res drive A D S R
    };
    std::array<OpUI, sx::numOps> opUI;
    int selectedOp = 0;

    // global tabs
    std::array<juce::TextButton, 4> tabs;
    int currentTab = 0;
    std::array<KnobList, 4> tabKnobs;
    Combo voiceMode, filterType, lfoWave, delaySync;

    juce::MidiKeyboardComponent keyboard;

    juce::Rectangle<int> header, algoPanel, mixerPanel, envPanel, opPanel, globalPanel, waveArea;
    int lastProgram = -1;
    juce::int64 paintedSignature = -1;
    std::array<float, sx::numOps> meter {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SextantEditor)
};
