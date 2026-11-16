#include "sim/world.h"

#include <cmath>
#include <cstring>
#include <initializer_list>

#include "core/lead_rule.h"

#include "sim/tuning.h"

namespace rf::sim {

using namespace tuning;

namespace {
constexpr float kMaxSpeciesRadius = 40.0f;  // larger than any species' radius (ram V: 18 x 1.3 = 23.4); M5 part 5: elites grow past it (maxEnemyR_)
constexpr float kGridSlack = 32.0f;       // enemies move at most ~25 wu a tick after the grid was built

bool insideArena(float x, float y) { return std::fabs(x) <= kArenaHalfW && std::fabs(y) <= kArenaHalfH; }
}  // namespace

// ---------------------------------------------------------------------------------------------- set-up

void World::init(const SpeciesTable* species) {
    species_ = species;
    std::memset(&enemies_, 0, sizeof enemies_);
    std::memset(&bullets_, 0, sizeof bullets_);
    std::memset(&shots_, 0, sizeof shots_);
    std::memset(&shards_, 0, sizeof shards_);
    std::memset(events_, 0, sizeof events_);
    std::memset(idKey_, 0, sizeof idKey_);
    netLog_.clear();
    netOpCount_ = 0;
    runId_ = nextRunId_ = 0;
    predictUndoS_ = kPredictUndoS;
    tick_ = 0;
    resetRun(1, 0.0, 1);
}

void World::resetRun(u64 seed, double startTick, u32 players, u8 presentMask) {
    seed_ = seed;
    rng_.reseed(seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull);
    Enemies& e = enemies_;
    // Free every slot; generations keep counting, so a handle from the last run stays stale.
    e.count = 0;
    e.freeCount = kMaxEnemies;
    for (u32 s = 0; s < kMaxEnemies; ++s) e.freeSlots[s] = kMaxEnemies - 1 - s;  // slot 0 is taken first
    bullets_.count = shots_.count = shards_.count = 0;
    for (u32& n : shotsLive_) n = 0;
    for (bool& b : thinning_) b = false;
    for (Trail& t : trails_) t.n = 0;  // M5 part 4
    for (Spot& m : emines_) m.until = -1.0;
    for (Spot& d : decoys_) d.until = -1.0;
    serial_ = 0;
    dipS_ = 0.0f;
    allDownDipS_ = 0.0f;
    threatInView_ = 0.0f;
    crowdInFrame_ = 0;
    crowdGoal_ = 0.0f;
    restartPending_ = false;
    hitCueTick_[0] = hitCueTick_[1] = -1e18;  // play test 3
    arcing_ = 0;
    m0_ = m1_ = startTick;
    playerCount_ = players < 1 ? 1 : (players > kMaxPlayers ? kMaxPlayers : players);
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        PlayerState& pl = players_[p];
        pl = PlayerState{};
        pl.present = p < playerCount_ && ((presentMask >> p) & 1u) != 0;
        pl.alive = pl.present;
        pl.pos = pl.prevPos = Vec2{static_cast<float>(p) * 60.0f, 0.0f};
    }
    const Spawner::Mode mode = spawner_.mode;
    const bool trios = spawner_.trios;
    spawner_ = Spawner{};
    spawner_.mode = mode;
    spawner_.trios = trios;
    spawner_.startTick = startTick;
    // M3 stage B: the director starts the run on Track 1's first Groove; no Trio, Boost or boss yet.
    upgradeRng_.reseed(seed * 0xD1B54A32D192ED03ull + 0x8CB92BA72F3D8DD7ull);
    director_ = DirectorState{};
    director_.songs = setList_;  // M5: the set list outlives the run
    director_.startTick = startTick;
    director_.lastBar = kNoBar;
    director_.boostTicks = static_cast<i32>(rng_.range(kBoostEveryLoS, kBoostEveryHiS) / kDt);
    phrase_ = phraseAt(director_, 0);
    for (TrioState& t : trio_) t = TrioState{};
    boostPickups_.count = 0;
    cellPickups_.count = 0;
    boss_ = BossState{};
    pen_ = PenState{};
    // M5 part 5: no structure, event, bomb or meteor yet; the arena's own stream (the host's placements and draws).
    for (ArenaBody& b : arena_) b = ArenaBody{};
    arenaEvent_ = ArenaEventState{};
    for (ArenaMeteor& m : meteors_) m = ArenaMeteor{};
    for (ArenaBomb& b : bombs_) b = ArenaBomb{};
    lastEvent_ = kNoEvent;
    eliteClock_ = 0.0f;
    commanderCount_ = 0;
    arenaRng_ = hash32(static_cast<u32>(seed ^ (seed >> 32)), 0xA4E7Au) | 1u;
    rules_.scoreMultiplier = rules_.magnetMultiplier = 1.0f;
    // M4: the co-op bookkeeping (nothing in solo reads it).
    restartSeed_ = seed;
    restartPlayers_ = playerCount_;
    formationTurn_ = 0;
    runTicks_ = 0;
    if (role_ != NetRole::Solo) std::memset(idKey_, 0, sizeof idKey_);
    viewPlayer_ = 0;
    for (u32 p = 0; p < kMaxPlayers; ++p)
        if (isLocal(p)) {
            viewPlayer_ = p;
            break;
        }
}

// ---------------------------------------------------------------------------------------------- tick

void World::tick(const TickInput& in) {
    ++tick_;
    ++runTicks_;
    eventCount_ = 0;
    if (in.retry && !restartPending_) requestRestart();
    const double prev = m1_;
    double next = in.music1 > prev ? in.music1 : prev;
    restarted_ = false;
    if (restartPending_ && next >= restartAt_) {
        // RETRY: the new run begins on this bar line (the poses of this tick still describe the old run, so
        // they are not applied; the controllers restart from the world's start positions). Solo: the seed
        // after this one; co-op: the host's seed and seats (scheduleRun).
        const double bar = restartAt_;
        resetRun(restartSeed_, bar, restartPlayers_, restartMask_);
        runId_ = nextRunId_;
        restarted_ = true;
        ++counters_.restarts;
        Event ev;
        ev.type = EventType::Restart;
        ev.tick = bar;
        emit(ev);
    }
    m0_ = m1_;
    m1_ = next;
    bpm_ = in.bpm > 1.0f ? in.bpm : 120.0f;
    latencyTicks_ = in.latencyTicks;

    if (netOpCount_) applyNetOps();  // M4: what the network brought since the last tick
    applyPlayers(in);
    if (role_ != NetRole::Solo) updateDowned();
    runSpawner();  // the director first: the phrase this tick plays in (Surge, the Build's last bar)
    // The loop's ramps this tick (GAME_DESIGN 8, "The loop is where a run ends"): 1 through Track 3.
    loopSpeed_ = std::fmin(loopRamp(kLoopEnemySpeedRamp, phrase_.track, director_.songs), kLoopEnemySpeedMax);
    loopBulletSpeed_ = std::fmin(loopRamp(kLoopBulletSpeedRamp, phrase_.track, director_.songs), kLoopBulletSpeedMax);
    loopShooters_ = std::fmin(loopRamp(kLoopShooterRamp, phrase_.track, director_.songs), kLoopShooterMax);
    const float dt = kDt * timeScale();
    if (dipS_ > 0.0f) dipS_ -= kDt;
    // M5 part 3: Time Stretch (GAME_DESIGN 6.4) halves the enemies' and their bullets' speed while any player has it.
    stretch_ = 1.0f;
    for (u32 p = 0; p < playerCount_; ++p)
        if (players_[p].present && players_[p].boost(kTimeStretch)) stretch_ = kTimeStretchSpeed;
    fxThisTick_ = 0;
    flungMask_ = 0;

    // The grid holds the solid enemies as the tick begins (indices stay valid until compact()); flocking
    // and the collisions below query it with slack for the movement since. M5: built before the shots, so a volley's
    // lines (Accent, Beam) can ask it; the bodies a Nova kills below stay in it, skipped as dead.
    maxEnemyR_ = kMaxSpeciesRadius;  // M5 part 5: the queries' reach covers the largest body (a Giant elite)
    for (u32 i = 0; i < kMaxEnemies; ++i) held_[i] = 0;  // (pullBodies: each body's own motion undone at most once a tick)
    for (u32 i = 0; i < enemies_.count; ++i) {
        solid_[i] = (enemies_.flags[i] & kFlagDead) == 0 && enemies_.state[i] != kWarping;
        if (enemies_.radius[i] > maxEnemyR_) maxEnemyR_ = enemies_.radius[i];
    }
    grid_.build(enemies_.x, enemies_.y, enemies_.count, solid_);
    buildGroups();  // M5 part 4: Wyrms' chains, Tethers' pairs
    findWells();

    fbShot0_ = shots_.count;  // (M5 part 4: Feedback records what this tick fires)
    updateThinning();         // M5 (GAME_DESIGN 6.6): on a beat
    spawnShots(in);
    fireEchoes();  // M5: Echo's ghost volleys
    updateNovas();

    updateElites(dt * stretch_);  // M5 part 5: shields, Regenerating, Twins, the Commanders' reach (before the motion)
    updateEnemies(dt * stretch_);
    bulletSrc_ = kCauseNone;  // (M6: what fires outside an enemy's own update names nothing)
    updateTrails();  // M5 part 4: Weaver trails, Ram B's wake
    inBossUpdate_ = true;
    updateBoss(dt);
    inBossUpdate_ = false;
    bossPhasePay();  // M6 (GAME_DESIGN 5): after the fight's own update and the host's verdicts
    updateRiffs(dt);
    updateRiffsM5(dt);     // M5 part 3: Riffs 4-12
    updateUpgradesM5(dt);  // M5 part 3: the bar's and the beat's Mods, the Boosts that act every tick
    updateTrios();
    shelterTrios();  // play test 2: bodies touching a level-up orb's ring are shoved off before the contacts
    updateBoosts();
    updateCells(dt);
    moveProjectiles(dt);
    fenceShots();    // M5 part 4: the Sentry Array's fence stops shots
    aegisBullets();  // M5: Aegis's arc stops the bullets that reach it
    holdShards(dt);  // M5 part 5: Magnetic elites
    updateArena(dt); // M5 part 5: structures and events (Lenses, Currents and Bumpers on this tick's paths)
    collide();
    if (scriptHits_) {
        // (tests, pictures: a hit asked for from outside the tick lands inside it, so its events go out like any)
        for (u32 p = 0; p < playerCount_; ++p) {
            PlayerState& pl = players_[p];
            if (!((scriptHits_ >> p) & 1u) || !pl.present || !pl.alive || pl.down) continue;
            const bool autoNova = rules_.autoNova;
            rules_.autoNova = false;
            hitPlayer(p, pl.pos);
            rules_.autoNova = autoNova;
        }
        scriptHits_ = 0;
    }
    bossContact();
    if (boss_.active && !boss_.dying && (boss_.kind == BossKind::Feedback || boss_.kind == BossKind::Reprise)) feedbackRecord(fbShot0_);  // (M5 part 6: the Reprise's Echo)  // M5 part 4: the tape
    if (role_ == NetRole::Client) updatePredictions();
    if (role_ == NetRole::Host) finishNetLog();
    compact();
}

