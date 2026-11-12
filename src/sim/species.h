#pragma once
// The enemy genome (GAME_DESIGN 7.1): species = family x tier x variant, every number from
// design/species.json (embedded at build time, parsed once): a species' health, speed, radius, score,
// threat and Shards are the family's base times the tier and variant multipliers. Behaviour code is
// per family (sim/world_enemies.cpp), never per species. M3 has behaviour for the six families of the
// first slice; the table holds all 6 x 5 x 3 = 90 of their species, so pictures and tests can use tiers
// the slice's roster does not reach (a medium or large kill needs tier III or V).

#include "core/types.h"

namespace rf::sim {

enum class Family : u8 { Drifter, Hound, Shoal, Ram, Carom, Sentry, Count };
constexpr u32 kFamilyCount = static_cast<u32>(Family::Count);
const char* familyName(Family f);  // "drifter", ... (also the glyph family in design/glyphs.json)

enum Variant : u8 { kVariantA = 0, kVariantB = 1, kVariantC = 2 };

// A family's base numbers and motion keys (species.json "families").
struct FamilyData {
    float health = 1, speed = 0, radius = 14, score = 0, threat = 1;
    u8 hue = 0;  // look.md 2: fodder -> enemy hue 1 (index 0), hunter -> 2, special -> 3
    // motion (only the family's own keys are set)
    float wanderTurn = 0;                                 // drifter
    float turnRate = 0;                                   // hound, rad/s
    float flockRadius = 0, separation = 0;                // shoal
    u32 groupMin = 1, groupMax = 1;
    float dashSpeed = 0, telegraphBeats = 1, dashBeats = 1, restBeats = 2;  // ram
    float speedGainPerBounce = 0, maxSpeedFactor = 1;     // carom
    u32 patternArms = 0, fireEverySteps = 2;              // sentry
    float bulletSpeed = 0;
};

struct Species {
    char id[16] = {};  // "hound_2_B"
    Family family = Family::Drifter;
    u8 tier = 1;       // 1..5
    u8 variant = kVariantA;
    u8 hue = 0;
    u8 shards = 1;
    float health = 1, speed = 0, radius = 14, score = 0, threat = 1;
    // look.md 5 kill recipe: 0 small (tier I-II), 1 medium (III-IV), 2 large (V, elites)
    u8 killSize() const { return tier <= 2 ? 0 : (tier <= 4 ? 1 : 2); }
    bool ranged() const { return variant == kVariantB && (family == Family::Hound || family == Family::Sentry); }
};

class SpeciesTable {
public:
    static constexpr u32 kCount = kFamilyCount * 5 * 3;
    static constexpr u32 kMaxRoster = 16;

    // Parses the embedded design/species.json. False (logged) when a key the game needs is missing.
    bool load();

    static u16 index(Family f, u32 tier, u32 variant) {
        return static_cast<u16>((static_cast<u32>(f) * 5 + (tier - 1)) * 3 + variant);
    }
    const Species& get(u16 i) const { return species_[i]; }
    i32 find(const char* id) const;  // "drifter_1_A"; -1 when unknown
    const FamilyData& family(Family f) const { return families_[static_cast<u32>(f)]; }

    // enemyBullet
    float bulletSpeed = 240.0f;
    float bulletRadius = 5.0f;

    // firstSlice: the roster with debuts and the Groove budget per bar by Track and cycle.
    struct RosterEntry {
        u16 species = 0;
        u8 track = 1, cycle = 1;
    };
    RosterEntry roster[kMaxRoster];
    u32 rosterCount = 0;
    float budget[2][8] = {};  // [track 1..2][cycle]
    u32 budgetCycles[2] = {};
    // Track 1 runs Short cycles (24 bars), Track 2 Standard ones (48 bars) (species.json note, GAME_DESIGN 4).
    static constexpr u32 kCycleBars[2] = {24, 48};

private:
    FamilyData families_[kFamilyCount];
    Species species_[kCount];
};

}  // namespace rf::sim
