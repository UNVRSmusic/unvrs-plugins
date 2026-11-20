#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour plate   { 0xffe7ddcb };
    const juce::Colour plateDk { 0xffd8ccb6 };
    const juce::Colour edge    { 0xffb9aa90 };
    const juce::Colour ink     { 0xff2b2420 };
    const juce::Colour inkDim  { 0xff76695a };
    const juce::Colour wine    { 0xff7d2a2a };
    const juce::Colour screen  { 0xfff6f1e6 };
    const juce::Colour lampOn  { 0xffff9a3c };
    const juce::Colour filter  { 0xff5a5048 };

    inline juce::Colour band (int b)
    {
        static const juce::Colour c[] { juce::Colour (0xffb0413e), juce::Colour (0xffc8792a), juce::Colour (0xff9a8a1e),
                                        juce::Colour (0xff3f8f6b), juce::Colour (0xff3b6fa8), juce::Colour (0xff7b4f9e) };
        return c[juce::jlimit (0, 5, b)];
    }
}

static juce::Font serif (float size, bool bold = false) { return juce::FontOptions ("Georgia", size, bold ? juce::Font::bold : juce::Font::plain); }
static juce::Font sans (float size, bool bold = false)  { return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain); }

//==============================================================================
VelourPLookAndFeel::VelourPLookAndFeel()
{
    setColour (juce::Label::textColourId, Pal::ink);
    setColour (juce::Slider::textBoxTextColourId, Pal::inkDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Pal::wine.withAlpha (0.3f));
    setColour (juce::TextEditor::textColourId, Pal::ink);
    setColour (juce::CaretComponent::caretColourId, Pal::wine);
    setColour (juce::ComboBox::textColourId, Pal::ink);
    setColour (juce::PopupMenu::backgroundColourId, Pal::screen);
    setColour (juce::PopupMenu::textColourId, Pal::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Pal::wine);
    setColour (juce::PopupMenu::highlightedTextColourId, Pal::plate);
    setColour (juce::TooltipWindow::backgroundColourId, Pal::screen);
    setColour (juce::TooltipWindow::textColourId, Pal::ink);
    setColour (juce::TooltipWindow::outlineColourId, Pal::edge);
}

void VelourPLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = juce::Colour::fromString (s.getProperties().getWithDefault ("accent", Pal::wine.toString()).toString());
    const bool centred = (bool) s.getProperties().getWithDefault ("centred", false);

    // value arc on the scale ring
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::edge);
    g.strokePath (track, juce::PathStrokeType (2.0f));
    const float from = centred ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path v;
        v.addCentredArc (c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent);
        g.strokePath (v, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = r - 6.0f;
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillEllipse (c.x - kr + 1.0f, c.y - kr + 2.0f, kr * 2.0f, kr * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3834), c.x, c.y - kr, juce::Colour (0xff100e0d), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (c.x - kr * 0.66f, c.y - kr * 0.66f, kr * 1.32f, kr * 1.32f, 1.0f);

    g.setColour (accent.interpolatedWith (Pal::plate, 0.35f));
    g.drawLine ({ c.getPointOnCircumference (kr * 0.25f, angle), c.getPointOnCircumference (kr - 1.0f, angle) }, 2.0f);
}

void VelourPLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = juce::Colour::fromString (b.getProperties().getWithDefault ("accent", Pal::wine.toString()).toString());

    g.setColour (on ? accent : Pal::plateDk);
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (highlighted ? Pal::ink : Pal::edge);
    g.drawRoundedRectangle (area, 4.0f, 1.0f);
    g.setColour (on ? Pal::plate : Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText (b.getButtonText(), area, juce::Justification::centred);
}

void VelourPLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (box.isMouseOver (true) ? Pal::ink : Pal::edge);
    g.drawRoundedRectangle (area, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 10.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (Pal::inkDim);
    g.fillPath (arrow);
}

juce::Font VelourPLookAndFeel::getComboBoxFont (juce::ComboBox&) { return sans (11.5f); }

//==============================================================================
CurveView::CurveView (VelourPProcessor& p) : proc (p)
{
    setTooltip ("Drag a point: frequency and gain. Mouse wheel: Q (bands) or slope (filters).\nDouble-click: reset the band to 0 dB / switch a filter off.");
}

