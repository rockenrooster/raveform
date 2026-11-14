// M5 part 3 (GAME_DESIGN 6.2-6.6): what the rest of the upgrades do in the world. The 28 Mods, 9 Riffs, 12 Boosts and 8
// Fusions the first slice did not have; the numbers no table gives are in tuning.h's "M5 PART 3" block (docs/PROGRESS.md,
// M5 part 3, "Numbers chosen").
//
// Co-op (M4, TECH_SPEC 8.1): damage is dealt only by the device that owns the player (its hits are claims there); what
// moves bodies or is seen (pulls, knockbacks, decoys, lances, mines, comets) runs on every device from the poses and the
// Trio and Boost events every device has, so their worlds agree (the lead's rule for part 3).

#include <cmath>
#include <initializer_list>

#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kDeg = kTau / 360.0f;
constexpr float kQuerySlackWu = 72.0f;  // the largest enemy (40 wu) and a tick's movement since the grid was built
constexpr u32 kFxPerTick = 384;         // upgrade events a tick at most (the view and the band aggregate them anyway)

inline i32 beatInBar(double tick) {
    const i64 b = static_cast<i64>(std::floor(tick / kTicksPerBeat + 0.5));
    return static_cast<i32>(((b % 4) + 4) % 4);
}
inline double inBeatOf(double tick) {
    double r = std::fmod(tick, kTicksPerBeat);
    return r < 0.0 ? r + kTicksPerBeat : r;
}
inline bool onBeat(double tick) {
    const double r = inBeatOf(tick);
    return r < 0.5 || kTicksPerBeat - r < 0.5;
}
inline bool offBeat(double tick) { return std::fabs(inBeatOf(tick) - 0.5 * kTicksPerBeat) < 0.5; }
// Distance from c to the segment a + dir * [0, len]; t the place along it.
inline float segmentDist(Vec2 c, Vec2 a, Vec2 dir, float len, float& t) {
    const Vec2 d = c - a;
    t = clampf(d.x * dir.x + d.y * dir.y, 0.0f, len);
    return length(c - (a + dir * t));
}
}  // namespace

// ---------------------------------------------------------------------------------------------- what the view reads

float World::crescendo(u32 p) const {
    const PlayerState& pl = players_[p];
    const u32 st = pl.loadout.mods[kCrescendo];
    if (!st) return 1.0f;
    const float cap = kCrescendoMax[st < 2 ? st : 2];
    return 1.0f + std::fmin(kCrescendoStep * static_cast<float>(pl.cleanBars), cap);
}

bool World::afterparty(u32 p) const {
    const u32 st = players_[p].loadout.mods[kAfterparty];
    if (!st || phrase_.section != Section::Drop) return false;
    return phrase_.sectionBar < phrase_.sectionBars / 2 + kAfterpartyBars * st;
}

u32 World::wingmen(u32 p, Vec2* pos, Vec2* dir, float* share, u32 cap) const {
    const PlayerState& pl = players_[p];
    const Loadout& l = pl.loadout;
    const Vec2 d = normalizeOr(pl.aim, dirOf(pl.heading));
    const Vec2 side = perpOf(d);
    u32 n = 0;
    const auto add = [&](Vec2 at, Vec2 to, float s) {
        if (n >= cap) return;
        pos[n] = at;
        dir[n] = to;
        share[n] = s;
        ++n;
    };
    if (const u8 lv = l.riffs[riffIndex(kMirror)]) {
        const float s = kMirrorShare[lv < 5 ? lv : 5];
        if (l.fused(kQuartet)) {
            // Quartet: three wingmen in formation, covering the sides and the back.
            add(pl.pos + side * kWingWu, side, s);
            add(pl.pos - side * kWingWu, side * -1.0f, s);
            add(pl.pos - d * kWingWu, d * -1.0f, s);
        } else {
            // Mirror: one wingman beside the ship, mirrored across the aim's axis, firing along it.
            add(pl.pos + side * kWingWu, d, s);
        }
    }
    if (pl.boost(kChorus)) {
        // Chorus: two ghost ships in formation behind, copying the shots at full strength.
        add(pl.pos - d * (0.6f * kChorusWu) + side * kChorusWu, d, 1.0f);
        add(pl.pos - d * (0.6f * kChorusWu) - side * kChorusWu, d, 1.0f);
    }
    return n;
}

float World::aegisAngle(u32 p) const {
    // One turn a bar from the run's first bar line.
    const double ph = std::fmod(m1_ - director_.startTick, kTicksPerBar) / kTicksPerBar;
    return static_cast<float>(ph) * kTau + static_cast<float>(p) * 0.25f * kTau;
}

float World::sweeperAngle(u32 p) const {
    const PlayerState& pl = players_[p];
    const u8 lv = pl.loadout.riffs[riffIndex(kSweeper)];
    double turn = lv >= 5 ? kTicksPerBar : 2.0 * kTicksPerBar;
    if (pl.boost(kHyper)) turn *= 0.5;
    // The band's rule (audio, riffs.sweeper): the arm stands at twelve o'clock on the One of every second bar of the song.
    const double song = m1_ + static_cast<double>(songShift_) * kTicksPerBar;
    const double ph = std::fmod(song, turn) / turn;
    return static_cast<float>(ph) * kTau - 0.25f * kTau;
}

bool World::cometAt(u32 p, u32 k, Vec2& at, float& radius) const {
    const PlayerState& pl = players_[p];
    if (k >= PlayerState::kComets) return false;
    const PlayerState::Comet& c = pl.comets[k];
    if (c.start < 0.0) return false;
    const float u = static_cast<float>((m1_ - c.start) / kTicksPerBeat);  // beats since the throw
    if (u < 0.0f || u >= 2.0f) return false;
    const Vec2 apex = c.from + c.dir * kCometOutWu;
    if (u < 1.0f) {
        at = c.from + c.dir * (kCometOutWu * std::sin(u * 0.5f * kPi));
    } else {
        const float b = 0.5f - 0.5f * std::cos((u - 1.0f) * kPi);  // back to the ship, easing in and out
        at = apex + (pl.pos - apex) * b;
    }
    radius = kCometRadiusWu * c.size;
    return true;
}

Vec2 World::carouselBomb(u32 p, u32 k) const {
    const PlayerState& pl = players_[p];
    const double ph = std::fmod(m1_ - director_.startTick, kTicksPerBar) / kTicksPerBar;
    const float a = static_cast<float>(ph) * kTau + kTau * static_cast<float>(k) / 6.0f;
    return pl.pos + dirOf(a) * kCarouselWu;
}

Vec2 World::chaseTarget(u32 p) const {
    // Afterimage: the decoy draws the enemies chasing this player for its beats.
    const PlayerState& pl = players_[p];
    return pl.decoyUntil > m1_ ? pl.decoyAt : pl.pos;
}

// ---------------------------------------------------------------------------------------------- helpers

void World::emitFx(u32 p, u8 fx, Vec2 at, Vec2 to, u8 c, float f, double tick) {
    if (fxThisTick_ >= kFxPerTick) return;
    ++fxThisTick_;
    Event ev;
    ev.type = EventType::Upgrade;
    ev.a = static_cast<u8>(p);
    ev.b = fx;
    ev.c = c;
    ev.x = at.x;
    ev.y = at.y;
    ev.x2 = to.x;
    ev.y2 = to.y;
    ev.f = f;
    ev.tick = tick;
    emit(ev);
}

void World::addLance(u32 p, Vec2 from, Vec2 dir, float len, float half, u8 kind, bool standing, double tick) {
    PlayerState& pl = players_[p];
    PlayerState::Lance& l = pl.lances[pl.lanceNext];
    pl.lanceNext = static_cast<u8>((pl.lanceNext + 1u) % PlayerState::kLances);
    l.from = from;
    l.dir = dir;
    l.tick = tick;
    l.length = len;
    l.half = half;
    l.kind = kind;
    l.standing = standing;
}

i32 World::nearestEnemy(Vec2 at, float r, i32 skip) const {
    const Enemies& e = enemies_;
    const float reach = r + kQuerySlackWu;
    i32 best = -1;
    float bestD = r * r;
    grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, [&](u32 i) {
        if (static_cast<i32>(i) == skip || (e.flags[i] & kFlagDead) || e.state[i] == kWarping) return;
        const float d2 = lengthSq(Vec2{e.x[i], e.y[i]} - at);
        if (d2 < bestD || (d2 == bestD && static_cast<i32>(i) < best)) {
            bestD = d2;
            best = static_cast<i32>(i);
        }
    });
    return best;
}

