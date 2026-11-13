// The mini-bosses, batch 1 (design/bosses/minibosses.md; common numbers from minibosses.json, the rest quoted in
// sim/boss_prose.h), and the Pen. A mini-boss arrives on the middle cycle's Drop from Track 2 on, holds the Drop while it
// lives (at most 8 bars past its 16), and is its family grown large: Alpha Hound sends packs, The Hive is a cloud of darts
// round a Queen, Battering Ram charges into the Pen's walls, Carom King bounces round the Pen on the beat. What it calls
// are ordinary enemies (kFlagCalled), so the network's spawns, verdicts and corrections carry them like any.
//
// MiniState.state: Alpha 0 circling, 1 locked (its lunge's line), 2 lunging, 3 winded. Ram 0 turning, 1 locked,
// 2 charging, 3 skidding / resting, 4 stunned.

#include <cmath>
#include <cstring>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;
using namespace prose;

namespace {
constexpr float kDeg = kTau / 360.0f;
i64 imod(i64 a, i64 m) { return ((a % m) + m) % m; }
double beatLine(double t) { return std::floor(t / kTicksPerBeat) * kTicksPerBeat; }
}  // namespace

// ---------------------------------------------------------------------------------------------- the Pen

void World::penRise(Vec2 centre, double tick) {
    const MiniData& md = species_->minis;
    pen_.up = true;
    pen_.half = 0.5f * md.penSide;
    const float m = pen_.half + 60.0f;
    pen_.centre = Vec2{clampf(centre.x, -kArenaHalfW + m, kArenaHalfW - m), clampf(centre.y, -kArenaHalfH + m, kArenaHalfH - m)};
    pen_.riseTick = tick;
    pen_.fallTick = -1.0;
    // Fodder caught outside dissolves (the host's verdict; a client hears of each).
    if (role_ == NetRole::Client) return;
    Enemies& e = enemies_;
    // And never more than penMax bodies of fodder inside (minibosses.json common.crowd.penMax, the lead 2026-11-13).
    u32 inside = 0;
    const u32 maxInside = static_cast<u32>(md.penCrowdMax);
    for (u32 i = 0; i < e.count; ++i) {
        if (e.flags[i] & kFlagDead) continue;
        const Species& s = species_->get(e.species[i]);
        if (s.hue != 0) continue;
        if (penInside(Vec2{e.x[i], e.y[i]}, 0.0f) && ++inside <= maxInside) continue;
        e.flags[i] |= kFlagDead;
        Event ev;
        ev.type = EventType::Kill;
        ev.species = e.species[i];
        ev.a = s.killSize();
        ev.b = 0xFF;
        ev.c = 1;  // dissolved, no Shards
        ev.x = e.x[i];
        ev.y = e.y[i];
        emit(ev);
        if (role_ == NetRole::Host) logDeath(i, 0xFF, kDeathDissolve);
    }
}

void World::penFall(double tick) {
    if (!pen_.up) return;
    pen_.up = false;
    pen_.fallTick = tick;
}

bool World::penInside(Vec2 p, float r) const {
    return std::fabs(p.x - pen_.centre.x) < pen_.half - r && std::fabs(p.y - pen_.centre.y) < pen_.half - r;
}

u8 World::penKeep(Vec2 from, Vec2& to, float r) const {
    // Walls stop ships and bodies without hurting: what was inside stays inside, what was outside stays outside.
    if (!pen_.up) return 0;
    const Vec2 c = pen_.centre;
    const float h = pen_.half;
    u8 hit = 0;
    if (std::fabs(from.x - c.x) < h && std::fabs(from.y - c.y) < h) {
        const float l = h - r;
        if (to.x < c.x - l || to.x > c.x + l) {
            to.x = clampf(to.x, c.x - l, c.x + l);
            hit |= 1;
        }
        if (to.y < c.y - l || to.y > c.y + l) {
            to.y = clampf(to.y, c.y - l, c.y + l);
            hit |= 2;
        }
        return hit;
    }
    const float l = h + r;
    if (std::fabs(to.x - c.x) >= l || std::fabs(to.y - c.y) >= l) return 0;
    // Outside, pushed back across the side it came from.
    const bool sideX = std::fabs(from.x - c.x) >= h;
    if (sideX) {
        to.x = c.x + (from.x > c.x ? l : -l);
        hit |= 1;
    } else {
        to.y = c.y + (from.y > c.y ? l : -l);
        hit |= 2;
    }
    return hit;
}

Vec2 World::penPlace(Vec2 p, float r) {
    // A spawn while the Pen stands lands inside it (the crowd warps in inside), away from every ship.
    if (!pen_.up || penInside(p, r)) return p;
    const float l = pen_.half - r - 30.0f;
    Vec2 best = pen_.centre;
    for (u32 tries = 0; tries < 8; ++tries) {
        const Vec2 q = pen_.centre + Vec2{rng_.range(-l, l), rng_.range(-l, l)};
        best = q;
        bool clear = true;
        for (u32 k = 0; k < playerCount_; ++k)
            if (players_[k].alive && lengthSq(players_[k].pos - q) < kSpawnMinPlayerWu * kSpawnMinPlayerWu) clear = false;
        if (clear) break;
    }
    return best;
}

void World::updatePen() {
    // The Pen falls when its fight ends (the fight's end calls penFall); nothing else moves it.
    if (pen_.up && (!boss_.active || !isMiniBoss(boss_.kind))) penFall(m1_);
}

// ---------------------------------------------------------------------------------------------- common

float World::miniScore() const {
    // minibosses.json common.reward: 25 times the score of a tier-5 body of its family.
    const MiniData::Entry* e = species_->minis.find(boss_.kind);
    if (!e) return 0.0f;
    for (u32 f = 0; f < kFamilyCount; ++f)
        if (std::strcmp(familyName(static_cast<Family>(f)), e->family) == 0)
            return kMiniScoreTier5 * species_->get(SpeciesTable::index(static_cast<Family>(f), 5, kVariantA)).score;
    return 0.0f;
}

u32 World::miniNaturalEnd() const {
    // The run bar on which the middle cycle's Drop would end without its mini-boss.
    const DirectorState& d = director_;
    const u32* len = kSectionBars[formOfTrack(d.track, d.songs)];
    const u32 cycleBars = len[0] + len[1] + len[2] + len[3];
    return d.trackBar + cycleBars * (cyclesOfTrack(d.track) / 2u) + len[0] + len[1] + len[2];
}

