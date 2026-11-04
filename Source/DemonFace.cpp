#include "DemonFace.h"
#include "LardLookAndFeel.h"

using namespace juce;

namespace {

constexpr float W = 720.0f, H = 420.0f;

Colour col(uint32 argb) { return Colour(argb); }

// dégradé radial elliptique (JUCE ne fait que des cercles : on écrase le repère)
void fillRadial(Graphics& g, Point<float> c, float rx, float ry, Colour inner, Colour outer)
{
    Graphics::ScopedSaveState s(g);
    g.addTransform(AffineTransform::scale(1.0f, ry / rx, c.x, c.y));
    g.setGradientFill(ColourGradient(inner, c.x, c.y, outer, c.x + rx, c.y, true));
    g.fillEllipse(c.x - rx, c.y - rx, rx * 2.0f, rx * 2.0f);
}

// trait avec halo : passes larges et transparentes puis trait net
void glowStroke(Graphics& g, const Path& p, Colour c, float width, float glow)
{
    g.setColour(c.withMultipliedAlpha(0.12f * glow));
    g.strokePath(p, PathStrokeType(width + 7.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour(c.withMultipliedAlpha(0.25f * glow));
    g.strokePath(p, PathStrokeType(width + 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour(c);
    g.strokePath(p, PathStrokeType(width, PathStrokeType::curved, PathStrokeType::rounded));
}

Path quadPath(Point<float> a, Point<float> ctl, Point<float> b)
{
    Path p;
    p.startNewSubPath(a);
    p.quadraticTo(ctl, b);
    return p;
}

Path polygon(std::initializer_list<Point<float>> pts)
{
    Path p;
    bool first = true;
    for (auto q : pts) { if (first) { p.startNewSubPath(q); first = false; } else p.lineTo(q); }
    p.closeSubPath();
    return p;
}

Path mirrored(const Path& p)
{
    Path m(p);
    m.applyTransform(AffineTransform::scale(-1.0f, 1.0f).translated(W, 0.0f));
    return m;
}

// --- géométrie partagée avec la maquette (repère 720 x 420) ---
float eyeOpen(float f) { return 7.0f + 15.0f * f; }

Path eyePath(float ox, float ix, float f)
{
    const float h = eyeOpen(f), cx = (ox + ix) * 0.5f;
    Path p;
    p.startNewSubPath(ox, 74.0f);
    p.quadraticTo(cx, 80.0f - 1.1f * h, ix, 94.0f);
    p.quadraticTo(cx, 84.0f + 1.9f * h, ox, 74.0f);
    p.closeSubPath();
    return p;
}

struct Mouth {
    Point<float> L { 205.0f, 238.0f }, R { 515.0f, 238.0f };
    float cx = 360.0f, cyU = 280.0f, cyD;
    explicit Mouth(float f) : cyD(350.0f + 50.0f * f) {}
    static float q(float a, float c, float b, float t) { const float u = 1.0f - t; return u * u * a + 2.0f * u * t * c + t * t * b; }
    Point<float> up(float t) const { return { q(L.x, cx, R.x, t), q(L.y, cyU, R.y, t) }; }
    Point<float> dn(float t) const { return { q(L.x, cx, R.x, t), q(L.y, cyD, R.y, t) }; }
    Path outline() const
    {
        Path p;
        p.startNewSubPath(L);
        p.quadraticTo(cx, cyU, R.x, R.y);
        p.quadraticTo(cx, cyD, L.x, L.y);
        p.closeSubPath();
        return p;
    }
};

struct Ember { float x, y, r, phase, speed; bool dim; };
const Ember embers[] = {
    { 60, 260, 1.2f, 0.1f, 9 , false }, { 120, 330, 2.1f, 1.7f, 12, true }, { 30, 90, 3.0f, 2.9f, 7, false },
    { 640, 60, 1.2f, 0.6f, 10, true }, { 690, 300, 2.1f, 3.3f, 8, false }, { 600, 250, 3.0f, 4.1f, 11, true },
    { 250, 20, 1.2f, 5.2f, 6, false }, { 470, 24, 2.1f, 0.9f, 9, true }, { 90, 180, 3.0f, 2.2f, 13, false },
    { 650, 200, 1.2f, 3.8f, 7, true }, { 160, 40, 2.1f, 4.6f, 10, false }, { 560, 40, 3.0f, 1.3f, 8, true },
    { 20, 370, 1.2f, 2.5f, 12, false }, { 700, 370, 2.1f, 5.7f, 9, true }, { 300, 370, 3.0f, 0.4f, 6, false },
    { 420, 372, 1.2f, 3.0f, 11, true }, { 140, 110, 2.1f, 4.9f, 7, false }, { 580, 120, 3.0f, 1.9f, 10, true },
};

Path bevelled(Rectangle<float> r, float cut)
{
    return polygon({ { r.getX() + cut, r.getY() }, { r.getRight(), r.getY() },
                     { r.getRight(), r.getBottom() - cut }, { r.getRight() - cut, r.getBottom() },
                     { r.getX(), r.getBottom() }, { r.getX(), r.getY() + cut } });
}

} // namespace

static Image makeGlowSprite(Colour c)
{
    Image img(Image::ARGB, 256, 256, true);
    Graphics g(img);
    g.setGradientFill(ColourGradient(c, 128.0f, 128.0f, c.withAlpha(0.0f), 256.0f, 128.0f, true));
    g.fillEllipse(0.0f, 0.0f, 256.0f, 256.0f);
    return img;
}

DemonFace::DemonFace()
{
    setOpaque(true);
    setInterceptsMouseClicks(false, false);
    eyeSprite = makeGlowSprite(col(0xffFF7A2A));
    throatSprite = makeGlowSprite(col(0xffFFB03A));
}

void DemonFace::setFatness(float f)
{
    f = jlimit(0.0f, 1.0f, f);
    if (std::abs(f - fatness) > 1.0e-4f) { fatness = f; repaint(); }
}

void DemonFace::setHeat(float grDb) { heat = jlimit(0.0f, 1.0f, grDb / 12.0f); }

void DemonFace::setHeaderPanel(Rectangle<float> r) { headerPanel = r; invalidate(); }

void DemonFace::tick(double seconds)
{
    t = seconds;
    // on ne redessine que ce qui bouge : lueur des yeux, gorge, colonnes des braises
    const float sx = (float) getWidth() / W, sy = (float) getHeight() / H;
    auto dirty = [&](float x, float y, float w, float h) {
        repaint(Rectangle<float>(x * sx, y * sy, w * sx, h * sy).getSmallestIntegerContainer());
    };
    dirty(140.0f, 30.0f, 440.0f, 112.0f);    // yeux
    dirty(200.0f, 240.0f, 320.0f, 125.0f);   // gorge (lueur jusqu'à y = 358 à fond)
    for (const auto& e : embers)
        dirty(e.x - 8.0f, e.y - 66.0f, 16.0f, 72.0f);
}

void DemonFace::paint(Graphics& g)
{
    // cache à la résolution réelle de l'écran (Retina, mise à l'échelle Windows)
    const float scale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    const float fq = std::round(fatness * 400.0f) / 400.0f;   // pas de 0,25 %
    if (!cacheValid || cache.isNull() || std::abs(fq - cacheFat) > 1.0e-6f || std::abs(scale - cacheScale) > 1.0e-3f) {
        const int pw = jmax(1, roundToInt((float) getWidth() * scale));
        const int ph = jmax(1, roundToInt((float) getHeight() * scale));
        cache = Image(Image::RGB, pw, ph, false);
        Graphics cg(cache);
        cg.addTransform(AffineTransform::scale((float) pw / W, (float) ph / H));
        renderStatic(cg, fq);
        cacheFat = fq; cacheScale = scale; cacheValid = true;
        ++cacheRebuilds;
    }
    // le cache est à la résolution physique : transformation inverse = copie pixel pour pixel
    g.setImageResamplingQuality(Graphics::lowResamplingQuality);
    g.drawImageTransformed(cache, AffineTransform::scale((float) getWidth() / (float) cache.getWidth(),
                                                         (float) getHeight() / (float) cache.getHeight()));
    g.setImageResamplingQuality(Graphics::mediumResamplingQuality);

    Graphics::ScopedSaveState s(g);
    g.addTransform(AffineTransform::scale((float) getWidth() / W, (float) getHeight() / H));
    paintAnimated(g);
}

void DemonFace::renderStatic(Graphics& g, float f) const
{
    g.fillAll(LardColours::bg);

    // brumes violettes et magenta
    const float haze = 0.25f + 0.45f * f;
    fillRadial(g, { 40, 150 }, 190, 215, col(0xffB0207F).withAlpha(haze * 0.75f), col(0x00B0207F));
    fillRadial(g, { 690, 170 }, 190, 215, col(0xff7A1FA8).withAlpha(haze * 0.75f), col(0x007A1FA8));
    fillRadial(g, { 360, 210 }, 320, 205, col(0xff3A1470).withAlpha(0.45f), col(0x003A1470));

    // cheveux en pointes
    {
        static const float tips[] = { 26, 44, 30, 58, 36, 22, 50, 34, 62, 28, 46, 38, 24, 54, 32, 48, 26, 40, 60, 30 };
        Path hair;
        hair.startNewSubPath(-20, -10);
        hair.lineTo(-20, 0);
        for (int i = 0; i < 20; ++i) {
            const float x0 = -20.0f + (float) i * 40.0f;
            hair.lineTo(x0 + 14.0f, tips[i]);
            hair.lineTo(x0 + 40.0f, (i % 2) ? 8.0f : 4.0f);
        }
        hair.lineTo(760, 0);
        hair.lineTo(760, -10);
        hair.closeSubPath();
        g.setColour(col(0xff050208));
        g.fillPath(hair);
        g.setColour(col(0xff6B2AA8).withAlpha(0.6f));
        g.strokePath(hair, PathStrokeType(1.0f));
    }

    // aura derrière Fatness
    {
        const float ar = 80.0f * (0.7f + 0.5f * f) * 1.35f;
        fillRadial(g, auraCentre, ar, ar, LardColours::fire.withAlpha(0.1f + 0.6f * f), LardColours::fire.withAlpha(0.0f));
    }

    // trait rouge du visage
    {
        const float a = 0.45f + 0.55f * f;
        const Colour line = LardColours::red.withAlpha(a);

        const Path browL = polygon({ { 196, 50 }, { 270, 47 }, { 348, 80 }, { 340, 90 }, { 266, 62 }, { 200, 64 } });
        for (const Path& b : { browL, mirrored(browL) }) {
            g.setColour(col(0xffE01E28).withAlpha(a * 0.25f));
            g.strokePath(b, PathStrokeType(6.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour(col(0xffE01E28).withAlpha(a));
            g.fillPath(b);
            g.setColour(line);
            g.strokePath(b, PathStrokeType(1.5f));
        }

        struct Stroke { Path p; float w; };
        std::vector<Stroke> strokes;
        auto both = [&](Path p, float w) { strokes.push_back({ mirrored(p), w }); strokes.push_back({ std::move(p), w }); };
        both(quadPath({ 222, 99 }, { 275, 112 }, { 334, 104 }), 2.2f);   // cernes
        both(quadPath({ 228, 108 }, { 278, 122 }, { 330, 113 }), 1.6f);
        both(quadPath({ 236, 117 }, { 282, 130 }, { 326, 122 }), 1.1f);
        {
            Path nose;                                                     // arête du nez
            nose.startNewSubPath(344, 96);
            nose.quadraticTo(338, 150, 331, 196);
            nose.quadraticTo(334, 214, 349, 219);
            both(nose, 2.4f);
        }
        both(quadPath({ 198, 236 }, { 166, 200 }, { 170, 146 }), 2.6f);   // plis du sourire
        both(quadPath({ 186, 214 }, { 160, 190 }, { 158, 160 }), 1.2f);
        strokes.push_back({ quadPath({ 349, 219 }, { 360, 226 }, { 371, 219 }), 1.6f });
        for (auto& s : strokes) glowStroke(g, s.p, line, s.w, a);
    }

    // yeux incandescents
    for (const Path& eye : { eyePath(214, 340, f), eyePath(506, 380, f) }) {
        const auto b = eye.getBounds();
        fillRadial(g, b.getCentre(), b.getWidth() * 0.75f, b.getWidth() * 0.45f,
                   LardColours::fire.withAlpha(0.45f + 0.4f * f), LardColours::fire.withAlpha(0.0f));
        Graphics::ScopedSaveState s(g);
        const Point<float> c { b.getX() + b.getWidth() * 0.55f, b.getY() + b.getHeight() * 0.55f };
        const float rx = b.getWidth() * 0.6f, ry = b.getHeight() * 0.6f;
        g.addTransform(AffineTransform::scale(1.0f, ry / rx, c.x, c.y));
        ColourGradient grad(col(0xffFFFBD6), c.x, c.y, col(0xffE5261F), c.x + rx, c.y, true);
        grad.addColour(0.35, col(0xffFFE24A));
        grad.addColour(0.75, col(0xffFF8A1F));
        g.setGradientFill(grad);
        Path e(eye);
        e.applyTransform(AffineTransform::scale(1.0f, ry / rx, c.x, c.y).inverted());
        g.fillPath(e);
    }

    // sourire
    {
        const Mouth m(f);
        const Path mouth = m.outline();
        const float glow = 0.25f + 0.6f * f;
        fillRadial(g, { 360, 266 }, 210, 70, LardColours::fire.withAlpha(glow * 0.55f), LardColours::fire.withAlpha(0.0f));
        g.setColour(col(0xff1A0306));
        g.fillPath(mouth);
        glowStroke(g, mouth, LardColours::red, 2.5f, glow);

        const int N = 13;
        const float t0 = 0.07f, step = 0.86f / (float) (N - 1);
        const float teethAlpha = 0.6f + 0.4f * f;
        auto tooth = [&](Point<float> a, Point<float> tip, Point<float> b) {
            Path p = polygon({ a, tip, b });
            const auto bb = p.getBounds();
            const bool down = tip.y > a.y;
            ColourGradient grad(col(0xffFFF3A6).withAlpha(teethAlpha), 0, down ? bb.getY() : bb.getBottom(),
                                col(0xffFF7A1A).withAlpha(teethAlpha), 0, down ? bb.getBottom() : bb.getY(), false);
            grad.addColour(0.6, col(0xffFFC233).withAlpha(teethAlpha));
            g.setGradientFill(grad);
            g.fillPath(p);
            g.setColour(col(0xff8A0A12).withAlpha(teethAlpha));
            g.strokePath(p, PathStrokeType(1.2f));
        };
        for (int i = 0; i < N; ++i) {
            const float tt = t0 + (float) i * step, dt = step * 0.48f;
            const auto mid = m.up(tt), opp = m.dn(tt);
            const float fang = (i == 2 || i == N - 3) ? 0.78f : 0.52f;
            tooth(m.up(tt - dt), { mid.x, mid.y + (opp.y - mid.y) * fang }, m.up(tt + dt));
        }
        for (int i = 0; i < N - 1; ++i) {
            const float tt = t0 + ((float) i + 0.5f) * step, dt = step * 0.46f;
            const auto mid = m.dn(tt), opp = m.up(tt);
            tooth(m.dn(tt - dt), { mid.x, mid.y + (opp.y - mid.y) * 0.42f }, m.dn(tt + dt));
        }
    }

    // cadre du logo
    if (!headerPanel.isEmpty()) {
        const Path shape = bevelled(headerPanel, 12.0f);
        for (int i = 3; i >= 1; --i) {
            g.setColour(LardColours::fire.withAlpha(0.07f));
            g.strokePath(shape, PathStrokeType(2.0f + (float) i * 6.0f));
        }
        g.setColour(LardColours::panel);
        g.fillPath(shape);
        g.setColour(LardColours::ember);
        g.strokePath(shape, PathStrokeType(2.0f));
    }

    // fond de la barre du bas
    {
        const Path shape = bevelled({ 16.0f, H - 12.0f - 52.0f, W - 32.0f, 52.0f }, 10.0f);
        g.setColour(LardColours::bar);
        g.fillPath(shape);
        g.setColour(LardColours::ember.withAlpha(0.53f));
        g.strokePath(shape, PathStrokeType(1.5f));
    }
}

void DemonFace::paintAnimated(Graphics& g) const
{
    const float f = fatness;
    const float tt = (float) t;
    // bruit doux, entre 0 et 1
    const float n = 0.5f + 0.25f * std::sin(7.1f * tt) + 0.15f * std::sin(13.7f * tt + 1.0f)
                         + 0.10f * std::sin(23.3f * tt + 2.0f);
    const float n2 = 0.5f + 0.25f * std::sin(5.3f * tt + 0.7f) + 0.15f * std::sin(17.9f * tt + 2.4f)
                          + 0.10f * std::sin(29.1f * tt + 0.3f);

    // les yeux vacillent, et flambent quand ça compresse
    const float eyeA = jlimit(0.0f, 0.9f, (0.12f + 0.28f * f) * (0.6f + 0.8f * n) + 0.45f * heat);
    const float erx = 80.0f + 30.0f * f + 25.0f * heat, ery = 34.0f + 10.0f * f + 10.0f * heat;
    g.setOpacity(eyeA);
    for (auto c : { Point<float>(277.0f, 86.0f), Point<float>(443.0f, 86.0f) })
        g.drawImage(eyeSprite, { c.x - erx, c.y - ery, erx * 2.0f, ery * 2.0f });

    // la gorge rougeoie
    const Mouth m(f);
    const float mouthA = jlimit(0.0f, 0.8f, (0.08f + 0.22f * f) * (0.6f + 0.8f * n2) + 0.35f * heat);
    const float mry = 26.0f + 14.0f * f, mcy = (m.cyU + m.cyD) * 0.5f - 22.0f;
    g.setOpacity(mouthA);
    g.drawImage(throatSprite, { 360.0f - 150.0f, mcy - mry, 300.0f, mry * 2.0f });
    g.setOpacity(1.0f);

    // braises qui montent
    const float emberBase = 0.15f + 0.7f * f;
    for (const auto& e : embers) {
        const float rise = std::fmod(tt * e.speed + e.phase * 13.0f, 60.0f);   // 0..60 px puis recommence
        const float life = 1.0f - rise / 60.0f;
        const float x = e.x + 4.0f * std::sin(tt * 1.3f + e.phase);
        const float y = e.y - rise;
        const float a = emberBase * (e.dim ? 0.6f : 1.0f) * life * (0.7f + 0.3f * std::sin(tt * 9.0f + e.phase * 3.0f));
        g.setColour(col(0xffFF4A2A).withAlpha(jlimit(0.0f, 1.0f, a)));
        g.fillEllipse(x - e.r, y - e.r, e.r * 2.0f, e.r * 2.0f);
    }
}
