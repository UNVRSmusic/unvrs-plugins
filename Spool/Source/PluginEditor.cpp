#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour bg       { 0xff17130f };
    const juce::Colour panel    { 0xff221c17 };
    const juce::Colour panelHi  { 0xff2e2620 };
    const juce::Colour line     { 0xff3d332b };
    const juce::Colour text     { 0xfff0e6d4 };
    const juce::Colour textDim  { 0xff9a8b78 };
    const juce::Colour rec      { 0xffff5a36 };
    const juce::Colour play     { 0xff5fd3a0 };
    const juce::Colour dub      { 0xffffb238 };
    const juce::Colour mod      { 0xff8fb8ff };
}

static juce::Font sans (float size, bool bold = false)
{
    return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
}

static juce::Colour stateColour (LooperEngine::State s)
{
    switch (s)
    {
        case LooperEngine::State::recording:   return Pal::rec;
        case LooperEngine::State::overdubbing: return Pal::dub;
        case LooperEngine::State::playing:     return Pal::play;
        case LooperEngine::State::stopped:
        case LooperEngine::State::empty:       break;
    }
    return Pal::textDim;
}

//==============================================================================
SpoolLookAndFeel::SpoolLookAndFeel()
{
    setColour (juce::Label::textColourId, Pal::textDim);
    setColour (juce::Slider::textBoxTextColourId, Pal::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, Pal::text);
    setColour (juce::ComboBox::backgroundColourId, Pal::panelHi);
    setColour (juce::ComboBox::outlineColourId, Pal::line);
    setColour (juce::PopupMenu::backgroundColourId, Pal::panelHi);
    setColour (juce::PopupMenu::textColourId, Pal::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Pal::dub);
    setColour (juce::PopupMenu::highlightedTextColourId, Pal::bg);
    setColour (juce::TextButton::textColourOffId, Pal::text);
    setColour (juce::TextButton::textColourOnId, Pal::text);
    setColour (juce::TooltipWindow::backgroundColourId, Pal::panelHi);
    setColour (juce::TooltipWindow::textColourId, Pal::text);
    setColour (juce::TooltipWindow::outlineColourId, Pal::line);
}

void SpoolLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float arcR = r - 3.0f;
    const bool centred = (bool) s.getProperties().getWithDefault ("centred", false);
    const auto accent = juce::Colour::fromString (s.getProperties().getWithDefault ("accent", Pal::dub.toString()).toString());

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::line);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = centred ? (startAngle + endAngle) * 0.5f : startAngle;
    const float to = startAngle + pos * (endAngle - startAngle);
    if (std::abs (to - from) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // knob body
    const float kr = arcR - 7.0f;
    g.setGradientFill (juce::ColourGradient (Pal::panelHi.brighter (0.18f), c.x, c.y - kr, Pal::panel.darker (0.4f), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    const float angle = startAngle + pos * (endAngle - startAngle);
    g.setColour (Pal::text);
    g.drawLine ({ c.getPointOnCircumference (kr * 0.3f, angle), c.getPointOnCircumference (kr * 0.85f, angle) }, 2.2f);

    // ramp modulation marker
    const auto mod = s.getProperties()["mod"];
    if (! mod.isVoid())
    {
        const float ma = startAngle + (float) mod * (endAngle - startAngle);
        const auto p = c.getPointOnCircumference (arcR, ma);
        g.setColour (Pal::mod);
        g.fillEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f);
    }
}

void SpoolLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = juce::Colour::fromString (b.getProperties().getWithDefault ("accent", Pal::dub.toString()).toString());

    g.setColour (on ? accent.withAlpha (0.16f) : Pal::panelHi);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (on ? accent : (highlighted ? Pal::textDim : Pal::line));
    g.drawRoundedRectangle (area, 5.0f, 1.0f);

    auto led = area.removeFromLeft (juce::jmin (area.getHeight(), 24.0f)).reduced (juce::jmin (area.getHeight(), 24.0f) * 0.34f);
    g.setColour (on ? accent : Pal::line.brighter (0.25f));
    g.fillEllipse (led);

    g.setColour (on ? Pal::text : Pal::textDim);
    g.setFont (sans (11.0f, true));
    g.drawText (b.getButtonText(), area.withTrimmedRight (6.0f), juce::Justification::centred);
}

void SpoolLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto lit = b.getProperties()["lit"];
    const auto litColour = lit.isVoid() ? juce::Colours::transparentBlack : juce::Colour::fromString (lit.toString());

    g.setColour (down ? Pal::line : Pal::panelHi);
    g.fillRoundedRectangle (area, 7.0f);
    if (! lit.isVoid())
    {
        g.setColour (litColour.withAlpha (0.22f));
        g.fillRoundedRectangle (area, 7.0f);
    }
    g.setColour (! lit.isVoid() ? litColour : (highlighted ? Pal::textDim : Pal::line));
    g.drawRoundedRectangle (area, 7.0f, ! lit.isVoid() ? 1.6f : 1.0f);
}

void SpoolLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.isEnabled() ? Pal::text : Pal::textDim.withAlpha (0.5f));
    g.setFont (sans (b.getHeight() > 40 ? 14.0f : 11.0f, true));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void SpoolLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Pal::panelHi);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (box.isMouseOver (true) ? Pal::textDim : Pal::line);
    g.drawRoundedRectangle (area, 5.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Pal::textDim);
    g.fillPath (arrow);
}

//==============================================================================
SpoolEditor::SpoolEditor (SpoolProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (volume,    "volume",    "VOLUME");
    setupKnob (repeats,   "repeats",   "REPEATS");
    setupKnob (stability, "stability", "STABILITY");
    setupKnob (modA,      "modA",      "MOD A");
    setupKnob (modB,      "modB",      "MOD B");
    setupKnob (rampRate,  "rampRate",  "RATE");
    volume.slider.getProperties().set ("centred", true);
    modA.slider.getProperties().set ("centred", true);
    modB.slider.getProperties().set ("centred", true);
    rampRate.slider.getProperties().set ("accent", Pal::mod.toString());
    rampRate.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    volume.slider.setTooltip ("Loop level. Unity at noon, about +6 dB fully right.");
    repeats.slider.setTooltip ("How much of the loop survives each pass while recording. Full = classic looper, lower = layers fade away like a delay.");
    stability.slider.setTooltip ("Tape character: wow, flutter, darkening, saturation, hiss and dropouts. In Additive mode it wears the loop out pass by pass.");
    modA.slider.setTooltip ("Smooth Speed: left slows to a stop, right up to 2x.\nDropper: left random drops, right repeating pattern.\nTrimmer: left pulls the end in, right pushes the start forward.");
    modB.slider.setTooltip ("Stepped Speed: octaves and fifths, down to 1/4 and up to 4x.\nScrambler: left random, right repeating sequence.\nFilter: left low-pass, right high-pass.");

    setupChoice (mode,     "mode",     SpoolProcessor::modeChoices);
    setupChoice (sync,     "sync",     SpoolProcessor::syncChoices);
    setupChoice (modASel,  "modASel",  SpoolProcessor::modAChoices);
    setupChoice (modBSel,  "modBSel",  SpoolProcessor::modBChoices);
    setupChoice (rampMode, "rampMode", SpoolProcessor::rampModeChoices);
    setupChoice (rampDir,  "rampDir",  SpoolProcessor::rampDirChoices);
    mode.box.setTooltip ("Normal: modifiers only colour playback.\nAdditive: overdubs print what you hear into the loop.\nSampler: each recording replaces the loop; Trigger restarts it.");
    sync.box.setTooltip ("Quantise record / one-shot / trigger to the host's beats or bars (while the host is playing).");

    setupToggle (modAOn,     "modAOn",      "ON");
    setupToggle (modBOn,     "modBOn",      "ON");
    setupToggle (reverse,    "reverse",     "REVERSE");
    setupToggle (dryKill,    "dryKill",     "DRY KILL");
    setupToggle (dubAfter,   "dubAfterRec", "DUB AFTER REC");
    setupToggle (sampleLoop, "sampleLoop",  "SAMPLER LOOP");
    setupToggle (rampSync,   "rampSync",    "SYNC TO LOOP");
    setupToggle (rampVol,    "rampVolume",    "VOL");
    setupToggle (rampRep,    "rampRepeats",   "REP");
    setupToggle (rampA,      "rampModA",      "A");
    setupToggle (rampStab,   "rampStability", "STAB");
    setupToggle (rampB,      "rampModB",      "B");
    for (auto* t : { &rampSync, &rampVol, &rampRep, &rampA, &rampStab, &rampB })
        t->button.getProperties().set ("accent", Pal::mod.toString());

    setupTransport (recButton,     "Record / overdub / play. Sampler: tap to record and tap to stop, or hold to record only while held. MIDI note C1.");
    setupTransport (stopButton,    "Stop (MIDI C#1)");
    setupTransport (oneShotButton, "Overdub exactly one pass, then stop recording. Handy for printing modifiers in Additive mode (MIDI D1).");
    setupTransport (undoButton,    "Step back one layer (MIDI D#1)");
    setupTransport (redoButton,    "Step forward one layer (MIDI E1)");
    setupTransport (clearButton,   "Delete the loop (MIDI F1)");
    setupTransport (triggerButton, "Restart the loop from the top (MIDI F#1)");
    setupTransport (retrigButton,  "Restart the ramp");
    setupTransport (exportButton,  "Save the current layer as a WAV file to Music/Spool");

    recButton.onPress     = [this] { recPressTime = juce::Time::getMillisecondCounterHiRes(); proc.sendCommand (SpoolProcessor::Command::recPlay); };
    recButton.onRelease   = [this]
    {
        const double held = juce::Time::getMillisecondCounterHiRes() - recPressTime;
        if ((int) proc.apvts.getRawParameterValue ("mode")->load() == 2 && held > 350.0
            && proc.engine.getInfo().state == LooperEngine::State::recording)
            proc.sendCommand (SpoolProcessor::Command::recPlay);
    };
    stopButton.onPress    = [this] { proc.sendCommand (SpoolProcessor::Command::stop); };
    oneShotButton.onPress = [this] { proc.sendCommand (SpoolProcessor::Command::oneShot); };
    undoButton.onPress    = [this] { proc.sendCommand (SpoolProcessor::Command::undo); };
    redoButton.onPress    = [this] { proc.sendCommand (SpoolProcessor::Command::redo); };
    clearButton.onPress   = [this] { proc.sendCommand (SpoolProcessor::Command::clear); };
    triggerButton.onPress = [this] { proc.sendCommand (SpoolProcessor::Command::trigger); };
    retrigButton.onPress  = [this] { proc.retriggerRamp(); };
    exportButton.onClick  = [this]
    {
        juce::File f;
        if (proc.exportLoop (f))
        {
            statusLabel.setText ("Saved " + f.getFileName(), juce::dontSendNotification);
            f.revealToUser();
        }
        else
            statusLabel.setText ("Nothing to export yet", juce::dontSendNotification);
    };

    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setFont (sans (11.0f));
    addAndMakeVisible (statusLabel);

    setSize (1000, 640);
    startTimerHz (30);
}

