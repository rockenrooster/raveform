// M5 part 4: Polyrhythm, the boss of Track 5 (design/bosses/polyrhythm.md, .json). Two rotors joined by the link: Three
// fires three times a bar and Four four times, and what they fire is walls: arcs thrown by the corners, a gap in front of
// every face, and the faces are the only place a rotor can be hurt. On the One both fire with their gaps along the link.
// Hemiola (60% of their joint health and below) reverses their turn every bar and burns the link every second bar; with
// one dead the survivor plays both counts (Solo).
//
// Everything is a function of song time, the pivot (it follows the players' centre) and which rotors live. Co-op: the
// host decides the deaths (as every part's) and when Hemiola begins (bossDecide kGeomPolyHemiola); the pivot travels as
// the boss's sync point. A wall erased by a hit is this device's own (what you see is what hits you); a Nova's erasing
// follows the Nova on every device.

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kDegR = kTau / 360.0f;
constexpr u32 kSectors = 64;
constexpr float kSector = kTau / static_cast<float>(kSectors);

float wrap2Pi(float a) {
    a = std::fmod(a, kTau);
    return a < 0.0f ? a + kTau : a;
}
u32 sectorOf(float a) { return static_cast<u32>(wrap2Pi(a) / kSector) % kSectors; }
void eraseArc(PolyRing& r, float a0, float a1) {
    // Every sector the arc [a0, a1] touches.
    const u32 n = static_cast<u32>(std::ceil((a1 - a0) / kSector)) + 1u;
    for (u32 k = 0; k <= n && k < kSectors; ++k) r.erased |= 1ull << sectorOf(a0 + static_cast<float>(k) * kSector);
}
u32 quartersLost(const BossState& s, u32 k) {
    if (s.partMax <= 0.0f) return 0;
    const float lost = 1.0f - s.partHp[k] / s.partMax;
    const i32 q = static_cast<i32>(std::floor(lost * 4.0f));
    return static_cast<u32>(q < 0 ? 0 : (q > 3 ? 3 : q));
}
}  // namespace

float World::polyLinkAngle(double t) const {
    const BossState& s = boss_;
    const double since = t > s.fightTick ? t - s.fightTick : 0.0;
    return s.pr.linkAngle0 + static_cast<float>(since / (prose::kPrLinkTurnBars * kTicksPerBar)) * kTau;
}

Vec2 World::polyRotor(u32 k) const { return boss_.body[k < 2 ? k : 0]; }

float World::polyFace(u32 k, double t) const {
    // A face of rotor k looks at the other on every One; between pulses it turns turnDeg, so its gaps step round
    // (Hemiola: the other way every second bar from its first).
    const BossState& s = boss_;
    const PolyData& d = species_->poly;
    k = k < 2 ? k : 0;
    const float toOther = polyLinkAngle(t) + (k == 0 ? 0.0f : kPi);
    const double bar = std::floor(t / kTicksPerBar);
    const double inBar = t - bar * kTicksPerBar;
    const float done = static_cast<float>(inBar / (kTicksPerBar / static_cast<double>(d.pulses[k] ? d.pulses[k] : 1u)));
    float sign = 1.0f;
    const i64 b = static_cast<i64>(bar);
    if (s.pr.phase == 1 && b >= s.pr.hemiolaBar0 && ((b - s.pr.hemiolaBar0) & 1)) sign = -1.0f;
    return toOther + sign * d.turnDeg[k] * kDegR * done;
}

float World::polySecondFace(double t) const {
    // Solo: the dead rotor's count on a second ring from the survivor, turning the other way; on the One its gap lines up
    // with the survivor's own, in front of the face that looked at the partner.
    const BossState& s = boss_;
    const PolyData& d = species_->poly;
    const u32 k = (s.alive & 1u) ? 0u : 1u, dead = 1u - k;
    const float toOther = polyLinkAngle(t) + (k == 0 ? 0.0f : kPi);
    const double inBar = t - std::floor(t / kTicksPerBar) * kTicksPerBar;
    const float done = static_cast<float>(inBar / (kTicksPerBar / static_cast<double>(d.pulses[dead] ? d.pulses[dead] : 1u)));
    return toOther - d.turnDeg[dead] * kDegR * done;
}

