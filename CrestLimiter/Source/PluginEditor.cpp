#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour bg       { 0xff111317 };
    const juce::Colour panel    { 0xff181b21 };
    const juce::Colour panelHi  { 0xff20242c };
    const juce::Colour grid     { 0xff272c35 };
    const juce::Colour text     { 0xffd3d8e0 };
    const juce::Colour textDim  { 0xff7c8494 };
    const juce::Colour inWave   { 0xff343c4a };
    const juce::Colour outWave  { 0xff93a7bd };
    const juce::Colour accent   { 0xffff8c42 }; // gain reduction + knob arcs
    const juce::Colour ceiling  { 0xffe8e3d3 };
    const juce::Colour hot      { 0xffe5484d };
}

static constexpr float displayRange = 36.0f; // dB shown in the history display
static constexpr float meterRange   = 48.0f; // dB on the level meters
static constexpr float grRange      = 24.0f; // dB on the gain-reduction meter

static juce::Font mono (float size, bool bold = false)
{
    return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain);
}

static juce::Font sans (float size, bool bold = false)
{
    return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
}

static float toDb (float lin) { return juce::Decibels::gainToDecibels (lin, -100.0f); }

static juce::String fmtDb (float db, int decimals = 1)
{
    if (db <= -99.0f)
        return "-inf";
    return (db > 0.05f ? "+" : "") + juce::String (db, decimals);
}

//==============================================================================
CrestLookAndFeel::CrestLookAndFeel()
{
    setColour (juce::Label::textColourId, Pal::textDim);
    setColour (juce::Slider::textBoxTextColourId, Pal::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Pal::accent.withAlpha (0.4f));
    setColour (juce::ComboBox::backgroundColourId, Pal::panelHi);
    setColour (juce::ComboBox::textColourId, Pal::text);
    setColour (juce::ComboBox::outlineColourId, Pal::grid);
    setColour (juce::ComboBox::arrowColourId, Pal::textDim);
    setColour (juce::PopupMenu::backgroundColourId, Pal::panelHi);
    setColour (juce::PopupMenu::textColourId, Pal::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Pal::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, Pal::bg);
    setColour (juce::TextButton::buttonColourId, Pal::panelHi);
    setColour (juce::TextButton::textColourOffId, Pal::textDim);
    setColour (juce::TextButton::textColourOnId, Pal::text);
    setColour (juce::TextEditor::textColourId, Pal::text);
    setColour (juce::TextEditor::backgroundColourId, Pal::panelHi);
    setColour (juce::CaretComponent::caretColourId, Pal::accent);
}

void CrestLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (8.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = r - 3.0f;

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::grid);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (Pal::accent);
        g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = arcR - 8.0f;
    g.setGradientFill (juce::ColourGradient (Pal::panelHi.brighter (0.25f), c.x, c.y - kr,
                                             Pal::panel.darker (0.3f), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    g.setColour (Pal::text);
    g.drawLine ({ c.getPointOnCircumference (kr * 0.35f, angle), c.getPointOnCircumference (kr * 0.85f, angle) }, 2.2f);
}

void CrestLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? Pal::accent.withAlpha (0.18f) : Pal::panelHi);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (on ? Pal::accent : (highlighted ? Pal::textDim : Pal::grid));
    g.drawRoundedRectangle (area, 5.0f, 1.0f);

    auto led = area.removeFromLeft (area.getHeight()).reduced (area.getHeight() * 0.36f);
    g.setColour (on ? Pal::accent : Pal::grid.brighter (0.2f));
    g.fillEllipse (led);

    g.setColour (on ? Pal::text : Pal::textDim);
    g.setFont (sans (12.0f, true));
    g.drawText (b.getButtonText(), area.withTrimmedRight (8.0f), juce::Justification::centred);
}

void CrestLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? Pal::accent.withAlpha (0.25f) : Pal::panelHi);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (highlighted ? Pal::textDim : Pal::grid);
    g.drawRoundedRectangle (area, 5.0f, 1.0f);
}

void CrestLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Pal::panelHi);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (box.isMouseOver (true) ? Pal::textDim : Pal::grid);
    g.drawRoundedRectangle (area, 5.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Pal::textDim);
    g.fillPath (arrow);
}

