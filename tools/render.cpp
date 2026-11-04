// render.cpp — applique LardEngine à un WAV float32 (mono/stéréo), pour les tests.
// Usage : render in.wav out.wav fatness color gainDb mix [blockSize]
#include "../dsp/LardEngine.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <chrono>

#include "WavIO.h"

int main(int argc, char** argv) {
    if (argc < 7) { std::fprintf(stderr, "usage: render in out fat color gainDb mix [block]\n"); return 2; }
    Wav w;
    if (!readWav(argv[1], w) || w.ch < 1 || w.ch > 2) { std::fprintf(stderr, "lecture WAV impossible (float32 mono/stéréo attendu)\n"); return 1; }
    lard::Params p;
    p.fatness = (float)std::atof(argv[3]);
    p.color   = (float)std::atof(argv[4]);
    p.gainDb  = (float)std::atof(argv[5]);
    p.mix     = (float)std::atof(argv[6]);
    const int block = argc > 7 ? std::atoi(argv[7]) : 512;

    lard::Engine eng;
    // surcharges de voicing pour les essais : LARD_driveMax=6 etc.
    struct { const char* name; float* v; } tune[] = {
        {"compThrRangeDb", &eng.tuning.compThrRangeDb}, {"compRatioMax", &eng.tuning.compRatioMax},
        {"compMakeupRefDb", &eng.tuning.compMakeupRefDb}, {"compMakeupAmt", &eng.tuning.compMakeupAmt},
        {"driveMax", &eng.tuning.driveMax}, {"drivePow", &eng.tuning.drivePow},
        {"biasMax", &eng.tuning.biasMax}, {"satMakeupDb", &eng.tuning.satMakeupDb},
        {"satMakeupPow", &eng.tuning.satMakeupPow}, {"ceilKneeLo", &eng.tuning.ceilKneeLo},
        {"ceilKneeHi", &eng.tuning.ceilKneeHi}, {"satOutAtMax", &eng.tuning.satOutAtMax},
    };
    for (auto& t : tune) {
        char key[64]; std::snprintf(key, sizeof key, "LARD_%s", t.name);
        if (const char* s = std::getenv(key)) *t.v = (float)std::atof(s);
    }
    eng.setParams(p);
    eng.prepare(w.sr);

    const size_t frames = w.data.size() / (size_t)w.ch;
    std::vector<float> L(frames), R(frames);
    for (size_t i = 0; i < frames; ++i) {
        L[i] = w.data[i * w.ch];
        R[i] = w.data[i * w.ch + (w.ch - 1)];
    }
    auto t0 = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos < frames; pos += (size_t)block) {
        const int n = (int)std::min((size_t)block, frames - pos);
        float* chans[2] = { L.data() + pos, R.data() + pos };
        eng.process(chans, w.ch, n);
    }
    auto t1 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < frames; ++i) {
        w.data[i * w.ch] = L[i];
        if (w.ch == 2) w.data[i * w.ch + 1] = R[i];
    }
    if (!writeWav(argv[2], w)) return 1;
    const double secs = std::chrono::duration<double>(t1 - t0).count();
    const double audioSecs = (double)frames / w.sr;
    std::printf("latency=%d cpu_pct=%.3f\n", eng.latencySamples(), 100.0 * secs / audioSecs);
    return 0;
}
