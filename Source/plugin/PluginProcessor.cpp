#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Themes.h"

namespace
{
juce::String padId (int i, const char* what) { return "pad" + juce::String (i + 1) + what; }

juce::StringArray effectNames (bool includeOff)
{
    juce::StringArray s;
    for (int i = includeOff ? 0 : 1; i < sg::kNumFx; ++i) s.add (sg::fxName ((sg::Fx) i));
    return s;
}

const char* kDefaultSlots[sg::kNumPatterns] = { "Trap Fill", "Stutter Build", "Glitch Hop", "Tape-Stop Ending",
                                                "Breakdown Filter", "Reverse Turnaround", "DJ Echo Out", "Random Crunch" };
}

juce::AudioProcessorValueTreeState::ParameterLayout StepGridProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = [] (float v, int) { return String (v, 0) + "%"; };
    auto db = [] (float v, int) { return String (v, 1) + " dB"; };

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0, 100, 0.1f), 100.0f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24, 12, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Host Sync", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "Free BPM", NormalisableRange<float> (40, 240, 0.1f), 120.0f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " BPM"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "latch", 1 }, "Pad Latch", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "chain", 1 }, "Chain",
                                                         StringArray { "No Chain", "A-B", "A-C", "A-D", "A-E", "A-F", "A-G", "A-H" }, 0));
    const sg::Params defaults;
    for (int i = 0; i < sg::kNumPads; ++i)
    {
        const String n = "Pad " + String (i + 1) + " ";
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { padId (i, "fx"), 1 }, n + "Effect", effectNames (false),
                                                             (int) defaults.padFx[(size_t) i] - 1));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { padId (i, "amt"), 1 }, n + "Amount", NormalisableRange<float> (0, 100, 0.1f),
                                                            defaults.padAmount[(size_t) i] * 100.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    }
    return { p.begin(), p.end() };
}

StepGridProcessor::StepGridProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "StepGrid", createLayout())
{
    const auto& f = sg::factoryPatterns();
    for (int s = 0; s < sg::kNumPatterns; ++s)
        for (auto& fp : f)
            if (fp.pattern.name == kDefaultSlots[s]) patterns[(size_t) s] = fp.pattern;
    blockEvents.reserve (256);
}

bool StepGridProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const bool okOut = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return okOut && in == out;
}

void StepGridProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        for (int s = 0; s < sg::kNumPatterns; ++s) engine.setPattern (s, patterns[(size_t) s]);
        patternsDirty = false;
    }
    engine.prepare (sampleRate, samplesPerBlock);
}

sg::Params StepGridProcessor::readParams() const
{
    auto v = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    sg::Params p;
    p.mix = v ("mix") / 100.0f;
    p.outputDb = v ("output");
    p.sync = v ("sync") > 0.5f;
    p.bpm = v ("bpm");
    p.latch = v ("latch") > 0.5f;
    p.chainEnd = (int) v ("chain");
    for (int i = 0; i < sg::kNumPads; ++i)
    {
        p.padFx[(size_t) i] = (sg::Fx) ((int) v (padId (i, "fx")) + 1);
        p.padAmount[(size_t) i] = v (padId (i, "amt")) / 100.0f;
    }
    return p;
}

// ------------------------------------------------------------------ patterns (message thread)
sg::Pattern StepGridProcessor::getPattern (int slot) const
{
    const juce::SpinLock::ScopedLockType lock (patternLock);
    return patterns[(size_t) juce::jlimit (0, sg::kNumPatterns - 1, slot)];
}

void StepGridProcessor::setPattern (int slot, const sg::Pattern& p)
{
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        patterns[(size_t) juce::jlimit (0, sg::kNumPatterns - 1, slot)] = p;
    }
    patternsDirty = true;
}

void StepGridProcessor::pushUiEvent (sg::Event::Type type, int index)
{
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0) uiEvents[(size_t) scope.startIndex1] = { 0, type, index };
    else if (scope.blockSize2 > 0) uiEvents[(size_t) scope.startIndex2] = { 0, type, index };
}

void StepGridProcessor::selectPattern (int slot)
{
    setEditSlot (slot);
    pushUiEvent (sg::Event::SelectPattern, slot);
}

void StepGridProcessor::padDown (int pad) { pushUiEvent (sg::Event::PadOn, pad); }
void StepGridProcessor::padUp (int pad) { pushUiEvent (sg::Event::PadOff, pad); }

void StepGridProcessor::loadFactory (int index, int slot)
{
    const auto& f = sg::factoryPatterns();
    if (! juce::isPositiveAndBelow (index, (int) f.size())) return;
    setPattern (slot, f[(size_t) index].pattern);
}

