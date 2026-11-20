#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour panel    { 0xffc9c6bd };
    const juce::Colour panelDk  { 0xffa9a69c };
    const juce::Colour ink      { 0xff2b2b2b };
    const juce::Colour knob     { 0xff3a3a3c };
    const juce::Colour accent   { 0xffd2452f };
    const juce::Colour lcdBg    { 0xff5f8f86 };
    const juce::Colour lcdInk   { 0xff10251f };
}

//==============================================================================
RackLookAndFeel::RackLookAndFeel()
{
    setColour (juce::Label::textColourId, Colours::ink);
    setColour (juce::ComboBox::backgroundColourId, Colours::lcdBg);
    setColour (juce::ComboBox::textColourId, Colours::lcdInk);
    setColour (juce::ComboBox::outlineColourId, Colours::ink.withAlpha (0.6f));
    setColour (juce::ComboBox::arrowColourId, Colours::lcdInk);
    setColour (juce::PopupMenu::backgroundColourId, Colours::lcdBg);
    setColour (juce::PopupMenu::textColourId, Colours::lcdInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::lcdInk);
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::lcdBg);
    setColour (juce::Slider::textBoxTextColourId, Colours::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ToggleButton::textColourId, Colours::ink);
}

void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // scale ticks
    g.setColour (Colours::ink.withAlpha (0.55f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto p1 = c.getPointOnCircumference (r, a);
        const auto p2 = c.getPointOnCircumference (r - 4.0f, a);
        g.drawLine ({ p1, p2 }, 1.2f);
    }

    // knob body
    const float kr = r - 7.0f;
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (c.x - kr + 1.5f, c.y - kr + 2.5f, kr * 2.0f, kr * 2.0f);
    g.setGradientFill (juce::ColourGradient (Colours::knob.brighter (0.35f), c.x, c.y - kr,
                                             Colours::knob.darker (0.4f), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);

    // pointer
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawLine ({ c.getPointOnCircumference (kr * 0.25f, angle), c.getPointOnCircumference (kr * 0.9f, angle) }, 2.5f);
}

void RackLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto area = b.getLocalBounds().toFloat();
    auto led = area.removeFromLeft (area.getHeight()).reduced (area.getHeight() * 0.28f);

    g.setColour (b.getToggleState() ? Colours::accent : Colours::accent.darker (0.85f).withAlpha (0.6f));
    g.fillEllipse (led);
    g.setColour (Colours::ink.withAlpha (0.6f));
    g.drawEllipse (led, 1.0f);

    g.setColour (Colours::ink);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), area.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
}

//==============================================================================
S2KEditor::S2KEditor (S2KProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (input,  "input",  "INPUT");
    setupKnob (bits,   "bits",   "BITS");
    setupKnob (cutoff, "cutoff", "FILTER");
    setupKnob (reso,   "reso",   "RESO");
    setupKnob (drive,  "drive",  "DRIVE");
    setupKnob (noise,  "noise",  "NOISE");
    setupKnob (output, "output", "OUTPUT");
    setupKnob (mix,    "mix",    "MIX");

    setupCombo (rateBox,   rateLabel,   "rate",   "SAMPLE RATE",   S2KProcessor::rateChoices,   rateAtt);
    setupCombo (interpBox, interpLabel, "interp", "INTERPOLATION", S2KProcessor::interpChoices, interpAtt);

    addAndMakeVisible (aaButton);
    addAndMakeVisible (dacButton);
    aaAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "aa", aaButton);
    dacAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "dacfilter", dacButton);

    setSize (760, 330);
    startTimerHz (20);
}

S2KEditor::~S2KEditor()
{
    setLookAndFeel (nullptr);
}

void S2KEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void S2KEditor::setupCombo (juce::ComboBox& box, juce::Label& label, const juce::String& paramId,
                            const juce::String& name, const juce::StringArray& items,
                            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
{
    box.addItemList (items, 1);
    addAndMakeVisible (box);
    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, box);
}

