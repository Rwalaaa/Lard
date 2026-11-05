#include "HudWidgets.h"
#include "MakarLookAndFeel.h"
#include "MakarEngine.h"
#include <complex>

using namespace juce;

namespace {
    constexpr float cw = 236.0f, ch = 104.0f, topDb = 12.0f, botDb = -24.0f;
    constexpr double fs = 96000.0;   // fréquence d'affichage : la forme ne dépend pas de celle de l'hôte
    float xOf(float hz) { return std::log(hz / 20.0f) / std::log(1000.0f) * cw; }
    float yOf(float db) { return (topDb - jlimit(botDb, topDb, db)) / (topDb - botDb) * ch; }
}

FilterCurve::FilterCurve()
{
    setInterceptsMouseClicks(false, false);
}

void FilterCurve::setSettings(const Settings& n)
{
    if (built && n == s) return;
    s = n;
    rebuild();
    repaint();
}

void FilterCurve::rebuild()
{
    // mêmes réglages que makar::Engine (filtres d'entrée et coupe-haut)
    const makar::Tuning T;
    stages.clear();
    auto add = [this](const lard::Biquad& b) { stages.push_back({ b.b0, b.b1, b.b2, b.a1, b.a2 }); };
    lard::Biquad b;
    if (s.lowCutHz > 20.5f) { b.setHighPass(fs, s.lowCutHz, 0.7071); add(b); }
    if (s.thump) { b.setPeak(fs, std::max(s.lowCutHz, T.thumpMinHz), T.thumpDb, T.thumpQ); add(b); }
    if (std::fabs(s.tone) > 1.0e-3f) {
        b.setLowShelf(fs, T.toneHz, -T.toneDb * s.tone, 0.5); add(b);
        b.setHighShelf(fs, T.toneHz, T.toneDb * s.tone, 0.5); add(b);
    }
    hcPole = 0.0;
    if (s.highCutHz < 19999.0f) {
        hcPole = std::exp(-2.0 * lard::kPi * std::min((double) s.highCutHz, 0.45 * fs) / fs);
        if (s.steep) {
            b.setLowPass(fs, s.highCutHz, 0.618034); add(b);
            b.setLowPass(fs, s.highCutHz, 1.618034); add(b);
        }
    }

    line.clear();
    constexpr int n = 160;
    for (int i = 0; i <= n; ++i) {
        const float hz = 20.0f * std::pow(1000.0f, (float) i / (float) n);
        const Point<float> pt(xOf(hz), yOf(magnitudeDb(hz)));
        if (i == 0) line.startNewSubPath(pt); else line.lineTo(pt);
    }
    fill = line;
    fill.lineTo(cw, ch);
    fill.lineTo(0.0f, ch);
    fill.closeSubPath();
    built = true;
}

float FilterCurve::magnitudeDb(float hz) const
{
    const std::complex<double> z1 = std::polar(1.0, -2.0 * lard::kPi * (double) hz / fs);
    const std::complex<double> z2 = z1 * z1;
    double mag = 1.0;
    for (const auto& c : stages)
        mag *= std::abs(c.b0 + c.b1 * z1 + c.b2 * z2) / std::abs(1.0 + c.a1 * z1 + c.a2 * z2);
    if (hcPole > 0.0)   // y = x + a (y' - x)  ->  H = (1 - a) / (1 - a z^-1)
        mag *= (1.0 - hcPole) / std::abs(1.0 - hcPole * z1);
    return (float) (20.0 * std::log10(std::max(mag, 1.0e-9)));
}

void FilterCurve::paint(Graphics& g)
{
    g.addTransform(AffineTransform::scale((float) getWidth() / cw, (float) getHeight() / ch));
    g.setColour(MkColours::line);
    for (float hz : { 100.0f, 1000.0f, 10000.0f }) g.fillRect(xOf(hz) - 0.5f, 0.0f, 1.0f, ch);
    for (float db : { 12.0f, 0.0f, -12.0f })     g.fillRect(0.0f, yOf(db) - 0.5f, cw, 1.0f);
    g.setColour(MkColours::acc2.withAlpha(0.12f));
    g.fillPath(fill);
    MakarLookAndFeel::neonStroke(g, line, MkColours::acc2, 2.0f);
}

// ---------------------------------------------------------------------------------------------

AttitudeMeter::AttitudeMeter(MakarLookAndFeel& l) : laf(l)
{
    setInterceptsMouseClicks(false, false);
    setDb(-60.0f);
}

int AttitudeMeter::litFor(float db)
{
    // -20 dB (repos) .. +50 dB (Drive + Punish au maximum)
    return roundToInt(jlimit(0.0f, 1.0f, (db + 20.0f) / 70.0f) * (float) segments);
}

void AttitudeMeter::setDb(float db)
{
    const int n = litFor(db);
    const String t = db <= -59.5f ? String("--- dB") : (db > 0.5f ? "+" : "") + String(roundToInt(db)) + " dB";
    if (n == lit && t == text) return;
    lit = n;
    text = t;
    repaint();
}

void AttitudeMeter::paint(Graphics& g)
{
    g.addTransform(AffineTransform::scale((float) getWidth() / (214.0f + 2 * margin),
                                          (float) getHeight() / (38.0f + 2 * margin)));
    g.addTransform(AffineTransform::translation((float) margin, (float) margin));

    Font f = laf.mono(10.0f);
    f.setExtraKerningFactor(0.2f);
    g.setFont(f);
    g.setColour(MkColours::label);
    g.drawText("ATTITUDE", 0, 0, 120, 13, Justification::centredLeft, false);
    g.setColour(MkColours::text);
    g.drawText(text, 94, 0, 120, 13, Justification::centredRight, false);

    for (int i = 0; i < segments; ++i) {
        const Rectangle<int> r(i * 9, 18, 7, 20);
        if (i < lit) {
            const Colour c = i >= 18 ? MkColours::warn : (i >= 12 ? MkColours::acc : MkColours::acc2);
            DropShadow(c.withAlpha(0.7f), 6, {}).drawForRectangle(g, r);
            g.setColour(c);
        } else {
            g.setColour(MkColours::line);
        }
        g.fillRect(r);
    }
}