u32 World::nearestEnemies(Vec2 at, float r, u32* out, u32 cap) const {
    // The `cap` nearest solid enemies within r, nearest first (ties by index).
    const Enemies& e = enemies_;
    const float reach = r + kQuerySlackWu;
    float dist[32];
    u32 n = 0;
    if (cap > 32) cap = 32;
    grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, [&](u32 i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) return;
        const float d2 = lengthSq(Vec2{e.x[i], e.y[i]} - at);
        if (d2 > r * r) return;
        u32 k = n < cap ? n++ : cap;
        if (k == cap) {
            if (d2 > dist[cap - 1] || (d2 == dist[cap - 1] && i > out[cap - 1])) return;
            k = cap - 1;
        }
        while (k > 0 && (dist[k - 1] > d2 || (dist[k - 1] == d2 && out[k - 1] > i))) {
            dist[k] = dist[k - 1];
            out[k] = out[k - 1];
            --k;
        }
        dist[k] = d2;
        out[k] = i;
    });
    return n;
}

void World::novaKill(u32 i, u32 p) {
    // As the Nova's front kills (updateNovas): a client claims it, a body still warping in dissolves.
    Enemies& e = enemies_;
    if (e.flags[i] & kFlagDead) return;
    if (role_ == NetRole::Client) {
        const bool warping = e.state[i] == kWarping;
        logClaim(i, 0.0f, warping ? kClaimDissolve : kClaimKill);
        predictKill(i, static_cast<u8>(p), warping);
    } else if (e.state[i] == kWarping) {
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
    } else {
        killEnemy(i, static_cast<u8>(p));
    }
}

void World::areaHit(u8 owner, Vec2 at, float r, float dmg, i32 skip, bool boss) {
    // Every solid enemy within r of `at` (and the boss, once) takes dmg.
    Enemies& e = enemies_;
    const float reach = r + kQuerySlackWu;
    grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, [&](u32 i) {
        if (static_cast<i32>(i) == skip || (e.flags[i] & kFlagDead)) return;
        const Vec2 d = Vec2{e.x[i], e.y[i]} - at;
        const float rr = r + e.radius[i];
        if (lengthSq(d) > rr * rr) return;
        hitEnemy(i, dmg, owner, Vec2{e.x[i], e.y[i]}, normalizeOr(d, Vec2{1.0f, 0.0f}));
    });
    if (boss && boss_.active && !boss_.dying) {
        u8 part = 0, pair = 0;
        float t = 0.0f;
        if (bossHitTest(at, at, r, part, pair, t)) damageBoss(part, pair, dmg, owner, at, normalizeOr(boss_.pos - at, Vec2{1.0f, 0.0f}));
    }
}

u32 World::lineHit(u8 owner, Vec2 from, Vec2 dir, float len, float half, float dmg) {
    // Every solid enemy whose body the line from `from` along `dir` (len long, half wide) touches, and the boss once.
    Enemies& e = enemies_;
    const Vec2 to = from + dir * len;
    const float reach = half + kQuerySlackWu;
    u32 n = 0;
    grid_.query(std::fmin(from.x, to.x) - reach, std::fmin(from.y, to.y) - reach, std::fmax(from.x, to.x) + reach,
                std::fmax(from.y, to.y) + reach, [&](u32 i) {
                    if (e.flags[i] & kFlagDead) return;
                    float t = 0.0f;
                    const Vec2 c{e.x[i], e.y[i]};
                    if (segmentDist(c, from, dir, len, t) > half + e.radius[i]) return;
                    hitEnemy(i, dmg, owner, from + dir * t, dir);
                    ++n;
                });
    if (boss_.active && !boss_.dying) {
        u8 part = 0, pair = 0;
        float t = 0.0f;
        if (bossHitTest(from, to, half, part, pair, t)) damageBoss(part, pair, dmg, owner, from + (to - from) * t, dir);
    }
    return n;
}

void World::arcFrom(u32 i, u8 owner, u32 jumps, float dmg, u8 style) {
    // Lightning from enemy i to the nearest other within reach, and on from there (each enemy struck once).
    Enemies& e = enemies_;
    u32 struck[8];
    u32 ns = 0;
    struck[ns++] = i;
    Vec2 at{e.x[i], e.y[i]};
    for (u32 j = 1; j <= jumps && ns < 8; ++j) {
        const float reach = kArcReachWu + kQuerySlackWu;
        i32 best = -1;
        float bestD = kArcReachWu * kArcReachWu;
        grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, [&](u32 k) {
            if ((e.flags[k] & kFlagDead) || e.state[k] == kWarping) return;
            for (u32 s = 0; s < ns; ++s)
                if (struck[s] == k) return;
            const float d2 = lengthSq(Vec2{e.x[k], e.y[k]} - at);
            if (d2 < bestD || (d2 == bestD && static_cast<i32>(k) < best)) {
                bestD = d2;
                best = static_cast<i32>(k);
            }
        });
        if (best < 0) break;
        const Vec2 next{e.x[best], e.y[best]};
        struck[ns++] = static_cast<u32>(best);
        emitFx(owner, kFxArc, at, next, static_cast<u8>(j), static_cast<float>(style), m1_);
        // (play test 3: an arc's own kill arcs no further; nor does it show a shot's lane)
        const u8 wasArcing = arcing_, wasFlags = hitShotFlags_;
        arcing_ = 1;
        hitShotFlags_ = 0;
        hitEnemy(static_cast<u32>(best), dmg, owner, next, normalizeOr(next - at, Vec2{1.0f, 0.0f}));
        arcing_ = wasArcing;
        hitShotFlags_ = wasFlags;
        at = next;
    }
}

void World::pullBodies(Vec2 at, float r, float speed, float dt) {
    // Wells, Black Hole, Collapse Field: bodies within r move toward `at` (not past it); on every device.
    Enemies& e = enemies_;
    const float reach = r + kQuerySlackWu;
    grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, [&](u32 i) {
        if ((e.flags[i] & kFlagDead) || e.frozenUntil[i] > m1_) return;
        const Vec2 d = at - Vec2{e.x[i], e.y[i]};
        const float l2 = lengthSq(d);
        if (l2 > r * r || l2 < 1.0f) return;
        const float l = std::sqrt(l2);
        const float step = std::fmin(speed * dt, l);
        e.x[i] += d.x / l * step;
        e.y[i] += d.y / l * step;
    });
}

// ---------------------------------------------------------------------------------------------- the volley

float World::shotDamage(u32 p, double stepTick, u8& flags, u8& pierceAdd) const {
    // species.json note: one player shot deals 1 damage. Play test 1 (GAME_DESIGN 6.2): Overcharge +50% a stack; On-Beat
    // Crit: shots on quarter-note beats x3, x5, x8 (bursting over 80 wu). M5: Syncopation, off-beat shots (the "and" of
    // each beat) +50% (+100% at two) and +2 pierce; Crescendo, +5% a clean bar up to its cap.
    const Loadout& l = players_[p].loadout;
    float damage = 1.0f + kOverchargePerStack * static_cast<float>(l.mods[kOvercharge]);
    if (l.mods[kOnBeatCrit] && onBeat(stepTick)) {
        damage *= onBeatCritMultiplier(l.mods[kOnBeatCrit]);
        flags |= kShotCrit;
    }
    if (const u8 st = l.mods[kSyncopation]; st && offBeat(stepTick)) {
        damage *= kSyncopationDamage[st < 2 ? st : 2];
        pierceAdd = static_cast<u8>(pierceAdd + kSyncopationPierce);
    }
    return damage * crescendo(p);
}

bool World::shotRoom(u32 p) const { return shotsLive_[p < kMaxPlayers ? p : 0] < kShotsSoftCap; }

void World::updateThinning() {
    // GAME_DESIGN 6.6: thinning starts above the soft cap of a player's live shots and ends below three quarters of it, and
    // starts or ends only on a beat.
    forCrossings(m0_, m1_, kTicksPerBeat, [&](double, i64) {
        for (u32 p = 0; p < kMaxPlayers; ++p) {
            if (!thinning_[p] && shotsLive_[p] > kShotsSoftCap) thinning_[p] = true;
            else if (thinning_[p] && shotsLive_[p] < kShotsThinEnd) thinning_[p] = false;
        }
    });
}

