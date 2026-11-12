#include "sim/upgrades.h"

#include <cstdio>

#include "sim/tuning.h"

namespace rf::sim {

using namespace tuning;

namespace {
// GAME_DESIGN 6.2 / 6.3 (names, caps); icons from design/icons.json.
const UpgradeInfo kInfo[kUpgradeCount] = {
    {"SPLIT", "split", 4, true},
    {"DOUBLE-TIME", "double_time", 2, true},
    {"PIERCE", "pierce", 3, true},
    {"RICOCHET", "ricochet", 3, true},
    {"OVERCHARGE", "overcharge", 5, true},
    {"EXTRA CELL", "extra_cell", 3, false},
    {"MAGNET", "magnet", 4, false},
    {"ON-BEAT CRIT", "on_beat_crit", 3, true},
    {"ORBITALS", "orbitals", kRiffMaxLevel, true},
    {"PULSE RING", "pulse_ring", kRiffMaxLevel, true},
    {"SENTINEL", "sentinel", kRiffMaxLevel, true},
};
const UpgradeInfo kNone{"?", "", 0, false};

// GAME_DESIGN 6.4; durations in bars (Halo: until it has absorbed three hits, at most eight bars).
const BoostInfo kBoosts[kBoostCount] = {
    {"PRISM", "prism", "FIVE-WAY RAINBOW FAN", 4},
    {"HYPER", "hyper", "DOUBLE-SPEED GRID", 4},
    {"HALO", "halo", "BLOCKS THREE HITS", 8},
    {"MAGNET STORM", "magnet_storm", "EVERY SHARD COMES", 4},
};

const char* const kNumberWords[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN"};
}  // namespace

const UpgradeInfo& upgradeInfo(u8 u) { return u < kUpgradeCount ? kInfo[u] : kNone; }
const BoostInfo& boostInfo(u8 b) { return kBoosts[b < kBoostCount ? b : 0]; }

const char* upgradeWords(u8 u, u32 level) {
    // Effect words: two to four words, a number where there is one (screens.md "The Trio").
    static char buf[48];
    switch (u) {
        case kSplit: return "+1 SHOT LANE";
        case kDoubleTime: return level <= 1 ? "TRIPLET FIRE GRID" : "THIRTY-SECOND FIRE GRID";
        case kPierce: return "+1 ENEMY PIERCED";
        case kRicochet: return "+1 WALL BOUNCE";
        case kOvercharge: return "+25% SHOT DAMAGE";
        case kExtraCell: return "+1 SHIELD CELL";
        case kMagnet: return "WIDER SHARD PULL";
        case kOnBeatCrit:
            std::snprintf(buf, sizeof buf, "BEAT SHOTS X%u", level + 1);
            return buf;
        case kOrbitals:
            if (level <= 1) return "NEW RIFF: TWO ORBITERS";
            std::snprintf(buf, sizeof buf, "LEVEL %u: %s ORBITERS", level, kNumberWords[level + 1 < 8 ? level + 1 : 7]);
            return buf;
        case kPulseRing:
            switch (level) {
                case 0:
                case 1: return "NEW RIFF: BEAT RINGS";
                case 2: return "LEVEL 2: EVERY BEAT";
                case 3: return "LEVEL 3: SECOND RING";
                case 4: return "LEVEL 4: WIDER RINGS";
                default: return "LEVEL 5: HEAVIER RINGS";
            }
        case kSentinel:
            if (level <= 1) return "NEW RIFF: OFF-BEAT DRONE";
            std::snprintf(buf, sizeof buf, "LEVEL %u: BURST OF %u", level, level);
            return buf;
        default: return "";
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

u32 drawTrio(const Loadout& l, u8& sinceRiff, Rng& rng, u8 out[3]) {
    u8 cand[kUpgradeCount];
    float weight[kUpgradeCount];
    u32 n = 0;
    for (u8 u = 0; u < kUpgradeCount; ++u) {
        if (!l.canTake(u)) continue;
        cand[n] = u;
        weight[n] = !isRiff(u) ? kTrioWeightMod : (l.level(u) == 0 ? kTrioWeightNewRiff : kTrioWeightRiffLevel);
        ++n;
    }
    // Three without replacement, by weight.
    u32 count = 0;
    bool used[kUpgradeCount] = {};
    while (count < 3 && count < n) {
        float total = 0.0f;
        for (u32 k = 0; k < n; ++k)
            if (!used[k]) total += weight[k];
        float pick = rng.range(0.0f, total);
        u32 c = 0;
        for (; c < n; ++c) {
            if (used[c]) continue;
            if (pick < weight[c]) break;
            pick -= weight[c];
        }
        if (c >= n) {
            for (c = n; c-- > 0;)
                if (!used[c]) break;
        }
        used[c] = true;
        out[count++] = cand[c];
    }
    const auto offered = [&](auto pred) {
        for (u32 k = 0; k < count; ++k)
            if (pred(out[k])) return true;
        return false;
    };
    // Replace the last option that is not protected with a random candidate matching `pred`.
    const auto force = [&](auto pred, auto keep) {
        u32 m = 0;
        u8 pool[kUpgradeCount];
        for (u32 k = 0; k < n; ++k)
            if (!used[k] && pred(cand[k])) pool[m++] = static_cast<u8>(k);
        if (m == 0 || count == 0) return;
        const u32 k = pool[rng.below(m)];
        u32 slot = count - 1;
        for (u32 s = count; s-- > 0;) {
            if (!keep(out[s])) {
                slot = s;
                break;
            }
        }
        for (u32 j = 0; j < n; ++j)
            if (cand[j] == out[slot]) used[j] = false;
        used[k] = true;
        out[slot] = cand[k];
    };
    const auto newRiff = [&](u8 u) { return isRiff(u) && l.level(u) == 0; };
    const auto weapon = [&](u8 u) { return kInfo[u].weapon; };
    // A new Riff when fewer than four are owned and none was offered in the last two Trios.
    if (l.riffsOwned() < kRiffSlots && sinceRiff >= 2 && !offered(newRiff)) force(newRiff, [](u8) { return false; });
    // At least one option affects weapons (keep a new Riff placed above).
    if (!offered(weapon)) force(weapon, newRiff);
    sinceRiff = offered(newRiff) ? 0 : static_cast<u8>(sinceRiff < 250 ? sinceRiff + 1 : 250);
    return count;
}

}  // namespace rf::sim
