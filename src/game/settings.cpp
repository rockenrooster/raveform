#include "game/settings.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"

namespace rf {

void appFilePath(char* out, size_t cap, const char* name) {
    char* pref = SDL_GetPrefPath("raveform", "raveform");
    std::snprintf(out, cap, "%s%s", pref ? pref : "", name);
    SDL_free(pref);
}

bool writeAppFile(const char* name, const char* text, size_t length) {
    char path[512], tmp[520];
    appFilePath(path, sizeof path, name);
    std::snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE* f = std::fopen(tmp, "wb");
    if (!f) return false;
    const bool wrote = std::fwrite(text, 1, length, f) == length;
    const bool closed = std::fclose(f) == 0;
    if (!wrote || !closed) {
        std::remove(tmp);
        return false;
    }
    std::remove(path);
    return std::rename(tmp, path) == 0;
}

namespace {
template <typename F>
bool readKeys(const char* name, F&& f) {
    char path[512];
    appFilePath(path, sizeof path, name);
    FILE* in = std::fopen(path, "r");
    if (!in) return false;
    char line[256];
    while (std::fgets(line, sizeof line, in)) {
        char* eq = std::strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* v = eq + 1;
        const size_t n = std::strlen(v);
        if (n && (v[n - 1] == '\n' || v[n - 1] == '\r')) v[n - 1] = 0;
        f(line, v);
    }
    std::fclose(in);
    return true;
}
float clamp01f(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
}  // namespace

bool loadSettings(Settings& s) {
    const bool found = readKeys("settings.txt", [&](const char* k, const char* v) {
        const auto is = [&](const char* key) { return std::strcmp(k, key) == 0; };
        const long n = std::strtol(v, nullptr, 10);
        const float x = static_cast<float>(std::atof(v));
        if (is("variant")) s.variant = static_cast<u8>(n >= 0 && n < 3 ? n : 0);
        if (is("master")) s.master = clamp01f(x);
        if (is("music")) s.music = clamp01f(x);
        if (is("effects")) s.effects = clamp01f(x);
        if (is("haptics")) s.haptics = static_cast<u8>(n >= 0 && n < 3 ? n : 1);
        if (is("music_host_only")) s.musicHostOnly = n != 0;
        if (is("swap_sides")) s.swapSides = n != 0;
        if (is("auto_aim")) s.autoAim = n != 0;
        if (is("auto_nova")) s.autoNova = n != 0;
        if (is("effects_tier")) s.effectsTier = static_cast<u8>(n >= 0 && n < 5 ? n : 0);
        if (is("frame_rate")) s.frameRate = static_cast<u8>(n >= 0 && n < 4 ? n : 0);
        if (is("reduce_flashing")) s.reduceFlashing = n != 0;
        if (is("notice_seen")) s.noticeSeen = n != 0;
        if (is("probed_tier")) s.probedTier = static_cast<u32>(n >= 0 && n < 5 ? n : 0);
        if (is("ui_scale")) s.uiScale = static_cast<u8>(n >= 0 && n < 4 ? n : 1);
        if (is("hints_seen")) s.hintsSeen = static_cast<u32>(std::strtoul(v, nullptr, 10));
        if (is("runs_ended")) s.runsEnded = static_cast<u32>(std::strtoul(v, nullptr, 10));
    });
    if (found) RF_LOG("settings: loaded (sound %u, effects %u, reduce flashing %s)", s.variant, s.effectsTier, s.reduceFlashing ? "on" : "off");
    return found;
}

bool saveSettings(const Settings& s) {
    char text[640];
    const int n = std::snprintf(text, sizeof text,
                                "variant=%u\nmaster=%.3f\nmusic=%.3f\neffects=%.3f\nhaptics=%u\nswap_sides=%d\nauto_aim=%d\n"
                                "effects_tier=%u\nframe_rate=%u\nreduce_flashing=%d\nnotice_seen=%d\nprobed_tier=%u\nmusic_host_only=%d\n"
                                "auto_nova=%d\nui_scale=%u\nhints_seen=%u\nruns_ended=%u\n",
                                s.variant, s.master, s.music, s.effects, s.haptics, s.swapSides ? 1 : 0, s.autoAim ? 1 : 0, s.effectsTier,
                                s.frameRate, s.reduceFlashing ? 1 : 0, s.noticeSeen ? 1 : 0, s.probedTier, s.musicHostOnly ? 1 : 0,
                                s.autoNova ? 1 : 0, s.uiScale, s.hintsSeen, s.runsEnded);
    const bool ok = n > 0 && writeAppFile("settings.txt", text, static_cast<size_t>(n));
    if (!ok) RF_WARN("settings: could not be saved");
    return ok;
}

i32 HighScores::add(const HighScore& h) {
    u32 at = 0;
    while (at < count && entry[at].score >= h.score) ++at;
    if (at >= kMax) return -1;
    const u32 last = count < kMax ? count : kMax - 1;
    for (u32 k = last; k > at; --k) entry[k] = entry[k - 1];
    entry[at] = h;
    if (count < kMax) ++count;
    return static_cast<i32>(at);
}

bool loadHighScores(HighScores& h) {
    h.count = 0;
    return readKeys("highscores.txt", [&](const char* k, const char* v) {
        if (std::strcmp(k, "run") != 0 || h.count >= HighScores::kMax) return;
        HighScore e;
        unsigned long long score = 0;
        unsigned track = 1, chain = 1, secs = 0, kills = 0, set = 0;
        if (std::sscanf(v, "%llu %u %u %u %u %u", &score, &track, &chain, &secs, &kills, &set) >= 5) {  // (M6: the set's flag, sixth)
            e.setComplete = set != 0;
            e.score = score;
            e.track = track;
            e.bestChain = chain;
            e.seconds = secs;
            e.kills = kills;
            h.add(e);
        }
    });
}

bool saveHighScores(const HighScores& h) {
    char text[1024];
    int n = 0;
    for (u32 k = 0; k < h.count && n >= 0 && n < static_cast<int>(sizeof text) - 80; ++k) {
        const HighScore& e = h.entry[k];
        n += std::snprintf(text + n, sizeof text - static_cast<size_t>(n), "run=%llu %u %u %u %u %u\n",
                           static_cast<unsigned long long>(e.score), e.track, e.bestChain, e.seconds, e.kills, e.setComplete ? 1u : 0u);
    }
    return n >= 0 && writeAppFile("highscores.txt", text, static_cast<size_t>(n));
}

}  // namespace rf
