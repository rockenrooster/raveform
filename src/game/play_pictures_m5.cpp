// M5: --fight (a run that jumps to a fight: benchmarks and pictures) and the m5_ review pictures (scripts/review-m5.ps1):
// for each boss every phase with its weak point shown and its death, for each mini-boss its tell, its attack, its x 2
// moment and its death, and one 8-frame strip across each one's most eventful beat. The careful bot plays the fight with
// hits costing nothing; each picture waits for its moment (sim state), at most 60 s.

#include <cmath>
#include <cstring>

#include "core/log.h"
#include "game/play.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
bool starts(const char* a, const char* b) { return std::strncmp(a, b, std::strlen(b)) == 0; }

enum class Moment : u8 {
    Crest, Jaw, Lunge, Death,                                   // Longwave (Death: every fight)
    Sing, Cage, Fugue, Held,                                    // the Choir
    Howl, Charge, Winded,                                       // Alpha Hound
    Tell, Ring, Spear, Wall,                                    // The Hive
    Lock, Rush, Stun,                                           // Battering Ram
    Bounce, Hot, Split,                                         // Carom King
};

struct Pic {
    const char* name;   // m5_<name>
    const char* fight;  // sim/boss.h bossId
    u8 phase;           // --fight's phase
    Moment moment;
    bool strip;         // eight frames, four apart: a beat
};

const Pic kPics[] = {
    {"lw_swell", "longwave", 0, Moment::Crest, false},
    {"lw_jaw", "longwave", 0, Moment::Jaw, false},
    {"lw_chop", "longwave", 1, Moment::Crest, false},
    {"lw_whitecap", "longwave", 2, Moment::Lunge, false},
    {"lw_bare", "longwave", 3, Moment::Jaw, false},
    {"lw_death", "longwave", 2, Moment::Death, false},
    {"lw_strip", "longwave", 1, Moment::Crest, true},
    {"ch_unison", "choir", 0, Moment::Sing, false},
    {"ch_canon", "choir", 1, Moment::Sing, false},
    {"ch_cage", "choir", 1, Moment::Cage, false},
    {"ch_fugue", "choir", 2, Moment::Fugue, false},
    {"ch_solo", "choir", 3, Moment::Held, false},
    {"ch_death", "choir", 1, Moment::Death, false},
    {"ch_strip", "choir", 1, Moment::Cage, true},
    {"alpha_tell", "alpha_hound", 0, Moment::Howl, false},
    {"alpha_attack", "alpha_hound", 0, Moment::Charge, false},
    {"alpha_x2", "alpha_hound", 0, Moment::Winded, false},
    {"alpha_half", "alpha_hound", 1, Moment::Howl, false},
    {"alpha_death", "alpha_hound", 0, Moment::Death, false},
    {"alpha_strip", "alpha_hound", 1, Moment::Charge, true},
    {"hive_tell", "the_hive", 0, Moment::Tell, false},
    {"hive_ring", "the_hive", 0, Moment::Ring, false},
    {"hive_spear", "the_hive", 0, Moment::Spear, false},
    {"hive_wall", "the_hive", 0, Moment::Wall, false},
    {"hive_half", "the_hive", 1, Moment::Ring, false},
    {"hive_death", "the_hive", 0, Moment::Death, false},
    {"hive_strip", "the_hive", 0, Moment::Tell, true},
    {"ram_tell", "battering_ram", 0, Moment::Lock, false},
    {"ram_attack", "battering_ram", 0, Moment::Rush, false},
    {"ram_x2", "battering_ram", 0, Moment::Stun, false},
    {"ram_death", "battering_ram", 0, Moment::Death, false},
    {"ram_strip", "battering_ram", 1, Moment::Rush, true},
    {"carom_tell", "carom_king", 0, Moment::Bounce, false},
    {"carom_x2", "carom_king", 0, Moment::Hot, false},
    {"carom_split", "carom_king", 2, Moment::Split, false},
    {"carom_death", "carom_king", 1, Moment::Death, false},
    {"carom_strip", "carom_king", 3, Moment::Bounce, true},
};
constexpr u32 kPicCount = sizeof(kPics) / sizeof(kPics[0]);

