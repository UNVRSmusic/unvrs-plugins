#include "PluginEditor.h"

using namespace sx;

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
}

static juce::Font sans (float size, bool bold = false) { return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain); }

//==============================================================================
SextantLookAndFeel::SextantLookAndFeel()
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
    setColour (juce::TextButton::textColourOffId, Pal::inkDim);
    setColour (juce::TextButton::textColourOnId, Pal::bg);
    setColour (juce::TooltipWindow::backgroundColourId, Pal::bg);
    setColour (juce::TooltipWindow::textColourId, Pal::ink);
    setColour (juce::TooltipWindow::outlineColourId, Pal::line);
}

void SextantLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool centred = (bool) s.getProperties().getWithDefault ("centred", false);
    const float alpha = s.getAlpha() < 0.99f ? 0.4f : 1.0f;

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float from = centred ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path v;
        v.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (Pal::teal.withMultipliedAlpha (alpha));
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

void SextantLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle, juce::Slider& s)
{
    // operator level fader: wide track, filled from the bottom, live meter beside it
    const auto accent = (bool) s.getProperties().getWithDefault ("carrier", true) ? Pal::teal : Pal::coral;
    auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
    auto track = area.withSizeKeepingCentre (14.0f, area.getHeight());
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (track, 7.0f);
    auto fill = track.withTop (pos);
    g.setColour (accent.withAlpha (0.85f));
    g.fillRoundedRectangle (fill, 7.0f);
    const float meter = (float) s.getProperties().getWithDefault ("meter", 0.0f);
    auto m = juce::Rectangle<float> (track.getRight() + 6.0f, area.getY(), 4.0f, area.getHeight());
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (m, 2.0f);
    g.setColour (Pal::ink.withAlpha (0.8f));
    g.fillRoundedRectangle (m.withTop (m.getBottom() - m.getHeight() * juce::jlimit (0.0f, 1.0f, meter)), 2.0f);
    g.setColour (Pal::ink);
    g.fillRoundedRectangle (juce::Rectangle<float> (track.getX() - 6.0f, pos - 4.0f, track.getWidth() + 12.0f, 8.0f), 3.0f);
}

void SextantLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::teal : Pal::panel);
    g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
    g.setColour (highlighted ? Pal::ink : (on ? Pal::teal : Pal::line));
    g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.0f);
    g.setColour (on ? Pal::bg : Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText (b.getButtonText(), area, juce::Justification::centred);
}

void SextantLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const juce::Colour accent = b.getProperties().contains ("accent") ? juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["accent"]) : Pal::teal;
    g.setColour (on ? accent : (down ? Pal::line : Pal::panel));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (highlighted ? Pal::ink : (on ? accent : Pal::line));
    g.drawRoundedRectangle (area, 6.0f, 1.0f);
}

void SextantLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (box.isMouseOver (true) ? Pal::ink : Pal::line);
    g.drawRoundedRectangle (area, 6.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (Pal::inkDim);
    g.fillPath (arrow);
}

juce::Font SextantLookAndFeel::getComboBoxFont (juce::ComboBox&) { return sans (12.0f, true); }
juce::Font SextantLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return sans (11.5f, true); }

void SextantLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
AlgoView::AlgoView (SextantProcessor& p) : proc (p)
{
    setTooltip ("Algorithm: which operator modulates which. Carriers (teal) are heard, modulators (orange) shape their tone.\n"
                "Click an operator to edit it. EDIT shows the routing matrix: click a cell to connect row -> column, OUT row = heard.");
}

juce::Rectangle<float> AlgoView::matrixCell (int row, int col) const
{
    auto area = getLocalBounds().toFloat().reduced (8.0f).withTrimmedLeft (22.0f).withTrimmedTop (18.0f);
    const float cw = area.getWidth() / numOps, ch = area.getHeight() / (numOps + 1);
    return { area.getX() + col * cw + 2.0f, area.getY() + row * ch + 2.0f, cw - 4.0f, ch - 4.0f };
}

void AlgoView::paint (juce::Graphics& g)
{
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    const auto r = proc.currentRouting();
    if (editing) paintMatrix (g, r); else paintDiagram (g, r);
}

void AlgoView::paintMatrix (juce::Graphics& g, const Routing& r)
{
    g.setFont (sans (10.0f, true));
    for (int c = 0; c < numOps; ++c)
    {
        auto top = matrixCell (0, c).translated (0.0f, -16.0f).withHeight (14.0f);
        g.setColour (Pal::inkDim);
        g.drawText ("TO " + juce::String (c + 1), top, juce::Justification::centred);
    }
    for (int row = 0; row <= numOps; ++row)
    {
        auto lab = matrixCell (row, 0).withX (8.0f).withWidth (20.0f);
        g.setColour (row == numOps ? Pal::teal : Pal::inkDim);
        g.drawText (row == numOps ? "OUT" : juce::String (row + 1), lab, juce::Justification::centredLeft);
        for (int c = 0; c < numOps; ++c)
        {
            auto cell = matrixCell (row, c);
            bool on;
            juce::Colour col;
            if (row == numOps) { on = r.isCarrier (c); col = Pal::teal; }
            else if (row == c)
            {
                const float fb = proc.apvts.getRawParameterValue (SextantProcessor::opId (c, "Feedback"))->load();
                g.setColour (Pal::coral.withAlpha (0.15f + 0.6f * fb));
                g.fillRoundedRectangle (cell, 4.0f);
                g.setColour (Pal::inkDim);
                g.drawText ("FB", cell, juce::Justification::centred);
                continue;
            }
            else { on = r.has (row, c); col = Pal::coral; }
            g.setColour (on ? col : Pal::knob);
            g.fillRoundedRectangle (cell, 4.0f);
        }
    }
}

