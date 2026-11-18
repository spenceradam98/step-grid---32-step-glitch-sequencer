#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

// MIDI map (fixed, so a controller or deckrig can be mapped once):
//   notes 36-43 (C1-G1)  select pattern A-H (switches on the next step)
//   notes 48-55 (C2-G2)  perform pads 1-8 (hold, or toggle with Latch)
namespace midimap
{
constexpr int kPatternBase = 36;
constexpr int kPadBase = 48;
}

class StepGridProcessor : public juce::AudioProcessor
{
public:
    StepGridProcessor();
    ~StepGridProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // Programs = factory patterns, loaded into the selected slot.
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- pattern editing (message thread)
    sg::Pattern getPattern (int slot) const;
    void setPattern (int slot, const sg::Pattern& p);
    template <typename Fn> void editPattern (int slot, Fn&& fn)
    {
        auto p = getPattern (slot);
        fn (p);
        setPattern (slot, p);
    }
    int getEditSlot() const { return editSlot; }
    void setEditSlot (int s) { editSlot = juce::jlimit (0, sg::kNumPatterns - 1, s); }
    void selectPattern (int slot);            // plays it from the next step and makes it the edit slot
    void padDown (int pad);
    void padUp (int pad);
    void loadFactory (int index, int slot);

    const sg::Engine& getEngine() const { return engine; }

    int getTheme() const { return theme.load(); }
    void setTheme (int t) { theme.store (t); }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    sg::Params readParams() const;
    void pushUiEvent (sg::Event::Type type, int index);

    sg::Engine engine;
    std::array<sg::Pattern, sg::kNumPatterns> patterns {};
    juce::SpinLock patternLock;
    std::atomic<bool> patternsDirty { true };

    // UI -> audio events
    juce::AbstractFifo uiFifo { 64 };
    std::array<sg::Event, 64> uiEvents {};
    std::vector<sg::Event> blockEvents;

    int editSlot = 0;
    int currentProgram = 0;
    std::atomic<int> theme { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGridProcessor)
};
