// M5 part 5: elites and their twelve traits (design/traits.md, traits.json; GAME_DESIGN 7.3). An elite is a species of the
// Track's roster made dangerous: x 1.3 its size, x 6 its health, x 10 its score, x 4 its threat, 20 Shards and a Cell. The
// director buys one with the threat budget about once in 16 bars in Track 2, once in 8 by Track 6; never in a Break or a
// fight; three at once in Tracks 2-6, five from Track 7. Each carries traits drawn from the ones the run has met.
//
// Co-op: the host buys elites and draws their traits; a client hears of each as an arena operation naming the enemy's
// spawn number (World::applyArenaOp) and makes the same elite. What a trait does at a death (Splitting's copies are the
// host's spawns; Volatile's bomb, Magnetic's Shards, Commander's stun) happens on every device when the death is known.

#include <cmath>

#include "sim/boss_prose.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

using namespace tuning;
using namespace prose;

namespace {
constexpr float kDegR = kTau / 360.0f;
}  // namespace

u32 World::elitesAlive() const {
    u32 n = 0;
    for (u32 i = 0; i < enemies_.count; ++i) n += (enemies_.traits[i] & kEliteBit) && !(enemies_.flags[i] & kFlagDead) ? 1u : 0u;
    return n;
}

u16 World::drawTraits(u32 track) {
    // traits.md: one in Tracks 2-6, two in 7-10, in the loop one more each time round (at most four); drawn from the traits
    // the run has met (their Track reached); never Cloaked and Phasing on one body.
    const TraitData& td = species_->traits;
    const u32 songs = director_.songs;
    const u32 met = inLoop(track, songs) ? songs : track;
    u32 n = (track >= 7 && track <= 10 && !inLoop(track, songs)) ? td.carried[1] : td.carried[0];
    if (inLoop(track, songs)) n = (songs >= 7 ? td.carried[1] : td.carried[0]) + (track - songs - 1) / (songs ? songs : 1u) + 1u;
    if (n > 4) n = 4;
    u8 pool[kTraitCount];
    u32 m = 0;
    for (u32 t = 0; t < kTraitCount; ++t)
        if (td.fromTrack[t] <= met) pool[m++] = static_cast<u8>(t);
    u16 out = 0;
    for (u32 k = 0; k < n && m; ++k) {
        const u32 pick = static_cast<u32>(rand01(arenaRng_) * static_cast<float>(m)) % m;
        const u32 t = pool[pick];
        pool[pick] = pool[--m];
        if ((t == kCloaked && hasTrait(out, kPhasing)) || (t == kPhasing && hasTrait(out, kCloaked))) {
            --k;  // (draw again)
            continue;
        }
        out = static_cast<u16>(out | traitBit(t));
    }
    return out;
}

void World::eliteBar(const Phrase& ph, double land) {
    // traits.md "How often": from Track 2's second cycle, never in a Break, a boss fight or a mini-boss fight; about once in 16
    // bars in Track 2 falling evenly to once in 8 by Track 6 (and so on). The budget pays x 4 the species' threat.
    DirectorState& d = director_;
    if (ph.track < 2 || (ph.track == 2 && ph.cycle < 2) || ph.section == Section::Break || ph.trackChange || ph.bossDrop || boss_.active) return;
    const float t = static_cast<float>(ph.track < 6 ? ph.track : 6u);
    const float every = kElPaceBarsT2 + (kElPaceBarsT6 - kElPaceBarsT2) * (t - 2.0f) / 4.0f;
    eliteClock_ += 1.0f / every;
    if (eliteClock_ < 1.0f) return;
    const TraitData& td = species_->traits;
    const u32 cap = (ph.track >= 7 || inLoop(ph.track, d.songs)) ? td.alive[1] : td.alive[0];
    if (elitesAlive() >= cap || enemies_.count + 2 >= aliveCap()) return;
    // Any species of the Track's roster but Shoal darts and Wyrm segments (decisions: no Wyrm heads, no Tether pairs yet).
    u16 cand[SpeciesTable::kMaxRoster];
    u32 n = 0;
    for (u32 k = 0; k < species_->rosterCount; ++k) {
        const SpeciesTable::RosterEntry& r = species_->roster[k];
        const Family f = species_->get(r.species).family;
        if (!familyBuilt(f) || f == Family::Shoal || f == Family::Wyrm || f == Family::Tether) continue;
        const bool debuted = inLoop(ph.track, d.songs) ? r.track <= d.songs : (r.track < ph.track || (r.track == ph.track && r.cycle <= ph.cycle));
        if (debuted) cand[n++] = r.species;
    }
    if (!n) return;
    const u16 sp = cand[static_cast<u32>(rand01(arenaRng_) * static_cast<float>(n)) % n];
    const float cost = species_->get(sp).threat * td.threat;
    if (d.bank + 1e-4f < cost && cost <= d.budgetPerBar * kBankBars) return;  // it waits for the bank
    d.bank = std::fmax(d.bank - cost, 0.0f);
    eliteClock_ -= 1.0f;
    // Where a threat formation lands: round the formation's player, outside the central third of its view.
    u32 fp = 0;
    const Vec2 c = formationCentre(&fp);
    const float a = randRange(arenaRng_, 0.0f, kTau);
    const float r = randRange(arenaRng_, 0.6f * kFrameHalfWWu, 0.9f * kFrameHalfWWu);
    spawnElite(sp, c + dirOf(a) * r, land, drawTraits(ph.track));
}