float World::polyRingRadius(const PolyRing& r, double t) const {
    return species_->poly.rotorRadius + r.speed * static_cast<float>((t - r.tick) / ticksPerSecond());
}

bool World::polyRingAlive(const PolyRing& r, double t) const {
    return r.tick >= 0.0 && t >= r.tick && static_cast<float>((t - r.tick) / ticksPerSecond()) < species_->poly.wallLife;
}

bool World::polyWallAt(const PolyRing& r, float angle) const {
    // The corners throw the arcs (centred on them); the faces throw nothing.
    const u32 n = r.sides ? r.sides : 3u;
    const float corner0 = r.face0 + kPi / static_cast<float>(n);
    const float half = 0.5f * r.arcDeg * kDegR;
    bool on = false;
    for (u32 j = 0; j < n && !on; ++j) on = std::fabs(wrapAngle(angle - (corner0 + kTau * static_cast<float>(j) / static_cast<float>(n)))) <= half;
    return on && !((r.erased >> sectorOf(angle)) & 1ull);
}

bool World::polyWallHit(Vec2 at, Vec2& from) const {
    const BossState& s = boss_;
    const PolyData& d = species_->poly;
    for (const PolyRing& r : s.pr.rings) {
        if (!polyRingAlive(r, m1_)) continue;
        const float rad = polyRingRadius(r, m1_);
        const Vec2 rel = at - r.at;
        const float dist = length(rel);
        if (std::fabs(dist - rad) > 0.5f * d.wallThickness + kPlayerHitRadius) continue;
        const float a = angleOf(rel);
        if (!polyWallAt(r, a)) continue;
        from = r.at + dirOf(a) * rad;
        return true;
    }
    return false;
}

u8 World::polyLinkBurn(double t) const {
    // Hemiola: every second bar the link burns on beats 3 and 4, after a bar of danger dashes (shown until it burns: the
    // aisle is still open on the One).
    const BossState& s = boss_;
    if (!s.active || s.dying || s.pr.phase != 1 || s.alive != 3u) return 0;
    const i64 bar = static_cast<i64>(std::floor(t / kTicksPerBar));
    const i64 rel = bar - s.pr.hemiolaBar0;
    if (rel < 0) return 0;
    const double inBar = t - static_cast<double>(bar) * kTicksPerBar;
    if ((rel & 1) == 0) return 1;
    if (inBar < 2.0 * kTicksPerBeat) return 1;
    if (inBar < (2.0 + species_->poly.burnBeats) * kTicksPerBeat) return 2;
    return 0;
}

void World::enterPoly(Vec2 centre) {
    BossState& s = boss_;
    const PolyData& d = species_->poly;
    s.fightTick = s.enterTick + 2.0 * kTicksPerBar;  // Three lands on beat 1, Four on beat 3; the link draws over the next bar
    s.phaseTick = s.fightTick;
    s.alive = 0x3;
    s.partMax = d.health[0] * s.scale;
    s.partHp[0] = d.health[0] * s.scale;
    s.partHp[1] = d.health[1] * s.scale;
    s.coreMax = s.partHp[0] + s.partHp[1];
    s.coreHp = s.coreMax;
    s.pr = PolyState{};
    const float mx = kArenaHalfW - 0.5f * d.span - d.rotorRadius - 120.0f, my = kArenaHalfH - d.rotorRadius - 220.0f;
    s.pos = s.prevPos = Vec2{clampf(centre.x, -mx, mx), clampf(centre.y, -my, my)};
    s.pr.linkAngle0 = 0.0f;  // across the frame (the screen is wide)
    for (u32 k = 0; k < 2; ++k) {
        s.body[k] = s.bodyPrev[k] = s.pos + dirOf(s.pr.linkAngle0) * ((k == 0 ? -0.5f : 0.5f) * d.span);
        s.bodyAng[k] = s.bodyPrevAng[k] = polyFace(k, s.enterTick);
    }
}

