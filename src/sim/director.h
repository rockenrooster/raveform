#pragma once
// The director (GAME_DESIGN 4 and 8; M3 stage B): the run as a DJ set. Every bar of the run has a place
// in the phrase cycle (Groove, Build, Drop, Break), computed here from the bar number and the few things
// that happened (which Track the run is in, when the boss died). The world spends the threat budget on
// formations from it (sim/world_spawner.cpp) and the band is told where it is (game/play.cpp), so the
// sim is the one source of the form and the music follows it on bar lines.
//
// M5 part 1: each Track ends in its own boss (Kaleidos, Longwave, The Choir; sim/boss.h bossOfSong) and from Track 2 on
// the middle cycle's Drop brings a mini-boss and holds while it lives (at most 8 bars past its 16). The loop begins
// after the last Track of the set list (DirectorState::songs), never at a fixed Track number.
//
// Track 1 "First Light" plays three Short cycles (8/4/8/4 bars; play test 2: it had four); its third Drop
// brings Kaleidos and is held while Kaleidos lives. Its death schedules the Track-change Break (eight bars:
// tempo, key and palette cross-fade, conductor.json trackChange) into Track 2 "Solar Wind", three Standard
// cycles (12/4/16/4 since play test 2; it was 16/8/16/8) ending in Kaleidos again, then Track 3 "Reef" the same.
// After that the run loops by the endless rule (GAME_DESIGN 4.2, last row: +4 BPM per Track, capped at 174) with
// the budget still rising.

#include "sim/sim_types.h"
#include "sim/species.h"

namespace rf::sim {

enum class Section : u8 { Groove, Build, Drop, Break };  // the order of audio::Section
const char* sectionLabel(Section s);                      // "GROOVE", ...

constexpr u32 kSectionBars[2][4] = {{8, 4, 8, 4}, {12, 4, 16, 4}};  // GAME_DESIGN 4.1 (play test 2): Short, Standard
constexpr u32 kTrackChangeBars = 8;  // the band's Track-change Break (conductor.json trackChange)
constexpr u32 kNoBar = 0xFFFFFFFFu;

// The set list (GAME_DESIGN 4.2, the lead's decision of 2026-11-12): a run plays every Track the game has, in order,
// and the loop (section 8: ramps, no refills, the halo burn, the ENCORE card) begins after the last of them. Its
// length is data (World::setSetList: the music's Tracks the simulation has content for), never a fixed number;
// kSongs is only the length when nothing says otherwise (the first slice's three). kMaxSongs: GAME_DESIGN 4.2's ten.
constexpr u32 kSongs = 3;
constexpr u32 kMaxSongs = 10;
// The song a run Track plays (0 First Light, 1 Solar Wind, 2 Reef, ...; then the loop over all of them), its form,
// cycles and tempo, for a set list of `songs` Tracks.
inline u8 songOfTrack(u32 track, u32 songs) {
    const u32 n = songs ? songs : 1u;
    return static_cast<u8>((track + n - 1) % n);
}
inline u32 formOfTrack(u32 track, u32 songs) { return songOfTrack(track, songs) == 0 ? 0u : 1u; }  // First Light Short
inline u32 cyclesOfTrack(u32 /*track*/) { return 3u; }  // GAME_DESIGN 4.2 (play test 2: Track 1 too)
inline bool inLoop(u32 track, u32 songs) { return track > songs; }
float bpmOfTrack(u32 track, u32 songs);  // the set list's tempi, then +4 per Track up to 174 (GAME_DESIGN 4.2)
const char* songName(u32 song);  // "FIRST LIGHT", "SOLAR WIND", "REEF", ... (GAME_DESIGN 4.2's ten)
// The loop ramps (GAME_DESIGN 8, "The loop is where a run ends", play test 2): a factor once for every Track after the
// first pass (the loop's first Track on), compounding: factor^(track - songs), 1 through the last Track of the set list.
inline float loopRamp(float factor, u32 track, u32 songs) {
    float r = 1.0f;
    for (u32 t = songs + 1; t <= track && t < 256; ++t) r *= factor;
    return r;
}

struct Phrase {
    u32 bar = 0;               // bars since the run began
    u8 track = 1;              // the run's Track (1, 2, 3, then 4 = First Light again, ...)
    u8 song = 0;               // its music: 0 First Light, 1 Solar Wind, 2 Reef
    u8 cycle = 1, cycles = 3;  // 1-based; 0 in the Track-change Break
    Section section = Section::Groove;
    u16 sectionBar = 0, sectionBars = 8;  // a held boss Drop keeps counting past sectionBars
    bool trackChange = false;  // the Break after a boss: into `track`
    bool bossDrop = false;     // the Track's last Drop: the boss enters on its downbeat and holds it
    bool miniDrop = false;     // M5: the middle cycle's Drop from Track 2 on: a mini-boss enters on its downbeat
    bool held = false;         // M5: a bar of the middle Drop past its length, held for the mini-boss
    float bpm = 120.0f;        // this bar's tempo (the Build's last bar at 85%; the Track change ramps)
    float trackBpm = 120.0f;   // the Track's own tempo

