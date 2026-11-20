#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour bg      { 0xff0e1020 };
    const juce::Colour panel   { 0xff161a2e };
    const juce::Colour panelHi { 0xff1f2440 };
    const juce::Colour line    { 0xff2b3154 };
    const juce::Colour text    { 0xffe6e9f5 };
    const juce::Colour textDim { 0xff8a91b4 };
    const juce::Colour accent  { 0xff9fb4ff };
    const juce::Colour accent2 { 0xff7fe3d4 };
    const juce::Colour ice     { 0xffc9f2ff };
}

static juce::Font sans (float size, bool bold = false)
{
    return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
}

static const char* const allParams[] { "character", "predelay", "size", "decay", "diffusion", "bloom", "early", "bassMult",
                                       "dampHz", "trebleMult", "modDepth", "modRate", "lowCut", "highCut", "width", "duck",
                                       "mix", "freeze", "shimmer", "shimmerPitch" };

//==============================================================================
NimbusLookAndFeel::NimbusLookAndFeel()
{
    setColour (juce::Label::textColourId, Pal::textDim);
    setColour (juce::Slider::textBoxTextColourId, Pal::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, Pal::text);
    setColour (juce::ComboBox::backgroundColourId, Pal::panelHi);
    setColour (juce::PopupMenu::backgroundColourId, Pal::panelHi);
    setColour (juce::PopupMenu::textColourId, Pal::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Pal::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, Pal::bg);
    setColour (juce::TooltipWindow::backgroundColourId, Pal::panelHi);
    setColour (juce::TooltipWindow::textColourId, Pal::text);
    setColour (juce::TooltipWindow::outlineColourId, Pal::line);
}

void NimbusLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float arcR = r - 3.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto col = juce::Colour::fromString (s.getProperties().getWithDefault ("accent", Pal::accent.toString()).toString());

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (value, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = arcR - 8.0f;
    g.setGradientFill (juce::ColourGradient (Pal::panelHi.brighter (0.25f), c.x, c.y - kr, Pal::bg, c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    const auto tip = c.getPointOnCircumference (kr * 0.72f, angle);
    g.setColour (Pal::text);
    g.fillEllipse (tip.x - 2.5f, tip.y - 2.5f, 5.0f, 5.0f);
}

void NimbusLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::ice.withAlpha (0.2f) : Pal::panelHi);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (on ? Pal::ice : (highlighted ? Pal::textDim : Pal::line));
    g.drawRoundedRectangle (area, 6.0f, on ? 1.5f : 1.0f);

    auto led = area.removeFromLeft (area.getHeight()).reduced (area.getHeight() * 0.34f);
    g.setColour (on ? Pal::ice : Pal::line.brighter (0.3f));
    g.fillEllipse (led);

    g.setColour (on ? Pal::text : Pal::textDim);
    g.setFont (sans (12.0f, true));
    g.drawText (b.getButtonText(), area.withTrimmedRight (8.0f), juce::Justification::centred);
}

void NimbusLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Pal::panelHi);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (box.isMouseOver (true) ? Pal::textDim : Pal::line);
    g.drawRoundedRectangle (area, 6.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Pal::textDim);
    g.fillPath (arrow);
}