void World::tagShot(u32 p, u8 extraMods, i32 laneOff, u32 lanes) {
    // What the owner's Mods and Boosts give a primary shot in flight (lastShot_: the one just spawned). Wave: every lane on
    // the same curve about its own direction; Helix Array: the lanes ride strands in mirrored pairs, the outer pair at the
    // full amplitude and the pair inside it at 2/3 (80 and 53 wu with Split's five lanes since play test 3; a third pair would ride
    // at 1/3), the middle lane straight (GAME_DESIGN 6.2, 6.5).
    if (lastShot_ < 0) return;
    const u32 i = static_cast<u32>(lastShot_);
    Shots& s = shots_;
    const PlayerState& pl = players_[p];
    const Loadout& l = pl.loadout;
    u8 m = extraMods;
    if (l.mods[kSeeker]) m |= kShotSeek;
    if (l.mods[kWave]) {
        s.radius[i] *= kWaveRadius[l.mods[kWave] < 2 ? l.mods[kWave] : 2];
        if (l.fused(kHelixArray)) {
            const i32 pairs = static_cast<i32>(lanes / 2u);
            const i32 k = laneOff < 0 ? -laneOff : laneOff;
            if (k > 0 && pairs > 0) {
                const i32 level = pairs - k;  // 0 the outer pair, 1 inside it, 2 the next
                m |= kShotWave;
                m |= level == 0 ? kShotHelix : (level == 1 ? static_cast<u8>(kShotHelix | kShotHelixIn) : kShotHelixIn);
                if (laneOff < 0) m |= kShotWaveNeg;
            }
        } else {
            m |= kShotWave;
        }
    }
    s.mods[i] = m;
    if (m & kShotWave) {
        // (it starts on its curve: advanced along it as far as along the line)
        const Vec2 o = waveAt(i, s.life0[i] - s.life[i]);
        s.x[i] += o.x;
        s.y[i] += o.y;
    }
    if (pl.boost(kSupernova)) m |= kShotBlast;
    if (pl.boost(kRicochetFever)) {
        m |= kShotFever;
        s.bounces[i] = static_cast<u8>(s.bounces[i] + kFeverBounces);
    }
    u32 forks = l.mods[kFork];
    if (pl.boost(kFractal)) forks = forks + 2u > 3u ? 3u : forks + 2u;
    s.forks[i] = static_cast<u8>(forks);
    s.mods[i] = m;
}

u32 World::volley(u32 p, Vec2 origin, float base, float advanceS, float damage, u8 flags, u8 pierce, u8 bounces, u32 lanes,
                  bool prism, u32 thin, i64 /*step*/, u8 middle, u8 extraMods) {
    // Split: +2 lanes a stack in a tight fan (3, 5; play test 3); Prism: every lane refracts into five. Thinned (GAME_DESIGN 6.6),
    // the lanes beyond the middle three fly half as far; above kShotsMiddleOnly the middle lane alone fires (play test 1). M5: `middle` 1 leaves the middle lane out (Accent's
    // lance took its place), 2 makes it Heavy Step's orb.
    const u32 fan = prism ? 5u : 1u;
    const i32 mid = static_cast<i32>(lanes / 2);
    u32 made = 0;
    for (u32 lane = 0; lane < lanes; ++lane) {
        const i32 off = static_cast<i32>(lane) - mid;
        const i32 ring = off < 0 ? -off : off;  // 0 the middle, 1 its neighbours, 2 the outer pair
        if (thin == 2 && ring >= 1) continue;
        if (ring == 0 && middle == 1) continue;
        // GAME_DESIGN 6.6: thinned, the lanes beyond the middle three fly half as far (every lane still fires). Helix Array
        // (6.5): the lanes stop fanning and ride their strands along the aim.
        const float life = thin == 1 && ring >= 2 ? kShotLifeS * kThinReach : kShotLifeS;
        const bool helix = players_[p].loadout.fused(kHelixArray) && players_[p].loadout.mods[kWave];
        const float la = base + (helix ? 0.0f : static_cast<float>(off) * kSplitFanDeg * kDeg);
        if (ring == 0 && middle == 2) {
            // Heavy Step: a slow orb that explodes (on its hit, or where its life ends).
            spawnShotRaw(p, origin, dirOf(la), advanceS * kHeavyOrbSpeed, damage, kShotOrb, flags, kHeavyOrbLifeS, 0, 0);
            if (lastShot_ >= 0) {
                const u32 i = static_cast<u32>(lastShot_);
                shots_.speed[i] = kShotSpeed * kHeavyOrbSpeed;
                shots_.radius[i] *= kHeavyOrbRadius;
                shots_.mods[i] = kShotBlast;
            }
            ++made;
            continue;
        }
        for (u32 f = 0; f < fan && made < kMaxShotsPerStep; ++f, ++made) {
            const float a = la + (static_cast<float>(f) - 0.5f * static_cast<float>(fan - 1)) * kPrismFanDeg * kDeg;
            const u8 laneFlag = prism ? shotLaneFlags(static_cast<i32>(f) - 2) : 0;
            spawnShotRaw(p, origin, dirOf(a), advanceS, damage, kShotPrimary, static_cast<u8>(flags | laneFlag), life, pierce, bounces);
            tagShot(p, extraMods, off, lanes);
        }
    }
    return made;
}

bool World::accentLance(u32 p, double stepTick, Vec2 muzzle, Vec2 d, float damage) {
    // Accent: the shot on beat 1 (then 1 and 3, then every beat) is a full-length piercing lance; Standing Wave keeps it
    // lethal for a bar.
    const PlayerState& pl = players_[p];
    const u8 st = pl.loadout.mods[kAccent];
    if (!st || !onBeat(stepTick)) return false;
    const i32 b = beatInBar(stepTick);
    if (!(st >= 3 || b == 0 || (st == 2 && b == 2))) return false;
    if (isLocal(p)) lineHit(static_cast<u8>(p), muzzle, d, kAccentLengthWu, kAccentHalfWu, damage * kAccentDamage);
    addLance(p, muzzle, d, kAccentLengthWu, kAccentHalfWu, kLanceAccent, pl.loadout.fused(kStandingWave), stepTick);
    emitFx(p, kFxLance, muzzle, muzzle + d * kAccentLengthWu, kLanceAccent, kAccentHalfWu, stepTick);
    return true;
}

void World::volleyExtras(u32 p, float advanceS, double stepTick, float base, float damage, u8 flags, u8 pierce, u8 bounces,
                         u32 lanes, u32 thin, i64 step) {
    // What a volley brings beside the ship's own shots: Echo's ghost volleys a beat later, Rear Guard's off-beat shots,
    // Mirror's (or Quartet's) wingmen and Chorus's ghosts. Effects that multiply shots carry their own cap (GAME_DESIGN
    // 6.6): none of these fire above the soft cap of the player's live shots.
    PlayerState& pl = players_[p];
    const Loadout& l = pl.loadout;
    if (!shotRoom(p)) return;  // (GAME_DESIGN 6.6: the extras wait while the player is over the soft cap)
    const u8 plain = static_cast<u8>(flags & kShotCrit);
    if (const u8 st = l.mods[kEcho]) {
        for (u32 k = 1; k <= st; ++k) {
            for (PlayerState::Echo& ec : pl.echoes) {
                if (ec.tick >= 0.0) continue;
                ec.tick = stepTick + static_cast<double>(k) * kTicksPerBeat;
                ec.angle = base;
                ec.damage = damage * kEchoDamage;
                ec.lanes = static_cast<u8>(lanes);
                break;
            }
        }
    }
    const Vec2 d = dirOf(base);
    if (const u8 st = l.mods[kRearGuard]; st && offBeat(stepTick)) {
        spawnShotRaw(p, pl.pos - d * kMuzzleWu, d * -1.0f, advanceS, damage, kShotPrimary, plain, kShotLifeS, pierce, bounces);
        tagShot(p, 0, 0, 1);
        if (st >= 2) {
            const Vec2 side = perpOf(d);
            spawnShotRaw(p, pl.pos + side * kMuzzleWu, side, advanceS, damage, kShotPrimary, plain, kShotLifeS, pierce, bounces);
            tagShot(p, 0, 0, 1);
            spawnShotRaw(p, pl.pos - side * kMuzzleWu, side * -1.0f, advanceS, damage, kShotPrimary, plain, kShotLifeS, pierce, bounces);
            tagShot(p, 0, 0, 1);
        }
    }
    Vec2 wp[5], wd[5];
    float ws[5];
    const u32 n = wingmen(p, wp, wd, ws, 5);
    for (u32 k = 0; k < n; ++k)
        volley(p, wp[k] + wd[k] * kMuzzleWu, angleOf(wd[k]), advanceS, damage * ws[k], plain, pierce, bounces, lanes, false, thin, step, 0, 0);
}

void World::beamVolley(u32 p, Vec2 muzzle, Vec2 d, float damage, u32 lanes, double stepTick) {
    // Beam: the primary is a continuous laser along the aim; each grid step it burns what it lies on, as wide and as hard
    // as the volley's lanes together (look.md v2.5: 20 wu with one lane; part 3 drew and burned 28).
    PlayerState& pl = players_[p];
    const float half = kBeamHalfWu + 4.0f * static_cast<float>(lanes > 1u ? lanes - 1u : 0u);
    if (isLocal(p)) lineHit(static_cast<u8>(p), muzzle, d, kBeamLengthWu, half, damage * static_cast<float>(lanes));
    pl.beamTick = stepTick;
    pl.beamLanes = static_cast<u8>(lanes);
}

