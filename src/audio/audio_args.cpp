#include "audio/audio_args.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"

namespace rf {

namespace {

bool eq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

bool oneOf(const char* v, const char* const* list, u32 n) {
    for (u32 i = 0; i < n; ++i)
        if (eq(v, list[i])) return true;
    return false;
}

bool copy(char* dst, size_t cap, const char* src) {
    const size_t n = std::strlen(src);
    if (n + 1 > cap) return false;
    std::memcpy(dst, src, n + 1);
    return true;
}

}  // namespace

bool isAudioArg(const char* arg) {
    static const char* const kArgs[] = {"--band", "--no-audio", "--track",   "--sound",       "--intensity", "--section",
                                        "--hold", "--fire",     "--haptics", "--sync-offset", "--silent", "--audio-profile"};
    return oneOf(arg, kArgs, countOf(kArgs));
}

bool parseAudioArg(const char* arg, const char* next, AudioArgs& a, bool& usedNext) {
    usedNext = false;
    if (eq(arg, "--no-audio")) {
        a.noAudio = true;
        return true;
    }
    if (eq(arg, "--silent")) {
        a.silent = true;
        return true;
    }
    if (eq(arg, "--hold")) {
        a.hold = true;
        return true;
    }
    if (eq(arg, "--fire")) {
        a.fire = true;
        return true;
    }
    if (eq(arg, "--audio-profile")) {
        a.audioProfile = true;
        return true;
    }
    if (!next) return false;
    usedNext = true;
    if (eq(arg, "--band")) {
        static const char* const kModes[] = {"off", "play", "staged", "full", "stress", "metronome"};
        return oneOf(next, kModes, countOf(kModes)) && copy(a.band, sizeof a.band, next);
    }
    if (eq(arg, "--track")) return copy(a.track, sizeof a.track, next);
    if (eq(arg, "--sound")) return copy(a.sound, sizeof a.sound, next);
    if (eq(arg, "--section")) {
        static const char* const kSections[] = {"groove", "build", "drop", "break"};
        return oneOf(next, kSections, countOf(kSections)) && copy(a.section, sizeof a.section, next);
    }
    if (eq(arg, "--intensity")) {
        char* end = nullptr;
        a.intensity = std::strtof(next, &end);
        return end != next && *end == '\0' && a.intensity >= 0.0f && a.intensity <= 1.0f;
    }
    if (eq(arg, "--haptics")) {
        if (!eq(next, "on") && !eq(next, "off")) return false;
        a.haptics = eq(next, "on") ? 1 : 0;
        return true;
    }
    if (eq(arg, "--sync-offset")) {
        char* end = nullptr;
        a.syncOffsetMs = std::strtof(next, &end);
        a.syncOffsetSet = true;
        return end != next && *end == '\0' && a.syncOffsetMs >= -500.0f && a.syncOffsetMs <= 1000.0f;
    }
    return false;
}

void logAudioArgs(const AudioArgs& a) {
    char offset[16] = "saved";
    if (a.syncOffsetSet) std::snprintf(offset, sizeof offset, "%.0f", a.syncOffsetMs);
    RF_LOG("args (audio): band=%s no_audio=%d silent=%d track=%s sound=%s intensity=%.2f section=%s hold=%d fire=%d haptics=%d "
           "sync_offset=%s profile=%d",
           a.band[0] ? a.band : "(default)", a.noAudio ? 1 : 0, a.silent ? 1 : 0, a.track[0] ? a.track : "(default)",
           a.sound[0] ? a.sound : "(default)", a.intensity, a.section[0] ? a.section : "(default)", a.hold ? 1 : 0,
           a.fire ? 1 : 0, a.haptics, offset, a.audioProfile ? 1 : 0);
}

}  // namespace rf
