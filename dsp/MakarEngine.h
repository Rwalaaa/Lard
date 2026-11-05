// MakarEngine.h — moteur audio du plugin "MAKAR" (saturateur de piste à 5 caractères, C++ pur).
// Chaîne : entrée -> coupe-bas, Thump, Tone -> [Transfo/Bande : accentuation] -> x4 : saturation (ADAA)
//          -> retour x1 -> [désaccentuation] -> voicing du style -> coupe-haut (Steep) -> anti-continu
//          -> Auto -> mix (sec aligné) -> sortie.
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include "Oversampler4x.h"
#include "Biquad.h"

namespace makar {

using lard::dbToGain;
using lard::kPi;
using lard::Biquad;

enum Style { Bande = 0, Transfo = 1, Germanium = 2, Triode = 3, Pentode = 4, kNumStyles = 5 };

struct Params {
    float drive    = 0.3f;   // 0..1
    bool  punish   = false;  // +20 dB d'attaque
    int   style    = Bande;
    float mix      = 1.0f;   // 0..1
    float outputDb = 0.0f;   // -24..+12
    // avant la saturation
    float lowCutHz = 20.0f;  // 20 (coupé) .. 1000 Hz, 12 dB/oct
    bool  thump    = false;  // bosse dans le grave à la fréquence du coupe-bas
    float tone     = 0.0f;   // -1 (sombre) .. +1 (brillant)
    // après la saturation
    float highCutHz = 22000.0f; // 20000+ (coupé) .. 1000 Hz
    bool  steep    = false;  // pente 6 -> 30 dB/oct
    bool  autoGain = false;  // compense le niveau ajouté par le Drive
};

// Constantes de voicing (réglages à l'oreille)
struct Tuning {
    float driveMinDb = -12.0f, driveRangeDb = 42.0f, punishDb = 20.0f;
    float refLevel   = 0.25f;  // -12 dBFS passe au même niveau, quel que soit le Drive
    float liftDb     = 6.0f;   // le Drive fait monter le niveau (comme le matériel), Auto compense
    float punishLiftDb = 3.0f;
    float triodeBias = 0.35f;
    float germaniumK = 1.8f;   // côté négatif plafonné à 1/k : asymétrie
    float transfoEmphDb = 6.0f, transfoEmphHz = 150.0f;   // Transfo : le grave sature d'abord
    float bandeEmphDb = 6.0f, bandeEmphHz = 3000.0f;      // Bande : l'aigu sature d'abord, puis est rabaissé
    float bandeDriveDb = -6.0f;                            // la bande attaque plus doucement
    float compShelfHz = 14500.0f, compShelfDb = 1.0f, compShelfS = 0.5f; // aplatit l'aigu (x4 + ADAA)
    float thumpDb = 4.0f, thumpQ = 1.0f, thumpMinHz = 50.0f;
    float toneDb = 5.0f, toneHz = 800.0f;  // bascule : étagères basse et haute opposées
    float autoRefLevel = 0.125f;            // Auto vise un programme vers -18 dBFS
    float transfoAutoDb = 5.0f;             // Transfo : le grave accentué attaque plus fort la saturation
    float bandeAutoDb = 0.0f;
};

class Engine {
public:
    static constexpr int kMaxChannels = 2;
    using OS = lard::Oversampler4x;

    Tuning tuning;

    void prepare(double sampleRate) {
        sr = (float) sampleRate;
        dcR = 1.0f - 2.0f * (float) kPi * 5.0f / sr;
        smoothCoefChunk = std::exp(-(float) kChunk / (0.020f * sr));
        compShelf.setHighShelf(sr, tuning.compShelfHz, tuning.compShelfDb, tuning.compShelfS);
        lastStyle = -1;
        lastLowCut = lastHighCut = lastTone = -999.0f;
        lastThump = lastSteep = -1;
        envAtk = std::exp(-1.0f / (0.010f * sr));
        envRel = std::exp(-1.0f / (0.300f * sr));
        reset();
        cur = target;
        punishAmt = target.punish ? 1.0f : 0.0f;   // pas de rampe au chargement
        computeDerived();
    }

    void reset() {
        for (int c = 0; c < kMaxChannels; ++c) {
            os[c].reset();
            adaaX1[c] = adaaF1[c] = 0.0;
            fCacheOk[c] = false;
            avgX1[c] = 0.0f;
            dcX1[c] = dcY1[c] = 0.0f;
            dryBuf[c].assign((size_t) latencySamples() + 1, 0.0f);
            for (float& v : wetDelay[c]) v = 0.0f;
            styleChanged[c] = false;
        }
        dryPos = 0;
        wetPos = 0;
        compShelf.reset(); emphPre.reset(); emphPost.reset(); voiceLow.reset(); voiceHigh.reset();
        lowCut.reset(); thumpPk.reset(); toneLow.reset(); toneHigh.reset(); hcA.reset(); hcB.reset();
        for (float& v : hcPole) v = 0.0f;
        env = 0.0f;
        attitude = -60.0f;
    }

