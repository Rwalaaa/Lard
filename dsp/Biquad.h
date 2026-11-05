// Biquad.h — filtre biquad partagé par les moteurs (C++ pur).
#pragma once
#include <cmath>
#include <algorithm>
#include "Oversampler4x.h"

namespace lard {

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
    // étagère basse (RBJ), gain en dB, pente S
    void setLowShelf(double fs, double f0, double gainDb, double S) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * std::min(f0, 0.49 * fs) / fs;
        const double c = std::cos(w0), s = std::sin(w0);
        const double alpha = s / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
        const double sa = 2.0 * std::sqrt(A) * alpha;
        const double a0 = (A + 1) + (A - 1) * c + sa;
        b0 = (float) (A * ((A + 1) - (A - 1) * c + sa) / a0);
        b1 = (float) (2 * A * ((A - 1) - (A + 1) * c) / a0);
        b2 = (float) (A * ((A + 1) - (A - 1) * c - sa) / a0);
        a1 = (float) (-2 * ((A - 1) + (A + 1) * c) / a0);
        a2 = (float) (((A + 1) + (A - 1) * c - sa) / a0);
    }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    // passe-haut / passe-bas du 2e ordre (RBJ), facteur de qualité Q
    void setHighPass(double fs, double f0, double Q) { setPass(fs, f0, Q, true); }
    void setLowPass(double fs, double f0, double Q)  { setPass(fs, f0, Q, false); }

    // cloche (RBJ), gain en dB
    void setPeak(double fs, double f0, double gainDb, double Q) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * std::min(f0, 0.49 * fs) / fs;
        const double c = std::cos(w0), alpha = std::sin(w0) / (2.0 * Q);
        const double a0 = 1.0 + alpha / A;
        b0 = (float) ((1.0 + alpha * A) / a0);
        b1 = (float) (-2.0 * c / a0);
        b2 = (float) ((1.0 - alpha * A) / a0);
        a1 = (float) (-2.0 * c / a0);
        a2 = (float) ((1.0 - alpha / A) / a0);
    }

    void setPass(double fs, double f0, double Q, bool high) {
        const double w0 = 2.0 * kPi * std::min(f0, 0.49 * fs) / fs;
        const double c = std::cos(w0), alpha = std::sin(w0) / (2.0 * Q);
        const double a0 = 1.0 + alpha;
        const double k = high ? (1.0 + c) / 2.0 : (1.0 - c) / 2.0;
        b0 = (float) (k / a0);
        b1 = (float) ((high ? -2.0 * k : 2.0 * k) / a0);
        b2 = (float) (k / a0);
        a1 = (float) (-2.0 * c / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    // étagère haute (RBJ), gain en dB, pente S
    void setHighShelf(double fs, double f0, double gainDb, double S) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * std::min(f0, 0.49 * fs) / fs;
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

} // namespace lard
