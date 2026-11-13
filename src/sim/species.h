#pragma once
// The enemy genome (GAME_DESIGN 7.1): species = family x tier x variant, every number from
// design/species.json (embedded at build time, parsed once): a species' health, speed, radius, score,
// threat and Shards are the family's base times the tier and variant multipliers. Behaviour code is
// per family (sim/world_enemies.cpp), never per species. M3 has behaviour for the six families of the
// first slice; the table holds all 6 x 5 x 3 = 90 of their species, so pictures and tests can use tiers
// the slice's roster does not reach (a medium or large kill needs tier III or V).

#include "core/types.h"
#include "sim/boss.h"

namespace rf::sim {

// The sixteen families of GAME_DESIGN 7.2 (M5: design/species_m5.json). The first six are the first slice's, in their
// M3 order (a species' index is on the wire and in the hash); the rest are appended.
enum class Family : u8 {
    Drifter, Hound, Shoal, Ram, Carom, Sentry,                // the first slice (M3)
    Flanker, Skitter, Moth, Brood, Blink,                     // M5 part 2 (Tracks 2 and 3)
    Wyrm, Tether, Weaver, Well, Mimic,                        // M5 part 4 (Tracks 4 to 6)
    Count
};
constexpr u32 kFamilyCount = static_cast<u32>(Family::Count);
const char* familyName(Family f);  // "drifter", ... (also the glyph family in design/glyphs.json)
// Families with behaviour code so far (M5 part 2: the first eleven). The director never picks the others.
inline bool familyBuilt(Family f) { return f <= Family::Blink; }
u64 speciesDataDigest();  // a digest of the embedded species.json (M4: peers must match)
u64 bossDataDigest();     // ... and of the boss pages (design/bosses/*.json)

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
    // M5 part 2 (species_m5.json families.*.motion)
    float orbitRadius = 0, closeRate = 0;                 // flanker: circles at orbitRadius, closing closeRate wu/s
    float closePerBar = 0, turnsPerBar = 0, minRadius = 0;  // moth (with orbitRadius): a clock that closes on the beat
    float dodgeDistance = 0, dodgeCooldownBeats = 2;      // skitter
    float miniRadius = 8, miniHealth = 1, miniSpeed = 260, miniScore = 3;  // brood: its minis (the broodling species)
    u32 maxMinisEach = 6;
    float hopEveryBeats = 1, hopWu = 200, strikeTellBeats = 1, strikeSpeed = 700, strikeBeats = 1;  // blink (+ restBeats)
    u32 hops = 4;
    // Variants B and C (species_m5.json variants.<family>; GAME_DESIGN 7.2). Only the family's own are set.
    struct Variants {
        // B
        float aimedShotEveryBars = 0; u32 aimedBullets = 0;       // drifter
        float bulletsPerBounce = 0, bounceFanDeg = 0;             // carom
        u32 petals = 0, petalEverySteps = 2; float petalRotateDeg = 0, petalSpeed = 0;  // moth
        float hatchEveryBars = 1; u32 hatchCount = 0;             // brood
        u32 pathBullets = 0; float pathBulletSpeed = 0;           // blink
        u32 sideShotEverySteps = 4, sideBullets = 0;              // flanker
        float mineLifeBars = 0, mineRadius = 0;                   // skitter
        u32 streamLines = 0; float lineSpeed = 0;                 // shoal
        float wakeBars = 0, wakeWidth = 0;                        // ram
        // C
        float shieldArcDeg = 0, shieldTurnRate = 0;               // hound
        u32 burstBullets = 0; float burstAtRadius = 0;            // moth
        u32 swarmOnDeath = 0;                                     // brood
        float decoyLifeBeats = 0;                                 // blink
        float speedUpPerDeath = 0, speedMaxFactor = 1, speedWithinWu = 0;  // flanker
        float blinkHopWu = 0, blinkCooldownBeats = 2;             // skitter
        u32 splitInto = 0, childTierDrop = 1;                     // drifter
        u32 splitEveryBounces = 0, maxSplits = 0;                 // carom
        float domeRadius = 0, domeWithinWu = 0;                   // sentry
        u32 reboundDashes = 0;                                    // ram
        float spearTellBeats = 1, spearSpeed = 0;                 // shoal
    } v;
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
    // A shooter that tells a beat ahead (kGlint) and so gets the edge indicator off screen (GAME_DESIGN 8: fairness).
    bool ranged() const {
        return variant == kVariantB && (family == Family::Hound || family == Family::Sentry || family == Family::Drifter ||
                                        family == Family::Flanker || family == Family::Moth);
    }
};

class SpeciesTable {
public:
    static constexpr u32 kFamilySpecies = kFamilyCount * 5 * 3;  // 240 (GAME_DESIGN 7.1)
    // M5: one more row after them, the Brood's mini (the broodling: families.brood.motion.mini*, chasing like a Hound).
    static constexpr u32 kBroodling = kFamilySpecies;
    static constexpr u32 kCount = kFamilySpecies + 1;
    static constexpr u32 kMaxRoster = 128;
    static constexpr u32 kMaxTracks = 10;  // budgets and crowd targets by Track (species_m5.json: Tracks 1 to 10)

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
    float budget[kMaxTracks][8] = {};  // [track 1..10][cycle]: threats only since 2026-11-12
    u32 budgetCycles[kMaxTracks] = {};
    u32 budgetTracks = 0;              // the last Track with a budget
    // firstSlice.crowd (GAME_DESIGN 8, the crowd and the threats): the population the director keeps in and
    // just around each player's frame, by Track and cycle, scaled by section (M3 stage C2: count, arrive, stray).
    struct Crowd {
        float target[kMaxTracks][8] = {};
        u32 targetCycles[kMaxTracks] = {};
        u32 targetTracks = 0;  // the last Track with a target
        float loopStep = 20.0f, max = 400.0f;
        float groove = 1.0f, build[2] = {1.0f, 1.3f}, drop[2] = {1.6f, 1.2f}, breakScale = 0.0f, bossDrop = 0.5f;
        float miniDrop = 0.75f;  // M5 (minibosses.json common.crowd.sectionScale): the mini-boss's Drop keeps this share
        float countFrameScale = 1.25f;  // count: the frame grown this much each way
        float floodOutsideWu[2] = {120.0f, 320.0f}, ringFrameScale = 1.15f, cornerOutsideWu = 150.0f;  // arrive
        float refillBars = 2.0f;
        u32 formationMin = 16, formationMax = 80;
        float strayFrameScale = 2.0f;  // stray: outside every frame grown this much ...
        u32 strayBars = 2;             // ... for this many bars
    } crowd;
    // Track 1 runs three Short cycles (24 bars), later Tracks three Standard ones (36 bars since play test 2)
    // (species.json note, GAME_DESIGN 4).
    static constexpr u32 kCycleBars[2] = {24, 36};

    // The bosses (design/bosses/*.json), loaded with the species: Kaleidos (the first slice's), and since M5 part 1
    // Longwave, The Choir and the mini-bosses.
    BossData boss;
    LongwaveData longwave;
    ChoirData choir;
    MiniData minis;

private:
    FamilyData families_[kFamilyCount];
    Species species_[kCount];
};

}  // namespace rf::sim
