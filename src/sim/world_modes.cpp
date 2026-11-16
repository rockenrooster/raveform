// M8 (docs/MODES_PLAN.md 2.3, 2.5; design/modes/README.md 5, design/modes/one_track.md): what the modes share in the world: the
// kit and its load-in, ONE TRACK's outro and the run's end. Nothing here runs in THE SET (World::tick calls updateModes only for
// another mode), so the set's state and hash are as they were.

#include "sim/bot.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

namespace {
// The loadout's part of taking an upgrade (World::applyUpgrade without the ship: its cells, Novas and drones come as it lands).
void takeInto(Loadout& l, u8 u) {
    if (isFusionOrb(u)) {
        l.fusions = static_cast<u8>(l.fusions | (1u << fusionOfOrb(u)));
        return;
    }
    if (isBoostOrb(u) || !l.canTake(u)) return;
    if (!l.order[u]) l.order[u] = static_cast<u8>(l.taken < 254 ? l.taken + 1 : 255);
    if (l.taken < 255) ++l.taken;
    if (isRiff(u)) ++l.riffs[riffIndex(u)];
    else ++l.mods[u];
}
}  // namespace

void World::dealKit(u32 p, u32 count, u32 act) {
    // README.md 5: dealt by the game's own rules (the run's first Trio offers Split and Double-Time, the first orb is firepower,
    // the weights, a Fusion when both its halves are at their caps) and picked by the careful bot's score, from a stream of the
    // run's seed, the seat and the act: a build the game could have dealt, none a trap, no two runs' the same. Nothing of it is
    // sent: every device draws every player's.
    if (p >= kMaxPlayers) return;
    KitState& k = kit_[p];
    k = KitState{};
    k.from = director_.startTick;
    if (!count) return;
    Rng rng(seed_ * 0x2545F4914F6CDD1Dull + 0x4B17ull * (p + 1u) + 0x9E3779B97F4A7C15ull * (act + 1u));
    Loadout l = players_[p].loadout;
    u8 sinceRiff = 0;
    const u32 want = count < kKitMax ? count : kKitMax;
    for (u32 j = 0; j < want * 2u && k.count < want; ++j) {
        u8 opts[kMaxOrbs];
        const u32 m = drawTrio(l, sinceRiff, rng, opts, k.count == 0);
        if (!m) break;
        const u8 u = opts[botPickOrb(l, opts, m)];
        if (isBoostOrb(u)) break;  // (everything at its cap: a Boost is no part of a kit)
        takeInto(l, u);
        k.orb[k.count++] = u;
    }
    // The run's own Trios go on from here (its first is no longer the Split and Double-Time one).
    trio_[p].opened = k.count;
    trio_[p].sinceRiff = sinceRiff;
}

double World::kitLandTick(u32 p, u32 i) const {
    // README.md 5: over the run's first two bars, one landing on each sixteenth after the first downbeat (more than fit: two to a
    // sixteenth); the closing chord and ring on bar 3's downbeat.
    const KitState& k = kit_[p < kMaxPlayers ? p : 0];
    const double sixteenth = kTicksPerBeat / 4.0;
    const u32 slots = (species_ ? species_->modes.loadIn.bars : 2u) * 16u - 1u;  // (31: the last sixteenth before bar 3)
    const u32 n = k.count ? k.count : 1u;
    const u32 slot = n <= slots ? i + 1u : 1u + (i * slots) / n;
    return k.from + sixteenth * static_cast<double>(slot);
}

void World::updateModes() {
    // The kit: each upgrade applied as it lands on its ship (the view's icon and sparks, the band's chirp: KitLand).
    for (u32 p = 0; p < playerCount_; ++p) {
        KitState& k = kit_[p];
        PlayerState& pl = players_[p];
        while (k.landed < k.count) {
            const double t = kitLandTick(p, k.landed);
            if (t > m1_) break;
            const u8 u = k.orb[k.landed];
            if (isFusionOrb(u)) pl.loadout.fusions = static_cast<u8>(pl.loadout.fusions | (1u << fusionOfOrb(u)));
            else applyUpgrade(p, u);
            Event ev;
            ev.type = EventType::KitLand;
            ev.a = static_cast<u8>(p);
            ev.b = u;
            ev.c = k.landed;
            ev.f = static_cast<float>(k.count);
            ev.x = pl.pos.x;
            ev.y = pl.pos.y;
            ev.tick = t;
            emit(ev);
            ++k.landed;
            if (k.landed == k.count) {
                // The run starts with every cell and three Novas plus the stock's (README.md 5).
                pl.cells = pl.cellCap;
                if (pl.novas < pl.novaCap) pl.novas = pl.novaCap;
            }
        }
    }
    // ONE TRACK's outro (one_track.md): every Shard on the field flies to its player, as under Magnet Storm.
    if (phrase_.outro)
        for (u32 i = 0; i < shards_.count; ++i) shards_.homing[i] = 1;
    // The run's end: the outro's last bar line.
    DirectorState& d = director_;
    if (d.outroBar != kNoBar && !d.runEnded && phrase_.ended) {
        d.runEnded = true;
        Event ev;
        ev.type = EventType::RunEnd;
        ev.a = 0;
        ev.tick = tickOfBar(d.outroBar + d.outroBars);
        emit(ev);
    }
}

}  // namespace rf::sim
