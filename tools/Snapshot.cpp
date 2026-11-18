// Renders the editor to PNG for docs/ (developer tool). Usage: StepGridSnapshot <out.png> [theme index]
#include "../Source/plugin/PluginProcessor.h"
#include "../Source/plugin/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "screenshot.png"));
    StepGridProcessor proc;
    proc.setTheme (argc > 2 ? juce::String (argv[2]).getIntValue() : 0);
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // run the free clock to step 11 of pattern A with a few samples of audio
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < 6000 * 11 / 512 + 1; ++b)
    {
        for (int i = 0; i < 512; ++i) buf.setSample (0, i, 0.2f * std::sin ((float) (b * 512 + i) * 0.03f)), buf.setSample (1, i, buf.getSample (0, i));
        proc.processBlock (buf, midi);
    }
    proc.padDown (2);   // show a held pad
    proc.processBlock (buf, midi);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (auto* e = dynamic_cast<StepGridEditor*> (ed.get())) { e->selectStep (12); e->refreshNow(); }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