void AlgoView::paintDiagram (juce::Graphics& g, const Routing& r)
{
    // depth: carriers 0, a modulator sits one level above the deepest operator it feeds
    std::array<int, numOps> depth;
    depth.fill (-1);
    for (int k = 0; k < numOps; ++k) if (r.isCarrier (k)) depth[(size_t) k] = 0;
    for (int pass = 0; pass < numOps; ++pass)
        for (int src = 0; src < numOps; ++src)
            for (int dst = 0; dst < numOps; ++dst)
                if (src != dst && r.has (src, dst) && depth[(size_t) dst] >= 0 && depth[(size_t) dst] < numOps - 1 && depth[(size_t) src] <= depth[(size_t) dst])
                    depth[(size_t) src] = depth[(size_t) dst] + 1;
    int maxDepth = 0;
    for (auto d : depth) maxDepth = juce::jmax (maxDepth, d);

    auto area = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
    const float outY = area.getBottom() - 6.0f;
    const float levelH = (area.getHeight() - 30.0f) / (float) juce::jmax (1, maxDepth + 1);
    const float bw = 34.0f, bh = 24.0f;

    // x placement level by level, ordered by the targets' positions
    std::array<float, numOps> xs {};
    std::vector<int> unused;
    for (int d = 0; d <= maxDepth; ++d)
    {
        std::vector<std::pair<float, int>> row;
        for (int k = 0; k < numOps; ++k)
        {
            if (depth[(size_t) k] != d) continue;
            float key = (float) k;
            if (d > 0)
            {
                float sum = 0; int n = 0;
                for (int t = 0; t < numOps; ++t) if (r.has (k, t) && depth[(size_t) t] >= 0 && depth[(size_t) t] < d) { sum += xs[(size_t) t]; ++n; }
                if (n > 0) key = sum / n + k * 0.01f;
            }
            row.push_back ({ key, k });
        }
        std::sort (row.begin(), row.end());
        for (size_t i = 0; i < row.size(); ++i)
            xs[(size_t) row[i].second] = area.getX() + area.getWidth() * (float) (i + 1) / (float) (row.size() + 1);
    }
    for (int k = 0; k < numOps; ++k)
    {
        if (depth[(size_t) k] < 0) { unused.push_back (k); continue; }
        const float y = outY - 18.0f - levelH * (depth[(size_t) k] + 0.5f);
        boxes[(size_t) k] = { xs[(size_t) k] - bw * 0.5f, y - bh * 0.5f, bw, bh };
    }
    for (size_t i = 0; i < unused.size(); ++i)
        boxes[(size_t) unused[i]] = { area.getRight() - bw - 2.0f, area.getY() + i * (bh + 4.0f), bw, bh };

    // output bar + carrier lines
    g.setColour (Pal::line);
    g.fillRoundedRectangle (juce::Rectangle<float> (area.getX() + 10.0f, outY - 3.0f, area.getWidth() - 20.0f, 3.0f), 1.5f);
    for (int k = 0; k < numOps; ++k)
        if (r.isCarrier (k))
        {
            g.setColour (Pal::teal.withAlpha (0.7f));
            g.drawLine (boxes[(size_t) k].getCentreX(), boxes[(size_t) k].getBottom(), boxes[(size_t) k].getCentreX(), outY - 3.0f, 1.6f);
        }
    // modulation lines
    for (int src = 0; src < numOps; ++src)
        for (int dst = 0; dst < numOps; ++dst)
            if (src != dst && r.has (src, dst) && depth[(size_t) src] >= 0 && depth[(size_t) dst] >= 0)
            {
                const auto a = boxes[(size_t) src], b = boxes[(size_t) dst];
                juce::Path p;
                if (a.getBottom() <= b.getY())
                {
                    p.startNewSubPath (a.getCentreX(), a.getBottom());
                    p.lineTo (b.getCentreX(), b.getY());
                }
                else
                {   // backwards connection: route around the side
                    const float side = juce::jmax (a.getRight(), b.getRight()) + 10.0f;
                    p.startNewSubPath (a.getRight(), a.getCentreY());
                    p.cubicTo (side, a.getCentreY(), side, b.getCentreY(), b.getRight(), b.getCentreY());
                }
                g.setColour (Pal::coral.withAlpha (0.8f));
                g.strokePath (p, juce::PathStrokeType (1.6f));
            }
    // boxes + feedback loops
    for (int k = 0; k < numOps; ++k)
    {
        const auto b = boxes[(size_t) k];
        const bool used = depth[(size_t) k] >= 0;
        const float lvl = proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Level"))->load();
        const float fb = proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Feedback"))->load();
        const int mode = (int) proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Mode"))->load();
        if (fb > 0.01f && mode == modeFM && used)
        {
            juce::Path loop;
            loop.addArc (b.getRight() - 6.0f, b.getY() - 9.0f, 14.0f, 14.0f, 0.0f, juce::MathConstants<float>::pi * 1.5f);
            g.setColour (Pal::coral.withAlpha (0.4f + 0.6f * fb));
            g.strokePath (loop, juce::PathStrokeType (1.5f));
        }
        const auto col = ! used ? Pal::knob : (r.isCarrier (k) ? Pal::teal : Pal::coral);
        g.setColour (col.withAlpha (used ? 0.25f + 0.75f * lvl / 99.0f : 0.6f));
        g.fillRoundedRectangle (b, 5.0f);
        if (k == selected)
        {
            g.setColour (Pal::ink);
            g.drawRoundedRectangle (b.expanded (2.0f), 6.0f, 1.5f);
        }
        g.setColour (used ? Pal::bg : Pal::inkDim);
        g.setFont (sans (12.0f, true));
        g.drawText (juce::String (k + 1), b, juce::Justification::centred);
        if (mode != modeFM && used)
        {
            // mode tag as a small pill to the right of its own box (above it would land on the next operator in a stack)
            const char* tag[] { "", "RING", "FILT", "FOLD" };
            const auto pill = juce::Rectangle<float> (b.getRight() + 5.0f, b.getCentreY() + 1.0f, 30.0f, 12.0f);
            g.setColour (Pal::bg.withAlpha (0.85f));
            g.fillRoundedRectangle (pill, 6.0f);
            g.setColour (col);
            g.drawRoundedRectangle (pill, 6.0f, 1.0f);
            g.setFont (sans (8.5f, true));
            g.drawText (tag[mode], pill, juce::Justification::centred);
        }
    }
}

