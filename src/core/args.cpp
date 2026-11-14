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
            a.paletteSet = true;
            usedNext = true;
        // M3 begin
        } else if (is("--bot")) {
            a.bot = true;
        } else if (is("--autoplay")) {
            a.autoplay = true;
        } else if (is("--reckless-after") && next) {
            a.recklessAfter = static_cast<float>(std::atof(next));
            ++i;
        } else if (is("--seed") && next) {
            u32 s = 0;
            ok = parseUint(next, s);
            a.seed = s;
            usedNext = true;
        } else if (is("--nohits")) {
            a.noHits = true;
        } else if (is("--loadout") && next) {
            ok = std::strcmp(next, "max") == 0;
            a.loadoutMax = ok;
            usedNext = true;
        } else if (is("--fast-forward") && next) {
            ok = parseFloat(next, a.fastForward) && a.fastForward >= 0.0f && a.fastForward <= 3600.0f;
            usedNext = true;
        } else if (is("--fight") && next) {
            // M5: --fight <id>[:<phase>] (sim/boss.h bossId: kaleidos, longwave, choir, alpha_hound, the_hive, ...).
            ok = copyString(a.fight, sizeof a.fight, next);
            if (ok) {
                char* colon = std::strchr(a.fight, ':');
                if (colon) {
                    a.fightPhase = static_cast<u8>(std::atoi(colon + 1));
                    a.fightHold = std::strstr(colon + 1, ":hold") != nullptr;
                    *colon = 0;
                }
            }
            usedNext = true;
        } else if (is("--drop") && next) {
            // M5: --drop <track>.<cycle> (benchmarks: a Track's Drop with its own roster and crowd).
            const int t = std::atoi(next);
            const char* dot = std::strchr(next, '.');
            const int cy = dot ? std::atoi(dot + 1) : 1;
            ok = t >= 1 && t <= 10 && cy >= 1 && cy <= 3;
            a.jumpTrack = static_cast<u8>(t);
            a.jumpCycle = static_cast<u8>(cy);
            usedNext = true;
        } else if (is("--drop-hold")) {
            a.holdDrop = true;  // M7 (the thermal check): with --drop, that Drop never ends
        } else if (is("--picture") && next) {
            ok = copyString(a.picture, sizeof a.picture, next);
            usedNext = true;
        // M3 end
        // M4 begin
        } else if (is("--host")) {
            a.net = 1;
        } else if (is("--join") && next) {
            a.net = 2;
            ok = copyString(a.joinIp, sizeof a.joinIp, next);
            usedNext = true;
        } else if (is("--bind") && next) {
            ok = copyString(a.bindIp, sizeof a.bindIp, next);
            usedNext = true;
        } else if (is("--port") && next) {
            ok = parseUint(next, a.port) && a.port >= 1024 && a.port <= 65535;
            usedNext = true;
        } else if (is("--players") && next) {
            ok = parseUint(next, a.players) && a.players >= 2 && a.players <= 4;
            usedNext = true;
        } else if (is("--netsim") && next) {
            ok = copyString(a.netsim, sizeof a.netsim, next);
            usedNext = true;
        } else if (is("--loopback-bots") && next) {
            ok = parseUint(next, a.loopbackBots) && a.loopbackBots <= 3;
            usedNext = true;
        } else if (is("--net-overlay")) {
            a.netOverlay = true;
        } else if (is("--net-capture-bar") && next) {
            ok = parseUint(next, a.netCaptureBar) && a.netCaptureBar > 0;
            usedNext = true;
        } else if (is("--net-view-seat") && next) {
            u32 v = 0;
            ok = parseUint(next, v) && v < 4;
            a.netViewSeat = static_cast<i32>(v);
            usedNext = true;
        } else if (is("--coop") && next) {
            // M4 stage B: host | join | join:<code> | join:<ip> | join:list
            if (std::strcmp(next, "host") == 0) {
                a.coop = 1;
            } else if (std::strncmp(next, "join", 4) == 0 && (next[4] == '\0' || next[4] == ':')) {
                a.coop = 2;
                ok = next[4] == '\0' || copyString(a.coopTarget, sizeof a.coopTarget, next + 5);
            } else {
                ok = false;
            }
            usedNext = true;
        } else if (is("--coop-start")) {
            a.coopStart = 2;
            if (next && next[0] >= '2' && next[0] <= '4' && next[1] == '\0') {
                a.coopStart = static_cast<u32>(next[0] - '0');
                usedNext = true;
            }
        } else if (is("--coop-name") && next) {
            ok = copyString(a.coopName, sizeof a.coopName, next);
            usedNext = true;
        } else if (is("--coop-permission-screen")) {
            a.coopPermission = 1;
            if (next && std::strcmp(next, "refused") == 0) {
                a.coopPermission = 2;  // the screen as it is after a refusal (pictures)
                usedNext = true;
            }
        } else if (is("--coop-demo-rooms")) {
            a.coopDemoRooms = true;
        } else if (is("--net-content-salt") && next) {
            ok = parseUint(next, a.contentSalt);
            usedNext = true;
        } else if (is("--coop-shot") && next) {
            ok = copyString(a.coopShot, sizeof a.coopShot, next);
            usedNext = true;
        } else if (is("--coop-down-at") && next) {
            ok = parseFloat(next, a.coopDownAt) && a.coopDownAt >= 0.0f;
            usedNext = true;
        } else if (is("--coop-quit-at") && next) {
            ok = parseFloat(next, a.coopQuitAt) && a.coopQuitAt >= 0.0f;
            usedNext = true;
        } else if (is("--coop-pause-at") && next) {
            ok = parseFloat(next, a.coopPauseAt) && a.coopPauseAt >= 0.0f;
            usedNext = true;
        // M4 end
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
        } else if (is("--offscreen")) {
            a.offscreen = true;
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
           "look=%s reduce_flashing=%d tier=%s cuts=%u palette=%s strip=%u page=%u governor=%d fixed_step=%d mute=0x%x "
           "bot=%d seed=%llu fast_forward=%.1f picture='%s' net=%s%s players=%u port=%u bind='%s' netsim='%s' loopback_bots=%u",
           a.scenario, a.seconds, a.warmupSeconds, a.frames, a.screenshot, a.width, a.height, a.validation,
           a.tone ? 1 : 0, a.swappy ? 1 : 0, a.fps, a.fastCores ? "big" : "all", a.overlay ? 1 : 0, a.cycles,
           a.sceneFile, a.still ? 1 : 0, a.bloom ? 1 : 0, a.look, a.reduceFlashing ? 1 : 0,
           a.tier[0] ? a.tier : "(default)", a.cuts, a.palette, a.strip, a.page, a.governor, a.fixedStep ? 1 : 0, a.mute,
           a.bot ? 1 : 0, static_cast<unsigned long long>(a.seed), a.fastForward, a.picture,
           a.net == 1 ? "host" : (a.net == 2 ? "join " : "solo"), a.net == 2 ? a.joinIp : "", a.players, a.port, a.bindIp, a.netsim,
           a.loopbackBots);
}

}  // namespace rf