//==============================================================================
NimbusEditor::NimbusEditor (NimbusProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (predelay,   "predelay",   "PREDELAY",  "Gap before the reverb starts. Keeps the dry attack clear.");
    setupKnob (size,       "size",       "SIZE",      "Size of the space (delay-line lengths). Changes glide smoothly.");
    setupKnob (decay,      "decay",      "DECAY",     "Reverb time (RT60) in the mid range.");
    setupKnob (diffusion,  "diffusion",  "DIFFUSION", "Low = grainy and echoey, high = smooth and dense.");
    setupKnob (bloom,      "bloom",      "BLOOM",     "Makes the tail swell in slowly instead of starting at full density.");
    setupKnob (early,      "early",      "EARLY",     "Early reflections: the sense of walls and distance.");
    setupKnob (bassMult,   "bassMult",   "BASS DECAY", "Low-frequency reverb time relative to Decay.");
    setupKnob (dampHz,     "dampHz",     "DAMPING",   "Frequency above which the tail decays faster.");
    setupKnob (trebleMult, "trebleMult", "TREBLE DECAY", "High-frequency reverb time relative to Decay. Lower = darker, more natural.");
    setupKnob (modDepth,   "modDepth",   "MOD DEPTH", "Slow movement inside the tail. Removes ringing and adds lushness; high = chorused.");
    setupKnob (modRate,    "modRate",    "MOD RATE",  "Speed of the internal modulation.");
    setupKnob (shimmer,    "shimmer",    "SHIMMER",   "Pitch-shifts part of the tail on every pass, so it climbs (or falls) in pitch.\n"
                                                         "Choose the interval next to Character. Safe at any setting, including Freeze.");
    setupKnob (lowCut,     "lowCut",     "LOW CUT",   "High-pass on the reverb only. Keeps the mix clean.");
    setupKnob (highCut,    "highCut",    "HIGH CUT",  "Low-pass on the reverb only.");
    setupKnob (width,      "width",      "WIDTH",     "Stereo width of the reverb. 100 % = natural.");
    setupKnob (duck,       "duck",       "DUCKING",   "Pushes the reverb down while the input is playing, so it blooms in the gaps.");
    setupKnob (mix,        "mix",        "MIX",       "Dry / wet balance (equal power). Use 100 % on a send.");

    for (auto* k : { &bassMult, &dampHz, &trebleMult, &modDepth, &modRate, &shimmer })
        k->slider.getProperties().set ("accent", Pal::accent2.toString());
    for (auto* k : { &lowCut, &highCut, &width, &duck, &mix })
        k->slider.getProperties().set ("accent", Pal::ice.toString());

    character.addItemList (NimbusProcessor::characterChoices, 1);
    character.setJustificationType (juce::Justification::centredLeft);
    character.setTooltip ("Room: short and tight. Hall: classic concert hall. Plate: dense and bright, no reflections.\n"
                          "Cathedral: huge stone space. Ambient: vast, slow-blooming and deeply modulated.");
    addAndMakeVisible (character);
    characterAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "character", character);

    shimmerPitch.addItemList (NimbusProcessor::shimmerChoices, 1);
    shimmerPitch.setJustificationType (juce::Justification::centredLeft);
    shimmerPitch.setTooltip ("Shimmer interval");
    addAndMakeVisible (shimmerPitch);
    shimmerPitchAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "shimmerPitch", shimmerPitch);

    freeze.setTooltip ("Holds the current tail forever and stops new input from entering it.");
    addAndMakeVisible (freeze);
    freezeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "freeze", freeze);

    for (auto* id : allParams)
        proc.apvts.addParameterListener (id, this);

    setSize (960, 650);
    startTimerHz (30);
}

NimbusEditor::~NimbusEditor()
{
    for (auto* id : allParams)
        proc.apvts.removeParameterListener (id, this);
    setLookAndFeel (nullptr);
}

void NimbusEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name, const juce::String& tip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    k.slider.setTooltip (tip);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (sans (10.5f, true));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void NimbusEditor::timerCallback()
{
    const float lvl = proc.outLevel.load();
    glow = juce::jmax (lvl, glow * 0.9f);
    repaint (display);
}

//==============================================================================
void NimbusEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg);

    g.setColour (Pal::text);
    g.setFont (sans (26.0f, true));
    g.drawText ("NIMBUS", header.withWidth (140), juce::Justification::centredLeft);
    g.setColour (Pal::accent);
    g.setFont (sans (12.0f, true));
    g.drawText ("REVERB", header.withTrimmedLeft (132).withWidth (80).translated (0, 3), juce::Justification::centredLeft);

    const char* titles[] { "SPACE", "TONE & MOTION", "OUTPUT" };
    for (size_t i = 0; i < rows.size(); ++i)
    {
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (rows[i].toFloat(), 10.0f);
        g.setColour (Pal::textDim);
        g.setFont (sans (10.0f, true));
        g.drawText (titles[i], rows[i].reduced (14, 8).removeFromTop (12), juce::Justification::centredLeft);
    }

    paintDisplay (g);
}