float CurveView::xFor (double f) const { const auto pl = plot(); return pl.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * pl.getWidth(); }
double CurveView::fAt (float x) const { const auto pl = plot(); return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - pl.getX()) / pl.getWidth()))); }
float CurveView::yFor (double db) const { const auto pl = plot(); return pl.getCentreY() - (float) (juce::jlimit (-20.0, 20.0, db) / 20.0) * pl.getHeight() * 0.5f; }
double CurveView::dbAt (float y) const { const auto pl = plot(); return juce::jlimit (-18.0, 18.0, (double) ((pl.getCentreY() - y) / (pl.getHeight() * 0.5f) * 20.0f)); }

juce::Point<float> CurveView::nodePos (int node) const
{
    const auto s = proc.readSettings();
    if (node == hpfNode) return { xFor (juce::jmax (20.0, s.hpf > 0 ? s.hpf : 20.0)), yFor (0.0) };
    if (node == lpfNode) return { xFor (juce::jmin (20000.0, s.lpf > 0 ? s.lpf : 20000.0)), yFor (0.0) };
    const auto& b = s.bands[(size_t) node];
    return { xFor (b.freq), yFor (b.gain) };
}

int CurveView::hitNode (juce::Point<float> p) const
{
    int best = noNode;
    float bestD = 13.0f;
    for (int n = hpfNode - 1; n < VelourPProcessor::numBands; ++n)
    {
        if (n == hpfNode - 1) continue; // nodes: lpfNode(-2), hpfNode(-1), 0..5
        const float d = nodePos (n).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = n; }
    }
    return best;
}

void CurveView::setParam (const juce::String& id, float value)
{
    if (auto* prm = param (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void CurveView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (hover != noNode ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void CurveView::mouseExit (const juce::MouseEvent&) { hover = noNode; repaint(); }

void CurveView::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitNode (e.position);
    if (dragging == noNode)
        return;
    if (onSelect) onSelect (dragging);

    if (dragging == hpfNode) param ("hpf")->beginChangeGesture();
    else if (dragging == lpfNode) param ("lpf")->beginChangeGesture();
    else
    {
        param (VelourPProcessor::bandId (dragging, "Freq"))->beginChangeGesture();
        param (VelourPProcessor::bandId (dragging, "Gain"))->beginChangeGesture();
    }
}

void CurveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == noNode)
        return;
    const double f = fAt (e.position.x);
    if (dragging == hpfNode)      setParam ("hpf", (float) juce::jlimit (10.0, 1000.0, f));
    else if (dragging == lpfNode) setParam ("lpf", (float) juce::jlimit (1000.0, 22000.0, f));
    else
    {
        setParam (VelourPProcessor::bandId (dragging, "Freq"), (float) f);
        if (! e.mods.isShiftDown()) // shift = frequency only
            setParam (VelourPProcessor::bandId (dragging, "Gain"), (float) dbAt (e.position.y));
    }
    repaint();
}

void CurveView::mouseUp (const juce::MouseEvent&)
{
    if (dragging == hpfNode) param ("hpf")->endChangeGesture();
    else if (dragging == lpfNode) param ("lpf")->endChangeGesture();
    else if (dragging != noNode)
    {
        param (VelourPProcessor::bandId (dragging, "Freq"))->endChangeGesture();
        param (VelourPProcessor::bandId (dragging, "Gain"))->endChangeGesture();
    }
    dragging = noNode;
}

void CurveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int n = hitNode (e.position);
    auto reset = [this] (const juce::String& id, float v)
    {
        auto* prm = param (id);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        prm->endChangeGesture();
    };
    if (n == hpfNode)      reset ("hpf", 10.0f);
    else if (n == lpfNode) reset ("lpf", 22000.0f);
    else if (n >= 0)       reset (VelourPProcessor::bandId (n, "Gain"), 0.0f);
    repaint();
}

void CurveView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int n = hover != noNode ? hover : hitNode (e.position);
    if (n == noNode)
        return;
    if (n == hpfNode || n == lpfNode)
    {
        auto* prm = param (n == hpfNode ? "hpfSlope" : "lpfSlope");
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (w.deltaY > 0 ? 1.0f : 0.0f);
        prm->endChangeGesture();
    }
    else
    {
        auto* prm = param (VelourPProcessor::bandId (n, "Q"));
        const float q = prm->convertFrom0to1 (prm->getValue()) * std::pow (1.15f, w.deltaY * 8.0f);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (juce::jlimit (0.1f, 10.0f, q)));
        prm->endChangeGesture();
    }
    repaint();
}

