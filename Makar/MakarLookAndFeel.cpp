#include "MakarLookAndFeel.h"
#include "MakarBinaryData.h"

using namespace juce;

MakarLookAndFeel::MakarLookAndFeel()
{
    displayFace = Typeface::createSystemTypefaceFor(MakarBinary::MakarDisplayBlack_ttf,
                                                    (size_t) MakarBinary::MakarDisplayBlack_ttfSize);
    monoFace    = Typeface::createSystemTypefaceFor(MakarBinary::JetBrainsMonoBoldlatin_ttf,
                                                    (size_t) MakarBinary::JetBrainsMonoBoldlatin_ttfSize);
}

Font MakarLookAndFeel::display(float h) const { return Font(FontOptions(displayFace).withHeight(h)); }
Font MakarLookAndFeel::mono(float h) const    { return Font(FontOptions(monoFace).withHeight(h)); }

void MakarLookAndFeel::neonStroke(Graphics& g, const Path& p, Colour c, float width, float glow)
{
    if (glow > 0.0f) {
        g.setColour(c.withMultipliedAlpha(0.12f * glow));
        g.strokePath(p, PathStrokeType(width + 8.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(c.withMultipliedAlpha(0.25f * glow));
        g.strokePath(p, PathStrokeType(width + 3.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour(c);
    g.strokePath(p, PathStrokeType(width, PathStrokeType::curved, PathStrokeType::rounded));
}

Path MakarLookAndFeel::bevel(Rectangle<float> r, float cut)
{
    Path p;
    p.startNewSubPath(r.getX() + cut, r.getY());
    p.lineTo(r.getRight(), r.getY());
    p.lineTo(r.getRight(), r.getBottom() - cut);
    p.lineTo(r.getRight() - cut, r.getBottom());
    p.lineTo(r.getX(), r.getBottom());
    p.lineTo(r.getX(), r.getY() + cut);
    p.closeSubPath();
    return p;
}

void MakarLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos,
                                        float a0, float a1, Slider& s)
{
    const auto& props = s.getProperties();
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float angle = a0 + pos * (a1 - a0);

    if (props[MkStyle::kind].toString() == "drive") {
        // couronne de 41 segments, dégradé cyan -> magenta (-> jaune avec Punish), repère 240 unités
        const float k = (float) jmin(w, h) / 240.0f;
        const bool hot = (bool) props[MkStyle::hot];
        const Colour end = hot ? MkColours::warn : MkColours::acc;
        for (int i = 0; i <= 40; ++i) {
            const float t = (float) i / 40.0f;
            const float a = a0 + t * (a1 - a0);
            const bool on = t <= pos + 1.0e-4f;
            Path seg;
            seg.startNewSubPath(cx + 84.0f * k * std::sin(a), cy - 84.0f * k * std::cos(a));
            seg.lineTo(cx + 102.0f * k * std::sin(a), cy - 102.0f * k * std::cos(a));
            if (on) {
                const Colour c = MkColours::acc2.interpolatedWith(end, t);
                g.setColour(c.withAlpha(0.22f));
                g.strokePath(seg, PathStrokeType(11.0f * k));
                g.setColour(c);
            } else {
                g.setColour(MkColours::line);
            }
            g.strokePath(seg, PathStrokeType(5.0f * k));
        }
        const float dr = 70.0f * k;
        g.setColour(MkColours::panel);
        g.fillEllipse(cx - dr, cy - dr, dr * 2.0f, dr * 2.0f);
        g.setColour(MkColours::line);
        g.drawEllipse(cx - dr, cy - dr, dr * 2.0f, dr * 2.0f, 2.0f * k);

        Path ring;
        const float rr = 62.0f * k;
        ring.addEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);
        Path dashed;
        const float dashes[] = { 3.0f * k, 6.0f * k };
        PathStrokeType(1.0f * k).createDashedStroke(dashed, ring, dashes, 2);
        g.setColour(MkColours::acc.withAlpha(0.25f + 0.6f * pos));
        g.fillPath(dashed);

        // aiguille lumineuse
        Path needle;
        needle.addRectangle(-3.0f, -66.0f, 6.0f, 26.0f);
        needle.applyTransform(AffineTransform::scale(k).rotated(angle).translated(cx, cy));
        g.setColour(MkColours::text.withAlpha(0.25f));
        g.strokePath(needle, PathStrokeType(5.0f * k));
        g.setColour(MkColours::text);
        g.fillPath(needle);

        g.setFont(display(26.0f * k));
        g.setColour(MkColours::text);
        g.drawText(String(pos * 10.0f, 1), Rectangle<float>(cx - 60.0f * k, cy - 24.0f * k, 120.0f * k, 34.0f * k),
                   Justification::centred, false);
        Font lab = mono(10.0f * k);
        lab.setExtraKerningFactor(0.3f);
        g.setFont(lab);
        g.setColour(MkColours::label);
        g.drawText("DRIVE", Rectangle<float>(cx - 60.0f * k, cy + 18.0f * k, 120.0f * k, 14.0f * k),
                   Justification::centred, false);
        return;
    }

    // petit potard (repère 100 unités) : piste, arc cyan, disque, aiguille
    const float k = (float) jmin(w, h) / 100.0f;
    const bool centre = (bool) props[MkStyle::centre];
    const float r = 40.0f * k;
    Path track;
    track.addCentredArc(cx, cy, r, r, 0.0f, a0, a1, true);
    g.setColour(MkColours::line);
    g.strokePath(track, PathStrokeType(9.0f * k));
    const float mid = (a0 + a1) * 0.5f;
    const float from = centre ? jmin(mid, angle) : a0, to = centre ? jmax(mid, angle) : angle;
    if (to - from > 0.005f) {
        Path value;
        value.addCentredArc(cx, cy, r, r, 0.0f, from, to, true);
        g.setColour(MkColours::acc2);
        g.strokePath(value, PathStrokeType(9.0f * k));
    }
    const float dr = 29.0f * k;
    g.setColour(MkColours::panel);
    g.fillEllipse(cx - dr, cy - dr, dr * 2.0f, dr * 2.0f);
    g.setColour(MkColours::text);
    g.drawEllipse(cx - dr, cy - dr, dr * 2.0f, dr * 2.0f, 2.5f * k);
    const float sx = std::sin(angle), sy = -std::cos(angle);
    g.drawLine(cx + sx * 12.0f * k, cy + sy * 12.0f * k, cx + sx * 24.0f * k, cy + sy * 24.0f * k, 4.0f * k);
}

void MakarLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        Slider::SliderStyle, Slider&)
{
    const auto bar = Rectangle<float>((float) x, (float) y + ((float) h - 6.0f) * 0.5f, (float) w, 6.0f);
    g.setColour(MkColours::line);
    g.fillRect(bar);
    const auto fill = bar.withRight(jlimit(bar.getX(), bar.getRight(), pos));
    if (fill.getWidth() > 0.5f)   // halo néon (box-shadow 0 0 8px de la maquette)
        DropShadow(MkColours::acc.withAlpha(0.8f), 8, {}).drawForRectangle(g, fill.toNearestInt());
    g.setColour(MkColours::acc);
    g.fillRect(fill);
}

void MakarLookAndFeel::drawButtonBackground(Graphics& g, Button& b, const Colour&, bool highlighted, bool)
{
    const String kind = b.getProperties()[MkStyle::kind].toString();
    const bool on = b.getToggleState();
    const auto r = b.getLocalBounds().toFloat().reduced(1.0f);

    if (kind == "punish") {
        const Path shape = bevel(r, 0.0f);
        g.setColour(MkColours::panel);
        g.fillPath(shape);
        if (on) {
            // bande de danger rayée jaune/noir
            Graphics::ScopedSaveState s(g);
            g.reduceClipRegion(shape);
            g.fillAll(MkColours::bg);
            g.setColour(MkColours::warn);
            for (float xs = r.getX() - r.getHeight(); xs < r.getRight(); xs += 16.0f) {
                Path stripe;
                stripe.addQuadrilateral(xs, r.getBottom(), xs + 8.0f, r.getBottom(),
                                        xs + 8.0f + r.getHeight(), r.getY(), xs + r.getHeight(), r.getY());
                g.fillPath(stripe);
            }
        }
        g.setColour(on ? MkColours::warn : (highlighted ? MkColours::label : MkColours::line));
        g.drawRect(r, 2.0f);
        return;
    }

    const bool cyan = kind == "toggle" || kind == "auto";
    const Colour onColour = cyan ? MkColours::acc2 : MkColours::acc;
    const Path shape = kind == "auto" ? bevel(r, 8.0f) : bevel(r, 0.0f);
    if (on) {   // le halo du circuit actif est peint par le fond (il déborde du bouton)
        g.setColour(onColour);
        g.fillPath(shape);
    } else if (highlighted) {
        g.setColour(onColour.withAlpha(0.08f));
        g.fillPath(shape);
    }
    g.setColour(on ? onColour : MkColours::line);
    g.strokePath(shape, PathStrokeType(kind == "auto" ? 1.5f : 1.0f));
}

void MakarLookAndFeel::drawButtonText(Graphics& g, TextButton& b, bool, bool)
{
    const String kind = b.getProperties()[MkStyle::kind].toString();
    const bool on = b.getToggleState();
    const auto r = b.getLocalBounds();

    if (kind == "punish") {
        Font f = display(15.0f);
        f.setExtraKerningFactor(0.2f);
        g.setFont(f);
        if (on) {   // texte lisible sur les rayures
            g.setColour(MkColours::bg);
            g.fillRect(r.withSizeKeepingCentre(118, 22));
        }
        g.setColour(on ? MkColours::text : MkColours::label);
        g.drawText("PUNISH", r, Justification::centred, false);
        return;
    }

    Font f = mono(13.0f);
    f.setExtraKerningFactor(kind == "auto" ? 0.15f : 0.12f);
    const Colour fg = on ? MkColours::bg : (kind == "toggle" ? MkColours::label : MkColours::text);
    if (kind == "circuit") {
        g.setFont(mono(10.0f));
        g.setColour(fg.withMultipliedAlpha(0.75f));
        g.drawText(b.getProperties()[MkStyle::code].toString(), r.withTrimmedLeft(12).withWidth(24),
                   Justification::centredLeft, false);
        g.setFont(f);
        g.setColour(fg);
        g.drawText(b.getButtonText(), r.withTrimmedLeft(40), Justification::centredLeft, false);
        return;
    }
    if (kind == "toggle") f = mono(12.0f), f.setExtraKerningFactor(0.17f);
    g.setFont(f);
    g.setColour(fg);
    const String txt = kind == "auto" ? String::fromUTF8("AUTO \xc2\xb7 ") + (on ? "ON" : "OFF") : b.getButtonText();
    g.drawText(txt, r, Justification::centred, false);
}
