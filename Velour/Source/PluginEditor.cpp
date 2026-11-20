#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour plate    { 0xffe7ddcb };
    const juce::Colour plateDk  { 0xffd8ccb6 };
    const juce::Colour edge     { 0xffb9aa90 };
    const juce::Colour ink      { 0xff2b2420 };
    const juce::Colour inkDim   { 0xff76695a };
    const juce::Colour wine     { 0xff7d2a2a };
    const juce::Colour screen   { 0xfff6f1e6 };
    const juce::Colour lampOn   { 0xffff9a3c };
}

static juce::Font serif (float size, bool bold = false)
{
    return juce::FontOptions ("Georgia", size, bold ? juce::Font::bold : juce::Font::plain);
}

static juce::Font sans (float size, bool bold = false)
{
    return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
}

//==============================================================================
VelourLookAndFeel::VelourLookAndFeel()
{
    setColour (juce::Label::textColourId, Pal::ink);
    setColour (juce::Slider::textBoxTextColourId, Pal::inkDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Pal::wine.withAlpha (0.3f));
    setColour (juce::TextEditor::textColourId, Pal::ink);
    setColour (juce::CaretComponent::caretColourId, Pal::wine);
    setColour (juce::TooltipWindow::backgroundColourId, Pal::screen);
    setColour (juce::TooltipWindow::textColourId, Pal::ink);
    setColour (juce::TooltipWindow::outlineColourId, Pal::edge);
}

void VelourLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const int steps = (int) s.getProperties().getWithDefault ("steps", 0);

    // scale ticks: one per step for selector knobs, 0..10 for the rest
    const int ticks = steps > 0 ? steps - 1 : 10;
    g.setColour (Pal::ink.withAlpha (0.7f));
    for (int i = 0; i <= ticks; ++i)
    {
        const float a = startAngle + (float) i / (float) ticks * (endAngle - startAngle);
        const bool major = steps > 0 || i % 5 == 0;
        g.drawLine ({ c.getPointOnCircumference (r, a), c.getPointOnCircumference (r - (major ? 6.0f : 3.5f), a) }, major ? 1.4f : 1.0f);
    }

    // bakelite knob: skirt + cap
    const float kr = r - 9.0f;
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillEllipse (c.x - kr + 1.0f, c.y - kr + 3.0f, kr * 2.0f, kr * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3532), c.x, c.y - kr, juce::Colour (0xff0f0d0c), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);

    const float cr = kr * 0.68f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4440), c.x - cr * 0.4f, c.y - cr, juce::Colour (0xff141211), c.x + cr * 0.4f, c.y + cr, false));
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 1.0f);

    // pointer line on the skirt
    g.setColour (Pal::plate);
    g.drawLine ({ c.getPointOnCircumference (cr * 0.2f, angle), c.getPointOnCircumference (kr - 1.5f, angle) }, 2.2f);
}

void VelourLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? Pal::wine : Pal::plateDk);
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (highlighted ? Pal::ink : Pal::edge);
    g.drawRoundedRectangle (area, 4.0f, 1.0f);

    g.setColour (on ? Pal::plate : Pal::inkDim);
    g.setFont (sans (11.5f, true));
    g.drawText (b.getButtonText(), area, juce::Justification::centred);
}

//==============================================================================
VelourEditor::VelourEditor (VelourProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (hpf,       "hpf",       "HIGH-PASS", "Gentle 12 dB/oct low cut. Fully left = off.");
    setupKnob (lowFreq,   "lowFreq",   "FREQ",      "Low section frequency (Hz).", VelourProcessor::lowFreqs.size());
    setupKnob (lowBoost,  "lowBoost",  "BOOST",     "Low shelf boost with a slight resonant bump.");
    setupKnob (lowAtten,  "lowAtten",  "ATTEN",     "Low cut, set a little above the boost frequency.\n"
                                                    "Boost + atten together = full bottom with a tighter low-mid.");
    setupKnob (midFreq,   "midFreq",   "FREQ",      "Mid bell frequency.");
    setupKnob (midGain,   "midGain",   "GAIN",      "Mid boost / cut. Proportional Q: small moves are wide, big moves get narrower.");
    setupKnob (highFreq,  "highFreq",  "FREQ",      "High boost frequency.", VelourProcessor::highFreqs.size());
    setupKnob (highBoost, "highBoost", "BOOST",     "High bell boost.");
    setupKnob (highBw,    "highBw",    "BANDWIDTH", "Sharp (focused presence) to broad (open air).");
    setupKnob (attenFreq, "attenFreq", "FREQ",      "High attenuation shelf frequency.", VelourProcessor::attenFreqs.size());
    setupKnob (highAtten, "highAtten", "ATTEN",     "High shelf cut. Boost + atten together = sheen without harshness.");
    setupKnob (iron,      "iron",      "IRON",      "Transformer: low end thickens and saturates first, top end rounds off.");
    setupKnob (drive,     "drive",     "DRIVE",     "Tube make-up stage after the EQ (even harmonics, soft compression).\n"
                                                    "Everything you boost hits it harder.");
    setupKnob (output,    "output",    "OUTPUT",    "Output level.");

    analog.setTooltip ("Analog path (transformer + tube). Off = clean EQ only.");
    addAndMakeVisible (analog);
    analogAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "analog", analog);

    setSize (1150, 480);
    startTimerHz (30);
}

VelourEditor::~VelourEditor()
{
    setLookAndFeel (nullptr);
}

void VelourEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name, const juce::String& tip, int steps)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    k.slider.setTooltip (tip);
    if (steps > 0)
        k.slider.getProperties().set ("steps", steps);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (sans (10.5f, true));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void VelourEditor::timerCallback()
{
    const bool analogOn = analog.getToggleState();
    glow = analogOn ? juce::jmax (proc.driveActivity.load(), glow * 0.9f) : glow * 0.8f;
    repaint (lamp.expanded (12));

    // only rebuild the curve when a parameter moved
    juce::String key;
    for (auto* prm : proc.getParameters())
        key << prm->getValue() << ";";
    if (key != lastKey)
    {
        lastKey = key;
        repaint (display);
    }
}

//==============================================================================
void VelourEditor::paint (juce::Graphics& g)
{
    // faceplate with a subtle brushed gradient
    g.setGradientFill (juce::ColourGradient (Pal::plate.brighter (0.04f), 0.0f, 0.0f, Pal::plate.darker (0.05f), 0.0f, (float) getHeight(), false));
    g.fillAll();
    g.setColour (Pal::edge);
    g.drawRect (getLocalBounds(), 2);

    // header: logo and lamp
    g.setColour (Pal::wine);
    g.setFont (serif (30.0f, true));
    g.drawText ("Velour", header.withWidth (130), juce::Justification::centredLeft);
    g.setColour (Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText ("COLOURED EQUALISER", header.withTrimmedLeft (118).withWidth (200).translated (0, 4), juce::Justification::centredLeft);

    {
        const auto l = lamp.toFloat();
        g.setColour (Pal::lampOn.withAlpha (0.15f + 0.5f * glow));
        g.fillEllipse (l.expanded (6.0f + 6.0f * glow));
        g.setGradientFill (juce::ColourGradient (Pal::lampOn.interpolatedWith (juce::Colours::white, 0.5f).withAlpha (0.4f + 0.6f * glow), l.getCentreX(), l.getY(),
                                                 Pal::wine.withAlpha (0.6f + 0.4f * glow), l.getCentreX(), l.getBottom(), false));
        g.fillEllipse (l);
        g.setColour (Pal::ink.withAlpha (0.6f));
        g.drawEllipse (l, 1.2f);
    }

    // sections
    for (const auto& s : sections)
    {
        const auto r = s.area.toFloat();
        g.setColour (Pal::plateDk);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Pal::edge);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (Pal::wine);
        g.setFont (serif (13.0f, true));
        g.drawText (s.title, s.area.withHeight (24), juce::Justification::centred);
    }

    paintDisplay (g);
}

void VelourEditor::paintDisplay (juce::Graphics& g)
{
    const auto area = display.toFloat();
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (Pal::edge);
    g.drawRoundedRectangle (area, 6.0f, 1.2f);

    const auto plot = area.reduced (14.0f, 10.0f).withTrimmedLeft (22.0f).withTrimmedBottom (12.0f);
    constexpr double range = 18.0;
    auto xFor = [&] (double f) { return plot.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * plot.getWidth(); };
    auto yFor = [&] (double db) { return plot.getCentreY() - (float) (juce::jlimit (-range, range, db) / range) * plot.getHeight() * 0.5f; };

    g.setFont (sans (9.5f));
    for (double f : { 30.0, 60.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xFor (f);
        g.setColour (Pal::edge.withAlpha (0.45f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (Pal::inkDim);
        g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 20, (int) plot.getBottom() + 1, 40, 11, juce::Justification::centred);
    }
    for (double db : { -12.0, -6.0, 0.0, 6.0, 12.0 })
    {
        const float y = yFor (db);
        g.setColour (db == 0.0 ? Pal::inkDim.withAlpha (0.6f) : Pal::edge.withAlpha (0.45f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (Pal::inkDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) area.getX() + 4, (int) y - 6, 28, 12, juce::Justification::centredRight);
    }

    const double rate = juce::jmax (44100.0, proc.getSampleRate()) * 4.0;
    const auto coefs = VelourProcessor::designEq (proc.readEq(), rate);
    juce::Path fill;
    curve.clear();
    for (int i = 0; i <= 240; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 240.0);
        const float x = xFor (f), y = yFor (VelourProcessor::magnitudeDb (coefs, f, rate));
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, yFor (0.0)); }
        else          curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (plot.getRight(), yFor (0.0));
    fill.closeSubPath();

    g.setColour (Pal::wine.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (Pal::wine);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void VelourEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 0);
    header = b.removeFromTop (54).withTrimmedTop (6);

    {
        auto h = header;
        analog.setBounds (h.removeFromRight (96).withSizeKeepingCentre (96, 28));
        h.removeFromRight (16);
        lamp = h.removeFromRight (22).withSizeKeepingCentre (22, 22);
    }

    display = b.removeFromTop (150);
    b.removeFromTop (12);
    auto row = b.removeFromTop (b.getHeight() - 14);

    sections.clear();
    constexpr int kw = 72, pad = 10, gap = 8;
    auto addSection = [&] (const juce::String& title, std::initializer_list<Knob*> knobs)
    {
        const int w = (int) knobs.size() * kw + 2 * pad;
        auto s = row.removeFromLeft (w);
        row.removeFromLeft (gap);
        sections.push_back ({ title, s });

        auto inner = s.reduced (pad, 8).withTrimmedTop (22);
        for (auto* k : knobs)
        {
            auto col = inner.removeFromLeft (kw);
            k->label.setBounds (col.removeFromTop (14));
            k->slider.setBounds (col.withSizeKeepingCentre (kw, juce::jmin (col.getHeight(), 104)));
        }
    };

    addSection ("Filter",     { &hpf });
    addSection ("Low",        { &lowFreq, &lowBoost, &lowAtten });
    addSection ("Mid",        { &midFreq, &midGain });
    addSection ("High",       { &highFreq, &highBoost, &highBw });
    addSection ("High Atten", { &attenFreq, &highAtten });
    addSection ("Character",  { &iron, &drive, &output });
}
