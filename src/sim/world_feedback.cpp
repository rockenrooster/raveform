// M5 part 4: Feedback, the boss of Track 6 (design/bosses/feedback.md, .json). A loudspeaker inside a loop of tape with no
// attack of its own: it records each player's bar and an Echo plays it back a bar later from the mirror point, the same
// shots aimed the other way. The Head can be hurt only while every Echo is silent. Regeneration adds a second Echo two
// bars behind at the player's own old place; Howl runs the tape double speed and burns the Loop.
//
// What the Echo plays back: the ship's path, every shot the player's ship and Riffs fired (where from and which way, so
// the Mods' lanes and spreads come back), and its Novas (as Nulls). Riffs that fire nothing (Orbitals, Pulse Ring,
// Sweeper) and what shots do after they leave (Pierce, Split, Ricochet, homing) are not echoed: listed in PROGRESS.
//
// Co-op (feedback.md): each device records every player as it sees them (its own exactly) and spawns the echo shots
// itself; a hit is decided by the device that owns the player, as for every bullet. The health is the host's.

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr double kStep = kTicksPerStep;
}

u32 World::feedbackEchoes() const { return boss_.fb.phase == 0 ? 1u : 2u; }

double World::feedbackDelay(u32 echo) const {
    // Slapback and Regeneration: one bar and two; Howl (the tape at double speed): two beats and four.
    if (boss_.fb.phase == 2) return (echo == 0 ? 2.0 : 4.0) * kTicksPerBeat;
    return (echo == 0 ? 1.0 : 2.0) * kTicksPerBar;
}

bool World::feedbackPose(u32 p, double t, Vec2& rel, Vec2& aim, bool& alive) const {
    // The pose recorded at song time t (between two ticks: the later one).
    if (p >= kMaxPlayers) return false;
    const FeedbackTape::Lane& l = tape_.lanes[p];
    if (!l.poseCount) return false;
    for (u32 k = 0; k < l.poseCount; ++k) {
        const u32 i = (l.poseHead + FeedbackTape::kPoses - 1 - k) % FeedbackTape::kPoses;
        const FeedbackTape::Pose& q = l.poses[i];
        if (q.tick > t) continue;
        if (k == 0) {
            rel = q.rel;
            aim = q.aim;
            alive = q.alive != 0;
            return true;
        }
        // Between q and the one after it.
        const FeedbackTape::Pose& n = l.poses[(i + 1) % FeedbackTape::kPoses];
        const float u = n.tick > q.tick ? static_cast<float>((t - q.tick) / (n.tick - q.tick)) : 0.0f;
        rel = q.rel + (n.rel - q.rel) * clampf(u, 0.0f, 1.0f);
        aim = q.aim;
        alive = q.alive != 0;
        return true;
    }
    return false;  // older than the tape
}

float World::feedbackNullRadius(const FeedbackTape::Null& n, double t) const {
    // A Null grows as a Nova's front does (to its reach over its time).
    const float s = static_cast<float>((t - n.tick) / ticksPerSecond());
    if (s < 0.0f || s > kNovaFrontS) return -1.0f;
    return kNovaFrontWu * s / kNovaFrontS;
}

bool World::feedbackHits(Vec2 at, Vec2& from) const {
    // An Echo's body, a Null's front passing, and in Howl the burning Loop.
    const BossState& s = boss_;
    const FeedbackData& d = species_->feedback;
    const FeedbackState& f = s.fb;
    for (u32 e = 0; e < FeedbackState::kEchoes; ++e) {
        if (!(f.active & (1u << e))) continue;
        const float r = d.echoRadius + kPlayerHitRadius;
        if (lengthSq(at - s.body[1 + e]) <= r * r) {
            from = s.body[1 + e];
            return true;
        }
    }
    for (const FeedbackTape::Null& n : tape_.nulls) {
        const float r0 = feedbackNullRadius(n, m0_), r1 = feedbackNullRadius(n, m1_);
        if (r1 < 0.0f) continue;
        const float dist = length(at - n.at);
        if (dist >= (r0 < 0.0f ? 0.0f : r0) - prose::kFbNullBand && dist <= r1 + kPlayerHitRadius) {
            from = n.at;
            return true;
        }
    }
    if (f.phase == 2) {
        const float dist = length(at - s.pos);
        if (std::fabs(dist - f.loopR) <= 0.5f * d.loopBand + kPlayerHitRadius) {
            from = s.pos + normalizeOr(at - s.pos, Vec2{1.0f, 0.0f}) * f.loopR;
            return true;
        }
    }
    return false;
}

float World::feedbackScale() const {
    // Open (x 1) only while every Echo is silent; in co-op x the share of the living players' Echoes that are silent.
    const FeedbackData& d = species_->feedback;
    return d.dmgClosed + (d.dmgOpen - d.dmgClosed) * boss_.fb.open;
}

