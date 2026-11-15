// The fights (design/bosses/*.md). What every fight shares: its entrance, health by the players present, the hits
// and their multipliers, part deaths, its death and rewards, the co-op verdicts. And Kaleidos (M3 stage B): the wheel's
// angle, who rests, every bullet and beam are functions of song time and of which pairs are alive; health is the only
// state the fight keeps. Longwave and the Choir: world_longwave.cpp, world_choir.cpp; the mini-bosses and the Pen:
// world_miniboss.cpp.

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kDeg = kTau / 360.0f;
constexpr float kArmStep = kTau / 6.0f;

u32 pairsIn(u32 mask) { return (mask & 1u) + ((mask >> 1) & 1u) + ((mask >> 2) & 1u); }
u32 bitsIn(u32 m) {
    u32 n = 0;
    for (; m; m &= m - 1) ++n;
    return n;
}

// Half-width of an arm's hull at `u` wu from the centre: the facets' widths, joined.
float armHalfWidth(const BossData& b, float u) {
    const BossData::Facet* f = b.facets;
    if (u <= f[0].armRadius) return f[0].halfWidth;
    for (u32 k = 0; k + 1 < 4; ++k) {
        if (u <= f[k + 1].armRadius) {
            const float s = (u - f[k].armRadius) / (f[k + 1].armRadius - f[k].armRadius);
            return f[k].halfWidth + (f[k + 1].halfWidth - f[k].halfWidth) * s;
        }
    }
    return u <= f[3].armRadius + f[3].length * 0.5f ? f[3].halfWidth : 10.0f;  // then the tip star
}

double barStartOf(double t) { return std::floor(t / kTicksPerBar) * kTicksPerBar; }

// The build a player has grown by Track t, as Kaleidos's health followed it through the first slice (the lead: 1.5x in
// Track 2, 2x in Track 3, then +6% a Track): a boss in the loop keeps the share of the build its home Track asked for.
float buildGrowth(u32 t) {
    if (t < 1) t = 1;
    if (t <= 3) return kBossTrackHealth[t - 1];
    return kBossTrackHealth[2] * std::pow(kBossEndlessHealth, static_cast<float>(t - 3));
}
}  // namespace

static_assert(kMaxPlayersBoss == kMaxPlayers, "the boss's pace keeps a number a player");
static_assert(sizeof(kBossPaceNovice) / sizeof(kBossPaceNovice[0]) >= kBossKinds, "a novice's number a boss");

u32 bossScaledCount(u32 n, float scale, u32 least) {
    // A count scaled by a lever (play test 1), rounded, never below `least` (an exact count at scale 1).
    if (scale == 1.0f) return n;
    const u32 m = static_cast<u32>(std::lround(static_cast<float>(n) * scale));
    return m < least ? least : m;
}

u32 World::bossFightBar(double tick) const {
    if (tick < boss_.fightTick) return 0;
    return static_cast<u32>(std::floor((tick - boss_.fightTick) / kTicksPerBar + 1e-9));
}

float World::bossArmAngle(u32 arm) const { return boss_.angle + static_cast<float>(arm) * kArmStep; }

float World::bossBulletDensity() const {
    // Play test 1's lever on the bullet density (1 = the page's numbers) on Kaleidos; the loop ramps every boss's
    // (GAME_DESIGN 8), held inside the enemy bullet pool.
    const float lever = boss_.kind == BossKind::Kaleidos ? kBossBulletScale : kBossBulletScaleM5;
    return std::fmin(lever * loopRamp(kLoopThreatRamp, director_.track, director_.songs), kBossBulletDensityMax);
}

bool World::bossOn() const { return boss_.active && !boss_.dying && m1_ >= boss_.fightTick; }

namespace {
float wheelAt(const BossState& s, const BossData& b, double t) {
    const BossData::Phase& ph = b.phases[s.phase < 4 ? s.phase : 3];
    if (t <= s.phaseTick) return s.angle0;
    return s.angle0 + ph.direction * kTau * static_cast<float>((t - s.phaseTick) / (static_cast<double>(ph.turnBars) * kTicksPerBar));
}
}  // namespace

bool World::bossResting(u32 pair, double tick) const {
    const BossState& s = boss_;
    if (s.kind != BossKind::Kaleidos || !s.active || s.dying || tick < s.fightTick || !(s.alive & (1u << pair))) return false;
    const u32 fb = bossFightBar(tick);
    switch (s.phase) {
        case 0: return fb % 3 == pair;  // kaleidos.md rule 1: one pair rests each bar, A, B, C in turn
        case 1: {
            // The two pairs alternate bars: one fires, the other rests.
            u32 live[2] = {0, 0}, n = 0;
            for (u32 p = 0; p < 3 && n < 2; ++p)
                if (s.alive & (1u << p)) live[n++] = p;
            return live[(fb + 1) % 2] == pair;
        }
        case 2: return tick - barStartOf(tick) >= 3.0 * kTicksPerBeat;  // fires three beats, rests on beat 4
        default: return false;
    }
}

u8 World::bossFiringMask(double t) const {
    const BossState& s = boss_;
    if (s.kind != BossKind::Kaleidos || !s.active || s.dying || t < s.fightTick) return 0;
    const BossData::Phase& ph = species_->boss.phases[s.phase < 4 ? s.phase : 3];
    if (ph.fireStepTicks <= 0.0) return 0;
    const double inStep = std::fmod(t, ph.fireStepTicks);
    if (inStep > 0.5 && ph.fireStepTicks - inStep > 0.5) return 0;
    const double inBar = t - barStartOf(t);
    u8 mask = 0;
    for (u32 p = 0; p < 3; ++p) {
        if (!(s.alive & (1u << p)) || bossResting(p, t)) continue;
        // Refraction: the firing pair opens its bar with the beam, then fans on the eighths.
        if (s.phase == 1 && inBar < kTicksPerBeat) continue;
        mask = static_cast<u8>(mask | (1u << p));
    }
    return mask;
}

u8 World::bossBeamState(u32 pair, double tick) const {
    const BossState& s = boss_;
    if (s.kind != BossKind::Kaleidos || !s.active || s.dying || s.phase != 1 || tick < s.fightTick || !(s.alive & (1u << pair))) return 0;
    const u32 fb = bossFightBar(tick);
    const double inBar = tick - barStartOf(tick);
    // The pair that rests this bar fires next bar: telegraphed for the whole of this one (4 beats) ...
    if (bossResting(pair, tick) && fb >= s.phaseBar0) return 1;
    // ... and opens its bar with a beam one beat long, when it rested the bar before within this phase.
    if (!bossResting(pair, tick) && inBar < kTicksPerBeat && fb >= s.phaseBar0 + 1) return 2;
    return 0;
}

// ---------------------------------------------------------------------------------------------- entrance

float World::bossHealthScale(BossKind kind) const {
    // x (1 + 0.5 per extra player) as for every boss (GAME_DESIGN 8; M4 stage B: those still in the run).
    float scale = 1.0f + kCoopBossHealthPerPlayer * static_cast<float>(presentCount() - 1);
    const u32 tr = director_.track < 1 ? 1u : director_.track;
    if (isMiniBoss(kind)) {
        // minibosses.json common.healthByTrack: by Track from Track 2, x 1.25 for each Track of the loop, x its own factor.
        const MiniData& md = species_->minis;
        const u32 last = md.healthTracks;
        const u32 firstPass = tr < director_.songs ? tr : director_.songs;
        const u32 t = firstPass < 2 ? 2u : (firstPass > last ? last : firstPass);
        float h = md.healthByTrack[t];
        if (inLoop(tr, director_.songs)) h *= std::pow(md.loopFactor, static_cast<float>(tr - director_.songs));
        const MiniData::Entry* e = md.find(kind);
        const u32 mk = static_cast<u32>(kind) - static_cast<u32>(BossKind::AlphaHound);
        return scale * h * (e ? e->healthFactor : 1.0f) * (mk < 12 ? kMiniHealthTune[mk] : 1.0f) * rules_.healthLever[static_cast<u32>(kind)];
    }
    // A boss's own health is for its home Track's build; elsewhere (the loop) it follows the build's growth since, and
    // the loop's ramp. GAME_DESIGN 8 (play test 1, second pass): the first boss of a run, Track 1's, is the teacher.
    const u32 home = bossHomeTrack(kind);
    scale *= buildGrowth(tr) / buildGrowth(home);
    if (tr == 1) scale *= kFirstBossHealth;
    if (kind == BossKind::Kaleidos) scale *= kKaleidosHealthTune;
    scale *= loopRamp(kLoopHealthRamp, tr, director_.songs);
    if (kind == BossKind::Longwave) scale *= kLongwaveHealthTune;
    if (kind == BossKind::Choir) scale *= kChoirHealthTune;
    if (kind == BossKind::EventHorizon) scale *= kHorizonHealthTune;
    if (kind == BossKind::Polyrhythm) scale *= kPolyHealthTune;
    if (kind == BossKind::Feedback) scale *= kFeedbackHealthTune;
    if (kind == BossKind::Cadence) scale *= kCadenceHealthTune;
    if (kind == BossKind::Arpeggiator) scale *= kArpHealthTune;
    if (kind == BossKind::Morgana) scale *= kMorganaHealthTune;
    if (kind == BossKind::Reprise) scale *= kRepriseHealthTune;
    scale *= rules_.healthLever[static_cast<u32>(kind)];  // (measurements only)
    return scale;
}

