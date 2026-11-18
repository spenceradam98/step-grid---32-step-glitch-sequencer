// UI themes: touchstorm's five plus Club LED. The theme is saved with each plugin instance.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Pattern.h"

struct Theme
{
    const char* name;
    juce::Colour bg, panel, padBg, grid, accent, accent2, text, dimText;
    bool led = false;   // Club LED: pads glow like backlit rubber buttons
};

const Theme& themeAt (int index);
int numThemes();
int themeIndexByName (const juce::String& name);
juce::Colour fxColour (sg::Fx f, const Theme& t);

class StepGridLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StepGridLookAndFeel();
    void setTheme (const Theme& t);
    const Theme& theme() const { return *th; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (13.0f, h * 0.5f), juce::Font::bold)); }

private:
    const Theme* th = nullptr;
};
