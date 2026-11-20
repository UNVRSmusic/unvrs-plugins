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
}

static juce::Font sans (float size, bool bold = false) { return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain); }

//==============================================================================
IctusLookAndFeel::IctusLookAndFeel()
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

void IctusLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool centred = (bool) s.getProperties().getWithDefault ("centred", false);
    const auto accent = (bool) s.getProperties().getWithDefault ("coral", false) ? Pal::coral : Pal::teal;
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = centred ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path v;
        v.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (v, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = r - 7.0f;
    g.setColour (Pal::knob);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);
    g.setColour (Pal::ink.withAlpha (alpha));
    g.drawLine ({ c.getPointOnCircumference (kr * 0.3f, angle), c.getPointOnCircumference (kr * 0.9f, angle) }, 2.0f);
}

void IctusLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = (bool) b.getProperties().getWithDefault ("coral", false) ? Pal::coral : Pal::teal;
    g.setColour (on ? accent : Pal::panel);
    g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
    g.setColour (highlighted ? Pal::ink : (on ? accent : Pal::line));
    g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.0f);
    g.setColour (on ? Pal::bg : Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText (b.getButtonText(), area, juce::Justification::centred);
}

void IctusLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
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

juce::Font IctusLookAndFeel::getComboBoxFont (juce::ComboBox&) { return sans (12.0f, true); }

void IctusLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
IctusEditor::IctusEditor (IctusProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    // presets
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        preset.addItem (proc.getProgramName (i), i + 1);
    preset.setTooltip ("Factory presets. Start here, then shape the body with the resonator controls.");
    preset.onChange = [this]
    {
        const int idx = preset.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram())
            proc.setCurrentProgram (idx);
        lastProgram = proc.getCurrentProgram();
    };
    addAndMakeVisible (preset);

    // mallet
    setupToggle (malletOn, malletOnAtt, "malletOn", "The mallet: a short impulse, like a stick or hammer hitting the body.");
    setupKnob (malletVol,      "malletVol",      "VOLUME",    "Mallet strength.");
    setupKnob (malletStiff,    "malletStiff",    "STIFFNESS", "Soft felt (left) to hard metal (right). A harder mallet has a shorter contact time and excites higher modes.");
    setupKnob (malletVelStiff, "malletVelStiff", "VEL > STIFF", "How much velocity makes the mallet harder. Real mallets brighten when you hit harder.");
    setupKnob (malletNoise,    "malletNoise",    "NOISE",     "Adds a burst of contact noise to the impact (scrape, pick, stick).");
    setupKnob (malletColor,    "malletColor",    "COLOR",     "Brightness of the impact noise.");

    // noise
    setupToggle (noiseOn, noiseOnAtt, "noiseOn", "Noise exciter: breath, bow or scrape. Keeps the body sounding while the note is held.");
    setupCombo (noiseType, noiseTypeAtt, "noiseType", IctusProcessor::noiseTypeChoices, "Filter applied to the noise.");
    setupKnob (noiseVol,  "noiseVol",  "VOLUME",  "Noise exciter level.");
    setupKnob (noiseFreq, "noiseFreq", "FREQ",    "Noise filter frequency.");
    setupKnob (noiseRes,  "noiseRes",  "RES",     "Noise filter resonance.");
    setupKnob (noiseKey,  "noiseKey",  "KEY",     "How much the filter follows the played note.");
    setupKnob (noiseA,    "noiseA",    "ATTACK",  "Noise envelope attack.");
    setupKnob (noiseD,    "noiseD",    "DECAY",   "Noise envelope decay.");
    setupKnob (noiseS,    "noiseS",    "SUSTAIN", "Noise envelope sustain. Above zero the body keeps sounding while the key is held.");
    setupKnob (noiseR,    "noiseR",    "RELEASE", "Noise envelope release.");

    // resonators
    for (int r = 0; r < 2; ++r)
    {
        auto& u = res[(size_t) r];
        auto id = [r] (const char* n) { return IctusProcessor::resId (r, n); };
        u.on.setButtonText ("RES " + juce::String (r + 1));
        if (r == 1) u.on.getProperties().set ("coral", true);
        setupToggle (u.on, u.onAtt, id ("On"), "Resonator on/off.");
        setupCombo (u.type, u.typeAtt, id ("Type"), IctusProcessor::typeChoices,
                    "The vibrating body.\nBeam: free bar (or clamped tine). Marimba: tuned bar with tube.\nString: stiff string. Membrane: drum head (Bessel modes).\n"
                    "Plate: rectangular plate. Pipe: air column, closed to open.\nBell: church-bell partials. Bowl: singing bowl / glass.");
        setupCombo (u.modes, u.modesAtt, id ("Modes"), IctusProcessor::modeCountChoices, "Number of simulated modes. More = richer highs and denser drums, more CPU.");
        setupKnob (u.tune,     id ("Tune"),     "TUNE",     "Coarse tuning in semitones.", true);
        setupKnob (u.fine,     id ("Fine"),     "FINE",     "Fine tuning in cents.", true);
        setupKnob (u.key,      id ("Key"),      "KEY",      "Keyboard tracking. 0 % = fixed pitch at C4 (drums), 100 % = chromatic.");
        setupKnob (u.decay,    id ("Decay"),    "DECAY",    "Ring time (T60) of the fundamental at C4. Higher notes ring shorter, as in real instruments.");
        setupKnob (u.damp,     id ("Damp"),     "RELEASE",  "How long the body keeps ringing after note-off.\n0 % = muted by the hand, 100 % = rings on freely.");
        setupKnob (u.material, id ("Material"), "MATERIAL", "How fast high modes die compared to low ones.\nWood/rubber (left): highs vanish quickly. Metal/glass (right): everything rings.");
        setupKnob (u.shape,    id ("Shape"),    "SHAPE",    "Body geometry, depends on the type (name shown above the knob).");
        setupKnob (u.inharm,   id ("Inharm"),   "INHARM",   "Stretches (right) or compresses (left) the overtone series.", true);
        setupKnob (u.bright,   id ("Bright"),   "BRIGHT",   "Tilts the level of the higher modes.", true);
        setupKnob (u.hit,      id ("Hit"),      "HIT",      "Where the body is struck. 0 % = centre, 100 % = towards the edge or end.\nModes with a node at that point stay silent.");
        setupKnob (u.listen,   id ("Listen"),   "LISTEN",   "Where the sound is picked up (0 % centre).");
        setupKnob (u.width,    id ("Width"),    "WIDTH",    "Distance between the left and right pickup points. Real stereo from two positions on the body.");
        setupKnob (u.gain,     id ("Gain"),     "GAIN",     "Output level of this resonator.", true);
        if (r == 1)
            for (auto* k : { &u.tune, &u.fine, &u.key, &u.decay, &u.damp, &u.material, &u.shape, &u.inharm, &u.bright, &u.hit, &u.listen, &u.width, &u.gain })
                k->slider.getProperties().set ("coral", true);
    }

    // global
    setupCombo (structure, structureAtt, "structure", IctusProcessor::structureChoices,
                "Parallel: both resonators are struck by the exciters.\nSerial: resonator 1 drives resonator 2 (string on a plate, bar on a tube).");
    setupKnob (voices,   "voices",   "VOICES",    "Polyphony. 1 = mono: a new note retunes the still ringing body.");
    setupKnob (velocity, "velocity", "VELOCITY",  "Velocity sensitivity of the level.");
    setupKnob (penvAmt,  "penvAmt",  "PITCH ENV", "Pitch drop (or rise) at the start of each note, e.g. for toms.", true);
    setupKnob (penvTime, "penvTime", "ENV TIME",  "Duration of the pitch envelope.");
    setupKnob (bleed,    "bleed",    "BLEED",     "Mixes the raw exciter into the output (click, breath).");
    setupKnob (volume,   "volume",   "VOLUME",    "Output level.");

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (48);
    keyboard.setKeyWidth (16.0f);
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

    setSize (1120, 652);
    timerCallback();
    startTimerHz (30);
}

