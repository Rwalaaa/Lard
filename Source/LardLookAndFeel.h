#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace LardColours {
    inline const juce::Colour bg       { 0xff0D0618 };
    inline const juce::Colour panel    { 0xff160A24 };
    inline const juce::Colour bar      { 0xff12081E };
    inline const juce::Colour off      { 0xff2A1638 };
    inline const juce::Colour ember    { 0xffFF6A3D };
    inline const juce::Colour fire     { 0xffFF3A1F };
    inline const juce::Colour red      { 0xffFF2A2A };
    inline const juce::Colour amber    { 0xffFFC24A };
    inline const juce::Colour hot      { 0xffFFD25C };
    inline const juce::Colour deepRed  { 0xff9A1E2E };
    inline const juce::Colour text     { 0xffF4E9FF };
    inline const juce::Colour label    { 0xffC9B6DF };
    inline const juce::Colour fatValue { 0xffFF9A6A };
    inline const juce::Colour rank     { 0xffFF8A4A };
}

// Styles de potard, posés sur le Slider via getProperties()["lardStyle"]
namespace LardStyle {
    inline constexpr const char* key   = "lardStyle";
    inline constexpr const char* fat   = "fat";     // arc depuis la gauche, braise
    inline constexpr const char* color = "color";   // arc depuis le centre, rouge -> ambre
}

class LardLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LardLookAndFeel();

    juce::Font display(float height) const;   // Dela Gothic One
    juce::Font body(float height) const;      // Zen Kaku Gothic New Bold

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    // angles des potards (radians, sens horaire depuis midi) : -135° .. +135°
    static constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float endAngle   = juce::MathConstants<float>::pi * 2.75f;

private:
    juce::Typeface::Ptr displayFace, bodyFace;
};