void World::setSetList(u32 songs) {
    // GAME_DESIGN 4.2: the loop begins after the last Track of the set list (from the next run on).
    setList_ = static_cast<u8>(songs < 1 ? 1u : (songs > kMaxSongs ? kMaxSongs : songs));
    director_.songs = setList_;
}

void World::requestRestartAt(double barTick) {
    requestRestart();
    if (barTick > m1_) restartAt_ = barTick;
}

void World::requestRestart() {
    if (restartPending_) return;
    restartPending_ = true;
    restartAt_ = (std::floor((m1_ + 0.5 * kTicksPerBar) / kTicksPerBar) + 1.0) * kTicksPerBar;
    restartSeed_ = seed_ + 0x9E3779B97F4A7C15ull;
    restartPlayers_ = playerCount_;
    restartMask_ = 0;
    for (u32 p = 0; p < playerCount_; ++p)
        if (players_[p].present || playerCount_ == 1) restartMask_ = static_cast<u8>(restartMask_ | (1u << p));
    nextRunId_ = static_cast<u16>(runId_ + 1);
}

void World::emit(const Event& e) {
    if (eventCount_ >= kMaxEvents) {
        ++counters_.eventsDropped;
        return;
    }
    events_[eventCount_++] = e;
}

float World::tickAgeOf(double tick) const {
    const double s = (m1_ - tick) / static_cast<double>(ticksPerSecond());
    return s > 0.0 ? static_cast<float>(s) : 0.0f;
}

// ---------------------------------------------------------------------------------------------- players

void World::applyPlayers(const TickInput& in) {
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        const PlayerPose& ps = in.players[p];
        pl.prevPos = pl.pos;
        pl.prevHeading = pl.heading;
        if (pl.invulS > 0.0f) pl.invulS -= kDt;
        if (restarted_) continue;
        if (!pl.alive || !ps.present) {
            pl.fire = 0;
            pl.dashing = false;
            pl.vel = Vec2{};
            continue;
        }
        pl.pos = Vec2{clampf(ps.pos.x, -kArenaHalfW, kArenaHalfW), clampf(ps.pos.y, -kArenaHalfH, kArenaHalfH)};
        pl.vel = ps.vel;
        pl.aim = ps.aim;
        pl.heading = ps.heading;
        pl.fire = ps.fire;
        pl.dashing = ps.dashing;
        if (ps.novaPressed) pressNova(p, false, true);
        if (ps.dashStarted) {
            ++pl.dashes;
            if (ps.perfectDash) ++pl.perfectDashes;
            dashExtras(p, ps.dashFrom, ps.dashTo, ps.perfectDash);  // M5: Afterimage, Shockstep
            pl.lastDashFrom = ps.dashFrom;  // M5 part 4: Mimic C
            pl.lastDashTo = ps.dashTo;
            pl.lastDashTick = m1_;
            Event ev;
            ev.type = EventType::Dash;
            ev.a = static_cast<u8>(p);
            ev.b = ps.perfectDash ? 1 : 0;
            ev.x = ps.dashFrom.x;
            ev.y = ps.dashFrom.y;
            ev.x2 = ps.dashTo.x;
            ev.y2 = ps.dashTo.y;
            emit(ev);
        }
    }
}

bool World::pressNova(u32 p, bool free, bool pressed) {
    PlayerState& pl = players_[p];
    if (!rules_.novas || (pl.novas == 0 && !free) || pl.novaPending) return false;
    // R6: invulnerable at once; the blast lands on the next beat. The band plays the inhale from the press and the
    // blast on a beat of its write time, which is ahead of the heard time by the output latency, and moves to the
    // following beat when the next one is closer than six ticks (engine.cpp, CmdType::Nova): the same rule here, so
    // picture and sound land together.
    if (!free) {
        --pl.novas;
        ++pl.novasUsed;
    }
    pl.novaPending = true;
    const double earliest = m1_ + static_cast<double>(latencyTicks_) + 6.0;
    pl.novaBlastTick = std::ceil(earliest / kTicksPerBeat) * kTicksPerBeat;
    const bool hole = blackHoleNova(p);  // M5: Black Hole makes it a three-beat singularity (its blast later)
    // The review's fixes (H5, 2026-11-15): Feedback's tape records only the Novas the player pressed (Auto Nova's, an Extra
    // Life's and Black Hole's are not recorded); a teammate's flag travels with its Nova event (Session: MsgPlayerEvent b).
    pl.novaPressed = pressed && !hole;
    pl.novaInvulS = 1e9f;
    Event ev;
    ev.type = EventType::NovaPress;
    ev.a = static_cast<u8>(p);
    ev.b = pl.novaPressed ? 1 : 0;
    ev.c = free ? 1 : 0;  // (the review's fixes, M45: a free Nova, an Extra Life's or Black Hole's own: the view's haptic skips it)
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    ev.tick = pl.novaBlastTick;
    emit(ev);
    return true;
}

void World::spawnShots(const TickInput& in) {
    for (u32 p = 0; p < playerCount_; ++p) {
        const PlayerState& pl = players_[p];
        if (!pl.alive) continue;
        // The band's lead rule is the local player's; another player's shots here are its fire flag on the
        // sim's grid (cosmetic copies, M4: the owner resolves its hits).
        if (in.bandShots && isLocal(p)) {
            // The band's lead rule: one volley for each heard grid step with fire held (TECH_SPEC 6.3).
            for (u32 k = 0; k < in.shotCount; ++k)
                if (in.shots[k].player == p) fireShot(p, in.shots[k].advanceS < 0.0f ? 0.0f : in.shots[k].advanceS, in.shots[k].tick);
        } else if (pl.fire) {
            // The same rule on the sim's own clock: a volley on every grid step crossed while fire is held
            // (1/16, moved along by Double-Time, the Surge's gear and Hyper), fewer on the assist's
            // (core/lead_rule.h: every other step and the off-beat's, one in four skipped on the 1/32 grid and faster); each
            // advanced by the time since its step. The band (Band::computeShots) and the engine's lead keep the same rule.
            const double grid = leadGrid(p);
            const i64 perBar = static_cast<i64>(kTicksPerBar / grid + 0.5);
            forCrossings(m0_, m1_, grid, [&](double t, i64 k) {
                if (pl.fire == 2 && assistSkipsStep(static_cast<u32>(grid + 0.5), static_cast<u32>(((k % perBar) + perBar) % perBar))) return;
                fireShot(p, tickAgeOf(t), t);
            });
        }
    }
}

