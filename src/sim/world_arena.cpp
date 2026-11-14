// M5 part 5: the arena (design/arena.md, arena.json; GAME_DESIGN 9): six structures that stand for a Track (Vault, Pylon,
// Rift, Bumper, Current, Lens) and eight events of a few bars (Vault Drop, Rift Surge, Elite Hunt, Comet, Eclipse, Meteor
// Rain, Convoy, Storm Cell). Each is one sentence and sounds like part of the Track.
//
// Co-op: the host places structures, starts events, spawns what they let through and decides when a body breaks; each is an
// arena operation (logArena, applied by World::applyArenaOp on a client). A client's shots on a body are claimed like its
// shots on a boss. What touches only a player (a Current's push, a Bumper's kick, a meteor or a Storm Cell taking a cell) is
// that player's own device's; what a player's structure does to enemies (a tuned Pylon's pulse, a Bumper's discharge) is
// resolved on that player's device, as their weapons are; what the arena itself does to enemies (a meteor, the Storm Cell's
// lightning, a Volatile bomb) is the host's.

#include <cmath>
#include <initializer_list>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;
using namespace prose;

namespace {
constexpr float kDegR = kTau / 360.0f;
bool hasHealth(u8 kind) {
    return kind == static_cast<u8>(StructKind::Vault) || kind == static_cast<u8>(StructKind::Rift) || kind == kBodyGoldVault ||
           kind == kBodyComet || kind == kBodyCarrier;
}
float bodyRadius(const ArenaData& a, u8 kind) {
    if (kind < kStructKinds) return a.radius[kind];
    if (kind == kBodyGoldVault) return a.radius[static_cast<u32>(StructKind::Vault)];
    if (kind == kBodyComet) return a.glyph[ArenaData::gComet].radius;
    return kArCarrierRadius;
}
}  // namespace

// ---------------------------------------------------------------------------------------------- queries

float World::trackScale() const {
    // arena.md: the mini-boss health of that Track (minibosses.json healthByTrack; Track 1 uses 300), x 1.25 a Track of the
    // loop as the mini-bosses', x (1 + 0.5 per extra player) (a decision: the page is silent on co-op health here).
    const MiniData& md = species_->minis;
    const u32 tr = phrase_.track < 1 ? 1u : phrase_.track;
    const u32 first = tr < director_.songs ? tr : director_.songs;
    float h = first <= 1 ? kArTrack1Scale : md.healthByTrack[first > md.healthTracks ? md.healthTracks : first];
    if (inLoop(tr, director_.songs)) h *= std::pow(md.loopFactor, static_cast<float>(tr - director_.songs));
    return h * (1.0f + kCoopBossHealthPerPlayer * static_cast<float>(presentCount() - 1));
}

bool World::eclipse() const {
    const ArenaEventState& ev = arenaEvent_;
    return ev.kind == static_cast<u8>(EventKind::Eclipse) && ev.stage == 1 && m1_ >= ev.start && m1_ < ev.end;
}

Vec2 World::currentFlow(Vec2 at) const {
    // A Current: anything inside its lane (240 x 1,600 wu) moves 260 wu/s faster along it.
    Vec2 f{};
    for (const ArenaBody& b : arena_) {
        if (!b.alive || b.kind != static_cast<u8>(StructKind::Current)) continue;
        const Vec2 d = dirOf(b.angle), rel = at - b.pos;
        if (std::fabs(dot(rel, d)) <= 0.5f * kArCurrentLength && std::fabs(cross(d, rel)) <= 0.5f * kArCurrentWidth) f = f + d * kArCurrentPush;
    }
    return f;
}

bool World::bumperBounce(Vec2& pos, Vec2& vel, float r) const {
    // A Bumper: ships bounce off it unhurt with a kick of 600 wu/s.
    bool hit = false;
    for (const ArenaBody& b : arena_) {
        if (!b.alive || b.kind != static_cast<u8>(StructKind::Bumper)) continue;
        const Vec2 rel = pos - b.pos;
        const float rr = species_->arena.radius[static_cast<u32>(StructKind::Bumper)] + r;
        const float l2 = lengthSq(rel);
        if (l2 >= rr * rr) continue;
        const Vec2 n = normalizeOr(rel, Vec2{1.0f, 0.0f});
        pos = b.pos + n * rr;
        vel = n * kArBumperKick;
        hit = true;
    }
    return hit;
}

// ---------------------------------------------------------------------------------------------- placing

bool World::arenaSpot(Vec2& out, Vec2 centre, float lo, float hi, float apart, u32& rngState) const {
    // arena.md: by the run's seed relative to the players' centre, never inside a player's frame, never within 400 wu of a
    // wall, `apart` from every structure standing.
    for (u32 tries = 0; tries < 48; ++tries) {
        const float a = randRange(rngState, 0.0f, kTau), d = randRange(rngState, lo, hi);
        const Vec2 p = centre + dirOf(a) * d;
        if (std::fabs(p.x) > kArenaHalfW - kArWallKeepWu || std::fabs(p.y) > kArenaHalfH - kArWallKeepWu) continue;
        bool ok = true;
        for (u32 q = 0; q < playerCount_ && ok; ++q)
            if (players_[q].present && std::fabs(p.x - players_[q].pos.x) < kFrameHalfWWu && std::fabs(p.y - players_[q].pos.y) < kFrameHalfHWu) ok = false;
        for (const ArenaBody& b : arena_)
            if (ok && b.alive && b.kind < kStructKinds && lengthSq(b.pos - p) < apart * apart) ok = false;
        if (!ok) continue;
        out = p;
        return true;
    }
    return false;
}

i32 World::addBody(u8 kind, Vec2 pos, float angle, double tick) {
    i32 slot = -1;
    for (u32 k = 0; k < kArenaBodies && slot < 0; ++k)
        if (!arena_[k].alive) slot = static_cast<i32>(k);
    if (slot < 0) return -1;
    ArenaBody& b = arena_[slot];
    b = ArenaBody{};
    b.kind = kind;
    b.alive = 1;
    b.pos = pos;
    b.angle = angle;
    b.placed = tick;
    b.track = phrase_.track;
    const float ts = trackScale();
    if (kind == static_cast<u8>(StructKind::Vault)) b.hp = ts * kArVaultHealth;
    if (kind == static_cast<u8>(StructKind::Rift)) b.hp = ts * kArRiftHealth;
    if (kind == kBodyGoldVault) b.hp = ts * kArVaultDropHealth;
    if (kind == kBodyComet) b.hp = ts * kArCometHealth;
    if (kind == kBodyCarrier) b.hp = ts * kArCarrierHealth;
    b.hpMax = b.hp;
    if (role_ == NetRole::Host) logArena(3, static_cast<u8>(slot), kind, pos, Vec2{angle, 0.0f}, tick, b.track);
    return slot;
}

