// M8 part 6 (design/modes/jam.md; modes.json jam): JAM, the play room on the Stage. Nothing hurts; the band's bed is the mixer's eight
// pads (a fly-over switches a layer on the next bar line); DROP is the room's own Build and Drop; the Track pads change the room's
// Track through the Track-change Break; SOUND the band's sound; TOYS and BOOSTS open shelves of free upgrades round the ship (one
// beat in an orb takes it, every further beat a stack); EMPTY puts everything back; a Shard level is a present. The pads are the
// room's: a device asks and the host puts the change on a bar line (arena operation 10). POP-OFF (versus): five Trios, a Build, a
// Drop of sixteen bars kept at 200 bodies, a pop one and an elite five.

#include <cmath>
#include <cstring>

#include "sim/arena.h"
#include "sim/boss.h"
#include "sim/tuning.h"
#include "sim/world.h"

namespace rf::sim {

namespace {
constexpr u8 kRoomMixer = 0, kRoomDrop = 1, kRoomTrack = 2, kRoomSound = 3;
}  // namespace

u32 World::jamLayersOn() const {
    u32 n = 0;
    for (u32 k = 0; k < 8; ++k) n += (jam_.mixer >> k) & 1u;
    return n;
}

void World::resetJam() {
    // The room as it opens: KICK alone on (modes.json onAtStart), DROP ready, every shelf at its first, nobody in a pad.
    const ModesData::Jam& d = species_->modes.jam;
    jam_ = JamState{};
    jam_.mixer = jam_.mixerNext = d.onAtStart;
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        players_[p].cells = players_[p].cellCap = 4;  // (rule 1: the halo shows four cells and never changes)
        jam_.shelf[p].pad = 0xFF;
    }
}

float World::jamCrowdTarget(const Phrase& ph) const {
    // Rule 4: twenty dancers a layer on (twenty more a layer for each further player), at most 160; x 1.6 in a Drop. POP-OFF: the floor
    // kept at 200 in its Drop, the dancers gathering to half that in its Build, none in the draft.
    const ModesData::Jam& d = species_->modes.jam;
    if (setup_.versus) {
        if (ph.section == Section::Drop) return static_cast<float>(d.popCrowd);
        if (ph.section == Section::Build) return 0.5f * static_cast<float>(d.popCrowd);
        return 0.0f;
    }
    u32 present = 0;
    for (u32 p = 0; p < playerCount_; ++p) present += players_[p].present ? 1u : 0u;
    float n = static_cast<float>(d.perLayer * jamLayersOn() * (present ? present : 1u));
    n = std::fmin(n, static_cast<float>(d.crowdMax));
    if (ph.section == Section::Drop) n = std::fmin(n * d.dropScale, static_cast<float>(d.crowdMax) * d.dropScale);
    if (ph.trackChange) n = 0.0f;
    return n;
}

void World::jamAsk(u32 p, u8 pad) {
    // A player asks for a room pad (the mixer, DROP, a Track, SOUND): the host decides (solo: this world); a client tells the host.
    if (role_ == NetRole::Client) {
        Event ev;
        ev.type = EventType::JamAsk;
        ev.a = static_cast<u8>(p);
        ev.b = pad;
        emit(ev);
        return;
    }
    jamDecide(pad);
}