void World::fireShot(u32 p, float advanceS, double stepTick) {
    PlayerState& pl = players_[p];
    const Loadout& l = pl.loadout;
    const Vec2 d = normalizeOr(pl.aim, dirOf(pl.heading));
    const Vec2 muzzle = pl.pos + d * kMuzzleWu;
    // The shot's damage and flags (Overcharge, On-Beat Crit; M5: Syncopation, Crescendo; world_upgrades_m5.cpp).
    u8 flags = 0, pierceAdd = 0;
    const float damage = shotDamage(p, stepTick, flags, pierceAdd);
    const bool prism = pl.boost(kPrism);
    if (prism) flags |= kShotPrism;
    // Split: a lane a stack in a tight fan (2, 3, 4, 5; eb10554); Prism: every lane refracts into five. While the player's
    // shots are thinned (GAME_DESIGN 6.6) the lanes beyond the middle three fly half as far; above kShotsMiddleOnly the
    // middle lane alone fires (an even fan: its middle pair; volley()) (tuning.h; the pool stays inside its 1,024 a player
    // with Split at its cap, Prism, Double-Time 2 and Hyper at 174 BPM, in the Build's slowed last bar too).
    const u32 lanes = splitLanes(l.mods[kSplit]);
    const u32 live = shotsLive_[p < kMaxPlayers ? p : 0];
    // M5 (GAME_DESIGN 6.6, thinning never flickers): while thinned the lanes beyond the middle three fly half as far.
    const u32 thin = lanes < 3 ? 0u : (live >= kShotsMiddleOnly ? 2u : (thinning_[p < kMaxPlayers ? p : 0] ? 1u : 0u));
    const i64 step = static_cast<i64>(std::floor(stepTick / leadGrid(p) + 0.5));
    const float base = angleOf(d);
    const u8 pierce = static_cast<u8>(pierceCount(l.mods[kPierce]) + pierceAdd);
    const u8 bounces = static_cast<u8>(ricochetBounces(l.mods[kRicochet]));
    ++pl.volleys;
    u32 made = 0;
    if (pl.boost(kBeam)) {
        // M5: Beam, the primary as a continuous laser (no shots).
        beamVolley(p, muzzle, d, damage, lanes, stepTick);
    } else {
        // M5: Accent's lance takes the middle lane's place on its beats; Heavy Step makes every fourth volley's an orb.
        const bool lance = accentLance(p, stepTick, muzzle, d, damage);
        const bool heavy = !lance && l.mods[kHeavyStep] && (pl.volleys % 4u) == 0u;
        // (Syncopation alone adds pierce in shotDamage: an off-beat volley; the view draws its shots accented)
        shotAccent_ = pierceAdd ? 1 : 0;
        made = volley(p, muzzle, base, advanceS, damage, flags, pierce, bounces, lanes, prism, thin, step, lance ? 1 : (heavy ? 2 : 0), 0);
        shotAccent_ = 0;
        volleyExtras(p, advanceS, stepTick, base, damage, flags, pierce, bounces, lanes, thin, step);
    }
    ++pl.shotsFired;
    Event ev;
    ev.type = EventType::Shot;
    ev.a = static_cast<u8>(p);
    ev.x = muzzle.x;
    ev.y = muzzle.y;
    ev.x2 = d.x;
    ev.y2 = d.y;
    ev.b = static_cast<u8>(made);
    ev.c = flags;
    ev.age = advanceS;
    emit(ev);
}