IctusEditor::~IctusEditor()
{
    setLookAndFeel (nullptr);
}

void IctusEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name, const juce::String& tip, bool centred)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 15);
    k.slider.setTooltip (tip);
    if (centred) k.slider.getProperties().set ("centred", true);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (sans (10.0f, true));
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void IctusEditor::setupCombo (juce::ComboBox& c, ComboAtt& att, const juce::String& id, const juce::StringArray& items, const juce::String& tip)
{
    c.addItemList (items, 1);
    c.setTooltip (tip);
    addAndMakeVisible (c);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c);
}

void IctusEditor::setupToggle (juce::ToggleButton& b, ButtonAtt& att, const juce::String& id, const juce::String& tip)
{
    b.setTooltip (tip);
    addAndMakeVisible (b);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b);
}

void IctusEditor::timerCallback()
{
    const auto s = proc.readSettings();
    bool changed = false;
    for (int r = 0; r < 2; ++r)
    {
        if (! shownValid[(size_t) r] || ! (shownParams[(size_t) r] == s.res[r].rp))
        {
            ictus::buildLayout (s.res[r].rp, layouts[(size_t) r]);
            shownParams[(size_t) r] = s.res[r].rp;
            shownValid[(size_t) r] = true;
            changed = true;
        }
        res[(size_t) r].shape.label.setText (ictus::shapeName (s.res[r].rp.type), juce::dontSendNotification);
        const bool on = s.res[r].on;
        for (auto* k : { &res[(size_t) r].tune, &res[(size_t) r].fine, &res[(size_t) r].key, &res[(size_t) r].decay, &res[(size_t) r].damp,
                         &res[(size_t) r].material, &res[(size_t) r].shape, &res[(size_t) r].inharm, &res[(size_t) r].bright,
                         &res[(size_t) r].hit, &res[(size_t) r].listen, &res[(size_t) r].width, &res[(size_t) r].gain })
            k->slider.setAlpha (on ? 1.0f : 0.45f);
    }
    for (auto* k : { &malletVol, &malletStiff, &malletVelStiff, &malletNoise, &malletColor })
        k->slider.setAlpha (s.malletOn ? 1.0f : 0.45f);
    for (auto* k : { &noiseVol, &noiseFreq, &noiseRes, &noiseKey, &noiseA, &noiseD, &noiseS, &noiseR })
        k->slider.setAlpha (s.noiseOn ? 1.0f : 0.45f);

    if (proc.getCurrentProgram() != lastProgram)
    {
        lastProgram = proc.getCurrentProgram();
        preset.setSelectedId (lastProgram + 1, juce::dontSendNotification);
    }

    const int v = proc.voicesPlaying.load();
    if (changed || v != lastVoices)
    {
        lastVoices = v;
        repaint (modeArea);
    }
}

//==============================================================================
void IctusEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg);

    g.setColour (Pal::ink);
    g.setFont (sans (26.0f, true));
    g.drawText ("ICTUS", header.withWidth (100), juce::Justification::centredLeft);
    g.setColour (Pal::teal);
    g.setFont (sans (11.0f, true));
    g.drawText ("MODAL PHYSICAL SYNTH", header.withTrimmedLeft (96).withWidth (180).translated (0, 3), juce::Justification::centredLeft);
    g.setColour (Pal::inkDim);
    g.setFont (sans (10.0f, true));
    g.drawText ("UNVRS", header.withTrimmedLeft (262).withWidth (60).translated (0, 3), juce::Justification::centredLeft);

    for (auto p : { malletPanel, noisePanel, res1Row, res2Row, globalPanel })
    {
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (p.toFloat(), 10.0f);
    }
    g.setColour (Pal::inkDim);
    g.setFont (sans (11.0f, true));
    g.drawText ("GLOBAL", globalPanel.getX() + 14, globalPanel.getY() + 10, 110, 22, juce::Justification::centredLeft);

    paintModes (g);
}

