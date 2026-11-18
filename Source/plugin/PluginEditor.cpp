#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int kW = 1160, kH = 700;
const Rectangle<int> kPaletteArea (16, 72, 150, 612);
const Rectangle<int> kCentreArea (182, 72, 712, 590);
const Rectangle<int> kStepPanel (182, 368, 712, 150);
const Rectangle<int> kInfoPanel (182, 530, 712, 154);
const Rectangle<int> kRightArea (910, 72, 234, 612);

String slotName (int s) { return String::charToString ((juce_wchar) ('A' + s)); }
}

// ================================================================== grid
Rectangle<float> GridView::cellBounds (int i) const
{
    const float gap = 5.0f, label = 14.0f;
    const float w = ((float) getWidth() - 15.0f * gap) / 16.0f;
    const float rowH = ((float) getHeight() - 2.0f * label - 10.0f) / 2.0f;
    const int row = i / 16, col = i % 16;
    const float y = label + (float) row * (rowH + label + 10.0f);
    return { (float) col * (w + gap), y, w, rowH };
}

int GridView::cellAt (Point<float> p) const
{
    for (int i = 0; i < sg::kNumSteps; ++i)
        if (cellBounds (i).expanded (2.5f, 6.0f).contains (p)) return i;
    return -1;
}

void GridView::paint (Graphics& g)
{
    const auto& t = ed.theme();
    const auto& p = ed.shown;
    const auto& eng = ed.proc.getEngine();
    const bool showingPlaying = eng.currentPattern() == ed.proc.getEditSlot();
    const int playStep = eng.currentStep();
    const bool light = t.bg.getPerceivedBrightness() > 0.5f;

    for (int i = 0; i < sg::kNumSteps; ++i)
    {
        auto r = cellBounds (i);
        const bool inside = i < p.length;
        // step number
        g.setColour (i % 4 == 0 ? t.text.withAlpha (inside ? 0.8f : 0.3f) : t.dimText.withAlpha (inside ? 0.8f : 0.3f));
        g.setFont (FontOptions (10.5f, i % 4 == 0 ? Font::bold : Font::plain));
        g.drawText (String (i + 1), r.withY (r.getY() - 14.0f).withHeight (13.0f), Justification::centredLeft);

        g.setColour (t.padBg.brighter (i % 8 < 4 ? 0.0f : 0.04f));
        g.fillRoundedRectangle (r, 6.0f);

        const int owner = p.ownerOf (i);
        if (owner >= 0)
        {
            const auto& s = p.steps[(size_t) owner];
            auto col = fxColour (s.fx, t);
            const float bright = 0.3f + 0.7f * s.prob;
            if (owner < i)
            {
                // tied continuation: joined to the pad on its left
                auto rr = r.withX (r.getX() - 6.0f).withWidth (r.getWidth() + 6.0f).reduced (0, r.getHeight() * 0.18f);
                g.setColour (col.withAlpha (0.55f * bright * (inside ? 1.0f : 0.35f)));
                g.fillRoundedRectangle (rr, 4.0f);
            }
            else
            {
                if (t.led && inside)
                {
                    g.setColour (col.withAlpha (0.22f * bright));
                    g.fillRoundedRectangle (r.expanded (3.0f), 8.0f);
                }
                g.setColour (col.withAlpha (bright * (inside ? 1.0f : 0.35f)));
                g.fillRoundedRectangle (r, 6.0f);
                // amount bar
                auto bar = r.reduced (5.0f).removeFromBottom (4.0f);
                g.setColour (Colours::black.withAlpha (0.25f));
                g.fillRoundedRectangle (bar, 2.0f);
                g.setColour (Colours::white.withAlpha (0.85f));
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * s.amount), 2.0f);
                g.setColour (col.getPerceivedBrightness() > 0.6f ? Colours::black.withAlpha (0.85f) : Colours::white);
                g.setFont (FontOptions (jmin (11.0f, r.getWidth() * 0.27f), Font::bold));
                g.drawFittedText (sg::fxShortName (s.fx), r.reduced (2.0f).withTrimmedBottom (r.getHeight() * 0.45f).toNearestInt(), Justification::centredBottom, 1, 0.7f);
                g.setFont (FontOptions (10.0f));
                g.drawText (String (sg::amountText (s.fx, s.amount)), r.reduced (2.0f).withTrimmedTop (r.getHeight() * 0.52f).withTrimmedBottom (8.0f), Justification::centred);
                if (s.prob < 1.0f)
                {
                    g.setFont (FontOptions (9.0f, Font::bold));
                    g.drawText (String (roundToInt (s.prob * 100.0f)) + "%", r.reduced (3.0f, 2.0f), Justification::topRight);
                }
            }
        }

        if (! inside)
        {
            g.setColour (t.bg.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 6.0f);
        }
        g.setColour (t.grid.withAlpha (i % 4 == 0 ? 1.0f : 0.6f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        if (i == ed.selectedStep)
        {
            g.setColour (t.accent2);
            g.drawRoundedRectangle (r.reduced (1.5f), 5.0f, 2.0f);
        }
        if (showingPlaying && i == playStep && inside)
        {
            g.setColour ((light ? Colours::black : Colours::white).withAlpha (0.18f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (t.accent);
            g.drawRoundedRectangle (r.expanded (1.5f), 7.0f, 2.5f);
            if (t.led)
            {
                g.setColour (t.accent.withAlpha (0.3f));
                g.drawRoundedRectangle (r.expanded (4.0f), 9.0f, 3.0f);
            }
        }
    }
}

void GridView::mouseDown (const MouseEvent& e)
{
    const int i = cellAt (e.position);
    lastPainted = -1;
    if (i < 0) return;
    ed.selectStep (i);
    if (e.mods.isPopupMenu() || e.mods.isAltDown()) { paintFx = sg::Fx::Count; return; }   // select only
    const auto& p = ed.shown;
    const int owner = p.ownerOf (i);
    paintFx = (owner == i && p.steps[(size_t) i].fx == ed.paletteFx) ? sg::Fx::Off : ed.paletteFx;
    ed.setStepFx (i, paintFx);
    lastPainted = i;
}

void GridView::mouseDrag (const MouseEvent& e)
{
    if (paintFx == sg::Fx::Count) return;
    const int i = cellAt (e.position);
    if (i < 0 || i == lastPainted) return;
    lastPainted = i;
    ed.setStepFx (i, paintFx);
    ed.selectStep (i);
}

void GridView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int i = cellAt (e.position);
    if (i >= 0) ed.changeAmount (i, w.deltaY > 0 ? 0.05f : -0.05f);
}

// ================================================================== pads
void PadButton::paint (Graphics& g)
{
    const auto& t = ed.theme();
    const auto& eng = ed.proc.getEngine();
    const bool on = (eng.padMask() >> idx) & 1u;
    const bool active = eng.activePad() == idx;
    const auto fx = (sg::Fx) ((int) ed.proc.apvts.getRawParameterValue ("pad" + String (idx + 1) + "fx")->load() + 1);
    const auto col = fxColour (fx, t);
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (active && t.led)
    {
        g.setColour (col.withAlpha (0.3f));
        g.fillRoundedRectangle (r.expanded (2.0f), 10.0f);
    }
    g.setColour (active ? col : (on ? col.withAlpha (0.6f) : t.padBg.interpolatedWith (col, 0.18f)));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (active ? col.brighter (0.4f) : col.withAlpha (0.7f));
    g.drawRoundedRectangle (r, 8.0f, active ? 2.5f : 1.2f);
    g.setColour (active ? (col.getPerceivedBrightness() > 0.6f ? Colours::black : Colours::white) : t.text);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (String (idx + 1), r.reduced (7.0f, 4.0f), Justification::topLeft);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText (sg::fxShortName (fx), r, Justification::centred);
}

void PadButton::mouseDown (const MouseEvent&) { ed.proc.padDown (idx); }
void PadButton::mouseUp (const MouseEvent&) { ed.proc.padUp (idx); }

// ================================================================== editor
StepGridEditor::StepGridEditor (StepGridProcessor& owner) : AudioProcessorEditor (&owner), proc (owner)
{
    setLookAndFeel (&lnf);
    shown = proc.getPattern (proc.getEditSlot());

    // ---- header
    const auto& f = sg::factoryPatterns();
    String lastCat;
    for (int i = 0; i < (int) f.size(); ++i)
    {
        if (lastCat != f[(size_t) i].category) { lastCat = f[(size_t) i].category; presetBox.addSectionHeading (lastCat); }
        presetBox.addItem (f[(size_t) i].pattern.name, i + 1);
    }
    presetBox.setTextWhenNothingSelected ("Load a pattern into this slot...");
    presetBox.onChange = [this]
    {
        if (updating || presetBox.getSelectedId() <= 0) return;
        proc.loadFactory (presetBox.getSelectedId() - 1, proc.getEditSlot());
        refreshPattern();
    };
    addAndMakeVisible (presetBox);
    for (int i = 0; i < numThemes(); ++i) themeBox.addItem (themeAt (i).name, i + 1);
    themeBox.onChange = [this] { proc.setTheme (themeBox.getSelectedId() - 1); applyTheme(); };
    addAndMakeVisible (themeBox);
    addAndMakeVisible (syncButton);
    syncAtt = std::make_unique<ButtonAttachment> (proc.apvts, "sync", syncButton);
    bpmSlider.setSliderStyle (Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (Slider::TextBoxRight, false, 78, 22);
    bpmSlider.setTooltip ("Own tempo: used when Host Sync is off, when the host gives no tempo, and in the standalone");
    addAndMakeVisible (bpmSlider);
    bpmAtt = std::make_unique<SliderAttachment> (proc.apvts, "bpm", bpmSlider);

    // ---- palette
    for (int i = 0; i < sg::kNumFx; ++i)
    {
        auto& b = paletteButtons[(size_t) i];
        b.setButtonText (i == 0 ? String ("Off / erase") : String (sg::fxName ((sg::Fx) i)));
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { selectPalette ((sg::Fx) i); };
        addAndMakeVisible (b);
    }

    // ---- slots
    for (int s = 0; s < sg::kNumPatterns; ++s)
    {
        auto& b = slotButtons[(size_t) s];
        b.setButtonText (slotName (s));
        b.setTooltip ("Pattern " + slotName (s) + " (MIDI note " + MidiMessage::getMidiNoteName (midimap::kPatternBase + s, true, true, 3) + ")");
        b.onClick = [this, s] { proc.selectPattern (s); refreshPattern(); };
        addAndMakeVisible (b);
    }
    copyButton.onClick = [this] { clipboard = proc.getPattern (proc.getEditSlot()); hasClipboard = true; };
    pasteButton.onClick = [this] { if (hasClipboard) { proc.setPattern (proc.getEditSlot(), clipboard); refreshPattern(); } };
    clearButton.onClick = [this] { proc.editPattern (proc.getEditSlot(), [] (sg::Pattern& p) { p.clear(); p.name = "Empty"; }); refreshPattern(); };
    randButton.onClick = [this]
    {
        proc.editPattern (proc.getEditSlot(), [] (sg::Pattern& p) { sg::randomise (p, (uint32_t) Random::getSystemRandom().nextInt()); p.name = "Random"; });
        refreshPattern();
    };
    for (auto* b : { &copyButton, &pasteButton, &clearButton, &randButton }) addAndMakeVisible (*b);
    chainBox.addItemList (proc.apvts.getParameter ("chain")->getAllValueStrings(), 1);
    chainBox.setTooltip ("Chain: play patterns A to the chosen slot in order, one after another");
    addAndMakeVisible (chainBox);
    chainAtt = std::make_unique<ComboAttachment> (proc.apvts, "chain", chainBox);

    // ---- pattern settings
    auto setupLabel = [this] (Label& l, const String& text)
    {
        l.setText (text, dontSendNotification);
        l.setFont (FontOptions (11.0f, Font::bold));
        l.setJustificationType (Justification::centredRight);
        addAndMakeVisible (l);
    };
    setupLabel (rateLabel, "RATE");
    setupLabel (lengthLabel, "LENGTH");
    setupLabel (swingLabel, "SWING");
    for (int r = 0; r < sg::kNumRates; ++r) rateBox.addItem (sg::rateName ((sg::Rate) r), r + 1);
    rateBox.onChange = [this]
    {
        if (updating) return;
        const auto r = (sg::Rate) (rateBox.getSelectedId() - 1);
        proc.editPattern (proc.getEditSlot(), [r] (sg::Pattern& p) { p.rate = r; });
        refreshPattern();
    };
    addAndMakeVisible (rateBox);
    lengthSlider.setSliderStyle (Slider::LinearHorizontal);
    lengthSlider.setRange (1, 32, 1);
    lengthSlider.setTextBoxStyle (Slider::TextBoxRight, false, 40, 22);
    lengthSlider.onValueChange = [this]
    {
        if (updating) return;
        const int len = (int) lengthSlider.getValue();
        proc.editPattern (proc.getEditSlot(), [len] (sg::Pattern& p) { p.length = len; });
        refreshPattern();
    };
    addAndMakeVisible (lengthSlider);
    swingSlider.setSliderStyle (Slider::LinearHorizontal);
    swingSlider.setRange (0, 100, 1);
    swingSlider.setTextValueSuffix ("%");
    swingSlider.setTextBoxStyle (Slider::TextBoxRight, false, 48, 22);
    swingSlider.onValueChange = [this]
    {
        if (updating) return;
        const float s = (float) swingSlider.getValue() / 100.0f;
        proc.editPattern (proc.getEditSlot(), [s] (sg::Pattern& p) { p.swing = s; });
        refreshPattern();
    };
    addAndMakeVisible (swingSlider);

    addAndMakeVisible (grid);

    // ---- step editor
    for (int i = 0; i < sg::kNumFx; ++i) stepFxBox.addItem (sg::fxName ((sg::Fx) i), i + 1);
    stepFxBox.onChange = [this] { if (! updating) setStepFx (selectedStep, (sg::Fx) (stepFxBox.getSelectedId() - 1)); };
    addAndMakeVisible (stepFxBox);
    setupKnob (amountKnob, amountLabel, "AMOUNT");
    setupKnob (probKnob, probLabel, "CHANCE");
    setupKnob (tieKnob, tieLabel, "LENGTH");
    amountKnob.setRange (0, 1, 0.01);
    amountKnob.textFromValueFunction = [this] (double v) { return String (sg::amountText (shown.steps[(size_t) selectedStep].fx, (float) v)); };
    probKnob.setRange (0, 100, 1);
    probKnob.setTextValueSuffix ("%");
    tieKnob.setRange (1, 16, 1);
    tieKnob.textFromValueFunction = [] (double v) { return String ((int) v) + ((int) v == 1 ? " step" : " steps"); };
    amountKnob.onValueChange = [this]
    {
        if (updating) return;
        const int i = selectedStep;
        const float a = (float) amountKnob.getValue();
        proc.editPattern (proc.getEditSlot(), [i, a] (sg::Pattern& p) { p.steps[(size_t) i].amount = a; });
        refreshPattern();
    };
    probKnob.onValueChange = [this]
    {
        if (updating) return;
        const int i = selectedStep;
        const float pr = (float) probKnob.getValue() / 100.0f;
        proc.editPattern (proc.getEditSlot(), [i, pr] (sg::Pattern& p) { p.steps[(size_t) i].prob = pr; });
        refreshPattern();
    };
    tieKnob.onValueChange = [this]
    {
        if (updating) return;
        const int i = selectedStep;
        const int len = (int) tieKnob.getValue();
        proc.editPattern (proc.getEditSlot(), [i, len] (sg::Pattern& p)
        {
            auto& s = p.steps[(size_t) i];
            if (s.fx == sg::Fx::Off) return;
            s.length = jlimit (1, jmax (1, sg::kNumSteps - i), len);
            for (int j = i + 1; j < jmin (sg::kNumSteps, i + s.length); ++j) p.steps[(size_t) j] = {};   // swallowed by the tie
        });
        refreshPattern();
    };

    // ---- perform pads
    for (int i = 0; i < sg::kNumPads; ++i)
    {
        pads.push_back (std::make_unique<PadButton> (*this, i));
        pads.back()->setTooltip ("Hold to override the grid (MIDI note " + MidiMessage::getMidiNoteName (midimap::kPadBase + i, true, true, 3) + ")");
        addAndMakeVisible (*pads.back());
        auto& cb = padFxBoxes[(size_t) i];
        cb.addItemList (proc.apvts.getParameter ("pad" + String (i + 1) + "fx")->getAllValueStrings(), 1);
        addAndMakeVisible (cb);
        padFxAtts.push_back (std::make_unique<ComboAttachment> (proc.apvts, "pad" + String (i + 1) + "fx", cb));
        auto& sl = padAmtSliders[(size_t) i];
        sl.setSliderStyle (Slider::LinearHorizontal);
        sl.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        sl.setPopupDisplayEnabled (true, false, this);
        addAndMakeVisible (sl);
        padAmtAtts.push_back (std::make_unique<SliderAttachment> (proc.apvts, "pad" + String (i + 1) + "amt", sl));
    }
    addAndMakeVisible (latchButton);
    latchAtt = std::make_unique<ButtonAttachment> (proc.apvts, "latch", latchButton);

    // ---- master
    setupKnob (mixKnob, mixLabel, "MIX");
    setupKnob (outKnob, outLabel, "OUTPUT");
    mixAtt = std::make_unique<SliderAttachment> (proc.apvts, "mix", mixKnob);
    outAtt = std::make_unique<SliderAttachment> (proc.apvts, "output", outKnob);

    setSize (kW, kH);
    applyTheme();
    selectPalette (sg::Fx::Stutter);
    refreshPattern();
    startTimerHz (30);
}

StepGridEditor::~StepGridEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void StepGridEditor::setupKnob (Slider& s, Label& l, const String& name)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
    addAndMakeVisible (s);
    l.setText (name, dontSendNotification);
    l.setFont (FontOptions (11.0f, Font::bold));
    l.setJustificationType (Justification::centred);
    addAndMakeVisible (l);
}

void StepGridEditor::applyTheme()
{
    lastTheme = proc.getTheme();
    lnf.setTheme (themeAt (lastTheme));
    themeBox.setSelectedId (lastTheme + 1, dontSendNotification);
    for (auto* l : { &rateLabel, &lengthLabel, &swingLabel, &amountLabel, &probLabel, &tieLabel, &mixLabel, &outLabel })
        l->setColour (Label::textColourId, theme().dimText);
    selectPalette (paletteFx);
    sendLookAndFeelChange();
    repaint();
}

void StepGridEditor::selectPalette (sg::Fx f)
{
    paletteFx = f;
    const auto& t = theme();
    for (int i = 0; i < sg::kNumFx; ++i)
    {
        auto& b = paletteButtons[(size_t) i];
        const auto col = fxColour ((sg::Fx) i, t);
        const bool sel = (int) f == i;
        b.setColour (TextButton::buttonColourId, sel ? col : t.panel.interpolatedWith (col, 0.22f));
        b.setColour (TextButton::textColourOffId, sel ? (col.getPerceivedBrightness() > 0.6f ? Colours::black : Colours::white) : t.text);
    }
}

void StepGridEditor::selectStep (int i)
{
    selectedStep = jlimit (0, sg::kNumSteps - 1, i);
    refreshStepEditor();
    grid.repaint();
}

void StepGridEditor::setStepFx (int i, sg::Fx fx)
{
    proc.editPattern (proc.getEditSlot(), [i, fx] (sg::Pattern& p)
    {
        const int owner = p.ownerOf (i);
        if (owner >= 0 && owner < i) p.steps[(size_t) owner].length = i - owner;   // cut the tie that covered i
        auto& s = p.steps[(size_t) i];
        if (fx == sg::Fx::Off) { s = {}; return; }
        if (s.fx == sg::Fx::Off) s = {};
        s.fx = fx;
    });
    refreshPattern();
}

void StepGridEditor::changeAmount (int i, float delta)
{
    const int owner = shown.ownerOf (i);
    if (owner < 0) return;
    proc.editPattern (proc.getEditSlot(), [owner, delta] (sg::Pattern& p)
    {
        auto& s = p.steps[(size_t) owner];
        s.amount = jlimit (0.0f, 1.0f, s.amount + delta);
    });
    selectStep (owner);
    refreshPattern();
}

void StepGridEditor::refreshPattern()
{
    shown = proc.getPattern (proc.getEditSlot());
    const ScopedValueSetter<bool> svs (updating, true);
    rateBox.setSelectedId ((int) shown.rate + 1, dontSendNotification);
    lengthSlider.setValue (shown.length, dontSendNotification);
    swingSlider.setValue (std::round (shown.swing * 100.0f), dontSendNotification);
    int presetId = 0;
    const auto& f = sg::factoryPatterns();
    for (int i = 0; i < (int) f.size(); ++i)
        if (f[(size_t) i].pattern.name == shown.name) presetId = i + 1;
    presetBox.setSelectedId (presetId, dontSendNotification);
    refreshStepEditor();
    grid.repaint();
}

void StepGridEditor::refreshStepEditor()
{
    const ScopedValueSetter<bool> svs (updating, true);
    const int owner = shown.ownerOf (selectedStep);
    const auto& s = shown.steps[(size_t) selectedStep];
    stepFxBox.setSelectedId ((int) s.fx + 1, dontSendNotification);
    const bool active = s.fx != sg::Fx::Off;
    amountKnob.setValue (s.amount, dontSendNotification);
    probKnob.setValue (std::round (s.prob * 100.0f), dontSendNotification);
    tieKnob.setValue (s.length, dontSendNotification);
    amountKnob.updateText();
    for (auto* k : { &amountKnob, &probKnob, &tieKnob }) k->setEnabled (active);
    const auto col = fxColour (s.fx, theme());
    amountKnob.getProperties().set ("colour", (int64) col.getARGB());
    probKnob.getProperties().set ("colour", (int64) col.getARGB());
    tieKnob.getProperties().set ("colour", (int64) col.getARGB());
    ignoreUnused (owner);
    repaint (kStepPanel);
}

void StepGridEditor::timerCallback()
{
    if (proc.getTheme() != lastTheme) applyTheme();
    const auto now = proc.getPattern (proc.getEditSlot());
    if (! (now == shown) || now.name != shown.name) refreshPattern();

    const auto& eng = proc.getEngine();
    const auto& t = theme();
    const int playing = eng.currentPattern(), queued = eng.queuedPattern();
    const bool blink = (Time::getMillisecondCounter() / 160) % 2 == 0;
    for (int s = 0; s < sg::kNumPatterns; ++s)
    {
        auto& b = slotButtons[(size_t) s];
        const bool edit = s == proc.getEditSlot();
        Colour c = t.panel.brighter (0.08f);
        if (s == playing) c = t.accent;
        else if (s == queued && blink) c = t.accent.withAlpha (0.45f);
        b.setColour (TextButton::buttonColourId, c);
        b.setColour (TextButton::textColourOffId, s == playing ? t.bg : t.text);
        b.setButtonText (edit ? "[" + slotName (s) + "]" : slotName (s));
    }
    grid.repaint();
    for (auto& p : pads) p->repaint();
    repaint (0, 0, kW, 60);
}

void StepGridEditor::paint (Graphics& g)
{
    const auto& t = theme();
    g.fillAll (t.bg);

    // header
    g.setColour (t.panel);
    g.fillRect (0, 0, kW, 56);
    g.setColour (t.grid);
    g.drawHorizontalLine (56, 0, (float) kW);
    g.setColour (t.text);
    g.setFont (FontOptions (24.0f, Font::bold));
    g.drawText ("STEP GRID", 18, 10, 160, 36, Justification::centredLeft);
    g.setColour (t.accent);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText ("32-STEP GLITCH SEQUENCER", 152, 18, 150, 22, Justification::centredLeft);
    const auto& eng = proc.getEngine();
    g.setColour (eng.hostLocked() ? t.accent2 : t.dimText);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawFittedText (String (eng.bpmInUse(), 1) + " BPM\n" + (eng.hostLocked() ? "HOST" : "FREE RUN"), 1040, 8, 104, 40, Justification::centredRight, 2);

    auto panel = [&] (Rectangle<int> r, const String& title)
    {
        g.setColour (t.panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (t.grid);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (t.dimText);
            g.setFont (FontOptions (11.0f, Font::bold));
            g.drawText (title, r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 16, Justification::centredLeft);
        }
    };
    panel (kPaletteArea, "EFFECT BRUSH");
    panel (kStepPanel, "");
    panel (kInfoPanel, "");
    panel (kRightArea, "PERFORM PADS");

    // step editor title
    const auto& s = shown.steps[(size_t) selectedStep];
    const int owner = shown.ownerOf (selectedStep);
    g.setColour (t.text);
    g.setFont (FontOptions (15.0f, Font::bold));
    String title = "STEP " + String (selectedStep + 1) + "  /  PATTERN " + slotName (proc.getEditSlot());
    g.drawText (title, kStepPanel.getX() + 16, kStepPanel.getY() + 10, 300, 20, Justification::centredLeft);
    g.setColour (t.dimText);
    g.setFont (FontOptions (12.0f));
    String hint;
    if (owner >= 0 && owner != selectedStep) hint = "Tied to step " + String (owner + 1) + " (" + sg::fxName (shown.steps[(size_t) owner].fx) + ")";
    else if (s.fx == sg::Fx::Off) hint = "Empty step: plays the input dry";
    else hint = sg::fxName (s.fx);
    g.drawText (hint, kStepPanel.getX() + 16, kStepPanel.getY() + 104, 210, 18, Justification::centredLeft);

    static const char* help[sg::kNumFx] = {
        "No effect.",
        "Repeats a slice; Amount sets 1/8, 1/16, 1/32 or 1/64.",
        "Slows the audio to a halt; Amount = how fast it stops.",
        "Spins the audio up from a stop; Amount = how fast.",
        "Plays the audio before the step backwards; Amount = blend.",
        "Bit depth and sample-rate crush; Amount = 10 to 3 bits.",
        "Low-pass filter sweeps down over the step; Amount = depth and resonance.",
        "High-pass filter sweeps up over the step; Amount = depth.",
        "1/32 rhythmic chop; Amount = depth.",
        "Drops the pitch; Amount = 0 to 12 semitones.",
        "Throws the sound to one side (alternates L and R); Amount = width.",
        "Sends the step into a dotted-1/8 echo that rings on; Amount = feedback.",
    };
    g.setColour (t.dimText);
    g.setFont (FontOptions (12.0f));
    g.drawFittedText (help[(int) s.fx], kStepPanel.getX() + 556, kStepPanel.getY() + 16, 144, 118, Justification::centredLeft, 6);

    // info panel
    auto info = kInfoPanel.reduced (16, 12);
    g.setColour (t.text);
    g.setFont (FontOptions (12.5f, Font::bold));
    g.drawText ("HOW TO PLAY", info.removeFromTop (18), Justification::centredLeft);
    g.setColour (t.dimText);
    g.setFont (FontOptions (12.5f));
    g.drawFittedText ("Pick an effect brush, then click or drag across the grid to paint steps (click a step with the same brush to erase). "
                      "Right-click selects a step without painting; the mouse wheel changes its amount. Brightness shows the step's chance to fire.\n"
                      "MIDI: notes C1-G1 (36-43) switch to patterns A-H on the next step. Notes C2-G2 (48-55) play perform pads 1-8. "
                      "With the host stopped, Step Grid keeps running on its own clock.",
                      info, Justification::topLeft, 6);

    // master title
    g.setColour (t.dimText);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText ("MASTER", kRightArea.getX() + 12, 572, 100, 16, Justification::centredLeft);
}

void StepGridEditor::resized()
{
    presetBox.setBounds (320, 14, 250, 28);
    themeBox.setBounds (580, 14, 120, 28);
    syncButton.setBounds (712, 14, 96, 28);
    bpmSlider.setBounds (816, 14, 210, 28);

    for (int i = 0; i < sg::kNumFx; ++i)
        paletteButtons[(size_t) i].setBounds (kPaletteArea.getX() + 10, kPaletteArea.getY() + 28 + i * 40, kPaletteArea.getWidth() - 20, 35);

    int x = kCentreArea.getX();
    for (int s = 0; s < sg::kNumPatterns; ++s) { slotButtons[(size_t) s].setBounds (x, 72, 42, 32); x += 45; }
    x += 8;
    for (auto* b : { &copyButton, &pasteButton, &clearButton, &randButton }) { b->setBounds (x, 72, 62, 32); x += 65; }
    chainBox.setBounds (x + 4, 74, kCentreArea.getRight() - x - 4, 28);

    rateLabel.setBounds (182, 116, 40, 26);
    rateBox.setBounds (226, 116, 90, 26);
    lengthLabel.setBounds (324, 116, 58, 26);
    lengthSlider.setBounds (386, 116, 200, 26);
    swingLabel.setBounds (596, 116, 52, 26);
    swingSlider.setBounds (652, 116, 242, 26);

    grid.setBounds (182, 152, 712, 204);

    stepFxBox.setBounds (kStepPanel.getX() + 16, kStepPanel.getY() + 40, 200, 28);
    const int ky = kStepPanel.getY() + 12;
    amountLabel.setBounds (kStepPanel.getX() + 236, ky, 96, 16);
    amountKnob.setBounds (kStepPanel.getX() + 236, ky + 16, 96, 104);
    probLabel.setBounds (kStepPanel.getX() + 340, ky, 96, 16);
    probKnob.setBounds (kStepPanel.getX() + 340, ky + 16, 96, 104);
    tieLabel.setBounds (kStepPanel.getX() + 444, ky, 96, 16);
    tieKnob.setBounds (kStepPanel.getX() + 444, ky + 16, 96, 104);

    const int px = kRightArea.getX() + 10, colW = 103;
    for (int i = 0; i < sg::kNumPads; ++i)
    {
        const int cx = px + (i % 2) * (colW + 8), cy = kRightArea.getY() + 28 + (i / 2) * 104;
        pads[(size_t) i]->setBounds (cx, cy, colW, 54);
        padFxBoxes[(size_t) i].setBounds (cx, cy + 57, colW, 22);
        padAmtSliders[(size_t) i].setBounds (cx + 2, cy + 81, colW - 4, 16);
    }
    latchButton.setBounds (px, kRightArea.getY() + 448, 214, 28);
    mixLabel.setBounds (px, 596, 100, 14);
    mixKnob.setBounds (px, 610, 100, 72);
    outLabel.setBounds (px + 114, 596, 100, 14);
    outKnob.setBounds (px + 114, 610, 100, 72);
}
