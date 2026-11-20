#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class PollenLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PollenLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

//==============================================================================
// Source waveform with the grain cloud on top. Click/drag sets Position, drop a file to load it.
class WaveView final : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
{
public:
    explicit WaveView (PollenProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

    void refresh();   // called from the editor's timer

private:
    void setPositionFromX (float x);
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (14.0f, 22.0f); }

    PollenProcessor& proc;
    juce::RangedAudioParameter* position;
    std::vector<float> overview;
    int shownSource = -1, shownVersion = -1;
    bool dragOver = false;

    struct Dot { float x, amp; uint32_t id; };
    std::vector<Dot> dots;
    std::array<float, pl::Engine::maxVoices> heads {};
};

//==============================================================================
class PollenEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PollenEditor (PollenProcessor&);
    ~PollenEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseFile();

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Section
    {
        juce::String title;
        std::vector<Knob*> knobs;
        juce::Rectangle<int> bounds;
    };
    Knob* makeKnob (const juce::String& id, const juce::String& name, const juce::String& tip, bool centred = false);
    void layoutSection (Section&, juce::Rectangle<int>);

    PollenProcessor& proc;
    PollenLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::ComboBox preset, source;
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, load { "LOAD" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    WaveView wave;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;
    juce::MidiKeyboardComponent keyboard;

    juce::String message;
    int messageTicks = 0, statusVersion = 0;
    int lastProgram = -1;
    juce::Rectangle<int> header, waveBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PollenEditor)
};
