#include "KoquilleLookAndFeel.h"
#include "KoqBinaryData.h"

using namespace juce;

KoquilleLookAndFeel::KoquilleLookAndFeel()
{
    displayFace = Typeface::createSystemTypefaceFor(KoqBinary::BigShouldersDisplayExtraBoldlatin_ttf,
                                                    (size_t) KoqBinary::BigShouldersDisplayExtraBoldlatin_ttfSize);
    monoFace    = Typeface::createSystemTypefaceFor(KoqBinary::IBMPlexMonoSemiBold_ttf,
                                                    (size_t) KoqBinary::IBMPlexMonoSemiBold_ttfSize);
    bodyFace    = Typeface::createSystemTypefaceFor(KoqBinary::IBMPlexSansCondensedSemiBold_ttf,
                                                    (size_t) KoqBinary::IBMPlexSansCondensedSemiBold_ttfSize);
}

Font KoquilleLookAndFeel::display(float h) const { return Font(FontOptions(displayFace).withHeight(h)); }
Font KoquilleLookAndFeel::mono(float h) const    { return Font(FontOptions(monoFace).withHeight(h)); }
Font KoquilleLookAndFeel::body(float h) const    { return Font(FontOptions(bodyFace).withHeight(h)); }

void KoquilleLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos,
                                           float a0, float a1, Slider& s)
{
    // dessin de la maquette, dans un carré de 100 unités
    const auto& props = s.getProperties();
    const String size = props[KoqStyle::size].toString();
    const bool big = size == "big", small = size == "small";
    const bool centre = (bool) props[KoqStyle::centre];
    const bool plain = (bool) props[KoqStyle::plain];
    const bool onBg = (bool) props[KoqStyle::onBg];

    const float side = (float) jmin(w, h);
    const float k = side / 100.0f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float r = (big ? 44.0f : small ? 40.0f : 42.0f) * k;
    const float stroke = (big ? 5.0f : small ? 8.0f : 6.0f) * k;
    const float diskR = (big ? 34.0f : small ? 30.0f : 32.0f) * k;
    const float angle = a0 + pos * (a1 - a0);
    const Colour valueColour = plain ? KoqColours::text : KoqColours::acc;
    const PathStrokeType st(stroke, PathStrokeType::curved, PathStrokeType::rounded);

    Path track;
    track.addCentredArc(cx, cy, r, r, 0.0f, a0, a1, true);
    g.setColour(KoqColours::track);
    g.strokePath(track, st);

    const float mid = (a0 + a1) * 0.5f;
    const float from = centre ? jmin(mid, angle) : a0, to = centre ? jmax(mid, angle) : angle;
    if (to - from > 0.005f) {
        Path value;
        value.addCentredArc(cx, cy, r, r, 0.0f, from, to, true);
        if (big) {   // halo du gros potard
            g.setColour(valueColour.withAlpha(0.18f));
            g.strokePath(value, PathStrokeType(stroke * 2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour(valueColour);
        g.strokePath(value, st);
    }

    g.setColour(onBg ? KoqColours::bg : KoqColours::panel);
    g.fillEllipse(cx - diskR, cy - diskR, diskR * 2.0f, diskR * 2.0f);
    g.setColour(KoqColours::text);
    g.drawEllipse(cx - diskR, cy - diskR, diskR * 2.0f, diskR * 2.0f, (big ? 1.5f : small ? 3.0f : 2.0f) * k);
    if (big) {
        const float rr = 29.0f * k;
        g.setColour(KoqColours::line);
        g.drawEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 0.8f * k);
    }

    // aiguille : trait radial dans le disque
    const float rOut = (big ? 31.0f : small ? 26.0f : 28.0f) * k;
    const float rIn  = (big ? 19.0f : small ? 14.0f : 16.0f) * k;
    const float sx = std::sin(angle), sy = -std::cos(angle);
    g.setColour(KoqColours::text);
    g.drawLine(cx + sx * rIn, cy + sy * rIn, cx + sx * rOut, cy + sy * rOut,
               (big ? 2.5f : small ? 4.0f : 3.0f) * k);
}

void KoquilleLookAndFeel::drawButtonBackground(Graphics& g, Button& b, const Colour&, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced(0.75f);
    const bool on = b.getToggleState();
    const bool pill = (bool) b.getProperties()[KoqStyle::pill];
    const float radius = pill ? r.getHeight() * 0.5f : 5.0f;
    if (on) {
        g.setColour(KoqColours::acc);
        g.fillRoundedRectangle(r, radius);
    } else if (highlighted) {
        g.setColour(KoqColours::acc.withAlpha(0.08f));
        g.fillRoundedRectangle(r, radius);
    }
    g.setColour(on ? KoqColours::acc : KoqColours::line);
    g.drawRoundedRectangle(r, radius, pill ? 1.5f : 1.0f);
}

void KoquilleLookAndFeel::drawButtonText(Graphics& g, TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const bool pill = (bool) b.getProperties()[KoqStyle::pill];
    Font f = body(pill ? 13.0f : 12.0f);
    if (pill) f.setExtraKerningFactor(0.11f);
    g.setFont(f);
    g.setColour(on ? KoqColours::bg : KoqColours::label);
    const String txt = pill ? String::fromUTF8("GAIN AUTO \xc2\xb7 ") + (on ? "ON" : "OFF")
                            : b.getButtonText();
    g.drawText(txt, b.getLocalBounds(), Justification::centred, false);
}
