// render.cpp — applique LardEngine à un WAV float32 (mono/stéréo), pour les tests.
// Usage : render in.wav out.wav fatness color gainDb mix [blockSize]
#include "../dsp/LardEngine.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <chrono>

struct Wav { int sr = 0, ch = 0; std::vector<float> data; };

static bool readWav(const char* path, Wav& w) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char riff[12];
    if (std::fread(riff, 1, 12, f) != 12 || std::memcmp(riff, "RIFF", 4) || std::memcmp(riff + 8, "WAVE", 4)) { std::fclose(f); return false; }
    uint16_t fmtTag = 0, bits = 0;
    bool gotFmt = false;
    while (true) {
        char id[4]; uint32_t size;
        if (std::fread(id, 1, 4, f) != 4 || std::fread(&size, 4, 1, f) != 1) break;
        if (!std::memcmp(id, "fmt ", 4)) {
            std::vector<uint8_t> b(size);
            if (std::fread(b.data(), 1, size, f) != size) break;
            std::memcpy(&fmtTag, &b[0], 2);
            uint16_t c; std::memcpy(&c, &b[2], 2); w.ch = c;
            uint32_t s; std::memcpy(&s, &b[4], 4); w.sr = (int)s;
            std::memcpy(&bits, &b[14], 2);
            if (fmtTag == 0xFFFE && size >= 26) std::memcpy(&fmtTag, &b[24], 2);
            gotFmt = true;
        } else if (!std::memcmp(id, "data", 4)) {
            if (!gotFmt || fmtTag != 3 || bits != 32) { std::fclose(f); return false; }
            w.data.resize(size / 4);
            size_t got = std::fread(w.data.data(), 4, w.data.size(), f);
            w.data.resize(got);
            std::fclose(f);
            return true;
        } else {
            std::fseek(f, (long)(size + (size & 1)), SEEK_CUR);
        }
    }
    std::fclose(f);
    return false;
}

static bool writeWav(const char* path, const Wav& w) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const uint32_t dataBytes = (uint32_t)(w.data.size() * 4);
    const uint32_t riffSize = 4 + (8 + 16) + (8 + dataBytes);
    const uint16_t tag = 3, ch = (uint16_t)w.ch, bits = 32, align = (uint16_t)(w.ch * 4);
    const uint32_t sr = (uint32_t)w.sr, br = sr * align, fmtSize = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riffSize, 4, 1, f); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); std::fwrite(&fmtSize, 4, 1, f);
    std::fwrite(&tag, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&sr, 4, 1, f);
    std::fwrite(&br, 4, 1, f); std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&dataBytes, 4, 1, f);
    std::fwrite(w.data.data(), 4, w.data.size(), f);
    std::fclose(f);
    return true;
}

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
