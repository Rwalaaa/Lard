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
    // voie de saturation parallèle (sur une bande choisie)
    float satAmount = 0.0f;  // 0..1
    int   satType   = 0;     // 0 = douce, 1 = agressive
    int   satBand   = 1;     // 0 = grave 100 Hz, 1 = bas-médium 400 Hz, 2 = médium 1,5 kHz, 3 = aigu 5 kHz
    int   calibration = 1;   // 0 = sombre, 1 = normal, 2 = brillant
    float air      = 0.0f;   // 0..1 : étagère au-dessus de 12 kHz, jusqu'à +6 dB
    bool  autoGain = true;   // compense le niveau du traité pour comparer à volume égal
};

// Filtre biquad (forme transposée II), états par canal
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = {}, z2[2] = {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
    inline float process(float x, int c) {
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    // étagère haute (RBJ), gain en dB, pente S
    void setHighShelf(double fs, double f0, double gainDb, double S) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * lard::kPi * std::min(f0, 0.49 * fs) / fs;
        const double c = std::cos(w0), s = std::sin(w0);
        const double alpha = s / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
        const double sa = 2.0 * std::sqrt(A) * alpha;
        const double a0 = (A + 1) - (A - 1) * c + sa;
        b0 = (float) (A * ((A + 1) + (A - 1) * c + sa) / a0);
        b1 = (float) (-2 * A * ((A - 1) + (A + 1) * c) / a0);
        b2 = (float) (A * ((A + 1) + (A - 1) * c - sa) / a0);
        a1 = (float) (2 * ((A - 1) - (A + 1) * c) / a0);
        a2 = (float) (((A + 1) - (A - 1) * c - sa) / a0);
    }
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
    // voie parallèle
    float satDriveRangeDb = 22.0f; // attaque max de la bande saturée
    float satLevel        = 0.6f;  // niveau ajouté à fond
    float satQ            = 0.8f;
    float satAggroBias    = 0.5f;  // asymétrie du caractère « agressive »
    float satAggroExtraDb = 6.0f;  // et attaque supplémentaire
    // calibration, air, compensation de l'aigu
    float calShelfDb  = 2.5f, calShelfHz = 6000.0f;
    float airMaxDb    = 6.0f, airHz = 12000.0f;
    float compShelfHz = 14500.0f, compShelfDb = 1.0f, compShelfS = 0.5f; // aplatit l'aigu jusqu'à 16 kHz
    // compensation de gain automatique
    float autoGainTau = 0.6f;      // fenêtre de mesure (s)
    float autoGainMaxDb = 12.0f;
};

class Engine {
public:
    static constexpr int kMaxChannels = 2;
    using OS = lard::Oversampler4x;

    Tuning tuning;

