#include "PluginEditor.h"

using namespace rud;

namespace Pal
{
    const juce::Colour bg      { 0xff14161a };
    const juce::Colour panel   { 0xff1c1f25 };
    const juce::Colour screen  { 0xff191c21 };
    const juce::Colour knob    { 0xff2a2e36 };
    const juce::Colour cell    { 0xff252931 };
    const juce::Colour line    { 0xff30353e };
    const juce::Colour ink     { 0xffe4e7ec };
    const juce::Colour inkDim  { 0xff8a909b };
    const juce::Colour teal    { 0xff3ccabf };
    const juce::Colour coral   { 0xffff7a5c };
}

static juce::Font sans (float size, bool bold = false) { return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain); }

//==============================================================================
RudimentLookAndFeel::RudimentLookAndFeel()
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
    setColour (juce::TooltipWindow::backgroundColourId, Pal::bg);
    setColour (juce::TooltipWindow::textColourId, Pal::ink);
    setColour (juce::TooltipWindow::outlineColourId, Pal::line);
}

void RudimentLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
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

    const float kr = r - 7.0f;
    g.setColour (Pal::knob);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);
    g.setColour (Pal::ink);
    g.drawLine ({ c.getPointOnCircumference (kr * 0.3f, angle), c.getPointOnCircumference (kr * 0.9f, angle) }, 2.0f);
}

void RudimentLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::teal : Pal::panel);
    g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
    g.setColour (highlighted ? Pal::ink : (on ? Pal::teal : Pal::line));
    g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.0f);
    g.setColour (on ? Pal::bg : Pal::inkDim);
    g.setFont (sans (12.0f, true));
    g.drawText ((on ? juce::String::fromUTF8 ("\xe2\x96\xa0  ") : juce::String::fromUTF8 ("\xe2\x96\xb6  ")) + b.getButtonText(), area, juce::Justification::centred);
}

void RudimentLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, h * 0.5f);
    g.setColour (box.isMouseOver (true) ? Pal::ink : Pal::line);
    g.drawRoundedRectangle (area, h * 0.5f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (Pal::inkDim);
    g.fillPath (arrow);
}

juce::Font RudimentLookAndFeel::getComboBoxFont (juce::ComboBox&) { return sans (12.0f, true); }

void RudimentLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
StepGrid::StepGrid (RudimentProcessor& p) : proc (p)
{
    setTooltip ("Click a step: on / off (drag to paint).  Shift- or right-click: accent.\n"
                "Alt-click a step: track length ends there (polymeter).  Drag the number: track length.\n"
                "Click a name: edit that sound and hear it.  M: mute.");
}

juce::Rectangle<float> StepGrid::cellRect (int row, int col) const
{
    const float x = stepsX() + col * cellW() + (col / 4) * 0.0f;
    return { x + 2.0f, row * rowH() + 3.0f, cellW() - 4.0f, rowH() - 6.0f };
}

juce::Rectangle<float> StepGrid::zoneRect (Zone z, int row) const
{
    const float y = row * rowH();
    switch (z)
    {
        case Zone::name:   return { 0.0f, y + 2.0f, nameW - 4.0f, rowH() - 4.0f };
        case Zone::mute:   return { nameW, y + 2.0f, muteW - 4.0f, rowH() - 4.0f };
        case Zone::length: return { nameW + muteW, y + 2.0f, lenW - 4.0f, rowH() - 4.0f };
        default:           return {};
    }
}

StepGrid::Zone StepGrid::zoneAt (juce::Point<int> p, int& row, int& col) const
{
    row = juce::jlimit (0, numElements - 1, (int) (p.y / rowH()));
    col = -1;
    const float x = (float) p.x;
    if (x < nameW) return Zone::name;
    if (x < nameW + muteW) return Zone::mute;
    if (x < nameW + muteW + lenW) return Zone::length;
    if (x >= stepsX())
    {
        col = juce::jlimit (0, numSteps - 1, (int) ((x - stepsX()) / cellW()));
        return Zone::step;
    }
    return Zone::none;
}