//==============================================================================
CrestEditor::CrestEditor (CrestProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (gain,      "gain",      "GAIN");
    setupKnob (ceiling,   "ceiling",   "CEILING");
    setupKnob (lookahead, "lookahead", "LOOKAHEAD");
    setupKnob (release,   "release",   "RELEASE");
    setupKnob (link,      "link",      "STEREO LINK");

    setupChoice (style,        "style",        "STYLE",        CrestProcessor::styleChoices);
    setupChoice (oversampling, "oversampling", "OVERSAMPLING", CrestProcessor::osChoices);
    setupChoice (dither,       "dither",       "DITHER",       CrestProcessor::ditherChoices);

    addAndMakeVisible (unityButton);
    unityButton.setTooltip ("Removes the input gain from the output so you can judge the limiting at equal loudness");
    unityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "unity", unityButton);

    addAndMakeVisible (resetButton);
    resetButton.onClick = [this]
    {
        proc.resetMeters();
        inHold = outHold = -100.0f;
        grHold = 0.0f;
    };

    // drop whatever piled up while the editor was closed
    proc.histFifo.read (proc.histFifo.getNumReady());

    setSize (960, 560);
    startTimerHz (30);
}

CrestEditor::~CrestEditor()
{
    setLookAndFeel (nullptr);
}

void CrestEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Pal::text);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (sans (11.0f, true));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void CrestEditor::setupChoice (Choice& ch, const juce::String& paramId, const juce::String& name, const juce::StringArray& items)
{
    ch.box.addItemList (items, 1);
    ch.box.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (ch.box);

    ch.label.setText (name, juce::dontSendNotification);
    ch.label.setFont (sans (10.0f, true));
    ch.label.setJustificationType (juce::Justification::bottomLeft);
    addAndMakeVisible (ch.label);

    ch.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, ch.box);
}

//==============================================================================
void CrestEditor::timerCallback()
{
    float newIn[2] { -100, -100 }, newOut[2] { -100, -100 }, newGr = 0.0f;
    bool any = false;

    if (const int ready = proc.histFifo.getNumReady(); ready > 0)
    {
        const auto scope = proc.histFifo.read (ready);
        scope.forEach ([&] (int idx)
        {
            const auto& pt = proc.histData[(size_t) idx];
            hist[(size_t) histWrite] = pt;
            histWrite = (histWrite + 1) % histSize;
            histCount = juce::jmin (histCount + 1, histSize);

            for (int c = 0; c < 2; ++c)
            {
                newIn[c]  = juce::jmax (newIn[c],  toDb (pt.inPk[c]));
                newOut[c] = juce::jmax (newOut[c], toDb (pt.outPk[c]));
            }
            newGr = juce::jmin (newGr, pt.grDb);
            any = true;
        });
    }

    constexpr float fall = 0.8f; // dB per frame (~24 dB/s)
    for (int c = 0; c < 2; ++c)
    {
        inLvl[c]  = juce::jmax (newIn[c],  inLvl[c] - fall);
        outLvl[c] = juce::jmax (newOut[c], outLvl[c] - fall);
        inHold  = juce::jmax (inHold, newIn[c]);
        outHold = juce::jmax (outHold, newOut[c]);
    }
    grLvl = any ? newGr : grLvl * 0.85f;
    grHold = juce::jmin (grHold, newGr);

    repaint (displayArea);
    repaint (meterArea);
}

//==============================================================================
void CrestEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg);

    // header
    g.setColour (Pal::text);
    g.setFont (sans (24.0f, true));
    g.drawText ("CREST", headerArea.getX(), headerArea.getY(), 100, headerArea.getHeight(), juce::Justification::centredLeft);
    g.setColour (Pal::accent);
    g.setFont (sans (13.0f, true));
    g.drawText ("LIMITER", headerArea.getX() + 92, headerArea.getY() + 2, 100, headerArea.getHeight(), juce::Justification::centredLeft);

    paintDisplay (g);
    paintMeters (g);

    // control strip
    auto strip = getLocalBounds().withTop (displayArea.getBottom() + 12).reduced (16, 0).withTrimmedBottom (12);
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (strip.toFloat(), 8.0f);
}

