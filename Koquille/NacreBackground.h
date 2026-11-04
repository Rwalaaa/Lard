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
    };
    void setState(const State& s);
    const State& getState() const { return state; }

    // animation : aiguille (0..1, couleur mesurée) et temps (s) ; ne redessine que ce qui bouge
    void setDynamics(float needle, double seconds);

    void paint(juce::Graphics&) override;
    void resized() override { staticCache = {}; spiralCache = {}; }

    int cacheRebuilds = 0;   // diagnostic (outil de capture)

private:
    void renderStatic(juce::Graphics&) const;
    void renderSpiral(juce::Graphics&, float density) const;
    void paintTube(juce::Graphics&, juce::Rectangle<int> r, float drive, bool pentode) const;
    void paintNeedle(juce::Graphics&) const;
    void repaintDesign(juce::Rectangle<int> designArea);

    State state;
    float needle = 0.0f;
    double t = 0.0;
    juce::Path spiral;
    std::vector<juce::Line<float>> chambers;
    juce::Rectangle<int> spiralArea;   // zone de la spirale (repère maquette)

    juce::Image staticCache, spiralCache;
    float cacheScale = 0.0f, spiralDensity = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NacreBackground)
};