SpoolEditor::~SpoolEditor()
{
    setLookAndFeel (nullptr);
}

void SpoolEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (sans (11.0f, true));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void SpoolEditor::setupChoice (Choice& ch, const juce::String& paramId, const juce::StringArray& items)
{
    ch.box.addItemList (items, 1);
    ch.box.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (ch.box);
    ch.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, ch.box);
}

void SpoolEditor::setupToggle (Toggle& t, const juce::String& paramId, const juce::String& text)
{
    t.button.setButtonText (text);
    addAndMakeVisible (t.button);
    t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, t.button);
}

void SpoolEditor::setupTransport (PressButton& b, const juce::String& tooltip)
{
    b.setTooltip (tooltip);
    addAndMakeVisible (b);
}

//==============================================================================
void SpoolEditor::timerCallback()
{
    info = proc.engine.getInfo();
    pulse += 0.12f;

    if (--overviewCountdown <= 0)
    {
        proc.engine.getOverview (overview, 240);
        overviewCountdown = info.state == LooperEngine::State::recording
                         || info.state == LooperEngine::State::overdubbing ? 3 : 10;
    }

    // ramp markers on the knobs
    Knob* targets[] { &volume, &repeats, &modA, &stability, &modB };
    Toggle* toggles[] { &rampVol, &rampRep, &rampA, &rampStab, &rampB };
    const bool rampOn = (int) proc.apvts.getRawParameterValue ("rampMode")->load() > 0;
    for (int i = 0; i < 5; ++i)
    {
        auto& props = targets[i]->slider.getProperties();
        if (rampOn && toggles[i]->button.getToggleState())
            props.set ("mod", proc.modulated[(size_t) i].load());
        else
            props.remove ("mod");
        targets[i]->slider.repaint();
    }

    // transport lights
    auto light = [] (juce::Button& b, std::optional<juce::Colour> c)
    {
        const auto old = b.getProperties()["lit"];
        const juce::var now = c ? juce::var (c->toString()) : juce::var();
        if (old != now)
        {
            if (c) b.getProperties().set ("lit", now);
            else   b.getProperties().remove ("lit");
            b.repaint();
        }
    };

    const bool waiting = proc.syncWaiting.load();
    const bool blink = std::fmod (pulse, 1.0f) < 0.5f;
    std::optional<juce::Colour> recLit;
    if (info.state == LooperEngine::State::recording)        recLit = Pal::rec;
    else if (info.state == LooperEngine::State::overdubbing) recLit = Pal::dub;
    else if (info.state == LooperEngine::State::playing)     recLit = Pal::play;
    if (waiting && blink) recLit = Pal::text;
    light (recButton, recLit);
    light (stopButton, info.state == LooperEngine::State::stopped ? std::optional<juce::Colour> (Pal::textDim) : std::nullopt);

    undoButton.setEnabled (info.cur > 0);
    redoButton.setEnabled (info.cur < info.count - 1);

    const bool sampler = (int) proc.apvts.getRawParameterValue ("mode")->load() == 2;
    oneShotButton.setEnabled (! sampler);
    sampleLoop.button.setEnabled (sampler);

    repaint (reelArea);
}