void World::fireEchoes() {
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        for (PlayerState::Echo& ec : pl.echoes) {
            if (ec.tick < 0.0 || ec.tick > m1_) continue;
            const double t = ec.tick;
            ec.tick = -1.0;
            if (!pl.alive || !shotRoom(p)) continue;
            const Vec2 d = dirOf(ec.angle);
            volley(p, pl.pos + d * kMuzzleWu, ec.angle, tickAgeOf(t), ec.damage, 0, static_cast<u8>(pierceCount(pl.loadout.mods[kPierce])),
                   static_cast<u8>(ricochetBounces(pl.loadout.mods[kRicochet])), ec.lanes, false, 0, 0, 0, kShotGhost);
            emitFx(p, kFxEcho, pl.pos, pl.pos + d, 0, 0.0f, t);
        }
    }
}

// ---------------------------------------------------------------------------------------------- shots in flight

void World::steerShots(float dt) {
    // Seeker (and Swarm's missiles, Confetti's sparks): shots curve toward the nearest enemy within reach, each looking
    // again every third tick (staggered by its index).
    Shots& s = shots_;
    const Enemies& e = enemies_;
    // M5 part 7: a fight's bodies are targets too: its parts that can be hurt (the Choir's centre, Cadence's or Polyrhythm's,
    // has nothing in it: the shots steered there never hit).
    BossPart parts[kBossBodies + 4];
    u32 partCount = 0;
    if (boss_.active && !boss_.dying && m1_ >= boss_.fightTick && boss_.kind != BossKind::Kaleidos) {
        partCount = bossParts(parts, kBossBodies + 4);
        u32 keep = 0;
        for (u32 k = 0; k < partCount; ++k)
            if (parts[k].part != 1) parts[keep++] = parts[k];
        partCount = keep;
    }
    for (u32 i = 0; i < s.count; ++i) {
        if (s.dead[i] || !(s.mods[i] & kShotSeek) || ((tick_ + i) % 3u) != 0) continue;
        const PlayerState& pl = players_[s.owner[i] < kMaxPlayers ? s.owner[i] : 0];
        float turn = kSeekerTurn[pl.loadout.mods[kSeeker] < 3 ? pl.loadout.mods[kSeeker] : 3];
        float reach = kSeekerReachWu;
        if (s.kind[i] == kShotMissile) {
            turn = pl.loadout.fused(kMurmuration) ? 9.0f : 6.0f;
            reach = pl.loadout.fused(kMurmuration) ? 2.0f * kSeekerReachWu : kSeekerReachWu;
        } else if (s.kind[i] == kShotPellet) {
            turn = 8.0f;
        }
        if (turn <= 0.0f) continue;
        const Vec2 at{s.x[i], s.y[i]};
        const i32 t = nearestEnemy(at, reach, -1);
        Vec2 goal = t >= 0 ? Vec2{e.x[t], e.y[t]} : at;
        if (boss_.active && !boss_.dying && m1_ >= boss_.fightTick) {
            // The fight's body is a target too, when it is the nearer (Kaleidos: the wheel; the others: their nearest part
            // that can be hurt).
            Vec2 bp = boss_.pos;
            float bd = lengthSq(boss_.pos - at);
            if (boss_.kind != BossKind::Kaleidos) {
                bd = 1e30f;
                for (u32 k = 0; k < partCount; ++k) {
                    const float d2 = lengthSq(parts[k].pos - at);
                    if (d2 < bd) {
                        bd = d2;
                        bp = parts[k].pos;
                    }
                }
            }
            if (bd < reach * reach * 4.0f && (t < 0 || bd < lengthSq(goal - at))) goal = bp;
        }
        if (goal.x == at.x && goal.y == at.y) continue;
        const float h = turnToward(angleOf(Vec2{s.dx[i], s.dy[i]}), angleOf(goal - at), turn * dt * 3.0f);
        const Vec2 nd = dirOf(h);
        s.dx[i] = nd.x;
        s.dy[i] = nd.y;
    }
}

Vec2 World::waveStep(u32 i, float dt) const {
    // Wave's (Helix Array's) sideways move over dt.
    const float a0 = shots_.life0[i] - shots_.life[i];
    return waveAt(i, a0 + dt) - waveAt(i, a0);
}

Vec2 World::waveAt(u32 i, float age) const {
    // Wave: a sine across the shot's line (Helix Array: its strand) at this age of the shot's flight.
    const Shots& s = shots_;
    const PlayerState& pl = players_[s.owner[i] < kMaxPlayers ? s.owner[i] : 0];
    const u8 st = pl.loadout.mods[kWave];
    // look.md v2.5 (shots that leave the straight line): a standing curve fixed to the aim line and measured from the
    // muzzle (its phase by the shot's own flight), one wave to a beat of flight (Helix Array: two beats), never sideways
    // faster than 0.6 of forward.
    float amp = kWaveAmpWu[st < 2 ? (st ? st : 1) : 2];
    float beats = 1.0f;
    const u8 m = s.mods[i];
    if (m & (kShotHelix | kShotHelixIn)) {
        amp = kHelixAmpWu * ((m & kShotHelix) ? ((m & kShotHelixIn) ? 2.0f / 3.0f : 1.0f) : 1.0f / 3.0f);
        beats = kHelixWaveBeats;
    }
    if (m & kShotWaveNeg) amp = -amp;
    const float w = kTau / (beats * 60.0f / std::fmax(bpm_, 30.0f));
    return perpOf(Vec2{s.dx[i], s.dy[i]}) * (amp * std::sin(w * age));
}

float World::blastRadius(u32 k, float& dmg) const {
    const Shots& s = shots_;
    const PlayerState& pl = players_[s.owner[k] < kMaxPlayers ? s.owner[k] : 0];
    switch (s.kind[k]) {
        case kShotOrb: {
            const u8 st = pl.loadout.mods[kHeavyStep] < 3 ? pl.loadout.mods[kHeavyStep] : 3;
            dmg = kHeavyBlastDamage[st ? st : 1];
            return kHeavyBlastWu[st ? st : 1];
        }
        case kShotBomb: dmg = kBombDamage; return kBombBlastWu;
        case kShotPellet: dmg = s.damage[k]; return kFlakBlastWu;
        default: dmg = s.damage[k] * kSupernovaShare; return kSupernovaWu;  // Supernova Rounds (play test 3: half the shot's damage)
    }
}

void World::shotBlast(u32 k, Vec2 at, i32 skip) {
    float dmg = 0.0f;
    const float r = blastRadius(k, dmg);
    const u8 owner = shots_.owner[k];
    if (isLocal(owner)) areaHit(owner, at, r, dmg, skip, true);
    const u8 c = shots_.kind[k] == kShotOrb ? 1 : (shots_.kind[k] == kShotBomb ? 3 : (shots_.kind[k] == kShotPellet ? 4 : 2));
    emitFx(owner, kFxBlast, at, at, c, r, m1_);
}

void World::shotHitExtras(u32 k, u32 i, Vec2 at, Vec2 dir) {
    // A real hit by shot k on enemy i (collide()), after its damage: Fork, the blasts, Gravity Rounds, Burn.
    Shots& s = shots_;
    const u8 owner = s.owner[k];
    if (owner >= kMaxPlayers) return;
    PlayerState& pl = players_[owner];
    const Loadout& l = pl.loadout;
    Enemies& e = enemies_;
    if (s.mods[k] & kShotBlast) shotBlast(k, at, static_cast<i32>(i));
    const bool primary = s.kind[k] == kShotPrimary || s.kind[k] == kShotMini;
    if (s.forks[k] > 0 && primary && shotRoom(owner)) {
        // Fork: on the first hit, two half-damage shots (Fractal: and they fork again).
        const u8 forks = static_cast<u8>(s.forks[k] - 1u);
        const float a = angleOf(dir);
        for (float side : {-1.0f, 1.0f}) {
            spawnShotRaw(owner, at, dirOf(a + side * kForkDeg * kDeg), 0.0f, s.damage[k] * kForkDamage, kShotPrimary,
                         static_cast<u8>(s.flags[k] & kShotLaneMask), kShotLifeS * 0.6f, 0, 0);
            if (lastShot_ >= 0) {
                shots_.forks[lastShot_] = forks;
                shots_.mods[lastShot_] = static_cast<u8>(s.mods[k] & (kShotSeek | kShotBlast | kShotFever));
                shots_.lastHit[lastShot_] = e.handle[i];
            }
        }
        s.forks[k] = 0;
        emitFx(owner, kFxFork, at, at + dir, 0, 0.0f, m1_);
    }
    if (!primary) return;
    if (l.mods[kGravityRounds]) {
        // Gravity Rounds: the hit tugs the bodies near it toward the impact (co-op: on the host, gravityTug).
        gravityTug(owner, at, i);
        emitFx(owner, kFxGravity, at, at, 0, kGravityReachWu[l.mods[kGravityRounds] < 2 ? l.mods[kGravityRounds] : 2], m1_);
    }
    if (const u8 st = l.mods[kBurn]; st && !(e.flags[i] & kFlagDead)) {
        // Burn: the hit sets it alight for two beats (its fire hurts on every beat, by the stacks).
        e.burnUntil[i] = m1_ + kBurnBeats * kTicksPerBeat;
        e.burnOwner[i] = owner;
        // (the shooter's own fire: it spreads again on a death)
        const u8 level = static_cast<u8>(e.burnLevel[i] & ~kBurnSpread);
        e.burnLevel[i] = level < st ? st : level;
    }
}

