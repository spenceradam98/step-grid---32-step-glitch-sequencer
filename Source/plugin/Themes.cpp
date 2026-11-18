#include "Themes.h"

using namespace juce;

namespace
{
const Theme kThemes[] = {
    // name        bg                    panel                 padBg                 grid                  accent                accent2               text                  dim
    { "Midnight", Colour (0xff0d1117), Colour (0xff161c26), Colour (0xff070a10), Colour (0x2250c8ff), Colour (0xff35d6ff), Colour (0xffff4fd8), Colour (0xffe6edf5), Colour (0xff7d8a9c), false },
    { "Magma",    Colour (0xff120808), Colour (0xff1e0f0d), Colour (0xff0a0303), Colour (0x26ff6a2a), Colour (0xffff6a2a), Colour (0xffffd23f), Colour (0xfff7e6dc), Colour (0xff9c7f75), false },
    { "Mint",     Colour (0xff0b1411), Colour (0xff12201b), Colour (0xff050c09), Colour (0x2242ffb0), Colour (0xff42ffb0), Colour (0xff9d7bff), Colour (0xffe2f7ee), Colour (0xff7a9a8d), false },
    { "Daylight", Colour (0xffe9ecf1), Colour (0xfff7f8fa), Colour (0xffdfe4ec), Colour (0x30304060), Colour (0xff2563eb), Colour (0xffe11d74), Colour (0xff1a2230), Colour (0xff6a7487), false },
    { "Vapor",    Colour (0xff1a1030), Colour (0xff24183f), Colour (0xff120a24), Colour (0x2aff8ae2), Colour (0xffff8ae2), Colour (0xff5ef2ff), Colour (0xfff4e9ff), Colour (0xff9a8ab8), false },
    { "Club LED", Colour (0xff050505), Colour (0xff0e0e0f), Colour (0xff171718), Colour (0x30ffffff), Colour (0xffff2d3c), Colour (0xff2dff7a), Colour (0xfff2f2f2), Colour (0xff7c7c7c), true },
};
}

const Theme& themeAt (int i) { return kThemes[jlimit (0, numThemes() - 1, i)]; }
int numThemes() { return (int) (sizeof (kThemes) / sizeof (kThemes[0])); }
int themeIndexByName (const String& name)
{
    for (int i = 0; i < numThemes(); ++i) if (name == kThemes[i].name) return i;
    return 0;
}

Colour fxColour (sg::Fx f, const Theme& t)
{
    static const uint32 cols[sg::kNumFx] = {
        0xff3a3f48,   // Off
        0xff3b82f6,   // Stutter
        0xffef4444,   // Tape Stop
        0xfff97316,   // Tape Start
        0xffa855f7,   // Reverse
        0xffeab308,   // Bitcrush
        0xff14b8a6,   // LP sweep
        0xff22d3ee,   // HP sweep
        0xff22c55e,   // Gate
        0xffec4899,   // Pitch down
        0xff84cc16,   // Pan flick
        0xff6366f1,   // Echo throw
    };
    auto c = Colour (cols[jlimit (0, sg::kNumFx - 1, (int) f)]);
    if (f == sg::Fx::Off) return t.padBg.brighter (0.15f);
    if (t.led) c = c.withSaturation (jmin (1.0f, c.getSaturation() * 1.15f)).brighter (0.15f);
    if (t.bg.getPerceivedBrightness() > 0.5f) c = c.darker (0.1f);
    return c;
}

StepGridLookAndFeel::StepGridLookAndFeel() { setTheme (themeAt (0)); }

