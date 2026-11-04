#include "LardLookAndFeel.h"
#include "BinaryData.h"

using namespace juce;

LardLookAndFeel::LardLookAndFeel()
{
    displayFace = Typeface::createSystemTypefaceFor(BinaryData::DelaGothicOneRegularlatin_ttf,
                                                    (size_t) BinaryData::DelaGothicOneRegularlatin_ttfSize);
    bodyFace    = Typeface::createSystemTypefaceFor(BinaryData::ZenKakuGothicNewBoldlatin_ttf,
                                                    (size_t) BinaryData::ZenKakuGothicNewBoldlatin_ttfSize);
    setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
}

Font LardLookAndFeel::display(float height) const
{
    return Font(FontOptions(displayFace).withHeight(height));
}

Font LardLookAndFeel::body(float height) const
{
    return Font(FontOptions(bodyFace).withHeight(height));
}

// Lueur bon marché : le même trait repassé plus large et plus transparent.
static void strokeWithGlow(Graphics& g, const Path& p, Colour c, float width, float glow)
{
    if (glow > 0.0f) {
        g.setColour(c.withMultipliedAlpha(0.10f * glow));
        g.strokePath(p, PathStrokeType(width + 14.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(c.withMultipliedAlpha(0.22f * glow));
        g.strokePath(p, PathStrokeType(width + 7.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour(c);
    g.strokePath(p, PathStrokeType(width, PathStrokeType::curved, PathStrokeType::rounded));
}

void LardLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float a0, float a1, Slider& s)
{
    // Le dessin suit la maquette, faite dans un carré de 220 px.
    const float size = (float) jmin(width, height);
    const float k = size / 220.0f;
    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.5f;
    const bool isColor = s.getProperties()[LardStyle::key].toString() == LardStyle::color;
    const float angle = a0 + sliderPos * (a1 - a0);
    const float r = 92.0f * k, arcW = 12.0f * k;

    // piste
    Path track;
    track.addCentredArc(cx, cy, r, r, 0.0f, a0, a1, true);
    if (isColor) {
        ColourGradient grad(LardColours::deepRed, cx - r, cy + r, LardColours::amber, cx + r, cy - r, false);
        g.setGradientFill(grad);
        g.setOpacity(0.35f);
        g.strokePath(track, PathStrokeType(arcW, PathStrokeType::curved, PathStrokeType::rounded));
        g.setOpacity(1.0f);
    } else {
        g.setColour(LardColours::off);
        g.strokePath(track, PathStrokeType(arcW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // arc de valeur
    const float centre = (a0 + a1) * 0.5f;
    const float from = isColor ? jmin(centre, angle) : a0;
    const float to   = isColor ? jmax(centre, angle) : angle;
    if (to - from > 0.005f) {
        Path value;
        value.addCentredArc(cx, cy, r, r, 0.0f, from, to, true);
        if (isColor) {
            ColourGradient grad(LardColours::deepRed, cx - r, cy + r, LardColours::amber, cx + r, cy - r, false);
            g.setGradientFill(grad);
            g.strokePath(value, PathStrokeType(arcW, PathStrokeType::curved, PathStrokeType::rounded));
        } else {
            strokeWithGlow(g, value, LardColours::ember, arcW, sliderPos);
        }
    }

    // repère du neutre (Color)
    if (isColor) {
        g.setColour(LardColours::text);
        g.drawLine(cx, cy - 106.0f * k, cx, cy - 96.0f * k, 2.0f * k);
    }

    // disque
    const float dr = 70.0f * k;
    g.setColour(LardColours::panel);
    g.fillEllipse(cx - dr, cy - dr, dr * 2.0f, dr * 2.0f);
    g.setColour(LardColours::text);
    g.drawEllipse(cx - dr, cy - dr, dr * 2.0f, dr * 2.0f, 3.0f * k);

    if (!isColor) {
        // anneau pointillé
        Path ring;
        const float rr = 61.0f * k;
        ring.addEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);
        Path dashed;
        const float dashes[] = { 2.0f * k, 5.0f * k };
        PathStrokeType(1.0f * k).createDashedStroke(dashed, ring, dashes, 2);
        g.setColour(LardColours::ember.withAlpha(0.6f));
        g.fillPath(dashed);
    }

    // aiguille
    Path needle;
    needle.startNewSubPath(0.0f, -64.0f);
    needle.lineTo(7.0f, -36.0f);
    needle.lineTo(0.0f, -40.0f);
    needle.lineTo(-7.0f, -36.0f);
    needle.closeSubPath();
    g.setColour(Colours::white);
    g.fillPath(needle, AffineTransform::scale(k).rotated(angle).translated(cx, cy));

    // fissures d'énergie quand Fatness dépasse 65 %
    if (!isColor && sliderPos > 0.65f) {
        const float ox = cx - 110.0f * k, oy = cy - 110.0f * k;
        auto crack = [&](std::initializer_list<Point<float>> pts) {
            Path p;
            bool first = true;
            for (auto pt : pts) {
                const Point<float> q { ox + pt.x * k, oy + pt.y * k };
                if (first) { p.startNewSubPath(q); first = false; } else p.lineTo(q);
            }
            strokeWithGlow(g, p, LardColours::amber, 2.0f * k, 0.8f);
        };
        crack({ { 60, 40 }, { 74, 58 }, { 66, 62 }, { 84, 84 } });
        crack({ { 168, 34 }, { 156, 54 }, { 166, 58 }, { 146, 80 } });
        crack({ { 184, 150 }, { 160, 146 }, { 164, 158 }, { 140, 156 } });
    }
}

void LardLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float, float,
                                       Slider::SliderStyle, Slider&)
{
    // barre fine de la maquette : 6 px, piste violette, remplissage clair
    const float barH = 6.0f;
    const auto area = Rectangle<float>((float) x, (float) y + ((float) height - barH) * 0.5f,
                                       (float) width, barH);
    g.setColour(LardColours::off);
    g.fillRect(area);
    g.setColour(LardColours::text);
    g.fillRect(area.withRight(jlimit(area.getX(), area.getRight(), sliderPos)));
}