void World::jamDecide(u8 pad) {
    // The host: the change on the first bar line at least half a bar away (sound cards' rule: never mid-bar), told to every device.
    const ModesData::Jam& d = species_->modes.jam;
    const u32 here = barOf(m1_);
    const double into = m1_ - tickOfBar(here);
    const u32 bar = into > 0.5 * kTicksPerBar ? here + 2u : here + 1u;
    const double at = tickOfBar(bar);
    u8 kind = kRoomMixer, value = 0;
    if (pad < ModesData::Jam::kMixer) {
        if (jam_.mixerLocked) return;
        kind = kRoomMixer;
        // (two asks before the bar line: the second undoes the first)
        value = static_cast<u8>(jam_.mixerNext ^ (1u << pad));
    } else if (pad == ModesData::Jam::kPadDrop) {
        if (director_.jamDropBar != kNoBar && here < director_.jamDropBar + d.dropBuildBars + d.dropBars) return;  // (one at a time)
        if (here < jam_.dropReadyBar || director_.jamChangeBar != kNoBar) return;                               // (resting; a Track change)
        kind = kRoomDrop;
    } else if (pad == ModesData::Jam::kPadSound) {
        kind = kRoomSound;
        value = static_cast<u8>((jam_.sound + 1u) % 3u);
    } else if (pad >= ModesData::Jam::kPadTrack0 && pad < ModesData::Jam::kPadTrack0 + ModesData::Jam::kTrackPads) {
        const u8 track = static_cast<u8>(pad - ModesData::Jam::kPadTrack0 + 1u);
        if (track == director_.jamTrack || director_.jamChangeBar != kNoBar) return;
        if (director_.jamDropBar != kNoBar && here < director_.jamDropBar + d.dropBuildBars + d.dropBars) return;  // (after the Drop)
        kind = kRoomTrack;
        value = track;
    } else {
        return;
    }
    if (role_ == NetRole::Host) logArena(10, kind, value, Vec2{}, Vec2{}, at);
    jamRoomOp(kind, value, at);
}

void World::jamRoomOp(u8 kind, u8 value, double at) {
    // Every device: the room's change on its bar line (the mixer, SOUND) or its phrase from there (DROP, a Track).
    const ModesData::Jam& d = species_->modes.jam;
    const u32 bar = barOf(at + 1.0);
    switch (kind) {
        case kRoomMixer:
            jam_.mixerNext = value;
            jam_.mixerAt = at;
            break;
        case kRoomSound:
            jam_.soundNext = value;
            jam_.soundAt = at;
            break;
        case kRoomDrop:
            director_.jamDropBar = bar;
            jam_.dropReadyBar = bar + d.dropBuildBars + d.dropBars + d.dropRestBars;
            break;
        case kRoomTrack: {
            u32 track = value < 1 ? 1u : (value > kModeTracks ? kModeTracks : value);
            director_.jamNext = static_cast<u8>(track);
            director_.jamChangeBar = bar;
            if (track > kMaxSongs) {
                const ModesData::Bonus& b = species_->modes.bonus[track - kMaxSongs - 1u];
                director_.jamNextSong = static_cast<u8>(track - 1u);
                director_.jamNextBpm = static_cast<float>(b.bpm);
                director_.jamNextCrowd = b.crowdOf;
                director_.jamNextBonus = static_cast<u8>(track);
            } else {
                director_.jamNextSong = static_cast<u8>(songOfTrack(track, director_.songs));
                director_.jamNextBpm = bpmOfTrack(track, director_.songs);
                director_.jamNextCrowd = static_cast<u8>(track);
                director_.jamNextBonus = 0;
            }
            break;
        }
        default: return;
    }
    Event ev;
    ev.type = EventType::JamRoom;
    ev.a = kind;
    ev.b = value;
    ev.tick = at;
    emit(ev);
}

void World::jamTake(u32 p, u8 orb) {
    // A toy or a Boost taken from a shelf (the player's own device decides; kPeJamTake tells the others): one more stack or level up
    // to its cap (a fifth Riff sends the oldest back to the box), a Boost for eight bars, a Fusion's gold orb.
    if (p >= kMaxPlayers) return;
    const ModesData::Jam& d = species_->modes.jam;
    PlayerState& pl = players_[p];
    Loadout& l = pl.loadout;
    if (isBoostOrb(orb)) {
        const u8 b = boostOfOrb(orb);
        activateBoost(p, b);
        for (ActiveBoost& a : pl.boosts)
            if (a.type == b) a.until = m1_ + static_cast<double>(d.boostBars) * kTicksPerBar;
    } else if (isFusionOrb(orb)) {
        l.fusions = static_cast<u8>(l.fusions | (1u << fusionOfOrb(orb)));
    } else if (orb < kUpgradeCount) {
        if (isRiff(orb) && !l.riffs[riffIndex(orb)] && l.riffsOwned() >= kRiffSlots) {
            // (four Riffs at once: the one taken first goes home)
            u8 oldest = kNoUpgrade;
            for (u8 u = kOrbitals; u < kUpgradeCount; ++u)
                if (l.riffs[riffIndex(u)] && (oldest == kNoUpgrade || l.order[u] < l.order[oldest])) oldest = u;
            if (oldest != kNoUpgrade) {
                l.riffs[riffIndex(oldest)] = 0;
                l.order[oldest] = 0;
                for (u32 f = 0; f < kFusionCount; ++f)
                    if (fusionInfo(static_cast<u8>(f)).a == oldest || fusionInfo(static_cast<u8>(f)).b == oldest) l.fusions = static_cast<u8>(l.fusions & ~(1u << f));
            }
        }
        applyUpgrade(p, orb);
    } else {
        return;
    }
    Event ev;
    ev.type = EventType::JamTake;
    ev.a = static_cast<u8>(p);
    ev.b = orb;
    ev.c = static_cast<u8>(orb < kUpgradeCount ? l.level(orb) : 1u);
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    emit(ev);
}