void StepGrid::paint (juce::Graphics& g)
{
    const auto& specs = elementSpecs();
    for (int r = 0; r < numElements; ++r)
    {
        // name (flashes with the element's level)
        auto nr = zoneRect (Zone::name, r);
        g.setColour (r == selected ? Pal::knob : Pal::panel);
        g.fillRoundedRectangle (nr, 6.0f);
        const float m = juce::jlimit (0.0f, 1.0f, proc.meters[(size_t) r].load() * 1.6f);
        if (m > 0.01f)
        {
            g.setColour (Pal::teal.withAlpha (0.45f * m));
            g.fillRoundedRectangle (nr, 6.0f);
        }
        if (r == selected)
        {
            g.setColour (Pal::teal);
            g.fillRoundedRectangle (nr.withWidth (4.0f), 2.0f);
        }
        g.setColour (r == selected ? Pal::ink : Pal::inkDim);
        g.setFont (sans (11.5f, true));
        g.drawText (specs[(size_t) r].name, nr.withTrimmedLeft (12.0f), juce::Justification::centredLeft);

        // mute
        const bool muted = proc.apvts.getRawParameterValue (RudimentProcessor::paramId (r, "Mute"))->load() > 0.5f;
        auto mr = zoneRect (Zone::mute, r).reduced (0.0f, 3.0f);
        g.setColour (muted ? Pal::coral : Pal::panel);
        g.fillRoundedRectangle (mr, 5.0f);
        g.setColour (muted ? Pal::bg : Pal::inkDim);
        g.setFont (sans (10.5f, true));
        g.drawText ("M", mr, juce::Justification::centred);

        // length
        const int len = proc.pattern.len (r);
        g.setColour (len < numSteps ? Pal::teal : Pal::inkDim);
        g.setFont (sans (11.0f, true));
        g.drawText (juce::String (len), zoneRect (Zone::length, r), juce::Justification::centred);

        // steps
        const int play = proc.playStep[(size_t) r].load();
        for (int c = 0; c < numSteps; ++c)
        {
            auto cr = cellRect (r, c);
            const bool inLen = c < len;
            const uint8_t st = proc.pattern.get (r, c);
            juce::Colour col = (c / 4) % 2 == 0 ? Pal::cell : Pal::cell.brighter (0.08f);
            if (st == 1) col = Pal::teal.withAlpha (0.8f);
            if (st == 2) col = Pal::coral;
            if (! inLen) col = col.withAlpha (0.25f);
            if (muted && st != 0) col = col.withSaturation (0.2f);
            g.setColour (col);
            g.fillRoundedRectangle (cr, 4.0f);
            if (c == play && inLen)
            {
                g.setColour (Pal::ink);
                g.drawRoundedRectangle (cr.reduced (0.5f), 4.0f, 1.6f);
            }
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    int row, col;
    dragZone = zoneAt (e.getPosition(), row, col);
    dragRow = row;
    switch (dragZone)
    {
        case Zone::name:
            if (onSelect) onSelect (row);
            proc.previewElement (row);
            break;
        case Zone::mute:
            if (auto* p = proc.apvts.getParameter (RudimentProcessor::paramId (row, "Mute")))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
                p->endChangeGesture();
            }
            break;
        case Zone::length:
            lenStart = proc.pattern.len (row);
            break;
        case Zone::step:
        {
            if (e.mods.isAltDown())
            {
                proc.pattern.length[(size_t) row] = col + 1;
                dragZone = Zone::none;
                break;
            }
            const uint8_t st = proc.pattern.get (row, col);
            if (e.mods.isShiftDown() || e.mods.isPopupMenu())
                paintValue = st == 2 ? 1 : 2;
            else
                paintValue = st == 0 ? 1 : 0;
            proc.pattern.set (row, col, paintValue);
            break;
        }
        default: break;
    }
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragZone == Zone::length)
    {
        const int len = juce::jlimit (1, numSteps, lenStart - e.getDistanceFromDragStartY() / 8);
        proc.pattern.length[(size_t) dragRow] = len;
        repaint();
    }
    else if (dragZone == Zone::step)
    {
        int row, col;
        if (zoneAt (e.getPosition(), row, col) == Zone::step && row == dragRow && col >= 0)
        {
            proc.pattern.set (row, col, paintValue);
            repaint();
        }
    }
}

void StepGrid::mouseUp (const juce::MouseEvent&) { dragZone = Zone::none; }

