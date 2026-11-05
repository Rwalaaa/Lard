#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Palette « Néon HUD » (maquette A)
namespace MkColours {
    inline const juce::Colour bg    { 0xff07060D };
    inline const juce::Colour panel { 0xff0F0C1C };
    inline const juce::Colour line  { 0xff2C2548 };
    inline const juce::Colour text  { 0xffEFEBFF };
    inline const juce::Colour label { 0xffA8A0CC };
    inline const juce::Colour acc   { 0xffFF2E88 };   // magenta
    inline const juce::Colour acc2  { 0xff22E6FF };   // cyan
    inline const juce::Colour warn  { 0xffFFD23F };   // Punish
}

// Réglages de dessin posés sur chaque composant via getProperties()
namespace MkStyle {
    inline constexpr const char* kind   = "mkKind";    // potards : "drive" | "small" ; boutons : "circuit" | "toggle" | "auto" | "punish"
    inline constexpr const char* centre = "mkCentre";  // petit potard : arc depuis le centre (Tone)
    inline constexpr const char* code   = "mkCode";    // bouton circuit : « 01 » ...
    inline constexpr const char* hot    = "mkHot";     // couronne Drive : Punish enclenché
}

class MakarLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    MakarLookAndFeel();

    juce::Font display(float h) const;   // Makar Display (Orbitron modifiée, graisse 900)
    juce::Font mono(float h) const;      // JetBrains Mono Bold

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                          float a0, float a1, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float, float,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    int getSliderThumbRadius(juce::Slider&) override { return 0; }   // barres Mix/Output : toute la largeur

    static constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float endAngle   = juce::MathConstants<float>::pi * 2.75f;

    // trait avec halo néon : passes larges et transparentes, puis trait net
    static void neonStroke(juce::Graphics&, const juce::Path&, juce::Colour, float width, float glow = 1.0f);
    static juce::Path bevel(juce::Rectangle<float> r, float cut);

private:
    juce::Typeface::Ptr displayFace, monoFace;
};
