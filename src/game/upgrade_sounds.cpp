#include "game/upgrade_sounds.h"

#include <cstring>

#include "audio/band.h"
#include "sim/world.h"

namespace rf {

using audio::Cue;
using namespace sim;

static_assert(sim::kModCount == audio::kModCount && sim::kRiffCount == audio::kRiffCount && sim::kBoostCount == audio::kBoostCount &&
                  sim::kFusionCount == audio::kFusionCount,
              "the band numbers the upgrades as the simulation does (GAME_DESIGN 6.2 to 6.5)");

audio::PlayerLoadout bandLoadout(const PlayerState& pl) {
    audio::PlayerLoadout o;
    std::memcpy(o.mods, pl.loadout.mods, sizeof o.mods);
    // Overcharge's four stacks (2026-11-16) drive the lead as the old five did at the top (sim/upgrades.h overchargeLook).
    o.mods[kOvercharge] = static_cast<u8>(overchargeLook(pl.loadout.mods[kOvercharge]));
    std::memcpy(o.riffs, pl.loadout.riffs, sizeof o.riffs);
    for (const ActiveBoost& a : pl.boosts)
        if (a.type < kBoostCount) o.boosts = static_cast<u16>(o.boosts | (1u << a.type));
    o.fusions = pl.loadout.fusions;
    return o;
}

u32 loadoutSignature(const audio::PlayerLoadout& l) {
    u32 h = 2166136261u;
    const auto mix = [&](u8 b) { h = (h ^ b) * 16777619u; };
    for (u8 m : l.mods) mix(m);
    for (u8 r : l.riffs) mix(r);
    mix(static_cast<u8>(l.boosts));
    mix(static_cast<u8>(l.boosts >> 8));
    mix(l.fusions);
    return h | 1u;  // never 0 (0: not posted yet)
}

void UpgradeCues::reset() {
    for (u32 k = 0; k < audio::kCueCount; ++k) {
        count[k] = 0;
        value[k] = 0.0f;
        pan[k] = 0.0f;
        tick[k] = -1.0;
    }
    burning = 0xFFFFFFFFu;
    ringLit = 0;
    ringBroken = false;
}

void UpgradeCues::add(const Event& e, float p, double songOffset) {
    if (e.type == EventType::Hit) {
        const u32 k = static_cast<u32>(Cue::PlayerHit);
        ++count[k];
        // Play test 3 (GAME_DESIGN 6.3 row 1): the Orbitals' guard ring broke instead of a cell: the glass crack, low.
        if (e.c == 1) ++count[static_cast<u32>(Cue::OrbitalsBreak)];
        return;
    }
    if (e.type == EventType::ExtraLife) {
        // Play test 3 (GAME_DESIGN 5): picked up (b 0), or spent (b 1: the stab on the free Nova's blast, the event's tick).
        const u32 k = static_cast<u32>(e.b ? Cue::ExtraLifeUse : Cue::ExtraLifePickup);
        if (!count[k]) tick[k] = e.b && e.tick > 0.0 ? e.tick + songOffset : -1.0;
        ++count[k];
        return;
    }
    if (e.type != EventType::Upgrade) return;
    Cue c = Cue::Count;
    float v = 0.0f;
    double at = -1.0;
    switch (e.b) {
        case kFxFork: c = Cue::ForkSplit; break;
        case kFxBlast: c = e.c == 1 ? Cue::HeavyStepBurst : Cue::SupernovaImpact; break;  // Supernova, Carousel's bombs, Flak 5
        case kFxArc:
            // Arc and Tesla Bloom: a zap a chord tone higher each jump; Storm Coil its own; Chain Storm is its Boost's layer.
            if (e.f < 1.5f) {
                c = Cue::ArcChain;
                v = static_cast<float>(e.c);
            } else if (e.f > 2.5f) {
                c = Cue::StormCoilStrike;
            }
            break;
        case kFxShatter: c = Cue::ShatterBurst; break;
        case kFxGravity: c = Cue::GravityHit; break;
        case kFxDashRing:  // (Shockstep's sound, the Dash's own ring since 2026-11-15)
            c = Cue::ShockstepDash;
            v = static_cast<float>(e.c);
            break;
        case kFxAegis: c = Cue::AegisBlock; break;
        case kFxLastLight: c = Cue::LastLightNova; break;
        case kFxCollector: c = Cue::CollectorBoost; break;
        case kFxRam: c = Cue::ColossusRam; break;
        case kFxShatterFrozen: c = Cue::FreezeShatter; break;
        case kFxConfetti: c = Cue::ConfettiKill; break;
        case kFxBounce:
            c = Cue::RicochetBounce;
            v = static_cast<float>(e.c);
            break;
        case kFxHole:
            // Black Hole: the cue's tick is the inhale's start, three beats before the singularity blows.
            c = Cue::BlackHoleNova;
            at = e.tick + songOffset - 3.0 * kTicksPerBeat;
            break;
        default: break;  // the rest is patterned: the band plays it from the loadout on its own clock
    }
    if (c == Cue::Count) return;
    const u32 k = static_cast<u32>(c);
    if (count[k] == 0) {
        pan[k] = p;
        value[k] = v;
        tick[k] = at;
    } else if (v > value[k]) {
        value[k] = v;  // an Arc's longest jump, a Perfect Dash, the most bounces
    }
    ++count[k];
}

void UpgradeCues::flush(Band& band, const World& w, u32 seat) {
    for (u32 k = 0; k < audio::kCueCount; ++k) {
        if (!count[k]) continue;
        band.cue(static_cast<Cue>(k), count[k], value[k], pan[k], tick[k]);
        count[k] = 0;
        value[k] = 0.0f;
        tick[k] = -1.0;
    }
    // Burn: the crackle's density follows the bodies this player has burning (posted when it changes).
    if (w.player(seat).loadout.mods[kBurn]) {
        const Enemies& e = w.enemies();
        u32 n = 0;
        for (u32 i = 0; i < e.count; ++i)
            if (!(e.flags[i] & kFlagDead) && e.burnOwner[i] == seat && w.burning(i)) ++n;
        if (n > 30) n = 30;
        if (n != burning) {
            band.cue(Cue::BurnCount, 1, static_cast<float>(n));
            burning = n;
        }
    } else if (burning != 0xFFFFFFFFu && burning != 0) {
        band.cue(Cue::BurnCount, 1, 0.0f);
        burning = 0;
    }
    // Play test 3 (GAME_DESIGN 6.3 row 1): a broken ring's shards relight one by one; each is a shaker tick a scale step above
    // the last (value: the shard's place, 0 the first).
    const PlayerState& me = w.player(seat);
    if (me.loadout.riffs[riffIndex(kOrbitals)]) {
        const u32 lit = w.orbitalsLit(seat, w.music());
        if (ringBroken && lit > ringLit) band.cue(Cue::OrbitalsRelight, 1, static_cast<float>(lit - 1));
        ringLit = lit;
        ringBroken = me.ringBroke >= 0.0;
    } else {
        ringLit = 0;
        ringBroken = false;
    }
}

}  // namespace rf