//==============================================================================
void SpoolEditor::paintSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (Pal::textDim);
    g.setFont (sans (10.0f, true));
    g.drawText (title, r.reduced (14, 8).removeFromTop (14), juce::Justification::centredLeft);
}

void SpoolEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg);

    // header
    g.setColour (Pal::text);
    g.setFont (sans (26.0f, true));
    g.drawText ("SPOOL", headerArea.removeFromLeft (100), juce::Justification::centredLeft);
    g.setColour (Pal::dub);
    g.setFont (sans (12.0f, true));
    g.drawText ("LOOPER", 112, headerArea.getY() + 3, 80, headerArea.getHeight(), juce::Justification::centredLeft);

    g.setColour (Pal::textDim);
    g.setFont (sans (10.0f, true));
    g.drawText ("MODE", mode.box.getBounds().translated (0, -14).withHeight (14), juce::Justification::centredLeft);
    g.drawText ("SYNC", sync.box.getBounds().translated (0, -14).withHeight (14), juce::Justification::centredLeft);

    paintSection (g, loopSection, "LOOP");
    paintSection (g, modSection,  "MODIFIERS");
    paintSection (g, rampSection, "RAMP");

    paintReel (g);
}

void SpoolEditor::paintReel (juce::Graphics& g)
{
    const auto area = reelArea.toFloat();
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (area, 12.0f);

    const auto c = area.getCentre();
    const float outer = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 22.0f;
    const float inner = outer * 0.52f;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const auto st = info.state;
    const auto col = stateColour (st);

    // reel body
    g.setColour (Pal::bg);
    g.fillEllipse (c.x - outer - 8.0f, c.y - outer - 8.0f, (outer + 8.0f) * 2.0f, (outer + 8.0f) * 2.0f);
    g.setColour (Pal::line);
    g.drawEllipse (c.x - inner, c.y - inner, inner * 2.0f, inner * 2.0f, 1.0f);

    const bool hasLoop = info.len > 0 && st != LooperEngine::State::empty;
    const bool trimmed = hasLoop && ! info.locked && (info.regionStart > 0.5 || info.regionEnd < info.len - 0.5);

    // waveform ring
    if (hasLoop && ! overview.empty())
    {
        const int bins = (int) overview.size();
        float maxPk = 0.0001f;
        for (auto v : overview) maxPk = juce::jmax (maxPk, v);
        const float norm = 1.0f / juce::jmax (0.25f, maxPk);

        for (int i = 0; i < bins; ++i)
        {
            const float a = twoPi * (float) i / (float) bins;
            const float len = (outer - inner - 6.0f) * juce::jlimit (0.02f, 1.0f, overview[(size_t) i] * norm);
            const double binPos = (double) i / bins * info.len;
            const bool outside = trimmed && (binPos < info.regionStart || binPos > info.regionEnd);
            g.setColour ((st == LooperEngine::State::recording ? Pal::rec : Pal::text).withAlpha (outside ? 0.15f : 0.75f));
            g.drawLine ({ c.getPointOnCircumference (inner + 3.0f, a), c.getPointOnCircumference (inner + 3.0f + len, a) }, 1.6f);
        }
    }

    // progress / playheads
    if (hasLoop && st != LooperEngine::State::recording)
    {
        const float ab = twoPi * (float) info.pos / (float) juce::jmax (1, info.len);
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, outer + 4.0f, outer + 4.0f, 0.0f, 0.0f, ab, true);
        g.setColour (col.withAlpha (0.8f));
        g.strokePath (arc, juce::PathStrokeType (3.0f));
        g.drawLine ({ c.getPointOnCircumference (inner, ab), c.getPointOnCircumference (outer + 6.0f, ab) }, 2.0f);

        if (! info.locked)
        {
            const float ar = twoPi * (float) (info.readPos / juce::jmax (1, info.len));
            const auto p = c.getPointOnCircumference (outer + 4.0f, ar);
            g.setColour (Pal::mod);
            g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        }
    }
    else if (st == LooperEngine::State::recording)
    {
        const float a = std::fmod (pulse * 0.6f, twoPi);
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, outer + 4.0f, outer + 4.0f, 0.0f, a, a + 1.2f, true);
        g.setColour (Pal::rec);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // centre read-out
    juce::String title;
    switch (st)
    {
        case LooperEngine::State::empty:       title = "EMPTY"; break;
        case LooperEngine::State::recording:   title = "REC"; break;
        case LooperEngine::State::playing:     title = "PLAY"; break;
        case LooperEngine::State::overdubbing: title = "DUB"; break;
        case LooperEngine::State::stopped:     title = "STOP"; break;
    }
    if (proc.syncWaiting.load())
        title = "WAIT";

    const double fs = juce::jmax (1.0, proc.engine.getSampleRate());
    auto centre = juce::Rectangle<float> (inner * 1.6f, inner * 1.2f).withCentre (c);
    g.setColour (col);
    g.setFont (sans (30.0f, true));
    g.drawText (title, centre.removeFromTop (centre.getHeight() * 0.48f), juce::Justification::centredBottom);

    g.setColour (Pal::text);
    g.setFont (sans (15.0f));
    g.drawText (info.len > 0 ? juce::String (info.len / fs, 2) + " s" : juce::String ("--"),
                centre.removeFromTop (22.0f), juce::Justification::centred);

    g.setColour (Pal::textDim);
    g.setFont (sans (11.0f, true));
    g.drawText (info.count > 0 ? "LAYER " + juce::String (info.cur + 1) + " / " + juce::String (info.count) : juce::String(),
                centre.removeFromTop (18.0f), juce::Justification::centred);

    // layer pips
    if (info.count > 0)
    {
        const int n = info.slots;
        const float pw = 10.0f, gap = 5.0f;
        float x = c.x - (n * pw + (n - 1) * gap) * 0.5f;
        const float y = area.getBottom() - 22.0f;
        for (int i = 0; i < n; ++i, x += pw + gap)
        {
            g.setColour (i == info.cur ? col : (i < info.count ? Pal::text.withAlpha (0.5f) : Pal::line));
            g.fillRoundedRectangle (x, y, pw, 6.0f, 2.0f);
        }
    }
}