void AlgoView::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    if (editing)
    {
        for (int row = 0; row <= numOps; ++row)
            for (int c = 0; c < numOps; ++c)
                if (matrixCell (row, c).contains (pos) && row != c)
                {
                    proc.editRouting (row == numOps ? c : row, c, row == numOps);
                    repaint();
                    return;
                }
        return;
    }
    for (int k = 0; k < numOps; ++k)
        if (boxes[(size_t) k].expanded (3.0f).contains (pos) && onSelect) { onSelect (k); return; }
}

//==============================================================================
SextantEditor::SextantEditor (SextantProcessor& p)
    : AudioProcessorEditor (p), proc (p), algoView (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    // ---- header: presets + algorithm stepping
    for (int i = 0; i < proc.getNumPrograms(); ++i) preset.addItem (proc.getProgramName (i), i + 1);
    preset.onChange = [this] { const int i = preset.getSelectedId() - 1; if (i >= 0 && i != proc.getCurrentProgram()) proc.setCurrentProgram (i); lastProgram = proc.getCurrentProgram(); };
    preset.setTooltip ("Factory sounds.");
    auto stepPreset = [this] (int d) { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n); };
    presetPrev.onClick = [stepPreset] { stepPreset (-1); };
    presetNext.onClick = [stepPreset] { stepPreset (1); };
    auto stepAlgo = [this] (int d)
    {
        auto* prm = proc.apvts.getParameter ("algo");
        const int n = SextantProcessor::userAlgorithmIndex() + 1;
        const int cur = (int) proc.apvts.getRawParameterValue ("algo")->load();
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ((cur + d + n) % n)));
        prm->endChangeGesture();
    };
    algoPrev.onClick = [stepAlgo] { stepAlgo (-1); };
    algoNext.onClick = [stepAlgo] { stepAlgo (1); };
    editAlgo.setTooltip ("Draw your own algorithm in the routing matrix. Editing switches to the User algorithm, starting from the current one.");
    editAlgo.onClick = [this] { algoView.editing = editAlgo.getToggleState(); algoView.repaint(); };
    algoBox.addItemList (SextantProcessor::algorithmNames(), 1);
    algoBox.setTooltip ("Algorithm: how the six operators are wired. Arrows step through them.");
    algoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "algo", algoBox);
    for (juce::Component* c : { (juce::Component*) &preset, (juce::Component*) &presetPrev, (juce::Component*) &presetNext,
                                (juce::Component*) &algoPrev, (juce::Component*) &algoNext, (juce::Component*) &editAlgo,
                                (juce::Component*) &algoBox })
        addAndMakeVisible (c);

    algoView.onSelect = [this] (int k) { selectOp (k); };
    addAndMakeVisible (algoView);

    // ---- operator mixer: the six levels are the main performance controls
    for (int k = 0; k < numOps; ++k)
    {
        auto& s = strips[(size_t) k];
        s.select.setButtonText ("OP " + juce::String (k + 1));
        s.select.setClickingTogglesState (false);
        s.select.onClick = [this, k] { selectOp (k); };
        s.select.setTooltip ("Select operator " + juce::String (k + 1) + " for editing.");
        addAndMakeVisible (s.select);
        s.level.setSliderStyle (juce::Slider::LinearVertical);
        s.level.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        s.level.setTooltip ("Operator level. On a carrier: loudness. On a modulator: brightness (modulation depth).");
        addAndMakeVisible (s.level);
        s.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, SextantProcessor::opId (k, "Level"), s.level);
    }

    // ---- operator editors
    const char* modeTips[] { "FM: a sine-family oscillator whose phase is modulated by its inputs.",
                             "Ring: multiplies the inputs with this oscillator (sum and difference tones, metallic).",
                             "Filter: the operator becomes a resonant filter for its inputs; its frequency is the cutoff.",
                             "Fold: wavefolder; its own wave plus the inputs are folded, more drive = more harmonics." };
    for (int k = 0; k < numOps; ++k)
    {
        auto& u = opUI[(size_t) k];
        auto id = [k] (const char* n) { return SextantProcessor::opId (k, n); };
        for (int m = 0; m < numModes; ++m)
        {
            auto& b = u.modeButtons[(size_t) m];
            b.setButtonText (SextantProcessor::modeNames[m].toUpperCase());
            b.setTooltip (modeTips[m]);
            b.onClick = [this, k, m]
            {
                auto* prm = proc.apvts.getParameter (SextantProcessor::opId (k, "Mode"));
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) m));
                prm->endChangeGesture();
            };
            addChildComponent (b);
        }
        setupCombo (u.wave, id ("Wave"), "WAVE", "Oscillator waveform.");
        u.wave.box.addItemList (SextantProcessor::waveNames, 1);
        u.wave.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id ("Wave"), u.wave.box);
        setupCombo (u.filterType, id ("FiltType"), "FILTER", "Filter type in Filter mode.");
        u.filterType.box.addItemList ({ "Low Pass", "Band Pass", "High Pass" }, 1);
        u.filterType.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id ("FiltType"), u.filterType.box);
        u.fixed.setTooltip ("Off: frequency follows the keyboard as a ratio. On: fixed frequency in Hz (drums, formants).");
        addChildComponent (u.fixed);
        u.fixedAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id ("Fixed"), u.fixed);

        makeKnob (u.knobs, id ("Coarse"),   "RATIO",     "Frequency ratio to the played note (0.5, 1, 2 ... 32).");
        makeKnob (u.knobs, id ("Fine"),     "FINE",      "Multiplies the ratio (x1.000 - x2.000) for inharmonic, bell-like ratios.");
        makeKnob (u.knobs, id ("Detune"),   "DETUNE",    "Detune in cents. Small offsets between carriers make the sound move.", true);
        makeKnob (u.knobs, id ("Freq"),     "FREQ",      "Frequency in Fixed mode.");
        makeKnob (u.knobs, id ("Feedback"), "FEEDBACK",  "Feeds the operator into itself: sine towards saw (FM mode).");
        makeKnob (u.knobs, id ("Vel"),      "VELOCITY",  "Velocity sensitivity. On modulators: harder = brighter.");
        makeKnob (u.knobs, id ("KeyLvl"),   "KEY LVL",   "Level change per octave (+-6 dB). Negative tames bright high notes.", true);
        makeKnob (u.knobs, id ("Res"),      "RES",       "Resonance in Filter mode.");
        makeKnob (u.knobs, id ("Drive"),    "DRIVE",     "Fold depth in Fold mode.");
        makeKnob (u.knobs, id ("A"),        "ATTACK",    "Envelope attack.");
        makeKnob (u.knobs, id ("D"),        "DECAY",     "Envelope decay.");
        makeKnob (u.knobs, id ("S"),        "SUSTAIN",   "Envelope sustain level.");
        makeKnob (u.knobs, id ("R"),        "RELEASE",   "Envelope release.");
    }

    // ---- global tabs
    const char* tabNames[] { "VOICE", "FILTER", "LFO", "FX" };
    for (int t = 0; t < 4; ++t)
    {
        tabs[(size_t) t].setButtonText (tabNames[t]);
        tabs[(size_t) t].onClick = [this, t] { showTab (t); };
        addAndMakeVisible (tabs[(size_t) t]);
    }
    setupCombo (voiceMode, "voiceMode", "MODE", "Poly, Mono (retriggers) or Legato (glides without retriggering).");
    voiceMode.box.addItemList ({ "Poly", "Mono", "Legato" }, 1);
    voiceMode.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "voiceMode", voiceMode.box);
    makeKnob (tabKnobs[0], "poly",      "VOICES",  "Maximum notes at once.");
    makeKnob (tabKnobs[0], "unison",    "UNISON",  "Stacked voices per note.");
    makeKnob (tabKnobs[0], "uniDetune", "DETUNE",  "Spread of the unison voices in cents.");
    makeKnob (tabKnobs[0], "uniSpread", "SPREAD",  "Stereo spread of the unison voices.");
    makeKnob (tabKnobs[0], "glide",     "GLIDE",   "Portamento time in Mono / Legato.");
    makeKnob (tabKnobs[0], "bend",      "BEND",    "Pitch bend range in semitones.");
    makeKnob (tabKnobs[0], "volume",    "VOLUME",  "Output level.");

    setupCombo (filterType, "fType", "TYPE", "Voice filter after the operators.");
    filterType.box.addItemList ({ "Off", "LP 12", "LP 24", "Band Pass", "High Pass" }, 1);
    filterType.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "fType", filterType.box);
    makeKnob (tabKnobs[1], "cutoff", "CUTOFF",  "Filter cutoff.");
    makeKnob (tabKnobs[1], "res",    "RES",     "Filter resonance.");
    makeKnob (tabKnobs[1], "fEnv",   "ENV",     "Envelope amount (+-6 octaves).", true);
    makeKnob (tabKnobs[1], "fKey",   "KEY",     "Keyboard tracking.");
    makeKnob (tabKnobs[1], "fVel",   "VEL",     "Velocity scales the envelope amount.");
    makeKnob (tabKnobs[1], "fA",     "ATTACK",  "Filter envelope attack.");
    makeKnob (tabKnobs[1], "fD",     "DECAY",   "Filter envelope decay.");
    makeKnob (tabKnobs[1], "fS",     "SUSTAIN", "Filter envelope sustain.");
    makeKnob (tabKnobs[1], "fR",     "RELEASE", "Filter envelope release.");

    setupCombo (lfoWave, "lfoWave", "WAVE", "LFO waveform.");
    lfoWave.box.addItemList ({ "Sine", "Triangle", "Square", "Saw", "S&H" }, 1);
    lfoWave.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "lfoWave", lfoWave.box);
    makeKnob (tabKnobs[2], "lfoRate",   "RATE",    "LFO speed.");
    makeKnob (tabKnobs[2], "lfoDelay",  "DELAY",   "Fade-in time after the first note.");
    makeKnob (tabKnobs[2], "lfoPitch",  "PITCH",   "Vibrato depth.");
    makeKnob (tabKnobs[2], "lfoFm",     "FM",      "Modulates all modulator levels: timbre wobble.");
    makeKnob (tabKnobs[2], "lfoAmp",    "AMP",     "Tremolo.");
    makeKnob (tabKnobs[2], "lfoFilter", "FILTER",  "Filter cutoff modulation.");
    makeKnob (tabKnobs[2], "mwVib",     "WHEEL",   "Mod wheel adds vibrato.");

    setupCombo (delaySync, "dSync", "SYNC", "Delay time synced to the host tempo, or free.");
    delaySync.box.addItemList ({ "Free", "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2" }, 1);
    delaySync.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "dSync", delaySync.box);
    makeKnob (tabKnobs[3], "chorus", "CHORUS",   "Stereo chorus.");
    makeKnob (tabKnobs[3], "dMix",   "DELAY",    "Ping-pong delay level.");
    makeKnob (tabKnobs[3], "dTime",  "TIME",     "Delay time when not synced.");
    makeKnob (tabKnobs[3], "dFb",    "FEEDBACK", "Delay repeats.");

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (17.0f);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffc9cdd4));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, Pal::bg);
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, Pal::line);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Pal::teal.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Pal::teal);
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, Pal::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, Pal::inkDim);
    addAndMakeVisible (keyboard);

    setSize (1240, 760);
    selectOp (0);
    showTab (0);
    timerCallback();
    startTimerHz (30);
}