void World::placeStructures(u32 track, double tick) {
    // arena.md: on the first downbeat of the Track, 1,000 to 1,800 wu from the players' centre, at least 700 wu apart; a
    // Track with Bumpers has them as a cluster 300-500 wu apart; Currents come as two crossing.
    const ArenaData& a = species_->arena;
    const u32 t = songOfTrack(track, director_.songs) + 1u;
    if (t > ArenaData::kTracks) return;
    const Vec2 c = playersCentre();
    i32 lastBumper = -1, lastCurrent = -1;
    u32 standing = 0;
    for (const ArenaBody& b : arena_) standing += b.alive && b.kind < kStructKinds && b.gone < 0.0 ? 1u : 0u;
    for (u32 k = 0; k < a.structCount[t] && standing < kArMaxStructures; ++k) {
        const u8 kind = a.structs[t][k];
        Vec2 p{};
        float ang = randRange(arenaRng_, 0.0f, kTau);
        bool ok = false;
        if (kind == static_cast<u8>(StructKind::Bumper) && lastBumper >= 0) {
            const Vec2 from = arena_[lastBumper].pos;
            for (u32 tries = 0; tries < 24 && !ok; ++tries) {
                p = from + dirOf(randRange(arenaRng_, 0.0f, kTau)) * randRange(arenaRng_, kArBumperApart[0], kArBumperApart[1]);
                ok = std::fabs(p.x) < kArenaHalfW - kArWallKeepWu && std::fabs(p.y) < kArenaHalfH - kArWallKeepWu;
                for (const ArenaBody& b : arena_)
                    if (ok && b.alive && b.kind == kind && lengthSq(b.pos - p) < kArBumperApart[0] * kArBumperApart[0]) ok = false;
            }
        } else if (kind == static_cast<u8>(StructKind::Current) && lastCurrent >= 0) {
            p = arena_[lastCurrent].pos;  // the second crosses the first
            ang = arena_[lastCurrent].angle + randRange(arenaRng_, 60.0f, 120.0f) * kDegR;
            ok = true;
        } else {
            ok = arenaSpot(p, c, kArPlaceNearWu, kArPlaceFarWu, kArApartWu, arenaRng_);
        }
        if (!ok) continue;
        const i32 s = addBody(kind, p, ang, tick);
        if (s < 0) continue;
        ++standing;
        if (kind == static_cast<u8>(StructKind::Bumper)) {
            arena_[s].tone = static_cast<u8>(lastBumper < 0 ? 0 : (arena_[lastBumper].tone + 1) % 4);
            lastBumper = s;
        }
        if (kind == static_cast<u8>(StructKind::Current)) lastCurrent = s;
    }
}

// ---------------------------------------------------------------------------------------------- the director's bar and beat

void World::arenaBar(const Phrase& ph, u32 bar, double barTick) {
    const bool host = role_ != NetRole::Client;
    const bool fight = boss_.active && !boss_.dying;
    if (host && spawner_.mode == Spawner::Mode::Slice) {
        // Structures: on the first downbeat of the Track; the last Track's dissolve over the Track change's Break (unless a
        // player is within 600 wu: then until that player leaves).
        if (!ph.trackChange && ph.cycle == 1 && ph.section == Section::Groove && ph.sectionBar == 0) placeStructures(ph.track, barTick);
        if (ph.trackChange && ph.sectionBar == 0)
            for (ArenaBody& b : arena_)
                if (b.alive && b.kind < kStructKinds && !b.surge) b.gone = barTick;
        // Events: Track 1's Comet in the Groove of its second cycle (play test 3: Track 1 has two); from Track 2 one in the Groove
        // of the second cycle, from Track 4 a second in the third; drawn by the seed from the Track's list, never the event of the
        // Track before; not in a fight. An event never outlasts its Groove (arena.md, play test 3).
        if (!ph.trackChange && ph.section == Section::Groove && ph.sectionBar == 0 && !fight && arenaEvent_.stage != 1 &&
            arenaEvent_.kind == kNoEvent) {
            const ArenaData& a = species_->arena;
            const u32 t = songOfTrack(ph.track, director_.songs) + 1u;
            const bool due = (t == 1 && ph.cycle == 2) || (ph.track >= 2 && ph.cycle == 2) || (ph.track >= 4 && ph.cycle == 3);
            if (due && t <= ArenaData::kTracks) {
                u8 pool[4];
                u32 n = 0;
                for (u32 k = 0; k < a.eventCount[t]; ++k)
                    if (a.events[t][k] != lastEvent_ || a.eventCount[t] == 1) pool[n++] = a.events[t][k];
                if (n) {
                    const EventKind k = static_cast<EventKind>(pool[static_cast<u32>(rand01(arenaRng_) * static_cast<float>(n)) % n]);
                    startEvent(k, barTick);
                }
            }
        }
    }
    if (host) {
        // Rifts: open with the Groove of the Track's second cycle, through the Build; at the Drop they close and give nothing.
        // A Rift Surge's open at the start of a Build and close at the end of the Drop.
        for (u32 k = 0; k < kArenaBodies; ++k) {
            ArenaBody& b = arena_[k];
            if (!b.alive || b.kind != static_cast<u8>(StructKind::Rift)) continue;
            const bool open = b.surge ? (ph.section == Section::Build || ph.section == Section::Drop) && !fight
                                      : ph.cycle == 2 && (ph.section == Section::Groove || ph.section == Section::Build) && !fight;
            const bool closing = b.surge ? (ph.section == Section::Break) : (ph.cycle == 2 && ph.section == Section::Drop) || ph.cycle == 3 ||
                                                                               ph.trackChange;
            if (b.from <= 0.0 && open) {
                b.from = barTick;
                emitCue(BossCue::RiOpen, barTick, 0, 0.0f, b.pos);
            }
            if (b.from > 0.0 && closing) {
                b.alive = 0;
                emitCue(BossCue::RiClose, barTick, 0, 0.0f, b.pos);
                logArena(7, static_cast<u8>(k), 0, b.pos, Vec2{}, barTick);
                continue;
            }
            if (b.from > 0.0 && open && enemies_.count + 6 < aliveCap()) {
                // Four of the Track's fodder (six for a Surge's) come through it every bar.
                const u32 n = b.surge ? kArSurgePerBar : kArRiftPerBar;
                for (u32 j = 0; j < n; ++j) {
                    const i32 sp = pickSpecies(1);
                    if (sp < 0) break;
                    const float ang = kTau * static_cast<float>(j) / static_cast<float>(n) + static_cast<float>(bar) * 0.7f;
                    spawnEnemy(static_cast<u16>(sp), b.pos + dirOf(ang) * 20.0f, ang, barTick + kTicksPerBeat);
                }
                emitCue(BossCue::RiThrough, barTick + kTicksPerBeat, 0, 0.0f, b.pos);
            }
        }
    }
    (void)bar;
}