void CurveView::updateAnalyser()
{
    // drain the processor FIFO into our ring
    const int ready = proc.analyserFifo.getNumReady();
    if (ready > 0)
    {
        const auto scope = proc.analyserFifo.read (ready);
        scope.forEach ([this] (int idx)
        {
            ring[(size_t) ringPos] = proc.analyserData[(size_t) idx];
            ringPos = (ringPos + 1) % fftSize;
        });
    }

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const auto pl = plot();
    const int w = juce::jmax (1, (int) pl.getWidth());
    if ((int) spectrum.size() != w)
        spectrum.assign ((size_t) w, -120.0f);

    const double rate = juce::jmax (1.0, proc.getSampleRate());
    for (int x = 0; x < w; ++x)
    {
        const double f0 = fAt (pl.getX() + (float) x), f1 = fAt (pl.getX() + (float) x + 1.0f);
        int b0 = (int) (f0 / rate * fftSize), b1 = juce::jmax (b0 + 1, (int) (f1 / rate * fftSize));
        b0 = juce::jlimit (1, fftSize / 2 - 1, b0);
        b1 = juce::jlimit (b0 + 1, fftSize / 2, b1);
        float m = 0.0f;
        for (int b = b0; b < b1; ++b) m = juce::jmax (m, fftData[(size_t) b]);
        // magnitude -> dBFS (Hann gain 0.5), plus a 3 dB/oct tilt so pink-ish music reads flat
        float db = juce::Decibels::gainToDecibels (m / (fftSize * 0.25f), -120.0f) + 3.0f * (float) std::log2 (f0 / 1000.0);
        auto& s = spectrum[(size_t) x];
        s = db > s ? db : s - 1.2f;
    }
    repaint();
}