SextantEditor::~SextantEditor() { setLookAndFeel (nullptr); }

SextantEditor::Knob* SextantEditor::makeKnob (KnobList& into, const juce::String& id, const juce::String& name, const juce::String& tip, bool centred)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 15);
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

void SextantEditor::setupCombo (Combo& c, const juce::String&, const juce::String& name, const juce::String& tip)
{
    c.box.setTooltip (tip);
    addChildComponent (c.box);
    c.label.setText (name, juce::dontSendNotification);
    c.label.setFont (sans (10.0f, true));
    c.label.setJustificationType (juce::Justification::centredLeft);
    addChildComponent (c.label);
}

void SextantEditor::selectOp (int op)
{
    selectedOp = op;
    algoView.selected = op;
    for (int k = 0; k < numOps; ++k)
    {
        auto& u = opUI[(size_t) k];
        const bool vis = k == op;
        for (auto& b : u.modeButtons) b.setVisible (vis);
        for (auto* c : { (juce::Component*) &u.wave.box, (juce::Component*) &u.wave.label, (juce::Component*) &u.fixed,
                         (juce::Component*) &u.filterType.box, (juce::Component*) &u.filterType.label })
            c->setVisible (vis);
        for (auto& kn : u.knobs) { kn->slider.setVisible (vis); kn->label.setVisible (vis); }
    }
    resized();
    repaint();
}

