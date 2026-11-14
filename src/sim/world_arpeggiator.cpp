// M5 part 6: Arpeggiator, the boss of Track 8 (design/bosses/arpeggiator.md, .json). The bullets are the tune: four lanes run
// from a machine across the frame and the lead's broken chord comes down them as blocks (danger bars that cannot be shot; a
// Nova clears the lanes). It is open while it loads a bar: x 2 on the core for the One; the console sheds shots (a quarter,
// GAME_DESIGN 7.6). Eighth notes: eight blocks a bar, and every fourth bar it moves to the next side of the players. Two hands:
// a second machine at right angles plays the chord stab's rhythm; either core can be hurt, one health.
//
// Co-op: the blocks follow the song's clock; the machines' places follow the players' centre (the heartbeat's sync point is
// the first machine's), their sides change on bar lines by the phase, which follows the health on every device. A Nova clears
// the lanes on every device (every player's Nova). No host decision.

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;
using namespace prose;

namespace {
Vec2 sideDir(u8 side) {
    // Where a machine stands from the players' centre: above (y up is negative), right, below, left.
    switch (side & 3u) {
        case 0: return Vec2{0.0f, -1.0f};
        case 1: return Vec2{1.0f, 0.0f};
        case 2: return Vec2{0.0f, 1.0f};
        default: return Vec2{-1.0f, 0.0f};
    }
}
}  // namespace

Vec2 World::arpMachine(u32 m) const {
    // A machine's core: kApPlaceWu from the players' centre (its distance line, set as it takes its side), at its place along its
    // own axis.
    const ArpState& a = boss_.ap;
    const Vec2 out = sideDir(a.side[m]);
    const Vec2 axis{-out.y, out.x};
    return out * a.across[m] + axis * a.along[m];
}

bool World::arpLane(u32 m, u32 lane, Vec2& from, Vec2& dir) const {
    // Lane 1-4 of machine m: from its port on the console's lower edge, into the field (toward the players).
    const ArpState& a = boss_.ap;
    if (m > 1 || (m == 1 && !a.two)) return false;
    const ArpData& d = species_->arp;
    const Vec2 out = sideDir(a.side[m]);
    const Vec2 axis{-out.y, out.x};
    dir = out * -1.0f;
    from = arpMachine(m) + dir * (0.5f * kApConsoleD) + axis * ((static_cast<float>(lane) - 2.5f) * d.laneWidth);
    return true;
}

float World::arpScale(u8 part, u8 index, double t) const {
    // The core: x 2 on the first beat of every bar (the shutter open as it loads the chart), x 1 at other times; the console:
    // a quarter (it sheds shots).
    (void)index;
    const ArpData& d = species_->arp;
    if (part == 1) return d.dmgConsole;
    const bool one = t - std::floor(t / kTicksPerBar) * kTicksPerBar < kTicksPerBeat;
    return one ? kApOne : d.dmgLoading;
}

void World::enterArpeggiator(Vec2 centre) {
    BossState& s = boss_;
    const ArpData& d = species_->arp;
    s.fightTick = s.enterTick + kTicksPerBar;  // the console draws itself and loads its first chart over a bar
    s.phaseTick = s.fightTick;
    s.coreMax = d.health * s.scale;
    s.coreHp = s.coreMax;
    s.alive = 0;
    s.ap = ArpState{};
    s.ap.side[0] = 0;
    s.ap.side[1] = 1;
    s.ap.across[0] = s.ap.across[1] = kApPlaceWu;
    const Vec2 c = centre;
    s.ap.along[0] = c.x;   // (above: its axis is x)
    s.ap.along[1] = c.y;   // (right: its axis is y)
    s.ap.across[0] = -c.y + kApPlaceWu;
    s.ap.across[1] = c.x + kApPlaceWu;
    s.pos = s.prevPos = arpMachine(0);
    s.body[0] = s.bodyPrev[0] = s.pos;
}

void World::arpNovaClear() {
    // A Nova clears the lanes (every player's, on every device).
    for (ArpState::Block& b : boss_.ap.blocks) b.tick = -1.0;
}

