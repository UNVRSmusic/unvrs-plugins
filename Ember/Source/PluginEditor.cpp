#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour bg      { 0xff141110 };
    const juce::Colour panel   { 0xff1e1917 };
    const juce::Colour panelHi { 0xff2a2320 };
    const juce::Colour line    { 0xff3a302b };
    const juce::Colour text    { 0xfff3e7dc };
    const juce::Colour textDim { 0xff9c8a7e };
    const juce::Colour ember   { 0xffff6a2b };
    const juce::Colour glow    { 0xffffb347 };
}

static juce::Font sans (float size, bool bold = false)
{
    return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
}

static constexpr float fMin = 50.0f, fMax = 18000.0f, wMin = 0.5f, wMax = 9.0f;

//==============================================================================
EmberLookAndFeel::EmberLookAndFeel()
{
    setColour (juce::Label::textColourId, Pal::textDim);
    setColour (juce::Slider::textBoxTextColourId, Pal::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, Pal::panelHi);
    setColour (juce::TooltipWindow::textColourId, Pal::text);
    setColour (juce::TooltipWindow::outlineColourId, Pal::line);
}

void EmberLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const bool big = r > 60.0f;
    const float thick = big ? 6.0f : 3.0f;
    const float arcR = r - thick;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::line);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        const auto col = Pal::ember.interpolatedWith (Pal::glow, pos);
        if (big)
        {
            g.setColour (col.withAlpha (0.25f));
            g.strokePath (value, juce::PathStrokeType (thick * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (col);
        g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = arcR - thick - (big ? 8.0f : 5.0f);
    g.setGradientFill (juce::ColourGradient (Pal::panelHi.brighter (0.2f), c.x, c.y - kr, Pal::bg, c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    g.setColour (Pal::text);
    g.drawLine ({ c.getPointOnCircumference (kr * 0.35f, angle), c.getPointOnCircumference (kr * 0.88f, angle) }, big ? 3.0f : 2.0f);

    juce::ignoreUnused (s);
}

//==============================================================================
FilterPad::FilterPad (juce::AudioProcessorValueTreeState& state)
    : apvts (state), freq (*state.getParameter ("freq")), width (*state.getParameter ("width"))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void FilterPad::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (bounds, 10.0f);

    const auto plot = plotArea();
    const float f0 = freq.convertFrom0to1 (freq.getValue());
    const float w = width.convertFrom0to1 (width.getValue());
    const double fl = f0 / std::pow (2.0, w * 0.5), fh = f0 * std::pow (2.0, w * 0.5);

    auto xFor = [&] (double f) { return plot.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * plot.getWidth(); };
    auto yForDb = [&] (double db) { return plot.getY() + (float) (juce::jlimit (0.0, 1.0, -db / 36.0)) * plot.getHeight(); };

    g.setFont (sans (9.5f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xFor (f);
        g.setColour (Pal::line.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (Pal::textDim);
        g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f),
                    (int) x - 20, (int) plot.getBottom() + 1, 40, 12, juce::Justification::centred);
    }

    // band-pass magnitude (2nd-order HP x 2nd-order LP)
    juce::Path curve, fill;
    for (int i = 0; i <= 200; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 200.0);
        const double a = f / fl, b = f / fh;
        const double hp = a * a / std::sqrt ((1 - a * a) * (1 - a * a) + 2 * a * a);
        const double lp = 1.0 / std::sqrt ((1 - b * b) * (1 - b * b) + 2 * b * b);
        const float x = xFor (f), y = yForDb (20.0 * std::log10 (hp * lp + 1.0e-9));
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getBottom()); }
        else          curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (Pal::ember.withAlpha (0.35f), 0.0f, plot.getY(), Pal::ember.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (Pal::ember);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // handle
    const float hx = xFor (f0);
    const float hy = plot.getY() + (w - wMin) / (wMax - wMin) * plot.getHeight();
    g.setColour (Pal::glow.withAlpha (0.3f));
    g.fillEllipse (hx - 12.0f, hy - 12.0f, 24.0f, 24.0f);
    g.setColour (Pal::glow);
    g.fillEllipse (hx - 6.0f, hy - 6.0f, 12.0f, 12.0f);

    g.setColour (Pal::textDim);
    g.setFont (sans (10.0f, true));
    g.drawText ("FILTER  " + freq.getCurrentValueAsText() + "  /  " + width.getCurrentValueAsText(),
                plot.toNearestInt().reduced (8, 4).removeFromTop (14), juce::Justification::centredRight);
}

void FilterPad::setFromMouse (juce::Point<float> p)
{
    const auto plot = plotArea();
    const double fx = juce::jlimit (0.0f, 1.0f, (p.x - plot.getX()) / plot.getWidth());
    const double f = juce::jlimit ((double) fMin, (double) fMax, 20.0 * std::pow (1000.0, fx));
    const float wy = juce::jlimit (0.0f, 1.0f, (p.y - plot.getY()) / plot.getHeight());
    freq.setValueNotifyingHost (freq.convertTo0to1 ((float) f));
    width.setValueNotifyingHost (width.convertTo0to1 (wMin + wy * (wMax - wMin)));
    repaint();
}

void FilterPad::mouseDown (const juce::MouseEvent& e)
{
    freq.beginChangeGesture();
    width.beginChangeGesture();
    setFromMouse (e.position);
}

void FilterPad::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void FilterPad::mouseUp (const juce::MouseEvent&)
{
    freq.endChangeGesture();
    width.endChangeGesture();
}

void FilterPad::mouseDoubleClick (const juce::MouseEvent&)
{
    for (auto* p : { &freq, &width })
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
    repaint();
}

//==============================================================================
EmberEditor::EmberEditor (EmberProcessor& p)
    : AudioProcessorEditor (p), proc (p), pad (p.apvts)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (pad);
    pad.setTooltip ("Drag: left/right = band centre, up/down = band width (top narrow, bottom wide). Double-click resets.");

    setupKnob (freq,     "freq",     "FREQ",     "Centre of the band that goes into the drive.");
    setupKnob (width,    "width",    "WIDTH",    "Width of that band in octaves.");
    setupKnob (drive,    "drive",    "DRIVE",    "Amount of distortion. Even 0 % adds a little grit.");
    setupKnob (tone,     "tone",     "TONE",     "Post-distortion tone: left darker, right brighter.");
    setupKnob (dynamics, "dynamics", "DYNAMICS", "Low: more drive = more compression and make-up gain.\nHigh: the input's dynamics come through even at full drive.");
    setupKnob (mix,      "mix",      "DRY/WET",  "Balance of clean and driven signal. Use 100 % on a return.");
    setupKnob (output,   "output",   "OUTPUT",   "Output level.");
    drive.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);

    setSize (860, 450);
    startTimerHz (30);
}

