// snapshot_koquille.cpp — ouvre l'interface de Koquille hors DAW et l'enregistre en PNG.
// Usage : koquille_snapshot out.png [id=valeur ...] [scale=2] [width=1140]
//   id = identifiants des paramètres (input, pentode, triode, density, satAmount, satType,
//        satBand, calibration, air, mix, output, autoGain) ; valeurs dans leurs unités.
#include <juce_gui_basics/juce_gui_basics.h>
#include "KoquilleProcessor.h"

int main(int argc, char** argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: koquille_snapshot out.png [id=valeur ...]\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI gui;
    KoquilleAudioProcessor proc;

    float scale = 1.0f;
    int width = 0;
    for (int i = 2; i < argc; ++i) {
        const juce::String a(argv[i]);
        const auto key = a.upToFirstOccurrenceOf("=", false, false);
        const float v = a.fromFirstOccurrenceOf("=", false, false).getFloatValue();
        if (key == "scale") { scale = v; continue; }
        if (key == "width") { width = (int) v; continue; }
        auto* p = proc.apvts.getParameter(key);
        if (p == nullptr) { std::fprintf(stderr, "paramètre inconnu : %s\n", argv[i]); return 2; }
        p->setValueNotifyingHost(p->convertTo0to1(v));
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    if (width > 0)
        editor->setSize(width, juce::roundToInt(width * 460.0 / 760.0));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(400);

    if (const char* b = std::getenv("KOQ_BENCH")) {
        const int n = juce::jmax(1, std::atoi(b));
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < n; ++i)
            (void) editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
        const double ms = (juce::Time::getMillisecondCounterHiRes() - t0) / n;
        std::printf("bench: %.2f ms par image complète (x%.0f)\n", ms, scale);
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