i32 World::spawnElite(u16 species, Vec2 pos, double land, u16 traits) {
    const i32 i = spawnEnemy(species, pos, 0.0f, land);
    if (i < 0) return -1;
    makeElite(static_cast<u32>(i), static_cast<u16>(traits | kEliteBit));
    if (hasTrait(traits, kTwin)) {
        // Twin: a second body with the one health bar, 200 to 300 wu away.
        const TraitData& td = species_->traits;
        const float a = randRange(arenaRng_, 0.0f, kTau);
        const i32 j = spawnEnemy(species, pos + dirOf(a) * (0.5f * (td.twinApart[0] + td.twinApart[1])), 0.0f, land);
        if (j >= 0) {
            makeElite(static_cast<u32>(j), static_cast<u16>(traits | kEliteBit));
            enemies_.twin[i] = enemies_.handle[j];
            enemies_.twin[j] = enemies_.handle[i];
            if (role_ == NetRole::Host) logArena(1, 0, 0, Vec2{}, Vec2{}, static_cast<double>(traits | kEliteBit), enemies_.serial[j]);
        }
    }
    if (role_ == NetRole::Host) logArena(1, 0, 0, Vec2{}, Vec2{}, static_cast<double>(traits | kEliteBit), enemies_.serial[i]);
    if (role_ == NetRole::Host && enemies_.twin[i]) {
        const i32 j = indexOf(enemies_.twin[i]);
        if (j >= 0) logArena(2, 0, 0, Vec2{}, Vec2{}, static_cast<double>(enemies_.serial[j]), enemies_.serial[i]);
    }
    return i;
}

void World::makeElite(u32 i, u16 traits) {
    // traits.md: x 1.3 the size, x 6 the health (Armored x 2 again, Giant x 1.8 and x 2.5, Twin's bar x 1.6), co-op x (1 + 0.5
    // per extra player) as mini-bosses and bosses.
    Enemies& e = enemies_;
    const TraitData& td = species_->traits;
    float size = td.scale, health = td.health * (1.0f + kCoopBossHealthPerPlayer * static_cast<float>(presentCount() - 1));
    if (hasTrait(traits, kArmored)) health *= td.armoredHealth;
    if (hasTrait(traits, kGiant)) {
        size *= td.giantScale;
        health *= td.giantHealth;
    }
    if (hasTrait(traits, kTwin)) health *= td.twinHealth;
    e.traits[i] = traits;
    e.radius[i] *= size;
    e.hp[i] *= health;
    e.hpMax[i] = e.hp[i];
    e.lastHit[i] = m1_;
    e.shield[i] = e.heading[i];
    Event ev;
    ev.type = EventType::BossCue;
    ev.a = static_cast<u8>(BossCue::ElArrive);
    ev.b = static_cast<u8>(traits & 0xFF);
    ev.c = static_cast<u8>((traits >> 8) & 0x0F);
    ev.species = e.species[i];
    ev.x = e.x[i];
    ev.y = e.y[i];
    ev.tick = e.until[i] > m1_ ? e.until[i] : m1_;
    emit(ev);
    // Its trait's mark sounds a beat later (the first trait's).
    for (u32 t = 0; t < kTraitCount; ++t)
        if (hasTrait(traits, t)) {
            emitCue(static_cast<BossCue>(static_cast<u32>(BossCue::ElArmored) + t), ev.tick + kTicksPerBeat, static_cast<u8>(t), 0.0f, Vec2{e.x[i], e.y[i]});
            break;
        }
}