u16 World::miniTierSpecies(Family f) const {
    // "Hounds of the Track's tier": the highest tier of the family the run's roster has brought in by now (variant A).
    u32 tier = 1;
    for (u32 k = 0; k < species_->rosterCount; ++k) {
        const SpeciesTable::RosterEntry& r = species_->roster[k];
        const Species& s = species_->get(r.species);
        if (s.family != f) continue;
        if (r.track < phrase_.track || (r.track == phrase_.track && r.cycle <= phrase_.cycle) || inLoop(phrase_.track, director_.songs))
            if (s.tier > tier) tier = s.tier;
    }
    return SpeciesTable::index(f, tier, kVariantA);
}

BossKind World::pickMini(u32 track) const {
    // minibosses.json common.pick: the run's seed draws one whose family the run has met, never the one met in the
    // Track before. Batch 1: Track 2 from Alpha Hound and The Hive, Track 3 from Battering Ram and Carom King; the loop's
    // Tracks draw from their song's pair (First Light's from all four).
    if (forcedMini_ != 0xFF) return static_cast<BossKind>(forcedMini_);
    BossKind cand[4];
    u32 n = 0;
    switch (songOfTrack(track, director_.songs)) {
        case 1: cand[n++] = BossKind::AlphaHound; cand[n++] = BossKind::Hive; break;
        case 2: cand[n++] = BossKind::BatteringRam; cand[n++] = BossKind::CaromKing; break;
        default:
            cand[n++] = BossKind::AlphaHound;
            cand[n++] = BossKind::Hive;
            cand[n++] = BossKind::BatteringRam;
            cand[n++] = BossKind::CaromKing;
            break;
    }
    BossKind keep[4];
    u32 m = 0;
    for (u32 k = 0; k < n; ++k)
        if (static_cast<u8>(cand[k]) != director_.lastMini) keep[m++] = cand[k];
    if (m == 0) return cand[0];
    const u32 h = hash32(hash32(static_cast<u32>(seed_), static_cast<u32>(seed_ >> 32) ^ 0x51EDu), 0x313B055u + track * 0x9E37u);
    return keep[(h >> 7) % m];
}

void World::enterMini(Vec2 pos, Vec2 penAt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    m = MiniState{};
    const MiniData::Entry* e = species_->minis.find(s.kind);
    const float r = e ? e->radius : 64.0f;
    s.coreMax = s.coreHp = s.scale;  // bossHealthScale gave the whole health: by Track, its factor, the players
    s.partMax = 0.0f;
    s.alive = 0;
    s.fightTick = s.enterTick + kMiniCardBars * kTicksPerBar;
    s.phaseTick = s.fightTick;
    if (e && e->pen) penRise(penAt, s.enterTick);
    if (pen_.up) {
        const float l = pen_.half - r - 80.0f;
        pos = Vec2{clampf(pos.x, pen_.centre.x - l, pen_.centre.x + l), clampf(pos.y, pen_.centre.y - l, pen_.centre.y + l)};
    } else {
        pos = Vec2{clampf(pos.x, -kArenaHalfW + r + 60.0f, kArenaHalfW - r - 60.0f), clampf(pos.y, -kArenaHalfH + r + 60.0f, kArenaHalfH - r - 60.0f)};
    }
    s.pos = s.prevPos = pos;
    s.body[0] = s.bodyPrev[0] = pos;
    const i32 near = nearestPlayer(pos.x, pos.y);
    m.face = normalizeOr((near >= 0 ? players_[near].pos : Vec2{}) - pos, Vec2{1.0f, 0.0f});
    s.angle = s.prevAngle = s.bodyAng[0] = s.bodyPrevAng[0] = angleOf(m.face);
    director_.midHolding = true;
    switch (s.kind) {
        case BossKind::Hive: {
            // 120 darts and the Queen; the Queen hatches more on every bar line.
            m.hives = 1;
            m.shape[0] = 0;
            m.shapeTick = s.fightTick;
            hiveShapeStart(s.fightTick);
            if (role_ != NetRole::Client) hiveHatch(kHiveDarts, s.enterTick + kWarpBeats * kTicksPerBeat);
            break;
        }
        case BossKind::CaromKing: {
            m.legEnd[0] = -1.0;  // its first leg starts on the fight's downbeat
            break;
        }
        default: break;
    }
    emitCue(BossCue::MiniEnter, s.enterTick, static_cast<u8>(s.kind), 0.0f, pos);
}

void World::miniHealthSteps() {
    // The host's verdicts on health: half health; the Carom King's splits at 75%, 50%, 25%.
    const BossState& s = boss_;
    if (s.coreMax <= 0.0f) return;
    const float f = s.coreHp / s.coreMax;
    if (s.kind == BossKind::CaromKing) {
        static const float kAt[3] = {0.75f, 0.5f, 0.25f};
        for (u8 k = s.mini.splits; k < 3; ++k)
            if (f <= kAt[k]) bossStep(static_cast<u8>(1 + k), 0xFF);
        return;
    }
    if (!s.mini.half && f <= 0.5f) bossStep(0, 0xFF);
}

void World::miniStep(u8 step) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    if (step == 0) {
        if (m.half) return;
        m.half = true;
        if (s.kind == BossKind::Hive) {
            // Two hives of 60 taking different shapes; one carries the Queen, the other a white decoy.
            m.hives = 2;
            m.shape[1] = static_cast<u8>((m.shape[0] + 1u) % 3u);
            hiveShapeStart(m.shapeTick);
            hiveReslot();
        }
        return;
    }
    if (s.kind != BossKind::CaromKing) return;
    // A Prince: the King sheds its outer square (radius 40, 12% of the King's full health, 1.3 times its speed).
    const u8 k = static_cast<u8>(step - 1);
    if (k >= 3 || k < m.splits) return;
    while (m.splits <= k) {
        const u8 p = m.splits++;
        s.alive |= 1u << p;
        s.partMax = s.coreMax * kPrinceHealth;
        s.partHp[p] = s.partMax;
        s.body[1 + p] = s.bodyPrev[1 + p] = s.body[0];
        // It leaves square to the King's course, alternately left and right.
        const Vec2 kd = normalizeOr(m.legTo[0] - m.legFrom[0], m.face);
        const Vec2 d = (p & 1u) ? perpOf(kd) * -1.0f : perpOf(kd);
        m.speedFactor[1 + p] = m.speedFactor[0] * kPrinceSpeed;
        caromLeg(1 + p, s.body[0], d, m1_, true);
        m.speedFactor[0] *= kKingSplitSpeed;
    }
}