void CurveView::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (Pal::edge);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.2f);

    const auto pl = plot();
    g.setFont (sans (9.5f));
    for (double f : { 30.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xFor (f);
        g.setColour (Pal::edge.withAlpha (0.4f));
        g.drawVerticalLine (juce::roundToInt (x), pl.getY(), pl.getBottom());
        g.setColour (Pal::inkDim);
        g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 20, (int) pl.getBottom() + 2, 40, 11, juce::Justification::centred);
    }
    for (double db : { -18.0, -12.0, -6.0, 0.0, 6.0, 12.0, 18.0 })
    {
        const float y = yFor (db);
        g.setColour (db == 0.0 ? Pal::inkDim.withAlpha (0.55f) : Pal::edge.withAlpha (0.4f));
        g.drawHorizontalLine (juce::roundToInt (y), pl.getX(), pl.getRight());
        g.setColour (Pal::inkDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) area.getX() + 2, (int) y - 6, 30, 12, juce::Justification::centredRight);
    }

    // analyser: -90..0 dBFS over the plot height
    if (! spectrum.empty())
    {
        juce::Path sp;
        sp.startNewSubPath (pl.getX(), pl.getBottom());
        for (size_t x = 0; x < spectrum.size(); ++x)
        {
            const float t = juce::jlimit (0.0f, 1.0f, (spectrum[x] + 90.0f) / 90.0f);
            sp.lineTo (pl.getX() + (float) x, pl.getBottom() - t * pl.getHeight());
        }
        sp.lineTo (pl.getRight(), pl.getBottom());
        sp.closeSubPath();
        g.setColour (Pal::inkDim.withAlpha (0.13f));
        g.fillPath (sp);
    }

    const auto s = proc.readSettings();
    const double rate = juce::jmax (44100.0, proc.getSampleRate()) * 4.0;
    const auto coefs = VelourPProcessor::design (s, rate);
    const int w = (int) pl.getWidth();

    // individual bands, faint
    for (int b = 0; b < VelourPProcessor::numBands; ++b)
    {
        const auto& bd = s.bands[(size_t) b];
        if (! bd.on || std::abs (bd.gain) < 0.01) continue;
        juce::Path p;
        for (int x = 0; x <= w; x += 2)
        {
            const double f = fAt (pl.getX() + (float) x);
            const float y = yFor (analogeq::magnitudeDb ({ coefs[(size_t) (2 + b)] }, f, rate));
            if (x == 0) p.startNewSubPath (pl.getX(), y); else p.lineTo (pl.getX() + (float) x, y);
        }
        g.setColour (Pal::band (b).withAlpha (0.45f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    // total
    juce::Path total, fill;
    for (int x = 0; x <= w; ++x)
    {
        const double f = fAt (pl.getX() + (float) x);
        std::vector<analogeq::Coefs> all (coefs.begin(), coefs.end());
        const float y = yFor (analogeq::magnitudeDb (all, f, rate));
        if (x == 0) { total.startNewSubPath (pl.getX(), y); fill.startNewSubPath (pl.getX(), yFor (0.0)); }
        else          total.lineTo (pl.getX() + (float) x, y);
        fill.lineTo (pl.getX() + (float) x, y);
    }
    fill.lineTo (pl.getRight(), yFor (0.0));
    fill.closeSubPath();
    g.setColour (Pal::wine.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (Pal::wine);
    g.strokePath (total, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // nodes
    auto drawNode = [&] (int n, juce::Colour c, const juce::String& label, bool active)
    {
        const auto p = nodePos (n);
        const bool hot = n == hover || n == dragging;
        const float r = hot ? 9.0f : 7.0f;
        if (hot) { g.setColour (c.withAlpha (0.25f)); g.fillEllipse (p.x - 14.0f, p.y - 14.0f, 28.0f, 28.0f); }
        g.setColour (active ? c : Pal::screen);
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour (c);
        g.drawEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f, 1.5f);
        g.setColour (active ? Pal::plate : c);
        g.setFont (sans (10.0f, true));
        g.drawText (label, juce::Rectangle<float> (p.x - r, p.y - r, r * 2.0f, r * 2.0f), juce::Justification::centred);
    };
    drawNode (hpfNode, Pal::filter, "H", s.hpf > 0);
    drawNode (lpfNode, Pal::filter, "L", s.lpf > 0);
    for (int b = 0; b < VelourPProcessor::numBands; ++b)
        drawNode (b, Pal::band (b), juce::String (b + 1), s.bands[(size_t) b].on);
}

//==============================================================================
VelourPEditor::VelourPEditor (VelourPProcessor& p)
    : AudioProcessorEditor (p), proc (p), curve (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (curve);
    curve.onSelect = [this] (int n) { selected = n; repaint(); };

    for (int b = 0; b < VelourPProcessor::numBands; ++b)
    {
        auto& s = strips[(size_t) b];
        const auto col = Pal::band (b);
        s.on.setButtonText (juce::String (b + 1));
        s.on.getProperties().set ("accent", col.toString());
        s.on.setTooltip ("Band on / off");
        addAndMakeVisible (s.on);
        s.onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, VelourPProcessor::bandId (b, "On"), s.on);
        setupCombo (s.type, VelourPProcessor::bandId (b, "Type"), VelourPProcessor::typeChoices, s.typeAtt);
        setupKnob (s.gain, VelourPProcessor::bandId (b, "Gain"), "GAIN", col);
        setupKnob (s.freq, VelourPProcessor::bandId (b, "Freq"), "FREQ", col);
        setupKnob (s.q,    VelourPProcessor::bandId (b, "Q"),    "Q",    col, "Bandwidth. With Console Q on, the value applies at 6 dB; smaller moves get wider, bigger ones narrower.");
        s.gain.slider.getProperties().set ("centred", true);
    }

    setupKnob (hpf, "hpf", "HIGH-PASS", Pal::filter);
    setupKnob (lpf, "lpf", "LOW-PASS", Pal::filter);
    lpf.slider.getProperties().set ("accent", Pal::filter.toString());
    setupCombo (hpfSlope, "hpfSlope", VelourPProcessor::slopeChoices, hpfSlopeAtt);
    setupCombo (lpfSlope, "lpfSlope", VelourPProcessor::slopeChoices, lpfSlopeAtt);

    setupKnob (iron,   "iron",   "IRON",   Pal::wine, "Transformer: low end thickens and saturates first, top end rounds off.");
    setupKnob (drive,  "drive",  "DRIVE",  Pal::wine, "Tube stage after the EQ: even harmonics, soft compression. Boosts drive it harder.");
    setupKnob (output, "output", "OUTPUT", Pal::wine);
    output.slider.getProperties().set ("centred", true);

    for (auto* t : { &analog, &consoleQ })
        addAndMakeVisible (*t);
    analog.setTooltip ("Analog path (transformer + tube). Off = clean EQ.");
    consoleQ.setTooltip ("Proportional Q on the bell bands, like a console: gentle moves are wide, big moves narrower.");
    analogAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "analog", analog);
    consoleQAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "consoleQ", consoleQ);

    setSize (1120, 680);
    startTimerHz (30);
}

VelourPEditor::~VelourPEditor()
{
    setLookAndFeel (nullptr);
}

void VelourPEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name, juce::Colour accent, const juce::String& tip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    k.slider.getProperties().set ("accent", accent.toString());
    if (tip.isNotEmpty()) k.slider.setTooltip (tip);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (sans (10.0f, true));
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void VelourPEditor::setupCombo (juce::ComboBox& box, const juce::String& id, const juce::StringArray& items,
                                std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
{
    box.addItemList (items, 1);
    box.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, box);
}

void VelourPEditor::timerCallback()
{
    curve.updateAnalyser();
    const bool analogOn = analog.getToggleState();
    glow = analogOn ? juce::jmax (proc.driveActivity.load(), glow * 0.9f) : glow * 0.8f;
    repaint (lamp.expanded (12));
}

void VelourPEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (Pal::plate.brighter (0.04f), 0.0f, 0.0f, Pal::plate.darker (0.05f), 0.0f, (float) getHeight(), false));
    g.fillAll();
    g.setColour (Pal::edge);
    g.drawRect (getLocalBounds(), 2);

    g.setColour (Pal::wine);
    g.setFont (serif (30.0f, true));
    g.drawText ("Velour", header.withWidth (110), juce::Justification::centredLeft);
    g.setColour (Pal::ink);
    g.setFont (serif (30.0f));
    g.drawText ("P", header.withTrimmedLeft (102).withWidth (30), juce::Justification::centredLeft);
    g.setColour (Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText ("PARAMETRIC COLOURED EQUALISER", header.withTrimmedLeft (134).withWidth (260).translated (0, 4), juce::Justification::centredLeft);

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

    auto panel = [&] (juce::Rectangle<int> r, const juce::String& title, juce::Colour c, bool sel)
    {
        g.setColour (Pal::plateDk);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (sel ? c : Pal::edge);
        g.drawRoundedRectangle (r.toFloat(), 6.0f, sel ? 2.0f : 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (c);
            g.setFont (serif (12.5f, true));
            g.drawText (title, r.withHeight (22), juce::Justification::centred);
        }
    };
    for (int b = 0; b < VelourPProcessor::numBands; ++b)
        panel (strips[(size_t) b].area, {}, Pal::band (b), selected == b);
    panel (filterArea, "Filters", Pal::filter, selected == -1 || selected == -2);
    panel (charArea, "Character", Pal::wine, false);
}

void VelourPEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 0);
    header = b.removeFromTop (54).withTrimmedTop (6);
    {
        auto h = header;
        analog.setBounds (h.removeFromRight (92).withSizeKeepingCentre (92, 28));
        h.removeFromRight (8);
        consoleQ.setBounds (h.removeFromRight (100).withSizeKeepingCentre (100, 28));
        h.removeFromRight (16);
        lamp = h.removeFromRight (22).withSizeKeepingCentre (22, 22);
    }

    curve.setBounds (b.removeFromTop (300));
    b.removeFromTop (10);
    auto row = b.removeFromTop (b.getHeight() - 14);

    constexpr int gap = 8;
    const int stripW = 112;
    auto placeKnob = [] (Knob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (13));
        k.slider.setBounds (r);
    };

    for (auto& s : strips)
    {
        s.area = row.removeFromLeft (stripW);
        row.removeFromLeft (gap);
        auto in = s.area.reduced (8, 8);
        auto top = in.removeFromTop (24);
        s.on.setBounds (top.removeFromLeft (26));
        top.removeFromLeft (4);
        s.type.setBounds (top);
        in.removeFromTop (4);
        const int kh = in.getHeight() / 3;
        placeKnob (s.gain, in.removeFromTop (kh));
        placeKnob (s.freq, in.removeFromTop (kh));
        placeKnob (s.q, in);
    }

    filterArea = row.removeFromLeft (124);
    row.removeFromLeft (gap);
    {
        auto in = filterArea.reduced (8, 8).withTrimmedTop (18);
        const int half = in.getHeight() / 2;
        auto a = in.removeFromTop (half), c = in;
        hpfSlope.setBounds (a.removeFromBottom (22).reduced (14, 0));
        placeKnob (hpf, a);
        lpfSlope.setBounds (c.removeFromBottom (22).reduced (14, 0));
        placeKnob (lpf, c);
    }

    charArea = row;
    {
        auto in = charArea.reduced (8, 8).withTrimmedTop (18);
        const int kh = in.getHeight() / 3;
        placeKnob (iron, in.removeFromTop (kh));
        placeKnob (drive, in.removeFromTop (kh));
        placeKnob (output, in);
    }
}
