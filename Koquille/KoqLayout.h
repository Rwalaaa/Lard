#pragma once
#include <juce_graphics/juce_graphics.h>

// Disposition de la maquette « Nacre » (repère 760 x 460), partagée par le fond et l'éditeur.
namespace KoqLayout {
    using R = juce::Rectangle<int>;
    inline constexpr int W = 760, H = 460;

    inline const R input      { 30, 118, 100, 100 };
    inline const R output     { 630, 118, 100, 100 };
    inline const R density    { 290, 98, 180, 180 };
    inline const R pentTube   { 168, 92, 88, 128 };
    inline const R triTube    { 504, 92, 88, 128 };
    inline const R pentKnob   { 181, 222, 62, 62 };
    inline const R triKnob    { 517, 222, 62, 62 };
    inline const R vu         { 578, 14, 150, 64 };
    inline const R autoGain   { 410, 24, 150, 44 };
    inline const R panel      { 16, 342, 728, 104 };

    // panneau du bas
    inline const R satKnob    { 34, 352, 58, 58 };
    inline const R types[2]   { { 104, 374, 52, 26 }, { 160, 374, 76, 26 } };
    inline const R bands[4]   { { 104, 406, 52, 26 }, { 160, 406, 66, 26 }, { 230, 406, 60, 26 }, { 294, 406, 48, 26 } };
    inline const R cals[3]    { { 370, 392, 58, 26 }, { 432, 392, 58, 26 }, { 494, 392, 62, 26 } };
    inline const R airKnob    { 586, 352, 58, 58 };
    inline const R mixKnob    { 664, 352, 58, 58 };
    inline constexpr int divider1 = 356, divider2 = 570;
}
