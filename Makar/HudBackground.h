#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class MakarLookAndFeel;

// Fond « Néon HUD » : grille, lueurs, logo à décalage chromatique, libellés fixes, crochets
// autour du Drive, barre du bas, lignes de balayage. Image mise en cache à l'échelle réelle ;
// par-dessus, les halos du circuit actif et de Punish (seuls éléments qui changent).
class HudBackground final : public juce::Component {
public:
    explicit HudBackground(MakarLookAndFeel&);

    void setHalos(int circuit, bool punish);
    void paint(juce::Graphics&) override;
    void resized() override { cache = {}; }

    int cacheRebuilds = 0;   // diagnostic (outil de capture)

private:
    void renderStatic(juce::Graphics&) const;
    MakarLookAndFeel& laf;
    juce::Image cache;
    float cacheScale = 0.0f;
    int circuit = -1;
    bool punish = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HudBackground)
};