void World::spawnBossNow(double enterTick, Vec2 pos, BossKind kind, Vec2 penAt) {
    BossState& s = boss_;
    s = BossState{};
    s.kind = kind;
    s.active = true;
    s.enterTick = enterTick;
    s.scale = bossHealthScale(kind);
    for (double& d : s.partDeath) d = -1.0;
    switch (kind) {
        case BossKind::Kaleidos: {
            const BossData& b = species_->boss;
            s.alive = 0x7;
            s.fightTick = enterTick + kTicksPerBar;  // the entrance bar: facets streak in and lock together
            s.phaseTick = s.fightTick;
            s.partMax = b.pairHealth * s.scale;
            s.coreMax = b.coreHealth * s.scale;
            for (u32 k = 0; k < 3; ++k) s.partHp[k] = s.partMax;
            s.coreHp = s.coreMax;
            const float m = b.radius + 120.0f;
            pos.x = clampf(pos.x, -kArenaHalfW + m, kArenaHalfW - m);
            pos.y = clampf(pos.y, -kArenaHalfH + m, kArenaHalfH - m);
            s.pos = s.prevPos = pos;
            break;
        }
        case BossKind::Longwave: enterLongwave(pos); break;
        case BossKind::Choir: enterChoir(pos); break;
        case BossKind::EventHorizon: enterHorizon(pos); break;  // M5 part 4
        case BossKind::Polyrhythm: enterPoly(pos); break;
        case BossKind::Feedback: enterFeedback(pos); break;
        case BossKind::Cadence: enterCadence(pos); break;
        case BossKind::Arpeggiator: enterArpeggiator(pos); break;
        case BossKind::Morgana: enterMorgana(pos); break;
        case BossKind::Reprise: enterReprise(pos); break;
        default: enterMini(pos, penAt); break;
    }
    if (isMiniBoss(kind)) director_.miniSpawned = true;
    else director_.bossSpawned = true;
    if (!isMiniBoss(kind)) {
        // GAME_DESIGN 5 (f32a441): nobody meets a boss on their last cell. As it arrives a player with fewer than two cells grows
        // back up to two (the arcs draw themselves over the entrance bar as Regen's do). Each device its own players.
        for (u32 p = 0; p < playerCount_; ++p) {
            PlayerState& pl = players_[p];
            if (!pl.present || !pl.alive || !isLocal(p)) continue;
            const u8 want = static_cast<u8>(pl.cellCap < kBossEntranceCells ? pl.cellCap : kBossEntranceCells);
            while (pl.cells < want) {
                ++pl.cells;
                Event ev;
                ev.type = EventType::CellTake;
                ev.a = static_cast<u8>(p);
                ev.b = pl.cells;
                ev.c = 1;  // (drawn as Regen's arc)
                ev.x = pl.pos.x;
                ev.y = pl.pos.y;
                emit(ev);
            }
        }
    }
    Event ev;
    ev.type = EventType::BossEnter;
    ev.a = static_cast<u8>(kind);
    ev.x = s.pos.x;
    ev.y = s.pos.y;
    ev.tick = s.fightTick;
    emit(ev);
    if (role_ == NetRole::Host && netLog_.bossCount < NetLog::kBoss) {
        NetLog::Boss& o = netLog_.boss[netLog_.bossCount++];
        o = NetLog::Boss{};
        o.kind = 0;
        o.boss = static_cast<u8>(kind);
        o.x = pos.x;
        o.y = pos.y;
        o.x2 = penAt.x;
        o.y2 = penAt.y;
        o.tick = enterTick;
    }
}

// ---------------------------------------------------------------------------------------------- hits

u32 World::bossParts(BossPart* out, u32 cap) const {
    const BossState& s = boss_;
    if (!s.active || s.dying || s.kind == BossKind::Kaleidos) return 0;
    u32 n = 0;
    const auto add = [&](Vec2 p, float r, u8 part, u8 index) {
        if (n < cap) out[n++] = BossPart{p, r, part, index};
    };
    switch (s.kind) {
        case BossKind::Longwave: {
            const LongwaveData& d = species_->longwave;
            add(s.body[0], d.headRadius, 3, 0);
            for (u32 k = 0; k < d.segments; ++k)
                if (s.alive & (1u << k)) add(s.body[1 + k], d.segRadius, 2, static_cast<u8>(k));
            break;
        }
        case BossKind::Choir: {
            const ChoirData& d = species_->choir;
            for (u32 k = 0; k < ChoirData::kVoices; ++k)
                if (s.alive & (1u << k)) add(s.body[1 + k], d.voiceRadius, 2, static_cast<u8>(k));
            break;
        }
        case BossKind::EventHorizon:
            if (m1_ >= s.fightTick) add(s.pos, species_->horizon.horizonRadius, 3, 0);  // the disc
            break;
        case BossKind::Polyrhythm: {
            // Each rotor: its faces (the glass and the gold bar: the only place it can be hurt), its body and its corners
            // (they shed shots).
            const PolyData& d = species_->poly;
            for (u32 k = 0; k < 2; ++k) {
                if (!(s.alive & (1u << k))) continue;
                const u32 sides = d.sides[k] ? d.sides[k] : 3u;
                const float apothem = d.rotorRadius * std::cos(kPi / static_cast<float>(sides));
                const float faceHalf = d.rotorRadius * std::sin(kPi / static_cast<float>(sides));
                const float f0 = s.bodyAng[k];
                for (u32 j = 0; j < sides; ++j) {
                    const float fa = f0 + kTau * static_cast<float>(j) / static_cast<float>(sides);
                    add(s.body[k] + dirOf(fa) * (apothem + 4.0f), faceHalf * d.faceBarFraction, 2, static_cast<u8>(k));
                    add(s.body[k] + dirOf(fa + kPi / static_cast<float>(sides)) * d.rotorRadius, 2.5f * d.cornerNodeRadius, 1, static_cast<u8>(k));
                }
                add(s.body[k], apothem, 1, static_cast<u8>(k));
            }
            break;
        }
        case BossKind::Feedback:
            if (m1_ >= s.fightTick) add(s.pos, species_->feedback.headRadius, 3, 0);  // the Head (its Echoes are not targets)
            break;
        case BossKind::Cadence:
            for (u32 k = 0; k < 4; ++k) add(s.body[1 + k], species_->cadence.pillarRadius, 3, static_cast<u8>(k));  // one health
            break;
        case BossKind::Morgana: add(s.body[0], species_->morgana.bodyRadius, 3, 0); break;
        case BossKind::Reprise: return repriseParts(out, cap);
        case BossKind::Arpeggiator:
            // Each machine: its core (one health for both) and its console (five circles along it: it sheds shots).
            if (m1_ < s.fightTick) break;
            for (u32 m = 0; m < (s.ap.two ? 2u : 1u); ++m) {
                const Vec2 c = arpMachine(m);
                add(c, species_->arp.coreRadius, 3, static_cast<u8>(m));
                const Vec2 out = s.ap.side[m] == 0 ? Vec2{0.0f, -1.0f} : (s.ap.side[m] == 1 ? Vec2{1.0f, 0.0f} : (s.ap.side[m] == 2 ? Vec2{0.0f, 1.0f} : Vec2{-1.0f, 0.0f}));
                const Vec2 axis{-out.y, out.x};
                for (i32 j = -2; j <= 2; ++j)
                    if (j) add(c + axis * (static_cast<float>(j) * 0.2f * prose::kApConsoleW), 0.5f * prose::kApConsoleD, 1, static_cast<u8>(m));
            }
            break;
        default: return miniParts(out, cap);
    }
    return n;
}

