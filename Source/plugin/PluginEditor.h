#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Themes.h"

class StepGridEditor;

// The 32-pad grid (2 rows of 16): pads coloured by effect, brightness = probability, playhead sweep.
class GridView : public juce::Component
{
public:
    explicit GridView (StepGridEditor& e) : ed (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::Rectangle<float> cellBounds (int i) const;
    int cellAt (juce::Point<float> p) const;

private:
    StepGridEditor& ed;
    sg::Fx paintFx = sg::Fx::Off;
    int lastPainted = -1;
};

// A performance pad: hold to override the grid (or tap to toggle with Latch).
class PadButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    PadButton (StepGridEditor& e, int index) : ed (e), idx (index) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    StepGridEditor& ed;
    int idx;
};

class StepGridEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StepGridEditor (StepGridProcessor&);
    ~StepGridEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // used by GridView / PadButton
    StepGridProcessor& proc;
    const Theme& theme() const { return lnf.theme(); }
    sg::Fx paletteFx = sg::Fx::Stutter;
    int selectedStep = 0;
    sg::Pattern shown;                     // copy of the edit slot, refreshed by the timer
    void selectStep (int i);
    void setStepFx (int i, sg::Fx fx);
    void changeAmount (int i, float delta);
    void refreshNow() { timerCallback(); }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void applyTheme();
    void refreshPattern();
    void refreshStepEditor();
    void setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name);
    void selectPalette (sg::Fx f);

    StepGridLookAndFeel lnf;
    int lastTheme = -1;

    // header
    juce::ComboBox presetBox, themeBox;
    juce::ToggleButton syncButton { "HOST SYNC" };
    juce::Slider bpmSlider;
    std::unique_ptr<ButtonAttachment> syncAtt;
    std::unique_ptr<SliderAttachment> bpmAtt;

    // palette
    std::array<juce::TextButton, sg::kNumFx> paletteButtons;

    // pattern slots
    std::array<juce::TextButton, sg::kNumPatterns> slotButtons;
    juce::TextButton copyButton { "COPY" }, pasteButton { "PASTE" }, clearButton { "CLEAR" }, randButton { "RANDOM" };
    juce::ComboBox chainBox;
    std::unique_ptr<ComboAttachment> chainAtt;
    sg::Pattern clipboard;
    bool hasClipboard = false;

    // pattern settings
    juce::ComboBox rateBox;
    juce::Slider lengthSlider, swingSlider;
    juce::Label rateLabel, lengthLabel, swingLabel;

    GridView grid { *this };

    // step editor
    juce::ComboBox stepFxBox;
    juce::Slider amountKnob, probKnob, tieKnob;
    juce::Label amountLabel, probLabel, tieLabel;

    // perform
    std::vector<std::unique_ptr<PadButton>> pads;
    std::array<juce::ComboBox, sg::kNumPads> padFxBoxes;
    std::array<juce::Slider, sg::kNumPads> padAmtSliders;
    std::vector<std::unique_ptr<ComboAttachment>> padFxAtts;
    std::vector<std::unique_ptr<SliderAttachment>> padAmtAtts;
    juce::ToggleButton latchButton { "LATCH" };
    std::unique_ptr<ButtonAttachment> latchAtt;

    // master
    juce::Slider mixKnob, outKnob;
    juce::Label mixLabel, outLabel;
    std::unique_ptr<SliderAttachment> mixAtt, outAtt;

    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGridEditor)
};
