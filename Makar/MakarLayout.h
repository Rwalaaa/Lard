#pragma once
#include <juce_graphics/juce_graphics.h>

// Disposition de la maquette A « Néon HUD » (repère 800 x 470), partagée par le fond et l'éditeur.
namespace MkLayout {
    using R = juce::Rectangle<int>;
    inline constexpr int W = 800, H = 470;

    // en-tête
    inline const R autoBtn   { 426, 18, 120, 44 };
    inline const R meter     { 562, 21, 214, 38 };   // libellé (13) + 5 + 24 segments de 7 x 20

    // colonne gauche : circuits
    inline constexpr int circuitX = 24, circuitY = 125, circuitW = 180, circuitH = 44, circuitStep = 52;
    inline R circuit(int i) { return { circuitX, circuitY + i * circuitStep, circuitW, circuitH }; }

    // centre
    inline const R drive     { 252, 92, 240, 240 };
    inline const R punish    { 302, 346, 140, 44 };

    // droite : filtres
    inline const R curve     { 540, 127, 236, 104 };
    inline const R knobCol[3] { { 540, 243, 72, 87 }, { 622, 243, 72, 87 }, { 704, 243, 72, 87 } };
    inline R knob(int i) { return { knobCol[i].getX() + 8, knobCol[i].getY(), 56, 56 }; }
    inline const R thump     { 540, 340, 114, 32 };
    inline const R steep     { 662, 340, 114, 32 };

    // barre du bas
    inline const R bar       { 16, 410, 768, 48 };
    inline const R mixBar    { 104, 422, 180, 24 };   // zone cliquable ; la barre fait 6 px de haut au centre
    inline const R outBar    { 502, 422, 180, 24 };
}