bool World::bossHitTest(Vec2 a, Vec2 bpt, float r, u8& part, u8& pair, float& tOut) const {
    const BossState& s = boss_;
    if (!s.active || s.dying) return false;
    if (s.kind != BossKind::Kaleidos) {
        BossPart parts[kBossBodies + 4];
        const u32 n = bossParts(parts, kBossBodies + 4);
        const Vec2 d = bpt - a;
        const float len2 = lengthSq(d);
        float best = 2.0f;
        for (u32 k = 0; k < n; ++k) {
            const Vec2 c = parts[k].pos - a;
            const float t = len2 > 1e-6f ? clampf((c.x * d.x + c.y * d.y) / len2, 0.0f, 1.0f) : 0.0f;
            const Vec2 q = d * t - c;
            const float rr = parts[k].radius + r;
            if (lengthSq(q) > rr * rr || t >= best) continue;
            best = t;
            part = parts[k].part;
            pair = parts[k].index;
        }
        if (best > 1.0f) return false;
        tOut = best;
        return true;
    }
    const BossData& b = species_->boss;
    // Sample the path (a shot moves ~23 wu a tick; the thinnest part is 20 wu wide).
    static const float kT[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    const float lensHalf = b.lensHalfAngleDeg * kDeg;
    for (float t : kT) {
        const Vec2 p = a + (bpt - a) * t;
        const Vec2 rel = p - s.pos;
        const float dist = length(rel);
        if (dist > b.radius + r + 12.0f) continue;
        // Arms of the living pairs.
        for (u32 k = 0; k < 6; ++k) {
            const u32 pr = k % 3;
            if (!(s.alive & (1u << pr))) continue;
            const Vec2 d = dirOf(s.angle + static_cast<float>(k) * kArmStep);
            const float u = rel.x * d.x + rel.y * d.y;
            if (u < kBossArmInner || u > kBossArmOuter) continue;
            const float v = std::fabs(rel.x * d.y - rel.y * d.x);
            if (v <= armHalfWidth(b, u) + r) {
                part = 2;
                pair = static_cast<u8>(pr);
                tOut = t;
                return true;
            }
        }
        // Lens plates (two per living pair, centred on its arms) stop shots; between and behind them, the core.
        if (dist <= b.lensRadius + r) {
            const float ang = angleOf(rel);
            bool plate = false;
            u8 platePair = 0;
            for (u32 k = 0; k < 6 && !plate; ++k) {
                const u32 pr = k % 3;
                if (!(s.alive & (1u << pr))) continue;
                if (std::fabs(wrapAngle(ang - (s.angle + static_cast<float>(k) * kArmStep))) <= lensHalf) {
                    plate = true;
                    platePair = static_cast<u8>(pr);
                }
            }
            if (plate && dist >= b.lensInner - r - 8.0f) {
                part = 1;
                pair = platePair;
                tOut = t;
                return true;
            }
            if (dist <= b.coreRingRadius[0] + r) {
                part = plate ? 1 : 3;  // inside the plate's arc the plate took it first
                pair = platePair;
                tOut = t;
                return true;
            }
        }
    }
    return false;
}

u8 World::shootBoss(Vec2 a, Vec2 b, float damage) {
    u8 part = 0, pair = 0;
    float t = 0.0f;
    if (!bossHitTest(a, b, kShotRadius, part, pair, t)) return 0;
    damageBoss(part, pair, damage, 0, a + (b - a) * t, normalizeOr(b - a, Vec2{1.0f, 0.0f}));
    return part;
}

float World::bossDamageScale(u8 part, u8 index, double tick, Vec2 dir) const {
    const BossState& s = boss_;
    switch (s.kind) {
        case BossKind::Kaleidos: {
            const BossData& b = species_->boss;
            if (part == 2) return bossResting(index, tick) ? b.dmgResting : b.dmgFiring;  // kaleidos.md rule 1
            if (part == 3) return tick - barStartOf(tick) < kTicksPerBeat ? b.dmgCoreDownbeat : b.dmgCoreExposed;  // rule 3
            return b.dmgLens;
        }
        case BossKind::Longwave: return longwaveScale(part, index, tick);
        case BossKind::Choir: return choirScale(part, index, tick);
        case BossKind::EventHorizon: return horizonScale();
        case BossKind::Polyrhythm: return polyScale(part, index);
        case BossKind::Feedback: return part == 3 ? feedbackScale() : 0.0f;
        case BossKind::Cadence: return part == 3 ? cadenceScale(index, tick) : 0.0f;
        case BossKind::Arpeggiator: return arpScale(part, index, tick);
        case BossKind::Morgana: return part == 3 ? morganaScale(tick) : 0.0f;
        case BossKind::Reprise: return repriseScale(part, index, tick);
        default: return miniScale(part, index, tick, dir);
    }
}

bool World::assistBossAim(Vec2 from, double tick, Vec2& at) const {
    // GAME_DESIGN 2 (play test 3): in a boss or mini-boss fight the assist aims at the boss: its open weak point when it has
    // one (a part a hit multiplies, judged when the shot arrives), else its nearest part that takes damage, else its nearest
    // part. Everything is led by the shot's flight. (The ordinary enemy within 250 wu that comes first: PlayerController.)
    const BossState& s = boss_;
    if (!s.active || s.dying || tick < s.fightTick) return false;
    const float reach = kShotSpeed * kShotLifeS;
    const float tps = ticksPerSecond();
    if (s.kind == BossKind::Kaleidos) {
        // The core through an open sector (a dead pair's direction, or every pair dead); else the nearest resting arm (the
        // weak point, rule 1); else the nearest arm. The arm's middle, led along the wheel's turn.
        const Vec2 rel = from - s.pos;
        if (length(rel) > reach + kBossArmOuter) return false;
        bool open = s.alive == 0;
        for (u32 k = 0; k < 6 && !open; ++k) {
            if (s.alive & (1u << (k % 3))) continue;
            if (std::fabs(wrapAngle(angleOf(rel) - bossArmAngle(k))) < kAssistSectorRad) open = true;
        }
        if (open) {
            at = s.pos;
            return true;
        }
        const float spin = wrapAngle(s.angle - s.prevAngle) / kDt;
        const float mid = 0.5f * (kBossArmInner + kBossArmOuter);
        float best = 1e30f;
        bool any = false;
        for (u32 pass = 0; pass < 2 && !any; ++pass) {
            for (u32 k = 0; k < 6; ++k) {
                const u32 pr = k % 3;
                if (!(s.alive & (1u << pr))) continue;
                const Vec2 now = s.pos + dirOf(bossArmAngle(k)) * mid;
                const float flight = length(now - from) / kShotSpeed;
                if (pass == 0 && !bossResting(pr, tick + static_cast<double>(flight * tps))) continue;
                const Vec2 led = s.pos + dirOf(bossArmAngle(k) + spin * flight) * mid;
                const float d = lengthSq(led - from);
                if (d < best) {
                    best = d;
                    at = led;
                    any = true;
                }
            }
        }
        return any;
    }
    // Tiers: 3 an open weak point (the most it multiplies, then the nearest), 2 a full hit, 1 armour's share, 0 any part.
    BossPart parts[kBossBodies + 4];
    const u32 n = bossParts(parts, kBossBodies + 4);
    float best[4] = {-1e30f, -1e30f, -1e30f, -1e30f};
    Vec2 bestAt[4] = {};
    for (u32 k = 0; k < n; ++k) {
        const BossPart& q = parts[k];
        const float d = length(q.pos - from) - q.radius;
        if (d > reach) continue;
        // (its motion: the nearest of the fight's bodies)
        u32 bi = 0;
        float bd = 1e30f;
        for (u32 j = 0; j < kBossBodies; ++j) {
            const float dj = lengthSq(s.body[j] - q.pos);
            if (dj < bd) {
                bd = dj;
                bi = j;
            }
        }
        const float flight = (d > 0.0f ? d : 0.0f) / kShotSpeed;
        const Vec2 led = q.pos + (s.body[bi] - s.bodyPrev[bi]) * (flight / kDt);
        const float scale = q.part == 1 ? 0.0f
                                        : bossDamageScale(q.part, q.index, tick + static_cast<double>(flight * tps), normalizeOr(led - from, Vec2{1.0f, 0.0f}));
        const u32 tier = scale > 1.0f + 1e-3f ? 3u : (scale >= 1.0f - 1e-3f ? 2u : (scale > 0.0f ? 1u : 0u));
        const float score = (tier == 3 ? scale * 1000.0f : 0.0f) - d * 0.1f;
        if (score > best[tier]) {
            best[tier] = score;
            bestAt[tier] = led;
        }
    }
    for (u32 t = 4; t-- > 0;)
        if (best[t] > -1e29f) {
            at = bestAt[t];
            return true;
        }
    return false;
}

void World::damageBoss(u8 part, u8 pair, float dmg, u8 owner, Vec2 at, Vec2 dir) {
    BossState& s = boss_;
    Event ev;
    ev.type = EventType::Impact;
    ev.species = kBossSpecies;
    ev.c = part;
    ev.a = pair;
    ev.b = owner;  // play test 2: the shooter
    ev.x = at.x;
    ev.y = at.y;
    ev.x2 = dir.x;
    ev.y2 = dir.y;
    if (!s.active || s.dying || m1_ < s.fightTick) {
        ev.c = 1;  // the entrance: nothing gets through yet
        emit(ev);
        bossHitCue(false, at, owner);
        return;
    }
    ++s.hits;
    // Polyrhythm's corners and bodies are armour on their rotor (they stop shots and take its armour's share).
    u8 dpart = part;
    float scale = 0.0f;
    if (part == 1 && s.kind == BossKind::Polyrhythm) {
        scale = species_->poly.dmgCorner;
        dpart = 2;
    } else if (part == 1 && s.kind == BossKind::Arpeggiator) {
        scale = species_->arp.dmgConsole;  // M5 part 6: the console sheds shots (arpeggiator.json: a quarter on the core's health)
        dpart = 3;
    } else if (part == 1 && s.kind == BossKind::SentryArray) {
        scale = miniScale(1, pair, m1_, dir);  // b1d4a2f: a domed turret takes a quarter, on that turret's health
        dpart = 2;
    } else if (part != 1) {
        scale = bossDamageScale(part, pair, m1_, dir);
    }
    // Armour (GAME_DESIGN 7.6, a boss's rule is a bonus, not a wall): a multiplier below 1 (Kaleidos as built: only its
    // lens plates, at nothing; its firing arms' third stays a hit).
    const bool kaleidos = s.kind == BossKind::Kaleidos;
    if (rules_.armourNothing && !kaleidos && scale < 1.0f) scale = 0.0f;  // (calibration: armour as before 2026-11-13)
    if (kaleidos ? scale <= 0.0f : scale < 1.0f) {
        // The shot sparks and stops (a lens plate, a closed jaw, a silent hood, a rotor's corner, a Ram's horns, Feedback's
        // shutter) and does the share the page gives armour (a quarter; Kaleidos's plates and the Ram's horns: nothing).
        ev.c = 1;
        ev.f = scale;
        emit(ev);
        bossHitCue(false, at, owner);
        if (s.kind == BossKind::Feedback && s.fb.open <= 0.0f) feedbackSwallow();
        if (scale > 0.0f) damageBossFinal(dpart, pair, dmg * scale, owner, at, dir, -1.0f);
        return;
    }
    damageBossFinal(dpart, pair, dmg * scale, owner, at, dir, scale);
}

void World::damageBossFinal(u8 part, u8 pair, float dmg, u8 owner, Vec2 at, Vec2 dir, float scale) {
    // The damage with its multiplier already applied: the claimant's (M4: a host applies a client's claim as
    // the client weighed it). `scale` < 0: no impact shown (a Nova's blast).
    BossState& s = boss_;
    if (!s.active || s.dying || pair >= kBossParts) return;
    if (scale >= 0.0f) {
        Event ev;
        ev.type = EventType::Impact;
        ev.species = kBossSpecies;
        ev.c = part;
        ev.a = pair;
        ev.b = owner;
        ev.x = at.x;
        ev.y = at.y;
        ev.x2 = dir.x;
        ev.y2 = dir.y;
        ev.f = scale;
        emit(ev);
        bossHitCue(scale >= 1.0f - 1e-3f && part != 1, at, owner);
    }
    const bool client = role_ == NetRole::Client;
    // (one claim a part a tick: since armour takes a quarter every shot on a boss claims, so the claims of a tick on the same
    // part are summed rather than sent one by one)
    bool merged = false;
    for (u32 k = 0; client && k < netLog_.bossClaimCount && !merged; ++k) {
        NetBossClaim& c = netLog_.bossClaims[k];
        if (c.part == part && c.pair == pair) {
            c.damage += dmg;
            merged = true;
        }
    }
    if (merged) {
    } else if (client && netLog_.bossClaimCount < NetLog::kBossClaims) {
        NetBossClaim& c = netLog_.bossClaims[netLog_.bossClaimCount++];
        c.part = part;
        c.pair = pair;
        c.damage = dmg;
    } else if (client) {
        ++netLog_.overflow;
    }
    if (!isMiniBoss(s.kind)) {
        // M5 part 7 (GAME_DESIGN 7.6, a boss keeps pace): the first eight bars' damage is counted a player; from the pace on
        // the boss takes its damage divided by it (after a client's claim is noted: the host divides by its own).
        // c9218dc (GAME_DESIGN 7.6, "only aimed damage counts"): what a player does while firing through the assist is the
        // novice's own fire and never raises the pace (a player whose pose is the assist's, fire 2, or not firing); the whole
        // is kept as the measured number (the novice's references are its whole eight bars).
        if (!s.paceSet && owner < kMaxPlayersBoss) {
            s.paceDmg[owner] += dmg;
            if (owner < kMaxPlayers && players_[owner].fire == 1) s.paceAimed[owner] += dmg;
        }
        dmg /= s.pace > 0.0f ? s.pace : 1.0f;
    }
    if (isMiniBoss(s.kind)) {
        s.mini.lastHurt = m1_;                                   // (Regenerating: quiet beats)
        if (s.kind == BossKind::DeepWell && part == 3) mini3Feed(dmg);  // M5 part 6: what it swallows fills it
    }
    if (s.kind == BossKind::Reprise) {  // M5 part 6: the core's health, or an arm's sixth in the Medley
        repriseDamage(part, pair, dmg, owner);
        return;
    }
    if (part == 2) {
        if (!(s.alive & (1u << pair))) return;
        s.partHp[pair] -= dmg;
        if (client) {
            if (s.partHp[pair] < 0.0f) s.partHp[pair] = 0.0f;  // the host decides when it breaks
        } else if (s.partHp[pair] <= 0.0f) {
            killBossPart(pair, owner);
        }
    } else if (part == 3) {
        s.coreHp -= dmg;
        if (client) {
            if (s.coreHp < 0.0f) s.coreHp = 0.0f;
        } else if (s.coreHp <= 0.0f) {
            killBoss(owner);
        } else if (isMiniBoss(s.kind)) {
            miniHealthSteps();  // half health, the Carom King's splits (the host's verdict, sent as steps)
        }
    }
}

void World::bossNova(u32 p) {
    // GAME_DESIGN 5: heavy damage to bosses in view, a Nova's blast on each part that can be hurt, the core once open
    // (Kaleidos: each living pair, the core once a pair is down; the others: every living part and the core; on armour the
    // armour's share, GAME_DESIGN 7.6, never more). Co-op: the Nova's owner deals it (a client claims it from the host).
    BossState& s = boss_;
    const PlayerState& pl = players_[p];
    if (!isLocal(p) || !bossOn()) return;
    const float reach = s.kind == BossKind::Kaleidos ? 270.0f : 0.0f;
    if (s.kind == BossKind::Kaleidos) {
        if (std::fabs(s.pos.x - pl.pos.x) > kNovaViewHalfW + reach || std::fabs(s.pos.y - pl.pos.y) > kNovaViewHalfH + reach) return;
        for (u8 k = 0; k < 3; ++k)
            if (s.alive & (1u << k)) damageBossFinal(2, k, kBossNovaDamage, static_cast<u8>(p), s.pos, Vec2{1.0f, 0.0f}, -1.0f);
        if (s.active && !s.dying && pairsIn(s.alive) != 3) damageBossFinal(3, 0, kBossNovaDamage, static_cast<u8>(p), s.pos, Vec2{1.0f, 0.0f}, -1.0f);
        return;
    }
    BossPart parts[kBossBodies + 4];
    const u32 n = bossParts(parts, kBossBodies + 4);
    u32 struck = 0;  // (a part with several circles, Polyrhythm's faces, takes the blast once)
    for (u32 k = 0; k < n && s.active && !s.dying; ++k) {
        const BossPart& q = parts[k];
        if (q.part == 1) continue;
        if (std::fabs(q.pos.x - pl.pos.x) > kNovaViewHalfW || std::fabs(q.pos.y - pl.pos.y) > kNovaViewHalfH) continue;
        if (q.part == 2 && !(s.alive & (1u << q.index))) continue;
        if (q.part == 2 && q.index < 32 && (struck & (1u << q.index))) continue;
        if (q.part == 2 && q.index < 32) struck |= 1u << q.index;
        float scale = bossDamageScale(q.part, q.index, m1_, normalizeOr(q.pos - pl.pos, Vec2{1.0f, 0.0f}));
        if (rules_.armourNothing && scale < 1.0f) scale = 0.0f;
        if (scale <= 0.0f) continue;
        damageBossFinal(q.part, q.index, kBossNovaDamage * kBossNovaShareM5 * std::fmin(scale, 1.0f), static_cast<u8>(p), q.pos, Vec2{1.0f, 0.0f}, -1.0f);
    }
}

// ---------------------------------------------------------------------------------------------- deaths

void World::killBossPart(u8 index, u8 owner) {
    BossState& s = boss_;
    if (!(s.alive & (1u << index))) return;
    s.alive &= ~(1u << index);
    s.partHp[index] = 0.0f;
    s.partDeath[index] = m1_;
    if (owner < kMaxPlayers && players_[owner].present && isLocal(owner)) {
        PlayerState& pl = players_[owner];
        pl.score += static_cast<u64>(std::llround(static_cast<double>(kBossPairScore) * pl.chain * rules_.scoreMultiplier));
    }
    if (role_ == NetRole::Host && netLog_.bossCount < NetLog::kBoss) {
        NetLog::Boss& o = netLog_.boss[netLog_.bossCount++];
        o = NetLog::Boss{};
        o.kind = 1;
        o.pair = index;
        o.killer = owner;
    }
    Event ev;
    ev.type = EventType::BossPair;
    ev.a = index;
    ev.b = static_cast<u8>(bitsIn(s.alive));
    ev.c = static_cast<u8>(s.kind);
    ev.x = s.pos.x;
    ev.y = s.pos.y;
    switch (s.kind) {
        case BossKind::Kaleidos: {
            // Its Shards burst from both tips (each device its own players', M4).
            for (u32 m = 0; m < 2; ++m) {
                const Vec2 tip = s.pos + dirOf(s.angle + static_cast<float>(index + 3 * m) * kArmStep) * species_->boss.tip.armRadius;
                for (u32 p = 0; p < playerCount_; ++p) {
                    if (!players_[p].present || !isLocal(p)) continue;
                    for (u32 k = 0; k < kBossPairShards / 2; ++k)
                        spawnShard(tip, dirOf(rng_.range(0.0f, kTau)) * rng_.range(kShardBurstLo, kShardBurstHi * 1.5f), static_cast<u8>(p));
                }
            }
            // The wheel flinches: it stops for one beat and turns the other way, faster (the next phase).
            s.angle0 = wheelAt(s, species_->boss, m1_);
            s.phase = static_cast<u8>(3 - pairsIn(s.alive));
            s.phaseTick = m1_ + kTicksPerBeat;
            s.phaseBar0 = bossFightBar(s.phaseTick) + 1;
            ev.b = static_cast<u8>(pairsIn(s.alive));
            ev.f = s.angle + static_cast<float>(index) * kArmStep;
            emit(ev);
            return;
        }
        case BossKind::Longwave: longwavePartDied(index, ev); break;
        case BossKind::Choir: choirPartDied(index, ev); break;
        case BossKind::Polyrhythm: polyPartDied(index, ev); break;
        case BossKind::Reprise: reprisePartDied(index, ev); break;
        default: miniPartDied(index, owner, ev); break;
    }
    emit(ev);
    // The Choir has no core: its last Voice is its death (Polyrhythm: its second rotor).
    if ((s.kind == BossKind::Choir || s.kind == BossKind::Polyrhythm || s.kind == BossKind::SentryArray || s.kind == BossKind::TwinTether) && s.alive == 0 &&
        role_ != NetRole::Client)
        killBoss(owner);
}

void World::killBoss(u8 owner) {
    BossState& s = boss_;
    if (s.dying || !s.active) return;
    s.dying = true;
    s.deathTick = m1_;
    s.coreHp = 0.0f;
    const bool mini = isMiniBoss(s.kind);
    // The score: Kaleidos's core is worth kBossCoreScore; a mini-boss 25 times a tier-5 body of its family.
    float worth = kBossCoreScore;
    if (mini) worth = miniScore();
    if (owner < kMaxPlayers && players_[owner].present && isLocal(owner)) {
        PlayerState& pl = players_[owner];
        ++pl.kills;
        pl.score += static_cast<u64>(std::llround(static_cast<double>(worth) * pl.chain * rules_.scoreMultiplier));
    }
    ++counters_.kills;
    DirectorState& d = director_;
    // The end: the first bar line at least half a bar away (the band, which hears of it now, can still start the
    // next section on that line): the Track-change Break after a boss, the Break after a mini-boss's held Drop.
    const double rel = (m1_ + 0.5 * kTicksPerBar - d.startTick) / kTicksPerBar;
    const u32 endBar = static_cast<u32>(std::floor(rel)) + 1;
    if (mini) {
        extraLifeDrop(s.pos, true);  // GAME_DESIGN 5 (2026-11-14): in Tracks 3, 5, 7 and 9 the mini-boss drops the Track's Extra Life
        miniDied(owner, endBar);
    } else {
        // Its bullets go with it; then the Shards (kaleidos.md: "then the Shards"): 200 per player (the Choir 220; each
        // device its own players', M4).
        for (u32 i = 0; i < bullets_.count; ++i) bullets_.dead[i] = 1;
        s.deathBars = s.kind == BossKind::Choir ? 2.0 : (s.kind == BossKind::Longwave ? 1.75 : (s.kind == BossKind::EventHorizon ? 1.25 : 1.0));
        // Event Horizon implodes for a bar and a beat of black first; everything it ate comes back with the flash.
        if (s.kind == BossKind::EventHorizon) s.payoutTick = m1_ + kTicksPerBar + kTicksPerBeat;
        else if (s.kind == BossKind::Polyrhythm) {
            // The playhead runs a last bar of sparks; the boss break and the Shards on the One that ends the Track.
            s.payoutTick = tickOfBar(endBar);
            s.deathBars = (s.payoutTick - m1_) / kTicksPerBar + 0.25;
        } else if (s.kind == BossKind::Reprise) {
            // It implodes, quickly (the owner, 2026-11-14: "implosion needs to be quick and big camera shake on explosion"), to the
            // first beat line at least three quarters of a beat on (repriseBurstTick) and bursts there with its Shards; the reversed
            // cymbal (RpDeath, its length in beats in f) ends on that beat. The Track still ends on its bar line (changeBar).
            s.payoutTick = repriseBurstTick(m1_);
            s.deathBars = (s.payoutTick - m1_) / kTicksPerBar + 0.25;
            emitCue(BossCue::RpDeath, s.payoutTick, 0, static_cast<float>((s.payoutTick - m1_) / kTicksPerBeat), s.pos);
        } else if (s.kind == BossKind::Arpeggiator) {
            // The chart reads GG; every block climbs back up its lane as Shards; the Shards on the One that ends the Track.
            s.payoutTick = tickOfBar(endBar);
            s.deathBars = (s.payoutTick - m1_) / kTicksPerBar + 0.25;
            s.ap.deathTick = m1_;
            emitCue(BossCue::ApDeath, m1_, 0, 0.0f, s.pos);
        } else if (s.kind == BossKind::Cadence) {
            // The four burst on the four beats of the last bar, then the tonic over a gong (the Shards with it).
            s.payoutTick = tickOfBar(endBar + 1);
            s.deathBars = (s.payoutTick - m1_) / kTicksPerBar + 0.25;
        } else if (s.kind == BossKind::Feedback) {
            // The tape is cut; the Head holds the root for two bars while the screen blooms, then the break on the One.
            s.payoutTick = tickOfBar(endBar + 1);
            s.deathBars = (s.payoutTick - m1_) / kTicksPerBar + 0.25;
            emitCue(BossCue::FbCut, m1_, 0, 0.0f, s.pos);
        } else {
            bossPayout();
        }
        // The Track ends: its Track-change Break begins on that bar line.
        d.changeBar = endBar;
    }
    if (role_ == NetRole::Host && netLog_.bossCount < NetLog::kBoss) {
        NetLog::Boss& o = netLog_.boss[netLog_.bossCount++];
        o = NetLog::Boss{};
        o.kind = 2;
        o.killer = owner;
        o.changeBar = endBar;
        o.x = s.pos.x;
        o.y = s.pos.y;
    }
    Event ev;
    ev.type = EventType::BossDeath;
    ev.a = static_cast<u8>(s.kind);
    ev.b = owner;
    ev.x = s.pos.x;
    ev.y = s.pos.y;
    ev.tick = tickOfBar(endBar);
    emit(ev);
}

void World::bossPayout() {
    // A boss's Shards and Cells for this device's players.
    const BossState& s = boss_;
    u32 shards = kBossShards;
    if (s.kind == BossKind::Choir) shards = prose::kChoirShards;
    if (s.kind == BossKind::EventHorizon) shards = prose::kHzShards;
    if (s.kind == BossKind::Polyrhythm) shards = prose::kPrShards;
    if (s.kind == BossKind::Feedback) shards = prose::kFbShards;
    if (s.kind == BossKind::Cadence) shards = prose::kCdShards;
    if (s.kind == BossKind::Arpeggiator) shards = species_->arp.rewardShards;
    if (s.kind == BossKind::Morgana) shards = species_->morgana.rewardShards;
    if (s.kind == BossKind::Reprise) shards = species_->reprise.rewardShards;
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present || !isLocal(p)) continue;
        for (u32 k = 0; k < shards; ++k)
            spawnShard(s.pos + dirOf(rng_.range(0.0f, kTau)) * rng_.range(0.0f, 60.0f), dirOf(rng_.range(0.0f, kTau)) * rng_.range(120.0f, 520.0f),
                       static_cast<u8>(p));
        // Play test 2 (GAME_DESIGN 6.1): a boss drops two Cells for each player, whatever its health.
        for (u32 k = 0; k < kBossCells; ++k)
            spawnCell(s.pos, dirOf(kTau * (static_cast<float>(k) + 0.25f) / static_cast<float>(kBossCells)) * 160.0f, static_cast<u8>(p));
    }
}