void SextantEditor::showTab (int tab)
{
    currentTab = tab;
    for (int t = 0; t < 4; ++t)
    {
        tabs[(size_t) t].setToggleState (t == tab, juce::dontSendNotification);
        for (auto& k : tabKnobs[(size_t) t]) { k->slider.setVisible (t == tab); k->label.setVisible (t == tab); }
    }
    Combo* combos[] { &voiceMode, &filterType, &lfoWave, &delaySync };
    for (int t = 0; t < 4; ++t) { combos[t]->box.setVisible (t == tab); combos[t]->label.setVisible (t == tab); }
    resized();
}

void SextantEditor::timerCallback()
{
    const auto r = proc.currentRouting();
    for (int k = 0; k < numOps; ++k)
    {
        auto& s = strips[(size_t) k];
        meter[(size_t) k] = juce::jmax (meter[(size_t) k] * 0.85f, proc.activity()[(size_t) k].load());
        const bool carrier = r.isCarrier (k);
        if ((bool) s.level.getProperties()["carrier"] != carrier || std::abs ((float) s.level.getProperties()["meter"] - meter[(size_t) k]) > 0.005f)
        {
            s.level.getProperties().set ("carrier", carrier);
            s.level.getProperties().set ("meter", meter[(size_t) k]);
            s.level.repaint();
        }
        s.select.getProperties().set ("accent", (juce::int64) (carrier ? Pal::teal : Pal::coral).getARGB());
        s.select.setToggleState (k == selectedOp, juce::dontSendNotification);

        // operator editor: highlight the mode, dim what does not apply
        auto& u = opUI[(size_t) k];
        const int mode = (int) proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Mode"))->load();
        const bool fixed = proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Fixed"))->load() > 0.5f;
        for (int m = 0; m < numModes; ++m) u.modeButtons[(size_t) m].setToggleState (m == mode, juce::dontSendNotification);
        auto dim = [] (Knob& kn, bool on) { kn.slider.setAlpha (on ? 1.0f : 0.45f); kn.label.setAlpha (on ? 1.0f : 0.45f); };
        dim (*u.knobs[0], ! fixed); dim (*u.knobs[1], ! fixed); dim (*u.knobs[3], fixed);
        dim (*u.knobs[4], mode == modeFM); dim (*u.knobs[7], mode == modeFilter); dim (*u.knobs[8], mode == modeFold);
        u.filterType.box.setEnabled (mode == modeFilter);
        u.wave.box.setEnabled (mode != modeFilter);
    }
    if (proc.getCurrentProgram() != lastProgram)
    {
        lastProgram = proc.getCurrentProgram();
        preset.setSelectedId (lastProgram + 1, juce::dontSendNotification);
    }
    // everything the editor paints itself (wave preview, mode/ratio captions, carrier tag):
    // repaint when any of it changes, so no stale pixels are left behind
    juce::int64 sig = selectedOp * 7919 + (juce::int64) r.conn * 31 + r.carriers;
    auto raw = [this] (int k, const char* n) { return proc.apvts.getRawParameterValue (SextantProcessor::opId (k, n))->load(); };
    for (int k = 0; k < numOps; ++k)
        sig = sig * 1000003 + (juce::int64) (raw (k, "Mode") * 7 + raw (k, "Wave") * 131 + raw (k, "Fixed") * 977
                                              + raw (k, "Coarse") * 1009 + std::round (raw (k, "Fine") * 1000.0f) * 3 + std::round (raw (k, "Freq")) * 17);
    if (sig != paintedSignature)
    {
        paintedSignature = sig;
        repaint();
    }
    algoView.repaint();
    repaint (envPanel);
}

