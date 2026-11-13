#pragma once
// The engine's own state shared by its two sources (engine.cpp: the band; content_sound.cpp, M5: the content's sound and
// the upgrades' patterned sound). Not for anyone else: include audio/engine.h.

#include "audio/engine.h"

namespace rf::audio {

constexpr float kRollLo = 0.35f, kRollHi = 1.0f;  // staged snare roll level

struct Engine::Player {
    u8 fire = 0;  // 0 off, 1 every grid step, 2 every other step (M3: the auto-aim assist's half-rate grid)
    Timbre timbre = Timbre::P1Saw;
    u8 riff[static_cast<u32>(Riff::Count)] = {};
    u8 split = 0, doubleTime = 0, pierce = 0, overcharge = 0, crit = 0;
    bool prism = false, hyper = false;
    float distance = 0.0f;
    bool beacon = false;  // M4: down, its beacon sounding (look.md 6)
    float beaconPan = 0.0f;
    // M5 (cues.h): every Mod's stacks (the first slice's five above are kept in step), the Boosts active, the Fusions.
    u8 mod[kModCount] = {};
    u16 boosts = 0;
    u8 fusions = 0;
    u8 lastNote = 69;  // the last lead note it played (Fork's fifth, the bounce blips, Seeker's glide)
};

struct Engine::BarPlan {
    i64 tick = 0;
    u32 index = 0;
    bool silent = true;
    Section section = Section::Groove;
    u32 inSection = 0, sectionBars = 1;
    const Chord* chord = nullptr;
    float pat[kPatCount] = {};       // per-layer level (0: off)
    bool hatOffBreak = false;
    BassKey bass = BassKey::None;
    float bassFade = 1.0f;
    LeadMode lead = LeadMode::Off;   // Staged
    float pad = 0.0f, padBright = 1.0f;
    i64 rollDiv = 0;                 // 0: no snare roll
    u32 rollBar = 0, rollBars = 1;
    float rollLo = kRollLo, rollHi = kRollHi;
    bool lastBuildBar = false;
    bool fill = false;
    bool pulseEveryBeat = false;
    bool riffDouble = false;         // hyper: Riff patterns at double rate
    bool sparse[kMaxPlayers] = {};   // Break: the bed echoes this player's arp
    u32 leadGrid[kMaxPlayers] = {};
    bool leadStopsHalf = false;      // Build's last bar: the lead stops after beat 2
    // M5: the upgrades' share of the bar (upgradeBar). `upgrades`: anything to play at all.
    bool upgrades = false;
    bool riffsOn = false;            // the Riffs' layers play (not in the Break or the Build's last bar)
    u8 riff[static_cast<u32>(Riff::Count)] = {};  // each Riff's level as heard (the highest owner's; a Fusion takes its part)
    u16 boosts = 0;                  // any player's Boosts
    u8 fusions = 0;                  // any player's Fusions
    bool timeStretch = false;        // the drums at half rate: this bar is the first (0) or second (1) of their pattern
    u8 stretchHalf = 0;
    float padOpen = 1.0f;            // Crescendo (the local player's)
};

}  // namespace rf::audio