void World::bossPhasePay() {
    // M6 (GAME_DESIGN 5, 1bcc897): "a boss pays for its phases": at each phase change one Cell for each player, at the boss,
    // drifting to them (kBossPhaseCellDriftWu; inside the magnet it homes as any Cell). The phase is the host's verdict where the
    // host decides it; each device drops its own players' Cells, as bossPayout does. Mini-bosses have no phases to pay for.
    BossState& s = boss_;
    if (!s.active || s.dying || isMiniBoss(s.kind)) return;
    while (s.phasePaid < s.phase) {
        ++s.phasePaid;
        for (u32 p = 0; p < playerCount_; ++p) {
            const PlayerState& pl = players_[p];
            if (!pl.present || !pl.alive || !isLocal(p)) continue;
            for (u32 k = 0; k < kBossPhaseCells; ++k) {
                const u32 before = cellPickups_.count;
                spawnCell(s.pos, normalizeOr(pl.pos - s.pos, Vec2{1.0f, 0.0f}) * kBossPhaseCellDriftWu, static_cast<u8>(p));
                if (cellPickups_.count > before) cellPickups_.homing[cellPickups_.count - 1] = 2;  // (drifting to its player: updateCells)
            }
        }
    }
}

void World::bossStep(u8 step, u8 owner) {
    // A change the host's health decides (a mini-boss's half health, the Carom King's splits): applied once, and sent.
    BossState& s = boss_;
    if (!s.active || s.dying) return;
    if (role_ == NetRole::Host && netLog_.bossCount < NetLog::kBoss) {
        NetLog::Boss& o = netLog_.boss[netLog_.bossCount++];
        o = NetLog::Boss{};
        o.kind = 1;
        o.pair = static_cast<u8>(kBossStepBase + step);
        o.killer = owner;
    }
    miniStep(step);
}

