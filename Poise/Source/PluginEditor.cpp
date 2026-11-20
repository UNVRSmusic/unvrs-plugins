#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour bg      { 0xff14161a };
    const juce::Colour panel   { 0xff1c1f25 };
    const juce::Colour screen  { 0xff191c21 };
    const juce::Colour knob    { 0xff2a2e36 };
    const juce::Colour line    { 0xff30353e };
    const juce::Colour ink     { 0xffe4e7ec };
    const juce::Colour inkDim  { 0xff8a909b };
    const juce::Colour teal    { 0xff3ccabf };
    const juce::Colour coral   { 0xffff7a5c };
    const juce::Colour input   { 0xff6f7682 };
}

static juce::Font sans (float size, bool bold = false) { return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain); }

//==============================================================================
PoiseLookAndFeel::PoiseLookAndFeel()
{
    setColour (juce::Label::textColourId, Pal::inkDim);
    setColour (juce::Slider::textBoxTextColourId, Pal::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Pal::teal.withAlpha (0.3f));
    setColour (juce::TextEditor::textColourId, Pal::ink);
    setColour (juce::CaretComponent::caretColourId, Pal::teal);
    setColour (juce::ComboBox::textColourId, Pal::ink);
    setColour (juce::PopupMenu::backgroundColourId, Pal::bg);
    setColour (juce::PopupMenu::textColourId, Pal::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Pal::teal);
    setColour (juce::PopupMenu::highlightedTextColourId, Pal::bg);
    setColour (juce::TextButton::textColourOffId, Pal::ink);
    setColour (juce::TextButton::textColourOnId, Pal::bg);
    setColour (juce::TooltipWindow::backgroundColourId, Pal::bg);
    setColour (juce::TooltipWindow::textColourId, Pal::ink);
    setColour (juce::TooltipWindow::outlineColourId, Pal::line);
}

void PoiseLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (5.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool centred = (bool) s.getProperties().getWithDefault ("centred", false);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = centred ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path v;
        v.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (Pal::teal);
        g.strokePath (v, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = r - 8.0f;
    g.setColour (Pal::knob);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);
    g.setColour (Pal::ink);
    g.drawLine ({ c.getPointOnCircumference (kr * 0.3f, angle), c.getPointOnCircumference (kr * 0.9f, angle) }, 2.0f);
}

void PoiseLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::teal : Pal::panel);
    g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
    g.setColour (highlighted ? Pal::ink : Pal::line);
    g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.0f);
    g.setColour (on ? Pal::bg : Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText (b.getButtonText(), area, juce::Justification::centred);
}

void PoiseLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::coral : (down ? Pal::line : Pal::panel));
    g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
    g.setColour (highlighted || on ? Pal::ink : Pal::line);
    g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.0f);
}

void PoiseLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, h * 0.5f);
    g.setColour (box.isMouseOver (true) ? Pal::ink : Pal::line);
    g.drawRoundedRectangle (area, h * 0.5f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (Pal::inkDim);
    g.fillPath (arrow);
}

juce::Font PoiseLookAndFeel::getComboBoxFont (juce::ComboBox&) { return sans (12.0f, true); }

//==============================================================================
PoiseEditor::PoiseEditor (PoiseProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (amount,     "amount",     "AMOUNT",         "How much of the measured imbalance gets corrected.");
    setupKnob (keep,       "keep",       "KEEP CHARACTER", "0 %: steer toward the target curve.\n100 %: keep the track's own tonal balance and only smooth out moments that drift from it.");
    setupKnob (speed,      "speed",      "SPEED",          "Reaction time. Slow sounds like mixing, fast like multiband compression.");
    setupKnob (resolution, "resolution", "RESOLUTION",     "Width of the corrections in octaves. Wider keeps instrument timbre intact.");
    setupKnob (range,      "range",      "RANGE",          "Maximum correction in either direction.");
    setupKnob (tilt,       "tilt",       "TILT",           "Tilts the target darker (left) or brighter (right).", true);
    setupKnob (lowLimit,   "lowLimit",   "LOW LIMIT",      "Nothing below this frequency is touched.");
    setupKnob (highLimit,  "highLimit",  "HIGH LIMIT",     "Nothing above this frequency is touched.");
    setupKnob (mix,        "mix",        "MIX",            "Scales all corrections.");
    setupKnob (output,     "output",     "OUTPUT",         "Output level.", true);

    target.addItemList (PoiseProcessor::targetChoices, 1);
    target.setTooltip ("Pink: equal energy per octave. Mix: pink with a gentle top-end roll-off, like most finished records.\n"
                       "Warm / Bright: tilted versions. Learned: the balance captured with LEARN.");
    mode.addItemList (PoiseProcessor::modeChoices, 1);
    for (auto* c : { &target, &mode })
    {
        c->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*c);
    }
    targetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "target", target);
    modeAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mode", mode);

    autoGain.setTooltip ("Keeps the overall level unchanged, so you hear balance and not loudness.");
    addAndMakeVisible (autoGain);
    autoGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "autoGain", autoGain);

    learn.setClickingTogglesState (true);
    learn.setTooltip ("Play a reference track you love, press LEARN, let it run through a chorus or two, press again.\nThe target switches to Learned.");
    learn.onClick = [this]
    {
        const bool on = learn.getToggleState();
        proc.engine.learning = on;
        if (! on)
        {
            // the engine finalises on its next frame; select the learned target
            juce::Timer::callAfterDelay (300, [safe = juce::Component::SafePointer<PoiseEditor> (this)]
            {
                if (safe != nullptr && safe->proc.engine.hasLearned.load())
                    if (auto* prm = safe->proc.apvts.getParameter ("target"))
                    {
                        prm->beginChangeGesture();
                        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) RebalanceEngine::learned));
                        prm->endChangeGesture();
                    }
            });
        }
    };
    learn.setToggleState (proc.engine.learning.load(), juce::dontSendNotification);
    addAndMakeVisible (learn);

    setSize (1060, 610);
    startTimerHz (30);
}

