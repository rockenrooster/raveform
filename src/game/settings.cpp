#include "game/settings.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <cstdio>

#include "core/log.h"
#include "core/save_file.h"

namespace rf {

// The app's files in SDL_GetPrefPath(); the reading and writing is game/save_data.cpp's (M7: crash-safe, by path).

void appFilePath(char* out, size_t cap, const char* name) {
    char* pref = SDL_GetPrefPath("raveform", "raveform");
    std::snprintf(out, cap, "%s%s", pref ? pref : "", name);
    SDL_free(pref);
}

bool writeAppFile(const char* name, const char* text, size_t length) {
    char path[512];
    appFilePath(path, sizeof path, name);
    return saveFileAtomic(path, text, length);
}

bool loadSettings(Settings& s) {
    char path[512];
    appFilePath(path, sizeof path, "settings.txt");
    const bool found = loadSettingsFile(path, s);
    if (found) RF_LOG("settings: loaded (sound %u, effects %u, reduce flashing %s)", s.variant, s.effectsTier, s.reduceFlashing ? "on" : "off");
    return found;
}

bool saveSettings(const Settings& s) {
    char path[512];
    appFilePath(path, sizeof path, "settings.txt");
    const bool ok = saveSettingsFile(path, s);
    if (!ok) RF_WARN("settings: could not be saved");
    return ok;
}

bool loadHighScores(HighScores& h) {
    char path[512];
    appFilePath(path, sizeof path, "highscores.txt");
    return loadHighScoresFile(path, h);
}

bool saveHighScores(const HighScores& h) {
    char path[512];
    appFilePath(path, sizeof path, "highscores.txt");
    return saveHighScoresFile(path, h);
}

}  // namespace rf