void World::bossDecide(u8 geom, Vec2 a, Vec2 b, double tick) {
    if (role_ == NetRole::Client) {
        bossGeom(geom, a, b, tick, false);  // this device's guess, until the host's arrives
        return;
    }
    bossGeom(geom, a, b, tick, true);
    if (role_ == NetRole::Host && netLog_.bossCount < NetLog::kBoss) {
        NetLog::Boss& o = netLog_.boss[netLog_.bossCount++];
        o = NetLog::Boss{};
        o.kind = 1;
        o.pair = static_cast<u8>(kBossGeomBase + geom);
        o.x = a.x;
        o.y = a.y;
        o.x2 = b.x;
        o.y2 = b.y;
        o.tick = tick;
    }
}

void World::bossGeom(u8 geom, Vec2 a, Vec2 b, double tick, bool host) {
    BossState& s = boss_;
    if (!s.active || s.dying || geom >= kGeomCount) return;
    // The latest moment wins; for the same moment the host's.
    if (host ? tick < s.geomTick[geom] : tick <= s.geomTick[geom]) return;
    s.geomTick[geom] = tick;
    MiniState& m = s.mini;
    switch (geom) {
        case kGeomHorizonBar: horizonBar(tick, a.x > 0.5f, static_cast<u32>(a.y)); break;  // M5 part 4
        case kGeomHorizonDrift:  // M4 net: the point the Well drifts toward from the tick on
            if (s.kind == BossKind::EventHorizon) {
                if (s.hz.driftAt >= 0.0 && tick > s.hz.driftAt) {  // (the one before is due first)
                    s.hz.driftTo = s.hz.driftNext;
                    s.hz.driftSet = true;
                }
                s.hz.driftNext = a;
                s.hz.driftAt = tick;
            }
            break;
        case kGeomHorizonPhase:  // M4 net: the host's phase and the beat it turns on
            if (s.kind == BossKind::EventHorizon && a.x >= 0.0f && a.x < 3.0f) {
                s.hz.phaseNext = static_cast<u8>(a.x);
                s.hz.phaseAt = tick;
            }
            break;
        case kGeomPolyHemiola:
            s.pr.phase = s.pr.phase < 1 ? 1 : s.pr.phase;
            s.pr.hemiolaBar0 = static_cast<i64>(a.x);
            break;
        case kGeomMothCourt: m.ringAt = a; break;  // M5 part 4
        case kGeomBlinkStar:
            m.ringAt = a;
            m.ringAng = b.x;
            break;
        case kGeomBossPace:  // M5 part 7: the fight's pace on a bar line of its first eight (the host's); after the eighth it stands
            s.pace = clampf(a.x, kBossPaceMin, kBossPaceMax);
            if (b.x >= static_cast<float>(kBossPaceBars) - 0.5f) {
                s.paceMeasured = a.y;
                s.paceSet = true;
            }
            break;
        case kGeomWeaverCage:  // M5 part 6: a new cage (the last one's laps go off)
            if (m.cageTick >= 0.0 && tick > m.cageTick) {
                m.prevCage = m.cage;
                m.prevCageAng = m.cageAng;
            }
            m.cage = a;
            m.cageAng = b.x;
            m.cageTick = tick;
            break;
        case kGeomWellBar: {  // M5 part 6: Deep Well's bar line, as the host decided it
            if (a.y > 0.5f) {
                m.chokeTick = tick;
                m.ringLeft = 0;
                emitCue(BossCue::DwChoke, tick, 0, 0.0f, s.body[0]);
                emitCue(BossCue::DwLow, tick, 1, 0.0f, s.body[0]);
                break;
            }
            // (0449eb3: the Track's count, the ring thinned evenly: the same on every device from the host's bar)
            const u32 n = trackCount(static_cast<u32>(std::fmin(std::floor(a.x / prose::kDwPerBullet), static_cast<float>(prose::kDwRingMax))), 4u,
                                     volleyIndex(tick, kTicksPerBar), 0u);
            if (n == 0) break;
            const u32 now = m.half ? n / 2u : n;
            m.ringLeft = static_cast<u16>(n - now);
            const float a0 = static_cast<float>(hash32(static_cast<u32>(tick), 0xD3E0u) % 360u) * kTau / 360.0f;
            const bool was = inBossUpdate_;
            inBossUpdate_ = true;  // (on a client the host's word comes outside updateBoss: still the fight's own)
            for (u32 j = 0; j < now; ++j) {
                const float ang = a0 + kTau * static_cast<float>(j) / static_cast<float>(now ? now : 1u);
                spawnBullet(s.body[0] + dirOf(ang) * 70.0f, dirOf(ang) * prose::kDwRingSpeed, tickAgeOf(tick), 0.0f);
            }
            inBossUpdate_ = was;
            emitCue(BossCue::DwRing, tick, 0, 0.0f, s.body[0]);
            break;
        }
        case kGeomHive0:
        case kGeomHive1: {
            const u32 h = geom - kGeomHive0;
            m.shapeAt[h] = a;
            m.shapeDir[h] = normalizeOr(b, Vec2{1.0f, 0.0f});
            m.gap[h] = angleOf(m.shapeDir[h]);
            break;
        }
        case kGeomSpear0:
        case kGeomSpear1: {
            const u32 h = geom - kGeomSpear0;
            m.shapeAt[h] = a;
            m.shapeDir[h] = normalizeOr(b, m.shapeDir[h]);
            break;
        }
        case kGeomAlphaLock:
        case kGeomRamLock:
            m.lockAt = a;
            m.lockDir = normalizeOr(b, m.lockDir);
            break;
        case kGeomLunge: s.lw.lungeDir = normalizeOr(b, s.lw.lungeDir); break;
        default: break;
    }
    // M4 net follow-up (docs/PROGRESS_m4_net.md): the host's word reaches a client after the moment it was made. A Spear
    // moves by charging on beats 1 and 3 of the bar (the host integrates it), so one that charged since then is carried on
    // by the charge it made, instead of starting it again from where it stood (its darts were 61 wu RMS off on the soak's
    // default seed). Clients only: the host and solo play are unchanged.
    if (host && role_ == NetRole::Client && geom <= kGeomSpear1) {  // (the Hive's four: its shapes and its Spear's locks)
        const u32 h = (geom == kGeomHive1 || geom == kGeomSpear1) ? 1u : 0u;
        if (m.shape[h] == 1) {
            double charged = 0.0;
            const i64 k0 = static_cast<i64>(std::floor(tick / kTicksPerBeat)), k1 = static_cast<i64>(std::floor(m1_ / kTicksPerBeat));
            for (i64 k = k0; k <= k1 && k - k0 < 16; ++k) {
                const i64 b4 = ((k % 4) + 4) % 4;
                if (b4 != 0 && b4 != 2) continue;
                const double from = std::fmax(tick, static_cast<double>(k) * kTicksPerBeat);
                const double to = std::fmin(m1_, static_cast<double>(k + 1) * kTicksPerBeat);
                if (to > from) charged += to - from;
            }
            if (charged > 0.0)
                m.shapeAt[h] = m.shapeAt[h] + m.shapeDir[h] * (prose::kHiveSpearSpeed * static_cast<float>(charged / static_cast<double>(ticksPerSecond())));
        }
    }
}

