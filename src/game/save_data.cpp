// The files the game keeps between runs, by path (M7: crash-safe saves; no SDL, so the tests link it): settings.txt (the
// settings and the first-run hints) and highscores.txt. Written whole through core/save_file (a temporary file renamed over the
// old one); read as key=value lines, so a missing, short or damaged file loads as defaults (every value checked, every unknown
// or broken line skipped).

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "core/save_file.h"
#include "game/settings.h"

namespace rf {

namespace {
template <typename F>
bool readKeys(const char* path, F&& f) {
    FILE* in = std::fopen(path, "rb");
    if (!in) return false;
    char line[256];
    while (std::fgets(line, sizeof line, in)) {
        line[sizeof line - 1] = 0;
        char* eq = std::strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* v = eq + 1;
        size_t n = std::strlen(v);
        while (n && (v[n - 1] == '\n' || v[n - 1] == '\r')) v[--n] = 0;
        f(line, v);
    }
    std::fclose(in);
    return true;
}
float clamp01f(float v) { return v >= 0.0f && v <= 1.0f ? v : (v > 1.0f ? 1.0f : 0.0f); }  // (NaN: 0)
}  // namespace

bool loadSettingsFile(const char* path, Settings& s) {
    return readKeys(path, [&](const char* k, const char* v) {
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
        if (is("hints_seen")) s.hintsSeen = static_cast<u32>(std::strtoul(v, nullptr, 10)) & 0x7FFu;  // (eleven hints)
        if (is("runs_ended")) s.runsEnded = static_cast<u32>(std::strtoul(v, nullptr, 10));
    });
}

bool saveSettingsFile(const char* path, const Settings& s) {
    char text[640];
    const int n = std::snprintf(text, sizeof text,
                                "variant=%u\nmaster=%.3f\nmusic=%.3f\neffects=%.3f\nhaptics=%u\nswap_sides=%d\nauto_aim=%d\n"
                                "effects_tier=%u\nframe_rate=%u\nreduce_flashing=%d\nnotice_seen=%d\nprobed_tier=%u\nmusic_host_only=%d\n"
                                "auto_nova=%d\nui_scale=%u\nhints_seen=%u\nruns_ended=%u\n",
                                s.variant, s.master, s.music, s.effects, s.haptics, s.swapSides ? 1 : 0, s.autoAim ? 1 : 0, s.effectsTier,
                                s.frameRate, s.reduceFlashing ? 1 : 0, s.noticeSeen ? 1 : 0, s.probedTier, s.musicHostOnly ? 1 : 0,
                                s.autoNova ? 1 : 0, s.uiScale, s.hintsSeen, s.runsEnded);
    return n > 0 && n < static_cast<int>(sizeof text) && saveFileAtomic(path, text, static_cast<size_t>(n));
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

bool loadHighScoresFile(const char* path, HighScores& h) {
    h.count = 0;
    return readKeys(path, [&](const char* k, const char* v) {
        if (std::strcmp(k, "run") != 0 || h.count >= HighScores::kMax) return;
        HighScore e;
        unsigned long long score = 0;
        unsigned track = 1, chain = 1, secs = 0, kills = 0, set = 0;
        if (std::sscanf(v, "%llu %u %u %u %u %u", &score, &track, &chain, &secs, &kills, &set) >= 5) {  // (M6: the set's flag, sixth)
            if (track < 1 || track > 999) return;  // (a damaged line: not a run this game could play)
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

bool saveHighScoresFile(const char* path, const HighScores& h) {
    char text[1024];
    int n = 0;
    for (u32 k = 0; k < h.count && n >= 0 && n < static_cast<int>(sizeof text) - 80; ++k) {
        const HighScore& e = h.entry[k];
        n += std::snprintf(text + n, sizeof text - static_cast<size_t>(n), "run=%llu %u %u %u %u %u\n",
                           static_cast<unsigned long long>(e.score), e.track, e.bestChain, e.seconds, e.kills, e.setComplete ? 1u : 0u);
    }
    return n >= 0 && saveFileAtomic(path, text, static_cast<size_t>(n));
}

}  // namespace rf
