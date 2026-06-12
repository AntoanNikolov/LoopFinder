// =============================================================================
// SnapshotTool.cpp
//
// Developer utility: renders the LoopFinder editor offscreen and writes a PNG.
// Optionally loads an audio file and runs the analysis first, so the full
// working UI (key badge, regions, list) can be inspected without a DAW.
//
//   LoopFinderSnapshot <output.png> [input-audio-file]
// =============================================================================

#include <JuceHeader.h>

#include "PluginProcessor.h"

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "usage: LoopFinderSnapshot <output.png> [input-audio-file]\n";
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pump = [] (int ms)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    };

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    auto* lfProc = dynamic_cast<lf::LoopFinderProcessor*> (proc.get());
    if (lfProc == nullptr)
        return 1;

    lfProc->prepareToPlay (44100.0, 512);

    if (argc > 2)
    {
        const juce::File audio (juce::File::getCurrentWorkingDirectory()
                                    .getChildFile (juce::String (argv[2])));
        if (! lfProc->loadFile (audio))
            std::cout << "warning: could not load " << argv[2] << "\n";

        pump (600);                       // let pitch detection finish
        lfProc->startAnalysis();
        for (int i = 0; i < 100 && lfProc->isAnalysing(); ++i)
            pump (100);
        pump (300);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (lfProc->createEditor());
    editor->setTopLeftPosition (0, 0);
    pump (400);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(),
                                                        true, 2.0f);

    const juce::File out (juce::File::getCurrentWorkingDirectory()
                              .getChildFile (juce::String (argv[1])));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    if (! stream.openedOk())
        return 1;

    juce::PNGImageFormat png;
    const bool ok = png.writeImageToStream (image, stream);
    std::cout << (ok ? "wrote " : "FAILED ") << out.getFullPathName() << "\n";

    editor.reset();
    proc.reset();
    return ok ? 0 : 1;
}
