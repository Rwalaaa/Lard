#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Fond de l'interface « Nacre » : halo océan, nautile qui s'illumine avec Density,
// lampes pentode/triode dont le filament suit leur gain, indicateur à aiguille,
// panneau du bas. Dessiné dans le repère de la maquette (760 x 460).
class NacreBackground final : public juce::Component {
public:
    NacreBackground();

    struct State {
        float density = 0.0f;     // 0..1
        float pentDrive = 0.0f;   // 0..1 : lueur de la pentode
        float triDrive = 0.0f;    // 0..1 : lueur de la triode
        float colour = 0.0f;      // 0..1 : aiguille « couleur »
    };
    void setState(const State& s);
    const State& getState() const { return state; }

    void paint(juce::Graphics&) override;

private:
    void paintSpiral(juce::Graphics&) const;
    void paintTube(juce::Graphics&, juce::Rectangle<int> r, float drive, bool pentode) const;
    void paintMeter(juce::Graphics&) const;

    State state;
    juce::Path spiral;
    std::vector<juce::Line<float>> chambers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NacreBackground)
};