//==============================================================================
RudimentEditor::RudimentEditor (RudimentProcessor& p)
    : AudioProcessorEditor (p), proc (p), grid (p)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        kit.addItem (proc.getProgramName (i), i + 1);
    kit.setTooltip ("Kit: a complete set of sounds. Does not touch the pattern or the mix.");
    kit.onChange = [this]
    {
        const int idx = kit.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram())
            proc.setCurrentProgram (idx);
        lastKit = proc.getCurrentProgram();
    };
    addAndMakeVisible (kit);

    patternBox.addItemList (RudimentProcessor::patternNames(), 1);
    patternBox.setTextWhenNothingSelected ("Load pattern...");
    patternBox.setTooltip ("Load a starting pattern (replaces the steps).");
    patternBox.onChange = [this]
    {
        const int idx = patternBox.getSelectedId() - 1;
        if (idx >= 0) proc.loadPattern (idx);
        grid.repaint();
    };
    addAndMakeVisible (patternBox);

    run.setTooltip ("Plays the sequencer without the host transport (uses the host tempo).\nWhen the host plays, the sequencer always follows the song position.");
    addAndMakeVisible (run);
    runAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "run", run);

    grid.onSelect = [this] (int e) { select (e); };
    addAndMakeVisible (grid);

    const auto& specs = elementSpecs();
    for (int e = 0; e < numElements; ++e)
    {
        for (const auto& ps : specs[(size_t) e].params)
            makeKnob (elementKnobs[(size_t) e], RudimentProcessor::paramId (e, ps.id), ps.label, ps.tip, ps.unit == Unit::semis && ps.min < 0.0f);
        makeKnob (elementKnobs[(size_t) e], RudimentProcessor::paramId (e, "Level"), "LEVEL", "Level of this sound in the mix.");
        makeKnob (elementKnobs[(size_t) e], RudimentProcessor::paramId (e, "Pan"), "PAN", "Position in the stereo field.", true);
    }

    makeKnob (masterKnobs, "swing",  "SWING",  "Delays every second 16th. 50 % straight, 67 % triplet shuffle.");
    makeKnob (masterKnobs, "accent", "ACCENT", "How much louder accented (orange) steps are than normal ones.");
    makeKnob (masterKnobs, "glue",   "GLUE",   "Bus compressor that ties the kit together.");
    makeKnob (masterKnobs, "drive",  "DRIVE",  "Bus saturation, 2x oversampled.");
    makeKnob (masterKnobs, "volume", "VOLUME", "Output level.");

    select (0);
    setSize (1120, 600);
    startTimerHz (30);
}

RudimentEditor::~RudimentEditor()
{
    setLookAndFeel (nullptr);
}

RudimentEditor::Knob* RudimentEditor::makeKnob (std::vector<std::unique_ptr<Knob>>& into, const juce::String& id, const juce::String& name, const juce::String& tip, bool centred)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 15);
    k->slider.setTooltip (tip);
    if (centred) k->slider.getProperties().set ("centred", true);
    addChildComponent (k->slider);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (sans (10.0f, true));
    k->label.setInterceptsMouseClicks (false, false);
    addChildComponent (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    into.push_back (std::move (k));
    return into.back().get();
}

void RudimentEditor::select (int e)
{
    selected = e;
    grid.selected = e;
    for (int i = 0; i < numElements; ++i)
        for (auto& k : elementKnobs[(size_t) i])
        {
            k->slider.setVisible (i == e);
            k->label.setVisible (i == e);
        }
    for (auto& k : masterKnobs) { k->slider.setVisible (true); k->label.setVisible (true); }
    updatePreview (true);
    resized();
    repaint();
}

void RudimentEditor::updatePreview (bool force)
{
    const auto& ps = elementSpecs()[(size_t) selected].params;
    std::vector<float> values;
    for (const auto& s : ps)
        values.push_back (proc.apvts.getRawParameterValue (RudimentProcessor::paramId (selected, s.id))->load());
    if (! force && values == previewParams && previewElement == selected)
        return;
    previewParams = values;
    previewElement = selected;

    // render the hit offline with the same DSP the plugin uses
    const double fs = 48000.0;
    auto el = makeElement (selected);
    el->prepare (fs);
    for (size_t i = 0; i < values.size(); ++i) el->p[i] = values[i];
    el->trigger (0.9f, 0.0f);
    const int maxLen = (int) (fs * 4.0);
    std::vector<float> L ((size_t) maxLen, 0.0f), R ((size_t) maxLen, 0.0f);
    int done = 0;
    while (done < maxLen && el->isActive())
    {
        const int n = juce::jmin (512, maxLen - done);
        el->render (L.data() + done, R.data() + done, n);
        done += n;
    }
    int last = 1;
    for (int i = 0; i < done; ++i)
        if (std::abs (L[(size_t) i]) + std::abs (R[(size_t) i]) > 2e-3f) last = i;
    const int len = juce::jmax ((int) (0.05 * fs), (int) (last * 1.05));
    preview.assign (L.begin(), L.begin() + juce::jmin (len, maxLen));
    for (int i = 0; i < (int) preview.size(); ++i) preview[(size_t) i] = 0.5f * (L[(size_t) i] + R[(size_t) i]);
    previewSeconds = (double) preview.size() / fs;
    repaint (waveArea);
}

void RudimentEditor::timerCallback()
{
    updatePreview (false);
    if (proc.getCurrentProgram() != lastKit)
    {
        lastKit = proc.getCurrentProgram();
        kit.setSelectedId (lastKit + 1, juce::dontSendNotification);
    }
    grid.repaint();
}