void World::feedbackSwallow() {
    // A shot swallowed by the closed Head: a closed hat (at most one a sixteenth).
    FeedbackState& f = boss_.fb;
    if (m1_ - f.lastSwallow < kStep) return;
    f.lastSwallow = m1_;
    emitCue(BossCue::FbSwallow, m1_, 0, 0.0f, boss_.pos);
}

bool World::spawnEchoShot(Vec2 pos, Vec2 vel, float advanceS, float lifeS, u8 player) {
    if (!spawnBullet(pos, vel, advanceS, lifeS)) return false;
    bullets_.echo[bullets_.count - 1] = static_cast<u8>(1u + player);
    return true;
}

void World::enterFeedback(Vec2 centre) {
    BossState& s = boss_;
    const FeedbackData& d = species_->feedback;
    // The music cuts for a beat; the Loop draws itself over the next bar and the Head assembles; recording starts on bar 3,
    // the first Echo plays on bar 4.
    s.fightTick = s.enterTick + 2.0 * kTicksPerBar;
    s.phaseTick = s.fightTick;
    s.coreMax = d.health * s.scale;
    s.coreHp = s.coreMax;
    s.alive = 0;
    const float m = d.loopRadius + 300.0f;
    s.pos = s.prevPos = Vec2{clampf(centre.x, -kArenaHalfW + m, kArenaHalfW - m), clampf(centre.y, -kArenaHalfH + m, kArenaHalfH - m)};
    s.body[0] = s.bodyPrev[0] = s.pos;
    s.fb = FeedbackState{};
    s.fb.loopR = d.loopRadius;
    tape_.clear();
}

void World::feedbackRecord(u32 shot0) {
    // The tape: every present player's pose this tick, the shots it fired (its ship's and its Riffs'), its Novas.
    const BossState& s = boss_;
    if (m1_ < s.fightTick) return;
    const i64 bar = static_cast<i64>(std::floor(m1_ / kTicksPerBar));
    for (u32 p = 0; p < playerCount_; ++p) {
        const PlayerState& pl = players_[p];
        if (!pl.present) continue;
        FeedbackTape::Lane& l = tape_.lanes[p];
        FeedbackTape::Pose& q = l.poses[l.poseHead];
        q.tick = m1_;
        q.rel = pl.pos - s.pos;
        q.aim = dirOf(pl.heading);
        q.alive = pl.alive ? 1 : 0;
        l.poseHead = (l.poseHead + 1) % FeedbackTape::kPoses;
        if (l.poseCount < FeedbackTape::kPoses) ++l.poseCount;
        if (l.bar != bar) {
            l.bar = bar;
            l.barShots = 0;
        }
    }
    const Shots& sh = shots_;
    for (u32 i = shot0; i < sh.count; ++i) {
        const u32 p = sh.owner[i];
        if (p >= playerCount_ || !players_[p].present) continue;
        FeedbackTape::Lane& l = tape_.lanes[p];
        ++l.barShots;
        if (l.barShots > prose::kFbBarCap && (l.barShots & 1u)) continue;  // past 120 a bar: every second one left out
        FeedbackTape::Shot& r = l.shots[l.shotHead];
        r.tick = m1_;
        r.off = Vec2{sh.px[i], sh.py[i]} - players_[p].pos;
        r.dir = Vec2{sh.dx[i], sh.dy[i]};
        r.primary = sh.kind[i] == kShotPrimary ? 1 : 0;
        l.shotHead = (l.shotHead + 1) % FeedbackTape::kShots;
        if (l.shotCount < FeedbackTape::kShots) ++l.shotCount;
    }
    for (u32 k = 0; k < eventCount_; ++k) {
        const Event& ev = events_[k];
        if (ev.type != EventType::NovaBlast || ev.a >= playerCount_) continue;
        FeedbackTape::Lane& l = tape_.lanes[ev.a];
        l.novas[l.novaHead] = ev.tick > 0.0 ? ev.tick : m1_;
        l.novaHead = (l.novaHead + 1) % FeedbackTape::kNovas;
        if (l.novaCount < FeedbackTape::kNovas) ++l.novaCount;
    }
}

