// snapshot.cpp — ouvre l'interface du plugin hors DAW et l'enregistre en PNG.
// Usage : lard_snapshot out.png fatness color gainDb mix [reductionDb] [échelle]
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

int main(int argc, char** argv)
{
    if (argc < 6) {
        std::fprintf(stderr, "usage: lard_snapshot out.png fat color gainDb mix [grDb] [scale]\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI gui;

    LardAudioProcessor proc;
    auto set = [&](const char* id, float v) {
        if (auto* p = proc.apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    set(ParamIDs::fatness, (float) std::atof(argv[2]));
    set(ParamIDs::color,   (float) std::atof(argv[3]));
    set(ParamIDs::gain,    (float) std::atof(argv[4]));
    set(ParamIDs::mix,     (float) std::atof(argv[5]));
    proc.gainReductionDb.store(argc > 6 ? (float) std::atof(argv[6]) : 0.0f);
    const float scale = argc > 7 ? (float) std::atof(argv[7]) : 1.0f;

    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    // laisse tourner la boucle de messages : minuteries (vu-mètre, animations)
    juce::MessageManager::getInstance()->runDispatchLoopUntil(400);

    const auto img = editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    out.deleteFile();
    juce::FileOutputStream os(out);
    if (!os.openedOk() || !juce::PNGImageFormat().writeImageToStream(img, os)) {
        std::fprintf(stderr, "écriture PNG impossible\n");
        return 1;
    }
    std::printf("%s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    editor.reset();
    return 0;
}
