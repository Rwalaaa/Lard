// LardEngine.h — moteur audio du plugin "Lard" (C++ pur, sans dépendance).
// Chaîne : compression -> couleur (tilt) -> saturation x4 suréchantillonnée
//          -> plafond doux -> anti-couleur partielle -> DC block -> mix -> gain.
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace lard {

constexpr double kPi = 3.14159265358979323846;

static inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
static inline float gainToDb(float g)  { return 20.0f * std::log10(std::max(g, 1.0e-9f)); }

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

// ---------------------------------------------------------------------------
// Constantes de voicing : ajustables pour les réglages à l'oreille.
struct Tuning {
    float compThrRangeDb = 14.0f;   // descente du seuil de -6 dB jusqu'à -6-14 dB
    float compRatioMax   = 4.5f;
    float compMakeupRefDb = -8.0f;  // niveau de référence pour le gain de compensation
    float compMakeupAmt  = 1.0f;    // 1 = compensation complète à ce niveau
    float driveMax       = 7.0f;
    float drivePow       = 1.6f;
    float biasMax        = 0.15f;   // dans l'argument de tanh (indépendant du drive)
    float satMakeupDb    = 0.0f;    // gain additionnel avant plafond (0 = plafond en sécurité seule)
    float satMakeupPow   = 1.3f;
    float satOutAtMax    = 0.85f;   // niveau de sortie de la saturation à Fatness = 1
    float ceilKneeLo     = 0.70f;   // genou du plafond à Fatness = 1
    float ceilKneeHi     = 0.95f;   // genou du plafond à Fatness = 0
};

struct Params {
    float fatness = 0.0f;  // 0..1
    float color   = 0.5f;  // 0..1, 0.5 = neutre
    float gainDb  = 0.0f;  // -24..+12
    float mix     = 1.0f;  // 0..1
};

class Engine {
public:
    static constexpr int kMaxChannels = 2;

    Tuning tuning;

    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        const float srOs = sr * Oversampler4x::kFactor;
        atkCoef = std::exp(-1.0f / (0.008f * sr));   // 8 ms
        relCoef = std::exp(-1.0f / (0.120f * sr));   // 120 ms
        tiltCoef = std::exp(-2.0f * (float)kPi * 700.0f / sr);
        tiltCoefOs = std::exp(-2.0f * (float)kPi * 700.0f / srOs);
        dcR = 1.0f - 2.0f * (float)kPi * 5.0f / srOs;   // tourne au rythme x4
        smoothCoefChunk = std::exp(-(float)kChunk / (0.020f * sr)); // ~20 ms
        dryDelayLen = Oversampler4x::latencyBaseSamples();
        reset();
        // les valeurs lissées partent directement des cibles
        cur = target;
        computeDerived();
    }

    void reset() {
        for (int c = 0; c < kMaxChannels; ++c) {
            os[c].reset();
            tiltPre[c] = tiltPost[c] = 0.0f;
            dcX1[c] = dcY1[c] = 0.0f;
            dryBuf[c].assign((size_t)Oversampler4x::latencyBaseSamples() + 1, 0.0f);
        }
        dryPos = 0;
        envDb = -120.0f;
        grDb = 0.0f;
    }

    void setParams(const Params& p) { target = p; }

    int latencySamples() const { return Oversampler4x::latencyBaseSamples(); }

    // Réduction de gain actuelle (dB, <= 0), utile pour un vu-mètre.
    float currentGainReductionDb() const { return -grDb; }

    // Traitement en place. numChannels = 1 ou 2.
    void process(float* const* ch, int numChannels, int numSamples) {
        numChannels = std::min(numChannels, kMaxChannels);
        int done = 0;
        while (done < numSamples) {
            const int n = std::min(kChunk, numSamples - done);
            // lissage des paramètres par blocs de 32 échantillons
            const float prevGain = outGain, prevMix = cur.mix;
            const float a = smoothCoefChunk;
            cur.fatness = target.fatness + a * (cur.fatness - target.fatness);
            cur.color   = target.color   + a * (cur.color   - target.color);
            cur.gainDb  = target.gainDb  + a * (cur.gainDb  - target.gainDb);
            cur.mix     = target.mix     + a * (cur.mix     - target.mix);
            computeDerived();
            processChunk(ch, numChannels, done, n, prevGain, prevMix);
            done += n;
        }
    }

private:
    static constexpr int kChunk = 32;

    float sr = 48000.0f;
    Params target, cur;
    float smoothCoefChunk = 0.0f;

    // dérivés
    float thrDb = 0, ratio = 1, kneeDb = 6, compMakeupDb = 0;
    float tiltLowPre = 1, tiltHighPre = 1, tiltLowPost = 1, tiltHighPost = 1;
    float drive = 0.001f, bias = 0, biasOut = 0, satNorm = 1, satMakeup = 1;
    float ceilK = 0.95f;
    float outGain = 1;

    // états
    float atkCoef = 0, relCoef = 0, tiltCoef = 0, tiltCoefOs = 0, dcR = 0.999f;
    float envDb = -120.0f, grDb = 0.0f;
    float tiltPre[kMaxChannels] = {}, tiltPost[kMaxChannels] = {};
    float dcX1[kMaxChannels] = {}, dcY1[kMaxChannels] = {};
    Oversampler4x os[kMaxChannels];
    std::vector<float> dryBuf[kMaxChannels];
    int dryPos = 0, dryDelayLen = 0;

    void computeDerived() {
        const float f = std::clamp(cur.fatness, 0.0f, 1.0f);
        const Tuning& T = tuning;
        // Compression : seuil qui descend, ratio qui monte avec Fatness
        thrDb = -6.0f - T.compThrRangeDb * f;
        ratio = 1.0f + (T.compRatioMax - 1.0f) * f;
        compMakeupDb = T.compMakeupAmt * std::max(0.0f, T.compMakeupRefDb - thrDb)
                       * (1.0f - 1.0f / ratio);

        // Couleur : tilt autour de 700 Hz, ±7,5 dB avant saturation, moitié inversée après
        const float t = std::clamp(cur.color, 0.0f, 1.0f) * 2.0f - 1.0f;
        tiltLowPre  = dbToGain(-7.5f * t);
        tiltHighPre = dbToGain(+7.5f * t);
        // atténuation seule : l'anti-couleur ne peut pas faire remonter les crêtes
        tiltLowPost  = dbToGain(std::min(0.0f, +3.75f * t));
        tiltHighPost = dbToGain(std::min(0.0f, -3.75f * t));

        // Saturation : la charge monte plus vite dans la 2e moitié de la course
        drive = 0.001f + T.driveMax * std::pow(f, T.drivePow);
        bias  = T.biasMax * f;                   // décalage dans l'argument de tanh -> harmoniques paires
        biasOut = std::tanh(bias);
        // tanh normalisée : pleine échelle -> pleine échelle, les petits signaux
        // gagnent drive/tanh(drive) -> plus de densité sans dépasser 0 dBFS
        // dénominateur = excursion max (côté négatif, à cause du biais) pour |x| = 1
        satNorm = (1.0f + (T.satOutAtMax - 1.0f) * f)
                / (std::tanh(drive - bias) + biasOut);
        satMakeup = dbToGain(T.satMakeupDb * std::pow(f, T.satMakeupPow));
        ceilK = T.ceilKneeHi + (T.ceilKneeLo - T.ceilKneeHi) * f;

        outGain = dbToGain(cur.gainDb);
    }

    inline float shaper(float x) const {
        return (std::tanh(drive * x + bias) - biasOut) * satNorm;
    }

    inline float ceilingClip(float x) const {
        const float c = 0.966f;   // -0,3 dBFS
        const float k = ceilK;
        const float ax = std::fabs(x);
        if (ax <= k) return x;
        const float y = k + (c - k) * std::tanh((ax - k) / (c - k));
        return x < 0.0f ? -y : y;
    }

    void processChunk(float* const* ch, int nch, int offset, int n,
                      float gainFrom, float mixFrom) {
        const float gainTo = outGain, mixTo = cur.mix;
        const float invN = 1.0f / (float)n;
        float up4[Oversampler4x::kFactor];

        for (int i = 0; i < n; ++i) {
            const int s = offset + i;
            const float interp = (float)(i + 1) * invN;
            const float g = gainFrom + (gainTo - gainFrom) * interp;
            const float mix = mixFrom + (mixTo - mixFrom) * interp;

            // --- détecteur stéréo lié ---
            float peak = 0.0f;
            for (int c = 0; c < nch; ++c) peak = std::max(peak, std::fabs(ch[c][s]));
            const float lvl = gainToDb(peak);
            envDb = (lvl > envDb) ? lvl + atkCoef * (envDb - lvl)
                                  : lvl + relCoef * (envDb - lvl);
            // courbe à genou doux
            float over = envDb - thrDb;
            float targetGr;
            if (2.0f * over < -kneeDb)      targetGr = 0.0f;
            else if (2.0f * over > kneeDb)  targetGr = over * (1.0f - 1.0f / ratio);
            else {
                const float x = over + kneeDb * 0.5f;
                targetGr = (1.0f - 1.0f / ratio) * x * x / (2.0f * kneeDb);
            }
            grDb = (targetGr > grDb) ? targetGr + atkCoef * (grDb - targetGr)
                                     : targetGr + relCoef * (grDb - targetGr);
            const float compGain = dbToGain(compMakeupDb - grDb);

            for (int c = 0; c < nch; ++c) {
                const float in = ch[c][s];

                // dry retardé pour rester en phase avec la voie traitée
                std::vector<float>& db = dryBuf[c];
                const int len = (int)db.size();
                db[(size_t)dryPos] = in;
                const float dry = db[(size_t)((dryPos + 1) % len)];

                float x = in * compGain;

                // tilt avant saturation
                tiltPre[c] = x + tiltCoef * (tiltPre[c] - x);
                x = tiltPre[c] * tiltLowPre + (x - tiltPre[c]) * tiltHighPre;

                // au rythme x4 : saturation -> anti-tilt -> DC block -> plafond
                // (le plafond reste le dernier étage, donc rien ne le dépasse ensuite)
                os[c].up(x, up4);
                for (int p = 0; p < Oversampler4x::kFactor; ++p) {
                    float v = shaper(up4[p]) * satMakeup;
                    tiltPost[c] = v + tiltCoefOs * (tiltPost[c] - v);
                    v = tiltPost[c] * tiltLowPost + (v - tiltPost[c]) * tiltHighPost;
                    // le biais d'asymétrie crée du continu
                    const float d = v - dcX1[c] + dcR * dcY1[c];
                    dcX1[c] = v; dcY1[c] = d;
                    up4[p] = ceilingClip(d);
                }
                const float y = os[c].down(up4);

                ch[c][s] = (dry + mix * (y - dry)) * g;
            }
            dryPos = (dryPos + 1) % (int)dryBuf[0].size();
        }
    }
};

} // namespace lard