static float xForFreq (juce::Rectangle<float> r, double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); }

void IctusEditor::paintModes (juce::Graphics& g)
{
    const auto area = modeArea.toFloat();
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (Pal::line);
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);

    const auto pl = area.reduced (16.0f, 12.0f).withTrimmedTop (16.0f).withTrimmedBottom (12.0f);
    g.setFont (sans (9.5f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xForFreq (pl, f);
        g.setColour (Pal::line.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), pl.getY(), pl.getBottom());
        g.setColour (Pal::inkDim);
        g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 20, (int) pl.getBottom() + 1, 40, 11, juce::Justification::centred);
    }
    g.setColour (Pal::line);
    g.drawHorizontalLine (juce::roundToInt (pl.getBottom()), pl.getX(), pl.getRight());

    const auto s = proc.readSettings();
    // amplitude of every mode as heard (strike x pickup), relative to the loudest
    float maxAmp = 1e-9f;
    for (int r = 0; r < 2; ++r)
        if (s.res[r].on)
            for (int i = 0; i < layouts[(size_t) r].count; ++i)
            {
                const auto& L = layouts[(size_t) r];
                maxAmp = juce::jmax (maxAmp, std::abs (L.exc[(size_t) i]) * 0.5f * (std::abs (L.outL[(size_t) i]) + std::abs (L.outR[(size_t) i])));
            }

    for (int r = 0; r < 2; ++r)
    {
        if (! s.res[r].on) continue;
        const auto& L = layouts[(size_t) r];
        const auto& rs = s.res[r];
        const double f0 = 261.6255653 * std::pow (2.0, (rs.tune + rs.fine * 0.01) / 12.0);
        const auto col = r == 0 ? Pal::teal : Pal::coral;
        for (int i = 0; i < L.count; ++i)
        {
            const double f = f0 * L.ratio[(size_t) i];
            if (f < 20.0 || f > 20000.0) continue;
            const float a = std::abs (L.exc[(size_t) i]) * 0.5f * (std::abs (L.outL[(size_t) i]) + std::abs (L.outR[(size_t) i])) / maxAmp;
            const float db = juce::Decibels::gainToDecibels (a, -60.0f);
            const float h = juce::jmap (db, -60.0f, 0.0f, 0.0f, pl.getHeight());
            if (h < 1.0f) continue;
            const double t = juce::jmax (0.01, (double) rs.decay * L.decay[(size_t) i]);
            const float alpha = juce::jlimit (0.2f, 1.0f, (float) juce::jmap (std::log10 (t), -2.0, 1.3, 0.2, 1.0));
            const float x = xForFreq (pl, f) + (r == 0 ? -1.0f : 1.0f);
            g.setColour (col.withAlpha (alpha));
            g.fillRect (juce::Rectangle<float> (x - 1.0f, pl.getBottom() - h, 2.0f, h));
        }
    }

    // captions
    g.setFont (sans (10.0f, true));
    g.setColour (Pal::inkDim);
    g.drawText ("MODES AT C4  (height = level, brightness = ring time)", (int) area.getX() + 16, (int) area.getY() + 8, 400, 14, juce::Justification::centredLeft);
    auto lg = juce::Rectangle<float> (area.getRight() - 300.0f, area.getY() + 8.0f, 284.0f, 14.0f);
    auto item = [&] (juce::Colour c, const juce::String& t, float w)
    {
        auto rr = lg.removeFromLeft (w);
        g.setColour (c);
        g.fillRect (rr.getX(), rr.getCentreY() - 1.5f, 14.0f, 3.0f);
        g.setColour (Pal::inkDim);
        g.drawText (t, rr.withTrimmedLeft (20.0f), juce::Justification::centredLeft);
    };
    item (Pal::teal, juce::String ("RES 1  ") + IctusProcessor::typeChoices[s.res[0].rp.type].toUpperCase(), 140.0f);
    item (Pal::coral, juce::String ("RES 2  ") + IctusProcessor::typeChoices[s.res[1].rp.type].toUpperCase(), 140.0f);
    g.setColour (Pal::inkDim);
    g.drawText ("VOICES " + juce::String (proc.voicesPlaying.load()), (int) area.getRight() - 110, (int) area.getBottom() - 22, 96, 14, juce::Justification::centredRight);
    g.drawText (s.structure == 1 ? "SERIAL  1 > 2" : "PARALLEL", (int) area.getRight() - 220, (int) area.getBottom() - 22, 100, 14, juce::Justification::centredRight);
}