void World::arenaBeat(double beatTick) {
    // Bumpers discharge on the beat after their eighth hit; a tuned Pylon pulses on the One; Meteor Rain's rings and landings;
    // the Storm Cell's lightning.
    const bool host = role_ != NetRole::Client;
    const double inBar = beatTick - std::floor(beatTick / kTicksPerBar + 1e-6) * kTicksPerBar;
    const bool one = inBar < 1.0;
    const float tr = static_cast<float>(phrase_.track);
    for (u32 k = 0; k < kArenaBodies; ++k) {
        ArenaBody& b = arena_[k];
        if (!b.alive) continue;
        if (b.kind == static_cast<u8>(StructKind::Bumper) && b.discharge) {
            b.discharge = 0;
            b.charge = 0.0f;
            emitCue(BossCue::BuDischarge, beatTick, b.tone, kArBumperRingWu, b.pos);
            if (b.owner < kMaxPlayers && isLocal(b.owner)) ringDamage(b.pos, kArBumperRingWu, kArBumperDamage * tr, b.owner);
        }
        if (b.kind == static_cast<u8>(StructKind::Pylon) && b.tuner < kMaxPlayers && one && beatTick < b.until) {
            b.from = beatTick;  // its ring runs out from here (updateArena deals it)
            emitCue(BossCue::PyPulse, beatTick, b.tuner, kArPylonPulseWu, b.pos);
        }
    }
    ArenaEventState& ev = arenaEvent_;
    if (ev.stage != 1) return;
    if (ev.kind == static_cast<u8>(EventKind::MeteorRain) && beatTick < ev.end - kArMeteorBeats * kTicksPerBeat + 1.0 && host) {
        // Three dashed rings of 110 wu in the frame, one where a player stands; they land two beats later.
        u32 placed = 0;
        const i32 who = coop() ? firstAlive() : 0;
        for (u32 m = 0; m < kArenaMeteors && placed < kArMeteorsBeat; ++m) {
            ArenaMeteor& mt = meteors_[m];
            if (mt.land >= 0.0) continue;
            Vec2 at = players_[who >= 0 ? who : 0].pos;
            if (placed) at = at + Vec2{randRange(arenaRng_, -kFrameHalfWWu, kFrameHalfWWu) * 0.85f, randRange(arenaRng_, -kFrameHalfHWu, kFrameHalfHWu) * 0.85f};
            mt.at = at;
            mt.land = beatTick + kArMeteorBeats * kTicksPerBeat;
            emitCue(BossCue::MeWarn, beatTick, 0, kArMeteorWu, at);
            logArena(5, static_cast<u8>(m), 0, at, Vec2{}, mt.land);
            ++placed;
        }
    }
    if (ev.kind == static_cast<u8>(EventKind::StormCell)) {
        // Lightning joins up to six bodies inside it (15 x the Track's number each: the host's); a ship inside on a beat is
        // warned, still inside on the next it loses a cell (its own device's).
        if (host) {
            u32 n = 0;
            for (u32 i = 0; i < enemies_.count && n < kArStormBodies; ++i) {
                if ((enemies_.flags[i] & kFlagDead) || enemies_.state[i] == kWarping) continue;
                if (lengthSq(Vec2{enemies_.x[i], enemies_.y[i]} - ev.pos) > kArStormWu * kArStormWu) continue;
                emitCue(BossCue::StStrike, beatTick, static_cast<u8>(n), 0.0f, Vec2{enemies_.x[i], enemies_.y[i]});
                hitEnemy(i, kArStormDamage * tr, 0xFF, Vec2{enemies_.x[i], enemies_.y[i]}, Vec2{1.0f, 0.0f});
                ++n;
            }
        }
        for (u32 p = 0; p < playerCount_; ++p) {
            PlayerState& pl = players_[p];
            if (!pl.present || !pl.alive || !isLocal(p)) continue;
            const bool inside = lengthSq(pl.pos - ev.pos) <= kArStormWu * kArStormWu;
            const u8 bit = static_cast<u8>(1u << p);
            if (inside && (ev.hits & bit) && !pl.invulnerable() && !rules_.godMode && !haloBlocks(p, ev.pos)) hitPlayer(p, ev.pos, static_cast<u16>(kCauseEventBase + static_cast<u32>(EventKind::StormCell)));
            ev.hits = static_cast<u8>(inside ? (ev.hits | bit) : (ev.hits & ~bit));
            if (inside) emitCue(BossCue::StWarn, beatTick, static_cast<u8>(p), 0.0f, pl.pos);
        }
    }
    if (ev.kind == static_cast<u8>(EventKind::Comet) && beatTick >= ev.start) {
        // Five Shards a beat behind it (each device its own players').
        for (const ArenaBody& b : arena_) {
            if (!b.alive || b.kind != kBodyComet) continue;
            u32 r = static_cast<u32>(beatTick) ^ 0xC0E7u;
            for (u32 p = 0; p < playerCount_; ++p) {
                if (!players_[p].present || !isLocal(p)) continue;
                for (u32 j = 0; j < kArCometShardsBeat; ++j)
                    spawnShard(b.pos - normalizeOr(b.vel, Vec2{1.0f, 0.0f}) * 40.0f, dirOf(randRange(r, 0.0f, kTau)) * randRange(r, 20.0f, 80.0f), static_cast<u8>(p));
            }
            emitCue(BossCue::CmShard, beatTick, 0, 0.0f, b.pos);
        }
    }
    if (ev.kind == static_cast<u8>(EventKind::Eclipse)) emitCue(BossCue::EcFlash, beatTick, 0, 0.0f, ev.pos);
    if (ev.kind == static_cast<u8>(EventKind::Convoy) && beatTick >= ev.start) emitCue(BossCue::CoRoll, beatTick, 0, 0.0f, ev.pos);
}

// ---------------------------------------------------------------------------------------------- every tick

