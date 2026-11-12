#pragma once
// Settings and local high scores (GAME_DESIGN 12, TECH_SPEC 11): plain key=value text in the app's private
// storage (SDL_GetPrefPath), written to a temporary file and renamed. Loaded once at start, saved when a
// screen that changes them closes (Settings DONE, the first-launch notice, a run's end).

#include "core/types.h"

namespace rf {

struct Settings {
    // SOUND
    u8 variant = 0;              // design/music/variants.json: 0 Club (the default), 1 Arcade, 2 Velvet
    float master = 1.0f, music = 1.0f, effects = 1.0f;  // 0..1
    u8 haptics = 1;              // 0 OFF, 1 SOFT, 2 STRONG
    // CONTROLS
    bool swapSides = false;
    bool autoAim = true;
    // LOOK
    u8 effectsTier = 0;          // 0 Auto, 1 Low, 2 Medium, 3 High, 4 Ultra
    u8 frameRate = 0;            // 0 Auto, 1 60, 2 90, 3 120
    bool reduceFlashing = false;
    // First launch.
    bool noticeSeen = false;     // the photosensitivity notice was acknowledged
    u32 probedTier = 0;          // the tier probe's pick (0: not probed yet), 1 Low .. 4 Ultra
};

bool loadSettings(Settings& s);
bool saveSettings(const Settings& s);

struct HighScore {
    u64 score = 0;
    u32 track = 1;
    u32 bestChain = 1;
    u32 seconds = 0;
    u32 kills = 0;
};

// Top 10 solo scores (co-op tables come with co-op).
struct HighScores {
    static constexpr u32 kMax = 10;
    HighScore entry[kMax];
    u32 count = 0;
    u64 best() const { return count ? entry[0].score : 0; }
    // Inserts in order; returns the rank (0 = new best) or -1 when it did not make the table.
    i32 add(const HighScore& h);
};

bool loadHighScores(HighScores& h);
bool saveHighScores(const HighScores& h);

// The app's private file `name` (SDL_GetPrefPath); allocates (load and save time only).
void appFilePath(char* out, size_t capacity, const char* name);
// Write `text` to `name` through a temporary file and a rename.
bool writeAppFile(const char* name, const char* text, size_t length);

}  // namespace rf