float World::polyScale(u8 part, u8 index) const {
    // Only the faces can be hurt (x 1; the Solo survivor's x 1.25); corners and body shed shots.
    const PolyData& d = species_->poly;
    if (part != 2 || !(boss_.alive & (1u << index))) return d.dmgCorner;
    return boss_.pr.phase == 2 ? d.dmgSolo : d.dmgFace;
}

void World::polyPartDied(u8 index, Event& ev) {
    // The first to die spins down and bursts on the One (the view); the survivor carries both counts and walks.
    BossState& s = boss_;
    PolyState& pr = s.pr;
    if (pr.firstDead == 0xFF) {
        pr.firstDead = index;
        pr.firstDeathTick = m1_;
    }
    ev.x = s.body[index].x;
    ev.y = s.body[index].y;
    if (s.alive) {
        pr.phase = 2;
        s.phase = 2;
        s.phaseTick = m1_;
        s.pos = s.body[(s.alive & 1u) ? 0 : 1];
    }
}

void World::polyFire(u32 k, double t, bool second) {
    BossState& s = boss_;
    PolyState& pr = s.pr;
    const PolyData& d = species_->poly;
    // The slot of the oldest ring (every ring dies after wallLife).
    u32 slot = 0;
    double oldest = 1e300;
    for (u32 j = 0; j < PolyState::kRings; ++j) {
        const PolyRing& r = pr.rings[j];
        if (!polyRingAlive(r, t)) {
            slot = j;
            break;
        }
        if (r.tick < oldest) {
            oldest = r.tick;
            slot = j;
        }
    }
    PolyRing& r = pr.rings[slot];
    r = PolyRing{};
    r.tick = t;
    r.at = pr.phase == 2 ? s.pos : s.body[k];
    r.face0 = second ? polySecondFace(t) : polyFace(k, t);
    r.sides = static_cast<u8>(d.sides[k]);
    r.arcDeg = d.arcDeg[k];
    r.rotor = static_cast<u8>(k);
    r.speed = d.ringSpeed;
    if (k == 1 && pr.phase == 1 && static_cast<i64>(std::floor(t / kTicksPerBar)) >= pr.hemiolaBar0) r.speed = prose::kPrFourHemiolaSpeed;
}

void World::polyEraseHit(Vec2 at) {
    // A wall that hits takes a Shield cell like a bullet and is erased where it hit.
    const PolyData& d = species_->poly;
    for (PolyRing& r : boss_.pr.rings) {
        if (!polyRingAlive(r, m1_)) continue;
        const float rad = polyRingRadius(r, m1_);
        const Vec2 rel = at - r.at;
        if (std::fabs(length(rel) - rad) > 0.5f * d.wallThickness + kPlayerHitRadius + 4.0f) continue;
        const float a = angleOf(rel), half = (kPlayerHitRadius + 12.0f) / std::fmax(rad, 1.0f);
        eraseArc(r, a - half, a + half);
    }
}

void World::polyErase(Vec2 at, float radius) {
    // A Nova erases every wall it touches (its front's reach, at the blast).
    for (PolyRing& r : boss_.pr.rings) {
        if (!polyRingAlive(r, m1_)) continue;
        const float rad = polyRingRadius(r, m1_);
        for (u32 j = 0; j < kSectors; ++j) {
            const Vec2 p = r.at + dirOf((static_cast<float>(j) + 0.5f) * kSector) * rad;
            if (lengthSq(p - at) <= radius * radius) r.erased |= 1ull << j;
        }
    }
}

