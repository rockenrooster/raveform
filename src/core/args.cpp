#include "core/args.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"

namespace rf {

namespace {

bool copyString(char* dst, size_t cap, const char* src) {
    const size_t n = std::strlen(src);
    if (n + 1 > cap) return false;
    std::memcpy(dst, src, n + 1);
    return true;
}

bool parseFloat(const char* s, float& out) {
    char* end = nullptr;
    const float v = std::strtof(s, &end);
    if (end == s || *end != '\0') return false;
    out = v;
    return true;
}

bool parseUint(const char* s, u32& out) {
    char* end = nullptr;
    const unsigned long v = std::strtoul(s, &end, 10);
    if (end == s || *end != '\0') return false;
    out = static_cast<u32>(v);
    return true;
}

}  // namespace

bool parseArgs(int argc, char** argv, AppArgs& a) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const char* next = i + 1 < argc ? argv[i + 1] : nullptr;
        auto is = [arg](const char* name) { return std::strcmp(arg, name) == 0; };
        bool ok = true;
        bool usedNext = false;

        if (is("--scenario") && next) {
            ok = copyString(a.scenario, sizeof a.scenario, next);
            usedNext = true;
        } else if (is("--seconds") && next) {
            ok = parseFloat(next, a.seconds) && a.seconds > 0.0f;
            usedNext = true;
        } else if (is("--warmup") && next) {
            ok = parseFloat(next, a.warmupSeconds) && a.warmupSeconds >= 0.0f;
            usedNext = true;
        } else if (is("--frames") && next) {
            ok = parseUint(next, a.frames);
            usedNext = true;
        } else if (is("--screenshot") && next) {
            ok = copyString(a.screenshot, sizeof a.screenshot, next);
            usedNext = true;
        } else if (is("--res") && next) {
            ok = std::sscanf(next, "%dx%d", &a.width, &a.height) == 2 && a.width > 0 && a.height > 0;
            usedNext = true;
        } else if (is("--fps") && next) {
            ok = parseFloat(next, a.fps) && a.fps >= 0.0f;
            usedNext = true;
        } else if (is("--cycles") && next) {
            ok = parseUint(next, a.cycles) && a.cycles > 0;
            usedNext = true;
        } else if (is("--validation")) {
            a.validation = 1;
        } else if (is("--no-validation")) {
            a.validation = 0;
        } else if (is("--tone")) {
            a.tone = true;
        } else if (is("--no-swappy")) {
            a.swappy = false;
        } else if (is("--no-overlay")) {
            a.overlay = false;
        } else {
            RF_ERR("unknown or incomplete argument: %s", arg);
            return false;
        }
        if (!ok) {
            RF_ERR("bad value for %s: %s", arg, next ? next : "(none)");
            return false;
        }
        if (usedNext) ++i;
    }
    return true;
}

void logArgs(const AppArgs& a) {
    RF_LOG("args: scenario='%s' seconds=%.1f warmup=%.1f frames=%u screenshot='%s' res=%dx%d "
           "validation=%d tone=%d swappy=%d fps=%.1f overlay=%d cycles=%u",
           a.scenario, a.seconds, a.warmupSeconds, a.frames, a.screenshot, a.width, a.height, a.validation,
           a.tone ? 1 : 0, a.swappy ? 1 : 0, a.fps, a.overlay ? 1 : 0, a.cycles);
}

}  // namespace rf
