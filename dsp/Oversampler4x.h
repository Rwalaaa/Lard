// Oversampler4x.h — outils communs aux moteurs Lard et Koquille (C++ pur).
// Suréchantillonneur x4 à phase linéaire + conversions dB.
#pragma once
#include <cmath>
#include <algorithm>
#include <iterator>

namespace lard {

constexpr double kPi = 3.14159265358979323846;

static inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
static inline float gainToDb(float g)  { return 20.0f * std::log10(std::max(g, 1.0e-9f)); }

// Entrée assainie : un seul NaN ou infini venu d'un autre plugin bloquerait définitivement les
// filtres (états non finis). Remplacé par 0 ; valeurs finies bornées à +60 dBFS.
static inline void sanitizeInput(float* x, int n) {
    for (int i = 0; i < n; ++i) {
        const float v = x[i];
        x[i] = std::isfinite(v) ? std::clamp(v, -1000.0f, 1000.0f) : 0.0f;
    }
}

// ---------------------------------------------------------------------------
// Suréchantillonneur x4, FIR à phase linéaire (sinc fenêtré Kaiser).
// Latence : (N-1)/2 échantillons x4 à la montée + idem à la descente
//         = (N-1)/4 échantillons à la fréquence de base (32 pour N = 129).
class Oversampler4x {
public:
    static constexpr int kFactor = 4;
    static constexpr int kTaps   = 129;
    static constexpr int kPhaseLen = (kTaps + kFactor - 1) / kFactor; // 33

    Oversampler4x() { design(); reset(); }

    void reset() {
        std::fill(std::begin(upHist), std::end(upHist), 0.0f);
        std::fill(std::begin(dnHist), std::end(dnHist), 0.0f);
        std::fill(std::begin(pending), std::end(pending), 0.0f);
        upPos = 0; dnPos = 0;
    }

    static constexpr int latencyBaseSamples() { return (kTaps - 1) / kFactor; }

    // 1 échantillon d'entrée -> 4 échantillons suréchantillonnés.
    // Historiques doublés : chaque échantillon est écrit deux fois pour que
    // la lecture soit toujours contiguë (pas de modulo, boucle vectorisable).
    void up(float x, float* out4) {
        upPos = (upPos == 0) ? kPhaseLen - 1 : upPos - 1;
        upHist[upPos] = x;
        upHist[upPos + kPhaseLen] = x;
        const float* hist = upHist + upPos;
        for (int p = 0; p < kFactor; ++p) {
            const float* hp = hPoly[p];
            float acc = 0.0f;
            for (int k = 0; k < kPhaseLen; ++k) acc += hp[k] * hist[k];
            out4[p] = acc;
        }
    }

    // 4 échantillons suréchantillonnés -> 1 échantillon de sortie.
    // Décimation sur la phase 0 (retard entier) : on pousse les phases 1..3
    // du bloc précédent, puis la phase 0 du bloc courant, avant le calcul.
    float down(const float* in4) {
        const float seq[kFactor] = { pending[0], pending[1], pending[2], in4[0] };
        for (int p = 0; p < kFactor; ++p) {
            dnPos = (dnPos == 0) ? kTaps - 1 : dnPos - 1;
            dnHist[dnPos] = seq[p];
            dnHist[dnPos + kTaps] = seq[p];
        }
        pending[0] = in4[1]; pending[1] = in4[2]; pending[2] = in4[3];
        const float* hist = dnHist + dnPos;
        float acc = 0.0f;
        for (int j = 0; j < kTaps; ++j) acc += h[j] * hist[j];
        return acc;
    }

private:
    float h[kTaps];
    float hPoly[kFactor][kPhaseLen];   // phases du filtre de montée, gain x4 inclus
    float upHist[2 * kPhaseLen];
    float dnHist[2 * kTaps];
    float pending[kFactor - 1] = {};
    int upPos = 0, dnPos = 0;

    static double besselI0(double x) {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k) {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1e-12 * sum) break;
        }
        return sum;
    }

    void design() {
        const double fc = 0.115;   // coupure, en cycles/échantillon au rythme x4 (≈0.46 x fs de base)
        const double beta = 8.0;
        const int M = kTaps - 1;
        const double i0b = besselI0(beta);
        double sum = 0.0;
        for (int j = 0; j < kTaps; ++j) {
            const double n = j - M / 2.0;
            const double sinc = (2 * j == M) ? 2.0 * fc
                                           : std::sin(2.0 * kPi * fc * n) / (kPi * n);
            const double r = 2.0 * j / M - 1.0;
            const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0b;
            h[j] = (float)(sinc * w);
            sum += sinc * w;
        }
        for (int j = 0; j < kTaps; ++j) h[j] = (float)(h[j] / sum); // gain DC = 1
        for (int p = 0; p < kFactor; ++p)
            for (int k = 0; k < kPhaseLen; ++k) {
                const int j = p + kFactor * k;
                hPoly[p][k] = (j < kTaps) ? h[j] * (float)kFactor : 0.0f;
            }
    }
};

} // namespace lard