    // attaque du circuit (dB) : niveau d'entrée de la saturation rapporté au niveau de référence
    float attitudeDb() const { return attitude; }

    void setParams(const Params& p) { target = p; }
    // suréchantillonneur (32) + 1 : ADAA (1/2 éch. x4) + moyenne (1/2 éch. x4) + 3 éch. x4
    int latencySamples() const { return OS::latencyBaseSamples() + 1; }

    void process(float* const* ch, int numChannels, int numSamples) {
        numChannels = std::min(numChannels, kMaxChannels);
        for (int c = 0; c < numChannels; ++c)
            lard::sanitizeInput(ch[c], numSamples);
        int done = 0;
        while (done < numSamples) {
            const int n = std::min(kChunk, numSamples - done);
            const float prevOut = outGain, prevMix = cur.mix;
            const float a = n == kChunk ? smoothCoefChunk : std::exp(-(float) n / (0.020f * sr));
            cur.drive    = target.drive    + a * (cur.drive    - target.drive);
            cur.mix      = target.mix      + a * (cur.mix      - target.mix);
            cur.outputDb = target.outputDb + a * (cur.outputDb - target.outputDb);
            // Punish : rampe sur le gain plutôt qu'un saut
            const float pTarget = target.punish ? 1.0f : 0.0f;
            punishAmt = pTarget + a * (punishAmt - pTarget);
            // fréquences lissées en échelle logarithmique
            auto logSmooth = [a](float cur_, float tgt) {
                const float lc = std::log(std::max(cur_, 1.0f)), lt = std::log(std::max(tgt, 1.0f));
                return std::exp(lt + a * (lc - lt));
            };
            cur.lowCutHz  = logSmooth(cur.lowCutHz, std::clamp(target.lowCutHz, 20.0f, 1000.0f));
            cur.highCutHz = logSmooth(cur.highCutHz, std::clamp(target.highCutHz, 1000.0f, 22000.0f));
            cur.tone = target.tone + a * (cur.tone - target.tone);
            cur.style = target.style;
            cur.thump = target.thump; cur.steep = target.steep; cur.autoGain = target.autoGain;
            computeDerived();
            for (bool& ok : fCacheOk) ok = false;   // g a pu changer : primitive précédente à recalculer
            processChunk(ch, numChannels, done, n, prevOut, prevMix);
            done += n;
        }
        attitude = 20.0f * std::log10(std::max(env * (float) g / tuning.refLevel, 1.0e-3f));
    }

private:
    static constexpr int kChunk = 32;

    float sr = 48000.0f;
    Params target, cur;
    float punishAmt = 0.0f;
    float smoothCoefChunk = 0.0f;

    // dérivés
    double g = 1.0, A = 1.0;        // attaque et gain de sortie de la saturation
    float outGain = 1.0f;
    int style = Bande, lastStyle = -1;

    // états
    float dcR = 0.999f;
    float dcX1[kMaxChannels] = {}, dcY1[kMaxChannels] = {};
    OS os[kMaxChannels];
    double adaaX1[kMaxChannels] = {}, adaaF1[kMaxChannels] = {};
    bool fCacheOk[kMaxChannels] = {};
    float avgX1[kMaxChannels] = {};
    bool styleChanged[kMaxChannels] = {};
    static constexpr int kWetDelay = 3;
    float wetDelay[kMaxChannels][kWetDelay] = {};
    int wetPos = 0;
    std::vector<float> dryBuf[kMaxChannels];
    int dryPos = 0;
    Biquad compShelf, emphPre, emphPost, voiceLow, voiceHigh;
    bool voiceLowOn = false, voiceHighOn = false, emphOn = false;

    // filtres d'entrée et de sortie
    Biquad lowCut, thumpPk, toneLow, toneHigh, hcA, hcB;
    float hcPole[kMaxChannels] = {}, hcCoef = 0.0f;
    bool lowCutOn = false, thumpOn = false, toneOn = false, hcOn = false, steepOn = false;
    float lastLowCut = -999.0f, lastHighCut = -999.0f, lastTone = -999.0f;
    int lastThump = -1, lastSteep = -1;
    double autoComp = 1.0;

    // attaque (vu-mètre)
    float env = 0.0f, envAtk = 0.99f, envRel = 0.999f, attitude = -60.0f;