    // Surge: the first half of a Drop, in two gears (GAME_DESIGN 4.1, conductor.json drop.surgeSteps):
    // one grid step faster for the first half of the Surge, two for the second.
    bool surge() const { return section == Section::Drop && sectionBar < sectionBars / 2; }
    u8 surgeGear() const { return !surge() ? 0 : (sectionBar < sectionBars / 4 ? 1 : 2); }
    bool buildLastBar() const { return section == Section::Build && sectionBar + 1 == sectionBars; }
    bool sameSection(const Phrase& o) const {
        return track == o.track && section == o.section && cycle == o.cycle && trackChange == o.trackChange;
    }
};

// What the director remembers between bars (part of the world's state and its hash).
struct DirectorState {
    double startTick = 0.0;    // the run's first bar line (musical ticks)
    u8 track = 1;              // the Track being played (the Track-change Break already belongs to the next)
    u32 trackBar = 0;          // run bar of the current Track's first Groove
    u32 changeBar = kNoBar;    // run bar of the Track-change Break into track + 1 (kNoBar: not scheduled)
    bool bossSpawned = false;  // this Track's boss has entered
    // Budget.
    float bank = 0.0f;
    i32 pending = -1;          // the next formation's species, waiting until the bank buys it
    float budgetPerBar = 0.0f;
    float assist = 1.0f;       // GAME_DESIGN 8: 0.8-1.3
    u8 cleanCycles = 0;
    bool hitThisCycle = false;
    // Boosts: one drops roughly every 20-30 s (GAME_DESIGN 6.1), from the next kill once due.
    i32 boostTicks = 0;        // sim ticks until the next Boost is due
    bool boostDue = false;
    u32 boostsDropped = 0;
    u32 crowdLost = 0;         // fodder killed since the last bar line (the crowd replaces it)
    float crowdDebt = 0.0f;    // C2: the frame's running shortfall against the target (owed on top of the gap)
    u32 crowdKills = 0;        // C2: fodder killed since the last bar line (the debt never exceeds it)
    u32 lastBar = kNoBar;      // the last bar the director planned
    float crowdDebtP[kMaxPlayers] = {};  // M4 co-op: each player's frame keeps its own debt
    u8 burnedTrack = 0;        // GAME_DESIGN 8 (the halo burns down): the last loop Track whose first downbeat burnt the halos
    u8 songs = kSongs;         // the set list's length (World::setSetList); the loop begins at Track songs + 1
    // M5 part 1 (minibosses.md): the middle cycle's Drop holds while its mini-boss lives, at most kMiniHoldBars past its
    // length. midHolding: the mini-boss lives (the Drop is open-ended up to the cap); midHold: the bars it was held,
    // once its end is known (the run bars after it move by that much).
    bool midHolding = false;
    u8 midHold = 0;
    bool miniSpawned = false;  // this Track's mini-boss has entered
    u8 lastMini = 0xFF;        // the mini-boss met in the Track before (BossKind; 0xFF none)
};

// Where bar `bar` of the run sits, given what the director knows now.
Phrase phraseAt(const DirectorState& d, u32 bar);
// Seconds of music per run bar is not constant (the Build's last bar, the Track change): the tempo at a
// musical tick, for clocks that are simulated rather than heard (headless runs, pictures).
float tempoAt(const DirectorState& d, double tick);
// The Groove budget per bar of a Track's cycle (species.json firstSlice.threatBudgetPerBar; past its
// last value it keeps rising, tuning::kEndlessBudgetStep per cycle).
float baseBudget(const float budget[3][8], const u32 budgetCycles[3], u32 track, u32 cycle);
// The crowd (GAME_DESIGN 8): fodder alive near the ships the director keeps, by Track and cycle
// (species.json firstSlice.crowd; past Track 3 it rises loopStepPerTrack a Track up to its max), and the
// section's scale on it.
float crowdTarget(const SpeciesTable::Crowd& c, u32 track, u32 cycle);
float crowdScale(const SpeciesTable::Crowd& c, const Phrase& p);
// The section's scale on that budget (GAME_DESIGN 8): Groove 1.0, Build 1.0 -> 1.6, Drop 2.0 -> 1.2,
// Break 0; the boss's Drop 0.3 (kaleidos.md).
float sectionScale(const Phrase& p);

}  // namespace rf::sim