// Inside the picture's frame around the ship (the camera follows it; at the fights' 0.75 a little more).
bool inFrame(Vec2 p, Vec2 ship, float zoom) {
    const float hw = 760.0f / zoom, hh = 330.0f / zoom;
    return std::fabs(p.x - ship.x) < hw && std::fabs(p.y - ship.y) < hh;
}
}  // namespace

// ---------------------------------------------------------------------------------------------- --fight

bool Play::setupFight(const char* id, u8 phase) {
    BossKind kind = BossKind::Kaleidos;
    if (!bossFromId(id, kind) || static_cast<u32>(kind) > static_cast<u32>(BossKind::CaromKing)) {
        RF_ERR("play: --fight '%s': no such fight built (kaleidos, longwave, choir, alpha_hound, the_hive, battering_ram, carom_king)", id);
        return false;
    }
    World& w = *world_;
    // Its home Track: a boss's own, a mini-boss's the Track whose middle Drop draws it (minibosses.json common.pick).
    u32 home = static_cast<u32>(kind) + 1u;
    if (isMiniBoss(kind)) home = (kind == BossKind::AlphaHound || kind == BossKind::Hive) ? 2u : 3u;
    if (home > w.setList()) home = w.setList();
    w.directorMut().track = static_cast<u8>(home);
    if (isMiniBoss(kind)) w.forceMini(kind);
    w.spawner().mode = Spawner::Mode::Slice;
    fightJump_ = true;
    fightKind_ = kind;
    fightPhase_ = phase;
    fightPhaseDone_ = false;
    fightPhaseAt_ = -1.0;
    // The fast-forward plays the Track on to the fight (fightReady stops it): at most ten minutes.
    scriptFastForwardS_ = std::fmax(scriptFastForwardS_, 600.0f);
    RF_LOG("play: --fight %s (phase %u): the run starts in Track %u and plays on to it", bossId(kind), phase, home);
    return true;
}

void Play::applyFightPhase() {
    // Its parts taken down to the phase asked for, as the host's verdicts would (killBossNextTick, bossHealthNow).
    World& w = *world_;
    const BossState& b = w.boss();
    const u8 ph = fightPhase_;
    if (!ph) return;
    switch (b.kind) {
        case BossKind::Kaleidos: w.killBossNextTick((1u << (ph < 3 ? ph : 3)) - 1u, false); break;
        case BossKind::Longwave: {
            // From the tail: 8 left in Chop, 16 gone; all 24 for the bare head.
            const u32 n = w.species().longwave.segments, keep = ph == 1 ? 16u : (ph == 2 ? 8u : 0u);
            u32 mask = 0;
            for (u32 k = keep; k < n; ++k) mask |= 1u << k;
            w.killBossNextTick(mask, false);
            break;
        }
        case BossKind::Choir: {
            static const u32 kKill[4] = {0x00, 0x22, 0xAA, 0xEE};  // Canon: 1, 5; Fugue: 1, 3, 5, 7; Solo: all but 0 and 4
            w.killBossNextTick(kKill[ph < 4 ? ph : 3], false);
            break;
        }
        case BossKind::CaromKing: w.bossHealthNow(1.0f - 0.25f * static_cast<float>(ph < 3 ? ph : 3) - 0.01f); break;
        default: w.bossHealthNow(0.49f); break;
    }
}

void Play::holdFight() {
    World& w = *world_;
    const BossState& b = w.boss();
    if (!fightJump_ || !opt_.fightHold || !fightPhaseDone_ || !b.active || b.dying || b.kind != fightKind_) return;
    float share = 1.0f;
    if (b.kind == BossKind::CaromKing) share = 1.0f - 0.25f * static_cast<float>(fightPhase_ < 3 ? fightPhase_ : 3) - 0.01f;
    else if (isMiniBoss(b.kind) && fightPhase_) share = 0.49f;
    w.bossRefill(share);
}