void World::miniPartDied(u8 index, u8 owner, Event& ev) {
    // A Prince: it breaks into two ordinary Caroms (the host spawns them; R4: they warp in).
    BossState& s = boss_;
    const Vec2 at = s.body[1 + index];
    ev.x = at.x;
    ev.y = at.y;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present || !isLocal(p)) continue;
        for (u32 k = 0; k < kPrinceShards; ++k)
            spawnShard(at, dirOf(rng_.range(0.0f, kTau)) * rng_.range(kShardBurstLo, kShardBurstHi), static_cast<u8>(p));
    }
    (void)owner;
    if (role_ == NetRole::Client || s.kind != BossKind::CaromKing) return;
    const u16 sp = miniTierSpecies(Family::Carom);
    const Vec2 kd = normalizeOr(s.mini.legTo[1 + index] - s.mini.legFrom[1 + index], Vec2{1.0f, 0.0f});
    for (u32 j = 0; j < kPrinceCaroms; ++j) {
        const Vec2 off = perpOf(kd) * (j ? -30.0f : 30.0f);
        spawnEnemy(sp, at + off, angleOf(kd) + (j ? -0.6f : 0.6f), beatLine(m1_) + kWarpBeats * kTicksPerBeat);
    }
}

void World::miniDied(u8 owner, u32 endBar) {
    // The kill: its reward, what it called, the Pen, and the Drop's end (the first bar line at least half a bar away).
    BossState& s = boss_;
    const MiniData& md = species_->minis;
    s.deathBars = 1.0;
    const Vec2 at = s.body[0];
    // A Boost orb and 60 Shards for each player (each device its own players'; the orbs are instanced).
    if (role_ != NetRole::Client)
        for (u32 k = 0; k < md.rewardOrbs; ++k) dropBoost(at + dirOf(kTau * static_cast<float>(k) / static_cast<float>(md.rewardOrbs)) * 40.0f,
                                                             static_cast<u8>(upgradeRng_.below(kBoostCount)));
    const bool walls = pen_.up;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present || !isLocal(p)) continue;
        for (u32 k = 0; k < md.rewardShards; ++k) {
            Vec2 from = at;
            if (walls) {
                // The Pen's walls fall outward as Shards.
                const float u = rng_.range(-1.0f, 1.0f);
                const u32 side = k % 4u;
                const Vec2 c = pen_.centre;
                const float h = pen_.half;
                from = side == 0 ? c + Vec2{-h, u * h} : side == 1 ? c + Vec2{h, u * h} : side == 2 ? c + Vec2{u * h, -h} : c + Vec2{u * h, h};
            }
            spawnShard(from, dirOf(rng_.range(0.0f, kTau)) * rng_.range(kShardBurstLo, kShardBurstHi * 1.5f), static_cast<u8>(p));
        }
    }
    miniRelease(true);
    penFall(m1_);
    DirectorState& d = director_;
    const u32 natural = miniNaturalEnd();
    const u32 hold = endBar > natural ? endBar - natural : 0u;
    d.midHold = static_cast<u8>(hold < static_cast<u32>(kMiniHoldBars) ? hold : static_cast<u32>(kMiniHoldBars));
    d.midHolding = false;
    d.lastMini = static_cast<u8>(s.kind);
    emitCue(BossCue::MiniDeath, m1_, static_cast<u8>(s.kind), 0.0f, at);
    (void)owner;
}

void World::miniRelease(bool killed) {
    // What it called: the Alpha's pack scatters (double Shards for two bars), every dart of the Hive falls as Shards (or,
    // warped out, dissolves), the Carom King's Princes lose their speed and drift as ordinary Caroms.
    BossState& s = boss_;
    Enemies& e = enemies_;
    if (s.kind == BossKind::AlphaHound) {
        for (u32 i = 0; i < e.count; ++i) {
            if ((e.flags[i] & kFlagDead) || !(e.flags[i] & kFlagCalled) || e.state[i] == kWarping) continue;
            e.state[i] = kFlee;
            e.until[i] = m1_ + kAlphaScatterBars * kTicksPerBar;
        }
    } else if (s.kind == BossKind::Hive) {
        for (u32 i = 0; i < e.count; ++i) {
            if ((e.flags[i] & kFlagDead) || !(e.flags[i] & kFlagCalled)) continue;
            if (role_ == NetRole::Client) continue;  // the host's verdicts follow
            if (killed && e.state[i] != kWarping) {
                killEnemy(i, 0xFF);
            } else {
                e.flags[i] |= kFlagDead;
                Event ev;
                ev.type = EventType::Kill;
                ev.species = e.species[i];
                ev.b = 0xFF;
                ev.c = 1;
                ev.x = e.x[i];
                ev.y = e.y[i];
                emit(ev);
                if (role_ == NetRole::Host) logDeath(i, 0xFF, kDeathDissolve);
            }
        }
    } else if (s.kind == BossKind::CaromKing && killed && role_ != NetRole::Client) {
        const u16 sp = miniTierSpecies(Family::Carom);
        for (u32 k = 0; k < 3; ++k) {
            if (!(s.alive & (1u << k))) continue;
            const i32 i = spawnEnemy(sp, s.body[1 + k], s.bodyAng[1 + k], m1_);
            if (i >= 0) e.a[i] = kPrinceDrift;
        }
    }
    s.alive = 0;
}

void World::miniGone() {
    // The Drop's hold ran out: it warps out and leaves nothing (minibosses.json common.drop).
    BossState& s = boss_;
    DirectorState& d = director_;
    miniRelease(false);
    penFall(m1_);
    emitCue(BossCue::MiniGone, m1_, static_cast<u8>(s.kind), 0.0f, s.body[0]);
    Event ev;
    ev.type = EventType::BossGone;
    ev.a = static_cast<u8>(s.kind);
    ev.x = s.body[0].x;
    ev.y = s.body[0].y;
    emit(ev);
    s.active = false;
    s.dying = false;
    d.midHold = static_cast<u8>(kMiniHoldBars);
    d.midHolding = false;
    d.lastMini = static_cast<u8>(s.kind);
}

u32 World::miniParts(BossPart* out, u32 cap) const {
    const BossState& s = boss_;
    const MiniData::Entry* e = species_->minis.find(s.kind);
    const float r = e ? e->radius : 64.0f;
    u32 n = 0;
    switch (s.kind) {
        case BossKind::Hive:
            if (n < cap) out[n++] = BossPart{s.body[0], r, 3, 0};
            if (s.mini.hives > 1 && n < cap) out[n++] = BossPart{s.body[1], r, 1, 0};  // the decoy: light that stops a shot
            break;
        case BossKind::CaromKing:
            if (n < cap) out[n++] = BossPart{s.body[0], kKingRadii[s.mini.splits < 4 ? s.mini.splits : 3], 3, 0};
            for (u32 k = 0; k < 3; ++k)
                if ((s.alive & (1u << k)) && n < cap) out[n++] = BossPart{s.body[1 + k], kPrinceRadius, 2, static_cast<u8>(k)};
            break;
        default:
            if (n < cap) out[n++] = BossPart{s.body[0], r, 3, 0};
            break;
    }
    return n;
}

