#include "audio/wav.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "audio/dsp.h"

namespace rf::audio {

namespace {

void put32(unsigned char* p, u32 v) {
    p[0] = static_cast<unsigned char>(v);
    p[1] = static_cast<unsigned char>(v >> 8);
    p[2] = static_cast<unsigned char>(v >> 16);
    p[3] = static_cast<unsigned char>(v >> 24);
}
void put16(unsigned char* p, u32 v) {
    p[0] = static_cast<unsigned char>(v);
    p[1] = static_cast<unsigned char>(v >> 8);
}

}  // namespace

bool writeWav(const char* path, const float* x, u64 frames, u32 channels, u32 sampleRate, bool pcm16) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const u32 bytesPer = pcm16 ? 2 : 4;
    const u64 dataBytes = frames * channels * bytesPer;
    unsigned char h[44] = {};
    std::memcpy(h, "RIFF", 4);
    put32(h + 4, static_cast<u32>(36 + dataBytes));
    std::memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16);
    put16(h + 20, pcm16 ? 1 : 3);
    put16(h + 22, channels);
    put32(h + 24, sampleRate);
    put32(h + 28, sampleRate * channels * bytesPer);
    put16(h + 32, channels * bytesPer);
    put16(h + 34, bytesPer * 8);
    std::memcpy(h + 36, "data", 4);
    put32(h + 40, static_cast<u32>(dataBytes));
    bool ok = std::fwrite(h, 1, 44, f) == 44;
    Random rng(99);
    constexpr u64 kChunk = 4096;
    unsigned char buf[kChunk * 4];
    const u64 total = frames * channels;
    for (u64 i = 0; ok && i < total; i += kChunk) {
        const u64 n = total - i < kChunk ? total - i : kChunk;
        if (pcm16) {
            for (u64 k = 0; k < n; ++k) {
                // TPDF dither of one LSB, as musicproto.write_wav.
                const double d = (rng.uniform() - rng.uniform()) / 32768.0;
                double v = x[i + k] + d;
                v = v < -1.0 ? -1.0 : (v > 1.0 ? 1.0 : v);
                const i32 s = static_cast<i32>(v * 32767.0);
                put16(buf + 2 * k, static_cast<u32>(static_cast<u16>(static_cast<std::int16_t>(s))));
            }
        } else {
            std::memcpy(buf, x + i, n * 4);
        }
        ok = std::fwrite(buf, 1, n * bytesPer, f) == n * bytesPer;
    }
    return std::fclose(f) == 0 && ok;
}

double integratedLoudness(const float* x, u64 frames, u32 channels, u32 sampleRate) {
    if (sampleRate != 48000 || frames == 0) return -70.0;
    // K-weighting at 48 kHz (BS.1770 coefficients, as musicproto.lufs).
    const double sb0 = 1.53512485958697, sb1 = -2.69169618940638, sb2 = 1.19839281085285, sa1 = -1.69065929318241,
                 sa2 = 0.73248077421585;
    const double ra1 = -1.99004745483398, ra2 = 0.99007225036621;
    double* k2 = static_cast<double*>(std::calloc(frames, sizeof(double)));  // sum over channels of k^2
    if (!k2) return -70.0;
    for (u32 c = 0; c < channels; ++c) {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, w1 = 0, w2 = 0;
        for (u64 i = 0; i < frames; ++i) {
            const double in = x[i * channels + c];
            const double y = sb0 * in + sb1 * x1 + sb2 * x2 - sa1 * y1 - sa2 * y2;
            x2 = x1;
            x1 = in;
            y2 = y1;
            y1 = y;
            const double w = y - 2.0 * u1 + u2 - ra1 * w1 - ra2 * w2;
            u2 = u1;
            u1 = y;
            w2 = w1;
            w1 = w;
            k2[i] += w * w;
        }
    }
    const u64 block = static_cast<u64>(0.4 * sampleRate), hop = static_cast<u64>(0.1 * sampleRate);
    u64 nb = 0;
    for (u64 i = 0; i + block < frames; i += hop) ++nb;
    double* power = static_cast<double*>(std::calloc(nb ? nb : 1, sizeof(double)));
    if (!power) {
        std::free(k2);
        return -70.0;
    }
    // Running sum over the block.
    u64 b = 0;
    for (u64 i = 0; i + block < frames; i += hop, ++b) {
        double s = 0.0;
        for (u64 k = 0; k < block; ++k) s += k2[i + k];
        power[b] = s / static_cast<double>(block);
    }
    std::free(k2);
    const auto loud = [](double p) { return -0.691 + 10.0 * std::log10(p > 1e-12 ? p : 1e-12); };
    double sum = 0.0;
    u64 n = 0;
    for (u64 i = 0; i < nb; ++i)
        if (loud(power[i]) > -70.0) {
            sum += power[i];
            ++n;
        }
    if (!n) {
        std::free(power);
        return -70.0;
    }
    const double rel = loud(sum / static_cast<double>(n)) - 10.0;
    const double gate = rel > -70.0 ? rel : -70.0;
    sum = 0.0;
    n = 0;
    for (u64 i = 0; i < nb; ++i)
        if (loud(power[i]) > gate) {
            sum += power[i];
            ++n;
        }
    std::free(power);
    return n ? loud(sum / static_cast<double>(n)) : -70.0;
}

}  // namespace rf::audio