void World::updateNovas() {
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (pl.novaPending && m1_ >= pl.novaBlastTick) {
            pl.novaPending = false;
            pl.novaFrontAge = tickAgeOf(pl.novaBlastTick);
            pl.novaAt = pl.pos;
            pl.novaInvulS = kNovaFrontS + kNovaGraceS;
            Event ev;
            ev.type = EventType::NovaBlast;
            ev.a = static_cast<u8>(p);
            ev.b = pl.novaPressed ? 1 : 0;  // (the review's fixes, H5: a pressed Nova, the one Feedback's tape records)
            ev.x = pl.pos.x;
            ev.y = pl.pos.y;
            ev.age = pl.novaFrontAge;
            ev.tick = pl.novaBlastTick;
            emit(ev);
            // GAME_DESIGN 5: heavy damage to bosses (in view). Co-op: the Nova's owner deals it (a client claims it).
            // M5 part 4 (polyrhythm.md): a Nova erases every wall its front touches (on every device: it follows the Nova).
            if (boss_.active && !boss_.dying && boss_.kind == BossKind::Polyrhythm) polyErase(pl.pos, kNovaFrontWu);
            if (boss_.active && !boss_.dying && boss_.kind == BossKind::Arpeggiator) arpNovaClear();  // M5 part 6: a Nova clears the lanes
            bossNova(p);
            arenaNova(p);  // M5 part 5: a Vault in view
        } else if (!pl.novaPending && pl.novaInvulS > 0.0f) {
            pl.novaInvulS -= kDt;
        }
        if (pl.novaFrontAge < 0.0f) continue;
        // The front clears enemy bullets and non-boss enemies in view as it passes (GAME_DESIGN 5).
        const float r = kNovaFrontWu * clampf(pl.novaFrontAge / kNovaFrontS, 0.0f, 1.0f);
        const float r2 = r * r;
        const Vec2 c = pl.novaAt;
        const auto inView = [&](float x, float y) {
            return std::fabs(x - c.x) <= kNovaViewHalfW && std::fabs(y - c.y) <= kNovaViewHalfH;
        };
        for (u32 b = 0; b < bullets_.count; ++b) {
            if (bullets_.dead[b]) continue;
            const float dx = bullets_.x[b] - c.x, dy = bullets_.y[b] - c.y;
            if (dx * dx + dy * dy <= r2 && inView(bullets_.x[b], bullets_.y[b])) bullets_.dead[b] = 1;
        }
        Enemies& e = enemies_;
        // Co-op: only the Nova's owner clears enemies (a client claims each from the host and predicts it);
        // every device clears the bullets.
        for (u32 i = 0; i < e.count && isLocal(p); ++i) {
            if (e.flags[i] & kFlagDead) continue;
            const float dx = e.x[i] - c.x, dy = e.y[i] - c.y;
            if (dx * dx + dy * dy > r2 || !inView(e.x[i], e.y[i])) continue;
            if (role_ == NetRole::Client) {
                const bool warping = e.state[i] == kWarping;
                logClaim(i, 0.0f, warping ? kClaimDissolve : kClaimKill);
                predictKill(i, static_cast<u8>(p), warping);
            } else if (e.state[i] == kWarping) {
                // Not yet solid: it dissolves (no Shards, no score).
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
        pl.novaFrontAge += kDt;
        if (pl.novaFrontAge > kNovaFrontS) pl.novaFrontAge = -1.0f;
    }
}

// ---------------------------------------------------------------------------------------------- motion

void World::moveProjectiles(float dt) {
    // Enemy bullets fly straight until their life ends or they meet a wall.
    // M5: Time Stretch slows them with the enemies (the player's own shots keep their speed).
    Bullets& b = bullets_;
    const float bdt = dt * stretch_;
    for (u32 i = 0; i < b.count; ++i) {
        b.px[i] = b.x[i];
        b.py[i] = b.y[i];
        if (b.dead[i]) continue;
        b.x[i] += b.vx[i] * bdt;
        b.y[i] += b.vy[i] * bdt;
        b.life[i] -= bdt;
        if (b.life[i] <= 0.0f || !insideArena(b.x[i], b.y[i])) b.dead[i] = 1;
    }
    Shots& s = shots_;
    steerShots(dt);  // M5: Seeker, missiles, sparks
    wellShots(dt);   // M5 part 4: Wells bend and swallow shots
    for (u32 i = 0; i < s.count; ++i) {
        if (s.dead[i]) continue;
        s.px[i] = s.x[i];
        s.py[i] = s.y[i];
        s.x[i] += s.dx[i] * s.speed[i] * dt;
        s.y[i] += s.dy[i] * s.speed[i] * dt;
        if (s.mods[i] & kShotWave) {
            // M5: Wave's sine across the line (Helix Array's braid).
            const Vec2 w = waveStep(i, dt);
            s.x[i] += w.x;
            s.y[i] += w.y;
        }
        s.life[i] -= dt;
        if (s.life[i] <= 0.0f && (s.mods[i] & kShotBlast) && s.kind[i] != kShotPrimary && s.kind[i] != kShotMini) {
            // M5: Heavy Step's orbs, Carousel's bombs and Flak's pellets at level 5 blow where their life ends.
            s.dead[i] = 1;
            shotBlast(i, Vec2{s.x[i], s.y[i]}, -1);
            continue;
        }
        if (!insideArena(s.x[i], s.y[i]) && s.bounces[i] > 0) {
            // Ricochet (GAME_DESIGN 6.2): off the arena wall, mirrored.
            --s.bounces[i];
            if (s.mods[i] & kShotFever) s.speed[i] = std::fmin(s.speed[i] * kFeverSpeedUp, 3.0f * kShotSpeed);  // M5: Ricochet Fever
            if (std::fabs(s.x[i]) > kArenaHalfW) {
                s.dx[i] = -s.dx[i];
                s.x[i] = std::copysign(2.0f * kArenaHalfW - std::fabs(s.x[i]), s.x[i]);
            }
            if (std::fabs(s.y[i]) > kArenaHalfH) {
                s.dy[i] = -s.dy[i];
                s.y[i] = std::copysign(2.0f * kArenaHalfH - std::fabs(s.y[i]), s.y[i]);
            }
            s.px[i] = s.x[i] - s.dx[i] * s.speed[i] * dt;
            s.py[i] = s.y[i] - s.dy[i] * s.speed[i] * dt;
            s.lastHit[i] = 0xFFFFFFFFu;
        }
        if (s.life[i] <= 0.0f || !insideArena(s.x[i], s.y[i])) s.dead[i] = 1;
    }
    // Shards: loose ones slow down and wait; inside a player's magnet they home in; after four bars they
    // are gone (GAME_DESIGN 5). Play test 1: while more than kShardsCrowded are out (the stronger build kills several
    // times as fast) the pool must make room for the next kills. Until the owner's play of f0624e5 a loose Shard then
    // lasted one bar, so every Shard older than that went out at once ("if too many shards are on screen it glitches out
    // and they timeout ... and disappear"); then one older than a bar flew to its player, as under Magnet Storm (GAME_DESIGN 5:
    // a Shard is never lost to the crowd); since 2026-11-15 they merge.
    // 2026-11-15 (the owner: that was "a free magnet storm"): the crowded field's old Shards merge with their neighbours instead
    // (mergeCrowdedShards), worth all of them and still to be collected.
    Shards& h = shards_;
    const double lifeTicks = kShardLifeBars * kTicksPerBar;
    mergeCrowdedShards();
    const float drag = std::exp(-kShardDrag * dt);
    for (u32 i = 0; i < h.count; ++i) {
        h.px[i] = h.x[i];
        h.py[i] = h.y[i];
        if (h.dead[i]) continue;
        const PlayerState& pl = players_[h.owner[i]];
        if (m1_ - h.born[i] > lifeTicks && !(h.homing[i] && pl.alive)) {  // (one on its way in is not lost)
            h.dead[i] = 1;
            continue;
        }
        bool home = false;
        Vec2 to{};
        float dist = 0.0f;
        if (pl.alive) {
            to = pl.pos - Vec2{h.x[i], h.y[i]};
            dist = length(to);
            // M5: Afterparty keeps the Surge's x2 for its player past the Surge.
            const float surge = (afterparty(h.owner[i]) && !phrase_.surge()) ? 2.0f : 1.0f;
            const float magnet = kShardMagnetWu * rules_.magnetMultiplier * surge *
                                 (1.0f + kMagnetPerStack * static_cast<float>(pl.loadout.mods[kMagnet]));
            home = h.homing[i] || dist < magnet;
        }
        if (home && dist > 1e-3f) {
            h.homing[i] = 1;
            const float speed = std::fmin(length(Vec2{h.vx[i], h.vy[i]}) + kShardHomeAccel * dt, kShardHomeMax);
            const Vec2 d = to * (1.0f / dist);
            h.vx[i] = d.x * speed;
            h.vy[i] = d.y * speed;
        } else {
            h.vx[i] *= drag;
            h.vy[i] *= drag;
        }
        h.x[i] += h.vx[i] * dt;
        h.y[i] += h.vy[i] * dt;
    }
}

// ---------------------------------------------------------------------------------------------- collisions

void World::collide() {
    Enemies& e = enemies_;
    // Shots x enemies: the shot's path this tick (a capsule) against each enemy circle; the first enemy
    // along the path takes the hit (ties broken by index, so the result does not depend on visit order).
    Shots& s = shots_;
    for (u32 k = 0; k < s.count; ++k) {
        if (s.dead[k]) continue;
        const float ax = s.px[k], ay = s.py[k], bx = s.x[k], by = s.y[k];
        const float dx = bx - ax, dy = by - ay;
        const float len2 = dx * dx + dy * dy;
        const float sr = s.radius[k];
        const float reach = sr + maxEnemyR_ + kGridSlack;
        i32 best = -1;
        float bestT = 2.0f;
        const u32 skip = s.lastHit[k];
        grid_.query(std::fmin(ax, bx) - reach, std::fmin(ay, by) - reach, std::fmax(ax, bx) + reach, std::fmax(ay, by) + reach,
                    [&](u32 i) {
                        if ((e.flags[i] & kFlagDead) || e.handle[i] == skip || elitePhased(i)) return;  // (M5: Phasing)
                        const float cx = e.x[i] - ax, cy = e.y[i] - ay;
                        const float t = len2 > 1e-6f ? clampf((cx * dx + cy * dy) / len2, 0.0f, 1.0f) : 0.0f;
                        const float qx = dx * t - cx, qy = dy * t - cy;
                        const float rr = e.radius[i] + sr;
                        if (qx * qx + qy * qy > rr * rr) return;
                        if (t < bestT || (t == bestT && static_cast<i32>(i) < best)) {
                            bestT = t;
                            best = static_cast<i32>(i);
                        }
                    });
        // M4: another player's shot is a cosmetic copy (its owner resolves its hits, TECH_SPEC 8.1): it stops at
        // what it meets and shows the impact, nothing more.
        const bool real = isLocal(s.owner[k]);
        // The boss, when it is nearer along the path than any enemy.
        if (boss_.active && !boss_.dying) {
            u8 part = 0, pair = 0;
            float bt = 0.0f;
            if (bossHitTest(Vec2{ax, ay}, Vec2{bx, by}, sr, part, pair, bt) && bt <= bestT) {
                s.dead[k] = 1;
                if (real) {
                    damageBoss(part, pair, s.damage[k], s.owner[k], Vec2{ax + dx * bt, ay + dy * bt}, Vec2{s.dx[k], s.dy[k]});
                    if (s.mods[k] & kShotBlast) shotBlast(k, Vec2{ax + dx * bt, ay + dy * bt}, -1);  // M5: the blast round it
                } else {
                    Event ev;
                    ev.type = EventType::Impact;
                    ev.species = kBossSpecies;
                    ev.c = part;
                    ev.a = pair;
                    ev.b = s.owner[k];
                    ev.x = ax + dx * bt;
                    ev.y = ay + dy * bt;
                    ev.x2 = s.dx[k];
                    ev.y2 = s.dy[k];
                    emit(ev);
                }
                continue;
            }
        }
        {
            // M5 part 5: a Vault, a Rift or an event's body nearer along the path than any enemy takes the shot.
            float at = 0.0f;
            i32 body = -1;
            if (arenaShotHit(k, Vec2{ax, ay}, Vec2{bx, by}, sr, at, body) && at <= bestT) {
                s.dead[k] = 1;
                if (real) damageBody(static_cast<u32>(body), s.damage[k], s.owner[k], Vec2{ax + dx * at, ay + dy * at}, false);
                continue;
            }
        }
        if (best < 0) continue;
        if (wellSwallow(static_cast<u32>(best), k)) continue;  // M5 part 4: Well B and C swallow shots
        if (!real) {
            s.dead[k] = 1;
            // M5: Gravity Rounds tug at a teammate's shot's hit too (the host moves the bodies; gravityTug).
            if (s.kind[k] == kShotPrimary && s.owner[k] < kMaxPlayers) gravityTug(s.owner[k], Vec2{ax + dx * bestT, ay + dy * bestT}, static_cast<u32>(best));
            Event ev;
            ev.type = EventType::Impact;
            ev.species = e.species[best];
            ev.b = s.owner[k];
            ev.c = shotHasLane(s.flags[k]) ? static_cast<u8>(shotLane(s.flags[k]) + 3) : 0;
            ev.x = ax + dx * bestT;
            ev.y = ay + dy * bestT;
            ev.x2 = s.dx[k];
            ev.y2 = s.dy[k];
            emit(ev);
            continue;
        }
        const u32 i = static_cast<u32>(best);
        const Vec2 at{ax + dx * bestT, ay + dy * bestT};
        const Vec2 dir{s.dx[k], s.dy[k]};
        const u8 owner = s.owner[k];
        // Prism (GAME_DESIGN 6.4): a shot's first hit spawns two mini-refractions (play test 1: the first only, and
        // none above the soft cap of live shots).
        const bool refract = (s.flags[k] & (kShotPrism | kShotRefracted)) == kShotPrism && s.kind[k] == kShotPrimary &&
                             shotRoom(owner);
        const u32 hitHandle = e.handle[i];
        // (its damage as it reaches this one: Pierce grows it for the next)
        const float dmg = s.damage[k];
        if (s.pierce[k] > 0) {
            // Pierce (GAME_DESIGN 6.2): through this one, on to the next, a quarter heavier (the owner's buff, 2026-11-15).
            --s.pierce[k];
            s.lastHit[k] = hitHandle;
            s.damage[k] *= kPierceGrowth;
        } else if (s.bounces[k] > 0 && s.kind[k] != kShotMini) {
            // Ricochet (play test 1): from the enemy it hit on to the nearest other one within 300 wu; none near: spent.
            const i32 next = ricochetTarget(i, at);
            if (next >= 0) {
                --s.bounces[k];
                if (s.mods[k] & kShotFever) s.speed[k] = std::fmin(s.speed[k] * kFeverSpeedUp, 3.0f * kShotSpeed);  // M5: Ricochet Fever
                if (owner < kMaxPlayers) {  // M5: the bounce's blip (its number in the shot's run)
                    const u32 all = ricochetBounces(players_[owner].loadout.mods[kRicochet]) + ((s.mods[k] & kShotFever) ? kFeverBounces : 0u);
                    emitFx(owner, kFxBounce, at, at, static_cast<u8>(all > s.bounces[k] ? all - s.bounces[k] : 1u), 0.0f, m1_);
                }
                const Vec2 nd = normalizeOr(Vec2{e.x[next], e.y[next]} - at, dir);
                s.dx[k] = nd.x;
                s.dy[k] = nd.y;
                s.x[k] = s.px[k] = at.x;
                s.y[k] = s.py[k] = at.y;
                s.lastHit[k] = hitHandle;
            } else {
                s.dead[k] = 1;
            }
        } else {
            s.dead[k] = 1;
        }
        const bool crit = (s.flags[k] & kShotCrit) != 0 && s.kind[k] == kShotPrimary;
        hitShotFlags_ = s.flags[k];  // play test 2: its Impact shows the shot's Prism lane
        hitEnemy(i, dmg, owner, at, dir);
        hitShotFlags_ = 0;
        if (crit) critBurst(i, owner, at, dmg * kCritBurstShare);
        shotHitExtras(k, i, at, dir);  // M5: Fork, the blasts, Gravity Rounds, Burn
        if (refract) {
            s.flags[k] = static_cast<u8>(s.flags[k] | kShotRefracted);
            const float a = angleOf(dir), deg = kTau / 360.0f;
            for (float side : {-1.0f, 1.0f})
                spawnShotRaw(owner, at, dirOf(a + side * kPrismMiniDeg * deg), 0.0f, kPrismMiniDamage, kShotMini,
                             static_cast<u8>(s.flags[k] & kShotLaneMask), kPrismMiniLifeS, 0, 0);
        }
    }

    // Players x enemy bodies and enemy bullets (GAME_DESIGN 5: hit radius 5 wu). Co-op: only this device's
    // own players (what you see is what hits you; the others tell us their hits).
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.alive || !isLocal(p)) continue;
        if (pl.invulnerable() || rules_.godMode) continue;
        if (sheltered(p)) continue;  // play test 2: inside a level-up orb's ring nothing hurts it
        const Vec2 c = pl.pos;
        i32 contact = -1;
        const float reach = maxEnemyR_ + kGridSlack + kPlayerHitRadius;
        grid_.query(c.x - reach, c.y - reach, c.x + reach, c.y + reach, [&](u32 i) {
            if ((e.flags[i] & kFlagDead) || contact >= 0 || elitePhased(i)) return;  // (M5: Phasing's touch is harmless off its half)
            const float r = e.radius[i] * kContactShrink + kPlayerHitRadius;
            const float dx = e.x[i] - c.x, dy = e.y[i] - c.y;
            if (dx * dx + dy * dy <= r * r) contact = static_cast<i32>(i);
        });
        if (contact >= 0) {
            if (!haloBlocks(p, Vec2{e.x[contact], e.y[contact]})) hitPlayer(p, Vec2{e.x[contact], e.y[contact]}, e.species[contact]);
            continue;
        }
        {
            // M5 part 4: the lethal lines (Tether beams, Weaver trails, Ram B's wake).
            Vec2 from{};
            u16 cause = kCauseNone;
            if (hazardHits(p, from, cause)) {
                if (!haloBlocks(p, from)) hitPlayer(p, from, cause);
                continue;
            }
        }
        const float rb = species_->bulletRadius + kPlayerHitRadius;
        for (u32 b = 0; b < bullets_.count; ++b) {
            if (bullets_.dead[b]) continue;
            const float dx = bullets_.x[b] - c.x, dy = bullets_.y[b] - c.y;
            if (dx * dx + dy * dy > rb * rb) continue;
            bullets_.dead[b] = 1;
            if (!haloBlocks(p, Vec2{bullets_.x[b], bullets_.y[b]})) hitPlayer(p, Vec2{bullets_.x[b], bullets_.y[b]}, bullets_.src[b], true);
            break;
        }
    }

    // Shards x their player.
    Shards& h = shards_;
    for (u32 i = 0; i < h.count; ++i) {
        if (h.dead[i]) continue;
        PlayerState& pl = players_[h.owner[i]];
        if (!pl.alive) continue;
        const float dx = h.x[i] - pl.pos.x, dy = h.y[i] - pl.pos.y;
        if (dx * dx + dy * dy > kShardPickupWu * kShardPickupWu) continue;
        h.dead[i] = 1;
        // GAME_DESIGN 5: each Shard collected adds +1 to that player's Chain (a merged one: all it is worth, 2026-11-15)
        const u32 worth = h.value[i] ? h.value[i] : 1u;
        pl.chain += worth;
        if (pl.chain > pl.bestChain) pl.bestChain = pl.chain;
        pl.pickups += worth;
        Event ev;
        ev.type = EventType::Pickup;
        ev.a = h.owner[i];
        ev.x = h.x[i];
        ev.y = h.y[i];
        emit(ev);
        for (u32 k = 0; k < worth; ++k) collectShard(h.owner[i]);  // play test 2: the Shard level meter
    }
}