float World::miniScale(u8 part, u8 /*index*/, double t, Vec2 dir) const {
    // Every one has a moment when it takes x 2 (minibosses.md).
    const BossState& s = boss_;
    const MiniState& m = s.mini;
    if (part == 1) return 0.0f;
    switch (s.kind) {
        case BossKind::AlphaHound: return t < m.winded ? 2.0f : 1.0f;
        case BossKind::Hive: return t >= m.cueTick && t < m.cueTick + kTicksPerBeat ? 2.0f : 1.0f;  // the tell's beat
        case BossKind::BatteringRam: {
            if (m.state == 4) return 2.0f;  // stunned: x 2 from every side
            // Its front 120 degrees sheds every shot (the shot travels along dir; it meets the front when it comes at the horns).
            const float c = std::cos(0.5f * kRamFrontDeg * kDeg);
            return dot(dir * -1.0f, m.face) > c ? 0.0f : 1.0f;
        }
        case BossKind::CaromKing: return part == 3 && t < m.hot[0] ? 2.0f : 1.0f;
        default: return 1.0f;
    }
}

// ---------------------------------------------------------------------------------------------- the fights

void World::updateMini(float dt) {
    BossState& s = boss_;
    DirectorState& d = director_;
    // The Drop's hold ran out: it warps out (every device at the same bar line).
    if (d.midHolding && barOf(m1_ + 0.5) >= miniNaturalEnd() + static_cast<u32>(kMiniHoldBars)) {
        miniGone();
        return;
    }
    switch (s.kind) {
        case BossKind::AlphaHound: updateAlpha(dt); break;
        case BossKind::Hive: updateHive(dt); break;
        case BossKind::BatteringRam: updateRam(dt); break;
        case BossKind::CaromKing: updateCarom(dt); break;
        default: break;
    }
    s.pos = s.body[0];
    s.angle = s.bodyAng[0];
}

// ---- Alpha Hound: it circles and sends the pack.

void World::alphaHowl(double t) {
    // Beat 1: six Hounds of the Track's tier warp in as a chevron 500 wu from the player on the Alpha's side, with a danger
    // line from the chevron's tip through the player's position; they land and charge on beat 3. From half health two
    // packs of five, from opposite sides.
    BossState& s = boss_;
    const i32 near = nearestPlayer(s.body[0].x, s.body[0].y);
    if (near < 0) return;
    const Vec2 P = players_[near].pos;
    const Vec2 u = normalizeOr(s.body[0] - P, Vec2{1.0f, 0.0f});
    const u16 sp = miniTierSpecies(Family::Hound);
    u32 alive = 0;
    for (u32 i = 0; i < enemies_.count; ++i)
        if (!(enemies_.flags[i] & kFlagDead) && (enemies_.flags[i] & kFlagCalled)) ++alive;
    const u32 packs = s.mini.half ? 2u : 1u, size = s.mini.half ? kAlphaHalfPack : kAlphaPack;
    static const float kChevron[6][2] = {{0, 0}, {-45, 40}, {-45, -40}, {-90, 80}, {-90, -80}, {-120, 0}};
    for (u32 k = 0; k < packs; ++k) {
        const Vec2 side = k == 0 ? u : u * -1.0f;
        const Vec2 tip = P + side * kAlphaChevronWu;
        const Vec2 dir = side * -1.0f;  // from the tip through the player
        for (u32 j = 0; j < size; ++j) {
            if (alive >= kAlphaCap) break;  // a howl that would pass the cap calls fewer
            const Vec2 at = tip - dir * kChevron[j][0] + perpOf(dir) * kChevron[j][1];
            const i32 i = spawnEnemy(sp, at, angleOf(dir), t + kWarpBeats * kTicksPerBeat);
            if (i < 0) break;
            enemies_.flags[i] |= kFlagCalled;
            ++alive;
            ++s.mini.called;
        }
        Event ev;
        ev.type = EventType::Telegraph;
        ev.species = kBossSpecies;
        ev.x = tip.x;
        ev.y = tip.y;
        ev.x2 = dir.x;
        ev.y2 = dir.y;
        ev.tick = t + kWarpBeats * kTicksPerBeat;
        emit(ev);
    }
    emitCue(BossCue::AlHowl, t, 0, 0.0f, s.body[0]);
    emitCue(BossCue::AlCharge, t + kWarpBeats * kTicksPerBeat, 0, 0.0f, s.body[0]);
}

void World::updateAlpha(float dt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    const bool fight = m1_ >= s.fightTick;
    // Bars and beats: the howl on beat 1 (host: the pack is spawned), the lunge's lock on beat 3 of every fourth bar
    // (third from half health), its dash on the next downbeat.
    if (fight) {
        forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
            if (t < s.fightTick - 0.5) return;
            const u32 j = bossFightBar(t + 1.0);
            const i64 b = imod(beat, 4);
            const u32 every = m.half ? kAlphaHalfLungeBars : kAlphaLungeBars;
            if (b == 0 && role_ != NetRole::Client) alphaHowl(t);
            if (b == 2 && j % every == every - 1 && m.state == 0) {
                const i32 near = nearestPlayer(s.body[0].x, s.body[0].y);
                const Vec2 at = near >= 0 ? players_[near].pos : s.body[0] + m.face * 400.0f;
                bossDecide(kGeomAlphaLock, at, normalizeOr(at - s.body[0], m.face), t);
                m.state = 1;
                m.cueTick = t + 2.0 * kTicksPerBeat;
                emitCue(BossCue::AlLunge, m.cueTick, 0, 0.0f, s.body[0]);
            }
            if (b == 0 && m.state == 1 && t >= m.cueTick - 0.5) {
                m.state = 2;
                m.stateUntil = t + kTicksPerBeat;
            }
            if (m.state == 2 && t >= m.stateUntil - 0.5) {
                m.state = 3;
                m.winded = t + kAlphaWindedBeats * kTicksPerBeat;
                m.stateUntil = m.winded;
            }
            if (m.state == 3 && t >= m.stateUntil - 0.5) m.state = 0;
        });
    }
    Vec2& p = s.body[0];
    const i32 near = nearestPlayer(p.x, p.y);
    if (m.state == 2) {
        p = p + m.lockDir * (kAlphaLungeSpeed * dt);
        m.face = m.lockDir;
    } else if (m.state == 0 && near >= 0) {
        // Circle the nearest player at 420 wu, 170 wu/s, turning to keep them in front.
        const Vec2 P = players_[near].pos;
        const Vec2 rel = p - P;
        const float r = length(rel);
        const Vec2 radial = r > 1e-3f ? rel * (1.0f / r) : Vec2{1.0f, 0.0f};
        const Vec2 tangent = perpOf(radial);
        const Vec2 v = normalizeOr(tangent + radial * clampf((kAlphaCircleWu - r) / 150.0f, -1.0f, 1.0f), tangent) * kAlphaCircleSpeed;
        p = p + v * dt;
        m.face = normalizeOr(P - p, m.face);
    } else if (near >= 0 && m.state == 1) {
        m.face = m.lockDir;
    }
    const MiniData::Entry* e = species_->minis.find(s.kind);
    const float r = e ? e->radius : 64.0f;
    p = Vec2{clampf(p.x, -kArenaHalfW + r, kArenaHalfW - r), clampf(p.y, -kArenaHalfH + r, kArenaHalfH - r)};
    s.bodyAng[0] = angleOf(m.face);
}