void World::updateArena(float dt) {
    const bool host = role_ != NetRole::Client;
    const ArenaData& a = species_->arena;
    const float tr = static_cast<float>(phrase_.track);
    Enemies& e = enemies_;
    for (u32 k = 0; k < kArenaBodies; ++k) {
        ArenaBody& b = arena_[k];
        if (!b.alive) continue;
        // Dissolving: gone at the end of the Break, unless a player is within 600 wu.
        if (b.gone >= 0.0 && host && m1_ >= b.gone + kTrackChangeBars * kTicksPerBar) {
            bool near = false;
            for (u32 p = 0; p < playerCount_; ++p)
                near = near || (players_[p].present && players_[p].alive && lengthSq(players_[p].pos - b.pos) < kArStayNearWu * kArStayNearWu);
            if (!near) {
                b.alive = 0;
                logArena(7, static_cast<u8>(k), 0, b.pos, Vec2{}, m1_);
                continue;
            }
        }
        switch (b.kind) {
            case static_cast<u8>(StructKind::Pylon): {
                // Stand inside its 150 wu ring for four beats and it fills; leave and it empties twice as fast. Tuned for a cycle.
                const bool tuned = b.tuner < kMaxPlayers && m1_ < b.until;
                if (!tuned) {
                    if (b.tuner < kMaxPlayers) {
                        b.tuner = 0xFF;  // it dims and can be tuned again
                        b.charge = 0.0f;
                    }
                    i32 in = -1;
                    for (u32 p = 0; p < playerCount_ && in < 0; ++p)
                        if (players_[p].present && players_[p].alive && lengthSq(players_[p].pos - b.pos) <= kArPylonRingWu * kArPylonRingWu) in = static_cast<i32>(p);
                    const float fill = dt / (kArPylonFillBeats * 60.0f / std::fmax(bpm_, 30.0f));
                    const float was = b.charge;
                    b.charge = in >= 0 ? b.charge + fill : std::fmax(0.0f, b.charge - kArPylonEmpty * fill);
                    if (in >= 0 && std::floor(b.charge * kArPylonFillBeats) > std::floor(was * kArPylonFillBeats))
                        emitCue(BossCue::PyFill, m1_, static_cast<u8>(std::floor(b.charge * kArPylonFillBeats)), 0.0f, b.pos);
                    if (b.charge >= 1.0f && in >= 0) {
                        b.charge = 1.0f;
                        b.tuner = static_cast<u8>(in);
                        b.until = m1_ + static_cast<double>(phrase_.track == 1 ? kArPylonBarsT1 : kArPylonBars) * kTicksPerBar;
                        b.from = -1.0;
                        emitCue(BossCue::PyTuned, m1_, static_cast<u8>(in), 0.0f, b.pos);
                    }
                } else if (b.from >= 0.0 && isLocal(b.tuner)) {
                    // Its pulse: a ring to 520 wu at 700 wu/s, 6 x the Track's number to each enemy once (the tuner's device).
                    const float s0 = static_cast<float>((m0_ - b.from) / (bpm_ * kTicksPerBeat / 60.0f));
                    const float s1 = static_cast<float>((m1_ - b.from) / (bpm_ * kTicksPerBeat / 60.0f));
                    const float r0 = std::fmax(0.0f, s0 * kArPylonPulseSpeed), r1 = std::fmin(kArPylonPulseWu, s1 * kArPylonPulseSpeed);
                    if (r1 > r0 && r0 < kArPylonPulseWu) {
                        const float reach = r1 + maxEnemyR_;
                        grid_.query(b.pos.x - reach, b.pos.y - reach, b.pos.x + reach, b.pos.y + reach, [&](u32 i) {
                            if (e.flags[i] & kFlagDead) return;
                            const float d = length(Vec2{e.x[i], e.y[i]} - b.pos);
                            if (d > r0 && d <= r1) hitEnemy(i, kArPylonDamage * tr, b.tuner, Vec2{e.x[i], e.y[i]}, normalizeOr(Vec2{e.x[i], e.y[i]} - b.pos, Vec2{1.0f, 0.0f}));
                        });
                    }
                    if (r1 >= kArPylonPulseWu) b.from = -1.0;
                }
                break;
            }
            case static_cast<u8>(StructKind::Bumper): {
                // Enemies bounce off it (set outside its ring).
                const float R = a.radius[static_cast<u32>(StructKind::Bumper)];
                const float reach = R + maxEnemyR_;
                grid_.query(b.pos.x - reach, b.pos.y - reach, b.pos.x + reach, b.pos.y + reach, [&](u32 i) {
                    if (e.flags[i] & kFlagDead) return;
                    const Vec2 rel = Vec2{e.x[i], e.y[i]} - b.pos;
                    const float rr = R + e.radius[i];
                    if (lengthSq(rel) >= rr * rr) return;
                    const Vec2 n = normalizeOr(rel, Vec2{1.0f, 0.0f});
                    e.x[i] = b.pos.x + n.x * rr;
                    e.y[i] = b.pos.y + n.y * rr;
                    e.kx[i] += n.x * kArBumperKick * 0.5f;
                    e.ky[i] += n.y * kArBumperKick * 0.5f;
                });
                break;
            }
            case static_cast<u8>(StructKind::Current): {
                // Enemies inside move 260 wu/s faster along it.
                const Vec2 d = dirOf(b.angle);
                const float reach = 0.5f * kArCurrentLength;
                grid_.query(b.pos.x - reach, b.pos.y - reach, b.pos.x + reach, b.pos.y + reach, [&](u32 i) {
                    if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) return;
                    const Vec2 rel = Vec2{e.x[i], e.y[i]} - b.pos;
                    if (std::fabs(dot(rel, d)) > 0.5f * kArCurrentLength || std::fabs(cross(d, rel)) > 0.5f * kArCurrentWidth) return;
                    e.x[i] += d.x * kArCurrentPush * dt;
                    e.y[i] += d.y * kArCurrentPush * dt;
                });
                break;
            }
            case static_cast<u8>(StructKind::Lens):
                b.angle += kTau * dt * (bpm_ / 60.0f) / (kArLensTurnBars * 4.0f);  // one turn every eight bars
                break;
            case kBodyComet:
            case kBodyCarrier:
                b.pos = b.pos + b.vel * dt;
                if (host && (std::fabs(b.pos.x) > kArenaHalfW + 200.0f || std::fabs(b.pos.y) > kArenaHalfH + 200.0f)) {
                    b.alive = 0;  // it leaves the arena
                    logArena(7, static_cast<u8>(k), 0, b.pos, Vec2{}, m1_);
                }
                break;
            default: break;
        }
    }
    arenaShots(dt);
    arenaBullets(dt);
    updateEvent(dt);
    // Meteors land on their beat: a cell from any ship inside (its own device), 20 x the Track's number to every enemy inside.
    for (ArenaMeteor& mt : meteors_) {
        if (mt.land < 0.0 || m1_ < mt.land) continue;
        const Vec2 at = mt.at;
        mt.land = -1.0;
        emitCue(BossCue::MeLand, m1_, 0, kArMeteorWu, at);
        for (u32 p = 0; p < playerCount_; ++p) {
            PlayerState& pl = players_[p];
            if (!pl.present || !pl.alive || !isLocal(p) || pl.invulnerable() || rules_.godMode) continue;
            if (lengthSq(pl.pos - at) <= kArMeteorWu * kArMeteorWu && !haloBlocks(p, at)) hitPlayer(p, at, static_cast<u16>(kCauseEventBase + static_cast<u32>(EventKind::MeteorRain)));
        }
        if (host) ringDamage(at, kArMeteorWu, kArMeteorDamage * tr, 0xFF);
    }
}

void World::ringDamage(Vec2 c, float r, float dmg, u8 owner) {
    Enemies& e = enemies_;
    const float reach = r + maxEnemyR_;
    grid_.query(c.x - reach, c.y - reach, c.x + reach, c.y + reach, [&](u32 i) {
        if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) return;
        const float rr = r + e.radius[i];
        if (lengthSq(Vec2{e.x[i], e.y[i]} - c) > rr * rr) return;
        hitEnemy(i, dmg, owner, Vec2{e.x[i], e.y[i]}, normalizeOr(Vec2{e.x[i], e.y[i]} - c, Vec2{1.0f, 0.0f}));
    });
}

