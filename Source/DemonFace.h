#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Fond de l'interface : visage de démon au trait rouge, yeux et sourire
// incandescents. Tout ce qui ne bouge qu'avec Fatness est mis en cache dans
// une image ; seules la lueur vacillante et les braises sont redessinées à
// chaque image d'animation.
class DemonFace final : public juce::Component {
public:
    DemonFace();

    void setFatness(float f);                       // 0..1, redessine le cache si besoin
    void setHeat(float grDb);                       // réduction de gain actuelle (dB)
    void setHeaderPanel(juce::Rectangle<float> r);  // cadre du logo (mesuré par l'éditeur)
    void setAuraCentre(juce::Point<float> c) { auraCentre = c; invalidate(); }
    void tick(double seconds);                      // avance l'animation

    void paint(juce::Graphics&) override;
    void resized() override { invalidate(); }

    int cacheRebuilds = 0;   // diagnostic (outil de capture)

private:
    void invalidate() { cacheValid = false; repaint(); }
    void renderStatic(juce::Graphics&, float f) const;
    void paintAnimated(juce::Graphics&) const;

    float fatness = 0.0f, heat = 0.0f;
    double t = 0.0;
    juce::Rectangle<float> headerPanel;
    juce::Point<float> auraCentre { 202.0f, 202.0f };

    juce::Image cache;
    juce::Image eyeSprite, throatSprite;   // halos pré-calculés, étirés et dosés à l'affichage
    float cacheFat = -1.0f, cacheScale = 0.0f;
    bool cacheValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DemonFace)
};