// ---- The Hive: a cloud that becomes shapes; only the Queen has health.

void World::hiveHatch(u32 n, double land) {
    BossState& s = boss_;
    u32 alive = 0;
    for (u32 i = 0; i < enemies_.count; ++i)
        if (!(enemies_.flags[i] & kFlagDead) && (enemies_.flags[i] & kFlagCalled)) ++alive;
    const u16 sp = SpeciesTable::index(Family::Shoal, 1, kVariantA);  // a dart: the lightest Shoal (dies to one shot)
    for (u32 k = 0; k < n && alive < kHiveMax; ++k) {
        const Vec2 at = s.body[0] + dirOf(rng_.range(0.0f, kTau)) * rng_.range(20.0f, 140.0f);
        const i32 i = spawnEnemy(sp, at, rng_.range(0.0f, kTau), land);
        if (i < 0) break;
        enemies_.flags[i] |= kFlagCalled;
        enemies_.hp[i] = 0.01f;  // dies to one shot
        enemies_.a[i] = -1.0f;   // no slot until the next bar line
        ++alive;
        ++s.mini.called;
    }
}

void World::hiveReslot() {
    // Each live dart's place in its hive's shape: by spawn number (the same order on every device), alternately into
    // the two hives after the half.
    BossState& s = boss_;
    MiniState& m = s.mini;
    Enemies& e = enemies_;
    u32* idx = hiveIdx_;
    u32 n = 0;
    for (u32 i = 0; i < e.count && n < kHiveMax + 8; ++i) {
        if ((e.flags[i] & kFlagDead) || !(e.flags[i] & kFlagCalled)) continue;
        // insertion by serial
        u32 j = n++;
        while (j > 0 && e.serial[idx[j - 1]] > e.serial[i]) {
            idx[j] = idx[j - 1];
            --j;
        }
        idx[j] = i;
    }
    m.slots[0] = m.slots[1] = 0;
    for (u32 r = 0; r < n; ++r) {
        const u32 h = m.hives > 1 ? r % 2u : 0u;
        e.a[idx[r]] = static_cast<float>(m.slots[h]++);
        e.b[idx[r]] = static_cast<float>(h);
    }
}

void World::hiveShapeStart(double t) {
    // A new shape for each hive, round the nearest player now.
    BossState& s = boss_;
    MiniState& m = s.mini;
    const i32 near = nearestPlayer(s.body[0].x, s.body[0].y);
    const Vec2 P = near >= 0 ? players_[near].pos : s.body[0];
    for (u32 h = 0; h < m.hives; ++h) {
        const Vec2 from = h == 0 ? s.body[0] : s.body[1];
        const Vec2 toP = normalizeOr(P - from, Vec2{1.0f, 0.0f});
        // The host's call (bossDecide), so every device's cloud takes the same shape in the same place.
        switch (m.shape[h]) {
            case 0: bossDecide(static_cast<u8>(kGeomHive0 + h), P, normalizeOr(P - from, Vec2{1.0f, 0.0f}), t); break;  // the gap faces away
            case 1: bossDecide(static_cast<u8>(kGeomHive0 + h), from, toP, t); break;
            default: bossDecide(static_cast<u8>(kGeomHive0 + h), P - toP * 500.0f, toP, t); break;  // the Wall starts 500 wu behind them
        }
    }
    m.shapeTick = t;
}

Vec2 World::hiveSlot(u32 h, float slot, double t, bool queen) const {
    // Where a dart (or the Queen) of hive h stands at song time t in its shape.
    const BossState& s = boss_;
    const MiniState& m = s.mini;
    const u32 n = m.slots[h] ? m.slots[h] : 1u;
    const double since = t - m.shapeTick;
    const float u = clampf(static_cast<float>(since / (kHiveShapeBars * kTicksPerBar)), 0.0f, 1.0f);
    switch (m.shape[h]) {
        case 0: {
            // Ring: radius 300 round the player closing to 120 over the two bars, one gap of 60 degrees that turns half a
            // turn a bar; the Queen rides the ring opposite the gap.
            const Vec2 c = m.shapeAt[h];  // round where the player stood as it formed
            const float R = kHiveRingFrom + (kHiveRingTo - kHiveRingFrom) * u;
            const float gap = m.gap[h] + kPi * static_cast<float>(since / kTicksPerBar);
            if (queen) return c + dirOf(gap + kPi) * R;
            const float span = kTau - kHiveRingGapDeg * kDeg;
            const float a = gap + 0.5f * kHiveRingGapDeg * kDeg + span * (slot + 0.5f) / static_cast<float>(n);
            return c + dirOf(a) * R;
        }
        case 1: {
            // Spear: a tight column (rows of three) that charges on beats 1 and 3 at 700 wu/s; the Queen is its last dart.
            const float row = queen ? static_cast<float>((n + 2) / 3 + 1) : std::floor(slot / 3.0f);
            const float col = queen ? 0.0f : std::fmod(slot, 3.0f) - 1.0f;
            return m.shapeAt[h] - m.shapeDir[h] * (row * 16.0f) + perpOf(m.shapeDir[h]) * (col * 14.0f);
        }
        default: {
            // Wall: 900 wu long, crossing at 160 wu/s, two gaps of 90 wu; the Queen flies behind its middle.
            const float secs = static_cast<float>(since) / ticksPerSecond();
            const Vec2 c = m.shapeAt[h] + m.shapeDir[h] * (kHiveWallSpeed * secs);
            if (queen) return c - m.shapeDir[h] * 60.0f;
            const float g = static_cast<float>(hash32(static_cast<u32>(m.shapeTick), 0x6A9u + h) % 300u) - 150.0f;
            const float g0 = g - 0.5f * kHiveWallGap, g1 = g0 + 0.5f * kHiveWallLen;
            const float open = kHiveWallLen - 2.0f * kHiveWallGap;
            float x = -0.5f * kHiveWallLen + open * (slot + 0.5f) / static_cast<float>(n);
            if (x >= g0) x += kHiveWallGap;
            if (x >= g1) x += kHiveWallGap;
            return c + perpOf(m.shapeDir[h]) * x;
        }
    }
}

