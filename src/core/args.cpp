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

struct MuteName {
    const char* name;
    u32 bit;
};
constexpr MuteName kMuteNames[] = {
    {"background", kMuteBackground}, {"grid", kMuteGrid},           {"heat", kMuteHeat},
    {"enemy", kMuteEnemy},           {"elite", kMuteElite},         {"boss", kMuteBoss},
    {"aura", kMuteAura},             {"bullet", kMuteBullet},       {"priority", kMutePriority},
    {"sparks", kMuteSparks},         {"flash", kMuteFlash},         {"killpool", kMutePool},
    {"fx", kMuteFx},                 {"warp", kMuteWarp},           {"shard", kMuteShard},
    {"player", kMutePlayer},         {"afterglow", kMuteAfterglow}, {"distortion", kMuteDistortion},
    {"aberration", kMuteAberration}, {"vignette", kMuteVignette},   {"pump", kMutePump},
    {"sparkles", kMuteSparkles},     {"pools", kMutePools},
};

// "aura,boss" -> kMuteAura | kMuteBoss; false on an unknown name.
bool parseMute(const char* s, u32& out) {
    u32 mask = 0;
    while (*s) {
        const char* end = std::strchr(s, ',');
        const size_t n = end ? static_cast<size_t>(end - s) : std::strlen(s);
        bool found = false;
        for (const MuteName& m : kMuteNames) {
            if (std::strlen(m.name) == n && std::strncmp(m.name, s, n) == 0) {
                mask |= m.bit;
                found = true;
            }
        }
        if (!found) return false;
        s += n;
        if (*s == ',') ++s;
    }
    out = mask;
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
        } else if (is("--scene") && next) {
            ok = copyString(a.sceneFile, sizeof a.sceneFile, next);
            usedNext = true;
        } else if (is("--look") && next) {
            ok = copyString(a.look, sizeof a.look, next);
            usedNext = true;
        } else if (is("--bloom") && next) {
            ok = std::strcmp(next, "on") == 0 || std::strcmp(next, "off") == 0;
            a.bloom = std::strcmp(next, "on") == 0;
            usedNext = true;
        } else if (is("--still")) {
            a.still = true;
        } else if (is("--reduce-flashing")) {
            a.reduceFlashing = true;
        } else if (is("--tier") && next) {
            ok = copyString(a.tier, sizeof a.tier, next);
            usedNext = true;
        } else if (is("--cuts") && next) {
            ok = parseUint(next, a.cuts) && a.cuts <= 11;
            usedNext = true;
        } else if (is("--palette") && next) {
            ok = copyString(a.palette, sizeof a.palette, next);
            usedNext = true;
        } else if (is("--strip") && next) {
            ok = parseUint(next, a.strip) && a.strip <= 64;
            usedNext = true;
        } else if (is("--page") && next) {
            ok = parseUint(next, a.page) && a.page >= 1 && a.page <= 3;
            usedNext = true;
        } else if (is("--governor") && next) {
            ok = std::strcmp(next, "on") == 0 || std::strcmp(next, "off") == 0;
            a.governor = std::strcmp(next, "on") == 0 ? 1 : 0;
            usedNext = true;
        } else if (is("--fixed-step")) {
            a.fixedStep = true;
        } else if (is("--gpu-burn") && next) {
            ok = parseUint(next, a.gpuBurn) && a.gpuBurn <= 200;
            usedNext = true;
        } else if (is("--mute") && next) {
            ok = parseMute(next, a.mute);
            usedNext = true;
        } else if (is("--affinity") && next) {
            ok = std::strcmp(next, "big") == 0 || std::strcmp(next, "all") == 0;
            a.fastCores = std::strcmp(next, "big") == 0;
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
        // M2 audio begin
        } else if (isAudioArg(arg)) {
            ok = parseAudioArg(arg, next, a.audio, usedNext);
        // M2 audio end
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
           "validation=%d tone=%d swappy=%d fps=%.1f affinity=%s overlay=%d cycles=%u scene='%s' still=%d bloom=%d "
           "look=%s reduce_flashing=%d tier=%s cuts=%u palette=%s strip=%u page=%u governor=%d fixed_step=%d mute=0x%x",
           a.scenario, a.seconds, a.warmupSeconds, a.frames, a.screenshot, a.width, a.height, a.validation,
           a.tone ? 1 : 0, a.swappy ? 1 : 0, a.fps, a.fastCores ? "big" : "all", a.overlay ? 1 : 0, a.cycles,
           a.sceneFile, a.still ? 1 : 0, a.bloom ? 1 : 0, a.look, a.reduceFlashing ? 1 : 0,
           a.tier[0] ? a.tier : "(default)", a.cuts, a.palette, a.strip, a.page, a.governor, a.fixedStep ? 1 : 0, a.mute);
}

}  // namespace rf