void StepGridLookAndFeel::setTheme (const Theme& t)
{
    th = &t;
    setColour (ResizableWindow::backgroundColourId, t.bg);
    setColour (Label::textColourId, t.text);
    setColour (ComboBox::backgroundColourId, t.padBg);
    setColour (ComboBox::textColourId, t.text);
    setColour (ComboBox::outlineColourId, t.grid.withAlpha (0.6f));
    setColour (ComboBox::arrowColourId, t.accent);
    setColour (PopupMenu::backgroundColourId, t.panel);
    setColour (PopupMenu::textColourId, t.text);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.3f));
    setColour (PopupMenu::highlightedTextColourId, t.text);
    setColour (PopupMenu::headerTextColourId, t.accent);
    setColour (TextButton::textColourOffId, t.text);
    setColour (TextButton::textColourOnId, t.bg);
    setColour (TextButton::buttonColourId, t.panel);
    setColour (Slider::textBoxTextColourId, t.text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (TooltipWindow::backgroundColourId, t.panel);
    setColour (TooltipWindow::textColourId, t.text);
    setColour (BubbleComponent::backgroundColourId, t.panel);
    setColour (BubbleComponent::outlineColourId, t.grid);
}

void StepGridLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto& t = *th;
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float size = jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float rad = size * 0.5f;
    const Colour col = s.getProperties().contains ("colour") ? Colour ((uint32) (int64) s.getProperties()["colour"]) : t.accent;
    const float ang = start + pos * (end - start);
    if (t.led)
    {
        const int leds = 15;
        for (int i = 0; i < leds; ++i)
        {
            const float a = start + (end - start) * (float) i / (float) (leds - 1);
            const bool lit = a <= ang + 0.001f;
            const auto p = c.getPointOnCircumference (rad - 3.0f, a);
            if (lit) { g.setColour (col.withAlpha (0.25f)); g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (p)); }
            g.setColour (lit ? col : t.padBg.brighter (0.2f));
            g.fillEllipse (Rectangle<float> (3.5f, 3.5f).withCentre (p));
        }
    }
    else
    {
        Path track, val;
        track.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, start, end, true);
        g.setColour (t.grid.withAlpha (0.5f));
        g.strokePath (track, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        val.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, start, ang, true);
        g.setColour (s.isEnabled() ? col : t.dimText);
        g.strokePath (val, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (t.bg.getPerceivedBrightness() > 0.5f ? t.padBg.darker (0.05f) : t.panel.brighter (0.15f));
    g.fillEllipse (r.reduced (size * 0.22f));
    g.setColour (t.text);
    g.drawLine (Line<float> (c.getPointOnCircumference (rad * 0.1f, ang), c.getPointOnCircumference (rad * 0.5f, ang)), 2.2f);
}

void StepGridLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                            Slider::SliderStyle, Slider& s)
{
    const auto& t = *th;
    auto r = Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (t.padBg.brighter (0.1f));
    g.fillRoundedRectangle (r, 3.0f);
    const Colour col = s.getProperties().contains ("colour") ? Colour ((uint32) (int64) s.getProperties()["colour"]) : t.accent;
    g.setColour (col);
    g.fillRoundedRectangle (r.withRight (pos), 3.0f);
    g.setColour (t.text);
    g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre ({ pos, r.getCentreY() }));
}

void StepGridLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& base, bool hi, bool down)
{
    const auto& t = *th;
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    Colour fill = base == findColour (TextButton::buttonColourId) ? t.panel.brighter (0.08f) : base;
    if (on && base == findColour (TextButton::buttonColourId)) fill = t.accent;
    if (hi) fill = fill.brighter (0.12f);
    if (down) fill = fill.darker (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 5.0f);
    if (on && t.led)
    {
        g.setColour (fill.withAlpha (0.35f));
        g.drawRoundedRectangle (r.expanded (1.0f), 6.0f, 3.0f);
    }
    g.setColour (t.grid.withAlpha (0.9f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void StepGridLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool hi, bool down)
{
    drawButtonBackground (g, b, findColour (TextButton::buttonColourId), hi, down);
    g.setColour (b.getToggleState() ? th->bg : th->text);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

void StepGridLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto& t = *th;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (t.padBg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? t.accent : t.grid.withAlpha (0.9f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (t.accent);
    g.fillPath (arrow);
}