void World::hitPlayer(u32 p, Vec2 from, u16 cause, bool bullet) {
    // The review's fixes (H8, 2026-11-15; GAME_DESIGN 6.1 "a ship inside an orb's ring cannot be hurt"): checked once here, where
    // every hit is taken, so a meteor, the Storm Cell, a Volatile blast and every source to come respect it.
    if (sheltered(p)) return;
    PlayerState& pl = players_[p];
    pl.lastHitCause = cause;  // M6: what took the cell (the game over's ENDED BY)
    pl.lastHitBullet = bullet;
    // Auto Nova (GAME_DESIGN 5, play test 1 second pass; on unless switched off): a hit that would take the last Shield
    // cell (or end the run, with none left) fires a Nova from the stock instead and costs nothing; the Nova keeps R6
    // (invulnerable at once, the blast on the next beat). With no Nova in stock the hit lands as before.
    // Play test 3 (GAME_DESIGN 6.3 row 1): the Orbitals' guard ring, every shard lit, takes the hit instead of a cell (first:
    // it is a spare cell).
    if (breakRing(p, from)) return;
    if (rules_.autoNova && pl.cells <= 1 && pressNova(p)) return;
    // Play test 3 (GAME_DESIGN 5): the hit that would end the run (co-op: put the ship down) spends an Extra Life instead.
    if (pl.cells == 0 && pl.extraLives) {
        spendExtraLife(p, from);
        return;
    }
    if (pl.cells == 0 && rules_.demoRevive && !coop()) {
        demoRevive(p, from);  // (the title's demo never dies: Rules::demoRevive)
        return;
    }
    ++pl.hitsTaken;
    if (pl.cells == 0 && coop()) {
        // GAME_DESIGN 10: in co-op the ship shatters into a beacon; the run goes on while anyone stands.
        downPlayer(p);
        return;
    }
    if (pl.cells == 0) {
        // GAME_DESIGN 4.3: a hit with no Shield cells left ends the run.
        pl.alive = false;
        pl.fire = 0;
        pl.novaPending = false;
        ++counters_.deaths;
        Event ev;
        ev.type = EventType::Death;
        ev.a = static_cast<u8>(p);
        ev.x = pl.pos.x;
        ev.y = pl.pos.y;
        emit(ev);
        return;
    }
    // GAME_DESIGN 5: a hit removes one cell, fires a panic pulse (clears bullets within 220 wu, shoves
    // enemies), grants 1.5 s of invulnerability and halves the Chain.
    --pl.cells;
    pl.invulS = kInvulnerableS;
    pl.chain = pl.chain / 2 > 1 ? pl.chain / 2 : 1u;  // halved, never below x1
    dipS_ = kHitDipS;
    // GAME_DESIGN 8, assist: a lost Shield cell eases the budget 10% (once a cycle).
    if (!director_.hitThisCycle) {
        director_.hitThisCycle = true;
        director_.assist = std::fmax(director_.assist - kAssistEaseStep, kAssistEaseMin);  // (M6: wider)
    }
    const Vec2 c = pl.pos;
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
        const float d = std::sqrt(d2);
        e.kx[i] += dx / d * kPanicShoveWu;
        e.ky[i] += dy / d * kPanicShoveWu;
    }
    Event ev;
    ev.type = EventType::Hit;
    ev.a = static_cast<u8>(p);
    ev.b = pl.cells;
    ev.x = c.x;
    ev.y = c.y;
    ev.x2 = from.x;
    ev.y2 = from.y;
    emit(ev);
    hitExtras(p);  // M5: Crescendo starts again, Last Light
}

void World::killEnemy(u32 i, u8 killer) {
    Enemies& e = enemies_;
    if (e.flags[i] & kFlagDead) return;
    if (role_ == NetRole::Client) {
        // M4: the host decides (TECH_SPEC 8.1); the kill is shown now, its Shards and score come with the host's word.
        predictKill(i, killer, false);
        return;
    }
    e.flags[i] |= kFlagDead;
    const Species& s = species_->get(e.species[i]);
    const bool elite = (e.traits[i] & kEliteBit) != 0;  // M5 part 5: an elite's death is a large kill and scores x 10
    Event ev;
    ev.type = EventType::Kill;
    ev.species = e.species[i];
    ev.a = elite ? 2 : s.killSize();
    ev.b = killer;
    ev.x = e.x[i];
    ev.y = e.y[i];
    ev.f = e.heading[i];
    emit(ev);
    ++counters_.kills;
    if (s.hue == 0 && director_.crowdLost < 100000u) {
        ++director_.crowdLost;
        ++director_.crowdKills;
    }
    if (killer < kMaxPlayers && players_[killer].present) {
        // Score = enemy value x Chain (GAME_DESIGN 5: the Chain is a multiplier from x1). Co-op: a player's own
        // device keeps its score (it hears of the kill from the host).
        PlayerState& pl = players_[killer];
        if (isLocal(killer)) {
            ++pl.kills;
            const float chain = static_cast<float>(pl.chain);
            const float surge = (afterparty(killer) && !phrase_.surge()) ? 2.0f : 1.0f;  // M5: Afterparty's longer Surge
            pl.score += static_cast<u64>(std::llround(static_cast<double>(s.score) * (elite ? species_->traits.score : 1.0f) * chain * rules_.scoreMultiplier * surge));
        }
        // A Boost is due (GAME_DESIGN 6.1, director controlled): this kill drops it.
        if (director_.boostDue && spawner_.mode == Spawner::Mode::Slice) {
            dropBoost(Vec2{e.x[i], e.y[i]}, static_cast<u8>(upgradeRng_.below(kBoostCount)));
            director_.boostDue = false;
            director_.boostTicks = static_cast<i32>(rng_.range(kBoostEveryLoS, kBoostEveryHiS) / kDt);
        }
        // Play test 3 (GAME_DESIGN 5): the Track's Extra Life, from a kill in the first cycle's Drop.
        if (spawner_.mode == Spawner::Mode::Slice) extraLifeDrop(Vec2{e.x[i], e.y[i]});
    }
    dropShards(i, killer);
    if (role_ == NetRole::Host) logDeath(i, killer, kDeathKill);
    familyDeath(i, s);  // M5: Brood C bursts into a swarm
    eliteDeath(i, killer);  // M5 part 5: what its traits do as it dies
}