    // ---- caractères : f (pente 1 à l'origine) et sa primitive F (F(0) = 0) ----
    static inline double logcosh(double z) {
        const double a = std::fabs(z);
        return a + std::log1p(std::exp(-2.0 * a)) - 0.69314718055994530942;
    }
    double f(double x) const {
        switch (style) {
            case Bande:
            case Transfo: return std::tanh(x);
            case Germanium: { const double k = tuning.germaniumK; return x >= 0 ? std::tanh(x) : std::tanh(k * x) / k; }
            case Triode: {
                const double b = tuning.triodeBias, s = 1.0 / std::cosh(b);
                return (std::tanh(x + b) - std::tanh(b)) / (s * s);
            }
            default:      return x / std::sqrt(1.0 + x * x);   // Pentode
        }
    }
    double F(double x) const {
        switch (style) {
            case Bande:
            case Transfo: return logcosh(x);
            case Germanium: { const double k = tuning.germaniumK; return x >= 0 ? logcosh(x) : logcosh(k * x) / (k * k); }
            case Triode: {
                const double b = tuning.triodeBias, s = 1.0 / std::cosh(b);
                return (logcosh(x + b) - logcosh(b) - std::tanh(b) * x) / (s * s);
            }
            default:      return std::sqrt(1.0 + x * x) - 1.0;   // Pentode
        }
    }

    void computeDerived() {
        const Tuning& T = tuning;
        style = std::clamp(cur.style, 0, (int) kNumStyles - 1);
        const float d = std::clamp(cur.drive, 0.0f, 1.0f);
        g = dbToGain(T.driveMinDb + T.driveRangeDb * d + T.punishDb * punishAmt
                     + (style == Bande ? T.bandeDriveDb : 0.0f));
        // niveau : une entrée à refLevel ressort à refLevel (moyenne des deux côtés : les
        // courbes asymétriques ne gonflent pas le volume), puis le Drive relève un peu
        const double r = g * T.refLevel;
        const double norm = T.refLevel / (0.5 * (f(r) - f(-r)));
        A = norm * dbToGain(T.liftDb * d + T.punishLiftDb * punishAmt);
        outGain = dbToGain(cur.outputDb);

        // Auto : gain inverse de celui que subit un programme vers -18 dBFS (calcul direct, sans pompage)
        {
            const double a = T.autoRefLevel * dbToGain(style == Transfo ? T.transfoAutoDb
                                                     : style == Bande ? T.bandeAutoDb : 0.0f);
            const double ra = g * a;
            const double gainAtRef = A * 0.5 * (f(ra) - f(-ra)) / a;
            autoComp = cur.autoGain ? 1.0 / std::max(gainAtRef, 1.0e-6) : 1.0;
        }

        // filtres d'entrée
        const bool thumpNow = cur.thump;
        if (std::fabs(cur.lowCutHz - lastLowCut) > 0.002f * lastLowCut || (int) thumpNow != lastThump) {
            lowCutOn = cur.lowCutHz > 20.5f;
            if (lowCutOn) lowCut.setHighPass(sr, cur.lowCutHz, 0.7071);
            thumpOn = thumpNow;
            if (thumpOn) thumpPk.setPeak(sr, std::max(cur.lowCutHz, T.thumpMinHz), T.thumpDb, T.thumpQ);
            lastLowCut = cur.lowCutHz;
            lastThump = (int) thumpNow;
        }
        if (std::fabs(cur.tone - lastTone) > 1.0e-3f) {
            toneOn = std::fabs(cur.tone) > 1.0e-3f;
            if (toneOn) {
                toneLow.setLowShelf(sr, T.toneHz, -T.toneDb * cur.tone, 0.5);
                toneHigh.setHighShelf(sr, T.toneHz, T.toneDb * cur.tone, 0.5);
            }
            lastTone = cur.tone;
        }
        // coupe-haut : 1 pôle (6 dB/oct) ou Butterworth d'ordre 5 (30 dB/oct)
        if (std::fabs(cur.highCutHz - lastHighCut) > 0.002f * lastHighCut || (int) cur.steep != lastSteep) {
            hcOn = cur.highCutHz < 19999.0f;
            steepOn = cur.steep;
            if (hcOn) {
                hcCoef = std::exp(-2.0f * (float) kPi * std::min(cur.highCutHz, 0.45f * sr) / sr);
                if (steepOn) {
                    hcA.setLowPass(sr, cur.highCutHz, 0.618034);
                    hcB.setLowPass(sr, cur.highCutHz, 1.618034);
                }
            }
            lastHighCut = cur.highCutHz;
            lastSteep = (int) cur.steep;
        }

        if (style != lastStyle) {
            // accentuation avant / désaccentuation après la saturation (inverses exactes)
            emphOn = style == Transfo || style == Bande;
            if (style == Transfo) {
                emphPre.setLowShelf(sr, T.transfoEmphHz, T.transfoEmphDb, 0.7);
                emphPost.setLowShelf(sr, T.transfoEmphHz, -T.transfoEmphDb, 0.7);
            } else if (style == Bande) {
                emphPre.setHighShelf(sr, T.bandeEmphHz, T.bandeEmphDb, 0.7);
                emphPost.setHighShelf(sr, T.bandeEmphHz, -T.bandeEmphDb, 0.7);
            }
            voiceLowOn = voiceHighOn = false;
            switch (style) {
                case Bande:     voiceLow.setLowShelf(sr, 70.0, 1.0, 0.7);  voiceHigh.setHighShelf(sr, 9000.0, -1.5, 0.7); voiceLowOn = voiceHighOn = true; break;
                case Transfo:   voiceHigh.setHighShelf(sr, 12000.0, 1.2, 0.7); voiceHighOn = true; break;
                case Germanium: voiceLow.setLowShelf(sr, 90.0, 1.5, 0.7);  voiceHigh.setHighShelf(sr, 7000.0, -2.0, 0.7); voiceLowOn = voiceHighOn = true; break;
                default: break;   // Triode, Pentode : sans voicing
            }
            if (lastStyle >= 0)
                for (int c = 0; c < kMaxChannels; ++c) styleChanged[c] = true;
            lastStyle = style;
        }
    }