void World::updateFeedback(float dt) {
    BossState& s = boss_;
    FeedbackState& f = s.fb;
    const FeedbackData& d = species_->feedback;
    (void)dt;
    s.body[0] = s.pos;
    if (m1_ < s.fightTick) return;
    // The phase by health: Regeneration below 60%, Howl at 30% (the Loop burns and tightens 15 wu a bar line to 300).
    const float share = s.coreMax > 0.0f ? s.coreHp / s.coreMax : 0.0f;
    const u8 ph = share <= prose::kFbHowlAt ? 2 : (share <= prose::kFbRegenAt ? 1 : 0);
    if (ph > f.phase) {
        f.phase = ph;
        s.phase = ph;
        s.phaseTick = m1_;
        if (ph == 2) f.howlTick = m1_;
        Event ev;
        ev.type = EventType::BossPair;
        ev.a = ph;
        ev.c = static_cast<u8>(BossKind::Feedback);
        ev.x = s.pos.x;
        ev.y = s.pos.y;
        emit(ev);
    }
    if (f.phase == 2)
        forCrossings(m0_, m1_, kTicksPerBar, [&](double, i64) { f.loopR = std::fmax(f.loopR - d.howlShrink, d.howlMin); });
    // The Echoes: where each player was a bar ago (two for the second), reflected through the Head (the second reflected
    // twice: where the player stood), 220-900 wu from it; a downed player's Echoes fade a bar later (their tape says so).
    const u32 echoes = feedbackEchoes();
    f.active = 0;
    f.playing = 0;
    u32 activeN = 0, silentN = 0;
    u32 echoShots = 0;
    for (u32 i = 0; i < bullets_.count; ++i) echoShots += (!bullets_.dead[i] && bullets_.echo[i]) ? 1u : 0u;
    const float tps = ticksPerSecond();
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present) continue;
        const FeedbackTape::Lane& l = tape_.lanes[p];
        for (u32 e = 0; e < echoes; ++e) {
            const u32 slot = 2u * p + e;
            const double D = feedbackDelay(e);
            Vec2 rel, aim;
            bool alive = false;
            if (!feedbackPose(p, m1_ - D, rel, aim, alive) || !alive) continue;
            if (lengthSq(rel) < 1.0f) rel = Vec2{1.0f, 0.0f};
            const float dist = clampf(length(rel), prose::kFbMinWu, prose::kFbMaxWu);
            const Vec2 dir = normalizeOr(rel, Vec2{1.0f, 0.0f}) * (e == 0 ? -1.0f : 1.0f);
            const Vec2 at = s.pos + dir * dist;
            s.body[1 + slot] = at;
            s.bodyAng[1 + slot] = angleOf(e == 0 ? aim * -1.0f : aim);
            f.active = static_cast<u8>(f.active | (1u << slot));
            ++activeN;
            // It plays what was fired a delay ago (one echo shot for each, the cap of 500 alive), and counts as playing for
            // two sixteenths after each primary shot.
            bool playing = false, sounded = false;
            for (u32 k = 0; k < l.shotCount; ++k) {
                const FeedbackTape::Shot& r = l.shots[(l.shotHead + FeedbackTape::kShots - 1 - k) % FeedbackTape::kShots];
                const double when = r.tick + D;
                if (when <= m0_ - kTicksPerBar) break;  // (older: neither played nor counting)
                if (r.primary && when <= m1_ && when >= m1_ - prose::kFbPlayingSteps * kStep) playing = true;
                if (when <= m0_ || when > m1_) continue;
                if (echoShots >= prose::kFbShotCap) continue;
                const Vec2 off = e == 0 ? r.off * -1.0f : r.off, sd = e == 0 ? r.dir * -1.0f : r.dir;
                if (spawnEchoShot(at + off, sd * d.shotSpeed, static_cast<float>((m1_ - when) / tps), d.shotLife, static_cast<u8>(p))) ++echoShots;
                if (!sounded) {
                    sounded = true;
                    emitCue(BossCue::FbEcho, when, static_cast<u8>(p), static_cast<float>(e), at);
                }
            }
            if (playing) f.playing = static_cast<u8>(f.playing | (1u << slot));
            else ++silentN;
            // Its Novas come back as Nulls.
            for (u32 k = 0; k < l.novaCount; ++k) {
                const double when = l.novas[k] + D;
                if (when <= m0_ || when > m1_) continue;
                FeedbackTape::Null& n = tape_.nulls[tape_.nullHead];
                n.tick = when;
                n.at = at;
                tape_.nullHead = (tape_.nullHead + 1) % FeedbackTape::kNulls;
                emitCue(BossCue::FbNull, when, static_cast<u8>(p), 0.0f, at);
            }
        }
    }
    f.open = activeN ? static_cast<float>(silentN) / static_cast<float>(activeN) : 1.0f;
    const bool open = f.open >= 0.999f;
    if (open && !f.wasOpen) {
        // The Head opens: it holds the Track's root two octaves up, a semitone higher for every tenth of its health lost.
        emitCue(BossCue::FbOpen, m1_, 0, std::floor((1.0f - share) * 10.0f), s.pos);
    }
    f.wasOpen = open;
}

}  // namespace rf::sim