//==============================================================================
void SpoolEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 0);
    headerArea = b.removeFromTop (58).withTrimmedTop (10);

    {
        auto h = headerArea;
        exportButton.setBounds (h.removeFromRight (110).withSizeKeepingCentre (110, 28).translated (0, 6));
        h.removeFromRight (10);
        statusLabel.setBounds (h.removeFromRight (220).withTrimmedTop (16));
        h.removeFromRight (10);
        sync.box.setBounds (h.removeFromRight (90).withTrimmedTop (16).withHeight (26));
        h.removeFromRight (10);
        mode.box.setBounds (h.removeFromRight (120).withTrimmedTop (16).withHeight (26));
    }

    transportArea = b.removeFromBottom (82).withTrimmedBottom (14);
    b.removeFromBottom (10);

    reelArea = b.removeFromLeft (440);
    b.removeFromLeft (12);

    loopSection = b.removeFromTop (178);
    b.removeFromTop (10);
    modSection = b.removeFromTop (172);
    b.removeFromTop (10);
    rampSection = b;

    auto placeKnob = [] (Knob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (14));
        k.slider.setBounds (r);
    };

    // LOOP: three knobs + option toggles
    {
        auto r = loopSection.reduced (14, 10).withTrimmedTop (18);
        auto toggles = r.removeFromRight (150);
        for (auto* k : { &volume, &repeats, &stability })
        {
            placeKnob (*k, r.removeFromLeft (100));
            r.removeFromLeft (4);
        }
        const int th = 26, gap = (toggles.getHeight() - 4 * th) / 3;
        for (auto* t : { &reverse, &dryKill, &dubAfter, &sampleLoop })
        {
            t->button.setBounds (toggles.removeFromTop (th));
            toggles.removeFromTop (gap);
        }
    }

    // MODIFIERS: two slots side by side
    {
        auto r = modSection.reduced (14, 10).withTrimmedTop (18);
        auto left = r.removeFromLeft (r.getWidth() / 2).withTrimmedRight (8);
        auto right = r.withTrimmedLeft (8);

        auto slot = [&] (juce::Rectangle<int> s, Knob& k, Choice& sel, Toggle& on)
        {
            auto controls = s.removeFromRight (s.getWidth() - 110);
            placeKnob (k, s);
            controls.removeFromTop (16);
            sel.box.setBounds (controls.removeFromTop (28));
            controls.removeFromTop (10);
            on.button.setBounds (controls.removeFromTop (28));
        };
        slot (left, modA, modASel, modAOn);
        slot (right, modB, modBSel, modBOn);
    }

    // RAMP
    {
        auto r = rampSection.reduced (14, 10).withTrimmedTop (18);
        placeKnob (rampRate, r.removeFromLeft (84).withTrimmedBottom (4));
        r.removeFromLeft (10);

        auto row1 = r.removeFromTop (28);
        rampMode.box.setBounds (row1.removeFromLeft (110));
        row1.removeFromLeft (8);
        rampDir.box.setBounds (row1.removeFromLeft (80));
        row1.removeFromLeft (8);
        rampSync.button.setBounds (row1.removeFromLeft (130));
        row1.removeFromLeft (8);
        retrigButton.setBounds (row1);

        r.removeFromTop (12);
        auto row2 = r.removeFromTop (28);
        const int w = (row2.getWidth() - 4 * 6) / 5;
        for (auto* t : { &rampVol, &rampRep, &rampA, &rampStab, &rampB })
        {
            t->button.setBounds (row2.removeFromLeft (w));
            row2.removeFromLeft (6);
        }
    }

    // transport
    {
        auto r = transportArea;
        recButton.setBounds (r.removeFromLeft (200));
        r.removeFromLeft (10);
        const int w = (r.getWidth() - 5 * 10) / 6;
        for (auto* btn : { &stopButton, &oneShotButton, &undoButton, &redoButton, &triggerButton, &clearButton })
        {
            btn->setBounds (r.removeFromLeft (w));
            r.removeFromLeft (10);
        }
    }
}