// ------------------------------------------------------------------ audio
void StepGridProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    for (int c = inCh; c < outCh; ++c) buffer.clear (c, 0, n);

    if (patternsDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (patternLock);
        if (lock.isLocked())
        {
            for (int s = 0; s < sg::kNumPatterns; ++s) engine.setPattern (s, patterns[(size_t) s]);
            patternsDirty = false;
        }
    }
    engine.setParams (readParams());

    sg::Transport tr;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) { tr.hasInfo = true; tr.bpm = *bpm; }
            if (auto ppq = pos->getPpqPosition()) tr.ppq = *ppq; else tr.hasInfo = false;
            tr.playing = pos->getIsPlaying();
        }

    blockEvents.clear();
    {
        const auto scope = uiFifo.read (uiFifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) blockEvents.push_back (uiEvents[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) blockEvents.push_back (uiEvents[(size_t) (scope.startIndex2 + i)]);
    }
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int pos = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            if (note >= midimap::kPatternBase && note < midimap::kPatternBase + sg::kNumPatterns)
                blockEvents.push_back ({ pos, sg::Event::SelectPattern, note - midimap::kPatternBase });
            else if (note >= midimap::kPadBase && note < midimap::kPadBase + sg::kNumPads)
                blockEvents.push_back ({ pos, sg::Event::PadOn, note - midimap::kPadBase });
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            if (note >= midimap::kPadBase && note < midimap::kPadBase + sg::kNumPads)
                blockEvents.push_back ({ pos, sg::Event::PadOff, note - midimap::kPadBase });
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            blockEvents.push_back ({ pos, sg::Event::AllPadsOff, 0 });
    }
    std::stable_sort (blockEvents.begin(), blockEvents.end(), [] (const sg::Event& a, const sg::Event& b) { return a.sample < b.sample; });

    float* l = buffer.getWritePointer (0);
    float* r = outCh > 1 ? buffer.getWritePointer (1) : nullptr;
    if (r == nullptr)
    {
        // mono: run the stereo engine on a copy and fold back
        float tmp[1024];
        for (int pos = 0; pos < n; pos += 1024)
        {
            const int len = juce::jmin (1024, n - pos);
            std::copy (l + pos, l + pos + len, tmp);
            int first = 0;
            while (first < (int) blockEvents.size() && blockEvents[(size_t) first].sample < pos) ++first;
            int last = first;
            std::vector<sg::Event> evs;
            while (last < (int) blockEvents.size() && blockEvents[(size_t) last].sample < pos + len)
            {
                auto e = blockEvents[(size_t) last++];
                e.sample -= pos;
                evs.push_back (e);
            }
            sg::Transport t = tr;
            t.ppq += (double) pos * tr.bpm / (60.0 * getSampleRate());
            engine.process (l + pos, tmp, len, t, evs.data(), (int) evs.size());
            for (int i = 0; i < len; ++i) l[pos + i] = 0.5f * (l[pos + i] + tmp[i]);
        }
        return;
    }
    engine.process (l, r, n, tr, blockEvents.data(), (int) blockEvents.size());
}

// ------------------------------------------------------------------ programs and state
int StepGridProcessor::getNumPrograms() { return (int) sg::factoryPatterns().size(); }

const juce::String StepGridProcessor::getProgramName (int index)
{
    const auto& f = sg::factoryPatterns();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? juce::String (f[(size_t) index].pattern.name) : juce::String();
}

void StepGridProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    loadFactory (index, editSlot);
}

void StepGridProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("theme", themeAt (getTheme()).name, nullptr);
    state.setProperty ("editSlot", editSlot, nullptr);
    state.setProperty ("playing", engine.currentPattern(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    for (int s = 0; s < sg::kNumPatterns; ++s)
    {
        const auto p = getPattern (s);
        state.setProperty ("pat" + juce::String (s), juce::String (sg::serialize (p)), nullptr);
        state.setProperty ("patName" + juce::String (s), juce::String (p.name), nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void StepGridProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    setTheme (themeIndexByName (tree.getProperty ("theme", "Midnight").toString()));
    editSlot = juce::jlimit (0, sg::kNumPatterns - 1, (int) tree.getProperty ("editSlot", 0));
    currentProgram = tree.getProperty ("program", 0);
    for (int s = 0; s < sg::kNumPatterns; ++s)
    {
        const auto key = "pat" + juce::String (s);
        if (! tree.hasProperty (key)) continue;
        auto p = sg::deserialize (tree.getProperty (key).toString().toStdString());
        p.name = tree.getProperty ("patName" + juce::String (s), "").toString().toStdString();
        setPattern (s, p);
    }
    pushUiEvent (sg::Event::SelectPattern, juce::jlimit (0, sg::kNumPatterns - 1, (int) tree.getProperty ("playing", 0)));
    apvts.replaceState (tree);
}

juce::AudioProcessorEditor* StepGridProcessor::createEditor() { return new StepGridEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StepGridProcessor(); }