//==============================================================================
void IctusEditor::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> knobs, int knobWidth)
{
    for (auto* k : knobs)
    {
        auto col = area.removeFromLeft (knobWidth);
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }
}

void IctusEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 0);
    header = b.removeFromTop (52).withTrimmedTop (4);
    preset.setBounds (header.removeFromRight (220).withSizeKeepingCentre (220, 30));

    modeArea = b.removeFromTop (140);
    b.removeFromTop (8);
    excRow = b.removeFromTop (104);
    b.removeFromTop (8);
    res1Row = b.removeFromTop (104);
    b.removeFromTop (8);
    res2Row = b.removeFromTop (104);
    b.removeFromTop (8);
    globalRow = b.removeFromTop (104);

    // exciters
    {
        auto row = excRow;
        malletPanel = row.removeFromLeft (120 + 5 * 64 + 16);
        row.removeFromLeft (10);
        noisePanel = row;

        auto m = malletPanel.reduced (8, 8);
        auto left = m.removeFromLeft (112);
        malletOn.setBounds (left.removeFromTop (26).reduced (4, 0));
        layoutKnobs (m, { &malletVol, &malletStiff, &malletVelStiff, &malletNoise, &malletColor }, 64);

        auto n = noisePanel.reduced (8, 8);
        auto nl = n.removeFromLeft (112);
        noiseOn.setBounds (nl.removeFromTop (26).reduced (4, 0));
        nl.removeFromTop (8);
        noiseType.setBounds (nl.removeFromTop (26).reduced (4, 0));
        layoutKnobs (n, { &noiseVol, &noiseFreq, &noiseRes, &noiseKey, &noiseA, &noiseD, &noiseS, &noiseR }, n.getWidth() / 8);
    }

    // resonators
    for (int r = 0; r < 2; ++r)
    {
        auto& u = res[(size_t) r];
        auto row = (r == 0 ? res1Row : res2Row).reduced (8, 8);
        auto left = row.removeFromLeft (124);
        u.on.setBounds (left.removeFromTop (26).reduced (4, 0));
        left.removeFromTop (6);
        u.type.setBounds (left.removeFromTop (26).reduced (4, 0));
        left.removeFromTop (6);
        u.modes.setBounds (left.removeFromTop (26).reduced (4, 0));
        layoutKnobs (row, { &u.tune, &u.fine, &u.key, &u.decay, &u.damp, &u.material, &u.shape, &u.inharm, &u.bright,
                            &u.hit, &u.listen, &u.width, &u.gain }, row.getWidth() / 13);
    }

    // global + keyboard
    {
        auto row = globalRow;
        globalPanel = row.removeFromLeft (124 + 6 * 66 + 16);
        row.removeFromLeft (10);
        keyboard.setBounds (row.reduced (0, 6));

        auto gp = globalPanel.reduced (8, 8);
        auto left = gp.removeFromLeft (124);
        left.removeFromTop (34);
        structure.setBounds (left.removeFromTop (26).reduced (4, 0));
        layoutKnobs (gp, { &voices, &velocity, &penvAmt, &penvTime, &bleed, &volume }, 66);
    }
}