void World::updateHive(float dt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    const bool fight = m1_ >= s.fightTick;
    if (fight) {
        forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
            if (t < s.fightTick - 0.5) return;
            const u32 j = bossFightBar(t + 1.0);
            // A new shape every two bars, on the bar line: Ring, Spear, Wall, and round again.
            if (j > 0 && j % kHiveShapeBars == 0) {
                for (u32 h = 0; h < m.hives; ++h) m.shape[h] = static_cast<u8>((m.shape[h] + 1u) % 3u);
                hiveShapeStart(t);
                emitCue(m.shape[0] == 0 ? BossCue::HvRing : (m.shape[0] == 1 ? BossCue::HvSpear : BossCue::HvWall), t, 0, 0.0f, s.body[0]);
            }
            // The tell: the last beat of a shape, every dart flashing white (the Queen takes x 2 while it lasts).
            if (j % kHiveShapeBars == kHiveShapeBars - 1) {
                m.cueTick = t + 3.0 * kTicksPerBeat;  // the tell: the shape's last beat
                emitCue(BossCue::HvShape, t + kTicksPerBar, 0, 0.0f, s.body[0]);
            }
            hiveReslot();
            // She hatches 12 darts a bar while she lives, up to 160.
            if (role_ != NetRole::Client) hiveHatch(kHiveHatch, t + kWarpBeats * kTicksPerBeat);
        });
        // The Spear: on beats 2 and 4 the line locks (a beat ahead), on beats 1 and 3 it charges for a beat.
        forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
            if (t < s.fightTick - 0.5) return;
            const i64 b = imod(beat, 4);
            for (u32 h = 0; h < m.hives; ++h) {
                if (m.shape[h] != 1) continue;
                if (b == 1 || b == 3) {
                    const i32 near = nearestPlayer(m.shapeAt[h].x, m.shapeAt[h].y);
                    if (near >= 0)
                        bossDecide(static_cast<u8>(kGeomSpear0 + h), m.shapeAt[h], normalizeOr(players_[near].pos - m.shapeAt[h], m.shapeDir[h]), t);
                    Event ev;
                    ev.type = EventType::Telegraph;
                    ev.species = kBossSpecies;
                    ev.x = m.shapeAt[h].x;
                    ev.y = m.shapeAt[h].y;
                    ev.x2 = m.shapeDir[h].x;
                    ev.y2 = m.shapeDir[h].y;
                    ev.tick = t + kTicksPerBeat;
                    emit(ev);
                }
            }
        });
        for (u32 h = 0; h < m.hives; ++h) {
            if (m.shape[h] != 1) continue;
            const i64 b = imod(static_cast<i64>(std::floor(m1_ / kTicksPerBeat)), 4);
            if (b == 0 || b == 2) m.shapeAt[h] = m.shapeAt[h] + m.shapeDir[h] * (kHiveSpearSpeed * dt);
        }
    }
    // The darts follow their places; the Queen (and the decoy) theirs.
    const float speed = kHiveDartSpeed;
    Enemies& e = enemies_;
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || !(e.flags[i] & kFlagCalled) || e.state[i] == kWarping) continue;
        e.state[i] = kFormation;
    }
    for (u32 h = 0; h < m.hives; ++h) {
        Vec2& q = s.body[h];
        const Vec2 want = fight ? hiveSlot(h, 0.0f, m1_, true) : q;
        const Vec2 to = want - q;
        const float l = length(to);
        q = l <= speed * dt ? want : q + to * (speed * dt / l);
        s.bodyAng[h] = l > 1e-3f ? angleOf(to) : s.bodyAng[h];
    }
}

// ---- Battering Ram: a bullfight in the Pen.

void World::ramCharge(double t) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    m.state = 2;
    m.stateUntil = -1.0;
    m.face = m.lockDir;
    m.wake = m.half;
    m.wakeFrom = s.body[0];
    m.wakeTo = s.body[0];
    m.wakeUntil = -1.0;
    emitCue(BossCue::RmCharge, t, 0, 0.0f, s.body[0]);
}