void World::dropShards(u32 i, u8 killer) {
    // Every enemy death drops its Shards for each player separately (GAME_DESIGN 5, 10); Magnet Storm:
    // kills drop double (GAME_DESIGN 6.4).
    // Co-op: each device keeps its own players' Shards only (they are local, TECH_SPEC 8.1), and a player gets
    // them from deaths within two frames of it (four frames of the crowd dissolving at a Break would otherwise
    // drop thousands far from anyone, gone before they could be reached).
    Enemies& e = enemies_;
    const Species& s = species_->get(e.species[i]);
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present || !isLocal(p)) continue;
        if (coop() && (std::fabs(e.x[i] - players_[p].pos.x) > kCoopShardReachW || std::fabs(e.y[i] - players_[p].pos.y) > kCoopShardReachH))
            continue;
        // Play test 2 (GAME_DESIGN 6.1): a kill (not a Break's dissolve) near a hurt player drops its Cell when one is due.
        if (e.traits[i] & kEliteBit) {
            eliteShards(i, p);  // M5 part 5: 20 Shards and a Cell
            continue;
        }
        if (killer != 0xFF) dropCellFor(p, Vec2{e.x[i], e.y[i]}, e.rng[i]);
        u32 n = players_[p].boost(kMagnetStorm) ? 2u * s.shards : s.shards;
        if (e.state[i] == kFlee) n *= 2u;  // M5: the Alpha's pack, scattering, drops double Shards
        // M5 part 3: Prospector, Shards more for the killer: 0.5 a stack (the owner's buff, 2026-11-15: 0.25, at most one), the
        // whole part sure and the rest a chance (two stacks: one more every kill; four: two).
        if (killer == p && players_[p].loadout.mods[kProspector]) {
            const float more = kProspectorChance * static_cast<float>(players_[p].loadout.mods[kProspector]);
            const u32 sure = static_cast<u32>(more);
            n += sure + (rand01(e.rng[i]) < more - static_cast<float>(sure) ? 1u : 0u);
        }
        // Play test 1: while the Shard pool is crowded a death drops one Shard (2026-11-15: worth all of its own; it was one and
        // the rest lost), and none in its last 64 places (the stronger build's bursts: a Drop's mega-wave, a boss's 200), so the
        // pool never overflows.
        u16 each = 1;
        if (shards_.count > kShardsCrowded && n > 1) {
            each = static_cast<u16>(n < 0xFFFFu ? n : 0xFFFFu);
            n = 1;
        }
        if (shards_.count + kShardsReserve >= Shards::kCap) n = 0;
        for (u32 k = 0; k < n; ++k) {
            const float a = randRange(e.rng[i], 0.0f, kTau);
            const float v = randRange(e.rng[i], kShardBurstLo, kShardBurstHi);
            spawnShard(Vec2{e.x[i], e.y[i]}, dirOf(a) * v, static_cast<u8>(p), each);
        }
    }
}

// ---------------------------------------------------------------------------------------------- pools

i32 World::indexOf(u32 handle) const {
    const u32 slot = handle & 0xFFFFu;
    if (slot >= kMaxEnemies || enemies_.gen[slot] != (handle >> 16)) return -1;
    const u32 i = enemies_.denseOf[slot];
    return i < enemies_.count && enemies_.handle[i] == handle ? static_cast<i32>(i) : -1;
}

i32 World::spawnEnemy(u16 speciesIndex, Vec2 pos, float heading, double landTick) {
    return spawnEnemyAs(speciesIndex, pos, heading, landTick, 0);
}

i32 World::spawnEnemyAs(u16 speciesIndex, Vec2 pos, float heading, double landTick, u32 serial) {
    Enemies& e = enemies_;
    if (e.count >= Enemies::kCap || e.freeCount == 0) {
        ++counters_.enemiesDropped;
        return -1;
    }
    const Species& s = species_->get(speciesIndex);
    const u32 slot = e.freeSlots[--e.freeCount];
    const u32 i = e.count++;
    e.denseOf[slot] = i;
    e.handle[i] = (static_cast<u32>(e.gen[slot]) << 16) | slot;
    // M5: while the Pen stands the director's spawns land inside it (a client takes the host's places as they are).
    if (pen_.up && !serial) pos = penPlace(pos, s.radius);
    pos.x = clampf(pos.x, -kArenaHalfW + s.radius, kArenaHalfW - s.radius);
    pos.y = clampf(pos.y, -kArenaHalfH + s.radius, kArenaHalfH - s.radius);
    e.x[i] = e.px[i] = pos.x;
    e.y[i] = e.py[i] = pos.y;
    e.vx[i] = e.vy[i] = e.kx[i] = e.ky[i] = 0.0f;
    e.heading[i] = e.pheading[i] = heading;
    e.hp[i] = s.health * loopRamp(kLoopHealthRamp, phrase_.track, director_.songs);  // the loop ramps health (GAME_DESIGN 8, play test 2)
    e.radius[i] = s.radius;
    e.a[i] = e.b[i] = 0.0f;
    e.species[i] = speciesIndex;
    e.group[i] = 0;
    e.flags[i] = 0;
    e.far[i] = 0;
    e.step[i] = 0;
    e.burnUntil[i] = e.frozenUntil[i] = -1.0;  // M5
    e.burnOwner[i] = 0xFF;
    e.burnLevel[i] = 0;
    e.traits[i] = 0;  // M5 part 5: makeElite() sets an elite's
    e.hpMax[i] = e.hp[i];
    e.lastHit[i] = -1.0;
    e.twin[i] = 0;
    e.shield[i] = heading;
    e.held[i] = 0;
    e.stunUntil[i] = -1.0;
    e.ox[i] = e.oy[i] = e.pred[i] = 0.0f;
    e.target[i] = 0xFF;
    // M4: a client's enemy carries the host's spawn number, so its own random stream is the host's.
    const u32 sn = serial ? serial : ++serial_;
    if (serial && serial > serial_) serial_ = serial;
    e.serial[i] = sn;
    e.rng[i] = hash32(static_cast<u32>(seed_ ^ (seed_ >> 32)), sn);
    if (role_ != NetRole::Solo) idInsert(sn, slot);
    if (role_ == NetRole::Host) logSpawn(i, landTick);
    if (landTick > m1_) {
        e.state[i] = kWarping;
        e.until[i] = landTick;
        Event ev;
        ev.type = EventType::WarpStart;
        ev.species = speciesIndex;
        ev.x = pos.x;
        ev.y = pos.y;
        ev.tick = landTick;
        emit(ev);
    } else {
        landEnemy(i);
    }
    ++counters_.spawned;
    return static_cast<i32>(i);
}

bool World::spawnBullet(Vec2 pos, Vec2 vel, float advanceS, float lifeS) {
    Bullets& b = bullets_;
    if (!inBossUpdate_) vel = vel * kCrowdBulletSpeedScale;  // M6 (A4): the crowd's bullets slower; the fights' as their pages say
    u32 i = b.count;
    if (!inBossUpdate_ && static_cast<float>(b.count) >= kBulletPoolCrowd * static_cast<float>(Bullets::kCap)) {
        // M5 part 7: the crowd's shooters hold a volley the pool's last 5% would take (kept for the fights' own patterns).
        ++counters_.bulletsHeld;
        return false;
    }
    if (b.count >= Bullets::kCap) {
        // M5 part 7 (GAME_DESIGN 6.6): a fight's own pattern is never what is refused: it takes the place of the crowd's
        // bullet nearest its end (the pool does not grow).
        i32 slot = -1;
        float least = 1e30f;
        for (u32 k = 0; inBossUpdate_ && k < b.count; ++k)
            if (!b.boss[k] && b.life[k] < least) {
                least = b.life[k];
                slot = static_cast<i32>(k);
            }
        if (slot < 0) {
            ++counters_.bulletsDropped;
            return false;
        }
        i = static_cast<u32>(slot);
        ++counters_.bulletsEvicted;
    } else {
        ++b.count;
    }
    vel = vel * loopBulletSpeed_;  // the loop speeds up every enemy bullet (GAME_DESIGN 8; 1 in the first pass)
    ++counters_.bulletsAll;
    if (inBossUpdate_) ++counters_.bulletsBoss;
    b.px[i] = pos.x;
    b.py[i] = pos.y;
    b.x[i] = pos.x + vel.x * advanceS;
    b.y[i] = pos.y + vel.y * advanceS;
    b.vx[i] = vel.x;
    b.vy[i] = vel.y;
    b.life[i] = (lifeS > 0.0f ? lifeS : kBulletLifeS) - advanceS;
    b.dead[i] = 0;
    b.echo[i] = 0;
    b.boss[i] = inBossUpdate_ ? 1 : 0;
    b.src[i] = inBossUpdate_ ? static_cast<u16>(kCauseBossBase + static_cast<u32>(boss_.kind)) : bulletSrc_;  // M6
    lastBullet_ = i;
    return true;
}