void World::updatePoly(float dt) {
    BossState& s = boss_;
    PolyState& pr = s.pr;
    const PolyData& d = species_->poly;
    const double t = m1_;
    // Where they are: the pivot follows the players' centre at 50 wu/s and the link turns round it once every 8 bars; the
    // Solo survivor walks toward the nearest player at 70 wu/s.
    if (pr.phase < 2) {
        if (t >= s.enterTick) {
            const Vec2 to = playersCentre() - s.pos;
            const float l = length(to);
            if (l > 1.0f) s.pos = s.pos + to * (std::fmin(l, prose::kPrPivotSpeed * dt) / l);
        }
        const float la = polyLinkAngle(t);
        for (u32 k = 0; k < 2; ++k) s.body[k] = s.pos + dirOf(la) * ((k == 0 ? -0.5f : 0.5f) * d.span);
    } else {
        const i32 near = nearestPlayer(s.pos.x, s.pos.y);
        if (near >= 0) {
            const Vec2 to = players_[near].pos - s.pos;
            const float l = length(to);
            if (l > 1.0f) s.pos = s.pos + to * (std::fmin(l, prose::kPrSoloWalk * dt) / l);
        }
        s.body[(s.alive & 1u) ? 0 : 1] = s.pos;
    }
    for (u32 k = 0; k < 2; ++k) s.bodyAng[k] = polyFace(k, t);
    s.coreHp = (s.alive & 1u ? s.partHp[0] : 0.0f) + (s.alive & 2u ? s.partHp[1] : 0.0f);
    if (t < s.fightTick) return;
    // Hemiola from the next bar line once their joint health is down to 60% (the host's word).
    if (pr.phase == 0 && s.alive == 3u && role_ != NetRole::Client && s.coreHp <= prose::kPrHemiolaShare * s.coreMax)
        bossDecide(kGeomPolyHemiola, Vec2{static_cast<float>(std::floor(t / kTicksPerBar) + 1.0), 0.0f}, Vec2{}, t);
    if (pr.phase == 1) s.phase = 1;
    // The pulses, the bar in twelfths: Three on 0, 4, 8; Four on 0, 3, 6, 9; the One on 0. The drums rise a semitone for
    // every quarter of their rotor's health lost (Four's rim sits on the flat second).
    forCrossings(m0_, m1_, kTicksPerBar / 12.0, [&](double tt, i64 idx) {
        const u32 j = static_cast<u32>(((idx % 12) + 12) % 12);
        const u32 every[2] = {12u / (d.pulses[0] ? d.pulses[0] : 3u), 12u / (d.pulses[1] ? d.pulses[1] : 4u)};
        if (pr.phase < 2) {
            for (u32 k = 0; k < 2; ++k) {
                if (!(s.alive & (1u << k)) || j % every[k]) continue;
                polyFire(k, tt, false);
                emitCue(k == 0 ? BossCue::PrThree : BossCue::PrFour, tt, static_cast<u8>(k), static_cast<float>(quartersLost(s, k) + k), s.body[k]);
            }
        } else {
            const u32 k = (s.alive & 1u) ? 0u : 1u, dead = 1u - k;
            const float semis = static_cast<float>(quartersLost(s, k));
            if (j % every[k] == 0) {
                polyFire(k, tt, false);
                emitCue(k == 0 ? BossCue::PrThree : BossCue::PrFour, tt, static_cast<u8>(k), semis + static_cast<float>(k), s.pos);
            }
            if (j % every[dead] == 0) {
                polyFire(dead, tt, true);
                emitCue(dead == 0 ? BossCue::PrThree : BossCue::PrFour, tt, static_cast<u8>(k), semis + static_cast<float>(dead), s.pos);
            }
        }
        if (j == 0) emitCue(BossCue::PrOne, tt, 0, 0.0f, s.pos);
    });
    // The link's burn: the telegraph tick on each beat of its dashes' bar, the impact as it burns.
    if (pr.phase == 1 && s.alive == 3u)
        forCrossings(m0_, m1_, kTicksPerBeat, [&](double tt, i64) {
            const i64 bar = static_cast<i64>(std::floor((tt + 1.0) / kTicksPerBar));
            const i64 rel = bar - pr.hemiolaBar0;
            const i64 beat = static_cast<i64>(std::floor((tt + 1.0 - static_cast<double>(bar) * kTicksPerBar) / kTicksPerBeat));
            const Vec2 mid = (s.body[0] + s.body[1]) * 0.5f;
            if (rel >= 0 && (rel & 1) == 0) emitCue(BossCue::PrLinkTick, tt, static_cast<u8>(beat), 0.0f, mid);
            if (rel >= 1 && (rel & 1) && beat == 2) emitCue(BossCue::PrLinkBurn, tt, 0, 0.0f, mid);
        });
}

}  // namespace rf::sim