    void prepare(double sampleRate) {
        sr = (float) sampleRate;
        srOs = sr * OS::kFactor;
        xInCoef  = std::exp(-2.0f * (float) kPi * tuning.xfmrInHz / sr);
        xOutCoef = std::exp(-2.0f * (float) kPi * tuning.xfmrOutHz / sr);
        couplingR = 1.0f - 2.0f * (float) kPi * 5.0f / srOs;    // liaison entre étages (5 Hz)
        dcR = 1.0f - 2.0f * (float) kPi * 5.0f / sr;
        smoothCoefChunk = std::exp(-(float) kChunk / (0.020f * sr));
        powCoef = std::exp(-1.0 / ((double) tuning.autoGainTau * sr));
        agCoef = std::exp(-1.0f / (0.3f * sr));
        colCoef = std::exp(-1.0 / (0.3 * (double) sr));
        compShelf.setHighShelf(sr, tuning.compShelfHz, tuning.compShelfDb, tuning.compShelfS);
        copyCoefs(compRef, compShelf);
        refR1 = 1.0f - 2.0f * (float) kPi * 5.0f / sr;
        lastBand = lastCal = -1;
        lastAir = -1.0f;
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
            adaaPx[c] = adaaTx[c] = adaaSx[c] = 0.0;
            svfIc1[c] = svfIc2[c] = 0.0f;
            for (float& v : wetDelay[c]) v = 0.0f;
        }
        dryPos = 0;
        wetPos = 0;
        compShelf.reset(); calShelf.reset(); airShelf.reset();
        pIn = pWet = 0.0;
        agGain = 1.0f;
        pColDiff = pColIn = 0.0;
        compRef.reset(); calRef.reset(); airRef.reset();
        for (int c = 0; c < kMaxChannels; ++c) refHp1X[c] = refHp1Y[c] = refHp2X[c] = refHp2Y[c] = 0.0f;
    }

    void setParams(const Params& p) { target = p; }

    // « couleur » ajoutée : écart traité / sec, relatif à l'entrée (dB, lissé ~0,3 s)
    float colourDb() const {
        if (pColIn < 1.0e-9) return -120.0f;
        return (float) (10.0 * std::log10(std::max(pColDiff / pColIn, 1.0e-12)));
    }
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
            cur.satAmount = target.satAmount + a * (cur.satAmount - target.satAmount);
            cur.air      = target.air      + a * (cur.air      - target.air);
            cur.satType = target.satType; cur.satBand = target.satBand;
            cur.calibration = target.calibration; cur.autoGain = target.autoGain;
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

    // voie parallèle : passe-bande à variables d'état (au rythme x4) + saturation ADAA
    float srOs = 192000.0f;
    float svfA1 = 1, svfA2 = 0, svfA3 = 0, svfK = 1.25f;
    float svfIc1[kMaxChannels] = {}, svfIc2[kMaxChannels] = {};
    double adaaSx[kMaxChannels] = {};
    float satDrive = 1.0f, satGainOut = 0.0f, satBiasS = 0.0f, satNormS = 1.0f;
    double satBiasOut = 0.0;
    int lastBand = -1, lastCal = -1;
    float lastAir = -1.0f;

    // couleur de sortie
    Biquad compShelf, calShelf, airShelf;
    bool calActive = false, airActive = false;

    // compensation de gain automatique
    double pIn = 0.0, pWet = 0.0, powCoef = 0.9999;
    float agGain = 1.0f, agCoef = 0.9999f;

    // mesure de couleur (pour l'indicateur à aiguille) : le sec passe par les mêmes
    // filtres linéaires que le traité, pour ne mesurer que l'apport non linéaire
    double pColDiff = 0.0, pColIn = 0.0, colCoef = 0.9999;
    float refHp1X[kMaxChannels] = {}, refHp1Y[kMaxChannels] = {};
    float refHp2X[kMaxChannels] = {}, refHp2Y[kMaxChannels] = {};
    float refR1 = 0.9999f;
    Biquad compRef, calRef, airRef;
    static void copyCoefs(Biquad& dst, const Biquad& src) {
        dst.b0 = src.b0; dst.b1 = src.b1; dst.b2 = src.b2; dst.a1 = src.a1; dst.a2 = src.a2;
    }
    inline float referenceOf(float dry, int c) {
        float y = dry - refHp1X[c] + refR1 * refHp1Y[c];   // liaison (5 Hz)
        refHp1X[c] = dry; refHp1Y[c] = y;
        float x = compRef.process(y, c);
        if (calActive) x = calRef.process(x, c);
        if (airActive) x = airRef.process(x, c);
        const float z = x - refHp2X[c] + dcR * refHp2Y[c];  // anti-continu (5 Hz)
        refHp2X[c] = x; refHp2Y[c] = z;
        return z;
    }

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

        // voie parallèle
        const float sa = std::clamp(cur.satAmount, 0.0f, 1.0f);
        const bool aggro = cur.satType == 1;
        satDrive = dbToGain(T.satDriveRangeDb * sa + (aggro ? T.satAggroExtraDb * sa : 0.0f));
        satGainOut = T.satLevel * sa;
        satBiasS = aggro ? T.satAggroBias : 0.0f;
        satBiasOut = std::tanh((double) satBiasS);
        const float sechS = 1.0f / std::cosh(satBiasS);
        satNormS = 1.0f / (satDrive * sechS * sechS);
        const int band = std::clamp(cur.satBand, 0, 3);
        if (band != lastBand) {
            static const float bandHz[4] = { 100.0f, 400.0f, 1500.0f, 5000.0f };
            const float g = std::tan((float) kPi * bandHz[band] / srOs);
            svfK = 1.0f / T.satQ;
            svfA1 = 1.0f / (1.0f + g * (g + svfK));
            svfA2 = g * svfA1;
            svfA3 = g * svfA2;
            lastBand = band;
        }

        // calibration et air (recalculés seulement s'ils changent)
        const int cal = std::clamp(cur.calibration, 0, 2);
        if (cal != lastCal) {
            calActive = cal != 1;
            calShelf.setHighShelf(sr, T.calShelfHz, cal == 0 ? -T.calShelfDb : T.calShelfDb, 0.7);
            copyCoefs(calRef, calShelf);
            lastCal = cal;
        }
        const float air = std::clamp(cur.air, 0.0f, 1.0f);
        if (std::fabs(air - lastAir) > 1.0e-4f) {
            airActive = air > 1.0e-4f;
            airShelf.setHighShelf(sr, T.airHz, T.airMaxDb * air, 0.8);
            copyCoefs(airRef, airShelf);
            lastAir = air;
        }
    }

    // saturation de la voie parallèle : tanh (douce) ou tanh décalée (agressive), pente unité
    inline double satF(double x) const {
        return (logcosh((double) satDrive * x + satBiasS) / satDrive - satBiasOut * x) * satNormS;
    }
    inline double satDirect(double x) const {
        return (std::tanh((double) satDrive * x + satBiasS) - satBiasOut) * satNormS;
    }

    // passe-bande normalisé (gain 1 au centre), au rythme x4
    inline float bandpass(float x, int c) {
        const float v3 = x - svfIc2[c];
        const float v1 = svfA1 * svfIc1[c] + svfA2 * v3;
        const float v2 = svfIc2[c] + svfA2 * svfIc1[c] + svfA3 * v3;
        svfIc1[c] = 2.0f * v1 - svfIc1[c];
        svfIc2[c] = 2.0f * v2 - svfIc2[c];
        return svfK * v1;
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

            float dryV[kMaxChannels] = {}, wetV[kMaxChannels] = {}, refV[kMaxChannels] = {};
            float powIn = 0.0f, powWet = 0.0f;
            const bool satOn = satGainOut > 1.0e-5f;

            for (int c = 0; c < nch; ++c) {
                const float in = ch[c][s];

                std::vector<float>& db = dryBuf[c];
                const int len = (int) db.size();
                db[(size_t) dryPos] = in;
                dryV[c] = db[(size_t) ((dryPos + 1) % len)];

                float x = xfmr(in * gin, xInLp[c], xInCoef, tuning.xfmrInAmount);

                os[c].up(x, up4);
                auto pF = [this](double v) { return pentodeF(v); };
                auto pD = [this](double v) { return pentodeDirect(v); };
                auto tF = [this](double v) { return triodeF(v); };
                auto tD = [this](double v) { return triodeDirect(v); };
                auto sF = [this](double v) { return satF(v); };
                auto sD = [this](double v) { return satDirect(v); };
                int wp = wetPos;
                for (int p = 0; p < OS::kFactor; ++p) {
                    float v = adaa(up4[p], adaaPx[c], pF, pD);
                    // condensateur de liaison : retire le continu avant la triode
                    const float hp = v - cplX1[c] + couplingR * cplY1[c];
                    cplX1[c] = v; cplY1[c] = hp;
                    float tv = adaa(hp, adaaTx[c], tF, tD);
                    // voie parallèle : même point de départ et même retard ADAA que la triode
                    const float bp = bandpass(hp, c);
                    if (satOn) tv += satGainOut * adaa(bp, adaaSx[c], sF, sD);
                    else       adaaSx[c] = bp;
                    // complète le retard de l'ADAA à un échantillon de base entier
                    up4[p] = wetDelay[c][wp];
                    wetDelay[c][wp] = tv;
                    wp = (wp + 1 == kWetDelay) ? 0 : wp + 1;
                }
                if (c == nch - 1) wetPos = wp;
                x = os[c].down(up4);

                x = xfmr(x, xOutLp[c], xOutCoef, tuning.xfmrOutAmount);

                // couleur de sortie : aigu aplati, calibration, air
                x = compShelf.process(x, c);
                if (calActive) x = calShelf.process(x, c);
                if (airActive) x = airShelf.process(x, c);

                // retire le continu laissé par l'asymétrie de la triode
                const float y = x - dcX1[c] + dcR * dcY1[c];
                dcX1[c] = x; dcY1[c] = y;

                // le sec n'a pas le gain d'entrée : on compense pour que Mix compare à niveau égal
                wetV[c] = y / std::max(gin, 1.0e-6f);
                // référence = sec passé par les mêmes filtres linéaires : sert au gain auto
                // (on ne compense pas l'infra-grave retiré) et à la mesure de couleur
                refV[c] = referenceOf(dryV[c], c);
                powIn += refV[c] * refV[c];
                powWet += wetV[c] * wetV[c];
            }
            dryPos = (dryPos + 1) % (int) dryBuf[0].size();

            // compensation de gain : même puissance moyenne que l'entrée (gain commun aux canaux)
            pIn  = powIn  + powCoef * (pIn  - powIn);
            pWet = powWet + powCoef * (pWet - powWet);
            float agTarget = 1.0f;
            if (cur.autoGain && pIn > 1.0e-7 && pWet > 1.0e-12) {
                const float lim = dbToGain(tuning.autoGainMaxDb);
                agTarget = std::clamp((float) std::sqrt(pIn / pWet), 1.0f / lim, lim);
            } else if (cur.autoGain) {
                agTarget = agGain;   // silence : on garde le dernier gain
            }
            agGain = agTarget + agCoef * (agGain - agTarget);

            float powDiff = 0.0f, powDry = 0.0f;
            for (int c = 0; c < nch; ++c) {
                const float wet = wetV[c] * agGain;
                const float d = wet - refV[c];
                powDiff += d * d;
                powDry += refV[c] * refV[c];
                ch[c][s] = (dryV[c] + mix * (wet - dryV[c])) * gout;
            }
            pColDiff = powDiff + colCoef * (pColDiff - powDiff);
            pColIn   = powDry  + colCoef * (pColIn   - powDry);
        }
    }
};

} // namespace koq
