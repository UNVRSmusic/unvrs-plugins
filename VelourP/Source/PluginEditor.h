#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

class VelourPLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VelourPLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

/** EQ curve with draggable band nodes and a spectrum analyser behind it. */
class CurveView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit CurveView (VelourPProcessor&);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Pulls new audio from the processor and updates the spectrum (call from a timer). */
    void updateAnalyser();

    std::function<void (int)> onSelect; // band index, or -1/-2 for HPF/LPF

private:
    static constexpr int hpfNode = -1, lpfNode = -2, noNode = -100;

    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (12.0f, 10.0f).withTrimmedLeft (24.0f).withTrimmedBottom (14.0f); }
    float xFor (double f) const;
    double fAt (float x) const;
    float yFor (double db) const;
    double dbAt (float y) const;
    juce::Point<float> nodePos (int node) const;
    int hitNode (juce::Point<float>) const;
    juce::RangedAudioParameter* param (const juce::String& id) const { return proc.apvts.getParameter (id); }
    void setParam (const juce::String& id, float value);

    VelourPProcessor& proc;
    int hover = noNode, dragging = noNode;

    // analyser
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring = std::vector<float> (fftSize, 0.0f);
    std::vector<float> fftData = std::vector<float> (2 * fftSize, 0.0f);
    std::vector<float> spectrum; // per pixel, dB
    int ringPos = 0;
};

class VelourPEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VelourPEditor (VelourPProcessor&);
    ~VelourPEditor() override;

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
    struct BandStrip
    {
        juce::ToggleButton on;
        juce::ComboBox type;
        Knob gain, freq, q;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
        juce::Rectangle<int> area;
    };

    void setupKnob (Knob&, const juce::String& id, const juce::String& name, juce::Colour accent, const juce::String& tip = {});
    void setupCombo (juce::ComboBox&, const juce::String& id, const juce::StringArray& items,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);

    VelourPProcessor& proc;
    VelourPLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    CurveView curve;

    std::array<BandStrip, VelourPProcessor::numBands> strips;
    Knob hpf, lpf, iron, drive, output;
    juce::ComboBox hpfSlope, lpfSlope;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hpfSlopeAtt, lpfSlopeAtt;
    juce::ToggleButton analog { "ANALOG" }, consoleQ { "CONSOLE Q" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> analogAtt, consoleQAtt;

    juce::Rectangle<int> header, lamp, filterArea, charArea;
    int selected = -100;
    float glow = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelourPEditor)
};
