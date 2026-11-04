// KoquilleEngine.h — moteur audio du plugin "Koquille" (saturateur à lampes, C++ pur).
// Chaîne : entrée -> transfo d'entrée -> [x4 : pentode -> liaison -> triode] -> transfo de sortie
//          -> DC block -> mix (sec aligné) -> sortie.
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include "Oversampler4x.h"

namespace koq {

using lard::dbToGain;
using lard::kPi;

struct Params {
    float inputDb  = 0.0f;   // -12..+12 : attaque des étages
    float pentode  = 0.0f;   // 0..1 : gain de l'étage pentode (impaires, mordant)
    float triode   = 0.0f;   // 0..1 : gain de l'étage triode (paires, chaleur)
    float density  = 0.0f;   // 0..1 : pousse les deux lampes ensemble
    float mix      = 1.0f;   // 0..1
    float outputDb = 0.0f;   // -12..+12
};

// Constantes de voicing (réglages à l'oreille)
struct Tuning {
    float stageRangeDb   = 24.0f;  // course de gain de la triode
    float pentodeRangeDb = 36.0f;  // la pentode mord plus tard : course plus longue
    float densityRangeDb = 12.0f;  // gain ajouté aux deux par Density
    float driveFloor     = 0.08f;  // gain « à zéro » : quasi linéaire
    float triodeBias     = 0.40f;  // asymétrie max de la triode (argument de tanh)
    float xfmrInAmount   = 0.40f;  // saturation du grave, transfo d'entrée
    float xfmrOutAmount  = 0.35f;  // idem, transfo de sortie
    float xfmrInHz       = 150.0f; // grave concerné
    float xfmrOutHz      = 110.0f;
};

class Engine {
public:
    static constexpr int kMaxChannels = 2;
    using OS = lard::Oversampler4x;

    Tuning tuning;

    void prepare(double sampleRate) {
        sr = (float) sampleRate;
        const float srOs = sr * OS::kFactor;
        xInCoef  = std::exp(-2.0f * (float) kPi * tuning.xfmrInHz / sr);
        xOutCoef = std::exp(-2.0f * (float) kPi * tuning.xfmrOutHz / sr);
        couplingR = 1.0f - 2.0f * (float) kPi * 5.0f / srOs;    // liaison entre étages (5 Hz)
        dcR = 1.0f - 2.0f * (float) kPi * 5.0f / sr;
        smoothCoefChunk = std::exp(-(float) kChunk / (0.020f * sr));
        reset();
        cur = target;
        computeDerived();
    }

    void reset() {
        for (int c = 0; c < kMaxChannels; ++c) {
            os[c].reset();
            xInLp[c] = xOutLp[c] = 0.0f;
            cplX1[c] = cplY1[c] = 0.0f;
            dcX1[c] = dcY1[c] = 0.0f;
            dryBuf[c].assign((size_t) latencySamples() + 1, 0.0f);
            adaaPx[c] = adaaTx[c] = 0.0;
            for (float& v : wetDelay[c]) v = 0.0f;
        }
        dryPos = 0;
        wetPos = 0;
    }

    void setParams(const Params& p) { target = p; }
    // suréchantillonneur (32) + 1 : l'anti-repliement (2 x 1/2 échantillon x4) est
    // complété à un échantillon entier par une petite ligne à retard
    int latencySamples() const { return OS::latencyBaseSamples() + 1; }

    void process(float* const* ch, int numChannels, int numSamples) {
        numChannels = std::min(numChannels, kMaxChannels);
        int done = 0;
        while (done < numSamples) {
            const int n = std::min(kChunk, numSamples - done);
            const float prevIn = inGain, prevOut = outGain, prevMix = cur.mix;
            const float a = smoothCoefChunk;
            cur.inputDb  = target.inputDb  + a * (cur.inputDb  - target.inputDb);
            cur.pentode  = target.pentode  + a * (cur.pentode  - target.pentode);
            cur.triode   = target.triode   + a * (cur.triode   - target.triode);
            cur.density  = target.density  + a * (cur.density  - target.density);
            cur.mix      = target.mix      + a * (cur.mix      - target.mix);
            cur.outputDb = target.outputDb + a * (cur.outputDb - target.outputDb);
            computeDerived();
            processChunk(ch, numChannels, done, n, prevIn, prevOut, prevMix);
            done += n;
        }
    }

private:
    static constexpr int kChunk = 32;

    float sr = 48000.0f;
    Params target, cur;
    float smoothCoefChunk = 0.0f;

    // dérivés
    float inGain = 1.0f, outGain = 1.0f;
    float gP = 0.08f, gT = 0.08f, biasT = 0.0f, normT = 1.0f;
    double biasTOut = 0.0;   // en double : tanh(b) - tanh(b) doit valoir exactement 0

    // états
    float xInCoef = 0, xOutCoef = 0, couplingR = 0.9999f, dcR = 0.999f;
    float xInLp[kMaxChannels] = {}, xOutLp[kMaxChannels] = {};
    float cplX1[kMaxChannels] = {}, cplY1[kMaxChannels] = {};
    float dcX1[kMaxChannels] = {}, dcY1[kMaxChannels] = {};
    OS os[kMaxChannels];
    std::vector<float> dryBuf[kMaxChannels];
    int dryPos = 0;
    // anti-repliement par primitive (ADAA) : entrée précédente de chaque étage
    double adaaPx[kMaxChannels] = {}, adaaTx[kMaxChannels] = {};
    static constexpr int kWetDelay = 3;   // + 1 (ADAA) = 4 échantillons x4 = 1 échantillon
    float wetDelay[kMaxChannels][kWetDelay] = {};
    int wetPos = 0;