// ---------------------------------------------------------------------------------------------- Kaleidos

void World::bossFire(double t) {
    BossState& s = boss_;
    const BossData& b = species_->boss;
    const BossData::Phase& ph = b.phases[s.phase < 4 ? s.phase : 3];
    const float adv = tickAgeOf(t);
    const float wheel = wheelAt(s, b, t);
    const u8 mask = bossFiringMask(t);
    if (mask) {
        const float spread = ph.spreadDeg * kDeg;
        for (u32 k = 0; k < 6; ++k) {
            if (!(mask & (1u << (k % 3)))) continue;
            const float a = wheel + static_cast<float>(k) * kArmStep;
            const Vec2 tip = s.pos + dirOf(a) * b.tip.armRadius;
            // (0449eb3: the Track's count on top of the lever; a tip's single shot every second volley or three in four)
            const u32 fan = bossCount(ph.fan ? ph.fan : 1u, 1u, volleyIndex(t, ph.fireStepTicks), k);
            if (!fan) continue;
            for (u32 j = 0; j < fan; ++j) {
                const float da = (static_cast<float>(j) - 0.5f * static_cast<float>(fan - 1)) * spread;
                spawnBullet(tip, dirOf(a + da) * b.bulletSpeed, adv, b.bulletLife);
            }
        }
    }
    // The core: Spiral adds a ring of 24 on beat 1; the naked core fires 16 every beat (every eighth from
    // its ninth bar), alternate rings turned half a step.
    if (ph.coreBurst) {
        const double inBar = t - barStartOf(t);
        bool fire = false;
        i64 volley = 0;  // (0449eb3: the burst's index among its own)
        if (s.phase == 2) {
            fire = inBar < 0.5;
            volley = volleyIndex(t, kTicksPerBar);
        } else {
            const u32 bars = bossFightBar(t) >= s.phaseBar0 ? bossFightBar(t) - s.phaseBar0 : 0;
            const double every = bars >= 8 ? 2.0 * kTicksPerStep : kTicksPerBeat;
            const double in = std::fmod(t, every);
            fire = t >= s.phaseTick && (in < 0.5 || every - in < 0.5);
            volley = volleyIndex(t, every);
        }
        if (fire) {
            const u32 index = static_cast<u32>(std::floor(t / (2.0 * kTicksPerStep) + 0.5));
            const u32 n = bossCount(ph.coreBurst, 4u, volley, 0u);  // (0449eb3: the ring thinned evenly)
            const float turn = (index & 1u) ? 0.5f * kTau / static_cast<float>(n) : 0.0f;
            for (u32 j = 0; j < n; ++j) {
                const float a = wheel + turn + kTau * static_cast<float>(j) / static_cast<float>(n);
                spawnBullet(s.pos + dirOf(a) * b.coreRingRadius[0], dirOf(a) * b.bulletSpeed, adv, b.bulletLife);
            }
            Event ev;
            ev.type = EventType::BossBurst;
            ev.b = static_cast<u8>(n);
            ev.x = s.pos.x;
            ev.y = s.pos.y;
            emit(ev);
        }
    }
}