void S2KEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (Colours::panel.brighter (0.08f), 0, 0, Colours::panel.darker (0.06f), 0, b.getBottom(), false));
    g.fillAll();

    // rack ears with screw holes
    for (auto ear : { b.withWidth (26.0f), b.withLeft (b.getRight() - 26.0f) })
    {
        g.setColour (Colours::panelDk);
        g.fillRect (ear);
        g.setColour (Colours::ink.withAlpha (0.35f));
        for (float yy : { 28.0f, b.getBottom() - 28.0f })
            g.fillRoundedRectangle (ear.getCentreX() - 5.0f, yy - 4.0f, 10.0f, 8.0f, 4.0f);
    }

    // header
    g.setColour (Colours::ink);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("S2K", 44, 14, 80, 30, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("16-BIT CONVERTER EMULATION", 100, 20, 220, 20, juce::Justification::centredLeft);

    // LCD
    g.setColour (Colours::ink);
    g.fillRoundedRectangle (lcdArea.toFloat().expanded (3.0f), 4.0f);
    g.setColour (Colours::lcdBg);
    g.fillRect (lcdArea);

    auto* rateParam = proc.apvts.getParameter ("rate");
    auto* bitsParam = proc.apvts.getParameter ("bits");
    const auto lcdText = rateParam->getCurrentValueAsText() + "  "
                       + bitsParam->getCurrentValueAsText() + "-BIT  "
                       + proc.apvts.getParameter ("interp")->getCurrentValueAsText().toUpperCase();
    g.setColour (Colours::lcdInk);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::bold));
    g.drawText (lcdText, lcdArea.reduced (8, 0), juce::Justification::centredLeft);

    // clip LED
    g.setColour (clipHold > 0 ? juce::Colour (0xffff3020) : juce::Colour (0xff5a1a14));
    g.fillEllipse (ledArea.toFloat());
    g.setColour (Colours::ink);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("ADC CLIP", ledArea.getRight() + 6, ledArea.getY() - 3, 70, ledArea.getHeight() + 6, juce::Justification::centredLeft);

    // divider
    g.setColour (Colours::ink.withAlpha (0.25f));
    g.drawHorizontalLine (118, 40.0f, b.getRight() - 40.0f);
}

void S2KEditor::resized()
{
    auto area = getLocalBounds().reduced (40, 0);

    auto header = area.removeFromTop (60);
    lcdArea = header.withTrimmedLeft (330).withTrimmedRight (110).reduced (0, 14);
    ledArea = { header.getRight() - 96, header.getCentreY() - 6, 12, 12 };

    auto controls = area.removeFromTop (56).reduced (0, 4);
    const int w = controls.getWidth() / 4;
    auto place = [] (juce::Rectangle<int> r, juce::Label& l, juce::Component& c)
    {
        l.setBounds (r.removeFromTop (18));
        c.setBounds (r.removeFromTop (24).reduced (2, 0));
    };
    place (controls.removeFromLeft (w).reduced (6, 0), rateLabel, rateBox);
    place (controls.removeFromLeft (w).reduced (6, 0), interpLabel, interpBox);
    aaButton.setBounds (controls.removeFromLeft (w).reduced (10, 12));
    dacButton.setBounds (controls.reduced (10, 12));

    area.removeFromTop (12);
    auto knobs = area.reduced (0, 6);
    const int kw = knobs.getWidth() / 8;
    for (auto* k : { &input, &bits, &cutoff, &reso, &drive, &noise, &output, &mix })
    {
        auto r = knobs.removeFromLeft (kw);
        k->label.setBounds (r.removeFromTop (18));
        k->slider.setBounds (r.withHeight (juce::jmin (r.getHeight(), 130)));
    }
}

void S2KEditor::timerCallback()
{
    if (proc.clipIndicator.exchange (false))
        clipHold = 8;
    else if (clipHold > 0)
        --clipHold;

    repaint (lcdArea.expanded (4));
    repaint (ledArea.expanded (2).withWidth (90));
}
