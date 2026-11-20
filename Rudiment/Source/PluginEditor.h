#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class RudimentLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RudimentLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

//==============================================================================
class StepGrid final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (RudimentProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void (int)> onSelect;
    int selected = 0;

private:
    enum class Zone { none, name, mute, length, step };
    Zone zoneAt (juce::Point<int>, int& row, int& col) const;
    juce::Rectangle<float> cellRect (int row, int col) const;
    juce::Rectangle<float> zoneRect (Zone, int row) const;
    float rowH() const { return (float) getHeight() / rud::numElements; }
    float stepsX() const { return nameW + muteW + lenW + 10.0f; }
    float cellW() const { return ((float) getWidth() - stepsX()) / rud::numSteps; }

    static constexpr float nameW = 118.0f, muteW = 30.0f, lenW = 40.0f;

    RudimentProcessor& proc;
    Zone dragZone = Zone::none;
    int dragRow = -1, lenStart = 16;
    uint8_t paintValue = 0;
};

//==============================================================================
class RudimentEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RudimentEditor (RudimentProcessor&);
    ~RudimentEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void select (int element);
    void updatePreview (bool force);
    void paintWave (juce::Graphics&);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    Knob* makeKnob (std::vector<std::unique_ptr<Knob>>& into, const juce::String& id, const juce::String& name, const juce::String& tip, bool centred = false);

    RudimentProcessor& proc;
    RudimentLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::ComboBox kit, patternBox;
    juce::ToggleButton run { "PLAY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAtt;

    StepGrid grid;
    std::array<std::vector<std::unique_ptr<Knob>>, rud::numElements> elementKnobs;
    std::vector<std::unique_ptr<Knob>> masterKnobs;

    juce::Rectangle<int> header, gridArea, elementPanel, waveArea, knobRow, masterPanel;
    int selected = 0, lastKit = -1;

    std::vector<float> preview;
    std::vector<float> previewParams;
    int previewElement = -1;
    double previewSeconds = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RudimentEditor)
};