void World::arenaShots(float dt) {
    // Player shots: a Lens splits one crossing it into three (once a Lens); a Current speeds one travelling with it (x 1.3
    // speed and damage, once) and slows one against it (x 0.7); a Bumper reflects one (a free bounce, x 1.5 damage after it)
    // and takes a charge.
    Shots& s = shots_;
    bool any = false;
    for (const ArenaBody& b : arena_)
        any = any || (b.alive && (b.kind == static_cast<u8>(StructKind::Lens) || b.kind == static_cast<u8>(StructKind::Current) ||
                                  b.kind == static_cast<u8>(StructKind::Bumper)));
    if (!any) return;
    const ArenaData& a = species_->arena;
    const u32 n0 = s.count;
    for (u32 k = 0; k < n0; ++k) {
        if (s.dead[k]) continue;
        const Vec2 p0{s.px[k], s.py[k]}, p1{s.x[k], s.y[k]};
        for (u32 j = 0; j < kArenaBodies; ++j) {
            const ArenaBody& b = arena_[j];
            if (!b.alive) continue;
            const u16 bit = static_cast<u16>(1u << j);
            if (b.kind == static_cast<u8>(StructKind::Lens) && !(s.arena[k] & bit)) {
                const Vec2 d = dirOf(b.angle) * (0.5f * kArLensLength);
                const Vec2 l0 = b.pos - d, l1 = b.pos + d;
                // Segment crossing (p0 p1) x (l0 l1).
                const Vec2 r = p1 - p0, q = l1 - l0;
                const float den = cross(r, q);
                if (std::fabs(den) < 1e-6f) continue;
                const float t = cross(l0 - p0, q) / den, u = cross(l0 - p0, r) / den;
                if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f) continue;
                s.arena[k] = static_cast<u16>(s.arena[k] | bit);
                const Vec2 at = p0 + r * t;
                const float ang = angleOf(Vec2{s.dx[k], s.dy[k]});
                const u16 mask = s.arena[k];
                const u8 owner = s.owner[k];
                for (float side : {-1.0f, 1.0f}) {
                    spawnShotRaw(owner, at, dirOf(ang + side * kArLensFanDeg * kDegR), 0.0f, s.damage[k] * kArLensSide, s.kind[k],
                                 static_cast<u8>((s.flags[k] & ~kShotLaneMask) | shotLaneFlags(side < 0.0f ? -1 : 1)), s.life[k], 0, 0);
                    if (lastShot_ >= 0) s.arena[lastShot_] = mask;
                }
                if (isLocal(owner)) emitCue(BossCue::LeSplit, m1_, owner, 0.0f, at);
            } else if (b.kind == static_cast<u8>(StructKind::Current)) {
                const Vec2 d = dirOf(b.angle), rel = p1 - b.pos;
                if (std::fabs(dot(rel, d)) > 0.5f * kArCurrentLength || std::fabs(cross(d, rel)) > 0.5f * kArCurrentWidth) continue;
                const bool with = dot(Vec2{s.dx[k], s.dy[k]}, d) > 0.0f;
                if (!(s.arena[k] & bit)) {
                    s.arena[k] = static_cast<u16>(s.arena[k] | bit);
                    if (with) s.damage[k] *= kArCurrentWith;
                }
                // (the shot's speed this tick: moved on by the difference)
                const float extra = (with ? kArCurrentWith : kArCurrentAgainst) - 1.0f;
                s.x[k] += s.dx[k] * s.speed[k] * extra * dt;
                s.y[k] += s.dy[k] * s.speed[k] * extra * dt;
            } else if (b.kind == static_cast<u8>(StructKind::Bumper)) {
                const float R = a.radius[static_cast<u32>(StructKind::Bumper)] + s.radius[k];
                const Vec2 rel = p1 - b.pos;
                if (lengthSq(rel) >= R * R || lengthSq(p0 - b.pos) < R * R) continue;
                const Vec2 n = normalizeOr(rel, Vec2{1.0f, 0.0f});
                const Vec2 dv{s.dx[k], s.dy[k]};
                const Vec2 rd = normalizeOr(dv - n * (2.0f * dot(dv, n)), n);
                s.dx[k] = rd.x;
                s.dy[k] = rd.y;
                s.x[k] = s.px[k] = b.pos.x + n.x * R;
                s.y[k] = s.py[k] = b.pos.y + n.y * R;
                if (!(s.arena[k] & bit)) s.damage[k] *= kArBumperShotDamage;
                s.arena[k] = static_cast<u16>(s.arena[k] | bit);
                ArenaBody& bm = arena_[j];
                emitCue(BossCue::BuBounce, m1_, bm.tone, 0.0f, b.pos);
                if (!bm.discharge && bm.charge + 1.0f >= static_cast<float>(kArBumperCharge)) {
                    bm.charge = static_cast<float>(kArBumperCharge);
                    bm.discharge = 1;
                    bm.owner = s.owner[k];
                } else if (!bm.discharge) {
                    bm.charge += 1.0f;
                }
                break;
            }
        }
    }
}