//==============================================================================
void RudimentEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg);

    g.setColour (Pal::ink);
    g.setFont (sans (26.0f, true));
    g.drawText ("RUDIMENT", header.withWidth (150), juce::Justification::centredLeft);
    g.setColour (Pal::teal);
    g.setFont (sans (11.0f, true));
    g.drawText ("DRUM SYNTH + SEQUENCER", header.withTrimmedLeft (146).withWidth (200).translated (0, 3), juce::Justification::centredLeft);
    g.setColour (Pal::inkDim);
    g.setFont (sans (10.0f, true));
    g.drawText ("UNVRS", header.withTrimmedLeft (330).withWidth (60).translated (0, 3), juce::Justification::centredLeft);

    for (auto p : { elementPanel, masterPanel })
    {
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (p.toFloat(), 10.0f);
    }
    g.setColour (Pal::ink);
    g.setFont (sans (13.0f, true));
    g.drawText (elementSpecs()[(size_t) selected].name, elementPanel.getX() + 16, elementPanel.getY() + 8, 200, 20, juce::Justification::centredLeft);
    g.setColour (Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText ("MASTER", masterPanel.getX() + 16, masterPanel.getY() + 8, 200, 20, juce::Justification::centredLeft);
    g.setFont (sans (10.0f));
    g.drawFittedText ("Click a name to edit and hear a sound.\nShift-click a step: accent.  Alt-click: track length.\nWith the host stopped, PLAY runs the pattern on its own.",
                      masterPanel.reduced (16, 0).withTrimmedTop (34).withHeight (56), juce::Justification::topLeft, 3);

    paintWave (g);
}

void RudimentEditor::paintWave (juce::Graphics& g)
{
    const auto area = waveArea.toFloat();
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (Pal::line);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);
    if (preview.empty()) return;

    auto pl = area.reduced (10.0f, 8.0f);
    const float mid = pl.getCentreY();
    float peak = 1e-6f;
    for (auto v : preview) peak = juce::jmax (peak, std::abs (v));
    const float scale = pl.getHeight() * 0.5f / juce::jmax (peak, 0.25f);

    const int w = (int) pl.getWidth();
    juce::Path fill;
    std::vector<float> mx ((size_t) w), mn ((size_t) w);
    for (int x = 0; x < w; ++x)
    {
        const size_t a = (size_t) ((double) x / w * preview.size()), b = juce::jmax (a + 1, (size_t) ((double) (x + 1) / w * preview.size()));
        float hi = -1e9f, lo = 1e9f;
        for (size_t i = a; i < b && i < preview.size(); ++i) { hi = juce::jmax (hi, preview[i]); lo = juce::jmin (lo, preview[i]); }
        mx[(size_t) x] = hi; mn[(size_t) x] = lo;
    }
    fill.startNewSubPath (pl.getX(), mid - mx[0] * scale);
    for (int x = 1; x < w; ++x) fill.lineTo (pl.getX() + (float) x, mid - mx[(size_t) x] * scale);
    for (int x = w - 1; x >= 0; --x) fill.lineTo (pl.getX() + (float) x, mid - mn[(size_t) x] * scale);
    fill.closeSubPath();
    g.setColour (Pal::teal.withAlpha (0.75f));
    g.fillPath (fill);

    g.setColour (Pal::inkDim);
    g.setFont (sans (10.0f, true));
    const juce::String len = previewSeconds < 1.0 ? juce::String (juce::roundToInt (previewSeconds * 1000.0)) + " ms" : juce::String (previewSeconds, 2) + " s";
    g.drawText (len, waveArea.reduced (12, 6), juce::Justification::topRight);
}

void RudimentEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 0);
    header = b.removeFromTop (52).withTrimmedTop (4);
    {
        auto h = header;
        run.setBounds (h.removeFromRight (100).withSizeKeepingCentre (100, 30));
        h.removeFromRight (10);
        patternBox.setBounds (h.removeFromRight (180).withSizeKeepingCentre (180, 30));
        h.removeFromRight (10);
        kit.setBounds (h.removeFromRight (180).withSizeKeepingCentre (180, 30));
    }
    gridArea = b.removeFromTop (8 * 32);
    grid.setBounds (gridArea);
    b.removeFromTop (12);

    auto bottom = b.removeFromTop (b.getHeight() - 14);
    masterPanel = bottom.removeFromRight (5 * 66 + 20);
    bottom.removeFromRight (10);
    elementPanel = bottom;

    auto ep = elementPanel.reduced (10, 8);
    ep.removeFromTop (24);
    waveArea = ep.removeFromTop (juce::jmax (60, ep.getHeight() - 104));
    ep.removeFromTop (6);
    knobRow = ep;
    auto row = knobRow;
    const auto& knobs = elementKnobs[(size_t) selected];
    const int kw = juce::jmin (66, row.getWidth() / juce::jmax (1, (int) knobs.size()));
    for (auto& k : knobs)
    {
        auto col = row.removeFromLeft (kw);
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }

    auto mp = masterPanel.reduced (10, 8);
    auto mrow = mp.removeFromBottom (98);
    for (auto& k : masterKnobs)
    {
        auto col = mrow.removeFromLeft (66);
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }
}