bool World::spawnShard(Vec2 pos, Vec2 vel, u8 owner, u16 value) {
    Shards& h = shards_;
    if (h.count >= Shards::kCap) {
        ++counters_.shardsDropped;
        return false;
    }
    const u32 i = h.count++;
    h.x[i] = h.px[i] = pos.x;
    h.y[i] = h.py[i] = pos.y;
    h.vx[i] = vel.x;
    h.vy[i] = vel.y;
    h.born[i] = m1_;
    h.owner[i] = owner < kMaxPlayers ? owner : 0;
    h.homing[i] = 0;
    h.dead[i] = 0;
    h.holder[i] = 0;
    h.value[i] = value ? value : 1;
    return true;
}

void World::mergeCrowdedShards() {
    // 2026-11-15 (the owner: the crowded field's old Shards flying in were "a free magnet storm"): above kShardsCrowded the loose
    // Shards older than kShardCrowdedBars merge with their neighbours of the same player (within kShardMergeWu: a cell of that
    // size), one Shard worth them all left where the first of them lies, still to be collected. Nothing is lost and nothing
    // flies in by itself; the merged Shard lives as long as the youngest it took in. In index order (deterministic).
    Shards& h = shards_;
    if (h.count <= kShardsCrowded) return;
    constexpr u32 kSlots = 4096;  // (open addressing on the stack; a key: the player and the cell)
    u32 key[kSlots], keeper[kSlots];
    for (u32& k : key) k = 0xFFFFFFFFu;
    const double old = kShardCrowdedBars * kTicksPerBar;
    u32 merged = 0;
    for (u32 i = 0; i < h.count; ++i) {
        if (h.dead[i] || h.homing[i] || h.holder[i] || m1_ - h.born[i] <= old) continue;
        const i32 cx = static_cast<i32>(std::floor(h.x[i] / kShardMergeWu)) & 0xFFF, cy = static_cast<i32>(std::floor(h.y[i] / kShardMergeWu)) & 0xFFF;
        const u32 k = (static_cast<u32>(h.owner[i]) << 24) | (static_cast<u32>(cx) << 12) | static_cast<u32>(cy);
        u32 slot = (k * 2654435761u) >> 20;  // (12 bits)
        while (key[slot] != 0xFFFFFFFFu && key[slot] != k) slot = (slot + 1) & (kSlots - 1);
        if (key[slot] == 0xFFFFFFFFu) {
            key[slot] = k;
            keeper[slot] = i;
            continue;
        }
        const u32 j = keeper[slot];
        const u32 sum = static_cast<u32>(h.value[j]) + h.value[i];
        h.value[j] = static_cast<u16>(sum < 0xFFFFu ? sum : 0xFFFFu);
        if (h.born[i] > h.born[j]) h.born[j] = h.born[i];
        h.dead[i] = 1;
        ++merged;
    }
    counters_.shardsMerged += merged;
}

void World::removeEnemyAt(u32 i) {
    Enemies& e = enemies_;
    const u32 slot = e.handle[i] & 0xFFFFu;
    e.gen[slot] = static_cast<u16>(e.gen[slot] + 1);  // every handle to this slot is now stale
    e.freeSlots[e.freeCount++] = slot;
    if (role_ != NetRole::Solo) idErase(e.serial[i]);
    const u32 last = --e.count;
    if (i != last) {
        e.x[i] = e.x[last];
        e.y[i] = e.y[last];
        e.px[i] = e.px[last];
        e.py[i] = e.py[last];
        e.ox[i] = e.ox[last];
        e.oy[i] = e.oy[last];
        e.pred[i] = e.pred[last];
        e.target[i] = e.target[last];
        e.vx[i] = e.vx[last];
        e.vy[i] = e.vy[last];
        e.kx[i] = e.kx[last];
        e.ky[i] = e.ky[last];
        e.heading[i] = e.heading[last];
        e.pheading[i] = e.pheading[last];
        e.hp[i] = e.hp[last];
        e.radius[i] = e.radius[last];
        e.a[i] = e.a[last];
        e.b[i] = e.b[last];
        e.until[i] = e.until[last];
        e.species[i] = e.species[last];
        e.group[i] = e.group[last];
        e.state[i] = e.state[last];
        e.flags[i] = e.flags[last];
        e.far[i] = e.far[last];
        e.step[i] = e.step[last];
        e.burnUntil[i] = e.burnUntil[last];
        e.frozenUntil[i] = e.frozenUntil[last];
        e.burnOwner[i] = e.burnOwner[last];
        e.burnLevel[i] = e.burnLevel[last];
        e.traits[i] = e.traits[last];  // M5 part 5
        e.hpMax[i] = e.hpMax[last];
        e.lastHit[i] = e.lastHit[last];
        e.twin[i] = e.twin[last];
        e.shield[i] = e.shield[last];
        e.held[i] = e.held[last];
        e.stunUntil[i] = e.stunUntil[last];
        e.handle[i] = e.handle[last];
        e.serial[i] = e.serial[last];
        e.rng[i] = e.rng[last];
        e.denseOf[e.handle[i] & 0xFFFFu] = i;
    }
}

void World::compact() {
    // Swap-remove in index order (TECH_SPEC 7.1): deterministic, no holes. A client's predicted deaths stay
    // until the host answers (M4).
    for (u32 i = 0; i < enemies_.count;) {
        if ((enemies_.flags[i] & (kFlagDead | kFlagPredicted)) == kFlagDead) {
            removeEnemyAt(i);
        } else {
            ++i;
        }
    }
    Bullets& b = bullets_;
    for (u32 i = 0; i < b.count;) {
        if (!b.dead[i]) {
            ++i;
            continue;
        }
        const u32 l = --b.count;
        b.x[i] = b.x[l];
        b.y[i] = b.y[l];
        b.px[i] = b.px[l];
        b.py[i] = b.py[l];
        b.vx[i] = b.vx[l];
        b.vy[i] = b.vy[l];
        b.life[i] = b.life[l];
        b.dead[i] = b.dead[l];
        b.echo[i] = b.echo[l];
        b.boss[i] = b.boss[l];
        b.src[i] = b.src[l];
    }
    Shots& s = shots_;
    for (u32 i = 0; i < s.count;) {
        if (!s.dead[i]) {
            ++i;
            continue;
        }
        const u32 l = --s.count;
        s.x[i] = s.x[l];
        s.y[i] = s.y[l];
        s.px[i] = s.px[l];
        s.py[i] = s.py[l];
        s.dx[i] = s.dx[l];
        s.dy[i] = s.dy[l];
        s.speed[i] = s.speed[l];
        s.life[i] = s.life[l];
        s.life0[i] = s.life0[l];
        s.damage[i] = s.damage[l];
        s.radius[i] = s.radius[l];
        s.lastHit[i] = s.lastHit[l];
        s.owner[i] = s.owner[l];
        s.dead[i] = s.dead[l];
        s.pierce[i] = s.pierce[l];
        s.bounces[i] = s.bounces[l];
        s.kind[i] = s.kind[l];
        s.flags[i] = s.flags[l];
        s.mods[i] = s.mods[l];
        s.forks[i] = s.forks[l];
        s.arena[i] = s.arena[l];
        s.accent[i] = s.accent[l];
    }
    // Play test 1: each player's live shots, for the pool's per-player caps in the next tick.
    for (u32& n : shotsLive_) n = 0;
    for (u32 i = 0; i < s.count; ++i) ++shotsLive_[s.owner[i] < kMaxPlayers ? s.owner[i] : 0];
    Shards& h = shards_;
    for (u32 i = 0; i < h.count;) {
        if (!h.dead[i]) {
            ++i;
            continue;
        }
        const u32 l = --h.count;
        h.x[i] = h.x[l];
        h.y[i] = h.y[l];
        h.px[i] = h.px[l];
        h.py[i] = h.py[l];
        h.vx[i] = h.vx[l];
        h.vy[i] = h.vy[l];
        h.born[i] = h.born[l];
        h.owner[i] = h.owner[l];
        h.homing[i] = h.homing[l];
        h.dead[i] = h.dead[l];
        h.holder[i] = h.holder[l];
        h.value[i] = h.value[l];
    }
}

// ---------------------------------------------------------------------------------------------- checks