bool Play::fightReady() {
    const BossState& b = world_->boss();
    if (!b.active || b.dying || b.kind != fightKind_ || music1_ < b.fightTick) return false;
    if (!fightPhaseDone_) {
        applyFightPhase();
        fightPhaseDone_ = true;
        fightPhaseAt_ = music1_;
        return false;
    }
    // A phase change settles on a beat or bar line; the first phase starts at once.
    return music1_ >= fightPhaseAt_ + (fightPhase_ ? 2.0 : 0.25) * kTicksPerBar;
}

// ---------------------------------------------------------------------------------------------- the m5_ pictures

// ---- M5 part 2: the families of Tracks 2 and 3 at their moments (a scripted group round a still ship, the spawner off),
// and the bot's run in Tracks 2 and 3 with their rosters.
namespace {
struct FamPic {
    const char* name;  // m5_fam_<name> or m5_<name>
    Family family;
    u8 tier, variant, count;
    bool fire, strip;
    u8 track, cycle;   // a Track picture (count 0): the bot's run, its cycle's Groove, four bars in
};
const FamPic kFamPics[] = {
    {"fam_flanker", Family::Flanker, 2, kVariantB, 4, false, false, 0, 0},
    {"fam_skitter", Family::Skitter, 2, kVariantA, 3, true, false, 0, 0},
    {"fam_moth", Family::Moth, 2, kVariantB, 6, false, false, 0, 0},
    {"fam_moth_strip", Family::Moth, 2, kVariantB, 6, false, true, 0, 0},
    {"fam_brood", Family::Brood, 1, kVariantB, 2, false, false, 0, 0},
    {"fam_brood_c", Family::Brood, 2, kVariantC, 1, true, false, 0, 0},
    {"fam_blink", Family::Blink, 2, kVariantB, 2, false, false, 0, 0},
    {"fam_blink_strip", Family::Blink, 2, kVariantB, 1, false, true, 0, 0},
    {"fam_hound_c", Family::Hound, 2, kVariantC, 4, true, false, 0, 0},
    {"fam_drifter_b", Family::Drifter, 1, kVariantB, 5, false, false, 0, 0},
    {"fam_carom_b", Family::Carom, 1, kVariantB, 3, false, false, 0, 0},
    {"t2_c3", Family::Drifter, 0, 0, 0, false, false, 2, 3},
    {"t3_c2", Family::Drifter, 0, 0, 0, false, false, 3, 2},
    {"t3_c3", Family::Drifter, 0, 0, 0, false, false, 3, 3},
};
constexpr u32 kFamPicCount = sizeof(kFamPics) / sizeof(kFamPics[0]);
constexpr u8 kFamBase = 100;  // m5Pic_ for a FamPic: kFamBase + its index
}  // namespace

bool Play::scriptSetupM5() {
    const char* p = opt_.picture;
    if (!starts(p, "m5_")) return false;
    for (u32 k = 0; k < kFamPicCount; ++k)
        if (std::strcmp(p + 3, kFamPics[k].name) == 0) m5Pic_ = static_cast<u8>(kFamBase + k);
    if (m5Pic_ != 0xFF) {
        const FamPic& f = kFamPics[m5Pic_ - kFamBase];
        World& w = *world_;
        w.rules().godMode = true;
        if (f.strip) stripStride_ = 4;
        if (f.track) {
            // The Track's cycle, four bars into its Groove: the run starts in that Track and is fast-forwarded there.
            w.directorMut().track = f.track;
            w.spawner().mode = Spawner::Mode::Slice;
            const u32* len = kSectionBars[formOfTrack(f.track, w.setList())];
            const u32 bars = (f.cycle - 1u) * (len[0] + len[1] + len[2] + len[3]) + 4u;
            scriptFastForwardS_ = static_cast<float>(bars) * 4.0f * 60.0f / bpmOfTrack(f.track, w.setList());
        } else {
            w.spawner().mode = Spawner::Mode::Off;
        }
        return true;
    }
    for (u32 k = 0; k < kPicCount; ++k)
        if (std::strcmp(p + 3, kPics[k].name) == 0) m5Pic_ = static_cast<u8>(k);
    if (m5Pic_ == 0xFF) {
        RF_ERR("play: no picture '%s'", p);
        return true;
    }
    const Pic& pic = kPics[m5Pic_];
    world_->rules().godMode = true;
    if (!setupFight(pic.fight, pic.phase)) return true;
    if (pic.strip) stripStride_ = 4;
    return true;
}