void World::gravityTug(u8 owner, Vec2 at, u32 skip, bool grid) {
    // (grid false: the grid is from the last tick, before its compaction (net ops at the tick's start): every body.)
    Enemies& e = enemies_;
    const u8 st = owner < kMaxPlayers ? players_[owner].loadout.mods[kGravityRounds] : 0;
    // Co-op: the host alone moves the bodies, at its own player's hits and the copies of the others' shots; its corrections
    // carry the tug to the clients (measured: a client predicting its own tugs, or every device tugging at every copy, put
    // the impaired soak's error at 14-15 wu RMS; the host alone at 9.5).
    if (!st || role_ == NetRole::Client) return;
    const float r = kGravityReachWu[st < 2 ? st : 2], pull = kGravityPull[st < 2 ? st : 2];
    const float reach = r + kQuerySlackWu;
    const auto tug = [&](u32 j) {
        if (j == skip || (e.flags[j] & kFlagDead) || e.frozenUntil[j] > m1_) return;
        const Vec2 d = at - Vec2{e.x[j], e.y[j]};
        const float l2 = lengthSq(d);
        if (l2 > r * r || l2 < 1.0f) return;
        const float len = std::sqrt(l2);
        e.kx[j] = clampf(e.kx[j] + d.x / len * pull, -2.0f * pull, 2.0f * pull);
        e.ky[j] = clampf(e.ky[j] + d.y / len * pull, -2.0f * pull, 2.0f * pull);
    };
    if (grid)
        grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, tug);
    else
        for (u32 j = 0; j < e.count; ++j) tug(j);
}

void World::killExtras(u32 i, u8 owner) {
    // A kill by this device's player (hitEnemy, before killEnemy): Arc, Shatter, Burn's spread, Confetti, Shard Halo.
    if (owner >= kMaxPlayers || extrasDepth_ >= 2) return;
    ++extrasDepth_;
    PlayerState& pl = players_[owner];
    const Loadout& l = pl.loadout;
    Enemies& e = enemies_;
    const Vec2 at{e.x[i], e.y[i]};
    const bool room = shotRoom(owner);
    // Play test 3 (GAME_DESIGN 6.2 rows 13-15, "half, one step"): Arc 1, 2, 3 more and an arc's kill arcs no further; a Shatter
    // piece's kill throws none; a fire caught from a death spreads no further.
    if (const u8 st = l.mods[kArc]; st && !arcing_) arcFrom(i, owner, st, kArcDamage, 0);
    if (const u8 st = l.mods[kShatter]; st && room && !(hitShotFlags_ & kShotPiece)) {
        // Shatter: the body's line segments fly out as shrapnel.
        const u32 n = kShatterShards[st < 3 ? st : 3];
        const float a0 = e.heading[i];
        for (u32 k = 0; k < n; ++k) {
            spawnShotRaw(owner, at, dirOf(a0 + kTau * static_cast<float>(k) / static_cast<float>(n)), 0.0f, 1.0f, kShotPellet, kShotPiece, kPelletLifeS, 0, 0);
            if (lastShot_ >= 0) {
                shots_.speed[lastShot_] = kPelletSpeed;
                shots_.lastHit[lastShot_] = e.handle[i];
            }
        }
        emitFx(owner, kFxShatter, at, at, static_cast<u8>(n), 0.0f, m1_);
    }
    if (e.burnUntil[i] > m1_ && !(e.burnLevel[i] & kBurnSpread)) {
        // Burn: a burning body lights its neighbours as it dies (once: theirs spreads no further).
        u32 lit = 0;
        const float reach = kBurnSpreadWu + kQuerySlackWu;
        grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, [&](u32 j) {
            if (j == i || (e.flags[j] & kFlagDead)) return;
            const float rr = kBurnSpreadWu + e.radius[j];
            if (lengthSq(Vec2{e.x[j], e.y[j]} - at) > rr * rr) return;
            const bool own = e.burnUntil[j] > m1_ && !(e.burnLevel[j] & kBurnSpread);  // (already burning by a hit: stays its own)
            e.burnUntil[j] = m1_ + kBurnBeats * kTicksPerBeat;
            e.burnOwner[j] = e.burnOwner[i];
            const u8 lj = static_cast<u8>(e.burnLevel[j] & ~kBurnSpread), li = static_cast<u8>(e.burnLevel[i] & ~kBurnSpread);
            e.burnLevel[j] = static_cast<u8>((lj < li ? li : lj) | (own ? 0u : kBurnSpread));
            ++lit;
        });
        if (lit) emitFx(owner, kFxBurnSpread, at, at, static_cast<u8>(lit < 255 ? lit : 255), 0.0f, m1_);
    }
    if (pl.boost(kConfetti) && room) {
        // Confetti: the kill bursts into six homing sparks.
        for (u32 k = 0; k < kConfettiSparks; ++k) {
            spawnShotRaw(owner, at, dirOf(kTau * static_cast<float>(k) / static_cast<float>(kConfettiSparks) + e.heading[i]), 0.0f, 1.0f,
                         kShotPellet, 0, 0.6f, 0, 0);
            if (lastShot_ >= 0) {
                shots_.speed[lastShot_] = kPelletSpeed;
                shots_.mods[lastShot_] = kShotSeek;
                shots_.lastHit[lastShot_] = e.handle[i];
            }
        }
        emitFx(owner, kFxConfetti, at, at, 6, 0.0f, m1_);
    }
    if (l.fused(kShardHalo) && room && !((flungMask_ >> owner) & 1u) && lengthSq(at - pl.pos) < 500.0f * 500.0f) {
        // Shard Halo: on a kill nearby the Orbitals fling shrapnel outward, a stream from every shard (once a tick).
        flungMask_ = static_cast<u8>(flungMask_ | (1u << owner));
        const u32 n = orbitalCount(l.riffs[riffIndex(kOrbitals)]);
        const double ph = std::fmod(m1_ - director_.startTick, kTicksPerBar) / kTicksPerBar;
        for (u32 k = 0; k < n && k < 16; ++k) {
            const float a = static_cast<float>(ph) * kTau + kTau * static_cast<float>(k) / static_cast<float>(n) - 0.25f * kTau;
            spawnShotRaw(owner, pl.pos + dirOf(a) * kOrbitalRadiusWu, dirOf(a), 0.0f, 1.0f, kShotPellet, 0, kPelletLifeS * 1.5f, 0, 0);
            if (lastShot_ >= 0) shots_.speed[lastShot_] = kPelletSpeed;
        }
        emitFx(owner, kFxShatter, pl.pos, pl.pos, static_cast<u8>(n), 1.0f, m1_);
    }
    --extrasDepth_;
}

void World::freezeShatter(u32 i, u8 owner) {
    // Freeze Frame: a frozen body dies in one hit and shatters the frozen bodies touching it, on and on (to a cap).
    Enemies& e = enemies_;
    u32 queue[kFreezeChainMax];
    u32 head = 0, tail = 0;
    queue[tail++] = i;
    while (head < tail) {
        const u32 c = queue[head++];
        const Vec2 at{e.x[c], e.y[c]};
        emitFx(owner, kFxShatterFrozen, at, at, 0, 0.0f, m1_);
        const float reach = kFreezeChainWu + kQuerySlackWu;
        grid_.query(at.x - reach, at.y - reach, at.x + reach, at.y + reach, [&](u32 j) {
            if (tail >= kFreezeChainMax || (e.flags[j] & kFlagDead) || e.frozenUntil[j] <= m1_) return;
            const float rr = kFreezeChainWu + e.radius[j];
            if (lengthSq(Vec2{e.x[j], e.y[j]} - at) > rr * rr) return;
            e.frozenUntil[j] = -1.0;  // shattering: not again
            hitEnemy(j, e.hp[j] + 1.0f, owner, Vec2{e.x[j], e.y[j]}, normalizeOr(Vec2{e.x[j], e.y[j]} - at, Vec2{1.0f, 0.0f}));
            queue[tail++] = j;
        });
    }
}

// ---------------------------------------------------------------------------------------------- the player

