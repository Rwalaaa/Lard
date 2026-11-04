// WavIO.h — lecture/écriture WAV float32 mono/stéréo pour les outils de test.
#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

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
