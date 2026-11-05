// MakarEngine.h — moteur audio du plugin "MAKAR" (saturateur de piste à 5 caractères, C++ pur).
// Chaîne (M1a) : entrée -> [Transfo/Bande : accentuation] -> x4 : saturation (ADAA)
//                -> retour x1 -> [désaccentuation] -> voicing du style -> anti-continu
//                -> mix (sec aligné) -> sortie.
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
        reset();
        cur = target;
        computeDerived();
    }

    void reset() {
        for (int c = 0; c < kMaxChannels; ++c) {
            os[c].reset();
            adaaX1[c] = 0.0;
            avgX1[c] = 0.0f;
            dcX1[c] = dcY1[c] = 0.0f;
            dryBuf[c].assign((size_t) latencySamples() + 1, 0.0f);
            for (float& v : wetDelay[c]) v = 0.0f;
            styleChanged[c] = false;
        }
        dryPos = 0;
        wetPos = 0;
        compShelf.reset(); emphPre.reset(); emphPost.reset(); voiceLow.reset(); voiceHigh.reset();
    }

    void setParams(const Params& p) { target = p; }
    // suréchantillonneur (32) + 1 : ADAA (1/2 éch. x4) + moyenne (1/2 éch. x4) + 3 éch. x4
    int latencySamples() const { return OS::latencyBaseSamples() + 1; }

    void process(float* const* ch, int numChannels, int numSamples) {
        numChannels = std::min(numChannels, kMaxChannels);
        int done = 0;
        while (done < numSamples) {
            const int n = std::min(kChunk, numSamples - done);
            const float prevOut = outGain, prevMix = cur.mix;
            const float a = smoothCoefChunk;
            cur.drive    = target.drive    + a * (cur.drive    - target.drive);
            cur.mix      = target.mix      + a * (cur.mix      - target.mix);
            cur.outputDb = target.outputDb + a * (cur.outputDb - target.outputDb);
            // Punish : rampe sur le gain plutôt qu'un saut
            const float pTarget = target.punish ? 1.0f : 0.0f;
            punishAmt = pTarget + a * (punishAmt - pTarget);
            cur.style = target.style;
            computeDerived();
            processChunk(ch, numChannels, done, n, prevOut, prevMix);
            done += n;
        }
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
    double adaaX1[kMaxChannels] = {};
    float avgX1[kMaxChannels] = {};
    bool styleChanged[kMaxChannels] = {};
    static constexpr int kWetDelay = 3;
    float wetDelay[kMaxChannels][kWetDelay] = {};
    int wetPos = 0;
    std::vector<float> dryBuf[kMaxChannels];
    int dryPos = 0;
    Biquad compShelf, emphPre, emphPost, voiceLow, voiceHigh;
    bool voiceLowOn = false, voiceHighOn = false, emphOn = false;

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
    inline float shape(double x, int c) {
        double y;
        const double d = x - adaaX1[c];
        if (styleChanged[c] || std::fabs(d) < 1.0e-6) {
            y = A * f(g * 0.5 * (x + adaaX1[c]));   // changement de style : pas de différence entre deux primitives
            styleChanged[c] = false;
        } else {
            y = A * (F(g * x) - F(g * adaaX1[c])) / (g * d);
        }
        adaaX1[c] = x;
        return (float) y;
    }

    void processChunk(float* const* ch, int nch, int offset, int n, float outFrom, float mixFrom) {
        const float outTo = outGain, mixTo = cur.mix;
        const float invN = 1.0f / (float) n;
        const bool emph = emphOn;
        float up4[OS::kFactor];

        for (int i = 0; i < n; ++i) {
            const int s = offset + i;
            const float k = (float) (i + 1) * invN;
            const float gout = outFrom + (outTo - outFrom) * k;
            const float mix  = mixFrom + (mixTo - mixFrom) * k;

            for (int c = 0; c < nch; ++c) {
                const float in = ch[c][s];
                std::vector<float>& db = dryBuf[c];
                const int len = (int) db.size();
                db[(size_t) dryPos] = in;
                const float dry = db[(size_t) ((dryPos + 1) % len)];

                float x = emph ? emphPre.process(in, c) : in;

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

                const float y = x - dcX1[c] + dcR * dcY1[c];
                dcX1[c] = x; dcY1[c] = y;

                ch[c][s] = (dry + mix * (y - dry)) * gout;
            }
            dryPos = (dryPos + 1) % (int) dryBuf[0].size();
        }
    }
};

} // namespace makar
