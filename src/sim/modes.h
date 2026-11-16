#pragma once
// M8 (docs/MODES_PLAN.md 2, design/modes/): the run's setup (which mode, which Track, together or each for themselves) and the
// modes' numbers, read from design/modes/modes.json at start-up (never retyped). THE SET is the setup's default: every run that
// does not ask for another mode is the set, bit for bit (MODES_PLAN briefing 4).

#include "core/types.h"

namespace rf::sim {

enum class Mode : u8 { Set, OneTrack, Headliners, FullBlast, Tracers, Songline, Jam, Count };
constexpr u32 kModeCount = static_cast<u32>(Mode::Count);
constexpr u32 kModeTracks = 12;  // the set's ten and the two bonus Tracks (11 Mirrorball, 12 Toybox)

struct RunSetup {
    Mode mode = Mode::Set;
    u8 track = 1;         // 1..12 for a mode that takes one; else 1
    bool versus = false;  // each player for themselves (design/modes/README.md 8)
    bool help = true;     // versus: the underdog's help (V7), on unless the host switches it off
    bool operator==(const RunSetup& o) const { return mode == o.mode && track == o.track && versus == o.versus && help == o.help; }
    bool operator!=(const RunSetup& o) const { return !(*this == o); }
};

const char* modeId(Mode m);                   // "set", "one", "headliners", "blast", "tracers", "songline", "jam"
bool modeFromId(const char* id, Mode& out);
// "<id>[:<track>]" (arguments): false on an unknown mode or a Track the mode does not take.
bool parseModeArg(const char* text, RunSetup& out, char* moment = nullptr, u32 momentCap = 0);

// A form's section (modes.json "forms"): its kind and length in bars (0: held, as a boss's Drop or JAM's Groove).
enum class FormKind : u8 { Groove, Build, Drop, Break, BossDrop, TrackChange };
struct FormSection {
    FormKind kind = FormKind::Groove;
    u16 bars = 0;
};
struct Form {
    static constexpr u32 kMax = 12;
    FormSection s[kMax];
    u32 count = 0;
    u32 bars() const;  // the form's whole length (a held section counts 0)
};

// Where a run's bar sits in a mode's form (docs/MODES_PLAN.md 2.2): its section, the bar in it, the section's length, the bars left
// to the form's end. A held section (0 bars: JAM's Groove, a boss's Drop) holds every bar from its start; a bar past the form's end
// is `ended`, at the last section's last bar.
struct FormPlace {
    u32 section = 0;
    FormKind kind = FormKind::Groove;
    u32 sectionBar = 0, sectionBars = 0;  // (sectionBars 0: held)
    u32 barsLeft = 0;                     // to the form's end (0 when held or ended)
    bool held = false, ended = false;
};
FormPlace formPlace(const Form& f, u32 bar);

struct ModesData {
    struct Info {
        char id[12] = {}, name[24] = {}, caption[40] = {}, nameCard[32] = {};
        bool takesTrack = false, stage = false;
        u8 trackLo = 1, trackHi = 10, home = 1;
        u16 timedBars = 0;
        bool trackColours = false;  // ONE TRACK: the chosen Track's accent to its first enemy hue
        float from[2] = {}, to[2] = {};  // OKLCH [L, hue] at full chroma
        char counts[8] = {};  // versus: score, acts, tiles, gates, pops
    } mode[kModeCount];
    struct Bonus {
        u8 track = 0, crowdOf = 1;
        u16 bpm = 0;
        char name[16] = {}, palette[16] = {}, homeOf[12] = {};
    } bonus[2];
    struct Forms {
        Form headlinersFirstAct, headlinersAct, blast, timed48, jam, jamDrop, popOff;
    } forms;
    struct Stage {
        float widthWu = 0, heightWu = 0, fitMarginWu = 0, minZoom = 0, tileWu = 0, spawnClearWu = 0;
        u32 tilesX = 0, tilesY = 0, rigFixtures = 0;
        float rigSpacingWu = 0, rigYFromTopWu = 0, rigLevel = 0;
    } stage;
    struct Kit {
        u8 atTrackStart[10] = {}, atBoss[10] = {};
        bool measured = false;
    } kit;
    struct OneTrack {
        u8 outroBars = 4;
    } oneTrack;
    struct LoadIn {  // README.md 5
        u32 bars = 2, perSixteenth = 1, perSixteenthPast32 = 2, bedLayerEvery = 4;
        float ringWu = 300.0f, spiralTurns = 0.25f, iconScale = 0.5f, graceWu = 600.0f, closeRingBeats = 0.5f;
        float closeRingWu[2] = {90.0f, 400.0f};
    } loadIn;
    struct Versus {
        float shoveWu = 0, shoveSeconds = 0, underdogShare = 0;
        float crownWidthWu = 0, crownAboveWu = 0, crownLevel = 0;
        u32 crownSparks = 0;
    } versus;

    const Info& of(Mode m) const { return mode[static_cast<u32>(m) < kModeCount ? static_cast<u32>(m) : 0]; }
    bool takes(Mode m, u32 track) const;  // the mode plays that Track
    bool load();
};
u64 modesDataDigest();  // design/modes/modes.json and the bonus Tracks (co-op peers must match)

}  // namespace rf::sim
