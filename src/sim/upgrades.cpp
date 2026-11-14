#include "sim/upgrades.h"

#include <cstdio>

#include "sim/tuning.h"

namespace rf::sim {

using namespace tuning;

namespace {
// GAME_DESIGN 6.2 / 6.3 (names, caps; play test 2: Extra Cell 4, Regen 3; play test 3: Fork 1; the owner, 2026-11-14, eb10554:
// Split 4, a lane a rank, and Double-Time 2 again); icons from design/icons.json. In the tables' order.
const UpgradeInfo kInfo[kUpgradeCount] = {
    {"SPLIT", "split", 4, true},
    {"DOUBLE-TIME", "double_time", 2, true},
    {"PIERCE", "pierce", 3, true},
    {"RICOCHET", "ricochet", 3, true},
    {"FORK", "fork", 1, true},
    {"SEEKER", "seeker", 3, true},
    {"HEAVY STEP", "heavy_step", 3, true},
    {"ACCENT", "accent", 3, true},
    {"ECHO", "echo", 2, true},
    {"WAVE", "wave", 2, true},
    {"OVERCHARGE", "overcharge", 5, true},
    {"REAR GUARD", "rear_guard", 2, true},
    {"ARC", "arc", 3, true},
    {"BURN", "burn", 3, true},
    {"SHATTER", "shatter", 3, true},
    {"GRAVITY ROUNDS", "gravity_rounds", 2, true},
    {"EXTRA CELL", "extra_cell", 4, false},
    {"REGEN", "regen", 3, false},
    {"AFTERIMAGE", "afterimage", 2, false},
    {"RESERVE", "reserve", 2, false},
    {"SHOCKSTEP", "shockstep", 3, false},
    {"AEGIS", "aegis", 3, false},
    {"LAST LIGHT", "last_light", 1, false},
    {"SLIPSTREAM", "slipstream", 2, false},  // M5 (GAME_DESIGN 6.2, the lead's ruling): +15%, cap 2
    {"MAGNET", "magnet", 4, false},
    {"PROSPECTOR", "prospector", 4, false},
    {"NOVA STOCK", "nova_stock", 3, false},
    {"ENCORE", "encore", 3, false},
    {"LUCKY BREAK", "lucky_break", 2, false},
    {"COLLECTOR", "collector", 1, false},  // (the owner, 2026-11-14: "too op late game ... capped at rank 1"; it was 2)
    {"ON-BEAT CRIT", "on_beat_crit", 3, true},
    {"SYNCOPATION", "syncopation", 2, true},
    {"BAR LINE", "bar_line", 3, true},
    {"CRESCENDO", "crescendo", 2, true},
    {"KICK PULSE", "kick_pulse", 3, false},
    {"AFTERPARTY", "afterparty", 2, true},
    {"ORBITALS", "orbitals", kRiffMaxLevel, true},
    {"PULSE RING", "pulse_ring", kRiffMaxLevel, true},
    {"SENTINEL", "sentinel", kRiffMaxLevel, true},
    {"LANCER", "lancer", kRiffMaxLevel, true},
    {"MINELAYER", "minelayer", kRiffMaxLevel, true},
    {"STORM COIL", "storm_coil", kRiffMaxLevel, true},
    {"SWARM", "swarm", kRiffMaxLevel, true},
    {"SINGULARITY", "singularity", kRiffMaxLevel, true},
    {"MIRROR", "mirror", kRiffMaxLevel, true},
    {"COMET", "comet", kRiffMaxLevel, true},
    {"SWEEPER", "sweeper", kRiffMaxLevel, true},
    {"FLAK", "flak", kRiffMaxLevel, true},
};
const UpgradeInfo kNone{"?", "", 0, false};

// GAME_DESIGN 6.4; durations in bars (Halo: until it has absorbed three hits, at most eight bars).
const BoostInfo kBoosts[kBoostCount] = {
    {"PRISM", "prism", "FIVE-WAY RAINBOW FAN", 4},
    {"HYPER", "hyper", "FIRE RATE X2", 4},
    {"SUPERNOVA ROUNDS", "supernova_rounds", "SHOTS EXPLODE", 4},
    {"COLOSSUS", "colossus", "GIANT, RAMS THROUGH", 2},
    {"TIME STRETCH", "time_stretch", "THE WORLD AT HALF SPEED", 2},
    {"FREEZE FRAME", "freeze_frame", "FREEZE AND SHATTER", 2},
    {"HALO", "halo", "BLOCKS THREE HITS", 8},
    {"MAGNET STORM", "magnet_storm", "PULLS EVERY SHARD", 4},
    {"BLACK HOLE", "black_hole", "PULLS ALL IN, THEN BLOWS", 4},
    {"CHAIN STORM", "chain_storm", "LIGHTNING EVERY BEAT", 4},
    {"CHORUS", "chorus", "TWO GHOST SHIPS", 4},
    {"BEAM", "beam", "A SWEEPING LASER", 4},
    {"RICOCHET FEVER", "ricochet_fever", "+5 BOUNCES, FASTER", 4},
    {"CONFETTI", "confetti", "KILLS BURST INTO SPARKS", 4},
    {"FRACTAL", "fractal", "HITS SPLIT SHOTS IN TWO", 4},
    {"CAROUSEL", "carousel", "A BOMB EVERY BEAT", 4},
};

// GAME_DESIGN 6.5.
const FusionInfo kFusions[kFusionCount] = {
    {"HELIX ARRAY", "helix_array", "SHOTS BRAID A HELIX", kSplit, kWave},
    {"STANDING WAVE", "standing_wave", "LANCES STAY A BAR", kEcho, kAccent},
    {"TESLA BLOOM", "tesla_bloom", "RINGS ARC LIGHTNING", kPulseRing, kArc},
    {"SHARD HALO", "shard_halo", "ORBITALS FLING SHRAPNEL", kOrbitals, kShatter},
    {"DOWNBEAT CANNON", "downbeat_cannon", "A GIANT BEAM EACH BAR", kLancer, kOnBeatCrit},
    {"COLLAPSE FIELD", "collapse_field", "MINES PULL IN", kMinelayer, kGravityRounds},
    {"MURMURATION", "murmuration", "A HOMING FLOCK", kSwarm, kSeeker},
    {"QUARTET", "quartet", "THREE WINGMEN", kMirror, kRearGuard},
};

const char* const kNumberWords[] = {"ZERO", "ONE", "TWO",   "THREE", "FOUR",  "FIVE",
                                     "SIX",  "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN"};

// The Riffs' words by level (M5: chosen for Riffs 4-12, screens.md's for 1-3).
const char* const kRiffWords[kRiffCount][5] = {
    {"", "", "", "", ""},  // Orbitals: counted below
    {"NEW RIFF: BEAT RINGS", "LEVEL 2: SECOND RING", "LEVEL 3: WIDER RINGS", "LEVEL 4: HEAVIER RINGS", "LEVEL 5: TWICE AS OFTEN"},
    {"NEW RIFF: OFF-BEAT DRONE", "LEVEL 2: BURST OF 4", "LEVEL 3: SECOND DRONE", "LEVEL 4: BURST OF 5", "LEVEL 5: THIRD DRONE"},
    {"NEW RIFF: A BEAM EACH BAR", "LEVEL 2: WIDER BEAM", "LEVEL 3: TWO BEAMS A BAR", "LEVEL 4: WIDER STILL", "LEVEL 5: HEAVIER BEAM"},
    {"NEW RIFF: A MINE A BAR", "LEVEL 2: TWO MINES", "LEVEL 3: THREE MINES", "LEVEL 4: FOUR MINES", "LEVEL 5: FIVE MINES"},
    {"NEW RIFF: RAPID ZAPS", "LEVEL 2: TWO TARGETS", "LEVEL 3: THREE, WIDER", "LEVEL 4: FOUR TARGETS", "LEVEL 5: FIVE, WIDER"},
    {"NEW RIFF: HOMING MISSILES", "LEVEL 2: EIGHT MISSILES", "LEVEL 3: TEN MISSILES", "LEVEL 4: TWELVE MISSILES", "LEVEL 5: SIXTEEN MISSILES"},
    {"NEW RIFF: A GRAVITY WELL", "LEVEL 2: WIDER WELL", "LEVEL 3: HEAVIER BLAST", "LEVEL 4: WIDER STILL", "LEVEL 5: HEAVIER STILL"},
    {"NEW RIFF: A WINGMAN AT 50%", "LEVEL 2: WINGMAN 60%", "LEVEL 3: WINGMAN 70%", "LEVEL 4: WINGMAN 80%", "LEVEL 5: WINGMAN 100%"},
    {"NEW RIFF: A BOOMERANG DISC", "LEVEL 2: BIGGER DISC", "LEVEL 3: TWO DISCS", "LEVEL 4: BIGGER STILL", "LEVEL 5: THREE DISCS"},
    {"NEW RIFF: A LASER ARM", "LEVEL 2: LONGER ARM", "LEVEL 3: SECOND ARM", "LEVEL 4: LONGER STILL", "LEVEL 5: FASTER ARMS"},
    {"NEW RIFF: PELLET BURSTS", "LEVEL 2: MORE PELLETS", "LEVEL 3: MORE STILL", "LEVEL 4: TWICE AS MANY", "LEVEL 5: PELLETS EXPLODE"},
};
}  // namespace

const UpgradeInfo& upgradeInfo(u8 u) { return u < kUpgradeCount ? kInfo[u] : kNone; }
const BoostInfo& boostInfo(u8 b) { return kBoosts[b < kBoostCount ? b : 0]; }
const FusionInfo& fusionInfo(u8 f) { return kFusions[f < kFusionCount ? f : 0]; }

const char* upgradeWords(u8 u, u32 level) {
    // Effect words: screens.md's table (final, play test 1, 2026-11-12) for the first slice, word for word; M5's chosen
    // in the same style (docs/PROGRESS.md, M5 part 3).
    static char buf[48];
    const u32 lv = level < 1 ? 1u : level;
    switch (u) {
        case kSplit: return "+1 SHOT LANE";                                  // eb10554: a lane a rank
        case kDoubleTime: return lv <= 1 ? "FIRE RATE X1.5" : "FIRE RATE X2";  // eb10554: two ranks again
        case kPierce: return "SHOTS PIERCE +2";
        case kRicochet: return "SHOTS BOUNCE +2";
        case kFork: return "SHOTS FORK ON A HIT";  // play test 3: one rank (forking again is Fractal's)
        case kSeeker: return lv <= 1 ? "SHOTS HOME IN" : (lv == 2 ? "TIGHTER HOMING" : "SHOTS HUNT");
        case kHeavyStep: return lv <= 1 ? "EVERY 4TH SHOT EXPLODES" : (lv == 2 ? "BIGGER BLASTS" : "HEAVIER BLASTS");
        case kAccent: return lv <= 1 ? "A LANCE EACH BAR" : (lv == 2 ? "TWO LANCES A BAR" : "A LANCE EVERY BEAT");
        case kEcho: return lv <= 1 ? "SHOTS ECHO A BEAT LATER" : "TWO ECHOES";
        case kWave: return lv <= 1 ? "WAVY SHOTS HIT WIDER" : "WIDER WAVES";
        case kOvercharge: return "+50% SHOT DAMAGE";
        case kRearGuard: return lv <= 1 ? "ALSO FIRE BACKWARD" : "AND SIDEWAYS";
        case kArc:
            std::snprintf(buf, sizeof buf, "KILLS ARC TO %u MORE", lv);  // play test 3: 1, 2, 3
            return buf;
        case kBurn: return lv <= 1 ? "HITS IGNITE" : (lv == 2 ? "HOTTER FIRE" : "FIERCE FIRE");
        case kShatter: return lv <= 1 ? "KILLS THROW SHRAPNEL" : "MORE SHRAPNEL";
        case kGravityRounds: return lv <= 1 ? "HITS PULL ENEMIES IN" : "A STRONGER PULL";
        case kExtraCell: return "+1 CELL, ALL REFILLED";
        case kRegen:  // play test 2 (screens.md): CELL REGROWS: 16 BARS, then 8 BARS, 4 BARS
            std::snprintf(buf, sizeof buf, "CELL REGROWS: %u BARS", regenEveryBars(lv));
            return buf;
        case kAfterimage: return lv <= 1 ? "DASH LEAVES A DECOY" : "DECOY LASTS LONGER";
        case kReserve: return "+1 DASH CHARGE";
        case kShockstep: return lv <= 1 ? "DASH: A DAMAGING RING" : "A BIGGER RING";
        case kAegis: return lv <= 1 ? "A SHIELD BLOCKS BULLETS" : "A WIDER SHIELD";
        case kLastLight: return "WHEN HIT: A FREE NOVA";
        case kSlipstream: return "+15% SPEED";
        case kMagnet:
            std::snprintf(buf, sizeof buf, "SHARD PULL X%u", lv + 1u);
            return buf;
        case kProspector:
            std::snprintf(buf, sizeof buf, "+%u%% EXTRA SHARDS", 25u * lv);
            return buf;
        case kNovaStock: return "+1 NOVA";
        case kEncore: return "BOOSTS LAST +1 BAR";
        case kLuckyBreak: return "ONE MORE CHOICE";
        case kCollector: return "A BOOST PER 500 SHARDS";
        case kOnBeatCrit:
            std::snprintf(buf, sizeof buf, "BEAT SHOTS X%u", static_cast<u32>(onBeatCritMultiplier(lv)));
            return buf;
        case kSyncopation: return lv <= 1 ? "OFF-BEATS PIERCE, +50%" : "OFF-BEATS +100%";
        case kBarLine: return lv <= 1 ? "A LASER CROSS EACH BAR" : (lv == 2 ? "AND DIAGONALS" : "LONGER LASERS");
        case kCrescendo: return lv <= 1 ? "DAMAGE CLIMBS WHILE UNHIT" : "CLIMBS TWICE AS HIGH";
        case kKickPulse: return lv <= 1 ? "KICK DRUM SHOVES ENEMIES" : (lv == 2 ? "THE SHOVE HURTS" : "A WIDER SHOVE");
        case kAfterparty: return "LONGER, FASTER SURGE";
        case kOrbitals: {
            // screens.md (play test 3): the guard ring; levels 2 and 4 count its shards, 3 and 5 relight it faster.
            const u32 n = orbitalCount(lv);
            if (lv <= 1) return "NEW RIFF: A GUARD RING";
            if (lv == 3 || lv >= 5) std::snprintf(buf, sizeof buf, "LEVEL %u: RELIGHTS FASTER", lv);
            else std::snprintf(buf, sizeof buf, "LEVEL %u: %s SHARDS", lv, kNumberWords[n < 12 ? n : 11]);
            return buf;
        }
        default:
            if (isRiff(u)) return kRiffWords[riffIndex(u)][(lv <= 5 ? lv : 5) - 1];
            return "";
    }
}

bool Loadout::canTake(u8 u) const {
    if (u >= kUpgradeCount) return false;
    if (isRiff(u)) {
        const u8 lv = riffs[riffIndex(u)];
        if (lv == 0) return riffsOwned() < kRiffSlots;
        return lv < kRiffMaxLevel;
    }
    return mods[u] < kInfo[u].cap;
}

bool Loadout::fusionDue(u8 f) const {
    if (f >= kFusionCount || fused(f)) return false;
    const FusionInfo& fi = kFusions[f];
    return level(fi.a) >= kInfo[fi.a].cap && level(fi.b) >= kInfo[fi.b].cap;
}

bool firepower(u8 u) {
    // GAME_DESIGN 6.1: the weapon (6.2's 1-16) and rhythm (31-36) Mods.
    return u < kExtraCell || (u >= kOnBeatCrit && u <= kAfterparty);
}

float dealWeight(const Loadout& l, u8 u) {
    // GAME_DESIGN 6.1 (M5): a Mod not held 1, held and under its cap 3, a new Riff 2 (none once four are held), a level of
    // a held Riff 3; an ingredient of a Fusion whose other ingredient is held counts double. 0: it cannot be offered.
    if (!l.canTake(u)) return 0.0f;
    const u32 lv = l.level(u);
    float w = isRiff(u) ? (lv == 0 ? kDealRiffNew : kDealRiffLevel) : (lv == 0 ? kDealModNew : kDealModHeld);
    if (lv == 0 && (u == kExtraCell || u == kRegen)) w = kDealCellsNew;  // (measured, M5 part 3: tuning.h)
    for (u8 f = 0; f < kFusionCount; ++f) {
        const FusionInfo& fi = kFusions[f];
        if (l.fused(f)) continue;
        if ((u == fi.a && l.level(fi.b) > 0) || (u == fi.b && l.level(fi.a) > 0)) {
            w *= kDealFusionFactor;
            break;
        }
    }
    return w;
}

u32 drawTrio(const Loadout& l, u8& sinceRiff, Rng& rng, u8 out[kMaxOrbs], bool firstOfRun) {
    // GAME_DESIGN 6.1 (M5, the lead's ruling of 2026-11-13): the Trio is dealt. The first orb is firepower (the weapon and
    // rhythm Mods held, the Riffs held and the four staples, by the same weights, what is at its cap left out; none: as
    // the others); the other orbs are the whole pool by weight; no two alike; the Riff guarantee; Lucky Break's orbs as the
    // others; then Boosts where nothing can be offered, and a due Fusion's gold orb.
    float weight[kUpgradeCount];
    for (u8 u = 0; u < kUpgradeCount; ++u) weight[u] = dealWeight(l, u);
    bool used[kUpgradeCount] = {};
    const auto draw = [&](auto pred) -> i32 {
        float total = 0.0f;
        for (u8 u = 0; u < kUpgradeCount; ++u)
            if (!used[u] && weight[u] > 0.0f && pred(u)) total += weight[u];
        if (total <= 0.0f) return -1;
        float pick = rng.range(0.0f, total);
        i32 last = -1;
        for (u8 u = 0; u < kUpgradeCount; ++u) {
            if (used[u] || weight[u] <= 0.0f || !pred(u)) continue;
            last = u;
            if (pick < weight[u]) return u;
            pick -= weight[u];
        }
        return last;
    };
    const auto staple = [](u8 u) { return u == kSplit || u == kDoubleTime || u == kOvercharge || u == kOnBeatCrit; };
    const auto fire = [&](u8 u) { return isRiff(u) ? l.level(u) > 0 : (staple(u) || (firepower(u) && l.mods[u] > 0)); };
    const auto any = [](u8) { return true; };
    const auto newRiff = [&](u8 u) { return isRiff(u) && l.level(u) == 0; };
    const u32 want = 3u + static_cast<u32>(l.mods[kLuckyBreak]);
    u32 count = 0;
    if (firstOfRun) {
        // a667116 (GAME_DESIGN 6.1, the owner): the first Trio of a run offers Split and Double-Time (each where it can still be
        // taken); the other orbs are drawn as ever, never one of those two again.
        const u8 firstPair[2] = {kSplit, kDoubleTime};
        for (u8 u : firstPair)
            if (weight[u] > 0.0f && count < want) {
                used[u] = true;
                out[count++] = u;
            }
    }
    if (count == 0) {
        i32 first = draw(fire);
        if (first < 0) first = draw(any);
        if (first >= 0) {
            used[first] = true;
            out[count++] = static_cast<u8>(first);
        }
    }
    while (count < want) {
        const i32 u = draw(any);
        if (u < 0) break;
        used[u] = true;
        out[count++] = static_cast<u8>(u);
    }
    bool riffOffered = false;
    for (u32 k = 0; k < count; ++k) riffOffered = riffOffered || newRiff(out[k]);
    // A new Riff when fewer than four are owned and none was offered in the last two Trios: it takes the last of the other
    // orbs' places (the firepower orb stays).
    if (l.riffsOwned() < kRiffSlots && sinceRiff >= 2 && !riffOffered && count >= 2) {
        const i32 r = draw(newRiff);
        if (r >= 0) {
            used[out[count - 1]] = false;
            used[r] = true;
            out[count - 1] = static_cast<u8>(r);
            riffOffered = true;
        }
    }
    sinceRiff = riffOffered ? 0 : static_cast<u8>(sinceRiff < 250 ? sinceRiff + 1 : 250);
    // Play test 2 (GAME_DESIGN 6.1): the orbs no upgrade can fill offer Boosts, each a different one.
    u8 boosts[kBoostCount];
    u32 nb = 0;
    for (u8 b = 0; b < kBoostCount; ++b) boosts[nb++] = b;
    while (count < want && nb > 0) {
        const u32 k = rng.below(nb);
        out[count++] = static_cast<u8>(kBoostOrb + boosts[k]);
        boosts[k] = boosts[--nb];
    }
    // GAME_DESIGN 6.5: a Fusion that is due comes as a gold orb more (the first due, in the table's order).
    for (u8 f = 0; f < kFusionCount && count < kMaxOrbs; ++f)
        if (l.fusionDue(f)) {
            out[count++] = static_cast<u8>(kFusionOrb + f);
            break;
        }
    return count;
}

u32 levelShards(u32 n) {
    if (n <= 1) return kLevelShardsFirst;  // the owner's play of f0624e5: the first level sooner
    double v = static_cast<double>(kLevelShardsBase);
    for (u32 k = 1; k < n && k < 400; ++k) v *= static_cast<double>(kLevelShardsRatio);
    const double r = v + 0.5;
    return r < 1.0 ? 1u : (r > 4.0e9 ? 4000000000u : static_cast<u32>(r));
}

}  // namespace rf::sim