    // ADAA du 1er ordre sur y = A·f(g·x) : primitive A·F(g·x)/g
    // F(g·x) de l'échantillon précédent est gardé : une seule primitive calculée par échantillon
    // (cache invalidé à chaque bloc de 32, quand g ou le style peuvent changer)
    inline float shape(double x, int c) {
        double y;
        const double d = x - adaaX1[c];
        const double Fx = F(g * x);
        if (styleChanged[c] || std::fabs(d) < 1.0e-6) {
            y = A * f(g * 0.5 * (x + adaaX1[c]));   // changement de style : pas de différence entre deux primitives
            styleChanged[c] = false;
        } else {
            const double F1 = fCacheOk[c] ? adaaF1[c] : F(g * adaaX1[c]);
            y = A * (Fx - F1) / (g * d);
        }
        adaaX1[c] = x;
        adaaF1[c] = Fx;
        fCacheOk[c] = true;
        return (float) y;
    }

    void processChunk(float* const* ch, int nch, int offset, int n, float outFrom, float mixFrom) {
        const float outTo = outGain, mixTo = cur.mix;
        const float invN = 1.0f / (float) n;
        const bool emph = emphOn;
        const float autoK = (float) autoComp;
        float up4[OS::kFactor];

        for (int i = 0; i < n; ++i) {
            const int s = offset + i;
            const float k = (float) (i + 1) * invN;
            const float gout = outFrom + (outTo - outFrom) * k;
            const float mix  = mixFrom + (mixTo - mixFrom) * k;
            float peak = 0.0f;

            for (int c = 0; c < nch; ++c) {
                const float in = ch[c][s];
                std::vector<float>& db = dryBuf[c];
                const int len = (int) db.size();
                db[(size_t) dryPos] = in;
                const float dry = db[(size_t) ((dryPos + 1) % len)];

                // mise en forme avant la saturation
                float x = in;
                if (lowCutOn) x = lowCut.process(x, c);
                if (thumpOn)  x = thumpPk.process(x, c);
                if (toneOn)   x = toneHigh.process(toneLow.process(x, c), c);
                peak = std::max(peak, std::fabs(x));
                if (emph) x = emphPre.process(x, c);

                os[c].up(x, up4);
                int wp = wetPos;
                for (int p = 0; p < OS::kFactor; ++p) {
                    const float v = shape(up4[p], c);
                    // moyenne de deux échantillons : complète le retard de l'ADAA à 1 éch. x4
                    const float avg = 0.5f * (v + avgX1[c]);
                    avgX1[c] = v;
                    up4[p] = wetDelay[c][wp];
                    wetDelay[c][wp] = avg;
                    wp = (wp + 1 == kWetDelay) ? 0 : wp + 1;
                }
                if (c == nch - 1) wetPos = wp;
                x = os[c].down(up4);

                if (emph) x = emphPost.process(x, c);
                x = compShelf.process(x, c);
                if (voiceLowOn)  x = voiceLow.process(x, c);
                if (voiceHighOn) x = voiceHigh.process(x, c);

                // coupe-haut après la saturation
                if (hcOn) {
                    hcPole[c] = x + hcCoef * (hcPole[c] - x);
                    x = hcPole[c];
                    if (steepOn) x = hcB.process(hcA.process(x, c), c);
                }

                const float y = (x - dcX1[c] + dcR * dcY1[c]);
                dcX1[c] = x; dcY1[c] = y;

                ch[c][s] = (dry + mix * (y * autoK - dry)) * gout;
            }
            dryPos = (dryPos + 1) % (int) dryBuf[0].size();
            env = peak > env ? peak + envAtk * (env - peak) : peak + envRel * (env - peak);
        }
    }
};

} // namespace makar