void World::dashExtras(u32 p, Vec2 from, Vec2 to, bool perfect) {
    // Afterimage: a decoy where the Dash began; Shockstep: a damaging ring where it ends (a Perfect Dash doubles it).
    PlayerState& pl = players_[p];
    const Loadout& l = pl.loadout;
    if (const u8 st = l.mods[kAfterimage]) {
        pl.decoyAt = from;
        pl.decoyUntil = m1_ + kDecoyBeats[st < 2 ? st : 2] * kTicksPerBeat;
        emitFx(p, kFxDecoy, from, from, st, 0.0f, pl.decoyUntil);
    }
    if (const u8 st = l.mods[kShockstep]) {
        // (its damage waits for this tick's grid: updateUpgradesM5)
        const float r = kShockRingWu[st < 3 ? st : 3];
        pl.shockDamage = isLocal(p) ? kShockDamage[st < 3 ? st : 3] * (perfect ? 2.0f : 1.0f) : 0.0f;
        pl.shockAt = to;
        pl.shockTick = m1_;
        pl.shockR = r;
        emitFx(p, kFxShockstep, to, to, perfect ? 1 : 0, r, m1_);
    }
}

void World::hitExtras(u32 p) {
    // The player lost a cell: Crescendo starts again; Last Light fires its free mini-Nova (once a cycle).
    PlayerState& pl = players_[p];
    pl.cleanBars = 0;
    if (!pl.loadout.mods[kLastLight] || !pl.alive) return;
    const u32 cycle = (static_cast<u32>(phrase_.track) << 8) | phrase_.cycle;
    if (pl.lastLightCycle == cycle) return;
    pl.lastLightCycle = cycle;
    const Vec2 c = pl.pos;
    const float r = kLastLightWu, r2 = r * r;
    for (u32 b = 0; b < bullets_.count; ++b)
        if (lengthSq(Vec2{bullets_.x[b], bullets_.y[b]} - c) <= r2) bullets_.dead[b] = 1;
    if (isLocal(p)) {
        Enemies& e = enemies_;
        for (u32 i = 0; i < e.count; ++i)
            if (!(e.flags[i] & kFlagDead) && lengthSq(Vec2{e.x[i], e.y[i]} - c) <= r2) novaKill(i, p);
    }
    emitFx(p, kFxLastLight, c, c, 0, r, m1_);
}

bool World::blackHoleNova(u32 p, double blowTick, const Vec2* at) {
    // Black Hole: the Nova pressed while it is on becomes a singularity that pulls for three beats, then blows on a beat.
    PlayerState& pl = players_[p];
    ActiveBoost* bh = nullptr;
    for (ActiveBoost& a : pl.boosts)
        if (a.type == kBlackHole) bh = &a;
    if (!bh) return false;
    bh->type = kNoBoost;
    Event end;
    end.type = EventType::BoostEnd;
    end.a = static_cast<u8>(p);
    end.b = kBlackHole;
    emit(end);
    // (a teammate's Nova here: the owner's beat, lowTick)
    pl.novaBlastTick = blowTick >= 0.0 ? blowTick : std::ceil((m1_ + kHoleBeats * kTicksPerBeat) / kTicksPerBeat) * kTicksPerBeat;
    pl.holeAt = at ? *at : pl.pos;  // a teammate's: where its owner pressed it
    pl.holeFrom = m1_;
    pl.holeBlow = pl.novaBlastTick;
    emitFx(p, kFxHole, pl.pos, pl.pos, 0, kHolePullWu, pl.holeBlow);
    return true;
}

void World::freezeView(u32 p, double until) {
    // Freeze Frame: every body in the player's frame stops where it is until `until`.
    // (co-op: the host decides and its corrections carry it, world_net.cpp; a client only shows the moment)
    const PlayerState& pl = players_[p];
    if (role_ == NetRole::Client) {
        emitFx(p, kFxFreeze, pl.pos, pl.pos, 0, 0.0f, until);
        return;
    }
    Enemies& e = enemies_;
    u32 n = 0;
    for (u32 i = 0; i < e.count; ++i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
        if (std::fabs(e.x[i] - pl.pos.x) > kFrameHalfWWu || std::fabs(e.y[i] - pl.pos.y) > kFrameHalfHWu) continue;
        e.frozenUntil[i] = until;
        e.vx[i] = e.vy[i] = e.kx[i] = e.ky[i] = 0.0f;
        ++n;
    }
    emitFx(p, kFxFreeze, pl.pos, pl.pos, static_cast<u8>(n < 255 ? n : 255), 0.0f, until);
}

void World::collectorShard(u32 p) {
    // Collector: every 250 Shards (150 at two stacks) spawn a Boost.
    PlayerState& pl = players_[p];
    const u8 st = pl.loadout.mods[kCollector];
    if (!st) return;
    if (++pl.collectorShards < kCollectorEvery[st < 2 ? st : 2]) return;
    pl.collectorShards = 0;
    const u8 b = static_cast<u8>(upgradeRng_.below(kBoostCount));
    activateBoost(p, b);
    emitFx(p, kFxCollector, pl.pos, pl.pos, b, 0.0f, m1_);
}

void World::aegisBullets() {
    // Aegis: a rotating arc 30-60 wu round the ship stops the enemy bullets that reach it (on every device, like the
    // Orbitals' shards).
    Bullets& b = bullets_;
    for (u32 p = 0; p < playerCount_; ++p) {
        const PlayerState& pl = players_[p];
        const u8 st = pl.loadout.mods[kAegis];
        if (!st || !pl.alive) continue;
        const float half = 0.5f * kAegisArcDeg[st < 3 ? st : 3] * kDeg;
        const float centre = aegisAngle(p);
        const float rIn = kAegisInWu, rOut = kAegisOutWu + species_->bulletRadius;
        u32 stopped = 0;
        for (u32 i = 0; i < b.count; ++i) {
            if (b.dead[i]) continue;
            const float dx = b.x[i] - pl.pos.x, dy = b.y[i] - pl.pos.y;
            if (std::fabs(dx) > rOut || std::fabs(dy) > rOut) continue;
            const float d2 = dx * dx + dy * dy;
            if (d2 > rOut * rOut || d2 < rIn * rIn) continue;
            if (std::fabs(wrapAngle(std::atan2(dy, dx) - centre)) > half) continue;
            b.dead[i] = 1;
            if (stopped++ < 4) emitFx(p, kFxAegis, Vec2{b.x[i], b.y[i]}, pl.pos, 0, 0.0f, m1_);
        }
    }
}

// ---------------------------------------------------------------------------------------------- every tick