EmberEditor::~EmberEditor()
{
    setLookAndFeel (nullptr);
}

void EmberEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name, const juce::String& tip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    k.slider.setTooltip (tip);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (sans (11.0f, true));
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void EmberEditor::timerCallback()
{
    heat = juce::jmax (proc.driveMeter.load(), heat * 0.88f);
    pad.repaint();
    repaint (transferArea);
    repaint (driveArea);
}

void EmberEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg);

    g.setColour (Pal::text);
    g.setFont (sans (26.0f, true));
    g.drawText ("EMBER", header.withWidth (120), juce::Justification::centredLeft);
    g.setColour (Pal::ember);
    g.setFont (sans (12.0f, true));
    g.drawText ("OVERDRIVE", header.withTrimmedLeft (112).withWidth (100).translated (0, 3), juce::Justification::centredLeft);

    // drive glow behind the big knob
    {
        const auto c = drive.slider.getBounds().toFloat().getCentre().translated (0.0f, -8.0f);
        const float rad = 120.0f;
        g.setGradientFill (juce::ColourGradient (Pal::ember.withAlpha (0.08f + 0.35f * heat), c.x, c.y,
                                                 Pal::ember.withAlpha (0.0f), c.x + rad, c.y, true));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);
    }

    g.setColour (Pal::panel);
    g.fillRoundedRectangle (bottomArea.toFloat(), 10.0f);
    paintTransfer (g);
}

void EmberEditor::paintTransfer (juce::Graphics& g)
{
    const auto area = transferArea.toFloat();
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (area, 10.0f);

    const auto plot = area.reduced (14.0f).withTrimmedTop (12.0f);
    g.setColour (Pal::line);
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());
    g.drawVerticalLine (juce::roundToInt (plot.getCentreX()), plot.getY(), plot.getBottom());

    const float gain = EmberProcessor::driveGain (proc.apvts.getRawParameterValue ("drive")->load());
    const float off = EmberProcessor::clip (EmberProcessor::bias);
    juce::Path p;
    for (int i = 0; i <= 120; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / 120.0f;
        float y = EmberProcessor::clip (gain * x * 0.25f + EmberProcessor::bias) - off;
        y = std::tanh (1.3f * y) / 0.8617f;
        const float px = plot.getX() + (x + 1.0f) * 0.5f * plot.getWidth();
        const float py = plot.getCentreY() - y * 0.42f * plot.getHeight();
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (Pal::glow.withAlpha (0.25f + 0.5f * heat));
    g.strokePath (p, juce::PathStrokeType (6.0f));
    g.setColour (Pal::glow);
    g.strokePath (p, juce::PathStrokeType (2.0f));

    g.setColour (Pal::textDim);
    g.setFont (sans (10.0f, true));
    g.drawText ("CURVE", area.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::centredLeft);
}

void EmberEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 0);
    header = b.removeFromTop (54).withTrimmedTop (6);

    auto left = b.removeFromLeft (430);
    b.removeFromLeft (14);

    padArea = left.removeFromTop (258);
    pad.setBounds (padArea);
    left.removeFromTop (10);
    auto fw = left.removeFromTop (110);
    for (auto* k : { &freq, &width })
    {
        auto col = fw.removeFromLeft (fw.getWidth() / (k == &freq ? 2 : 1));
        col = col.withSizeKeepingCentre (110, col.getHeight());
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }

    auto top = b.removeFromTop (258);
    transferArea = top.removeFromRight (170).withTrimmedTop (0);
    top.removeFromRight (10);
    driveArea = top;
    drive.label.setBounds (driveArea.removeFromTop (16));
    drive.slider.setBounds (driveArea.reduced (6, 0));
    driveArea = top;

    b.removeFromTop (10);
    bottomArea = b.removeFromTop (110);
    auto row = bottomArea.reduced (8, 6);
    const int w = row.getWidth() / 4;
    for (auto* k : { &tone, &dynamics, &mix, &output })
    {
        auto col = row.removeFromLeft (w);
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }
}