void World::arenaBullets(float dt) {
    // Enemy bullets: carried by Currents, bounced by Bumpers.
    Bullets& bl = bullets_;
    const ArenaData& a = species_->arena;
    for (const ArenaBody& b : arena_) {
        if (!b.alive || (b.kind != static_cast<u8>(StructKind::Current) && b.kind != static_cast<u8>(StructKind::Bumper))) continue;
        const bool current = b.kind == static_cast<u8>(StructKind::Current);
        const Vec2 d = dirOf(b.angle);
        const float R = a.radius[static_cast<u32>(StructKind::Bumper)] + species_->bulletRadius;
        for (u32 k = 0; k < bl.count; ++k) {
            if (bl.dead[k]) continue;
            const Vec2 rel = Vec2{bl.x[k], bl.y[k]} - b.pos;
            if (current) {
                if (std::fabs(dot(rel, d)) > 0.5f * kArCurrentLength || std::fabs(cross(d, rel)) > 0.5f * kArCurrentWidth) continue;
                bl.x[k] += d.x * kArCurrentPush * dt;
                bl.y[k] += d.y * kArCurrentPush * dt;
            } else if (lengthSq(rel) < R * R) {
                const Vec2 n = normalizeOr(rel, Vec2{1.0f, 0.0f});
                const Vec2 v{bl.vx[k], bl.vy[k]};
                if (dot(v, n) >= 0.0f) continue;
                const Vec2 rv = v - n * (2.0f * dot(v, n));
                bl.vx[k] = rv.x;
                bl.vy[k] = rv.y;
                bl.x[k] = b.pos.x + n.x * R;
                bl.y[k] = b.pos.y + n.y * R;
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------- bodies: hits and breaks

bool World::arenaShotHit(u32 shot, Vec2 a, Vec2 b, float r, float& t, i32& body) const {
    // The first body with health along a shot's path this tick (Vault, Rift, the events' bodies).
    (void)shot;
    const Vec2 d = b - a;
    const float len2 = lengthSq(d);
    bool hit = false;
    for (u32 k = 0; k < kArenaBodies; ++k) {
        const ArenaBody& o = arena_[k];
        if (!o.alive || !hasHealth(o.kind) || o.hp <= 0.0f) continue;
        if (o.kind == static_cast<u8>(StructKind::Rift) && o.from <= 0.0) continue;  // a closed Rift is not there yet
        const float rr = bodyRadius(species_->arena, o.kind) + r;
        const Vec2 c = o.pos - a;
        const float tt = len2 > 1e-6f ? clampf(dot(c, d) / len2, 0.0f, 1.0f) : 0.0f;
        if (lengthSq(d * tt - c) > rr * rr) continue;
        if (!hit || tt < t) {
            t = tt;
            body = static_cast<i32>(k);
            hit = true;
        }
    }
    return hit;
}

// M6 (the lead's A2): the arena's bodies with health take what hurts enemies, not only shots and Novas: areas (blasts,
// mines, wells, a Black Hole, Kick Pulse, the Comet's disc, an Orbitals shard), lines (lances, beams, Bar Line), a Pulse
// Ring's front, the Sweeper's arm. The owner's device decides (a client claims, as for shots).
bool World::arenaHurtable(u32 k) const {
    const ArenaBody& o = arena_[k];
    if (!o.alive || !hasHealth(o.kind) || o.hp <= 0.0f) return false;
    return !(o.kind == static_cast<u8>(StructKind::Rift) && o.from <= 0.0);  // (a closed Rift is not there yet)
}

void World::arenaArea(u8 owner, Vec2 at, float r, float dmg) {
    for (u32 k = 0; k < kArenaBodies; ++k) {
        if (!arenaHurtable(k)) continue;
        const float rr = bodyRadius(species_->arena, arena_[k].kind) + r;
        if (lengthSq(arena_[k].pos - at) <= rr * rr) damageBody(k, dmg, owner, arena_[k].pos, false);
    }
}

void World::arenaLine(u8 owner, Vec2 from, Vec2 dir, float len, float half, float dmg) {
    for (u32 k = 0; k < kArenaBodies; ++k) {
        if (!arenaHurtable(k)) continue;
        const Vec2 c = arena_[k].pos - from;
        const float t = clampf(dot(c, dir), 0.0f, len);
        const float rr = bodyRadius(species_->arena, arena_[k].kind) + half;
        if (lengthSq(c - dir * t) <= rr * rr) damageBody(k, dmg, owner, from + dir * t, false);
    }
}

void World::arenaRing(u8 owner, Vec2 c, float r0, float r, float dmg) {
    for (u32 k = 0; k < kArenaBodies; ++k) {
        if (!arenaHurtable(k)) continue;
        const float d = length(arena_[k].pos - c), R = bodyRadius(species_->arena, arena_[k].kind);
        if (d + R <= r0 || d - R > r) continue;  // the front did not pass it this tick
        damageBody(k, dmg, owner, arena_[k].pos, false);
    }
}

void World::arenaSweep(u8 owner, Vec2 c, float len, float a1, float swept, u32 arms, float dmg) {
    for (u32 k = 0; k < kArenaBodies; ++k) {
        if (!arenaHurtable(k)) continue;
        const Vec2 v = arena_[k].pos - c;
        const float dist = length(v), R = bodyRadius(species_->arena, arena_[k].kind);
        if (dist > len + R) continue;
        const float slack = dist > 1.0f ? std::fmin(R / dist, 0.5f) : 0.5f;
        for (u32 arm = 0; arm < arms; ++arm) {
            const float back = wrapAngle(a1 + static_cast<float>(arm) * kPi - angleOf(v));
            if (back >= -slack && back <= swept + slack) {
                damageBody(k, dmg, owner, arena_[k].pos, false);
                break;
            }
        }
    }
}
void World::damageBody(u32 slot, float dmg, u8 owner, Vec2 at, bool fromNet) {
    ArenaBody& b = arena_[slot];
    if (slot >= kArenaBodies || !b.alive || !hasHealth(b.kind) || b.hp <= 0.0f) return;
    if (role_ == NetRole::Client && !fromNet) {
        // A client's hit is the host's to apply (a boss claim on the body); shown here at once.
        if (netLog_.bossClaimCount < NetLog::kBossClaims) {
            NetBossClaim& c = netLog_.bossClaims[netLog_.bossClaimCount++];
            c.part = static_cast<u8>(kArenaPartBase + slot);
            c.pair = 0;
            c.damage = dmg;
        }
        b.hp = std::fmax(1.0f, b.hp - dmg);
    } else {
        b.hp -= dmg;
    }
    if (b.kind == static_cast<u8>(StructKind::Vault) || b.kind == kBodyGoldVault) {
        // Each quarter of its health lost, a ring of its ornament shatters and it rings a step higher.
        const u8 q = static_cast<u8>(clampf((1.0f - b.hp / std::fmax(b.hpMax, 1.0f)) * 4.0f, 0.0f, 4.0f));
        if (q > b.quarters) b.quarters = q;
        emitCue(BossCue::VaHit, m1_, b.quarters, 0.0f, at);
    }
    if (b.hp <= 0.0f && role_ != NetRole::Client) breakBody(slot, owner);
}

void World::breakBody(u32 slot, u8 killer) {
    ArenaBody& b = arena_[slot];
    if (!b.alive) return;
    if (role_ == NetRole::Host) logArena(6, static_cast<u8>(slot), killer, b.pos, Vec2{}, m1_);
    bodyRewards(slot);
    b.alive = 0;
}

void World::bodyRewards(u32 slot) {
    // Broken: a Vault one Boost and 40 Shards for each player; a Rift one Boost and 30 (and the flow stops); a Vault Drop's two
    // and 80; the Comet 60 Shards and one Boost; a carrier one Boost and 50 (the last of the three also a Cell). Boosts are the
    // host's drops (dropBoost: one orb a player); each device gives its own players their Shards and Cells.
    ArenaBody& b = arena_[slot];
    u32 shards = 0, boosts = 0;
    bool cell = false;
    BossCue cue = BossCue::VaBreak;
    switch (b.kind) {
        case static_cast<u8>(StructKind::Vault): shards = kArVaultShards; boosts = 1; break;
        case static_cast<u8>(StructKind::Rift): shards = kArRiftShards; boosts = 1; cue = BossCue::RiClose; break;
        case kBodyGoldVault: shards = kArVaultDropShards; boosts = kArVaultDropBoosts; break;
        case kBodyComet: shards = kArCometShards; boosts = 1; break;
        case kBodyCarrier:
            shards = kArCarrierShards;
            boosts = 1;
            cue = BossCue::CoBreak;
            cell = ++arenaEvent_.hits >= kArConvoyCarriers;
            break;
        default: break;
    }
    emitCue(cue, m1_, b.kind, 0.0f, b.pos);
    u32 r = hash32(static_cast<u32>(m1_), slot);
    for (u32 p = 0; p < playerCount_; ++p) {
        if (!players_[p].present || !isLocal(p)) continue;
        for (u32 k = 0; k < shards && shards_.count + kShardsReserve < Shards::kCap; ++k)
            spawnShard(b.pos, dirOf(randRange(r, 0.0f, kTau)) * randRange(r, kShardBurstLo, kShardBurstHi), static_cast<u8>(p));
        if (cell && players_[p].alive) spawnCell(b.pos, dirOf(randRange(r, 0.0f, kTau)) * kShardBurstLo, static_cast<u8>(p));
    }
    if (role_ != NetRole::Client)
        for (u32 k = 0; k < boosts; ++k) dropBoost(b.pos + dirOf(kTau * static_cast<float>(k) / 2.0f) * 30.0f, static_cast<u8>(upgradeRng_.below(kBoostCount)));
    if (b.surge) {
        // Rift Surge: all three closed: a Cell for each hurt player, 100 Shards for the others.
        u32 left = 0;
        for (u32 k = 0; k < kArenaBodies; ++k) left += (k != slot && arena_[k].alive && arena_[k].surge) ? 1u : 0u;
        if (!left) {
            for (u32 p = 0; p < playerCount_; ++p) {
                PlayerState& pl = players_[p];
                if (!pl.present || !pl.alive || !isLocal(p)) continue;
                if (pl.cells < pl.cellCap) {
                    spawnCell(b.pos, Vec2{}, static_cast<u8>(p));
                } else {
                    for (u32 k = 0; k < kArSurgeAllShards && shards_.count + kShardsReserve < Shards::kCap; ++k)
                        spawnShard(b.pos, dirOf(randRange(r, 0.0f, kTau)) * randRange(r, kShardBurstLo, kShardBurstHi), static_cast<u8>(p));
                }
            }
        }
    }
}

void World::arenaNova(u32 p) {
    // A Nova's blast takes a Vault (and the other bodies with health) in its player's view, as it takes a boss.
    const PlayerState& pl = players_[p];
    for (u32 k = 0; k < kArenaBodies; ++k) {
        const ArenaBody& b = arena_[k];
        if (!b.alive || !hasHealth(b.kind)) continue;
        if (std::fabs(b.pos.x - pl.pos.x) > kNovaViewHalfW || std::fabs(b.pos.y - pl.pos.y) > kNovaViewHalfH) continue;
        damageBody(k, kBossNovaDamage * kBossNovaShareM5, static_cast<u8>(p), b.pos, false);
    }
}

// ---------------------------------------------------------------------------------------------- events

void World::startEvent(EventKind k, double announceTick) {
    // Announced one bar ahead by a gold edge chevron and its name at the place (the Comet two beats ahead, no name), then it
    // runs (the host's choice of where: arenaRng_).
    ArenaEventState& ev = arenaEvent_;
    ev = ArenaEventState{};
    ev.kind = static_cast<u8>(k);
    ev.announce = announceTick;
    ev.stage = 0;
    const double bar = kTicksPerBar;
    ev.start = k == EventKind::Comet ? announceTick + kArCometWarnBeats * kTicksPerBeat : announceTick + bar;  // (the Comet: its chevron two beats ahead)
    const u32 bars = species_->arena.eventBars[static_cast<u32>(k)];
    ev.end = bars ? ev.start + static_cast<double>(bars) * bar : -1.0;
    // Play test 3 (arena.md): an event never outlasts its Groove. It is announced on the Groove's first bar line, so one
    // written longer ends with the Groove's last (the Elite Hunt's and the Convoy's twelve bars are the Groove's eight, the
    // announcing bar among them). The Rift Surge keeps its own written span (a Build to the end of its Drop).
    const double grooveEnd = announceTick + static_cast<double>(kSectionBars[0][0]) * bar;
    if (ev.end > grooveEnd) ev.end = grooveEnd;
    const Vec2 c = playersCentre();
    const float ang = randRange(arenaRng_, 0.0f, kTau);
    ev.dir = dirOf(ang);
    switch (k) {
        case EventKind::VaultDrop: arenaSpot(ev.pos, c, kArVaultDropNear, kArVaultDropFar, 300.0f, arenaRng_); break;
        case EventKind::RiftSurge:
        case EventKind::Eclipse:
        case EventKind::MeteorRain: ev.pos = c; break;
        case EventKind::EliteHunt: ev.pos = c + ev.dir * kArHuntWu; break;
        case EventKind::Comet: {
            // A straight line passing within 300 wu of the players' centre, from beyond the arena's edge.
            const Vec2 n{-ev.dir.y, ev.dir.x};
            ev.pos = c + n * randRange(arenaRng_, -kArCometPassWu, kArCometPassWu) - ev.dir * 2200.0f;
            break;
        }
        case EventKind::Convoy: ev.pos = c - ev.dir * kArConvoyFromWu; break;
        case EventKind::StormCell: ev.pos = c + ev.dir * 1100.0f; break;
        default: break;
    }
    ev.pos.x = clampf(ev.pos.x, -kArenaHalfW + kArWallKeepWu, kArenaHalfW - kArWallKeepWu);
    ev.pos.y = clampf(ev.pos.y, -kArenaHalfH + kArWallKeepWu, kArenaHalfH - kArWallKeepWu);
    lastEvent_ = static_cast<u8>(k);
    emitCue(BossCue::EvAnnounce, announceTick, static_cast<u8>(k), 0.0f, ev.pos);
    if (role_ == NetRole::Host) logArena(4, static_cast<u8>(k), 0, ev.pos, ev.dir, announceTick);
}

void World::endEvent() {
    ArenaEventState& ev = arenaEvent_;
    if (ev.kind == kNoEvent) return;
    for (ArenaMeteor& m : meteors_) m.land = -1.0;
    for (u32 k = 0; k < kArenaBodies; ++k) {
        ArenaBody& b = arena_[k];
        if (b.alive && (b.kind >= kBodyGoldVault || b.surge)) b.alive = 0;
    }
    if (ev.kind == static_cast<u8>(EventKind::Eclipse)) emitCue(BossCue::EcOff, m1_, 0, 0.0f, ev.pos);
    if (role_ == NetRole::Host) logArena(8, ev.kind, 0, ev.pos, Vec2{}, m1_);
    ev = ArenaEventState{};
}

void World::updateEvent(float dt) {
    ArenaEventState& ev = arenaEvent_;
    if (ev.kind == kNoEvent) return;
    const bool host = role_ != NetRole::Client;
    const EventKind k = static_cast<EventKind>(ev.kind);
    if (ev.stage == 0 && m1_ >= ev.start) {
        ev.stage = 1;
        if (host) {
            switch (k) {
                case EventKind::VaultDrop: {
                    const i32 s = addBody(kBodyGoldVault, ev.pos, 0.0f, ev.start);
                    if (s >= 0) arena_[s].until = ev.end;
                    emitCue(BossCue::VdLand, ev.start, 0, 0.0f, ev.pos);
                    break;
                }
                case EventKind::EliteHunt: {
                    // One elite of the Track's highest-threat family, two traits (three from Track 7).
                    u16 best = 0;
                    float bestThreat = -1.0f;
                    for (u32 j = 0; j < species_->rosterCount; ++j) {
                        const SpeciesTable::RosterEntry& r = species_->roster[j];
                        const Species& s = species_->get(r.species);
                        if (!familyBuilt(s.family) || s.family == Family::Shoal || s.family == Family::Wyrm || s.family == Family::Tether) continue;
                        if (r.track > phrase_.track && !inLoop(phrase_.track, director_.songs)) continue;
                        if (s.threat > bestThreat) {
                            bestThreat = s.threat;
                            best = r.species;
                        }
                    }
                    u16 traits = 0;
                    const u32 want = phrase_.track >= 7 ? kArHuntTraits7 : kArHuntTraits;
                    for (u32 tries = 0; tries < 8 && static_cast<u32>(__builtin_popcount(traits)) < want; ++tries)
                        traits = static_cast<u16>(traits | drawTraits(phrase_.track));
                    const i32 i = spawnElite(best, ev.pos, ev.start + kWarpBeats * kTicksPerBeat, traits);
                    if (i >= 0) {
                        ev.hunted = enemies_.handle[i];
                        if (role_ == NetRole::Host) logArena(9, 0, 0, Vec2{}, Vec2{}, ev.start, enemies_.serial[i]);
                    }
                    break;
                }
                case EventKind::Comet: {
                    const i32 s = addBody(kBodyComet, ev.pos, angleOf(ev.dir), ev.start);
                    if (s >= 0) arena_[s].vel = ev.dir * kArCometSpeed;
                    break;
                }
                case EventKind::Convoy: {
                    for (u32 j = 0; j < kArConvoyCarriers; ++j) {
                        const i32 s = addBody(kBodyCarrier, ev.pos - ev.dir * (kArCarrierApart * static_cast<float>(j)), angleOf(ev.dir), ev.start);
                        if (s >= 0) {
                            arena_[s].vel = ev.dir * kArConvoySpeed;
                            arena_[s].tone = static_cast<u8>(j);
                            arena_[s].until = ev.end;
                        }
                    }
                    // Eight Hounds of the Track's tier round them.
                    const u32 tier = phrase_.track < 2 ? 1u : (phrase_.track > 9 ? 5u : (phrase_.track + 1u) / 2u);
                    for (u32 j = 0; j < kArConvoyHounds; ++j) {
                        const Vec2 at = ev.pos - ev.dir * kArCarrierApart + dirOf(kTau * static_cast<float>(j) / kArConvoyHounds) * 220.0f;
                        spawnEnemy(SpeciesTable::index(Family::Hound, tier, kVariantA), at, angleOf(ev.dir), ev.start + kWarpBeats * kTicksPerBeat);
                    }
                    break;
                }
                case EventKind::Eclipse: emitCue(BossCue::EcOn, ev.start, 1, 0.0f, ev.pos); break;
                default: break;
            }
        } else if (k == EventKind::Eclipse) {
            emitCue(BossCue::EcOn, ev.start, 1, 0.0f, ev.pos);
        }
    }
    if (ev.stage != 1) return;
    switch (k) {
        case EventKind::RiftSurge:
            // At the start of a Build three Rifts open on a triangle 900 wu from the players' centre; the Break ends it.
            if (host && phrase_.section == Section::Build && !ev.hits) {
                ev.hits = 1;
                for (u32 j = 0; j < 3; ++j) {
                    Vec2 p = playersCentre() + dirOf(angleOf(ev.dir) + kTau * static_cast<float>(j) / 3.0f) * kArSurgeWu;
                    p.x = clampf(p.x, -kArenaHalfW + kArWallKeepWu, kArenaHalfW - kArWallKeepWu);
                    p.y = clampf(p.y, -kArenaHalfH + kArWallKeepWu, kArenaHalfH - kArWallKeepWu);
                    const i32 s = addBody(static_cast<u8>(StructKind::Rift), p, 0.0f, m1_);
                    if (s >= 0) {
                        arena_[s].surge = 1;
                        arena_[s].from = 0.0;
                        if (role_ == NetRole::Host) logArena(3, static_cast<u8>(s), static_cast<u8>(StructKind::Rift), p, Vec2{0.0f, 1.0f}, m1_, phrase_.track);
                    }
                }
            }
            if (host && ev.hits && phrase_.section == Section::Break) endEvent();
            break;
        case EventKind::EliteHunt: {
            // It flees the nearest player at 0.8 of its speed; not killed in twelve bars: it warps out.
            const i32 i = indexOf(ev.hunted);
            if (i >= 0 && enemies_.state[i] != kWarping) {
                Enemies& e = enemies_;
                const i32 p = nearestPlayer(e.x[i], e.y[i]);
                if (p >= 0) {
                    const Vec2 away = normalizeOr(Vec2{e.x[i], e.y[i]} - players_[p].pos, Vec2{1.0f, 0.0f});
                    const float sp = species_->get(e.species[i]).speed * eliteSpeed(static_cast<u32>(i)) * loopSpeed_;
                    e.x[i] = clampf(e.px[i] + away.x * sp * dt, -kArenaHalfW + e.radius[i], kArenaHalfW - e.radius[i]);
                    e.y[i] = clampf(e.py[i] + away.y * sp * dt, -kArenaHalfH + e.radius[i], kArenaHalfH - e.radius[i]);
                    e.heading[i] = angleOf(away);
                }
            }
            if (host && ev.end >= 0.0 && m1_ >= ev.end) {
                if (i >= 0 && !(enemies_.flags[i] & kFlagDead)) {
                    enemies_.flags[i] |= kFlagDead;  // it warps out (no Shards)
                    Event out;
                    out.type = EventType::Kill;
                    out.species = enemies_.species[i];
                    out.b = 0xFF;
                    out.c = 1;
                    out.x = enemies_.x[i];
                    out.y = enemies_.y[i];
                    emit(out);
                    if (role_ == NetRole::Host) logDeath(static_cast<u32>(i), 0xFF, kDeathDissolve);
                }
                endEvent();
            }
            break;
        }
        case EventKind::StormCell: {
            // It drifts toward the players' centre at 90 wu/s.
            const Vec2 to = playersCentre() - ev.pos;
            const float l = length(to);
            if (l > 1.0f) ev.pos = ev.pos + to * (std::fmin(kArStormSpeed * dt, l) / l);
            if (host && ev.end >= 0.0 && m1_ >= ev.end) endEvent();
            break;
        }
        case EventKind::Comet: {
            bool any = false;
            for (const ArenaBody& b : arena_) any = any || (b.alive && b.kind == kBodyComet);
            if (host && !any && m1_ > ev.start + kTicksPerBeat) endEvent();
            break;
        }
        case EventKind::Convoy:
        case EventKind::VaultDrop: {
            bool any = false;
            for (const ArenaBody& b : arena_) any = any || (b.alive && b.kind >= kBodyGoldVault);
            if (host && ((ev.end >= 0.0 && m1_ >= ev.end) || (!any && m1_ > ev.start + kTicksPerBeat))) endEvent();
            break;
        }
        default:
            if (host && ev.end >= 0.0 && m1_ >= ev.end) endEvent();
            break;
    }
}

// ---------------------------------------------------------------------------------------------- co-op

void World::logArena(u8 op, u8 a, u8 b, Vec2 p, Vec2 q, double tick, u32 u) {
    if (role_ != NetRole::Host) return;
    if (netLog_.bossCount >= NetLog::kBoss) {
        ++netLog_.overflow;
        return;
    }
    NetLog::Boss& o = netLog_.boss[netLog_.bossCount++];
    o = NetLog::Boss{};
    o.kind = 1;
    o.pair = static_cast<u8>(kArenaOpBase + op);
    o.boss = a;
    o.killer = b;
    o.x = p.x;
    o.y = p.y;
    o.x2 = q.x;
    o.y2 = q.y;
    o.tick = tick;
    o.changeBar = u;
}

void World::applyArenaOp(const NetOp& op) {
    // A client: the host's arena decision (op.a = kArenaOpBase + the operation, op.c its a, op.b its b, op.u its u).
    if (role_ != NetRole::Client) return;
    const u8 code = static_cast<u8>(op.a - kArenaOpBase);
    const u8 a = op.c, b = op.b;
    switch (code) {
        case 1: {  // an elite: enemy op.u with the traits in op.tick
            const i32 i = indexOfSerial(op.u);
            if (i >= 0 && !(enemies_.traits[i] & kEliteBit)) makeElite(static_cast<u32>(i), static_cast<u16>(op.tick));
            break;
        }
        case 2: {  // Twins: op.u and the serial in op.tick
            const i32 i = indexOfSerial(op.u), j = indexOfSerial(static_cast<u32>(op.tick));
            if (i >= 0 && j >= 0) {
                enemies_.twin[i] = enemies_.handle[j];
                enemies_.twin[j] = enemies_.handle[i];
            }
            break;
        }
        case 3: {  // a body placed in slot a
            if (a >= kArenaBodies) break;
            ArenaBody& o = arena_[a];
            o = ArenaBody{};
            o.kind = b;
            o.alive = 1;
            o.pos = Vec2{op.x, op.y};
            o.angle = op.x2;
            o.surge = op.y2 > 0.5f ? 1 : 0;
            o.placed = op.tick;
            o.track = static_cast<u8>(op.u);
            const float ts = trackScale();
            if (b == static_cast<u8>(StructKind::Vault)) o.hp = ts * kArVaultHealth;
            if (b == static_cast<u8>(StructKind::Rift)) o.hp = ts * kArRiftHealth;
            if (b == kBodyGoldVault) o.hp = ts * kArVaultDropHealth;
            if (b == kBodyComet) {
                o.hp = ts * kArCometHealth;
                o.vel = dirOf(op.x2) * kArCometSpeed;
            }
            if (b == kBodyCarrier) {
                o.hp = ts * kArCarrierHealth;
                o.vel = dirOf(op.x2) * kArConvoySpeed;
            }
            o.hpMax = o.hp;
            break;
        }
        case 4: startEvent(static_cast<EventKind>(a), op.tick); arenaEvent_.pos = Vec2{op.x, op.y}; arenaEvent_.dir = Vec2{op.x2, op.y2}; break;
        case 5:
            if (a < kArenaMeteors) {
                meteors_[a].at = Vec2{op.x, op.y};
                meteors_[a].land = op.tick;
                emitCue(BossCue::MeWarn, m1_, 0, kArMeteorWu, meteors_[a].at);
            }
            break;
        case 6:
            if (a < kArenaBodies && arena_[a].alive) {
                bodyRewards(a);
                arena_[a].alive = 0;
            }
            break;
        case 7:
            if (a < kArenaBodies) arena_[a].alive = 0;
            break;
        case 8: endEvent(); break;
        case 9: {
            const i32 i = indexOfSerial(op.u);
            if (i >= 0) arenaEvent_.hunted = enemies_.handle[i];
            break;
        }
        default: break;
    }
}

}  // namespace rf::sim