void World::jamEmpty(u32 p) {
    // EMPTY: everything the ship holds goes back in the box (its Mods, Riffs, Fusions and Boosts; the Nova stock to its start).
    if (p >= kMaxPlayers) return;
    PlayerState& pl = players_[p];
    pl.loadout = Loadout{};
    for (ActiveBoost& a : pl.boosts) {
        if (a.type == kNoBoost) continue;
        Event end;
        end.type = EventType::BoostEnd;
        end.a = static_cast<u8>(p);
        end.b = a.type;
        emit(end);
        a = ActiveBoost{};
    }
    pl.novaCap = kNovaCapacity;
    if (pl.novas > pl.novaCap) pl.novas = pl.novaCap;
    Event ev;
    ev.type = EventType::JamEmpty;
    ev.a = static_cast<u8>(p);
    ev.x = pl.pos.x;
    ev.y = pl.pos.y;
    emit(ev);
}

u8 World::jamShelfOrb(u32 p, u32 k) const {
    // The orb k of player p's open shelf (kNoUpgrade: none); the seventh place is a due Fusion's gold orb.
    const ModesData::Jam& d = species_->modes.jam;
    const JamState::Shelf& s = jam_.shelf[p < kMaxPlayers ? p : 0];
    if (s.pad == 0xFF) return kNoUpgrade;
    if (k == 6) {
        if (s.pad != 0) return kNoUpgrade;
        const Loadout& l = players_[p].loadout;
        for (u32 f = 0; f < kFusionCount; ++f)
            if (l.fusionDue(static_cast<u8>(f))) return static_cast<u8>(kFusionOrb + f);
        return kNoUpgrade;
    }
    if (s.pad == 0) return s.index < d.toyShelves && k < 6 ? d.toy[s.index][k] : kNoUpgrade;
    return s.index < d.boostShelves && k < d.boostShelfSize[s.index] ? static_cast<u8>(kBoostOrb + d.boostShelf[s.index][k]) : kNoUpgrade;
}

Vec2 World::jamShelfOrbPos(u32 p, u32 k) const {
    // Six orbs evenly round a ring of 170 wu about where the ship stood (the Trio's ring); a Fusion's gold orb above it.
    const ModesData::Jam& d = species_->modes.jam;
    const JamState::Shelf& s = jam_.shelf[p < kMaxPlayers ? p : 0];
    if (k == 6) return s.centre + Vec2{0.0f, -d.ringWu - 70.0f};
    const float a = -1.5707963f + 6.2831853f * static_cast<float>(k) / static_cast<float>(d.orbs ? d.orbs : 6u);
    return s.centre + Vec2{std::cos(a), std::sin(a)} * d.ringWu;
}