void World::updateRam(float dt) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    const MiniData::Entry* e = species_->minis.find(s.kind);
    const float r = e ? e->radius : 80.0f;
    const bool fight = m1_ >= s.fightTick;
    Vec2& p = s.body[0];
    if (fight) {
        forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
            if (t < s.fightTick - 0.5) return;
            const i64 b = imod(beat, 4);
            const u32 j = bossFightBar(t + 1.0);
            // Two bars a charge: bar one beats 1-2 it turns to face the nearest player, beat 3 it locks, beat 4 it holds;
            // bar two beat 1 it charges.
            if (m.state == 4 && t >= m.stateUntil - 0.5) {
                m.state = 0;
                emitCue(BossCue::RmStun, t, 0, 0.0f, p);
            }
            if (m.state == 3 && t >= m.stateUntil - 0.5) {
                if (m.again) {
                    // Half health: after a skid it locks again in one beat and charges a second time.
                    m.again = false;
                    const i32 near = nearestPlayer(p.x, p.y);
                    const Vec2 at = near >= 0 ? players_[near].pos : p + m.face * 400.0f;
                    bossDecide(kGeomRamLock, at, normalizeOr(at - p, m.face), t);
                    m.state = 1;
                    m.cueTick = t + kTicksPerBeat;
                    emitCue(BossCue::RmLock, t, 1, 0.0f, p);
                } else {
                    m.state = 0;
                }
            }
            if (j % 2u == 0u && b == 2 && m.state == 0) {
                const i32 near = nearestPlayer(p.x, p.y);
                const Vec2 at = near >= 0 ? players_[near].pos : p + m.face * 400.0f;
                bossDecide(kGeomRamLock, at, normalizeOr(at - p, m.face), t);
                m.state = 1;
                m.cueTick = t + 2.0 * kTicksPerBeat;
                m.again = m.half;
                emitCue(BossCue::RmLock, t, 0, 0.0f, p);
            }
            if (m.state == 1 && t >= m.cueTick - 0.5) ramCharge(t);
        });
    }
    const i32 near = nearestPlayer(p.x, p.y);
    if (m.state == 0 && near >= 0) {
        // Turning to face the nearest player.
        const float want = angleOf(players_[near].pos - p);
        m.face = dirOf(turnToward(angleOf(m.face), want, kRamTurnRate * dt));
    } else if (m.state == 2) {
        // The charge: to where the player stood at the lock; if a wall is within 300 wu behind that place it cannot stop.
        const Vec2 from = p;
        Vec2 to = p + m.lockDir * (kRamChargeSpeed * dt);
        const float past = dot(to - m.lockAt, m.lockDir);
        bool wallBehind = false;
        if (pen_.up) {
            Vec2 probe = m.lockAt + m.lockDir * kRamWallWu;
            wallBehind = !penInside(probe, r);
        }
        if (past >= 0.0f && !wallBehind) {
            to = m.lockAt;  // it skids to a stop there
            m.state = 3;
            m.stateUntil = beatLine(m1_) + kTicksPerBeat;
        }
        const u8 hit = penKeep(from, to, r);
        p = to;
        m.wakeTo = p;
        if (hit && wallBehind) {
            // The wall rings: 16 bullets fan out from the point of impact, and it is stunned.
            const Vec2 n = (hit & 1u) ? Vec2{p.x > pen_.centre.x ? -1.0f : 1.0f, 0.0f} : Vec2{0.0f, p.y > pen_.centre.y ? -1.0f : 1.0f};
            const Vec2 at = p - n * r;
            const u32 count = bossScaledCount(kRamWallBullets, bossBulletDensity(), 4u);
            const float speed = species_->bulletSpeed;
            for (u32 k = 0; k < count; ++k) {
                const float a = angleOf(n) + (static_cast<float>(k) / static_cast<float>(count > 1 ? count - 1 : 1) - 0.5f) * kRamWallFanDeg * kDeg;
                spawnBullet(at, dirOf(a) * speed, 0.0f, kBulletLifeS);
            }
            m.state = 4;
            m.again = false;
            m.stateUntil = beatLine(m1_) + (m.half ? kRamHalfStunBeats : kRamStunBeats) * kTicksPerBeat;
            m.winded = m.stateUntil;
            emitCue(BossCue::RmWall, m1_, 0, 0.0f, at);
            emitCue(BossCue::RmStun, m1_, 1, static_cast<float>(m.half ? kRamHalfStunBeats : kRamStunBeats), at);  // f: its beats
        }
        if (m.state != 2 && m.wake) m.wakeUntil = m1_ + kRamWakeBars * kTicksPerBar;
    }
    if (pen_.up) {
        Vec2 q = p;
        penKeep(pen_.centre, q, r);
        p = q;
    }
    s.bodyAng[0] = angleOf(m.face);
}

bool World::ramWakeHit(Vec2 at) const {
    const MiniState& m = boss_.mini;
    if (!m.wake || !(m1_ < m.wakeUntil || boss_.mini.state == 2)) return false;
    const Vec2 d = m.wakeTo - m.wakeFrom;
    const float l2 = lengthSq(d);
    const float t = l2 > 1e-6f ? clampf(dot(at - m.wakeFrom, d) / l2, 0.0f, 1.0f) : 0.0f;
    const float half = kRamWakeWu + kPlayerHitRadius;
    return lengthSq(m.wakeFrom + d * t - at) <= half * half;
}

// ---- Carom King: straight legs round the Pen, every bounce on a beat.

void World::caromLeg(u32 k, Vec2 from, Vec2 dir, double t, bool halfBeats) {
    // From `from` along `dir` to the Pen's wall (the arena's without one); its speed trimmed so the bounce lands on a
    // beat (a Prince: a half beat).
    BossState& s = boss_;
    MiniState& m = s.mini;
    const float r = k == 0 ? kKingRadii[m.splits < 4 ? m.splits : 3] : kPrinceRadius;
    const Vec2 c = pen_.up ? pen_.centre : Vec2{};
    const float hx = (pen_.up ? pen_.half : kArenaHalfW) - r, hy = (pen_.up ? pen_.half : kArenaHalfH) - r;
    dir = normalizeOr(dir, Vec2{1.0f, 0.0f});
    if (std::fabs(dir.x) < 0.15f) dir = normalizeOr(Vec2{dir.x < 0.0f ? -0.15f : 0.15f, dir.y}, dir);  // never along a wall
    if (std::fabs(dir.y) < 0.15f) dir = normalizeOr(Vec2{dir.x, dir.y < 0.0f ? -0.15f : 0.15f}, dir);
    float tx = 1e9f, ty = 1e9f;
    if (dir.x > 0.0f) tx = (c.x + hx - from.x) / dir.x;
    if (dir.x < 0.0f) tx = (c.x - hx - from.x) / dir.x;
    if (dir.y > 0.0f) ty = (c.y + hy - from.y) / dir.y;
    if (dir.y < 0.0f) ty = (c.y - hy - from.y) / dir.y;
    float dist = std::fmax(std::fmin(tx, ty), 1.0f);
    m.legWall[k] = tx < ty ? (dir.x > 0.0f ? 2 : 0) : (dir.y > 0.0f ? 3 : 1);
    const float speed = kCaromSpeed * m.speedFactor[k];
    const double grid = halfBeats ? 0.5 * kTicksPerBeat : kTicksPerBeat;
    const double ticks = static_cast<double>(dist / speed) * ticksPerSecond();
    const double end = std::fmax(std::floor((t + ticks) / grid + 0.5) * grid, std::floor(t / grid) * grid + grid);
    m.legFrom[k] = from;
    m.legTo[k] = from + dir * dist;
    m.legStart[k] = t;
    m.legEnd[k] = end;
}

