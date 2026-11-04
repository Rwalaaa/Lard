#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Palette « Nacre » (maquette B)
namespace KoqColours {
    inline const juce::Colour bg      { 0xff071219 };
    inline const juce::Colour panel   { 0xff0C1D26 };
    inline const juce::Colour line    { 0xff24414D };
    inline const juce::Colour text    { 0xffE9F4F6 };
    inline const juce::Colour label   { 0xff9DB8C0 };
    inline const juce::Colour acc     { 0xff5FE3D3 };
    inline const juce::Colour hot     { 0xffD8FFF9 };
    inline const juce::Colour track   { 0xff16303A };
    inline const juce::Colour pearl   { 0xffF3E9FF };
    inline const juce::Colour glassLine { 0xff6E9AA6 };
    inline const juce::Colour base    { 0xff03090C };
    inline const juce::Colour meterBg { 0xff06161D };
    inline const juce::Colour danger  { 0xffC8402A };
}

// Réglages de dessin posés sur chaque composant via getProperties()
namespace KoqStyle {
    inline constexpr const char* size   = "koqSize";    // "big" | "mid" | "small"
    inline constexpr const char* centre = "koqCentre";  // arc depuis le centre (Input, Output)
    inline constexpr const char* plain  = "koqPlain";   // arc clair au lieu de turquoise (Output, Mix)
    inline constexpr const char* onBg   = "koqOnBg";    // disque couleur fond (panneau du bas)
    inline constexpr const char* seg    = "koqSeg";     // bouton segmenté
    inline constexpr const char* pill   = "koqPill";    // bouton Gain auto
}

class KoquilleLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    KoquilleLookAndFeel();

    juce::Font display(float h) const;   // Big Shoulders Display ExtraBold
    juce::Font mono(float h) const;      // IBM Plex Mono SemiBold
    juce::Font body(float h) const;      // IBM Plex Sans Condensed SemiBold

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                          float a0, float a1, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    static constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float endAngle   = juce::MathConstants<float>::pi * 2.75f;

private:
    juce::Typeface::Ptr displayFace, monoFace, bodyFace;
};