void World::updateUpgradesM5(float dt) {
    // Time Stretch (GAME_DESIGN 6.4): the world at half speed while any player has it (decision: the world is one in
    // co-op, so everyone's enemies slow).
    Enemies& e = enemies_;
    const bool breakNow = phrase_.section == Section::Break;
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        const Loadout& l = pl.loadout;
        const u8 owner = static_cast<u8>(p);
        const bool local = isLocal(p);
        if (!pl.present) continue;
        if (pl.shockDamage > 0.0f) {
            // Shockstep's ring, as the Dash began this tick.
            areaHit(owner, pl.shockAt, pl.shockR, pl.shockDamage, -1, true);
            pl.shockDamage = 0.0f;
        }

        // Bar lines: Crescendo counts a clean bar; Bar Line's cross of lasers through the ship.
        forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
            if (pl.alive && pl.cleanBars < 0xFFFF) ++pl.cleanBars;
            const u8 st = l.mods[kBarLine];
            if (!st || !pl.alive) return;
            const u32 rays = st >= 2 ? 8u : 4u;
            const float len = kBarLineWu[st < 3 ? st : 3];
            for (u32 k = 0; k < rays; ++k) {
                const Vec2 d = dirOf(static_cast<float>(k) * kTau / static_cast<float>(rays));
                if (local) lineHit(owner, pl.pos, d, len, kBarLineHalfWu, kBarLineDamage);
                addLance(p, pl.pos, d, len, kBarLineHalfWu, kLanceBarLine, false, t);
            }
            emitFx(p, kFxBarLine, pl.pos, pl.pos, static_cast<u8>(rays), len, t);
        });

        if (!pl.alive) {
            for (PlayerState::Echo& ec : pl.echoes) ec.tick = -1.0;
            continue;
        }

        // Kick Pulse (the lead's ruling after part 3): a ring on every kick of the Track's pattern outside the Break, its push
        // and damage scaled by 4 / (kicks a bar) so the Mod is worth the same in every Track.
        if (const u8 st = l.mods[kKickPulse]; st && !breakNow) {
            const u16 mask = kickMask(phrase_.song);
            u32 kicks = 0;
            for (u32 b = 0; b < 16; ++b) kicks += (mask >> b) & 1u;
            const float scale = kicks ? 4.0f / static_cast<float>(kicks) : 1.0f;
            forCrossings(m0_, m1_, kTicksPerStep, [&](double t, i64 k) {
                if (!((mask >> static_cast<u32>(((k % 16) + 16) % 16)) & 1u)) return;
                const float r = kKickRingWu[st < 3 ? st : 3];
                const float reach = r + kQuerySlackWu;
                grid_.query(pl.pos.x - reach, pl.pos.y - reach, pl.pos.x + reach, pl.pos.y + reach, [&](u32 i) {
                    if (e.flags[i] & kFlagDead) return;
                    const Vec2 d = Vec2{e.x[i], e.y[i]} - pl.pos;
                    const float rr = r + e.radius[i];
                    const float l2 = lengthSq(d);
                    if (l2 > rr * rr || l2 < 1e-4f) return;
                    const float len = std::sqrt(l2);
                    if (e.frozenUntil[i] <= m1_) {
                        e.kx[i] += d.x / len * kKickPush * scale;
                        e.ky[i] += d.y / len * kKickPush * scale;
                    }
                    if (st >= 2 && local) hitEnemy(i, kKickDamage * scale, owner, Vec2{e.x[i], e.y[i]}, d * (1.0f / len));
                });
                pl.kickTick = t;
                pl.kickR = r;
                emitFx(p, kFxKick, pl.pos, pl.pos, st, r, t);
            });
        }

        // Beats: Chain Storm, Carousel, Burn.
        forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
            if (pl.boost(kChainStorm)) {
                // Lightning links the enemies near the ship, the nearest first, ship to body to body.
                u32 near[kChainStormMax];
                const u32 n = nearestEnemies(pl.pos, kChainStormWu, near, kChainStormMax);
                Vec2 from = pl.pos;
                for (u32 k = 0; k < n; ++k) {
                    const u32 i = near[k];
                    const Vec2 at{e.x[i], e.y[i]};
                    emitFx(p, kFxArc, from, at, static_cast<u8>(k + 1), 2.0f, t);
                    if (local) hitEnemy(i, kChainStormDamage, owner, at, normalizeOr(at - from, Vec2{1.0f, 0.0f}));
                    from = at;
                }
            }
            if (pl.boost(kCarousel) && shotRoom(p)) {
                const u32 k = static_cast<u32>(((beat % 6) + 6) % 6);
                const Vec2 at = carouselBomb(p, k);
                const Vec2 d = normalizeOr(at - pl.pos, Vec2{1.0f, 0.0f});
                spawnShotRaw(p, at, d, tickAgeOf(t) * (kBombSpeed / kShotSpeed), 1.0f, kShotBomb, 0, kBombLifeS, 0, 0);
                if (lastShot_ >= 0) {
                    shots_.speed[lastShot_] = kBombSpeed;
                    shots_.radius[lastShot_] = 10.0f;
                    shots_.mods[lastShot_] = kShotBlast;
                }
                emitFx(p, kFxCarousel, at, at + d, static_cast<u8>(k), 0.0f, t);
            }
            // Burn: every burning body this player lit takes its fire's damage on the beat.
            if (local && l.mods[kBurn]) {
                for (u32 i = 0; i < e.count; ++i) {
                    if ((e.flags[i] & kFlagDead) || e.burnOwner[i] != owner || e.burnUntil[i] < t) continue;
                    hitEnemy(i, kBurnDamage * static_cast<float>(e.burnLevel[i] & ~kBurnSpread), owner, Vec2{e.x[i], e.y[i]}, Vec2{0.0f, -1.0f});
                }
            }
        });

        // Standing Wave: Accent's lances stay lethal for a bar, burning what lies on them every eighth.
        if (l.fused(kStandingWave) && local)
            forCrossings(m0_, m1_, 0.5 * kTicksPerBeat, [&](double t, i64) {
                for (const PlayerState::Lance& ln : pl.lances)
                    if (ln.standing && t > ln.tick && t - ln.tick <= kTicksPerBar)
                        lineHit(owner, ln.from, ln.dir, ln.length, ln.half, kStandingDamage);
            });

        // Colossus: the giant ship rams through what it touches.
        if (pl.boost(kColossus)) {
            const float r = kPlayerVisualRadius * kColossusScale;
            const float reach = r + kQuerySlackWu;
            bool rammed = false;
            grid_.query(pl.pos.x - reach, pl.pos.y - reach, pl.pos.x + reach, pl.pos.y + reach, [&](u32 i) {
                if (e.flags[i] & kFlagDead) return;
                const Vec2 d = Vec2{e.x[i], e.y[i]} - pl.pos;
                const float rr = r + e.radius[i];
                if (lengthSq(d) > rr * rr) return;
                const Vec2 n = normalizeOr(d, dirOf(pl.heading));
                e.kx[i] += n.x * kPanicShoveWu * dt * 6.0f;
                e.ky[i] += n.y * kPanicShoveWu * dt * 6.0f;
                if (local) hitEnemy(i, kColossusRamDamage, owner, Vec2{e.x[i], e.y[i]}, n);
                rammed = true;
            });
            if (rammed) emitFx(p, kFxRam, pl.pos, pl.pos, 0, r, m1_);
        }

        // Black Hole: the singularity pulls the screen in, then blows on its beat.
        if (pl.holeFrom >= 0.0) {
            if (m1_ < pl.holeBlow) {
                pullBodies(pl.holeAt, kHolePullWu, kHolePull, dt);
            } else {
                if (local) areaHit(owner, pl.holeAt, kHoleBlastWu, kHoleDamage, -1, true);
                emitFx(p, kFxHoleBlow, pl.holeAt, pl.holeAt, 0, kHoleBlastWu, pl.holeBlow);
                pl.holeFrom = pl.holeBlow = -1.0;
            }
        }
    }
}

