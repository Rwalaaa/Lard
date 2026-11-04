// snapshot.cpp — ouvre l'interface du plugin hors DAW et l'enregistre en PNG.
// Usage : lard_snapshot out.png fatness color gainDb mix [reductionDb] [échelle]
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "DemonFace.h"

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
    // LARD_WIDTH=1080 : simule un redimensionnement de la fenêtre par l'utilisateur
    if (const char* w = std::getenv("LARD_WIDTH")) {
        const int width = std::atoi(w);
        editor->setSize(width, juce::roundToInt(width * 420.0 / 720.0));
    }
    // laisse tourner la boucle de messages : minuteries (vu-mètre, animations)
    juce::MessageManager::getInstance()->runDispatchLoopUntil(400);

    // LARD_BENCH=N : mesure le coût moyen d'une image complète de l'interface
    if (const char* b = std::getenv("LARD_BENCH")) {
        const int n = juce::jmax(1, std::atoi(b));
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < n; ++i)
            (void) editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
        const double ms = (juce::Time::getMillisecondCounterHiRes() - t0) / n;
        // pire cas : toute la fenêtre redessinée (l'animation n'en redessine qu'une partie)
        std::printf("bench: %.2f ms par image complète (x%.0f), soit %.1f %% d'un cœur à 25 i/s\n", ms, scale, ms * 25.0 / 10.0);

        // répartition : chaque enfant seul (0 = visage, puis potards et barres)
        auto timeIt = [&](const char* label) {
            const double s0 = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < n; ++i)
                (void) editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
            std::printf("  %-22s %.2f ms\n", label, (juce::Time::getMillisecondCounterHiRes() - s0) / n);
        };
        const int nc = editor->getNumChildComponents();
        for (int c = 0; c < nc; ++c) editor->getChildComponent(c)->setVisible(false);
        timeIt("textes seuls");
        editor->getChildComponent(0)->setVisible(true);
        auto* face = dynamic_cast<DemonFace*>(editor->getChildComponent(0));
        const int before = face != nullptr ? face->cacheRebuilds : -1;
        timeIt("textes + visage");
        if (face != nullptr)
            std::printf("  reconstructions du cache pendant la mesure : %d\n", face->cacheRebuilds - before);
        editor->getChildComponent(0)->setVisible(false);
        for (int c = 1; c < nc; ++c) editor->getChildComponent(c)->setVisible(true);
        timeIt("textes + potards");
        editor->getChildComponent(0)->setVisible(true);
    }

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
