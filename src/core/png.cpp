#include "core/png.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"

namespace rf {

namespace {

u32 g_crcTable[256];
bool g_crcReady = false;

void initCrc() {
    if (g_crcReady) return;
    for (u32 n = 0; n < 256; ++n) {
        u32 c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        g_crcTable[n] = c;
    }
    g_crcReady = true;
}

u32 crcUpdate(u32 crc, const u8* p, size_t n) {
    for (size_t i = 0; i < n; ++i) crc = g_crcTable[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    return crc;
}

u32 adler32(const u8* p, size_t n) {
    u32 a = 1, b = 0;
    while (n > 0) {
        const size_t chunk = n < 5552 ? n : 5552;  // largest run without u32 overflow
        for (size_t i = 0; i < chunk; ++i) {
            a += p[i];
            b += a;
        }
        a %= 65521u;
        b %= 65521u;
        p += chunk;
        n -= chunk;
    }
    return (b << 16) | a;
}

void putBE32(u8* p, u32 v) {
    p[0] = static_cast<u8>(v >> 24);
    p[1] = static_cast<u8>(v >> 16);
    p[2] = static_cast<u8>(v >> 8);
    p[3] = static_cast<u8>(v);
}

bool writeChunk(FILE* f, const char type[4], const u8* data, u32 size) {
    u8 header[8];
    putBE32(header, size);
    std::memcpy(header + 4, type, 4);
    u32 crc = crcUpdate(0xFFFFFFFFu, header + 4, 4);
    crc = crcUpdate(crc, data, size) ^ 0xFFFFFFFFu;
    u8 trailer[4];
    putBE32(trailer, crc);
    return std::fwrite(header, 1, 8, f) == 8 && (size == 0 || std::fwrite(data, 1, size, f) == size) &&
           std::fwrite(trailer, 1, 4, f) == 4;
}

}  // namespace

bool writePngUpright(const char* path, const u8* src, u32 srcW, u32 srcH, PixelLayout layout, int rotationDeg) {
    initCrc();
    const bool swapAxes = rotationDeg == 90 || rotationDeg == 270;
    const u32 outW = swapAxes ? srcH : srcW;
    const u32 outH = swapAxes ? srcW : srcH;
    const int r = layout == PixelLayout::RGBA8 ? 0 : 2;
    const int b = 2 - r;

    // Filtered scanlines: filter byte 0 + RGB.
    const size_t rowBytes = 1 + size_t(outW) * 3;
    const size_t rawSize = rowBytes * outH;
    const size_t blocks = (rawSize + 65534) / 65535;
    const size_t zSize = 2 + blocks * 5 + rawSize + 4;
    u8* raw = static_cast<u8*>(std::malloc(rawSize));
    u8* z = static_cast<u8*>(std::malloc(zSize));
    if (!raw || !z) {
        std::free(raw);
        std::free(z);
        RF_ERR("png: out of memory for %ux%u", outW, outH);
        return false;
    }

    // Logical pixel (x, y) reads physical pixel (px, py); see Swapchain::rotationDegrees for the convention.
    for (u32 y = 0; y < outH; ++y) {
        u8* row = raw + rowBytes * y;
        row[0] = 0;
        for (u32 x = 0; x < outW; ++x) {
            u32 px = x, py = y;
            switch (rotationDeg) {
                case 90: px = srcW - 1 - y; py = x; break;
                case 180: px = srcW - 1 - x; py = srcH - 1 - y; break;
                case 270: px = y; py = srcH - 1 - x; break;
                default: break;
            }
            const u8* s = src + (size_t(py) * srcW + px) * 4;
            u8* d = row + 1 + size_t(x) * 3;
            d[0] = s[r];
            d[1] = s[1];
            d[2] = s[b];
        }
    }

    size_t o = 0;
    z[o++] = 0x78;  // deflate, 32K window
    z[o++] = 0x01;  // no preset dictionary, check bits
    for (size_t pos = 0; pos < rawSize;) {
        const size_t n = rawSize - pos < 65535 ? rawSize - pos : 65535;
        const bool last = pos + n == rawSize;
        z[o++] = last ? 1 : 0;  // BFINAL, BTYPE=00 (stored)
        z[o++] = static_cast<u8>(n & 0xFF);
        z[o++] = static_cast<u8>(n >> 8);
        z[o++] = static_cast<u8>(~n & 0xFF);
        z[o++] = static_cast<u8>((~n >> 8) & 0xFF);
        std::memcpy(z + o, raw + pos, n);
        o += n;
        pos += n;
    }
    putBE32(z + o, adler32(raw, rawSize));
    o += 4;
    std::free(raw);

    bool ok = false;
    if (FILE* f = std::fopen(path, "wb")) {
        static const u8 kSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        u8 ihdr[13];
        putBE32(ihdr, outW);
        putBE32(ihdr + 4, outH);
        ihdr[8] = 8;   // bit depth
        ihdr[9] = 2;   // colour type RGB
        ihdr[10] = 0;  // deflate
        ihdr[11] = 0;  // adaptive filtering
        ihdr[12] = 0;  // no interlace
        ok = std::fwrite(kSignature, 1, 8, f) == 8 && writeChunk(f, "IHDR", ihdr, 13) &&
             writeChunk(f, "IDAT", z, static_cast<u32>(o)) && writeChunk(f, "IEND", nullptr, 0);
        ok = (std::fclose(f) == 0) && ok;
    }
    std::free(z);
    if (!ok) RF_ERR("png: failed to write %s", path);
    return ok;
}

}  // namespace rf