void World::updateArpeggiator(float dt) {
    BossState& s = boss_;
    ArpState& a = s.ap;
    const ArpData& d = species_->arp;
    s.prevPos = s.pos;
    s.bodyPrev[0] = s.body[0];
    s.bodyPrev[1] = s.body[1];
    const double life = static_cast<double>(d.laneLength / d.blockSpeed) * (bpm_ * kTicksPerBeat / 60.0);
    if (!s.dying)  // (dying, they climb back up their lanes as Shards: the view's; they hurt nobody)
        for (ArpState::Block& b : a.blocks)
            if (b.tick >= 0.0 && m1_ - b.tick > life) b.tick = -1.0;
    if (s.dying) {
        // The run up the scale: a chirp each sixteenth over two beats (the first with the death).
        forCrossings(m0_, m1_, kTicksPerStep, [&](double t, i64) {
            const double k = std::floor((t - a.deathTick) / kTicksPerStep + 0.5);
            if (a.deathTick >= 0.0 && k >= 1.0 && k < 8.0) emitCue(BossCue::ApDeath, t, static_cast<u8>(k), 0.0f, s.pos);
        });
        return;
    }
    // The machines follow the players along their own axes at 120 wu/s; their distance line is the players' centre's.
    const Vec2 c = playersCentre();
    a.centre = c;
    for (u32 m = 0; m < 2; ++m) {
        const Vec2 out = sideDir(a.side[m]);
        const Vec2 axis{-out.y, out.x};
        const float want = dot(c, axis), now = a.along[m];
        const float step = clampf(want - now, -kApFollow * dt, kApFollow * dt);
        a.along[m] = now + step;
        a.across[m] = dot(c, out) + kApPlaceWu;
        const float lim = (out.x != 0.0f ? kArenaHalfH : kArenaHalfW) - 0.5f * kApConsoleW;
        a.along[m] = clampf(a.along[m], -lim, lim);
        const float limA = (out.x != 0.0f ? kArenaHalfW : kArenaHalfH) - 200.0f;
        a.across[m] = clampf(a.across[m], -limA, limA);
    }
    s.pos = arpMachine(0);
    s.body[0] = s.pos;
    s.body[1] = a.two ? arpMachine(1) : s.pos;
    // The phase by health.
    const float share = s.coreMax > 0.0f ? s.coreHp / s.coreMax : 0.0f;
    const u8 phase = share > kApEighthsAt ? 0 : (share > kApTwoHandsAt ? 1 : 2);
    if (phase != a.phase && m1_ >= s.fightTick) {
        a.phase = phase;
        s.phase = phase;
        s.phaseTick = m1_;
        if (phase == 2 && !a.two) {
            a.two = true;
            a.side[1] = static_cast<u8>((a.side[0] + 1u) & 3u);  // at right angles to the first
            const Vec2 out = sideDir(a.side[1]);
            a.along[1] = dot(c, Vec2{-out.y, out.x});
            emitCue(BossCue::ApSecond, m1_, 0, 0.0f, arpMachine(1));
        }
    }
    if (m1_ < s.fightTick) return;
    // Bar lines: the shutter opens on the One; in the Eighth notes, every fourth bar the machine moves to the next side (its new
    // lanes dashed the bar before).
    forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64 barIndex) {
        emitCue(BossCue::ApShutter, t, 0, 0.0f, s.pos);
        if (a.phase == 1 && (barIndex % static_cast<i64>(kApMoveEvery)) == static_cast<i64>(kApMoveEvery) - 1) {
            a.moveTick = t + kTicksPerBar;
            emitCue(BossCue::ApMove, t, 0, 0.0f, s.pos);
        }
        if (a.moveTick >= 0.0 && t >= a.moveTick - 1.0) {
            a.side[0] = static_cast<u8>((a.side[0] + 1u) & 3u);
            const Vec2 out = sideDir(a.side[0]);
            a.along[0] = dot(c, Vec2{-out.y, out.x});
            a.across[0] = dot(c, out) + kApPlaceWu;
            a.moveTick = -1.0;
        }
    });
    // Sixteenths: the blocks. Quarter notes: steps 0, 4, 8, 12; Eighth notes: every second step; Two hands: the first machine's
    // quarter notes and the second's stab (steps 0, 3, 6, 9, 12, 14, its lanes in turn).
    forCrossings(m0_, m1_, kTicksPerStep, [&](double t, i64 stepIndex) {
        const u32 step = static_cast<u32>(((stepIndex % 16) + 16) % 16);
        const auto block = [&](u8 machine, u8 lane) {
            ArpState::Block& b = a.blocks[a.nextBlock];
            a.nextBlock = (a.nextBlock + 1u) % ArpState::kBlocks;
            b.tick = t;
            b.lane = lane;
            b.machine = machine;
            Vec2 from, dir;
            if (arpLane(machine, lane, from, dir)) {
                b.from = from;
                b.dir = dir;
                emitCue(BossCue::ApBlock, t, static_cast<u8>(lane - 1u), 0.0f, from);  // b: the chord tone (lane 1 the root)
            } else {
                b.tick = -1.0;
            }
        };
        const bool first = a.phase == 1 ? (step % 2u) == 0 : (step % 4u) == 0;
        if (first && !(a.moveTick >= 0.0 && t >= a.moveTick - 1.0)) block(0, kApLanesG[step]);
        if (a.two)
            for (u32 j = 0; j < 6; ++j)
                if (kApStabSteps[j] == step) block(1, static_cast<u8>(1u + (j % 4u)));
    });
}

bool World::arpBlockHit(Vec2 at, Vec2& from) const {
    // A block: a bar blockWidth wide and 28 deep falling along its lane's middle at blockSpeed (arpeggiator.json: 72 wu at 340 wu/s
    // since 2fbb4b2).
    const BossState& s = boss_;
    if (s.kind != BossKind::Arpeggiator || !s.active || s.dying) return false;
    const ArpData& d = species_->arp;
    const float tps = bpm_ * static_cast<float>(kTicksPerBeat) / 60.0f;
    for (const ArpState::Block& b : s.ap.blocks) {
        if (b.tick < 0.0) continue;
        const Vec2 o = b.from, dir = b.dir;
        const float dist = static_cast<float>(m1_ - b.tick) / tps * d.blockSpeed;
        if (dist < 0.0f || dist > d.laneLength) continue;
        const Vec2 c = o + dir * dist;
        const Vec2 rel = at - c;
        const Vec2 axis{-dir.y, dir.x};
        if (std::fabs(dot(rel, axis)) <= 0.5f * d.blockWidth + kPlayerHitRadius && std::fabs(dot(rel, dir)) <= 14.0f + kPlayerHitRadius) {
            from = c;
            return true;
        }
    }
    return false;
}

}  // namespace rf::sim