//==============================================================================
void SextantEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg);
    g.setColour (Pal::ink);
    g.setFont (sans (26.0f, true));
    g.drawText ("SEXTANT", header.withWidth (140), juce::Justification::centredLeft);
    g.setColour (Pal::teal);
    g.setFont (sans (11.0f, true));
    g.drawText ("6-OPERATOR FM", header.withTrimmedLeft (138).withWidth (120).translated (0, 3), juce::Justification::centredLeft);
    g.setColour (Pal::inkDim);
    g.setFont (sans (10.0f, true));
    g.drawText ("UNVRS", header.withTrimmedLeft (246).withWidth (60).translated (0, 3), juce::Justification::centredLeft);

    for (auto p : { algoPanel, mixerPanel, envPanel, opPanel, globalPanel })
    {
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (p.toFloat(), 10.0f);
    }
    g.setColour (Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText ("OPERATORS", mixerPanel.getX() + 14, mixerPanel.getY() + 8, 120, 18, juce::Justification::centredLeft);

    // ratio / frequency under each fader
    const auto r = proc.currentRouting();
    g.setFont (sans (10.5f, true));
    for (int k = 0; k < numOps; ++k)
    {
        const auto& s = strips[(size_t) k];
        const bool fixed = proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Fixed"))->load() > 0.5f;
        const int mode = (int) proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Mode"))->load();
        juce::String txt;
        if (fixed) txt = juce::String (juce::roundToInt (proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Freq"))->load())) + " Hz";
        else
        {
            const float ratio = SextantProcessor::coarseToRatio ((int) proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Coarse"))->load())
                              * (1.0f + proc.apvts.getRawParameterValue (SextantProcessor::opId (k, "Fine"))->load());
            txt = "x" + juce::String (ratio, ratio < 10 ? 2 : 1);
        }
        auto info = juce::Rectangle<int> (s.select.getX(), s.select.getBottom() + 2, s.select.getWidth(), 14);
        g.setColour (r.isCarrier (k) ? Pal::teal : Pal::coral);
        g.drawText (SextantProcessor::modeNames[mode].toUpperCase() + "  " + txt, info, juce::Justification::centred);
    }

    // operator editor title
    g.setColour (Pal::ink);
    g.setFont (sans (15.0f, true));
    g.drawText ("OP " + juce::String (selectedOp + 1), opPanel.getX() + 14, opPanel.getY() + 8, 80, 22, juce::Justification::centredLeft);
    g.setColour (r.isCarrier (selectedOp) ? Pal::teal : Pal::coral);
    g.setFont (sans (10.0f, true));
    g.drawText (r.isCarrier (selectedOp) ? "CARRIER" : "MODULATOR", opPanel.getX() + 70, opPanel.getY() + 12, 90, 16, juce::Justification::centredLeft);

    paintEnvelope (g);
    paintWave (g);
}

void SextantEditor::paintWave (juce::Graphics& g)
{
    if (waveArea.isEmpty()) return;
    const int w = (int) proc.apvts.getRawParameterValue (SextantProcessor::opId (selectedOp, "Wave"))->load();
    auto a = waveArea.toFloat();
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (a, 6.0f);
    juce::Path p;
    const auto& tab = waves().t[(size_t) juce::jlimit (0, numWaves - 1, w)];
    juce::Random rnd (3);
    for (int i = 0; i <= 64; ++i)
    {
        const float v = w == wNoise ? rnd.nextFloat() * 2.0f - 1.0f : tab[(size_t) (i * WaveTables::size / 64) % WaveTables::size];
        const juce::Point<float> pt (a.getX() + 4.0f + (a.getWidth() - 8.0f) * i / 64.0f, a.getCentreY() - v * (a.getHeight() * 0.38f));
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    g.setColour (Pal::teal);
    g.strokePath (p, juce::PathStrokeType (1.5f));
}

void SextantEditor::paintEnvelope (juce::Graphics& g)
{
    auto area = envPanel.toFloat().reduced (14.0f, 10.0f);
    g.setColour (Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText ("OP " + juce::String (selectedOp + 1) + " ENVELOPE", area.removeFromTop (18.0f), juce::Justification::centredLeft);
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 8.0f);
    auto pl = area.reduced (10.0f, 12.0f);

    auto v = [this] (const char* n) { return proc.apvts.getRawParameterValue (SextantProcessor::opId (selectedOp, n))->load(); };
    const float A = v ("A"), D = v ("D"), S = v ("S"), R = v ("R");
    // each segment gets width by the cube root of its time, so short and long envelopes both read well
    auto seg = [] (float t) { return std::cbrt (juce::jmax (0.0005f, t)); };
    const float hold = seg (0.4f);
    const float total = seg (A) + seg (D) + hold + seg (R);
    const float sx = pl.getWidth() / total;
    const float x0 = pl.getX(), x1 = x0 + seg (A) * sx, x2 = x1 + seg (D) * sx, x3 = x2 + hold * sx, x4 = x3 + seg (R) * sx;
    auto y = [&] (float lvl) { return pl.getBottom() - lvl * pl.getHeight(); };
    juce::Path p;
    p.startNewSubPath (x0, y (0));
    p.quadraticTo (x0 + (x1 - x0) * 0.4f, y (0.75f), x1, y (1));
    p.quadraticTo (x1 + (x2 - x1) * 0.25f, y (S), x2, y (S));
    p.lineTo (x3, y (S));
    p.quadraticTo (x3 + (x4 - x3) * 0.2f, y (0), x4, y (0));
    auto fill = p;
    fill.lineTo (x0, y (0));
    fill.closeSubPath();
    g.setColour (Pal::teal.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (Pal::teal);
    g.strokePath (p, juce::PathStrokeType (2.0f));
    g.setColour (Pal::inkDim);
    g.setFont (sans (9.5f, true));
    g.drawText ("A", juce::Rectangle<float> (x0, pl.getBottom() + 1.0f, x1 - x0, 11.0f), juce::Justification::centred);
    g.drawText ("D", juce::Rectangle<float> (x1, pl.getBottom() + 1.0f, x2 - x1, 11.0f), juce::Justification::centred);
    g.drawText ("S", juce::Rectangle<float> (x2, pl.getBottom() + 1.0f, x3 - x2, 11.0f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (x3, pl.getBottom() + 1.0f, x4 - x3, 11.0f), juce::Justification::centred);
}

//==============================================================================
void SextantEditor::layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, const std::vector<juce::Label*>& labels, int width)
{
    for (size_t i = 0; i < items.size(); ++i)
    {
        auto col = area.removeFromLeft (width);
        if (labels[i] != nullptr) labels[i]->setBounds (col.removeFromTop (14));
        items[i]->setBounds (col);
    }
}

void SextantEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 0);
    header = b.removeFromTop (54).withTrimmedTop (4);
    {
        auto h = header;
        presetNext.setBounds (h.removeFromRight (30).withSizeKeepingCentre (30, 30));
        preset.setBounds (h.removeFromRight (210).withSizeKeepingCentre (204, 30));
        presetPrev.setBounds (h.removeFromRight (30).withSizeKeepingCentre (30, 30));
    }

    keyboard.setBounds (b.removeFromBottom (72).withTrimmedBottom (10));
    b.removeFromBottom (8);

    auto rowA = b.removeFromTop (262);
    algoPanel = rowA.removeFromLeft (330);
    rowA.removeFromLeft (10);
    mixerPanel = rowA.removeFromLeft (6 * 96 + 20);
    rowA.removeFromLeft (10);
    envPanel = rowA;
    {
        // algorithm selection sits on top of the diagram it controls
        auto top = algoPanel.reduced (10, 8).removeFromTop (28);
        editAlgo.setBounds (top.removeFromRight (58).reduced (0, 1));
        top.removeFromRight (8);
        algoPrev.setBounds (top.removeFromLeft (28));
        algoNext.setBounds (top.removeFromRight (28));
        algoBox.setBounds (top.reduced (4, 0));
    }
    algoView.setBounds (algoPanel.reduced (10).withTrimmedTop (32));

    auto mix = mixerPanel.reduced (10).withTrimmedTop (22);
    for (int k = 0; k < numOps; ++k)
    {
        auto col = mix.removeFromLeft (96).reduced (6, 0);
        auto& s = strips[(size_t) k];
        s.select.setBounds (col.removeFromTop (26));
        col.removeFromTop (18);
        s.level.setBounds (col);
    }

    b.removeFromTop (10);
    opPanel = b.removeFromTop (196);
    b.removeFromTop (10);
    globalPanel = b;

    // operator editor
    {
        auto& u = opUI[(size_t) selectedOp];
        auto area = opPanel.reduced (12, 10);
        auto left = area.removeFromLeft (250);
        left.removeFromTop (28);
        auto modes = left.removeFromTop (28);
        for (auto& mb : u.modeButtons) mb.setBounds (modes.removeFromLeft (modes.getWidth() / (int) (&u.modeButtons.back() - &mb + 1)).reduced (2, 0));
        left.removeFromTop (8);
        auto waveRow = left.removeFromTop (44);
        u.wave.label.setBounds (waveRow.removeFromTop (14));
        u.wave.box.setBounds (waveRow.removeFromLeft (130).withHeight (26));
        waveRow.removeFromLeft (8);
        waveArea = waveRow.withHeight (26);
        left.removeFromTop (6);
        auto fr = left.removeFromTop (44);
        u.filterType.label.setBounds (fr.removeFromTop (14));
        u.filterType.box.setBounds (fr.removeFromLeft (130).withHeight (26));
        fr.removeFromLeft (8);
        u.fixed.setBounds (fr.withHeight (26));

        area.removeFromLeft (10);
        auto knobs = area.withTrimmedTop (30).withHeight (100);
        const int kw = knobs.getWidth() / 13;
        for (auto& kn : u.knobs)
        {
            auto col = knobs.removeFromLeft (kw);
            kn->label.setBounds (col.removeFromTop (14));
            kn->slider.setBounds (col);
        }
    }

    // global tabs
    {
        auto area = globalPanel.reduced (12, 8);
        auto tabRow = area.removeFromTop (26);
        for (auto& t : tabs) t.setBounds (tabRow.removeFromLeft (90).reduced (2, 0));
        area.removeFromTop (6);
        Combo* combos[] { &voiceMode, &filterType, &lfoWave, &delaySync };
        auto c = area.removeFromLeft (130);
        combos[currentTab]->label.setBounds (c.removeFromTop (14));
        combos[currentTab]->box.setBounds (c.removeFromTop (28).reduced (0, 1));
        area.removeFromLeft (10);
        for (auto& k : tabKnobs[(size_t) currentTab])
        {
            auto col = area.removeFromLeft (70);
            k->label.setBounds (col.removeFromTop (14));
            k->slider.setBounds (col);
        }
    }
}