void World::bossHitCue(bool weak, Vec2 at, u8 owner) {
    // GAME_DESIGN 7.6 (play test 3): every hit on a boss is heard, by the shooter's device: a bright tick on a weak point, a
    // dull knock on armour. Counted on the 1/32 step like kills: at most one of each a step; each further step of the same
    // beat at 0.7 of the one before, down to 0.3 (the cue's f).
    if (owner < kMaxPlayers && !isLocal(owner)) return;
    const u32 k = weak ? 0u : 1u;
    const double step = 0.5 * kTicksPerStep;
    const double slot = std::floor(m1_ / step) * step;
    if (slot <= hitCueTick_[k]) return;
    const bool sameBeat = std::floor(slot / kTicksPerBeat) == std::floor(hitCueTick_[k] / kTicksPerBeat);
    hitCueGain_[k] = sameBeat ? std::fmax(hitCueGain_[k] * 0.7f, 0.3f) : 1.0f;
    hitCueTick_[k] = slot;
    emitCue(weak ? BossCue::HitWeak : BossCue::HitArmour, m1_, 0, hitCueGain_[k], at);
}

void World::emitCue(BossCue c, double tick, u8 b, float f, Vec2 at) {
    Event ev;
    ev.type = EventType::BossCue;
    ev.a = static_cast<u8>(c);
    ev.b = b;
    ev.f = f;
    ev.x = at.x;
    ev.y = at.y;
    ev.tick = tick;
    ev.age = tick <= m1_ ? tickAgeOf(tick) : 0.0f;
    emit(ev);
}

bool World::bossBeamsHit(Vec2 at, Vec2& from) const {
    if (boss_.active && !boss_.dying && boss_.kind == BossKind::EventHorizon && horizonJetHits(at, from)) return true;  // M5 part 4: the jets
    // What a fight adds to its bodies' contact: the Choir's burning cage and held notes, the Ram's wake.
    switch (boss_.kind) {
        case BossKind::Choir:
            from = boss_.ch.centre;
            return choirBeamsHit(at);
        case BossKind::BatteringRam:
            from = boss_.body[0];
            return ramWakeHit(at);
        case BossKind::Feedback: return feedbackHits(at, from);  // its Echoes' bodies, the Nulls, the burning Loop
        case BossKind::BlinkStar: return blinkBeamHit(at, from);  // M5 part 4: the star fires
        case BossKind::Arpeggiator: return arpBlockHit(at, from);  // M5 part 6: the blocks
        case BossKind::Reprise: return repriseHits(at, from);       // M5 part 6: Feedback's Echo
        case BossKind::MirrorMimic:
        case BossKind::TwinTether:
        case BossKind::WeaverPrime: return mini3Hits(at, from);     // M5 part 6: the mirror point, the rope, the cage
        case BossKind::Polyrhythm: {
            // The walls, and the link while it burns.
            if (polyWallHit(at, from)) return true;
            if (polyLinkBurn(m1_) != 2) return false;
            const Vec2 a = boss_.body[0], d = boss_.body[1] - boss_.body[0];
            const float l2 = lengthSq(d);
            const float u = l2 > 1e-6f ? clampf(dot(at - a, d) / l2, 0.0f, 1.0f) : 0.0f;
            from = a + d * u;
            return lengthSq(at - from) <= (kPrLinkBurnHalfWu + kPlayerHitRadius) * (kPrLinkBurnHalfWu + kPlayerHitRadius);
        }
        default: return false;
    }
}

void World::bossHitPlayer(u32 p, Vec2 from, bool beam) {
    if (haloBlocks(p, from)) return;
    ++boss_.hitsTaken;
    ++(beam ? boss_.hitsBeam : boss_.hitsBody);
    hitPlayer(p, from, static_cast<u16>(kCauseBossBase + static_cast<u32>(boss_.kind)));
}

