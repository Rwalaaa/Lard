// snapshot_makar.cpp — ouvre l'interface de MAKAR hors DAW et l'enregistre en PNG.
// Usage : makar_snapshot out.png [id=valeur ...] [scale=2] [width=1200] [attitude=dB]
//   id = identifiants des paramètres (drive, punish, style, lowCut, thump, tone, highCut, steep,
//        mix, output, autoGain) ; valeurs dans leurs unités (style : 0..4).
#include <juce_gui_basics/juce_gui_basics.h>
#include "MakarProcessor.h"
#include "HudWidgets.h"

int main(int argc, char** argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: makar_snapshot out.png [id=valeur ...]\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI gui;
    MakarAudioProcessor proc;

    float scale = 1.0f;
    int width = 0;
    for (int i = 2; i < argc; ++i) {
        const juce::String a(argv[i]);
        const auto key = a.upToFirstOccurrenceOf("=", false, false);
        const float v = a.fromFirstOccurrenceOf("=", false, false).getFloatValue();
        if (key == "scale") { scale = v; continue; }
        if (key == "width") { width = (int) v; continue; }
        if (key == "attitude") { proc.attitudeDb.store(v); continue; }   // attaque simulée (dB) pour le vu-mètre
        auto* p = proc.apvts.getParameter(key);
        if (p == nullptr) { std::fprintf(stderr, "paramètre inconnu : %s\n", argv[i]); return 2; }
        p->setValueNotifyingHost(p->convertTo0to1(v));
    }

    // MAKAR_CURVE=1 : imprime la courbe des filtres affichée (dB) pour comparaison avec le moteur
    if (std::getenv("MAKAR_CURVE") != nullptr) {
        auto val = [&](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
        FilterCurve curve;
        curve.setSettings({ val(MakarIDs::lowCut), val(MakarIDs::tone), val(MakarIDs::highCut),
                            val(MakarIDs::thump) > 0.5f, val(MakarIDs::steep) > 0.5f });
        for (float f : { 30.f, 50.f, 80.f, 120.f, 200.f, 400.f, 800.f, 1500.f, 3000.f, 5000.f, 8000.f, 12000.f })
            std::printf("%g %.3f\n", (double) f, (double) curve.magnitudeDb(f));
        return 0;
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    if (width > 0)
        editor->setSize(width, juce::roundToInt(width * 470.0 / 800.0));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(400);

    if (const char* b = std::getenv("MAKAR_BENCH")) {
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