    void computeDerived() {
        const Tuning& T = tuning;
        const float dens = std::clamp(cur.density, 0.0f, 1.0f);
        const float p = std::clamp(cur.pentode, 0.0f, 1.0f);
        const float t = std::clamp(cur.triode, 0.0f, 1.0f);
        gP = T.driveFloor * dbToGain(T.pentodeRangeDb * p + T.densityRangeDb * dens);
        gT = T.driveFloor * dbToGain(T.stageRangeDb * t + T.densityRangeDb * dens);
        // asymétrie de la triode : nulle à zéro, pleine à fond
        biasT = T.triodeBias * std::sqrt(t);
        biasTOut = std::tanh((double) biasT);
        const float sech = 1.0f / std::cosh(biasT);
        normT = 1.0f / (gT * sech * sech);   // pente unité en petit signal
        inGain = dbToGain(cur.inputDb);
        outGain = dbToGain(cur.outputDb);
    }

    // pentode : f(x) = u / sqrt(1 + u²) / g, u = g x  (symétrique -> harmoniques impaires)
    //   primitive : F(x) = sqrt(1 + u²) / g²
    inline double pentodeF(double x) const {
        const double g = gP, u = g * x;
        return std::sqrt(1.0 + u * u) / (g * g);
    }
    inline double pentodeDirect(double x) const {
        const double u = gP * x;
        return u / std::sqrt(1.0 + u * u) / gP;
    }

    // triode : f(x) = (tanh(g x + b) - tanh b) * norm  (décalée -> harmoniques paires)
    //   primitive : F(x) = (logcosh(g x + b) / g - tanh(b) x) * norm
    static inline double logcosh(double z) {
        const double a = std::fabs(z);
        return a + std::log1p(std::exp(-2.0 * a)) - 0.69314718055994530942;
    }
    inline double triodeF(double x) const {
        return (logcosh((double) gT * x + (double) biasT) / gT - biasTOut * x) * normT;
    }
    inline double triodeDirect(double x) const {
        return (std::tanh((double) gT * x + (double) biasT) - biasTOut) * normT;
    }

    // ADAA du 1er ordre : (F(x) - F(x1)) / (x - x1), ou f au milieu si x ≈ x1
    template <typename Fn, typename Direct>
    static inline float adaa(double x, double& x1, Fn F, Direct f) {
        const double d = x - x1;
        double y;
        if (std::fabs(d) > 1.0e-5) y = (F(x) - F(x1)) / d;
        else                       y = f(0.5 * (x + x1));
        x1 = x;
        return (float) y;
    }

    // transformateur : seul le grave sature, doucement ; reconstruction parfaite au repos
    static inline float xfmr(float x, float& lp, float coef, float amount) {
        lp = x + coef * (lp - x);
        const float low = lp;
        const float a = amount;
        const float sat = (a > 1.0e-4f) ? std::tanh(a * low) / a : low;
        return x - low + sat;
    }

    void processChunk(float* const* ch, int nch, int offset, int n,
                      float inFrom, float outFrom, float mixFrom) {
        const float inTo = inGain, outTo = outGain, mixTo = cur.mix;
        const float invN = 1.0f / (float) n;
        float up4[OS::kFactor];

        for (int i = 0; i < n; ++i) {
            const int s = offset + i;
            const float k = (float) (i + 1) * invN;
            const float gin  = inFrom + (inTo - inFrom) * k;
            const float gout = outFrom + (outTo - outFrom) * k;
            const float mix  = mixFrom + (mixTo - mixFrom) * k;

            for (int c = 0; c < nch; ++c) {
                const float in = ch[c][s];

                std::vector<float>& db = dryBuf[c];
                const int len = (int) db.size();
                db[(size_t) dryPos] = in;
                const float dry = db[(size_t) ((dryPos + 1) % len)];

                float x = xfmr(in * gin, xInLp[c], xInCoef, tuning.xfmrInAmount);

                os[c].up(x, up4);
                auto pF = [this](double v) { return pentodeF(v); };
                auto pD = [this](double v) { return pentodeDirect(v); };
                auto tF = [this](double v) { return triodeF(v); };
                auto tD = [this](double v) { return triodeDirect(v); };
                int wp = wetPos;
                for (int p = 0; p < OS::kFactor; ++p) {
                    float v = adaa(up4[p], adaaPx[c], pF, pD);
                    // condensateur de liaison : retire le continu avant la triode
                    const float hp = v - cplX1[c] + couplingR * cplY1[c];
                    cplX1[c] = v; cplY1[c] = hp;
                    const float tv = adaa(hp, adaaTx[c], tF, tD);
                    // complète le retard de l'ADAA à un échantillon de base entier
                    up4[p] = wetDelay[c][wp];
                    wetDelay[c][wp] = tv;
                    wp = (wp + 1 == kWetDelay) ? 0 : wp + 1;
                }
                if (c == nch - 1) wetPos = wp;
                x = os[c].down(up4);

                x = xfmr(x, xOutLp[c], xOutCoef, tuning.xfmrOutAmount);

                // retire le continu laissé par l'asymétrie de la triode
                const float y = x - dcX1[c] + dcR * dcY1[c];
                dcX1[c] = x; dcY1[c] = y;

                // le sec n'a pas le gain d'entrée : on compense pour que Mix compare à niveau égal
                ch[c][s] = (dry + mix * (y / std::max(gin, 1.0e-6f) - dry)) * gout;
            }
            dryPos = (dryPos + 1) % (int) dryBuf[0].size();
        }
    }
};

} // namespace koq
