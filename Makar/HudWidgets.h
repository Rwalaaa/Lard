#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class MakarLookAndFeel;

// Courbe de réponse des filtres (Low Cut, Thump, Tone, High Cut, Steep), calculée avec les
// mêmes filtres que le moteur. 20 Hz - 20 kHz en log, +12 / -24 dB. Repère 236 x 104.
class FilterCurve final : public juce::Component {
public:
    FilterCurve();
    struct Settings {
        float lowCutHz = 20.0f, tone = 0.0f, highCutHz = 20000.0f;
        bool thump = false, steep = false;
        bool operator==(const Settings& o) const {
            return lowCutHz == o.lowCutHz && tone == o.tone && highCutHz == o.highCutHz
                && thump == o.thump && steep == o.steep;
        }
    };
    void setSettings(const Settings&);   // ne recalcule et ne redessine que si quelque chose change
    float magnitudeDb(float hz) const;   // réponse à une fréquence (aussi utilisée par les tests)
    void paint(juce::Graphics&) override;

private:
    void rebuild();
    Settings s;
    bool built = false;
    juce::Path line, fill;
    struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    std::vector<Coefs> stages;
    double hcPole = 0.0;   // pôle du coupe-haut 1er ordre (0 = coupé)
};

// Vu-mètre « Attitude » : niveau d'attaque du circuit, 24 segments + valeur en dB. Repère 214 x 38
// (plus une marge de 8 px tout autour pour les halos).
class AttitudeMeter final : public juce::Component {
public:
    explicit AttitudeMeter(MakarLookAndFeel&);
    static constexpr int margin = 8, segments = 24;
    void setDb(float db);                // ne redessine que si l'affichage change
    static int litFor(float db);         // segments allumés pour un niveau donné
    void paint(juce::Graphics&) override;

private:
    MakarLookAndFeel& laf;
    int lit = -1;
    juce::String text;
};