void World::bossContact() {
    const BossState& s = boss_;
    if (!s.active || s.dying || m1_ < s.fightTick || rules_.godMode) return;
    if (s.kind != BossKind::Kaleidos) {
        // The bodies hurt (a part's circle shrunk like every enemy's, kContactShrink), and what each fight adds (beams,
        // a burning cage, a wake).
        BossPart parts[kBossBodies + 4];
        const u32 n = bossParts(parts, kBossBodies + 4);
        for (u32 p = 0; p < playerCount_; ++p) {
            PlayerState& pl = players_[p];
            if (!pl.alive || pl.invulnerable() || !isLocal(p) || sheltered(p)) continue;
            bool hit = false;
            Vec2 from = s.pos;
            for (u32 k = 0; k < n && !hit; ++k) {
                if (parts[k].part == 1 && s.kind == BossKind::Hive) continue;  // the decoy is light
                if (s.kind == BossKind::Choir) {
                    // choir.json numbers.contact.hoodHurts: a silent hood is flown through unharmed (the lead, 94706dc).
                    const ChoirData::HoodHurts h = species_->choir.hoodHurts;
                    if (h == ChoirData::HoodHurts::Never || (h == ChoirData::HoodHurts::Singing && !choirSinging(parts[k].index, m1_))) continue;
                }
                const float r = parts[k].radius * kContactShrink + kPlayerHitRadius;
                if (lengthSq(pl.pos - parts[k].pos) <= r * r) {
                    hit = true;
                    from = parts[k].pos;
                }
            }
            bool beam = false;
            if (!hit) hit = beam = bossBeamsHit(pl.pos, from);
            if (hit && beam && s.kind == BossKind::Polyrhythm) polyEraseHit(pl.pos);  // (a wall is erased where it hit)
            if (hit) bossHitPlayer(p, from, beam);
        }
        return;
    }
    const BossData& b = species_->boss;
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.alive || pl.invulnerable() || !isLocal(p) || sheltered(p)) continue;  // M4: what you see is what hits you
                                                                                      // (play test 2: a level-up orb shelters)
        const Vec2 rel = pl.pos - s.pos;
        bool hit = lengthSq(rel) <= (b.contactCore + kPlayerHitRadius) * (b.contactCore + kPlayerHitRadius);
        for (u32 k = 0; k < 6 && !hit; ++k) {
            const u32 pr = k % 3;
            if (!(s.alive & (1u << pr))) continue;
            const Vec2 d = dirOf(s.angle + static_cast<float>(k) * kArmStep);
            const float u = rel.x * d.x + rel.y * d.y;
            if (u < kBossArmInner || u > kBossArmOuter) continue;
            const float v = std::fabs(rel.x * d.y - rel.y * d.x);
            if (v <= armHalfWidth(b, u) * kContactShrink + kPlayerHitRadius) hit = true;
            // A firing beam along this arm, out to the arena wall.
            if (!hit && bossBeamState(pr, m1_) == 2 && u > b.tip.armRadius &&
                v <= 0.5f * b.phases[1].beamWidth + kPlayerHitRadius)
                hit = true;
        }
        if (!hit) {
            // A beam reaches far past the wheel's radius.
            for (u32 k = 0; k < 6 && !hit; ++k) {
                const u32 pr = k % 3;
                if (bossBeamState(pr, m1_) != 2) continue;
                const Vec2 d = dirOf(s.angle + static_cast<float>(k) * kArmStep);
                const float u = rel.x * d.x + rel.y * d.y;
                const float v = std::fabs(rel.x * d.y - rel.y * d.x);
                if (u > b.tip.armRadius && v <= 0.5f * b.phases[1].beamWidth + kPlayerHitRadius) hit = true;
            }
        }
        if (hit) bossHitPlayer(p, s.pos);
    }
}

void World::updateBoss(float dt) {
    BossState& s = boss_;
    updatePen();
    if (!s.active) return;
    s.prevPos = s.pos;
    s.prevAngle = s.angle;
    for (u32 k = 0; k < kBossBodies; ++k) {
        s.bodyPrev[k] = s.body[k];
        s.bodyPrevAng[k] = s.bodyAng[k];
    }
    if (s.dying) {
        if (s.payoutTick >= 0.0 && m1_ >= s.payoutTick) {
            s.payoutTick = -1.0;
            bossPayout();
        }
        if (m1_ >= s.deathTick + s.deathBars * kTicksPerBar) s.active = false;
        return;
    }
    // M5 part 7 (GAME_DESIGN 7.6, ea472ab): on each bar line of a boss fight's first eight bars its pace: the players' damage a
    // player so far over the novice's eight-bar number in proportion to the bars gone (k/8 of it at bar k), held between 1 and
    // 4, a ratio under the dead band counting as 1; after the eighth it stands. The host's (or solo's) decision, sent; a client
    // waits for it.
    if (!isMiniBoss(s.kind) && !s.paceSet && role_ != NetRole::Client && m1_ >= s.fightTick) {
        forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
            const double k = std::floor((t - s.fightTick) / kTicksPerBar + 0.5);
            if (k < 1.0 || k > static_cast<double>(kBossPaceBars) || s.paceSet) return;
            float sum = 0.0f, aimed = 0.0f;
            for (u32 p = 0; p < kMaxPlayersBoss; ++p) {
                sum += s.paceDmg[p];
                aimed += s.paceAimed[p];
            }
            const u32 players = presentCount() ? presentCount() : 1u;
            const float per = sum / static_cast<float>(players);
            const float perAimed = aimed / static_cast<float>(players);
            const float novice = static_cast<u32>(s.kind) < 24u ? kBossPaceNovice[static_cast<u32>(s.kind)] : 0.0f;
            const float ratio = novice > 0.0f ? perAimed / (novice * static_cast<float>(k) / static_cast<float>(kBossPaceBars)) : 0.0f;
            const float pace = rules_.noPace || ratio < kBossPaceDeadBand ? 1.0f : clampf(ratio, kBossPaceMin, kBossPaceMax);
            bossDecide(kGeomBossPace, Vec2{pace, per}, Vec2{static_cast<float>(k), 0.0f}, t);
        });
    }
    if (scriptKillParts_ || scriptKillCore_) {
        for (u32 k = 0; k < kBossParts; ++k)
            if ((scriptKillParts_ & (1u << k)) && (s.alive & (1u << k))) killBossPart(static_cast<u8>(k), 0);
        if (scriptKillCore_ && s.active && !s.dying) killBoss(0);
        scriptKillParts_ = 0;
        scriptKillCore_ = false;
        if (s.dying || !s.active) return;
    }
    switch (s.kind) {
        case BossKind::Longwave: updateLongwave(dt); return;
        case BossKind::Choir: updateChoir(dt); return;
        case BossKind::EventHorizon:
            updateHorizon(dt);
            s.partHp[0] = static_cast<float>(s.hz.meal);  // (the heartbeat carries the meal: bossPartCount)
            return;
        case BossKind::Polyrhythm: updatePoly(dt); return;
        case BossKind::Feedback: updateFeedback(dt); return;
        case BossKind::Cadence: updateCadence(dt); return;
        case BossKind::Arpeggiator: updateArpeggiator(dt); return;
        case BossKind::Morgana: updateMorgana(dt); return;
        case BossKind::Reprise: updateReprise(dt); return;
        case BossKind::Kaleidos: break;
        default: updateMini(dt); return;
    }
    const BossData& b = species_->boss;
    s.angle = wheelAt(s, b, m1_);
    // Drift at 30 wu/s to stay 450-650 wu from the nearest player.
    i32 near = nearestPlayer(s.pos.x, s.pos.y);
    if (near >= 0) {
        const Vec2 to = players_[near].pos - s.pos;
        const float d = length(to);
        if (d > 1e-3f) {
            const Vec2 u = to * (1.0f / d);
            if (d < b.keepNear) s.pos = s.pos - u * (b.driftSpeed * dt);
            if (d > b.keepFar) s.pos = s.pos + u * (b.driftSpeed * dt);
        }
        const float m = b.radius + 60.0f;
        s.pos.x = clampf(s.pos.x, -kArenaHalfW + m, kArenaHalfW - m);
        s.pos.y = clampf(s.pos.y, -kArenaHalfH + m, kArenaHalfH - m);
    }
    s.body[0] = s.pos;
    s.bodyAng[0] = s.angle;
    if (m1_ < s.fightTick) return;
    forCrossings(m0_, m1_, kTicksPerStep, [&](double t, i64) {
        if (t >= s.fightTick) bossFire(t);
    });
    // Beams: announced at the start of their telegraph bar and when they fire.
    forCrossings(m0_, m1_, kTicksPerBar, [&](double t, i64) {
        for (u32 p = 0; p < 3; ++p) {
            const u8 st = bossBeamState(p, t + 1.0);
            if (!st) continue;
            Event ev;
            ev.type = EventType::BossBeam;
            ev.a = static_cast<u8>(p);
            ev.b = st == 2 ? 1 : 0;
            ev.tick = st == 2 ? t : t + kTicksPerBar;
            ev.x = s.pos.x;
            ev.y = s.pos.y;
            emit(ev);
        }
    });
}

}  // namespace rf::sim