bool Play::scriptInputM5(StickInput& in) {
    if (m5Pic_ == 0xFF) return false;
    World& w = *world_;
    if (m5Pic_ >= kFamBase) {
        const FamPic& f = kFamPics[m5Pic_ - kFamBase];
        const u32 t = scriptTick_;
        const i64 now = static_cast<i64>(frameNo_);
        const double music = music1_;
        const Vec2 ship = controller_.pose().pos;
        const Enemies& e = w.enemies();
        in = StickInput{};
        in.assist = false;  // (no auto-aim: the group lives to show itself; the shooters' scenes aim below)
        if (f.track) {
            in = bot_.think(w, 0, controller_, clock_.heardNow());
            if (ffRunning_) return true;
            in.nova = false;
            w.closeTrioNow(0);
            w.playerMut(0).levelsBanked = 0;
            if (captureFrom_ < 0 && t == 150) captureFrom_ = now;
            return true;
        }
        if (t == 200) {
            // The group lands on the next beat round the ship (after the run's Track card has mostly gone) (a ring for Moths, ahead for the shooters' targets).
            const double land = (std::floor(music / kTicksPerBeat) + 1.0) * kTicksPerBeat;
            const u16 sp = SpeciesTable::index(f.family, f.tier, f.variant);
            for (u32 k = 0; k < f.count; ++k) {
                const float a = kTau * static_cast<float>(k) / static_cast<float>(f.count);
                Vec2 at = ship + Vec2{std::cos(a) * 520.0f, std::sin(a) * 300.0f};
                if (f.fire) at = ship + Vec2{420.0f + 60.0f * static_cast<float>(k), -120.0f + 80.0f * static_cast<float>(k)};
                if (f.family == Family::Carom) at = ship + Vec2{-420.0f - 90.0f * static_cast<float>(k), -200.0f + 160.0f * static_cast<float>(k)};  // they cross to the wall
                w.spawnEnemy(sp, at, f.family == Family::Carom ? 0.0f : angleOf(ship - at), land);
            }
        }
        if (t == 201 && f.fire && f.family != Family::Brood) {
            // The shooters' targets live through the stream (the scene is their answer to it).
            for (u32 i = 0; i < e.count; ++i)
                if (!(e.flags[i] & kFlagDead)) w.setEnemyHp(i, 400.0f);
        }
        if (f.family == Family::Carom && t < 3) {
            // The Caroms bounce off the arena's right wall: the ship stands near it.
            controller_.reset(Vec2{kArenaHalfW - 520.0f, 0.0f});  // the wall inside the frame
        }
        if (f.fire && t > (f.family == Family::Brood ? 300u : 210u)) {
            // The ship's stream at the nearest of them.
            i32 best = -1;
            float bd = 1e30f;
            for (u32 i = 0; i < e.count; ++i) {
                if ((e.flags[i] & kFlagDead) || e.state[i] == kWarping) continue;
                const float d = lengthSq(Vec2{e.x[i], e.y[i]} - ship);
                if (d < bd) {
                    bd = d;
                    best = static_cast<i32>(i);
                }
            }
            if (best >= 0) {
                in.aim = Vec2{e.x[best], e.y[best]} - ship;
                in.aimActive = true;
            }
        }
        if (captureFrom_ >= 0 || t < 300) return true;  // (after the run's Track card, 2.25 bars)
        bool go = false;
        for (u32 i = 0; i < e.count && !go; ++i) {
            if (e.flags[i] & kFlagDead) continue;
            switch (f.family) {
                case Family::Flanker: go = e.state[i] == kGlint || e.state[i] == kFiring; break;
                case Family::Skitter: go = e.state[i] == kDodge; break;
                case Family::Moth: go = e.state[i] == kFiring && std::fmod(music, 48.0) < (f.strip ? 1.0 : 10.0); break;
                case Family::Brood: go = f.variant == kVariantB ? (e.species[i] == SpeciesTable::kBroodling && e.state[i] != kWarping && t > 420) : false; break;
                case Family::Blink: go = e.state[i] == kTelegraph && (f.strip || music > e.until[i] - 0.5 * kTicksPerBeat); break;
                case Family::Hound: go = true; break;
                case Family::Drifter: go = e.state[i] == kGlint && music > e.until[i] - 0.5 * kTicksPerBeat; break;
                case Family::Carom: go = w.bullets().count >= 3; break;
                default: break;
            }
        }
        if (f.family == Family::Brood && f.variant == kVariantC) {
            // Its death: the ship's stream kills it; the swarm a few frames later.
            for (u32 k = 0; k < w.eventCount() && !go; ++k)
                go = w.events()[k].type == EventType::Kill && w.events()[k].species == SpeciesTable::index(Family::Brood, 2, kVariantC);
            if (go) {
                captureFrom_ = now + 6;
                return true;
            }
            for (u32 i = 0; i < e.count; ++i)
                if (e.species[i] == SpeciesTable::index(Family::Brood, 2, kVariantC) && t == 310) w.setEnemyHp(i, 0.001f);
        }
        if (!go && t > 60u * 60u) go = true;
        if (go) {
            captureFrom_ = now + 1;
            RF_LOG("play: picture '%s' captures from frame %lld", opt_.picture, static_cast<long long>(captureFrom_));
        }
        return true;
    }
    in = bot_.think(w, 0, controller_, clock_.heardNow());
    if (ffRunning_) return true;
    in.nova = false;  // (no Nova's ring over the moment)
    // No Trio over the moment: an open one takes its first orb, and no level opens another.
    w.closeTrioNow(0);
    w.playerMut(0).levelsBanked = 0;
    if (captureFrom_ >= 0) return true;
    const Pic& pic = kPics[m5Pic_];
    const BossState& b = w.boss();
    const i64 now = static_cast<i64>(frameNo_);
    const u32 t = scriptTick_;
    const double music = music1_;
    const double inBar = music - std::floor(music / kTicksPerBar) * kTicksPerBar;
    const double inBeat = std::fmod(music, kTicksPerBeat);
    const Vec2 ship = controller_.pose().pos;
    const float zoom = view_.stats().zoom > 0.1f ? view_.stats().zoom : 1.0f;
    const auto seen = [&](Vec2 q) { return inFrame(q, ship, zoom); };
    const MiniState& m = b.mini;
    bool go = false;
    if (t < 150) return true;  // the camera settles (the fights' zoom eases in after the fast-forward)
    if (pic.moment == Moment::Death) {
        // The death: killed on a beat after a bar of the fight, captured as it plays out.
        if (b.active && !b.dying && inBeat < 2.0) w.killBossNextTick(0, true);
        const double after = b.kind == BossKind::Choir ? 0.6 : (b.kind == BossKind::Longwave ? 0.45 : 0.08);
        go = b.dying && music >= b.deathTick + after * kTicksPerBar;
    } else if (b.active && !b.dying) {
        switch (pic.moment) {
            case Moment::Crest:
                // A crest in the frame, early in its step (strip: from the step's start).
                for (u32 k = 0; k < kBossParts && !go; ++k)
                    if ((b.alive & (1u << k)) && w.longwaveCrest(k, music) >= 0 && seen(b.body[1 + k]) && (t > 1800u || length(b.body[1 + k] - ship) < 520.0f)) go = true;
                go = go && std::fmod(music, b.lw.stepTicks) < (pic.strip ? 2.0 : 8.0);
                break;
            case Moment::Jaw:
                go = w.longwaveJawOpen(music) && seen(b.body[0]) && (inBar > 10.0 || music < b.lw.jawUntil) && (t > 2400u || length(b.body[0] - ship) < 560.0f) &&
                     (pic.phase || music > b.fightTick + 2.0 * kTicksPerBar);  // (lw_jaw: not the same moment as lw_swell)
                break;
            case Moment::Lunge:
                go = b.lw.lungeArmed && music >= b.lw.lungeTelegraph + 60.0 && music < b.lw.lungeTick && seen(b.body[0]);
                break;
            case Moment::Sing: {
                const u8 singers = w.choirSingers(std::floor(music / 48.0) * 48.0);
                for (u32 k = 0; k < 8 && !go; ++k) go = (singers & (1u << k)) && seen(b.body[1 + k]);
                go = go && std::fmod(music, 48.0) > 8.0 && std::fmod(music, 48.0) < 20.0;
                break;
            }
            case Moment::Cage: go = pic.strip ? (w.choirCage(music) == 2 && inBar < 2.0) : (w.choirCage(music) == 2 && inBar > 20.0); break;
            case Moment::Fugue: go = b.ch.phase == 2 && inBar > 4.5 * 48.0 && inBar < 5.0 * 48.0; break;
            case Moment::Held:
                for (u32 k = 0; k < 8 && !go; ++k) go = w.choirHeld(k, music, nullptr) == 2 && inBar > 50.0;
                break;
            case Moment::Howl: go = music >= b.fightTick + kTicksPerBar && inBar > 40.0 && inBar < 60.0; break;
            case Moment::Charge: go = music >= b.fightTick + kTicksPerBar && inBar > (pic.strip ? 2.0 * 96.0 - 2.0 : 2.0 * 96.0 + 24.0) && inBar < 2.0 * 96.0 + 40.0; break;
            case Moment::Winded: go = music < m.winded && m.state == 3 && music > m.winded - 1.6 * kTicksPerBeat && seen(b.body[0]); break;
            case Moment::Tell:
                go = music >= m.cueTick + (pic.strip ? 0.0 : 20.0) && music < m.cueTick + kTicksPerBeat && music >= b.fightTick + kTicksPerBar;
                break;
            case Moment::Ring: go = m.shape[0] == 0 && music > m.shapeTick + 0.6 * kTicksPerBar && (pic.phase || music < b.fightTick + 2.0 * kTicksPerBar); break;
            case Moment::Spear: go = m.shape[0] == 1 && (inBar < 40.0 || (inBar > 192.0 && inBar < 232.0)) && inBeat > 25.0; break;
            case Moment::Wall: go = m.shape[0] == 2 && music > m.shapeTick + 1.0 * kTicksPerBar; break;
            case Moment::Lock: go = m.state == 1 && music > m.cueTick - 1.2 * kTicksPerBeat; break;
            case Moment::Rush: go = m.state == 2 && (pic.strip || music > m.stateUntil - 1.0); break;
            case Moment::Stun: go = m.state == 4 && music > m.stateUntil - 2.4 * kTicksPerBeat; break;
            case Moment::Bounce: go = music < m.hot[0] && music > m.hot[0] - kTicksPerBeat + (pic.strip ? 0.0 : 6.0) && seen(b.body[0]); break;
            case Moment::Hot: go = music < m.hot[0] && music > m.hot[0] - 0.6 * kTicksPerBeat && seen(b.body[0]); break;
            case Moment::Split: go = m.splits >= 1 && b.alive != 0 && music > fightPhaseAt_ + 2.5 * kTicksPerBar; break;
            default: break;
        }
    }
    // A moment that never comes (60 s): the picture is taken anyway, and says so.
    if (!go && t > 60u * 60u) {
        RF_LOG("play: picture '%s': its moment did not come in 60 s; taken as it is", opt_.picture);
        go = true;
    }
    // A mini-boss's moment with its body near the ship (the camera follows the ship).
    if (go && isMiniBoss(b.kind) && b.active && !b.dying && b.kind != BossKind::Hive && t < 2400u && length(b.body[0] - ship) > 560.0f) go = false;
    if (go) {
        captureFrom_ = now + 1;
        RF_LOG("play: picture '%s' captures from frame %lld (%s, phase %u, music %.1f)", opt_.picture, static_cast<long long>(captureFrom_),
               bossId(b.kind), static_cast<unsigned>(b.phase), music);
    }
    return true;
}

bool Play::scriptEventsM5() { return m5Pic_ != 0xFF; }

}  // namespace rf