PoiseEditor::~PoiseEditor()
{
    setLookAndFeel (nullptr);
}

void PoiseEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name, const juce::String& tip, bool centred)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 16);
    k.slider.setTooltip (tip);
    if (centred) k.slider.getProperties().set ("centred", true);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (sans (10.0f, true));
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void PoiseEditor::timerCallback()
{
    auto& e = proc.engine;
    for (int b = 0; b < e.numBands; ++b)
    {
        level[(size_t) b] += (e.guiLevel[(size_t) b].load() - level[(size_t) b]) * 0.35f;
        targetCurve[(size_t) b] = e.guiTarget[(size_t) b].load();
        gain[(size_t) b] += (e.guiGain[(size_t) b].load() - gain[(size_t) b]) * 0.35f;
    }
    blink += 0.15f;
    learn.setButtonText (learn.getToggleState() ? (std::fmod (blink, 1.0f) < 0.6f ? "LEARNING..." : "") : "LEARN");
    repaint (balanceArea);
    repaint (correctionArea);
}

//==============================================================================
void PoiseEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg);

    g.setColour (Pal::ink);
    g.setFont (sans (26.0f, true));
    g.drawText ("POISE", header.withWidth (110), juce::Justification::centredLeft);
    g.setColour (Pal::teal);
    g.setFont (sans (11.0f, true));
    g.drawText ("SPECTRAL REBALANCER", header.withTrimmedLeft (104).withWidth (180).translated (0, 3), juce::Justification::centredLeft);
    g.setColour (Pal::inkDim);
    g.setFont (sans (10.0f, true));
    g.drawText ("UNVRS", header.withTrimmedLeft (270).withWidth (60).translated (0, 3), juce::Justification::centredLeft);

    g.setColour (Pal::panel);
    g.fillRoundedRectangle (knobArea.toFloat(), 10.0f);

    paintBalance (g);
    paintCorrection (g);
}

static float xForFreq (juce::Rectangle<float> r, double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); }