void CrestEditor::paintDisplay (juce::Graphics& g)
{
    const auto area = displayArea.toFloat();
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (area, 8.0f);

    const auto plot = area.reduced (10.0f).withTrimmedLeft (28.0f);
    const float top = plot.getY(), h = plot.getHeight(), bottom = plot.getBottom();
    auto yFor = [&] (float db) { return top + juce::jlimit (0.0f, 1.0f, -db / displayRange) * h; };

    // grid
    g.setFont (mono (10.0f));
    for (int db = 0; db <= (int) displayRange; db += 6)
    {
        const float y = yFor ((float) -db);
        g.setColour (Pal::grid);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (Pal::textDim);
        g.drawText (db == 0 ? "0" : "-" + juce::String (db), (int) area.getX() + 4, (int) y - 7, 28, 14, juce::Justification::centredRight);
    }

    const int width = (int) plot.getWidth();
    const int count = juce::jmin (histCount, width);

    if (count > 1)
    {
        juce::Path inPath, outPath, grFill, grLine;
        const float x0 = plot.getRight() - (float) count;

        inPath.startNewSubPath (x0, bottom);
        outPath.startNewSubPath (x0, bottom);
        grFill.startNewSubPath (x0, top);

        for (int j = 0; j < count; ++j)
        {
            const auto& pt = hist[(size_t) ((histWrite - count + j + histSize) % histSize)];
            const float x = x0 + (float) j + 0.5f;
            const float yi = yFor (toDb (juce::jmax (pt.inPk[0], pt.inPk[1])));
            const float yo = yFor (toDb (juce::jmax (pt.outPk[0], pt.outPk[1])));
            const float yg = yFor (pt.grDb);

            inPath.lineTo (x, yi);
            outPath.lineTo (x, yo);
            grFill.lineTo (x, yg);
            if (j == 0) grLine.startNewSubPath (x, yg);
            else        grLine.lineTo (x, yg);
        }

        inPath.lineTo (plot.getRight(), bottom);  inPath.closeSubPath();
        outPath.lineTo (plot.getRight(), bottom); outPath.closeSubPath();
        grFill.lineTo (plot.getRight(), top);     grFill.closeSubPath();

        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());
        g.setColour (Pal::inWave);
        g.fillPath (inPath);
        g.setColour (Pal::outWave.withAlpha (0.85f));
        g.fillPath (outPath);
        g.setColour (Pal::accent.withAlpha (0.22f));
        g.fillPath (grFill);
        g.setColour (Pal::accent);
        g.strokePath (grLine, juce::PathStrokeType (1.6f));
        g.restoreState();
    }

    // ceiling
    const float ceilDb = proc.apvts.getRawParameterValue ("ceiling")->load();
    const float yc = yFor (ceilDb);
    const float dashes[] { 6.0f, 4.0f };
    g.setColour (Pal::ceiling.withAlpha (0.7f));
    g.drawDashedLine ({ plot.getX(), yc, plot.getRight(), yc }, dashes, 2, 1.0f);

    // legend
    auto legend = plot.toNearestInt().removeFromBottom (18).removeFromLeft (330).translated (6, -4);
    auto item = [&] (juce::Colour c, const juce::String& t, int w)
    {
        auto r = legend.removeFromLeft (w);
        g.setColour (c);
        g.fillRoundedRectangle (r.removeFromLeft (10).withSizeKeepingCentre (10, 10).toFloat(), 2.0f);
        g.setColour (Pal::textDim);
        g.setFont (sans (10.0f, true));
        g.drawText (t, r.withTrimmedLeft (5), juce::Justification::centredLeft);
    };
    item (Pal::inWave.brighter (0.3f), "INPUT", 70);
    item (Pal::outWave, "OUTPUT", 80);
    item (Pal::accent, "GAIN REDUCTION", 130);
}