void NimbusEditor::paintDisplay (juce::Graphics& g)
{
    const auto area = display.toFloat();
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (area, 10.0f);

    // soft glow that breathes with the output
    {
        const float a = juce::jlimit (0.0f, 0.35f, glow * 0.6f);
        g.setGradientFill (juce::ColourGradient (Pal::accent.withAlpha (a), area.getCentreX(), area.getBottom(),
                                                 Pal::accent.withAlpha (0.0f), area.getCentreX(), area.getY(), true));
        g.fillRoundedRectangle (area, 10.0f);
    }

    const auto plot = area.reduced (16.0f, 14.0f).withTrimmedLeft (30.0f).withTrimmedBottom (14.0f);
    const auto p = proc.readParams();

    auto xFor = [&] (double f) { return plot.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * plot.getWidth(); };
    auto yFor = [&] (double rt)
    {
        const double t = std::log (juce::jlimit (0.1, 40.0, rt) / 0.1) / std::log (400.0);
        return plot.getBottom() - (float) t * plot.getHeight();
    };

    // grid
    g.setFont (sans (9.5f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xFor (f);
        g.setColour (Pal::line.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (Pal::textDim);
        g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String ((int) f),
                    (int) x - 20, (int) plot.getBottom() + 2, 40, 12, juce::Justification::centred);
    }
    for (double rt : { 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0 })
    {
        const float y = yFor (rt);
        g.setColour (Pal::line.withAlpha (0.6f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (Pal::textDim);
        g.drawText (juce::String (rt, rt < 1.0 ? 2 : 0) + "s", (int) area.getX() + 6, (int) y - 6, 34, 12, juce::Justification::centredRight);
    }

    // wet EQ: shade what the cut filters remove
    {
        const float xl = xFor (juce::jmax (20.0f, p.lowCutHz)), xh = xFor (juce::jmin (20000.0f, p.highCutHz));
        g.setColour (Pal::bg.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (plot.getX(), plot.getY(), juce::jmax (0.0f, xl - plot.getX()), plot.getHeight()));
        g.fillRect (juce::Rectangle<float> (xh, plot.getY(), juce::jmax (0.0f, plot.getRight() - xh), plot.getHeight()));
        g.setColour (Pal::ice.withAlpha (0.45f));
        g.drawVerticalLine (juce::roundToInt (xl), plot.getY(), plot.getBottom());
        g.drawVerticalLine (juce::roundToInt (xh), plot.getY(), plot.getBottom());
    }

    // decay-vs-frequency curve
    if (curveDirty)
    {
        curve.clear();
        curveFill.clear();
        const int steps = 160;
        for (int i = 0; i <= steps; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / steps);
            const float x = xFor (f), y = yFor (ReverbEngine::rtAt (f, p));
            if (i == 0) { curve.startNewSubPath (x, y); curveFill.startNewSubPath (x, plot.getBottom()); }
            else          curve.lineTo (x, y);
            curveFill.lineTo (x, y);
        }
        curveFill.lineTo (plot.getRight(), plot.getBottom());
        curveFill.closeSubPath();
        curveDirty = false;
    }

    g.setGradientFill (juce::ColourGradient (Pal::accent.withAlpha (0.35f), 0.0f, plot.getY(),
                                             Pal::accent2.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
    g.fillPath (curveFill);
    g.setColour (Pal::accent.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (p.freeze ? Pal::ice : Pal::accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // read-out
    auto info = area.reduced (18.0f, 12.0f).removeFromTop (22.0f).removeFromRight (360.0f);
    g.setFont (sans (13.0f, true));
    g.setColour (p.freeze ? Pal::ice : Pal::text);
    const juce::String rtText = p.freeze ? juce::String ("FROZEN")
                                         : juce::String (p.decayS, 2) + " s   low " + juce::String (p.decayS * p.bassMult, 1)
                                           + " s   high " + juce::String (p.decayS * p.trebleMult, 1) + " s";
    g.drawText (rtText, info, juce::Justification::centredRight);
}

//==============================================================================
void NimbusEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 0);
    header = b.removeFromTop (60).withTrimmedTop (8);

    {
        auto h = header;
        freeze.setBounds (h.removeFromRight (120).withSizeKeepingCentre (120, 30));
        h.removeFromRight (12);
        character.setBounds (h.removeFromRight (160).withSizeKeepingCentre (160, 30));
        h.removeFromRight (12);
        shimmerPitch.setBounds (h.removeFromRight (170).withSizeKeepingCentre (170, 30));
    }

    display = b.removeFromTop (196);
    b.removeFromTop (10);

    const int rowH = (b.getHeight() - 16 - 2 * 8) / 3;
    for (auto& r : rows)
    {
        r = b.removeFromTop (rowH);
        b.removeFromTop (8);
    }

    auto layoutRow = [] (juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
    {
        auto inner = r.reduced (14, 6).withTrimmedTop (14);
        const int w = inner.getWidth() / 6;
        for (auto* k : knobs)
        {
            auto col = inner.removeFromLeft (w);
            k->label.setBounds (col.removeFromTop (14));
            k->slider.setBounds (col);
        }
    };

    layoutRow (rows[0], { &predelay, &size, &decay, &diffusion, &bloom, &early });
    layoutRow (rows[1], { &bassMult, &dampHz, &trebleMult, &modDepth, &modRate, &shimmer });
    layoutRow (rows[2], { &lowCut, &highCut, &width, &duck, &mix });
}
