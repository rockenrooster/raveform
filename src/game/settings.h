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
    bool musicHostOnly = false;  // M4 (GAME_DESIGN 12): in co-op, the music plays from the host's phone only
    // CONTROLS
    bool swapSides = false;
    bool autoAim = true;
    bool autoNova = true;        // play test 1, second pass (GAME_DESIGN 5): a Nova fires instead of losing the last cell
    // LOOK
    u8 effectsTier = 0;          // 0 Auto, 1 Low, 2 Medium, 3 High, 4 Ultra
    u8 frameRate = 0;            // 0 Auto, 1 60, 2 90, 3 120
    bool reduceFlashing = false;
    u8 uiScale = 1;              // M6: HUD text and touch controls 0 90%, 1 100%, 2 115%, 3 130%
    bool vivid = false;          // M6 (TECH_SPEC 5.2): COLOUR VIVID, Display-P3 where the screen offers it (STANDARD: sRGB)
    // First launch.
    bool noticeSeen = false;     // the photosensitivity notice was acknowledged
    u32 probedTier = 0;          // the tier probe's pick (0: not probed yet), 1 Low .. 4 Ultra
    // M6 (screens.md "First-run hints"): the hints shown on this device (a bit each), and runs played to their end. M8: twenty
    // bits (the modes' nine hints after the set's eleven: design/modes/screens.md "First-run hints").
    u32 hintsSeen = 0;
    u32 runsEnded = 0;
};

bool loadSettings(Settings& s);
bool saveSettings(const Settings& s);

struct HighScore {
    u64 score = 0;
    u32 track = 1;
    u32 bestChain = 1;
    u32 seconds = 0;
    u32 kills = 0;
    bool setComplete = false;  // M6: the run finished the set list (a gold star in the table)
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

// M8 (design/modes/README.md 9): modes.txt: the mode last played, each mode's last Track, and the modes' records. A solo run's
// only; versus keeps none (the room's crowns), JAM keeps none. A missing or damaged file is no records.
struct ModeRecords {
    static constexpr u32 kModes = 7, kTracks = 12;
    u8 mode = 0;                // the mode last played (0-6): the title's pill
    u8 track[kModes] = {};      // the Track last chosen for each mode (0: none yet, its home)
    struct One {
        u64 score = 0;
        bool cleared = false;
        u32 seconds = 0;
    } one[10];                  // ONE TRACK, Tracks 1-10
    u32 headlinersActs = 0, headlinersSeconds = 0;
    struct Timed {
        u64 score = 0;
        u32 second = 0;         // the mode's second number: the best wave, full houses, gates in a hundred
    } blast[kTracks], tracers[kTracks], songline[kTracks];
    struct Team {
        u64 score = 0;
        u8 players = 0;
    } team[kModes][kTracks];    // a co-op run's best team score and its player count, by mode and Track
};
bool loadModeRecords(ModeRecords& r);
bool saveModeRecords(const ModeRecords& r);

// The app's private file `name` (SDL_GetPrefPath); allocates (load and save time only).
void appFilePath(char* out, size_t capacity, const char* name);
// Write `text` to `name` through a temporary file and a rename (core/save_file: flushed, then renamed over the old file).
bool writeAppFile(const char* name, const char* text, size_t length);

// M7 (crash-safe saves): the same by path (game/save_data.cpp; no SDL, so the tests link it). A missing file loads nothing
// (false); a short or damaged one loads what it holds that is valid, the rest stays as given (the defaults).
bool loadSettingsFile(const char* path, Settings& s);
bool saveSettingsFile(const char* path, const Settings& s);
bool loadHighScoresFile(const char* path, HighScores& h);
bool saveHighScoresFile(const char* path, const HighScores& h);
bool loadModeRecordsFile(const char* path, ModeRecords& r);
bool saveModeRecordsFile(const char* path, const ModeRecords& r);

}  // namespace rf
