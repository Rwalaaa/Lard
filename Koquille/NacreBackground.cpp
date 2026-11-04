#include "NacreBackground.h"
#include "KoquilleLookAndFeel.h"
#include "KoqLayout.h"

using namespace juce;

namespace {
void fillRadial(Graphics& g, Point<float> c, float rx, float ry, Colour inner, Colour outer)
{
    Graphics::ScopedSaveState s(g);
    g.addTransform(AffineTransform::scale(1.0f, ry / rx, c.x, c.y));
    g.setGradientFill(ColourGradient(inner, c.x, c.y, outer, c.x + rx, c.y, true));
    g.fillEllipse(c.x - rx, c.y - rx, rx * 2.0f, rx * 2.0f);
}
}

NacreBackground::NacreBackground()
{
    setOpaque(true);
    setInterceptsMouseClicks(false, false);

    // nautile : spirale logarithmique r = a·e^(bθ), cloisons entre deux tours
    // ~145 px de rayon : un anneau autour de Density, sans toucher les potards voisins
    const float cx = 380.0f, cy = 188.0f, a0 = 6.5f, b = 0.215f;
    for (int i = 0; i <= 260; ++i) {
        const float th = (float) i / 260.0f * 4.6f * MathConstants<float>::pi;
        const float r = a0 * std::exp(b * th);
        const Point<float> p { cx + r * std::cos(th), cy + r * std::sin(th) };
        if (i == 0) spiral.startNewSubPath(p); else spiral.lineTo(p);
        if (i % 13 == 0 && i > 26) {
            const float r2 = a0 * std::exp(b * (th - MathConstants<float>::twoPi));
            chambers.emplace_back(p, Point<float> { cx + r2 * std::cos(th), cy + r2 * std::sin(th) });
        }
    }
}

void NacreBackground::setState(const State& s)
{
    state = s;
    repaint();
}

void NacreBackground::paint(Graphics& g)
{
    g.addTransform(AffineTransform::scale((float) getWidth() / (float) KoqLayout::W,
                                          (float) getHeight() / (float) KoqLayout::H));
    g.fillAll(KoqColours::bg);
    fillRadial(g, { 380.0f, 207.0f }, 430.0f, 270.0f, KoqColours::acc.withAlpha(0.10f), KoqColours::acc.withAlpha(0.0f));

    paintSpiral(g);
    paintTube(g, KoqLayout::pentTube, state.pentDrive, true);
    paintTube(g, KoqLayout::triTube, state.triDrive, false);
    paintMeter(g);

    // panneau du bas et séparateurs
    const auto p = KoqLayout::panel.toFloat();
    g.setColour(KoqColours::panel);
    g.fillRoundedRectangle(p, 10.0f);
    g.setColour(KoqColours::line);
    g.drawRoundedRectangle(p.reduced(0.75f), 10.0f, 1.5f);
    for (int x : { KoqLayout::divider1, KoqLayout::divider2 })
        g.fillRect(Rectangle<float>((float) x, p.getY() + 12.0f, 1.0f, p.getHeight() - 24.0f));
}

void NacreBackground::paintSpiral(Graphics& g) const
{
    const float d = state.density;
    const float op = 0.18f + 0.6f * d;
    // halo puis trait net
    g.setColour(KoqColours::acc.withAlpha(op * 0.25f));
    g.strokePath(spiral, PathStrokeType(7.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour(KoqColours::acc.withAlpha(op * 0.4f));
    g.strokePath(spiral, PathStrokeType(4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour(KoqColours::acc.withAlpha(op));
    g.strokePath(spiral, PathStrokeType(2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour(KoqColours::pearl.withAlpha(0.05f + 0.18f * d));
    for (const auto& l : chambers)
        g.drawLine(l, 0.8f);
}

void NacreBackground::paintTube(Graphics& g, Rectangle<int> area, float drive, bool pentode) const
{
    Graphics::ScopedSaveState s(g);
    g.addTransform(AffineTransform::translation((float) area.getX(), (float) area.getY()));
    drive = jlimit(0.0f, 1.0f, drive);

    // lueur du filament
    fillRadial(g, { 44.0f, 72.0f }, 34.0f, 58.0f, KoqColours::acc.withAlpha(0.2f + 0.75f * drive), KoqColours::acc.withAlpha(0.0f));

    Path glass;
    glass.startNewSubPath(14, 116);
    glass.lineTo(14, 44);
    glass.quadraticTo(14, 8, 44, 8);
    glass.quadraticTo(74, 8, 74, 44);
    glass.lineTo(74, 116);
    glass.closeSubPath();
    g.setColour(Colour::fromFloatRGBA(160.0f / 255.0f, 230.0f / 255.0f, 240.0f / 255.0f, 0.08f));
    g.fillPath(glass);
    g.setColour(KoqColours::glassLine);
    g.strokePath(glass, PathStrokeType(1.5f));

    Path getter;   // reflet argenté du sommet
    getter.startNewSubPath(36, 10);
    getter.quadraticTo(44, 6, 52, 10);
    g.setColour(Colour(0xffC9C2B6));
    g.strokePath(getter, PathStrokeType(2.0f));

    g.setColour(KoqColours::glassLine);
    if (pentode) {
        g.drawRect(Rectangle<float>(30, 40, 28, 60), 1.5f);
        for (int y = 48; y <= 88; y += 8)
            g.drawLine(34.0f, (float) y, 54.0f, (float) y, 1.0f);
    } else {
        g.drawRect(Rectangle<float>(32, 46, 24, 50), 1.5f);
    }
    g.setColour(KoqColours::hot.withAlpha(0.35f + 0.65f * drive));
    g.drawLine(44.0f, pentode ? 44.0f : 48.0f, 44.0f, pentode ? 96.0f : 94.0f, 2.5f);

    g.setColour(KoqColours::base);
    g.fillRoundedRectangle(10, 110, 68, 18, 3.0f);
}

void NacreBackground::paintMeter(Graphics& g) const
{
    Graphics::ScopedSaveState s(g);
    const auto v = KoqLayout::vu;
    g.addTransform(AffineTransform::translation((float) v.getX(), (float) v.getY()));
    g.reduceClipRegion(0, 0, v.getWidth(), v.getHeight());

    const auto frame = Rectangle<float>(1, 1, 148, 62);
    g.setColour(KoqColours::meterBg);
    g.fillRoundedRectangle(frame, 6.0f);
    g.setColour(KoqColours::line);
    g.drawRoundedRectangle(frame, 6.0f, 1.5f);

    for (int i = 0; i <= 10; ++i) {
        const float a = degreesToRadians(-50.0f + (float) i * 10.0f);
        const float r0 = (i % 5 == 0) ? 44.0f : 48.0f;
        g.setColour(i >= 8 ? KoqColours::danger : KoqColours::glassLine);
        g.drawLine(75.0f + r0 * std::sin(a), 70.0f - r0 * std::cos(a),
                   75.0f + 54.0f * std::sin(a), 70.0f - 54.0f * std::cos(a), 1.5f);
    }

    const float na = degreesToRadians(-50.0f + 100.0f * jlimit(0.0f, 1.0f, state.colour));
    g.setColour(KoqColours::acc);
    g.drawLine(75.0f, 70.0f, 75.0f + 52.0f * std::sin(na), 70.0f - 52.0f * std::cos(na), 2.0f);
    g.setColour(KoqColours::line);
    g.fillEllipse(70.0f, 61.0f, 10.0f, 10.0f);
}