void World::updateRiffsM5(float dt) {
    // Riffs 4-12 (GAME_DESIGN 6.3), by level 1-5. Hyper (6.4) runs them at double grid where a grid is theirs (Lancer,
    // Storm Coil, Flak, Sweeper's turn).
    Enemies& e = enemies_;
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.present) continue;
        const Loadout& l = pl.loadout;
        const u8 owner = static_cast<u8>(p);
        const bool local = isLocal(p);
        const bool hyper = pl.boost(kHyper);
        const Vec2 d = normalizeOr(pl.aim, dirOf(pl.heading));
        const auto lv = [&](u8 riff) { return l.riffs[riffIndex(riff)]; };

        // Minelayer's mines blow on the next bar line even with the ship down; Collapse Field pulls first.
        forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
            for (PlayerState::Mine& m : pl.mines) {
                if (m.blowTick < 0.0 || m.blowTick > t) continue;
                if (local) areaHit(owner, m.at, kMineBlastWu, kMineDamage, -1, true);
                emitFx(p, kFxMineBlow, m.at, m.at, 0, kMineBlastWu, t);
                m.blowTick = -1.0;
            }
        });
        if (l.fused(kCollapseField))
            for (const PlayerState::Mine& m : pl.mines)
                if (m.blowTick >= 0.0) pullBodies(m.at, kCollapsePullWu, kCollapsePull, dt);
        if (!pl.alive) continue;

        // Lancer: charges three beats, fires a full-length beam along the aim on beat 4 (beats 2 and 4 from level 3; every
        // beat under Hyper). Downbeat Cannon: a screen-wide critical beam every beat 1.
        if (const u8 L = lv(kLancer)) {
            forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
                const i32 b = static_cast<i32>(((beat % 4) + 4) % 4);
                const bool fire = hyper || b == 3 || (L >= 3 && b == 1);
                if (fire) {
                    const float half = kLancerHalfWu[L < 5 ? L : 5];
                    const float dmg = kLancerDamage * (L >= 5 ? 1.5f : 1.0f);
                    if (local) lineHit(owner, pl.pos, d, kLancerLengthWu, half, dmg);
                    addLance(p, pl.pos, d, kLancerLengthWu, half, kLanceLancer, false, t);
                    emitFx(p, kFxLance, pl.pos, pl.pos + d * kLancerLengthWu, kLanceLancer, half, t);
                }
                if (b == 0 && l.fused(kDownbeatCannon)) {
                    const float dmg = kLancerDamage * onBeatCritMultiplier(3);
                    if (local) lineHit(owner, pl.pos, d, kDownbeatLengthWu, kDownbeatHalfWu, dmg);
                    addLance(p, pl.pos, d, kDownbeatLengthWu, kDownbeatHalfWu, kLanceCannon, false, t);
                    emitFx(p, kFxLance, pl.pos, pl.pos + d * kDownbeatLengthWu, kLanceCannon, kDownbeatHalfWu, t);
                }
            });
        }

        // Minelayer: a cluster of mines (one a level) on every bar line; all blow on the next bar line.
        if (const u8 L = lv(kMinelayer)) {
            forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
                for (u32 k = 0; k < L; ++k) {
                    for (PlayerState::Mine& m : pl.mines) {
                        if (m.blowTick >= 0.0) continue;
                        m.at = k == 0 ? pl.pos : pl.pos + dirOf(kTau * static_cast<float>(k - 1) / static_cast<float>(L - 1)) * 60.0f;
                        m.blowTick = t + kTicksPerBar;
                        emitFx(p, kFxMineDrop, m.at, m.at, static_cast<u8>(k), 0.0f, t);
                        break;
                    }
                }
            });
        }

        // Storm Coil: on every triplet eighth it zaps the nearest enemies (one a level up to five) within its reach.
        if (const u8 L = lv(kStormCoil)) {
            forCrossings(m0_, m1_, (hyper ? 0.5 : 1.0) * kTicksPerBeat / 3.0, [&](double t, i64) {
                u32 near[5];
                const u32 n = nearestEnemies(pl.pos, kCoilReachWu[L < 5 ? L : 5], near, L < 5 ? L : 5);
                for (u32 k = 0; k < n; ++k) {
                    const Vec2 at{e.x[near[k]], e.y[near[k]]};
                    emitFx(p, kFxArc, pl.pos, at, static_cast<u8>(k + 1), 3.0f, t);
                    if (local) hitEnemy(near[k], kCoilDamage, owner, at, normalizeOr(at - pl.pos, Vec2{1.0f, 0.0f}));
                }
            });
        }

        // Swarm: a volley of homing micro-missiles each bar, launched as a 1/32 run from the bar line.
        if (const u8 L = lv(kSwarm)) {
            const u32 count = kSwarmCount[L < 5 ? L : 5];
            forCrossings(m0_, m1_, 0.5 * kTicksPerStep, [&](double t, i64 k) {
                const u32 j = static_cast<u32>(((k % 32) + 32) % 32);
                if (j >= count || !shotRoom(p)) return;
                const float side = (j & 1u) ? 1.0f : -1.0f;
                const float a = angleOf(d) + side * (25.0f + 5.0f * static_cast<float>(j / 2)) * kDeg;
                spawnShotRaw(p, pl.pos, dirOf(a), tickAgeOf(t) * (kMissileSpeed / kShotSpeed), kMissileDamage, kShotMissile, 0,
                             l.fused(kMurmuration) ? kMurmurationLifeS : kMissileLifeS, 0, 0);
                if (lastShot_ >= 0) {
                    shots_.speed[lastShot_] = kMissileSpeed;
                    shots_.mods[lastShot_] = kShotSeek;
                }
                emitFx(p, kFxSwarm, pl.pos, pl.pos + dirOf(a), static_cast<u8>(j), 0.0f, t);
            });
        }

        // Singularity: every fourth bar a well 300 wu ahead (placed on beat 2) pulls for three beats, then blows on beat 1.
        // The band's rule (audio, riffs.singularity): the pull is in the bars whose song index + 1 is a multiple of four.
        if (const u8 L = lv(kSingularity)) {
            // (co-op: the owner places it and tells the others, world_net.cpp kPeWell)
            if (local)
                forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64 bar) {
                    if ((((bar + songShift_ + 1) % 4) + 4) % 4 != 0 || pl.wellFrom >= 0.0) return;
                    pl.wellFrom = t + kTicksPerBeat;
                    pl.wellBlow = t + kTicksPerBar;
                });
            if (pl.wellFrom >= 0.0) {
                if (local && m0_ < pl.wellFrom && m1_ >= pl.wellFrom) {
                    pl.wellAt = pl.pos + d * kWellAheadWu;
                    pl.wellAt.x = clampf(pl.wellAt.x, -kArenaHalfW, kArenaHalfW);
                    pl.wellAt.y = clampf(pl.wellAt.y, -kArenaHalfH, kArenaHalfH);
                    emitFx(p, kFxWell, pl.wellAt, pl.wellAt, L, kWellPullWu[L < 5 ? L : 5], pl.wellBlow);
                }
                if (m1_ >= pl.wellFrom && m1_ < pl.wellBlow) pullBodies(pl.wellAt, kWellPullWu[L < 5 ? L : 5], kWellPull, dt);
                if (m1_ >= pl.wellBlow) {
                    if (local) areaHit(owner, pl.wellAt, kWellBlastWu[L < 5 ? L : 5], kWellDamage[L < 5 ? L : 5], -1, true);
                    emitFx(p, kFxWellBlow, pl.wellAt, pl.wellAt, L, kWellBlastWu[L < 5 ? L : 5], pl.wellBlow);
                    pl.wellFrom = pl.wellBlow = -1.0;
                }
            }
        }

        // Comet: a boomerang disc thrown along the aim each bar (two at level 3, three at 5), out a beat and back a beat; it
        // hurts what it touches once an eighth.
        if (const u8 L = lv(kComet)) {
            forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
                const u32 n = kCometCount[L < 5 ? L : 5];
                for (u32 k = 0; k < n && k < PlayerState::kComets; ++k) {
                    PlayerState::Comet& c = pl.comets[k];
                    const float off = n == 1 ? 0.0f : (static_cast<float>(k) - 0.5f * static_cast<float>(n - 1)) * 25.0f * kDeg;
                    c.from = pl.pos;
                    c.dir = dirOf(angleOf(d) + off);
                    c.start = t;
                    c.size = kCometSize[L < 5 ? L : 5];
                }
                emitFx(p, kFxComet, pl.pos, pl.pos + d, static_cast<u8>(n), 0.0f, t);
            });
            if (local)
                forCrossings(m0_, m1_, 0.5 * kTicksPerBeat, [&](double, i64) {
                    for (u32 k = 0; k < PlayerState::kComets; ++k) {
                        Vec2 at;
                        float r = 0.0f;
                        if (cometAt(p, k, at, r)) areaHit(owner, at, r, kCometDamage, -1, true);
                    }
                });
        }

        // Sweeper: a laser arm (two from level 3) turning round the ship once every two bars (every bar at 5); each pass
        // over a body burns it.
        if (const u8 L = lv(kSweeper)) {
            const float len = kSweeperLengthWu[L < 5 ? L : 5];
            const float a1 = sweeperAngle(p);
            double turn = L >= 5 ? kTicksPerBar : 2.0 * kTicksPerBar;
            if (hyper) turn *= 0.5;
            const float swept = static_cast<float>((m1_ - m0_) / turn) * kTau;
            if (local && swept > 0.0f) {
                const float reach = len + kQuerySlackWu;
                grid_.query(pl.pos.x - reach, pl.pos.y - reach, pl.pos.x + reach, pl.pos.y + reach, [&](u32 i) {
                    if (e.flags[i] & kFlagDead) return;
                    const Vec2 v = Vec2{e.x[i], e.y[i]} - pl.pos;
                    const float dist = length(v);
                    if (dist > len + e.radius[i]) return;
                    const float slack = dist > 1.0f ? std::fmin(e.radius[i] / dist, 0.5f) : 0.5f;
                    for (u32 arm = 0; arm < (L >= 3 ? 2u : 1u); ++arm) {
                        const float a = a1 + static_cast<float>(arm) * kPi;
                        const float back = wrapAngle(a - angleOf(v));  // how far the arm has passed it
                        if (back >= -slack && back <= swept + slack) {
                            hitEnemy(i, kSweeperDamage, owner, Vec2{e.x[i], e.y[i]}, dirOf(a + 0.5f * kPi));
                            break;
                        }
                    }
                });
            }
        }

        // Flak: on beats 2 and 4 (every beat under Hyper) a radial burst of pellets; at level 5 they explode.
        if (const u8 L = lv(kFlak)) {
            forCrossings(m0_, m1_, kTicksPerBeat, [&](double t, i64 beat) {
                const i32 b = static_cast<i32>(((beat % 4) + 4) % 4);
                if (!(hyper || b == 1 || b == 3) || !shotRoom(p)) return;
                const u32 n = kFlakPellets[L < 5 ? L : 5];
                const float a0 = static_cast<float>(beat % 7) * 0.37f;
                for (u32 k = 0; k < n; ++k) {
                    spawnShotRaw(p, pl.pos, dirOf(a0 + kTau * static_cast<float>(k) / static_cast<float>(n)), tickAgeOf(t) * (kPelletSpeed / kShotSpeed),
                                 1.0f, kShotPellet, 0, kPelletLifeS + 0.05f, 0, 0);
                    if (lastShot_ >= 0) {
                        shots_.speed[lastShot_] = kPelletSpeed;
                        if (L >= 5) shots_.mods[lastShot_] = kShotBlast;
                    }
                }
                emitFx(p, kFxFlak, pl.pos, pl.pos, static_cast<u8>(n), 0.0f, t);
            });
        }
    }
}

}  // namespace rf::sim
