#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour bg       { 0xff10121a };
    const juce::Colour panel    { 0xff171a24 };
    const juce::Colour screen   { 0xff0b0d13 };
    const juce::Colour knob     { 0xff252a38 };
    const juce::Colour line     { 0xff2b3142 };
    const juce::Colour ink      { 0xffefe8da };
    const juce::Colour inkDim   { 0xff8d8c9c };
    const juce::Colour gold     { 0xffffc95c };
    const juce::Colour lavender { 0xffa99bff };
}

static juce::Font sans (float size, bool bold = false) { return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain); }

//==============================================================================
PollenLookAndFeel::PollenLookAndFeel()
{
    setColour (juce::Label::textColourId, Pal::inkDim);
    setColour (juce::Slider::textBoxTextColourId, Pal::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Pal::gold.withAlpha (0.3f));
    setColour (juce::TextEditor::textColourId, Pal::ink);
    setColour (juce::CaretComponent::caretColourId, Pal::gold);
    setColour (juce::ComboBox::textColourId, Pal::ink);
    setColour (juce::PopupMenu::backgroundColourId, Pal::bg);
    setColour (juce::PopupMenu::textColourId, Pal::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Pal::gold);
    setColour (juce::PopupMenu::highlightedTextColourId, Pal::bg);
    setColour (juce::TextButton::textColourOffId, Pal::ink);
    setColour (juce::TextButton::textColourOnId, Pal::bg);
    setColour (juce::TooltipWindow::backgroundColourId, Pal::bg);
    setColour (juce::TooltipWindow::textColourId, Pal::ink);
    setColour (juce::TooltipWindow::outlineColourId, Pal::line);
}

void PollenLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool centred = (bool) s.getProperties().getWithDefault ("centred", false);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::line);
    g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = centred ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path v;
        v.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (Pal::gold.withAlpha (0.25f));
        g.strokePath (v, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Pal::gold);
        g.strokePath (v, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = r - 7.0f;
    juce::ColourGradient grad (Pal::knob.brighter (0.25f), c.x, c.y - kr, Pal::knob.darker (0.3f), c.x, c.y + kr, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);
    const auto tip = c.getPointOnCircumference (kr * 0.72f, angle);
    g.setColour (Pal::ink);
    g.fillEllipse (tip.x - 2.2f, tip.y - 2.2f, 4.4f, 4.4f);
}

void PollenLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? Pal::line : Pal::panel);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (highlighted ? Pal::gold : Pal::line);
    g.drawRoundedRectangle (area, 6.0f, 1.0f);
}

void PollenLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (box.isMouseOver (true) ? Pal::gold : Pal::line);
    g.drawRoundedRectangle (area, 6.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (Pal::inkDim);
    g.fillPath (arrow);
}

juce::Font PollenLookAndFeel::getComboBoxFont (juce::ComboBox&) { return sans (13.0f, true); }
juce::Font PollenLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return sans (12.0f, true); }

void PollenLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
WaveView::WaveView (PollenProcessor& p) : proc (p), position (p.apvts.getParameter ("position"))
{
    setTooltip ("The source. Gold dots are the grains playing right now, the line is Position and the band around it is Spray.\n"
                "Click or drag to move Position, double-click to reset it. Drop an audio file here to play it.");
    heads.fill (-1.0f);
}

void WaveView::refresh()
{
    const int src = (int) proc.apvts.getRawParameterValue ("source")->load();
    const int ver = proc.sampleVersion();
    if (src != shownSource || (src == PollenProcessor::userSourceIndex() && ver != shownVersion))
    {
        overview = proc.overviewFor (src);
        shownSource = src;
        shownVersion = ver;
    }

    auto& e = proc.display();
    const int n = e.dispCount.load (std::memory_order_acquire);
    dots.clear();
    for (int i = 0; i < n; ++i)
        dots.push_back ({ e.dispPos[(size_t) i].load (std::memory_order_relaxed),
                          e.dispAmp[(size_t) i].load (std::memory_order_relaxed),
                          e.dispId[(size_t) i].load (std::memory_order_relaxed) });
    for (size_t v = 0; v < heads.size(); ++v)
        heads[v] = e.dispHead[v].load (std::memory_order_relaxed);
    repaint();
}