void PoiseEditor::paintBalance (juce::Graphics& g)
{
    const auto area = balanceArea.toFloat();
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (Pal::line);
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);

    const auto pl = area.reduced (14.0f, 12.0f).withTrimmedLeft (24.0f).withTrimmedBottom (12.0f);
    constexpr float range = 15.0f;
    auto yFor = [&] (float db) { return pl.getCentreY() - juce::jlimit (-range, range, db) / range * pl.getHeight() * 0.5f; };

    g.setFont (sans (9.5f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xForFreq (pl, f);
        g.setColour (Pal::line.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), pl.getY(), pl.getBottom());
        g.setColour (Pal::inkDim);
        g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 20, (int) pl.getBottom() + 1, 40, 11, juce::Justification::centred);
    }
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        g.setColour (db == 0.0f ? Pal::line : Pal::line.withAlpha (0.5f));
        g.drawHorizontalLine (juce::roundToInt (yFor (db)), pl.getX(), pl.getRight());
        g.setColour (Pal::inkDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) area.getX() + 2, (int) yFor (db) - 6, 30, 12, juce::Justification::centredRight);
    }

    // limits shading
    const float lo = proc.apvts.getRawParameterValue ("lowLimit")->load(), hi = proc.apvts.getRawParameterValue ("highLimit")->load();
    g.setColour (Pal::bg.withAlpha (0.8f));
    g.fillRect (juce::Rectangle<float> (pl.getX(), pl.getY(), juce::jmax (0.0f, xForFreq (pl, lo) - pl.getX()), pl.getHeight()));
    const float xh = xForFreq (pl, hi);
    g.fillRect (juce::Rectangle<float> (xh, pl.getY(), juce::jmax (0.0f, pl.getRight() - xh), pl.getHeight()));

    const auto& e = proc.engine;
    if (e.numBands < 2)
        return;

    auto pathOf = [&] (auto valueOf)
    {
        juce::Path p;
        for (int b = 0; b < e.numBands; ++b)
        {
            const juce::Point<float> pt (xForFreq (pl, e.centres[(size_t) b]), yFor (valueOf (b)));
            if (b == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    };

    // input balance (filled), target (dashed), corrected result (teal)
    auto in = pathOf ([&] (int b) { return level[(size_t) b]; });
    auto fill = in;
    fill.lineTo (xForFreq (pl, e.centres[(size_t) (e.numBands - 1)]), pl.getBottom());
    fill.lineTo (xForFreq (pl, e.centres[0]), pl.getBottom());
    fill.closeSubPath();
    g.setColour (Pal::input.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (Pal::input);
    g.strokePath (in, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

    auto tgt = pathOf ([&] (int b) { return targetCurve[(size_t) b]; });
    juce::Path dashed;
    const float dashes[] { 6.0f, 4.0f };
    juce::PathStrokeType (1.4f).createDashedStroke (dashed, tgt, dashes, 2);
    g.setColour (Pal::ink.withAlpha (0.7f));
    g.fillPath (dashed);

    auto outP = pathOf ([&] (int b) { return level[(size_t) b] + gain[(size_t) b]; });
    g.setColour (Pal::teal);
    g.strokePath (outP, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // legend
    auto lg = area.reduced (16.0f, 10.0f).removeFromTop (14.0f).removeFromRight (330.0f);
    auto item = [&] (juce::Colour c, const juce::String& t, float w, bool dash)
    {
        auto r = lg.removeFromLeft (w);
        g.setColour (c);
        if (dash) { g.drawLine (r.getX(), r.getCentreY(), r.getX() + 6, r.getCentreY(), 1.5f); g.drawLine (r.getX() + 9, r.getCentreY(), r.getX() + 15, r.getCentreY(), 1.5f); }
        else        g.drawLine (r.getX(), r.getCentreY(), r.getX() + 15, r.getCentreY(), 2.5f);
        g.setColour (Pal::inkDim);
        g.setFont (sans (10.0f, true));
        g.drawText (t, r.withTrimmedLeft (20.0f), juce::Justification::centredLeft);
    };
    item (Pal::input, "INPUT", 80.0f, false);
    item (Pal::ink, "TARGET", 90.0f, true);
    item (Pal::teal, "RESULT", 90.0f, false);
}

void PoiseEditor::paintCorrection (juce::Graphics& g)
{
    const auto area = correctionArea.toFloat();
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (Pal::line);
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);

    const auto pl = area.reduced (14.0f, 10.0f).withTrimmedLeft (24.0f);
    const float range = juce::jmax (1.0f, proc.apvts.getRawParameterValue ("range")->load());
    const float zero = pl.getCentreY();
    g.setColour (Pal::line);
    g.drawHorizontalLine (juce::roundToInt (zero), pl.getX(), pl.getRight());
    g.setColour (Pal::inkDim);
    g.setFont (sans (9.5f));
    g.drawText (juce::String::charToString (0xb1) + juce::String (range, 0), (int) area.getX() + 2, (int) pl.getY(), 34, 12, juce::Justification::centredRight);
    g.drawText ("CORRECTION", (int) pl.getX() + 6, (int) pl.getY(), 100, 12, juce::Justification::centredLeft);

    const auto& e = proc.engine;
    for (int b = 0; b < e.numBands; ++b)
    {
        const float x = xForFreq (pl, e.centres[(size_t) b]);
        const float v = juce::jlimit (-1.0f, 1.0f, gain[(size_t) b] / range);
        const float h = v * pl.getHeight() * 0.5f;
        g.setColour (v >= 0.0f ? Pal::teal : Pal::coral);
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 6.0f, h >= 0 ? zero - h : zero, 12.0f, std::abs (h)).withHeight (juce::jmax (1.0f, std::abs (h))), 2.0f);
    }
}

//==============================================================================
void PoiseEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 0);
    header = b.removeFromTop (56).withTrimmedTop (6);
    {
        auto h = header;
        learn.setBounds (h.removeFromRight (120).withSizeKeepingCentre (120, 30));
        h.removeFromRight (10);
        autoGain.setBounds (h.removeFromRight (110).withSizeKeepingCentre (110, 30));
        h.removeFromRight (10);
        mode.setBounds (h.removeFromRight (130).withSizeKeepingCentre (130, 30));
        h.removeFromRight (10);
        target.setBounds (h.removeFromRight (120).withSizeKeepingCentre (120, 30));
    }

    balanceArea = b.removeFromTop (250);
    b.removeFromTop (8);
    correctionArea = b.removeFromTop (92);
    b.removeFromTop (10);
    knobArea = b.removeFromTop (b.getHeight() - 14);

    auto row = knobArea.reduced (10, 10);
    const int w = row.getWidth() / 10;
    for (auto* k : { &amount, &keep, &speed, &resolution, &range, &tilt, &lowLimit, &highLimit, &mix, &output })
    {
        auto col = row.removeFromLeft (w);
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }
}
