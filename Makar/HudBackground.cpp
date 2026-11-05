#include "HudBackground.h"
#include "MakarLayout.h"
#include "MakarLookAndFeel.h"

using namespace juce;

HudBackground::HudBackground(MakarLookAndFeel& l) : laf(l)
{
    setInterceptsMouseClicks(false, false);
    setOpaque(true);
}

void HudBackground::setHalos(int c, bool p)
{
    if (c == circuit && p == punish) return;
    // seules les zones des halos sont redessinées (l'image de fond reste en cache)
    auto halo = [](Rectangle<int> r) { return r.expanded(22); };
    if (circuit >= 0) repaint(halo(MkLayout::circuit(circuit)));
    if (c >= 0)       repaint(halo(MkLayout::circuit(c)));
    if (p != punish)  repaint(halo(MkLayout::punish));
    circuit = c;
    punish = p;
}

// dégradé radial elliptique façon CSS « radial-gradient(ellipse at …) »
static void ellipseGlow(Graphics& g, Point<float> c, float rx, float ry, Colour col)
{
    Graphics::ScopedSaveState s(g);
    g.addTransform(AffineTransform::scale(1.0f, ry / rx, c.x, c.y));
    g.setGradientFill(ColourGradient(col, c, col.withAlpha(0.0f), { c.x + rx, c.y }, true));
    g.fillRect(Rectangle<float>(c.x - rx, c.y - rx, rx * 2.0f, rx * 2.0f));
}

void HudBackground::renderStatic(Graphics& g) const
{
    using namespace MkLayout;
    g.fillAll(MkColours::bg);

    // grille de 40 px (depuis le bas, comme la maquette)
    g.setColour(MkColours::acc2.withAlpha(0.04f));
    for (int x = 0; x < W; x += 40) g.fillRect(x, 0, 1, H);
    for (int y = H - 1; y >= 0; y -= 40) g.fillRect(0, y, W, 1);

    ellipseGlow(g, { 0.47f * W, 0.46f * H }, 330.0f, 197.0f, MkColours::acc.withAlpha(0.16f));
    ellipseGlow(g, { 0.85f * W, 0.30f * H }, 433.0f, 209.0f, MkColours::acc2.withAlpha(0.10f));

    auto spaced = [](Font f, float px) { f.setExtraKerningFactor(px / f.getHeight()); return f; };

    // logo à décalage chromatique, coupé par une fine bande
    const Font logo = spaced(laf.display(46.0f), 8.0f);
    g.setFont(logo);
    auto logoAt = [&](float dx, float dy, Colour c) {
        g.setColour(c);
        g.drawText("MAKAR", Rectangle<float>(24.0f + dx, 14.0f + dy, 330.0f, 50.0f), Justification::topLeft, false);
    };
    logoAt(3.0f, 0.0f, MkColours::acc.withAlpha(0.85f));
    logoAt(-3.0f, 2.0f, MkColours::acc2.withAlpha(0.85f));
    logoAt(0.0f, 1.0f, MkColours::text);
    g.setColour(MkColours::bg);
    g.fillRect(24, 38, 290, 3);

    g.setFont(spaced(laf.mono(11.0f), 3.0f));
    g.setColour(MkColours::label);
    g.drawText("SATURATION // 5 CIRCUITS", 26, 66, 330, 14, Justification::centredLeft, false);

    const Font small = spaced(laf.mono(10.0f), 3.0f);
    g.setFont(small);
    g.drawText("CIRCUIT", circuitX, 104, 180, 13, Justification::centredLeft, false);
    g.drawText("FILTRES", curve.getX(), 104, 180, 13, Justification::centredLeft, false);

    // crochets HUD autour du Drive
    {
        const float x0 = (float) drive.getX(), y0 = (float) drive.getY();
        Path p;
        auto corner = [&](float ax, float ay, float bx, float by, float cx, float cy) {
            p.startNewSubPath(x0 + ax, y0 + ay); p.lineTo(x0 + bx, y0 + by); p.lineTo(x0 + cx, y0 + cy);
        };
        corner(6, 36, 6, 6, 36, 6);
        corner(204, 6, 234, 6, 234, 36);
        corner(6, 204, 6, 234, 36, 234);
        corner(204, 234, 234, 234, 234, 204);
        g.setColour(MkColours::acc2);
        g.strokePath(p, PathStrokeType(2.0f));
    }

    // panneau de la courbe des filtres
    g.setColour(MkColours::panel);
    g.fillRect(curve.toFloat().expanded(1.0f));
    g.setColour(MkColours::line);
    g.drawRect(curve.toFloat().expanded(1.0f), 1.0f);

    // barre du bas à coins biseautés
    const Path barShape = MakarLookAndFeel::bevel(bar.toFloat(), 10.0f);
    g.setColour(MkColours::panel);
    g.fillPath(barShape);
    g.setColour(MkColours::line);
    g.strokePath(barShape, PathStrokeType(1.0f));
    g.setFont(spaced(laf.mono(11.0f), 2.0f));
    g.setColour(MkColours::label);
    g.drawText("MIX",    36,  422, 56, 24, Justification::centredLeft, false);
    g.drawText("OUTPUT", 434, 422, 56, 24, Justification::centredLeft, false);

    // lignes de balayage (1 px sur 3, depuis le bas)
    g.setColour(Colours::white.withAlpha(0.025f));
    for (int y = H - 1; y >= 0; y -= 3) g.fillRect(0, y, W, 1);
}

void HudBackground::paint(Graphics& g)
{
    // fond en cache, à la résolution réelle de l'écran
    const float scale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    if (cache.isNull() || std::abs(scale - cacheScale) > 1.0e-3f) {
        cache = Image(Image::RGB, jmax(1, roundToInt((float) getWidth() * scale)),
                      jmax(1, roundToInt((float) getHeight() * scale)), false);
        Graphics cg(cache);
        cg.addTransform(AffineTransform::scale((float) cache.getWidth() / (float) MkLayout::W,
                                               (float) cache.getHeight() / (float) MkLayout::H));
        renderStatic(cg);
        cacheScale = scale;
        ++cacheRebuilds;
    }
    g.drawImageTransformed(cache, AffineTransform::scale((float) getWidth() / (float) cache.getWidth(),
                                                         (float) getHeight() / (float) cache.getHeight()));

    // halos néon sous les boutons actifs
    g.addTransform(AffineTransform::scale((float) getWidth() / (float) MkLayout::W,
                                          (float) getHeight() / (float) MkLayout::H));
    if (circuit >= 0)
        DropShadow(MkColours::acc.withAlpha(0.75f), 16, {}).drawForRectangle(g, MkLayout::circuit(circuit));
    if (punish)
        DropShadow(MkColours::warn.withAlpha(0.7f), 20, {}).drawForRectangle(g, MkLayout::punish);
}