void World::caromBounce(u32 k, double t) {
    // At the wall: reflect, speed up 8% (up to x 1.6), shed five bullets fanned 40 degrees either side of the new heading
    // (the King), turn gold for a beat.
    BossState& s = boss_;
    MiniState& m = s.mini;
    const Vec2 at = m.legTo[k];
    const Vec2 d = normalizeOr(m.legTo[k] - m.legFrom[k], Vec2{1.0f, 0.0f});
    Vec2 nd = d;
    const u8 wall = m.legWall[k];
    if (wall == 0 || wall == 2) nd.x = -nd.x;
    else nd.y = -nd.y;
    const float maxF = k == 0 ? kCaromMaxFactor * std::pow(kKingSplitSpeed, static_cast<float>(m.splits)) : kCaromMaxFactor * kPrinceSpeed;
    m.speedFactor[k] = std::fmin(m.speedFactor[k] * (1.0f + kCaromBounceGain), maxF);
    m.hot[k] = t + kCaromHotBeats * kTicksPerBeat;
    static const u8 kChord[4] = {0, 1, 2, 3};  // left the root, top the third, right the fifth, bottom the seventh
    emitCue(BossCue::CkBounce, t, static_cast<u8>(k == 0 ? 0 : 1), static_cast<float>(kChord[wall]), at);
    if (k == 0) {
        // The volley, inside the cap of 60 of its bullets in flight.
        u32 flying = 0;
        for (double until : m.bulletsUntil)
            if (until > t) flying += kCaromBounceBullets;
        if (flying + kCaromBounceBullets <= kCaromBulletCap) {
            const u32 n = bossScaledCount(kCaromBounceBullets, bossBulletDensity(), 3u);
            for (u32 j = 0; j < n; ++j) {
                const float a = angleOf(nd) + (static_cast<float>(j) / static_cast<float>(n > 1 ? n - 1 : 1) - 0.5f) * 2.0f * kCaromBounceFanDeg * kDeg;
                spawnBullet(at, dirOf(a) * species_->bulletSpeed, tickAgeOf(t), kBulletLifeS);
            }
            double* slot = &m.bulletsUntil[0];
            for (double& until : m.bulletsUntil)
                if (until < *slot) slot = &until;
            *slot = t + static_cast<double>(kBulletLifeS) * ticksPerSecond();
        }
    }
    caromLeg(k, at, nd, t, k != 0);
}

void World::updateCarom(float /*dt*/) {
    BossState& s = boss_;
    MiniState& m = s.mini;
    if (m1_ < s.fightTick) {
        s.bodyAng[0] = angleOf(m.face);
        return;
    }
    if (m.legEnd[0] < 0.0) caromLeg(0, s.body[0], m.face, s.fightTick, false);
    for (u32 k = 0; k < 4; ++k) {
        if (k > 0 && !(s.alive & (1u << (k - 1)))) continue;
        // Every bounce whose beat this tick crossed (at most a few).
        for (u32 guard = 0; guard < 4 && m.legEnd[k] >= 0.0 && m1_ >= m.legEnd[k]; ++guard) caromBounce(k, m.legEnd[k]);
        const double span = m.legEnd[k] - m.legStart[k];
        const float u = span > 1e-6 ? clampf(static_cast<float>((m1_ - m.legStart[k]) / span), 0.0f, 1.0f) : 1.0f;
        s.body[k] = m.legFrom[k] + (m.legTo[k] - m.legFrom[k]) * u;
        s.bodyAng[k] = angleOf(m.legTo[k] - m.legFrom[k]);
    }
    m.face = dirOf(s.bodyAng[0]);
}

// ---------------------------------------------------------------------------------------------- called bodies

bool World::minionMotion(u32 i, float dt) {
    // What a mini-boss's called bodies do instead of their family's motion; false: the family's own.
    Enemies& e = enemies_;
    if (!(e.flags[i] & kFlagCalled)) return false;
    switch (e.state[i]) {
        case kLane: return horizonLane(i, dt);  // M5 part 4: Event Horizon's lanes
        case kCharge: {
            // The pack's charge: along its line at 520 wu/s for two beats, through and past; then an ordinary Hound.
            e.vx[i] = e.a[i] * kAlphaChargeSpeed;
            e.vy[i] = e.b[i] * kAlphaChargeSpeed;
            e.heading[i] = angleOf(Vec2{e.a[i], e.b[i]});
            if (m1_ >= e.until[i]) {
                e.state[i] = kMoving;
                e.flags[i] &= static_cast<u8>(~kFlagCalled);
            }
            return true;
        }
        case kFlee: {
            // The pack scatters: away from the nearest player for two bars.
            const i32 tp = nearestPlayer(e.x[i], e.y[i]);
            const Vec2 away = tp >= 0 ? normalizeOr(Vec2{e.x[i], e.y[i]} - players_[tp].pos, dirOf(e.heading[i])) : dirOf(e.heading[i]);
            e.heading[i] = turnToward(e.heading[i], angleOf(away), 4.0f * dt);
            const Vec2 v = dirOf(e.heading[i]) * species_->get(e.species[i]).speed;
            e.vx[i] = v.x;
            e.vy[i] = v.y;
            if (m1_ >= e.until[i]) {
                e.state[i] = kMoving;
                e.flags[i] &= static_cast<u8>(~kFlagCalled);
            }
            return true;
        }
        case kFormation: {
            // A Hive dart: to its place in its hive's shape (before its first bar line, toward the Queen).
            const BossState& s = boss_;
            if (!s.active || s.kind != BossKind::Hive) {
                e.flags[i] &= static_cast<u8>(~kFlagCalled);
                e.state[i] = kMoving;
                return false;
            }
            const u32 h = e.b[i] > 0.5f && s.mini.hives > 1 ? 1u : 0u;
            const Vec2 want = e.a[i] >= 0.0f && m1_ >= s.fightTick ? hiveSlot(h, e.a[i], m1_, false) : s.body[h];
            const Vec2 to = want - Vec2{e.x[i], e.y[i]};
            const float l = length(to);
            const float sp = kHiveDartSpeed / loopSpeed_;
            const Vec2 v = l <= sp * dt ? to * (1.0f / (dt * loopSpeed_)) : to * (sp / l);
            e.vx[i] = v.x;
            e.vy[i] = v.y;
            if (l > 1e-3f) e.heading[i] = angleOf(to);
            return true;
        }
        default: return false;
    }
}

void World::minionLand(u32 i) {
    // A called body becomes solid: a pack Hound starts its charge along the line it warped in on.
    Enemies& e = enemies_;
    const Species& s = species_->get(e.species[i]);
    if (s.family == Family::Hound) {
        const Vec2 d = dirOf(e.heading[i]);
        e.a[i] = d.x;
        e.b[i] = d.y;
        e.state[i] = kCharge;
        e.until[i] = e.until[i] + kAlphaChargeBeats * kTicksPerBeat;
        e.pheading[i] = e.heading[i];
    } else if (s.family == Family::Shoal) {
        e.state[i] = kFormation;
        e.heading[i] = e.pheading[i] = randRange(e.rng[i], 0.0f, kTau);
    }
}

}  // namespace rf::sim