u64 World::hash() const {
    u64 h = 1469598103934665603ull;
    const auto mix = [&h](const void* data, size_t n) {
        const u8* p = static_cast<const u8*>(data);
        for (size_t i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
    };
    const auto val = [&mix](const auto& v) { mix(&v, sizeof v); };
    val(tick_);
    val(m1_);
    val(bpm_);
    val(dipS_);
    val(serial_);
    val(seed_);
    val(rng_);
    val(spawner_.nextSchool);
    val(spawner_.lastBar);
    val(spawner_.startTick);
    val(upgradeRng_);
    val(director_.track);
    val(director_.trackBar);
    val(director_.changeBar);
    val(director_.bossSpawned);
    val(director_.bank);
    val(director_.pending);
    val(director_.assist);
    val(director_.boostTicks);
    val(director_.boostDue);
    val(director_.lastBar);
    val(director_.crowdLost);
    val(director_.crowdKills);
    val(director_.crowdDebt);
    val(phrase_.bar);
    val(phrase_.section);
    val(phrase_.sectionBar);
    val(phrase_.track);
    for (const TrioState& tr : trio_) {
        val(tr.open);
        val(tr.orbs);
        mix(tr.upgrade, sizeof tr.upgrade);
        mix(tr.orbPos, sizeof tr.orbPos);
        val(tr.closeTick);
        val(tr.homing);
        val(tr.queued);
        val(tr.sinceRiff);
    }
    val(boostPickups_.count);
    mix(boostPickups_.x, boostPickups_.count * sizeof(float));
    mix(boostPickups_.y, boostPickups_.count * sizeof(float));
    mix(boostPickups_.type, boostPickups_.count);
    val(boss_.active);
    val(boss_.dying);
    val(boss_.alive);
    val(boss_.phase);
    val(boss_.phaseTick);
    val(boss_.angle0);
    val(boss_.angle);
    mix(boss_.partHp, sizeof boss_.partHp);
    val(boss_.coreHp);
    val(boss_.pos.x);
    val(boss_.pos.y);
    // M5 part 1: which fight, its bodies, the Pen, the mini-boss's hold.
    val(boss_.kind);
    mix(boss_.body, sizeof boss_.body);
    val(boss_.mini.state);
    val(boss_.mini.half);
    val(boss_.mini.splits);
    val(boss_.lw.phase);
    val(boss_.ch.caller);
    val(pen_.up);
    val(pen_.centre.x);
    val(pen_.centre.y);
    val(director_.midHold);
    val(director_.midHolding);
    val(director_.miniSpawned);
    val(director_.songs);
    // M5 part 5: the arena and the elites.
    for (const ArenaBody& b : arena_) {
        val(b.kind);
        val(b.alive);
        val(b.pos.x);
        val(b.pos.y);
        val(b.hp);
        val(b.charge);
        val(b.tuner);
    }
    val(arenaEvent_.kind);
    val(arenaEvent_.stage);
    val(arenaEvent_.pos.x);
    val(arenaEvent_.pos.y);
    val(arenaRng_);
    val(eliteClock_);
    const Enemies& e = enemies_;
    const u32 n = e.count;
    mix(e.traits, n * sizeof(u16));
    mix(e.hpMax, n * sizeof(float));
    val(n);
    mix(e.x, n * sizeof(float));
    mix(e.y, n * sizeof(float));
    mix(e.vx, n * sizeof(float));
    mix(e.vy, n * sizeof(float));
    mix(e.kx, n * sizeof(float));
    mix(e.ky, n * sizeof(float));
    mix(e.heading, n * sizeof(float));
    mix(e.hp, n * sizeof(float));
    mix(e.a, n * sizeof(float));
    mix(e.b, n * sizeof(float));
    mix(e.until, n * sizeof(double));
    mix(e.species, n * sizeof(u16));
    mix(e.group, n * sizeof(u16));
    mix(e.state, n);
    mix(e.flags, n);
    mix(e.far, n);
    mix(e.step, n);
    mix(e.burnUntil, n * sizeof(double));
    mix(e.frozenUntil, n * sizeof(double));
    mix(e.handle, n * sizeof(u32));
    mix(e.serial, n * sizeof(u32));
    mix(e.rng, n * sizeof(u32));
    const Bullets& b = bullets_;
    val(b.count);
    mix(b.x, b.count * sizeof(float));
    mix(b.y, b.count * sizeof(float));
    mix(b.vx, b.count * sizeof(float));
    mix(b.vy, b.count * sizeof(float));
    mix(b.life, b.count * sizeof(float));
    const Shots& s = shots_;
    val(s.count);
    mix(s.x, s.count * sizeof(float));
    mix(s.y, s.count * sizeof(float));
    mix(s.dx, s.count * sizeof(float));
    mix(s.dy, s.count * sizeof(float));
    mix(s.life, s.count * sizeof(float));
    mix(s.damage, s.count * sizeof(float));
    mix(s.pierce, s.count);
    mix(s.bounces, s.count);
    const Shards& sh = shards_;
    val(sh.count);
    mix(sh.x, sh.count * sizeof(float));
    mix(sh.y, sh.count * sizeof(float));
    mix(sh.vx, sh.count * sizeof(float));
    mix(sh.vy, sh.count * sizeof(float));
    mix(sh.born, sh.count * sizeof(double));
    mix(sh.homing, sh.count);
    mix(sh.value, sh.count * sizeof(u16));
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        const PlayerState& pl = players_[p];
        val(pl.alive);
        val(pl.pos.x);
        val(pl.pos.y);
        val(pl.cells);
        val(pl.novas);
        val(pl.invulS);
        val(pl.novaInvulS);
        val(pl.novaPending);
        val(pl.novaPressed);
        val(pl.novaBlastTick);
        val(pl.novaFrontAge);
        val(pl.chain);
        val(pl.bestChain);
        val(pl.score);
        val(pl.kills);
        val(pl.pickups);
        val(pl.shotsFired);
        mix(pl.loadout.mods, sizeof pl.loadout.mods);
        mix(pl.loadout.riffs, sizeof pl.loadout.riffs);
        val(pl.cellCap);
        for (const ActiveBoost& a : pl.boosts) {
            val(a.type);
            val(a.until);
            val(a.hits);
        }
        for (const Vec2& s : pl.sentinel) {
            val(s.x);
            val(s.y);
        }
    }
    for (const TrioState& t : trio_) {
        val(t.holdOrb);
        val(t.holdTicks);
        val(t.kind);
        val(t.breakWaiting);
    }
    // Play test 2: Shard levels, Cells, Regen.
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        const PlayerState& pl = players_[p];
        val(pl.levelShards);
        val(pl.level);
        val(pl.levelsBanked);
        val(pl.cellDue);
        val(pl.regenBars);
        val(pl.burnCap);
    }
    val(director_.burnedTrack);
    // M5 part 3: the new upgrades' state.
    mix(s.mods, s.count);
    mix(s.forks, s.count);
    mix(s.kind, s.count);
    mix(e.burnLevel, n);
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        const PlayerState& pl = players_[p];
        val(pl.loadout.fusions);
        val(pl.volleys);
        val(pl.cleanBars);
        val(pl.collectorShards);
        val(pl.wellFrom);
        val(pl.holeFrom);
        val(pl.ringBroke);    // play test 3
        val(pl.extraLives);
        for (const PlayerState::Mine& m : pl.mines) val(m.blowTick);
        for (const PlayerState::Comet& c : pl.comets) val(c.start);
        for (const PlayerState::Echo& ec : pl.echoes) val(ec.tick);
    }
    {
        const CellPickups& c = cellPickups_;
        val(c.count);
        mix(c.x, c.count * sizeof(float));
        mix(c.y, c.count * sizeof(float));
        mix(c.born, c.count * sizeof(double));
        mix(c.owner, c.count);
    }
    if (role_ != NetRole::Solo) {
        // M4: what co-op adds (solo hashes stay what M3's were).
        mix(e.target, n);
        mix(e.ox, n * sizeof(float));
        mix(e.oy, n * sizeof(float));
        val(runId_);
        for (u32 p = 0; p < kMaxPlayers; ++p) {
            val(players_[p].down);
            val(players_[p].present);
            val(players_[p].reviveTicks);
        }
    }
    return h;
}

bool World::finite() const {
    const auto ok = [](const float* v, u32 n) {
        for (u32 i = 0; i < n; ++i)
            if (!std::isfinite(v[i])) return false;
        return true;
    };
    const Enemies& e = enemies_;
    if (!ok(e.x, e.count) || !ok(e.y, e.count) || !ok(e.vx, e.count) || !ok(e.vy, e.count) || !ok(e.heading, e.count) ||
        !ok(e.hp, e.count))
        return false;
    if (!ok(bullets_.x, bullets_.count) || !ok(bullets_.y, bullets_.count)) return false;
    if (!ok(shots_.x, shots_.count) || !ok(shots_.y, shots_.count)) return false;
    if (!ok(shards_.x, shards_.count) || !ok(shards_.y, shards_.count)) return false;
    if (!ok(cellPickups_.x, cellPickups_.count) || !ok(cellPickups_.y, cellPickups_.count)) return false;
    for (u32 p = 0; p < playerCount_; ++p)
        if (!std::isfinite(players_[p].pos.x) || !std::isfinite(players_[p].pos.y)) return false;
    return true;
}

}  // namespace rf::sim
