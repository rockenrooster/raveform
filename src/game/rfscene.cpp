#include "game/rfscene.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"

namespace rf {

RfScene::~RfScene() {
    std::free(capsules);
    std::free(sparkles);
    std::free(pools);
    std::free(tris);
    std::free(rims);
    std::free(cores);
}

namespace {

// Up to `max` numbers after the keyword; returns how many were read.
u32 readNumbers(const char* s, float* out, u32 max) {
    u32 n = 0;
    while (n < max) {
        char* end = nullptr;
        const float v = std::strtof(s, &end);
        if (end == s) break;
        out[n++] = v;
        s = end;
    }
    return n;
}

bool startsWith(const char* line, const char* word) {
    const size_t n = std::strlen(word);
    return std::strncmp(line, word, n) == 0 && (line[n] == ' ' || line[n] == '\t');
}

}  // namespace

bool loadRfScene(const char* path, float poolMinRadius, RfScene& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        RF_ERR("scene: cannot open %s", path);
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    char* text = static_cast<char*>(std::malloc(static_cast<size_t>(size) + 1));
    if (!text || std::fread(text, 1, static_cast<size_t>(size), f) != static_cast<size_t>(size)) {
        std::fclose(f);
        std::free(text);
        RF_ERR("scene: cannot read %s", path);
        return false;
    }
    std::fclose(f);
    text[size] = '\0';

    // Pass 1: count primitives; pass 2: fill.
    u32 caps = 0, glows = 0, poolCount = 0, tris = 0, threat = 0;
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            out.capsules = static_cast<CapsuleGpu*>(std::calloc(caps ? caps : 1, sizeof(CapsuleGpu)));
            out.sparkles = static_cast<GlowGpu*>(std::calloc(glows ? glows : 1, sizeof(GlowGpu)));
            out.pools = static_cast<GlowGpu*>(std::calloc(poolCount ? poolCount : 1, sizeof(GlowGpu)));
            out.tris = static_cast<TriVertexGpu*>(std::calloc(tris ? tris * 3 : 1, sizeof(TriVertexGpu)));
            out.rims = static_cast<CapsuleGpu*>(std::calloc(threat ? threat : 1, sizeof(CapsuleGpu)));
            out.cores = static_cast<CapsuleGpu*>(std::calloc(threat ? threat : 1, sizeof(CapsuleGpu)));
            if (!out.capsules || !out.sparkles || !out.pools || !out.tris || !out.rims || !out.cores) {
                std::free(text);
                RF_ERR("scene: out of memory");
                return false;
            }
            caps = glows = poolCount = tris = threat = 0;
        }
        bool header = false;
        char* line = text;
        u32 lineNo = 0;
        while (line && *line) {
            char* next = std::strchr(line, '\n');
            if (next) *next = '\0';
            ++lineNo;
            float v[16];
            const char* rest = line;
            while (*rest && *rest != ' ') ++rest;
            if (std::strncmp(line, "rfscene", 7) == 0) {
                header = true;
            } else if (startsWith(line, "size") && readNumbers(rest, v, 2) == 2) {
                out.view.canvasW = v[0];
                out.view.canvasH = v[1];
            } else if (startsWith(line, "bloom") && readNumbers(rest, v, 1) == 1) {
                out.lookBloom = v[0];
            } else if (startsWith(line, "exposure") && readNumbers(rest, v, 1) == 1) {
                out.lookExposure = v[0];
            } else if (startsWith(line, "bg") && readNumbers(rest, v, 10) == 10) {
                Background& b = out.view.background;  // edge rgb, centre rgb, cx, cy, radius, power
                b.edge = {v[0], v[1], v[2]};
                b.centre = {v[3], v[4], v[5]};
                b.cx = v[6];
                b.cy = v[7];
                b.radius = v[8];
                b.power = v[9];
                out.view.hasBackground = true;
            } else if (startsWith(line, "neb") && readNumbers(rest, v, 6) == 6) {
                Background& b = out.view.background;
                if (pass == 1 && b.cloudCount < 8) b.clouds[b.cloudCount++] = {v[0], v[1], v[2], {v[3], v[4], v[5]}};
            } else if (startsWith(line, "cap")) {
                const u32 n = readNumbers(rest, v, 12);
                if (n != 9 && n != 12) {
                    RF_WARN("scene: line %u: bad cap", lineNo);
                } else if (pass == 1) {
                    const Vec3 c0{v[6], v[7], v[8]};
                    const Vec3 c1 = n == 12 ? Vec3{v[9], v[10], v[11]} : c0;
                    writeCapsule(out.capsules[caps], {v[0], v[1]}, {v[2], v[3]}, 0.5f * v[4], c0, c1,
                                 static_cast<u32>(v[5]) & (kFlatStart | kFlatEnd));
                    ++caps;
                } else {
                    ++caps;
                }
            } else if (startsWith(line, "disc") && readNumbers(rest, v, 6) == 6) {
                if (pass == 1) writeCapsule(out.capsules[caps], {v[0], v[1]}, {v[0], v[1]}, v[2], {v[3], v[4], v[5]},
                                            {v[3], v[4], v[5]}, kDisc);
                ++caps;
            } else if (startsWith(line, "glow") && readNumbers(rest, v, 6) == 6) {
                const bool pool = v[2] >= poolMinRadius;
                if (pass == 1) writeGlow(pool ? out.pools[poolCount] : out.sparkles[glows], {v[0], v[1]}, v[2], {v[3], v[4], v[5]});
                ++(pool ? poolCount : glows);
            } else if (startsWith(line, "tri") && readNumbers(rest, v, 15) == 15) {
                if (pass == 1)
                    for (u32 k = 0; k < 3; ++k)
                        writeTriVertex(out.tris[tris * 3 + k], {v[k * 5], v[k * 5 + 1]}, {v[k * 5 + 2], v[k * 5 + 3], v[k * 5 + 4]});
                ++tris;
            } else if (startsWith(line, "thr") && readNumbers(rest, v, 9) == 9) {
                if (pass == 1) {
                    const float half = 0.5f * v[4];
                    writeCapsule(out.rims[threat], {v[0], v[1]}, {v[2], v[3]}, half + v[5], {}, {}, 0);
                    writeCapsule(out.cores[threat], {v[0], v[1]}, {v[2], v[3]}, half, {v[6], v[7], v[8]},
                                 {v[6], v[7], v[8]}, 0);
                }
                ++threat;
            } else if (*line && *line != '#' && *line != '\r') {
                RF_WARN("scene: line %u not understood: %.40s", lineNo, line);
            }
            if (next) *next = '\n';  // restore for the second pass
            line = next ? next + 1 : nullptr;
        }
        if (!header) {
            std::free(text);
            RF_ERR("scene: %s is not an rfscene file", path);
            return false;
        }
    }
    std::free(text);
    StaticScene& s = out.view;
    s.capsules = out.capsules;
    s.capsuleCount = caps;
    s.sparkles = out.sparkles;
    s.sparkleCount = glows;
    s.pools = out.pools;
    s.poolCount = poolCount;
    s.triVertices = out.tris;
    s.triVertexCount = tris * 3;
    s.threatRims = out.rims;
    s.threatCores = out.cores;
    s.threatCount = threat;
    RF_LOG("scene: %s: canvas %.0fx%.0f, %u capsules, %u sparkles, %u pools, %u triangles, %u threat, %u clouds", path,
           s.canvasW, s.canvasH, caps, glows, poolCount, tris, threat, s.background.cloudCount);
    return true;
}

}  // namespace rf