float World::eliteSpeed(u32 i) const {
    // Swift (x 1.6) and a Commander's reach (x 1.3 for every body within 400 wu of one, the Commander too).
    const Enemies& e = enemies_;
    float k = 1.0f;
    if (e.traits[i] & kEliteBit) {
        if (hasTrait(e.traits[i], kSwift)) k *= species_->traits.swiftSpeed;
        // Elite Hunt: the hunted one flees at 0.8 of its speed (World::updateEvent turns it).
        if (arenaEvent_.kind == static_cast<u8>(EventKind::EliteHunt) && arenaEvent_.stage == 1 && e.handle[i] == arenaEvent_.hunted) k *= kArHuntFlee;
    }
    if (commanderCount_) {
        const float r2 = species_->traits.commanderReach * species_->traits.commanderReach;
        for (u32 c = 0; c < commanderCount_; ++c)
            if (lengthSq(Vec2{e.x[i], e.y[i]} - commanders_[c]) <= r2) {
                k *= species_->traits.commanderSpeed;
                break;
            }
    }
    return k;
}

bool World::elitePhased(u32 i) const {
    // Phasing: solid only in the first half of each beat.
    const Enemies& e = enemies_;
    if (!(e.traits[i] & kEliteBit) || !hasTrait(e.traits[i], kPhasing)) return false;
    const double inBeat = m1_ - std::floor(m1_ / kTicksPerBeat) * kTicksPerBeat;
    return inBeat >= species_->traits.phasingSolid * kTicksPerBeat;
}

bool World::eliteSheds(u32 i, Vec2 shotDir) const {
    // Shielded: its front 120 degrees sheds every shot (the shot comes at the front when it travels against the facing).
    const Enemies& e = enemies_;
    if (!(e.traits[i] & kEliteBit) || !hasTrait(e.traits[i], kShielded)) return false;
    const float c = std::cos(0.5f * species_->traits.shieldArc * kDegR);
    return dot(shotDir * -1.0f, dirOf(e.shield[i])) > c;
}

void World::eliteHit(u32 i, float dmg) {
    // Regenerating's quiet starts again; a Twin's other body takes the same (one health bar); Armored's outer hull shatters
    // at half its health.
    Enemies& e = enemies_;
    if (!(e.traits[i] & kEliteBit)) return;
    e.lastHit[i] = m1_;
    if (hasTrait(e.traits[i], kArmored) && e.hp[i] + dmg > e.hpMax[i] * species_->traits.armoredHull &&
        e.hp[i] <= e.hpMax[i] * species_->traits.armoredHull)
        emitCue(BossCue::ElShatter, m1_, 0, e.radius[i], Vec2{e.x[i], e.y[i]});
    if (e.twin[i]) {
        const i32 j = indexOf(e.twin[i]);
        if (j >= 0 && !(e.flags[j] & kFlagDead)) {
            e.hp[j] = e.hp[i];
            e.lastHit[j] = m1_;
        }
    }
}