void WaveView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (dragOver ? Pal::gold : Pal::line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, dragOver ? 2.0f : 1.0f);

    const auto area = plotArea();
    const float mid = area.getCentreY(), halfH = area.getHeight() * 0.5f;

    // caption
    const bool isUser = shownSource == PollenProcessor::userSourceIndex();
    juce::String caption = isUser ? proc.getSampleFile().getFileName() : PollenProcessor::sourceNames()[shownSource];
    if (isUser && proc.isSampleMissing()) caption = "Missing: " + caption;
    g.setFont (sans (11.0f, true));
    g.setColour (Pal::inkDim);
    g.drawText (caption.toUpperCase(), bounds.reduced (14.0f, 5.0f).removeFromTop (14.0f), juce::Justification::centredLeft);

    if (proc.isLoading())
    {
        g.setFont (sans (14.0f, true));
        g.setColour (Pal::gold);
        g.drawText ("Loading ...", area, juce::Justification::centred);
        if (overview.empty()) return;
    }

    if (overview.empty())
    {
        g.setFont (sans (14.0f));
        g.setColour (Pal::inkDim);
        g.drawText ("Drop an audio file here, or click LOAD", area, juce::Justification::centred);
        return;
    }

    // waveform
    const int bins = (int) overview.size();
    float peak = 1.0e-3f;
    for (auto v : overview) peak = juce::jmax (peak, v);
    juce::Path wf;
    wf.startNewSubPath (area.getX(), mid);
    for (int i = 0; i < bins; ++i)
        wf.lineTo (area.getX() + area.getWidth() * (i + 0.5f) / bins, mid - halfH * 0.9f * overview[(size_t) i] / peak);
    for (int i = bins; --i >= 0;)
        wf.lineTo (area.getX() + area.getWidth() * (i + 0.5f) / bins, mid + halfH * 0.9f * overview[(size_t) i] / peak);
    wf.closeSubPath();
    g.setColour (Pal::lavender.withAlpha (0.14f));
    g.fillPath (wf);
    g.setColour (Pal::lavender.withAlpha (0.4f));
    g.strokePath (wf, juce::PathStrokeType (1.0f));

    // spray band and position
    const float pos = proc.apvts.getRawParameterValue ("position")->load();
    const float spray = proc.apvts.getRawParameterValue ("spray")->load();
    const float px = area.getX() + area.getWidth() * pos;
    const float sw = area.getWidth() * spray;
    g.setColour (Pal::gold.withAlpha (0.07f));
    g.fillRect (juce::Rectangle<float> (px - sw * 0.5f, area.getY(), sw, area.getHeight()).getIntersection (area));

    // voice playheads (Position moved by Scan and Drift)
    for (auto h : heads)
        if (h >= 0.0f)
        {
            const float hx = area.getX() + area.getWidth() * h;
            g.setColour (Pal::ink.withAlpha (0.18f));
            g.drawVerticalLine ((int) hx, area.getY(), area.getBottom());
        }

    g.setColour (Pal::gold.withAlpha (0.85f));
    g.drawLine (px, area.getY() - 6.0f, px, area.getBottom() + 6.0f, 1.5f);
    juce::Path handle;
    handle.addTriangle (px - 5.0f, area.getY() - 10.0f, px + 5.0f, area.getY() - 10.0f, px, area.getY() - 3.0f);
    g.fillPath (handle);

    // grains
    for (const auto& d : dots)
    {
        const float hash = (float) ((d.id * 2654435761u) >> 8) / 16777216.0f;
        const float x = area.getX() + area.getWidth() * juce::jlimit (0.0f, 1.0f, d.x);
        const float y = mid + (hash - 0.5f) * area.getHeight() * 0.8f;
        const float a = juce::jlimit (0.0f, 1.0f, d.amp);
        const float r = 1.5f + 3.5f * a;
        g.setColour (Pal::gold.withAlpha (0.12f * a));
        g.fillEllipse (x - r * 3.0f, y - r * 3.0f, r * 6.0f, r * 6.0f);
        g.setColour (Pal::gold.interpolatedWith (juce::Colours::white, 0.4f * a).withAlpha (0.35f + 0.65f * a));
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    }
}

void WaveView::setPositionFromX (float x)
{
    const auto area = plotArea();
    position->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (x - area.getX()) / area.getWidth()));
}

void WaveView::mouseDown (const juce::MouseEvent& e)
{
    position->beginChangeGesture();
    setPositionFromX ((float) e.x);
}

