// M8 (docs/MODES_PLAN.md 2.3, 2.5; design/modes/README.md 5, design/modes/one_track.md): what the modes share in the world: the
// kit and its load-in, ONE TRACK's outro and the run's end. Nothing here runs in THE SET (World::tick calls updateModes only for
// another mode), so the set's state and hash are as they were.

#include <cmath>

#include "sim/bot.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

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
    trio_[p].opened += k.count;
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
    // Versus (README.md 7): a knocked out ship comes back on its bar line where it was, with two cells, a panic pulse, one bar in
    // which it cannot be hurt and half its Chain.
    if (setup_.versus)
        for (u32 p = 0; p < playerCount_; ++p) {
            PlayerState& pl = players_[p];
            if (!pl.knocked || m1_ < pl.backTick) continue;
            pl.knocked = false;
            pl.alive = true;
            pl.cells = pl.cellCap < 2 ? pl.cellCap : static_cast<u8>(2);
            pl.chain = pl.chain / 2 > 1 ? pl.chain / 2 : 1u;
            pl.invulS = static_cast<float>(kTicksPerBar) / ticksPerSecond();
            pl.vel = Vec2{};
            panicPulse(p);
            Event ev;
            ev.type = EventType::KnockBack;
            ev.a = static_cast<u8>(p);
            ev.x = pl.pos.x;
            ev.y = pl.pos.y;
            emit(ev);
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

void World::knockOut(u32 p, double backTick) {
    // README.md 7: the ship shatters (the view's large kill in its colour) and is out until that bar line; nothing hits it, it
    // fires nothing, its Trio closes. Its own device decides and tells the others (kPeKnock).
    PlayerState& pl = players_[p];
    pl.alive = false;
    pl.knocked = true;
    pl.backTick = backTick;
    pl.fire = 0;
    pl.dashing = false;
    pl.novaPending = false;
    pl.novaInvulS = 0.0f;
    pl.invulS = 0.0f;
    trio_[p].open = false;
    Event ev;
    ev.type = EventType::Knock;
    ev.a = static_cast<u8>(p);
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    ev.tick = backTick;
    emit(ev);
}

void World::versusBar(u32 bar) {
    // README.md 8, V5 and V7, on every bar line: the leader wears the crown (none while the top is level); the player in last
    // place, holding under modes.json's share of the leader's count, is the underdog until the next line (the host's switch).
    (void)bar;
    DirectorState& d = director_;
    u64 best = 0, worst = ~0ull;
    u32 atBest = 0, atWorst = 0;
    u8 leader = 0xFF, last = 0xFF;
    for (u32 p = 0; p < playerCount_; ++p) {
        const PlayerState& pl = players_[p];
        if (!pl.present) continue;
        const u64 c = pl.score;  // (THE SET, ONE TRACK: the score)
        if (leader == 0xFF || c > best) {
            best = c;
            leader = static_cast<u8>(p);
            atBest = 1;
        } else if (c == best) {
            ++atBest;
        }
        if (last == 0xFF || c < worst) {
            worst = c;
            last = static_cast<u8>(p);
            atWorst = 1;
        } else if (c == worst) {
            ++atWorst;
        }
    }
    const u8 crown = atBest == 1 && best > 0 ? leader : static_cast<u8>(0xFF);
    if (crown != d.crown) {
        Event ev;
        ev.type = EventType::Crown;
        ev.a = crown;
        ev.b = d.crown;
        emit(ev);
        d.crown = crown;
    }
    const float share = species_ ? species_->modes.versus.underdogShare : 0.7f;
    d.underdog = setup_.help && atWorst == 1 && last != leader && static_cast<double>(worst) < static_cast<double>(share) * static_cast<double>(best)
                     ? last
                     : static_cast<u8>(0xFF);
}

void World::rivalShove(u32 q, Vec2 dashEnd) {
    // V3: a Dash's shock ring (140 wu) pushes each rival ship in it 200 wu away from where the Dash ended over a quarter of a second,
    // a push it rides (PlayerState::shoves); the rival's own device applies it; a ship that cannot be hurt cannot be shoved.
    const float reach = kDashRingWu;
    const float by = species_ ? species_->modes.versus.shoveWu : 200.0f;
    const double ticks = static_cast<double>(species_ ? species_->modes.versus.shoveSeconds : 0.25f) * ticksPerSecond();
    for (u32 p = 0; p < playerCount_; ++p) {
        if (p == q || !isLocal(p)) continue;
        const PlayerState& pl = players_[p];
        if (!pl.present || !pl.alive || pl.invulS > 0.0f || pl.novaInvulS > 0.0f || sheltered(p)) continue;
        const Vec2 d = pl.pos - dashEnd;
        if (lengthSq(d) > reach * reach) continue;
        shovePlayer(p, PlayerState::kShoveRival, normalizeOr(d, Vec2{1.0f, 0.0f}) * by, m1_, ticks);
    }
}

void World::panicPulse(u32 p) {
    // GAME_DESIGN 5: clears the bullets within 220 wu and shoves the enemies there (as a hit's).
    const Vec2 c = players_[p].pos;
    const float r2 = kPanicRadiusWu * kPanicRadiusWu;
    for (u32 b = 0; b < bullets_.count; ++b) {
        const float dx = bullets_.x[b] - c.x, dy = bullets_.y[b] - c.y;
        if (dx * dx + dy * dy <= r2) bullets_.dead[b] = 1;
    }
    Enemies& e = enemies_;
    for (u32 i = 0; i < e.count; ++i) {
        const float dx = e.x[i] - c.x, dy = e.y[i] - c.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 > r2 || d2 < 1e-6f) continue;
        const float dd = std::sqrt(d2);
        e.kx[i] += dx / dd * kPanicShoveWu;
        e.ky[i] += dy / dd * kPanicShoveWu;
    }
}

}  // namespace rf::sim