void World::updateElites(float dt) {
    // Every tick: the Commanders' reach (for eliteSpeed), the shields turning to the nearest player, Twins keeping 200-300 wu
    // apart; on each beat Regenerating heals 8% after two quiet beats.
    Enemies& e = enemies_;
    const TraitData& td = species_->traits;
    commanderCount_ = 0;
    const bool beat = crossed(m0_, m1_, kTicksPerBeat);
    for (u32 i = 0; i < e.count; ++i) {
        if (!(e.traits[i] & kEliteBit) || (e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
        const u16 t = e.traits[i];
        const Vec2 at{e.x[i], e.y[i]};
        if (hasTrait(t, kCommander) && commanderCount_ < 8) commanders_[commanderCount_++] = at;
        if (hasTrait(t, kShielded)) {
            const i32 p = nearestPlayer(at.x, at.y);
            if (p >= 0) e.shield[i] = turnToward(e.shield[i], angleOf(players_[p].pos - at), td.shieldTurn * dt);
        }
        if (hasTrait(t, kRegenerating) && beat && m1_ - e.lastHit[i] >= static_cast<double>(td.regenQuiet) * kTicksPerBeat &&
            e.hp[i] < e.hpMax[i]) {
            e.hp[i] = std::fmin(e.hpMax[i], e.hp[i] + td.regenHeal * e.hpMax[i]);
            emitCue(BossCue::ElRegen, m1_, 0, 0.0f, at);
        }
        if (hasTrait(t, kTwin) && e.twin[i]) {
            const i32 j = indexOf(e.twin[i]);
            if (j >= 0 && !(e.flags[j] & kFlagDead)) {
                const Vec2 d = Vec2{e.x[j], e.y[j]} - at;
                const float l = length(d);
                if (l > 1e-3f && (l < td.twinApart[0] || l > td.twinApart[1])) {
                    const float want = l < td.twinApart[0] ? td.twinApart[0] : td.twinApart[1];
                    const Vec2 n = d * (1.0f / l);
                    // Half the gap each a tick, at most the body's own speed.
                    const float step = std::fmin(0.5f * std::fabs(l - want), 200.0f * dt) * (l < want ? -1.0f : 1.0f);
                    e.x[i] += n.x * step;
                    e.y[i] += n.y * step;
                }
            }
        }
    }
    updateBombs();
}

void World::eliteDeath(u32 i, u8 killer) {
    // What a trait does as its body dies (every device: the host's spawns travel as spawns).
    Enemies& e = enemies_;
    const u16 t = e.traits[i];
    if (!(t & kEliteBit)) return;
    const TraitData& td = species_->traits;
    const Vec2 at{e.x[i], e.y[i]};
    if (hasTrait(t, kSplitting) && role_ != NetRole::Client) {
        // Two copies, 0.6 of its size, each with 30% of its health, no trait, no further split.
        emitCue(BossCue::ElSplit, m1_, 0, 0.0f, at);
        for (u32 k = 0; k < td.splitCopies; ++k) {
            const float a = e.heading[i] + (k ? 0.5f : -0.5f) * kPi;
            const i32 j = spawnEnemy(e.species[i], at + dirOf(a) * e.radius[i], a, m1_);
            if (j < 0) continue;
            e.radius[j] = e.radius[i] * td.splitScale;
            e.hp[j] = e.hpMax[j] = e.hpMax[i] * td.splitHealth;
        }
    }
    if (hasTrait(t, kVolatile)) {
        // A danger ring of 160 wu fills over two beats and bursts on the beat.
        for (ArenaBomb& b : bombs_)
            if (b.burst < 0.0) {
                b.at = at;
                b.burst = (std::floor(m1_ / kTicksPerBeat) + static_cast<double>(td.volatileFuse)) * kTicksPerBeat;
                if (b.burst < m1_ + static_cast<double>(td.volatileFuse) * kTicksPerBeat - kTicksPerBeat * 0.5) b.burst += kTicksPerBeat;
                b.killer = killer;
                emitCue(BossCue::ElFuse, m1_, 0, td.volatileRing, at);
                break;
            }
    }
    if (hasTrait(t, kMagnetic)) {
        // Its Shards are released doubled.
        Shards& h = shards_;
        const u32 n0 = h.count;
        for (u32 k = 0; k < n0; ++k) {
            if (h.dead[k] || h.holder[k] != e.handle[i]) continue;
            h.holder[k] = 0;
            const Vec2 v = dirOf(randRange(e.rng[i], 0.0f, kTau)) * randRange(e.rng[i], kShardBurstLo, kShardBurstHi);
            h.vx[k] = v.x;
            h.vy[k] = v.y;
            for (u32 c = 1; c < static_cast<u32>(td.magnetRelease + 0.5f); ++c)
                spawnShard(Vec2{h.x[k], h.y[k]}, dirOf(randRange(e.rng[i], 0.0f, kTau)) * randRange(e.rng[i], kShardBurstLo, kShardBurstHi), h.owner[k]);
        }
    }
    if (hasTrait(t, kCommander)) {
        // Every body within 400 wu is stunned for a beat.
        const float r2 = td.commanderReach * td.commanderReach;
        const double until = m1_ + static_cast<double>(td.commanderStun) * kTicksPerBeat;
        for (u32 j = 0; j < e.count; ++j)
            if (j != i && !(e.flags[j] & kFlagDead) && lengthSq(Vec2{e.x[j], e.y[j]} - at) <= r2) e.stunUntil[j] = until;
        emitCue(BossCue::ElStun, m1_, 0, td.commanderReach, at);
    }
    if (e.twin[i] && role_ != NetRole::Client) {
        // They die together.
        const i32 j = indexOf(e.twin[i]);
        e.twin[i] = 0;
        if (j >= 0 && !(e.flags[j] & kFlagDead)) {
            e.twin[j] = 0;
            killEnemy(static_cast<u32>(j), killer);
        }
    }
    if (arenaEvent_.kind == static_cast<u8>(EventKind::EliteHunt) && arenaEvent_.stage == 1 && e.handle[i] == arenaEvent_.hunted) {
        // Killed within the twelve bars: a Trio for each player (as a level's) and 150 Shards.
        arenaEvent_.hits = 1;
        for (u32 p = 0; p < playerCount_; ++p) {
            if (!players_[p].present || !players_[p].alive || !isLocal(p)) continue;
            for (u32 k = 0; k < kArHuntShards; ++k)
                spawnShard(at, dirOf(randRange(e.rng[i], 0.0f, kTau)) * randRange(e.rng[i], kShardBurstLo, kShardBurstHi), static_cast<u8>(p));
            ++players_[p].levelsBanked;
        }
        emitCue(BossCue::HuKill, m1_, 0, 0.0f, at);
        endEvent();
    }
}

void World::eliteShards(u32 i, u32 p) {
    // An elite drops 20 Shards and one Cell, whatever the player's health (not in the loop: no kill drops a Cell there).
    Enemies& e = enemies_;
    const TraitData& td = species_->traits;
    const u32 n = players_[p].boost(kMagnetStorm) ? 2u * td.shards : td.shards;
    for (u32 k = 0; k < n && shards_.count + kShardsReserve < Shards::kCap; ++k)
        spawnShard(Vec2{e.x[i], e.y[i]}, dirOf(randRange(e.rng[i], 0.0f, kTau)) * randRange(e.rng[i], kShardBurstLo, kShardBurstHi), static_cast<u8>(p));
    if (!inLoop(phrase_.track, director_.songs) && players_[p].alive)
        spawnCell(Vec2{e.x[i], e.y[i]}, dirOf(randRange(e.rng[i], 0.0f, kTau)) * kShardBurstLo, static_cast<u8>(p));
}

void World::holdShards(float dt) {
    // Magnetic: loose Shards within 300 wu are stolen, up to forty a body; they orbit it until it dies.
    Enemies& e = enemies_;
    Shards& h = shards_;
    const TraitData& td = species_->traits;
    bool any = false;
    for (u32 i = 0; i < e.count && !any; ++i) any = (e.traits[i] & kEliteBit) && hasTrait(e.traits[i], kMagnetic) && !(e.flags[i] & kFlagDead);
    for (u32 k = 0; k < h.count; ++k) {
        if (h.dead[k]) continue;
        if (h.holder[k]) {
            const i32 i = indexOf(h.holder[k]);
            if (i < 0 || (e.flags[i] & kFlagDead)) {
                h.holder[k] = 0;  // (its holder left without dying here: the Shard falls loose)
                continue;
            }
            const float a = static_cast<float>(m1_ / kTicksPerBar) * kTau * 0.5f + static_cast<float>(k % 40u) * (kTau / 40.0f);
            const float r = e.radius[i] * 1.8f + 6.0f * static_cast<float>(k % 3u);
            h.x[k] = e.x[i] + std::cos(a) * r;
            h.y[k] = e.y[i] + std::sin(a) * r;
            h.vx[k] = h.vy[k] = 0.0f;
            h.born[k] = m1_;  // (held Shards do not age)
            continue;
        }
        if (!any || h.homing[k]) continue;
        const float r2 = td.magnetReach * td.magnetReach;
        for (u32 i = 0; i < e.count; ++i) {
            if (!(e.traits[i] & kEliteBit) || !hasTrait(e.traits[i], kMagnetic) || (e.flags[i] & kFlagDead) || e.held[i] >= td.magnetMax) continue;
            if (lengthSq(Vec2{h.x[k] - e.x[i], h.y[k] - e.y[i]}) > r2) continue;
            h.holder[k] = e.handle[i];
            ++e.held[i];
            break;
        }
    }
    (void)dt;
}

void World::updateBombs() {
    // Volatile: on its beat the ring bursts: a cell from every ship inside, 30 x the Track's number to every enemy inside (a
    // Volatile elite among them goes off in turn, through its own death).
    const TraitData& td = species_->traits;
    for (ArenaBomb& b : bombs_) {
        if (b.burst < 0.0 || m1_ < b.burst) continue;
        const Vec2 at = b.at;
        b.burst = -1.0;
        emitCue(BossCue::ElBurst, m1_, 0, td.volatileRing, at);
        for (u32 p = 0; p < playerCount_; ++p) {
            PlayerState& pl = players_[p];
            if (!pl.present || !pl.alive || !isLocal(p) || pl.invulnerable() || rules_.godMode) continue;
            if (lengthSq(pl.pos - at) <= td.volatileRing * td.volatileRing && !haloBlocks(p, at)) hitPlayer(p, at);
        }
        if (role_ != NetRole::Client) ringDamage(at, td.volatileRing, td.volatileDamage * static_cast<float>(phrase_.track), b.killer);
    }
}

}  // namespace rf::sim
