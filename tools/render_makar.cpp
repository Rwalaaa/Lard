// render_makar.cpp — applique MakarEngine à un WAV float32 (mono/stéréo), pour les tests.
// Usage : render_makar in.wav out.wav [nom=valeur ...]
//   noms : drive punish style mix output block
#include "../dsp/MakarEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include "WavIO.h"

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: render_makar in out [nom=valeur ...]\n"); return 2; }
    Wav w;
    if (!readWav(argv[1], w) || w.ch < 1 || w.ch > 2) { std::fprintf(stderr, "lecture WAV impossible (float32 mono/stéréo attendu)\n"); return 1; }

    makar::Params p;
    int block = 512;
    int style2 = -1;
    for (int i = 3; i < argc; ++i) {
        const char* eq = std::strchr(argv[i], '=');
        if (!eq) { std::fprintf(stderr, "argument invalide : %s\n", argv[i]); return 2; }
        const std::string key(argv[i], (size_t) (eq - argv[i]));
        const float v = (float) std::atof(eq + 1);
        if      (key == "drive")  p.drive = v;
        else if (key == "punish") p.punish = v != 0.0f;
        else if (key == "style")  p.style = (int) v;
        else if (key == "mix")    p.mix = v;
        else if (key == "output") p.outputDb = v;
        else if (key == "block")  block = (int) v;
        else if (key == "style2") style2 = (int) v;   // style appliqué à partir de la moitié du fichier
        else { std::fprintf(stderr, "réglage inconnu : %s\n", key.c_str()); return 2; }
    }

    makar::Engine eng;
    eng.setParams(p);
    eng.prepare(w.sr);

    const size_t frames = w.data.size() / (size_t) w.ch;
    std::vector<float> L(frames), R(frames);
    for (size_t i = 0; i < frames; ++i) {
        L[i] = w.data[i * w.ch];
        R[i] = w.data[i * w.ch + (w.ch - 1)];
    }
    auto t0 = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos < frames; pos += (size_t) block) {
        const int n = (int) std::min((size_t) block, frames - pos);
        if (style2 >= 0 && pos >= frames / 2 && p.style != style2) {
            p.style = style2;
            eng.setParams(p);
        }
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
    std::printf("latency=%d cpu_pct=%.3f\n", eng.latencySamples(), 100.0 * secs / ((double) frames / w.sr));
    return 0;
}