void CrestEditor::paintMeters (juce::Graphics& g)
{
    const auto area = meterArea.toFloat();
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (area, 8.0f);

    auto inner = meterArea.reduced (12);
    auto bars = inner.removeFromTop (178);
    const float bt = (float) bars.getY(), bh = (float) bars.getHeight() - 16.0f;

    auto levelBar = [&] (int x, float db, int w)
    {
        auto r = juce::Rectangle<float> ((float) x, bt, (float) w, bh);
        g.setColour (Pal::bg);
        g.fillRoundedRectangle (r, 2.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + meterRange) / meterRange);
        auto fill = r.withTop (r.getBottom() - frac * r.getHeight());
        g.setColour (db > -0.1f ? Pal::hot : Pal::outWave);
        g.fillRoundedRectangle (fill, 2.0f);
    };

    const int bw = 14, x = bars.getX() + 4;
    levelBar (x,           inLvl[0], bw);
    levelBar (x + bw + 3,  inLvl[1], bw);

    // gain reduction bar, grows downwards
    {
        auto r = juce::Rectangle<float> ((float) (bars.getCentreX() - 9), bt, 18.0f, bh);
        g.setColour (Pal::bg);
        g.fillRoundedRectangle (r, 2.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, -grLvl / grRange);
        g.setColour (Pal::accent);
        g.fillRoundedRectangle (r.withHeight (frac * r.getHeight()), 2.0f);
    }

    const int xr = bars.getRight() - 4 - 2 * bw - 3;
    levelBar (xr,          outLvl[0], bw);
    levelBar (xr + bw + 3, outLvl[1], bw);

    // captions + held values
    g.setFont (sans (10.0f, true));
    g.setColour (Pal::textDim);
    const int capY = bars.getBottom() - 14;
    g.drawText ("IN",  x - 6,  capY, 2 * bw + 15, 14, juce::Justification::centred);
    g.drawText ("GR",  bars.getCentreX() - 20, capY, 40, 14, juce::Justification::centred);
    g.drawText ("OUT", xr - 6, capY, 2 * bw + 15, 14, juce::Justification::centred);

    auto holds = inner.removeFromTop (20);
    g.setFont (mono (11.0f, true));
    g.setColour (inHold > -0.1f ? Pal::hot : Pal::text);
    g.drawText (fmtDb (inHold), holds.getX(), holds.getY(), 56, 20, juce::Justification::centredLeft);
    g.setColour (Pal::accent);
    g.drawText (fmtDb (grHold), holds.getCentreX() - 28, holds.getY(), 56, 20, juce::Justification::centred);
    g.setColour (Pal::text);
    g.drawText (fmtDb (outHold), holds.getRight() - 56, holds.getY(), 56, 20, juce::Justification::centredRight);

    inner.removeFromTop (8);
    g.setColour (Pal::grid);
    g.drawHorizontalLine (inner.getY(), (float) inner.getX(), (float) inner.getRight());
    inner.removeFromTop (6);

    // loudness
    auto row = [&] (const juce::String& name, const juce::String& value, const juce::String& unit, juce::Colour c)
    {
        auto r = inner.removeFromTop (22);
        g.setFont (sans (10.0f, true));
        g.setColour (Pal::textDim);
        g.drawText (name, r, juce::Justification::centredLeft);
        g.setFont (mono (13.0f, true));
        g.setColour (c);
        g.drawText (value, r.withTrimmedRight (40), juce::Justification::centredRight);
        g.setFont (sans (10.0f));
        g.setColour (Pal::textDim);
        g.drawText (unit, r.removeFromRight (36), juce::Justification::centredLeft);
    };

    const float tp = toDb (proc.truePeakMax.load());
    const float ceilDb = proc.apvts.getRawParameterValue ("ceiling")->load();

    row ("MOMENTARY",  fmtDb (proc.loudness.getMomentary()), "LUFS", Pal::text);
    row ("SHORT-TERM", fmtDb (proc.loudness.getShortTerm()), "LUFS", Pal::text);
    row ("INTEGRATED", fmtDb ((float) proc.loudness.getIntegrated()), "LUFS", Pal::accent);
    row ("TRUE PEAK",  fmtDb (tp, 2), "dBTP", tp > ceilDb + 0.05f ? Pal::hot : Pal::text);
}

//==============================================================================
void CrestEditor::resized()
{
    auto b = getLocalBounds();
    headerArea = b.removeFromTop (60).reduced (16, 8);

    // header choices, right aligned
    {
        auto h = headerArea;
        for (auto* ch : { &dither, &oversampling, &style })
        {
            auto col = h.removeFromRight (ch == &style ? 150 : 110);
            h.removeFromRight (12);
            ch->label.setBounds (col.removeFromTop (14));
            ch->box.setBounds (col.removeFromTop (26));
        }
    }

    b.removeFromTop (4);
    auto main = b.removeFromTop (330).reduced (16, 0);
    meterArea = main.removeFromRight (200);
    main.removeFromRight (12);
    displayArea = main;

    resetButton.setBounds (meterArea.getX() + 12, meterArea.getBottom() - 36, meterArea.getWidth() - 24, 26);

    // control strip
    auto strip = getLocalBounds().withTop (displayArea.getBottom() + 12).reduced (16, 0).withTrimmedBottom (12).reduced (12, 8);

    auto right = strip.removeFromRight (180);
    unityButton.setBounds (right.withSizeKeepingCentre (160, 32));

    const int kw = 110;
    for (auto* k : { &gain, &ceiling, &lookahead, &release, &link })
    {
        auto col = strip.removeFromLeft (kw);
        strip.removeFromLeft (8);
        k->label.setBounds (col.removeFromTop (16));
        k->slider.setBounds (col);
    }
}