void World::updateJam() {
    if (!species_) return;
    const ModesData::Jam& d = species_->modes.jam;
    const double beatTicks = kTicksPerBeat, dtBeats = (m1_ - m0_) / beatTicks;
    const bool barLine = crossed(m0_, m1_, kTicksPerBar);
    // The room's changes on their bar lines (every device: the host's word).
    if (jam_.mixerAt >= 0.0 && m1_ >= jam_.mixerAt) {
        jam_.mixerAt = -1.0;
        if (jam_.mixer != jam_.mixerNext) {
            const u8 was = jam_.mixer;
            jam_.mixer = jam_.mixerNext;
            Event ev;
            ev.type = EventType::JamMixer;
            ev.a = jam_.mixer;
            ev.b = was;
            emit(ev);
        }
    }
    if (jam_.soundAt >= 0.0 && m1_ >= jam_.soundAt) {
        jam_.soundAt = -1.0;
        jam_.sound = jam_.soundNext;
        Event ev;
        ev.type = EventType::JamSound;
        ev.a = jam_.sound;
        emit(ev);
    }
    if (barLine && director_.jamChangeBar != kNoBar && barOf(m1_) >= director_.jamChangeBar + kTrackChangeBars) {
        // Rule 8: past the Track-change Break the room is in its new Track (every device on the same bar line).
        DirectorState& dr = director_;
        dr.track = dr.jamNextCrowd;
        dr.bonus = dr.jamNextBonus;
        dr.bonusBpm = dr.jamNextBonus ? dr.jamNextBpm : 0.0f;
        dr.jamTrack = dr.jamNext;
        dr.jamSong = dr.jamNextSong;
        dr.jamBpm = dr.jamNextBpm;
        dr.trackBar = dr.jamChangeBar + kTrackChangeBars;
        dr.jamChangeBar = kNoBar;
        dr.jamDropBar = kNoBar;
    }
    if (barLine) {
        // Rule 1: the Nova comes back every bar. Rule 4: with five layers on an elite walks in every eight bars (the host's).
        for (u32 p = 0; p < playerCount_; ++p)
            if (players_[p].present && players_[p].novas < players_[p].novaCap) ++players_[p].novas;
        const u32 bar = barOf(m1_);
        const bool pop = setup_.versus;
        const u32 every = pop ? 4u : d.eliteEveryBars;
        const bool eliteDue = pop ? (phrase_.section == Section::Drop && phrase_.sectionBar % every == 0)
                                  : (jamLayersOn() >= d.eliteFromLayers && every && bar % every == 0);
        if (eliteDue && role_ != NetRole::Client) {
            u32 alive = 0;
            for (u32 i = 0; i < enemies_.count; ++i)
                if (!(enemies_.flags[i] & kFlagDead) && enemies_.state[i] != kWarping && !(enemies_.traits[i] & kEliteBit)) ++alive;
            if (alive) {
                u32 pick = rng_.below(alive);
                for (u32 i = 0; i < enemies_.count; ++i) {
                    if ((enemies_.flags[i] & kFlagDead) || enemies_.state[i] == kWarping || (enemies_.traits[i] & kEliteBit)) continue;
                    if (pick-- == 0) {
                        static constexpr u32 kTraits[3] = {kArmored, kSwift, kRegenerating};  // (one trait: armoured, swift or regenerating)
                        const u16 traits = traitBit(kTraits[rng_.below(3)]);
                        makeElite(i, traits);
                        if (role_ == NetRole::Host) logArena(1, 0, 0, Vec2{}, Vec2{}, static_cast<double>(traits | kEliteBit), enemies_.serial[i]);
                        break;
                    }
                }
            }
        }
        if (pop) {
            // POP-OFF: the draft (five Trios two bars each from the run's third bar), and the Drop's shared Boost orb every four bars.
            const u32 draftFrom = 2u, draftTo = draftFrom + d.picks * d.pickBars;
            if (bar >= draftFrom && bar < draftTo && (bar - draftFrom) % d.pickBars == 0)
                for (u32 p = 0; p < playerCount_; ++p)
                    if (players_[p].present && isLocal(p)) openTrio(p, tickOfBar(bar + d.pickBars) - 1.0, kTrioBreak);
            if (phrase_.section == Section::Drop && phrase_.sectionBar % 4u == 2u && role_ != NetRole::Client) {
                const Vec2 at{rng_.range(-0.6f, 0.6f) * halfW(), rng_.range(-0.5f, 0.5f) * halfH()};
                dropBoost(at, static_cast<u8>(rng_.below(kBoostCount)));
            }
        }
    }
    if (setup_.versus) return;  // (POP-OFF: no pads, no shelves)
    // The pads, each device for its own players: the mixer by a fly-over (entering it), the others by a two-beat hold (the Trio's).
    for (u32 p = 0; p < playerCount_; ++p) {
        PlayerState& pl = players_[p];
        if (!pl.present || !pl.alive || !isLocal(p)) continue;
        u32 inside = 0;
        for (u32 k = 0; k < ModesData::Jam::kPads; ++k) {
            const ModesData::Jam::Pad& pad = d.pad[k];
            const bool in = lengthSq(pl.pos - pad.at) <= pad.radiusWu * pad.radiusWu;
            if (!in) {
                jam_.hold[p][k] = 0.0f;
                continue;
            }
            inside |= 1u << k;
            const bool entered = !((jam_.inside[p] >> k) & 1u);
            if (pad.holdBeats <= 0.0f) {
                if (entered) jamAsk(p, static_cast<u8>(k));
                continue;
            }
            float& h = jam_.hold[p][k];
            if (h < 0.0f) continue;  // (done until it leaves)
            h += static_cast<float>(dtBeats);
            if (h < pad.holdBeats) continue;
            h = -1.0f;
            Event held;
            held.type = EventType::JamHold;
            held.a = static_cast<u8>(p);
            held.b = static_cast<u8>(k);
            held.x = pad.at.x;
            held.y = pad.at.y;
            emit(held);
            if (k == ModesData::Jam::kPadToys || k == ModesData::Jam::kPadBoosts) {
                // The next shelf of that pad, round where the ship stands.
                JamState::Shelf& s = jam_.shelf[p];
                const bool toys = k == ModesData::Jam::kPadToys;
                u8& next = toys ? jam_.toysNext[p] : jam_.boostsNext[p];
                s.pad = toys ? 0 : 1;
                s.index = next;
                next = static_cast<u8>((next + 1u) % (toys ? d.toyShelves : d.boostShelves));
                s.centre = pl.pos;
                s.closeAt = -1.0;
                for (float& o : s.orbHold) o = 0.0f;
                Event ev;
                ev.type = EventType::JamShelf;
                ev.a = static_cast<u8>(p);
                ev.b = s.pad;
                ev.c = s.index;
                ev.x = s.centre.x;
                ev.y = s.centre.y;
                emit(ev);
            } else if (k == ModesData::Jam::kPadEmpty) {
                jamEmpty(p);
            } else {
                jamAsk(p, static_cast<u8>(k));
            }
        }
        jam_.inside[p] = inside;
        // The shelf: an orb held a beat is taken, every further beat a stack; it stays while the ship is within 300 wu and closes two
        // bars after it leaves.
        JamState::Shelf& s = jam_.shelf[p];
        if (s.pad == 0xFF) continue;
        const bool near = lengthSq(pl.pos - s.centre) <= d.staysWithinWu * d.staysWithinWu;
        if (near) {
            s.closeAt = -1.0;
        } else if (s.closeAt < 0.0) {
            s.closeAt = m1_ + static_cast<double>(d.closesAfterBars) * kTicksPerBar;
        } else if (m1_ >= s.closeAt) {
            s.pad = 0xFF;
            continue;
        }
        for (u32 k = 0; k < 7; ++k) {
            const u8 orb = jamShelfOrb(p, k);
            if (orb == kNoUpgrade) {
                s.orbHold[k] = 0.0f;
                continue;
            }
            const Vec2 at = jamShelfOrbPos(p, k);
            if (lengthSq(pl.pos - at) > tuning::kTrioOrbWu * tuning::kTrioOrbWu) {
                s.orbHold[k] = 0.0f;
                continue;
            }
            const float before = s.orbHold[k];
            s.orbHold[k] += static_cast<float>(dtBeats);
            if (std::floor(s.orbHold[k] / d.takeBeats) > std::floor(before / d.takeBeats)) {
                const bool can = orb >= kUpgradeCount || pl.loadout.level(orb) < upgradeInfo(orb).cap;
                if (can) jamTake(p, orb);
            }
        }
    }
}

}  // namespace rf::sim