void WaveView::mouseDrag (const juce::MouseEvent& e) { setPositionFromX ((float) e.x); }
void WaveView::mouseUp (const juce::MouseEvent&) { position->endChangeGesture(); }

void WaveView::mouseDoubleClick (const juce::MouseEvent&)
{
    position->beginChangeGesture();
    position->setValueNotifyingHost (position->getDefaultValue());
    position->endChangeGesture();
}

bool WaveView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (proc.isAudioFile (f))
            return true;
    return false;
}

void WaveView::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (proc.isAudioFile (f))
        {
            proc.loadSampleAsync (juce::File (f));
            break;
        }
    repaint();
}

//==============================================================================
PollenEditor::PollenEditor (PollenProcessor& p)
    : AudioProcessorEditor (p), proc (p), wave (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        preset.addItem (proc.getProgramName (i), i + 1);
    preset.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    preset.onChange = [this] { if (preset.getSelectedId() > 0) proc.setCurrentProgram (preset.getSelectedId() - 1); };
    presetPrev.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + proc.getNumPrograms() - 1) % proc.getNumPrograms()); };
    presetNext.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    for (auto* c : std::initializer_list<juce::Component*> { &preset, &presetPrev, &presetNext })
        addAndMakeVisible (c);

    source.addItemList (PollenProcessor::sourceNames(), 1);
    sourceAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "source", source);
    source.setTooltip ("What the grains are cut from: five built-in sources tuned to C, or your own sample.");
    addAndMakeVisible (source);
    load.setTooltip ("Load an audio file (WAV, AIFF, FLAC, MP3...). You can also drop a file on the display.");
    load.onClick = [this] { chooseFile(); };
    addAndMakeVisible (load);

    addAndMakeVisible (wave);

    auto sec = [this] (const juce::String& t, std::vector<Knob*> k) { sections.push_back ({ t, std::move (k), {} }); };
    sec ("GRAIN", { makeKnob ("position", "Position", "Where in the source the grains are taken from."),
                    makeKnob ("size", "Size", "Length of each grain. Short = grainy texture, long = smooth, recognisable source."),
                    makeKnob ("density", "Density", "Grains started per second, per note. The level stays the same as you change it."),
                    makeKnob ("shape", "Shape", "Grain envelope: 0 = soft bell (smoothest), 100 = flat top (punchier, more of the source)."),
                    makeKnob ("spray", "Spray", "Random scatter of grain positions around Position. Mod wheel adds more.") });
    sec ("MOTION", { makeKnob ("scan", "Scan", "Moves the playhead through the source: +100 % = original speed, 0 = frozen, negative = backwards.", true),
                     makeKnob ("drift", "Drift", "Slow random wander of position and pitch. Keeps a held note alive."),
                     makeKnob ("reverse", "Reverse", "Share of grains played backwards."),
                     makeKnob ("spread", "Spread", "Stereo scatter of the grains.") });
    sec ("PITCH", { makeKnob ("transpose", "Transpose", "Pitch of the whole instrument in semitones. Built-in sources are in C.", true),
                    makeKnob ("jitter", "Jitter", "Random detune per grain: a few cents = chorus-like richness, more = unstable shimmer."),
                    makeKnob ("octaves", "Octaves", "Chance that a grain jumps an octave up (sometimes an octave and a fifth, or an octave down).") });
    sec ("VOICE", { makeKnob ("attack", "Attack", "Fade-in of each note."),
                    makeKnob ("release", "Release", "Fade-out after the key is released. Grains keep flowing meanwhile.") });
    sec ("SPACE", { makeKnob ("tone", "Tone", "Low-pass filter on the cloud."),
                    makeKnob ("reverb", "Reverb", "Wet/dry of the built-in ambient reverb."),
                    makeKnob ("decay", "Decay", "Reverb time."),
                    makeKnob ("volume", "Volume", "Output level.") });

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe9e4da));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, Pal::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, Pal::line);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Pal::gold);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Pal::gold.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, Pal::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, Pal::inkDim);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    setSize (940, 600);
    startTimerHz (30);
}

PollenEditor::~PollenEditor()
{
    stopTimer();
    for (auto& k : knobs) k->slider.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

PollenEditor::Knob* PollenEditor::makeKnob (const juce::String& id, const juce::String& name, const juce::String& tip, bool centred)
{
    auto k = std::make_unique<Knob>();
    k->slider.setLookAndFeel (&lnf);   // before the text box exists, so it takes our colours
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    k->slider.getProperties().set ("centred", centred);
    k->slider.setTooltip (tip);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    if (auto* p = proc.apvts.getParameter (id))
        k->slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    k->label.setText (name.toUpperCase(), juce::dontSendNotification);
    k->label.setFont (sans (10.5f, true));
    k->label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

void PollenEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a sample", proc.getSampleFile().getParentDirectory(), proc.audioWildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f == juce::File()) return;
                              proc.loadSampleAsync (f);
                          });
}

void PollenEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg);

    // wordmark
    auto h = header.reduced (18, 0);
    g.setColour (Pal::gold);
    g.setFont (sans (22.0f, true).withExtraKerningFactor (0.28f));
    g.drawText ("POLLEN", h.removeFromLeft (130), juce::Justification::centredLeft);
    g.setColour (Pal::inkDim);
    g.setFont (sans (11.0f));
    g.drawText ("granular", h.removeFromLeft (70), juce::Justification::centredLeft);

    if (messageTicks > 0)
    {
        g.setColour (Pal::ink.withAlpha (juce::jmin (1.0f, messageTicks / 20.0f)));
        g.setFont (sans (11.5f));
        g.drawText (message, header.withTrimmedLeft (220).withWidth (presetPrev.getX() - 230), juce::Justification::centredLeft);
    }

    for (auto& s : sections)
    {
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (s.bounds.toFloat(), 10.0f);
        g.setColour (Pal::gold.withAlpha (0.75f));
        g.setFont (sans (10.5f, true).withExtraKerningFactor (0.15f));
        g.drawText (s.title, s.bounds.reduced (12, 6).removeFromTop (14), juce::Justification::centredLeft);
    }
}

void PollenEditor::layoutSection (Section& s, juce::Rectangle<int> area)
{
    s.bounds = area;
    auto inner = area.reduced (6, 6).withTrimmedTop (14);
    const int w = inner.getWidth() / (int) s.knobs.size();
    for (auto* k : s.knobs)
    {
        auto cell = inner.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
}

void PollenEditor::resized()
{
    auto r = getLocalBounds();
    header = r.removeFromTop (54);
    {
        auto h = header.reduced (18, 12);
        load.setBounds (h.removeFromRight (64));
        h.removeFromRight (8);
        source.setBounds (h.removeFromRight (150));
        h.removeFromRight (28);
        presetNext.setBounds (h.removeFromRight (30));
        h.removeFromRight (4);
        preset.setBounds (h.removeFromRight (210));
        h.removeFromRight (4);
        presetPrev.setBounds (h.removeFromRight (30));
    }

    r.reduce (14, 0);
    keyboard.setBounds (r.removeFromBottom (66).withTrimmedBottom (10));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);   // C0..C7 + top C = 50 white keys
    keyboard.setLowestVisibleKey (24);
    r.removeFromBottom (8);
    waveBounds = r.removeFromTop (200);
    wave.setBounds (waveBounds);
    r.removeFromTop (10);

    auto layoutRow = [this] (juce::Rectangle<int> row, std::initializer_list<int> which)
    {
        int count = 0;
        for (int i : which) count += (int) sections[(size_t) i].knobs.size();
        const int gap = 10;
        const int unit = (row.getWidth() - gap * ((int) which.size() - 1)) / count;
        for (int i : which)
        {
            auto& s = sections[(size_t) i];
            layoutSection (s, row.removeFromLeft (unit * (int) s.knobs.size()));
            row.removeFromLeft (gap);
        }
    };
    const int rowH = (r.getHeight() - 10) / 2;
    layoutRow (r.removeFromTop (rowH), { 0, 1 });
    r.removeFromTop (10);
    layoutRow (r.removeFromTop (rowH), { 2, 3, 4 });
}

void PollenEditor::timerCallback()
{
    wave.refresh();
    if (proc.getCurrentProgram() != lastProgram)
    {
        lastProgram = proc.getCurrentProgram();
        preset.setSelectedId (lastProgram + 1, juce::dontSendNotification);
    }
    int v = 0;
    const auto st = proc.getStatus (v);
    if (v != statusVersion)
    {
        statusVersion = v;
        message = st;
        messageTicks = 150;
        repaint (header);
    }
    if (proc.isLoading())
        messageTicks = juce::jmax (messageTicks, 40);
    else if (messageTicks > 0 && --messageTicks < 20)
        repaint (header);
}
