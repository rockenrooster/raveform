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
// Event Horizon's health for --fight horizon:<phase> (horizon.json phases: 100-66, 66-33, 33-0).
float horizonShare(u8 phase) { return phase == 0 ? 1.0f : (phase == 1 ? 0.6f : 0.3f); }
// Feedback's (feedback.json phases: above 60%, 60-30%, 30% and below).
float feedbackShare(u8 phase) { return phase == 0 ? 1.0f : (phase == 1 ? 0.5f : 0.25f); }

enum class Moment : u8 {
    Crest, Jaw, Lunge, Death,                                   // Longwave (Death: every fight)
    Sing, Cage, Fugue, Held,                                    // the Choir
    Howl, Charge, Winded,                                       // Alpha Hound
    Tell, Ring, Spear, Wall,                                    // The Hive
    Lock, Rush, Stun,                                           // Battering Ram
    Bounce, Hot, Split,                                         // Carom King
    HzLanes, HzStarved, HzFed, HzWave, HzJetsShown, HzJets,     // Event Horizon (M5 part 4)
    PrOne, PrRings, PrBurnShown, PrBurn, PrSolo,                // Polyrhythm
    FbEchoes, FbOpen, FbTwo, FbHowl,                            // Feedback
    FbTape, FbNull, CdTurn,                                     // the review's fixes (H5, H9): a Nova on the tape, a Null's tell, Cadence's turn
    MqCourt, MqOpen, BmNest, BmExposed, BsDraw, BsFire, BsDazed, SaClock,  // mini-bosses batch 2
    CdFans,                                                     // Cadence (M5 part 6): the fans of beat 3 in the air
    ApBlocks, ApLoad, ApMove,                                   // Arpeggiator: blocks in the lanes, the shutter open, a side change shown
    MgDrift, MgRest, MgMirage, MgBlink, MgNext, MgLeap,         // Fata Morgana: fans in the calm, her crossing, the mirage lit and out, the next oasis, a leap
    RpFans, RpSolo, RpLast,                                     // Kaleidos, Reprise: the two-step's fans, a solo (Pic.phase - 10: its arm), Last Light
    MmAnswer, MmCopy, TtRope, TtSlack, WvCage, WvKnot, WvBurst, DwFill, DwChoke, DwRing,  // mini-bosses batch 3
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
    {"lw_unravel", "longwave", 0, Moment::Death, true},  // (2026-11-15: the whole body shatters, then the head breaks; frames a beat apart)
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
    // M5 part 4 (-Part part4): Event Horizon.
    {"hz_accretion", "horizon", 0, Moment::HzLanes, false},
    {"hz_starved", "horizon", 0, Moment::HzStarved, false},
    {"hz_fed", "horizon", 0, Moment::HzFed, false},
    {"hz_tidal", "horizon", 1, Moment::HzWave, false},
    {"hz_collapse", "horizon", 2, Moment::HzJetsShown, false},
    {"hz_jets", "horizon", 2, Moment::HzJets, false},
    {"hz_death", "horizon", 1, Moment::Death, false},
    {"hz_strip", "horizon", 1, Moment::HzFed, true},
    {"pr_one", "polyrhythm", 0, Moment::PrOne, false},
    {"pr_rings", "polyrhythm", 0, Moment::PrRings, false},
    {"pr_hemiola", "polyrhythm", 1, Moment::PrBurnShown, false},
    {"pr_burn", "polyrhythm", 1, Moment::PrBurn, false},
    {"pr_solo", "polyrhythm", 2, Moment::PrSolo, false},
    {"pr_death", "polyrhythm", 2, Moment::Death, false},
    {"pr_strip", "polyrhythm", 0, Moment::PrOne, true},
    {"fb_slapback", "feedback", 0, Moment::FbEchoes, false},
    {"fb_open", "feedback", 0, Moment::FbOpen, false},
    {"fb_regeneration", "feedback", 1, Moment::FbTwo, false},
    {"fb_howl", "feedback", 2, Moment::FbHowl, false},
    {"fb_death", "feedback", 1, Moment::Death, false},
    {"fb_strip", "feedback", 0, Moment::FbEchoes, true},
    // The review's fixes (2026-11-15, H5): a pressed Nova on the tape before it comes back; the Null's dashed tell closing on its
    // place (and, as a strip, a beat of it into the Null's growth).
    {"fb_tape", "feedback", 0, Moment::FbTape, false},
    {"fb_null", "feedback", 0, Moment::FbNull, false},
    {"fb_null_strip", "feedback", 0, Moment::FbNull, true},
    {"mq_court", "moth_queen", 0, Moment::MqCourt, false},
    {"mq_open", "moth_queen", 0, Moment::MqOpen, false},
    {"mq_half", "moth_queen", 1, Moment::MqCourt, false},
    {"mq_death", "moth_queen", 0, Moment::Death, false},
    {"mq_strip", "moth_queen", 0, Moment::MqOpen, true},
    {"bm_nest", "brood_mother", 0, Moment::BmNest, false},
    {"bm_exposed", "brood_mother", 0, Moment::BmExposed, false},
    {"bm_half", "brood_mother", 1, Moment::BmNest, false},
    {"bm_death", "brood_mother", 0, Moment::Death, false},
    {"bs_draw", "blink_star", 0, Moment::BsDraw, false},
    {"bs_fire", "blink_star", 0, Moment::BsFire, false},
    {"bs_dazed", "blink_star", 0, Moment::BsDazed, false},
    {"bs_half", "blink_star", 1, Moment::BsFire, false},
    {"bs_death", "blink_star", 0, Moment::Death, false},
    {"bs_strip", "blink_star", 0, Moment::BsDraw, true},
    {"sa_clock", "sentry_array", 0, Moment::SaClock, false},
    {"sa_half", "sentry_array", 1, Moment::SaClock, false},
    {"sa_death", "sentry_array", 0, Moment::Death, false},
    {"sa_strip", "sentry_array", 0, Moment::SaClock, true},
    {"cd_progression", "cadence", 0, Moment::CdFans, false},
    {"cd_modulation", "cadence", 1, Moment::CdFans, false},
    {"cd_inversion", "cadence", 2, Moment::CdFans, false},
    {"cd_death", "cadence", 1, Moment::Death, false},
    {"cd_strip", "cadence", 0, Moment::CdFans, true},
    // The review's fixes (2026-11-15, H9): the Modulation's turn announced: the pillars' dashed sweeps over the bar before it, and
    // as a strip, the end of that bar into the turn.
    {"cd_turn", "cadence", 1, Moment::CdTurn, false},
    {"cd_turn_strip", "cadence", 1, Moment::CdTurn, true},
    {"ap_quarters", "arpeggiator", 0, Moment::ApBlocks, false},
    {"ap_load", "arpeggiator", 0, Moment::ApLoad, false},
    {"ap_eighths", "arpeggiator", 1, Moment::ApBlocks, false},
    {"ap_move", "arpeggiator", 1, Moment::ApMove, false},
    {"ap_two", "arpeggiator", 2, Moment::ApBlocks, false},
    {"ap_death", "arpeggiator", 1, Moment::Death, false},
    {"ap_strip", "arpeggiator", 1, Moment::ApBlocks, true},
    {"mg_drift", "morgana", 0, Moment::MgDrift, false},
    {"mg_rest", "morgana", 0, Moment::MgRest, false},
    {"mg_mirage", "morgana", 1, Moment::MgMirage, false},
    {"mg_blink", "morgana", 1, Moment::MgBlink, false},
    {"mg_next", "morgana", 2, Moment::MgNext, false},
    {"mg_leap", "morgana", 2, Moment::MgLeap, false},
    {"mg_death", "morgana", 1, Moment::Death, false},
    {"mg_strip", "morgana", 2, Moment::MgLeap, true},
    {"rp_reprise", "reprise", 0, Moment::RpFans, false},
    {"rp_kaleidos", "reprise", 10, Moment::RpSolo, false},
    {"rp_longwave", "reprise", 11, Moment::RpSolo, false},
    {"rp_choir", "reprise", 12, Moment::RpSolo, false},
    {"rp_horizon", "reprise", 13, Moment::RpSolo, false},
    {"rp_poly", "reprise", 14, Moment::RpSolo, false},
    {"rp_feedback", "reprise", 15, Moment::RpSolo, false},
    {"rp_last", "reprise", 2, Moment::RpLast, false},
    {"rp_death", "reprise", 2, Moment::Death, false},
    {"rp_strip", "reprise", 11, Moment::RpSolo, true},
    {"mm_answer", "mirror_mimic", 0, Moment::MmAnswer, false},
    {"mm_copy", "mirror_mimic", 0, Moment::MmCopy, false},
    {"mm_half", "mirror_mimic", 1, Moment::MmAnswer, false},
    {"mm_death", "mirror_mimic", 1, Moment::Death, false},
    {"tt_rope", "twin_tether", 0, Moment::TtRope, false},
    {"tt_slack", "twin_tether", 0, Moment::TtSlack, false},
    {"tt_flail", "twin_tether", 1, Moment::TtRope, false},
    {"tt_death", "twin_tether", 1, Moment::Death, false},
    {"wv_cage", "weaver_prime", 0, Moment::WvCage, false},
    {"wv_knot", "weaver_prime", 0, Moment::WvKnot, false},
    {"wv_burst", "weaver_prime", 0, Moment::WvBurst, false},
    {"wv_half", "weaver_prime", 1, Moment::WvCage, false},
    {"wv_death", "weaver_prime", 1, Moment::Death, false},
    {"dw_fill", "deep_well", 0, Moment::DwFill, false},
    {"dw_choke", "deep_well", 0, Moment::DwChoke, false},
    {"dw_ring", "deep_well", 0, Moment::DwRing, false},
    {"dw_half", "deep_well", 1, Moment::DwRing, false},
    {"dw_death", "deep_well", 1, Moment::Death, false},
    {"wv_strip", "weaver_prime", 0, Moment::WvCage, true},
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
    if (!bossFromId(id, kind) || !fightBuilt(kind)) {
        RF_ERR("play: --fight '%s': no such fight built (kaleidos, longwave, choir, alpha_hound, the_hive, battering_ram, carom_king, "
               "horizon)", id);
        return false;
    }
    World& w = *world_;
    // Its home Track: a boss's own, a mini-boss's the Track whose middle Drop draws it (minibosses.json common.pick). A fight
    // past the run's set list (built before the Tracks between) stretches it to its Track, for this run.
    const u32 home = bossHomeTrack(kind);
    if (home > w.setList()) w.setSetList(home);
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
        case BossKind::EventHorizon: w.bossHealthNow(horizonShare(ph)); break;  // Tidal below 66%, Collapse below 33%
        case BossKind::Feedback: w.bossHealthNow(feedbackShare(ph)); break;
        case BossKind::Cadence: w.bossHealthNow(ph == 1 ? 0.6f : (ph == 2 ? 0.3f : 1.0f)); break;  // Modulation below 66%, Inversion below 33%
        case BossKind::Arpeggiator: w.bossHealthNow(ph == 1 ? 0.6f : (ph == 2 ? 0.3f : 1.0f)); break;  // Eighth notes below 66%, Two hands below 33%
        case BossKind::Morgana: w.bossHealthNow(ph == 1 ? 0.6f : (ph == 2 ? 0.3f : 1.0f)); break;      // Mirage below 66%, Crossing below 33%
        case BossKind::Reprise:
            // Medley (1, or 10 + an arm for its solo's picture): below 70%; Last Light (2): every arm shot off.
            if (ph == 1 || ph >= 10) w.bossHealthNow(0.6f);
            if (ph == 2) {
                w.bossHealthNow(0.6f);
                w.killBossNextTick(0x3Fu, false);
            }
            break;
        case BossKind::SentryArray: w.killBossNextTick(0x0Fu, false); break;  // half: four left
        case BossKind::TwinTether: w.killBossNextTick(0x1u, false); break;    // half: one node dead (the flail)
        case BossKind::Polyrhythm:
            // Hemiola: both at half; Solo: Four dead.
            if (ph == 1) w.bossHealthNow(0.5f);
            if (ph >= 2) w.killBossNextTick(0x2u, false);
            break;
        default: w.bossHealthNow(0.49f); break;
    }
}

void Play::holdFight() {
    World& w = *world_;
    const BossState& b = w.boss();
    if (!fightJump_ || !opt_.fightHold || !fightPhaseDone_ || !b.active || b.dying || b.kind != fightKind_) return;
    float share = 1.0f;
    if (b.kind == BossKind::CaromKing) share = 1.0f - 0.25f * static_cast<float>(fightPhase_ < 3 ? fightPhase_ : 3) - 0.01f;
    else if (b.kind == BossKind::EventHorizon) share = horizonShare(fightPhase_);
    else if (b.kind == BossKind::Polyrhythm) share = fightPhase_ == 1 ? 0.5f : 1.0f;
    else if (b.kind == BossKind::Feedback) share = feedbackShare(fightPhase_);
    else if (b.kind == BossKind::Reprise) share = fightPhase_ == 2 ? 0.2f : (fightPhase_ ? 0.6f : 1.0f);
    else if (b.kind == BossKind::Cadence || b.kind == BossKind::Arpeggiator || b.kind == BossKind::Morgana) share = fightPhase_ == 1 ? 0.6f : (fightPhase_ == 2 ? 0.3f : 1.0f);
    else if (isMiniBoss(b.kind) && fightPhase_) share = 0.49f;
    w.bossRefill(share);
}

void Play::maxLoadout() {
    // The upgrades' worst case for the measured frame (M5 part 3): every Mod at its cap, the four Riffs that throw or
    // look the most (Swarm, Flak, Storm Coil, Sweeper) at level 5, every Fusion they make, Chorus and Fractal always on.
    World& w = *world_;
    const u32 p = seat_;
    if (maxLoadoutRun_ != w.runId() || w.player(p).loadout.taken == 0) {
        maxLoadoutRun_ = w.runId();
        for (u8 u = 0; u < kModCount; ++u)
            while (w.player(p).loadout.canTake(u)) w.grantUpgrade(p, u);
        for (u8 r : {kSwarm, kFlak, kStormCoil, kSweeper})
            while (w.player(p).loadout.canTake(r)) w.grantUpgrade(p, r);
        u8 f = 0;
        for (u8 k = 0; k < kFusionCount; ++k)
            if (w.player(p).loadout.fusionDue(k)) f = static_cast<u8>(f | (1u << k));
        w.playerMut(p).loadout.fusions = f;
    }
    for (u8 b : {kChorus, kFractal})
        if (!w.player(p).boost(b)) w.grantBoost(p, b);
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
constexpr u16 kFamBase = 1000;  // m5Pic_ for a FamPic: kFamBase + its index (M5 part 6: past the 100 kPics)
static_assert(kPicCount < kFamBase, "the fights' pictures and the families' share m5Pic_");

// ---- M5 part 3: every new upgrade on screen (GAME_DESIGN 6.6: visible within a bar of the pick; eb10554: Split a lane a
// rank, so the loadouts take twice its stacks for the same fans as before). A still ship (the
// spawner off, hits costing nothing) is given its upgrades after the Track card, a group lands round it, and the picture
// is taken at its moment: a tick, or the first upgrade event of a kind after it.
enum UpLayout : u8 { kUpAhead, kUpRing, kUpCrowd };
enum UpAction : u8 { kUpNone, kUpDash, kUpNova };
struct UpPic {
    const char* name;      // m5_<name>
    u8 ups[16];            // upgrades in order (a Riff once a level; kNoUpgrade ends the list)
    u8 boosts[2];          // kNoBoost: none
    u8 fusions;            // bits
    u8 count;              // enemies in the group
    u8 layout;             // UpLayout
    float hp;              // their health (high: they live through the scene)
    bool fire;
    u8 action;             // UpAction at tick 300
    u8 waitFx, waitC;      // capture on this upgrade event after tick 300 (0xFF: at tick `at`), its c (0xFF: any)
    u16 at, after;         // the capture tick, or how many frames after the event
};
constexpr u8 N = kNoUpgrade;
constexpr u8 NB = kNoBoost;
const UpPic kUpPics[] = {
    {"up_weapons", {kFork, kFork, kSeeker, kSeeker, kHeavyStep, kHeavyStep, kHeavyStep, kEcho, kEcho, kOvercharge, N}, {NB, NB}, 0, 8, kUpAhead, 400.0f, true, kUpNone, 0xFF, 0xFF, 340, 0},
    {"up_wave", {kSplit, kSplit, kSplit, kSplit, kWave, kWave, N}, {NB, NB}, 0, 8, kUpAhead, 400.0f, true, kUpNone, 0xFF, 0xFF, 330, 0},
    {"up_helix", {kSplit, kSplit, kSplit, kSplit, kWave, kWave, N}, {NB, NB}, 1u << kHelixArray, 8, kUpAhead, 400.0f, true, kUpNone, 0xFF, 0xFF, 330, 0},
    {"up_accent", {kAccent, kAccent, kAccent, kOnBeatCrit, N}, {NB, NB}, 0, 10, kUpAhead, 400.0f, true, kUpNone, kFxLance, kLanceAccent, 300, 2},
    {"up_standing_wave", {kAccent, kAccent, kAccent, kEcho, kEcho, N}, {NB, NB}, 1u << kStandingWave, 10, kUpAhead, 400.0f, true, kUpNone, kFxLance, kLanceAccent, 300, 20},
    {"up_kills", {kArc, kArc, kArc, kBurn, kBurn, kBurn, kShatter, kShatter, kShatter, kGravityRounds, kGravityRounds, kSplit, kSplit, kSplit, kSplit, N}, {NB, NB}, 0, 40, kUpCrowd, 6.0f, true, kUpNone, kFxArc, 0xFF, 300, 1},
    {"up_wingmen", {kRearGuard, kRearGuard, kSyncopation, kMirror, kMirror, kMirror, kMirror, kMirror, N}, {NB, NB}, 0, 10, kUpRing, 400.0f, true, kUpNone, 0xFF, 0xFF, 337, 0},
    {"up_quartet", {kRearGuard, kRearGuard, kMirror, kMirror, kMirror, kMirror, kMirror, N}, {NB, NB}, 1u << kQuartet, 10, kUpRing, 400.0f, true, kUpNone, 0xFF, 0xFF, 337, 0},
    {"up_defence", {kAegis, kAegis, kAegis, kAfterimage, kAfterimage, kShockstep, kShockstep, kShockstep, kKickPulse, kKickPulse, N}, {NB, NB}, 0, 10, kUpRing, 400.0f, false, kUpDash, 0xFF, 0xFF, 312, 0},
    {"up_barline", {kBarLine, kBarLine, kBarLine, kCrescendo, kCrescendo, N}, {NB, NB}, 0, 16, kUpCrowd, 400.0f, false, kUpNone, kFxBarLine, 0xFF, 300, 2},
    {"riff_lancer", {kLancer, kLancer, kLancer, kMinelayer, kMinelayer, kMinelayer, kStormCoil, kStormCoil, kStormCoil, kSwarm, kSwarm, kSwarm, N}, {NB, NB}, 0, 16, kUpCrowd, 400.0f, false, kUpNone, kFxLance, kLanceLancer, 300, 3},
    {"riff_singularity", {kSingularity, kSingularity, kSingularity, kComet, kComet, kComet, kSweeper, kSweeper, kSweeper, kFlak, kFlak, kFlak, N}, {NB, NB}, 0, 24, kUpCrowd, 400.0f, false, kUpNone, kFxWell, 0xFF, 300, 50},
    {"boost_supernova_chain", {kSplit, kSplit, N}, {kSupernova, kChainStorm}, 0, 24, kUpCrowd, 400.0f, true, kUpNone, kFxArc, 0xFF, 330, 2},
    {"boost_colossus_carousel", {N}, {kColossus, kCarousel}, 0, 12, kUpRing, 400.0f, false, kUpNone, 0xFF, 0xFF, 340, 0},
    {"boost_freeze_fever", {kRicochet, N}, {kFreezeFrame, kRicochetFever}, 0, 20, kUpCrowd, 400.0f, true, kUpNone, 0xFF, 0xFF, 345, 0},
    {"boost_black_hole", {N}, {kBlackHole, kConfetti}, 0, 30, kUpCrowd, 400.0f, true, kUpNone, kFxHole, 0xFF, 300, 70},
    {"boost_chorus_beam", {kSplit, kSplit, kSplit, kSplit, N}, {kChorus, kBeam}, 0, 12, kUpAhead, 400.0f, true, kUpNone, 0xFF, 0xFF, 335, 0},
    {"boost_fractal_stretch", {kSplit, kSplit, N}, {kFractal, kTimeStretch}, 0, 16, kUpAhead, 400.0f, true, kUpNone, 0xFF, 0xFF, 335, 0},
    {"fusion_tesla", {kPulseRing, kPulseRing, kPulseRing, kPulseRing, kPulseRing, kArc, kArc, kArc, N}, {NB, NB}, 1u << kTeslaBloom, 24, kUpCrowd, 400.0f, false, kUpNone, kFxArc, 0xFF, 300, 1},
    {"fusion_shard_halo", {kOrbitals, kOrbitals, kOrbitals, kOrbitals, kOrbitals, kShatter, kShatter, kShatter, N}, {NB, NB}, 1u << kShardHalo, 40, kUpCrowd, 4.0f, true, kUpNone, kFxShatter, 0xFF, 300, 2},
    {"fusion_cannon", {kLancer, kLancer, kLancer, kLancer, kLancer, kOnBeatCrit, kOnBeatCrit, kOnBeatCrit, N}, {NB, NB}, 1u << kDownbeatCannon, 16, kUpAhead, 400.0f, false, kUpNone, kFxLance, kLanceCannon, 300, 2},
    {"fusion_collapse_murmuration", {kMinelayer, kMinelayer, kMinelayer, kMinelayer, kMinelayer, kGravityRounds, kGravityRounds, kSwarm, kSwarm, kSwarm, kSwarm, kSwarm, kSeeker, kSeeker, kSeeker, N}, {NB, NB}, (1u << kCollapseField) | (1u << kMurmuration), 30, kUpCrowd, 400.0f, false, kUpNone, kFxMineDrop, 0xFF, 300, 60},
};
constexpr u32 kUpPicCount = sizeof(kUpPics) / sizeof(kUpPics[0]);
constexpr u16 kUpBase = 2000;  // m5Pic_ for an UpPic: kUpBase + its index

// ---- M5 part 5: each elite trait on a body ahead of a still ship, each structure, each event (the spawner off, hits
// costing nothing); the picture at tick `at` (or `after` frames after the death of the elite the stream kills).
enum ArKind : u8 { kArElite, kArStruct, kArEvent };
struct ArPic {
    const char* name;  // m5_<name>
    u8 kind, id;       // ArKind; a TraitId, StructKind or EventKind
    u16 at;            // the capture tick (0: on the elite's death, `after` frames on)
    u8 after;
    bool fire;
};
const ArPic kArPics[] = {
    {"el_armored", kArElite, kArmored, 300, 0, true},        {"el_swift", kArElite, kSwift, 300, 0, false},
    {"el_giant", kArElite, kGiant, 300, 0, false},           {"el_splitting", kArElite, kSplitting, 0, 20, true},
    {"el_volatile", kArElite, kVolatile, 0, 40, true},       {"el_regenerating", kArElite, kRegenerating, 330, 0, false},
    {"el_shielded", kArElite, kShielded, 300, 0, true},      {"el_twin", kArElite, kTwin, 300, 0, false},
    {"el_magnetic", kArElite, kMagnetic, 420, 0, false},     {"el_commander", kArElite, kCommander, 330, 0, false},
    {"el_cloaked", kArElite, kCloaked, 300, 0, false},       {"el_phasing", kArElite, kPhasing, 300, 0, true},
    {"st_vault", kArStruct, 0, 330, 0, true},                {"st_pylon", kArStruct, 1, 520, 0, false},
    {"st_rift", kArStruct, 2, 420, 0, false},                {"st_bumper", kArStruct, 3, 360, 0, true},
    {"st_current", kArStruct, 4, 420, 0, true},              {"st_lens", kArStruct, 5, 330, 0, true},
    {"ev_vault_drop", kArEvent, 0, 420, 0, true},            {"ev_rift_surge", kArEvent, 1, 1150, 0, false},
    {"ev_elite_hunt", kArEvent, 2, 480, 0, false},           {"ev_comet", kArEvent, 3, 400, 0, true},
    {"ev_eclipse", kArEvent, 4, 420, 0, false},              {"ev_meteor_rain", kArEvent, 5, 400, 0, false},
    {"ev_convoy", kArEvent, 6, 480, 0, true},                {"ev_storm_cell", kArEvent, 7, 1000, 0, false},
};
constexpr u32 kArPicCount = sizeof(kArPics) / sizeof(kArPics[0]);
constexpr u16 kArBase = 3000;  // m5Pic_ for an ArPic: kArBase + its index
}  // namespace

bool Play::scriptSetupM5() {
    const char* p = opt_.picture;
    if (!starts(p, "m5_")) return false;
    for (u32 k = 0; k < kArPicCount; ++k)
        if (std::strcmp(p + 3, kArPics[k].name) == 0) m5Pic_ = static_cast<u16>(kArBase + k);
    if (m5Pic_ != 0xFFFF) {
        // M5 part 5: a still ship, hits costing nothing, the spawner off (the Track's first bars: no structure placed by itself).
        world_->rules().godMode = true;
        world_->spawner().mode = Spawner::Mode::Off;
        return true;
    }
    for (u32 k = 0; k < kUpPicCount; ++k)
        if (std::strcmp(p + 3, kUpPics[k].name) == 0) m5Pic_ = static_cast<u16>(kUpBase + k);
    if (m5Pic_ != 0xFFFF) {
        world_->rules().godMode = true;
        world_->spawner().mode = Spawner::Mode::Off;
        return true;
    }
    for (u32 k = 0; k < kFamPicCount; ++k)
        if (std::strcmp(p + 3, kFamPics[k].name) == 0) m5Pic_ = static_cast<u16>(kFamBase + k);
    if (m5Pic_ != 0xFFFF) {
        const FamPic& f = kFamPics[m5Pic_ - kFamBase];
        World& w = *world_;
        w.rules().godMode = true;
        if (f.strip) stripStride_ = 4;
        if (f.track) {
            // The Track's cycle, four bars into its Groove: the run starts in that Track and is fast-forwarded there.
            w.directorMut().track = f.track;
            w.spawner().mode = Spawner::Mode::Slice;
            const u32* len = kSectionBars[formOfTrack(f.track, w.setList())];
            // (c2f113a: every Track is two cycles; a picture written for the third cycle shows the second)
            const u32 cyc = f.cycle < cyclesOfTrack(f.track) ? f.cycle : cyclesOfTrack(f.track);
            const u32 bars = (cyc - 1u) * (len[0] + len[1] + len[2] + len[3]) + 4u;
            scriptFastForwardS_ = static_cast<float>(bars) * 4.0f * 60.0f / bpmOfTrack(f.track, w.setList());
        } else {
            w.spawner().mode = Spawner::Mode::Off;
        }
        return true;
    }
    for (u32 k = 0; k < kPicCount; ++k)
        if (std::strcmp(p + 3, kPics[k].name) == 0) m5Pic_ = static_cast<u16>(k);
    if (m5Pic_ == 0xFFFF) {
        RF_ERR("play: no picture '%s'", p);
        return true;
    }
    const Pic& pic = kPics[m5Pic_];
    world_->rules().godMode = true;
    if (!setupFight(pic.fight, pic.phase)) return true;
    // Part 4: the Track 5 build kills a rotor in seconds; the fight holds its health until the moment (not for its death).
    // (its death too: the hold gives nothing back once it is dying, and the picture kills it itself)
    if (fightKind_ == BossKind::Polyrhythm || fightKind_ == BossKind::Feedback || fightKind_ == BossKind::Cadence || fightKind_ == BossKind::Arpeggiator || fightKind_ == BossKind::Morgana || fightKind_ == BossKind::Reprise || (fightKind_ >= BossKind::MothQueen && fightKind_ <= BossKind::DeepWell))
        opt_.fightHold = true;
    if (pic.strip) stripStride_ = 4;
    // lw_unravel: a frame every beat over the death's two bars (at 126 BPM a beat is about 29 frames of 1/60 s).
    if (std::strcmp(pic.name, "lw_unravel") == 0) stripStride_ = 29;
    return true;
}

bool Play::scriptInputM5(StickInput& in) {
    if (m5Pic_ == 0xFFFF) return false;
    World& w = *world_;
    if (m5Pic_ >= kArBase) {
        // M5 part 5: an elite, a structure or an event round a still ship.
        const ArPic& a = kArPics[m5Pic_ - kArBase];
        const u32 t = scriptTick_;
        const i64 now = static_cast<i64>(frameNo_);
        const Vec2 ship = controller_.pose().pos;
        const Enemies& e = w.enemies();
        in = StickInput{};
        in.assist = false;
        const double land = (std::floor(music1_ / kTicksPerBeat) + 1.0) * kTicksPerBeat;
        if (t == 200 && a.kind == kArElite) {
            const u16 sp = SpeciesTable::index(a.id == kShielded ? Family::Hound : Family::Drifter, 3, kVariantA);
            const i32 i = w.spawnEliteNow(sp, ship + Vec2{380.0f, 0.0f}, traitBit(a.id));
            if (i >= 0 && (a.id == kSplitting || a.id == kVolatile)) w.setEnemyHp(static_cast<u32>(i), 140.0f);
            if (a.id == kCommander)
                for (u32 k = 0; k < 10; ++k)
                    w.spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), ship + Vec2{380.0f + std::cos(k * 0.63f) * 220.0f, std::sin(k * 0.63f) * 180.0f}, 0.0f, land);
            if (a.id == kMagnetic)
                for (u32 k = 0; k < 30; ++k) w.spawnShardNow(ship + Vec2{250.0f + 12.0f * static_cast<float>(k % 10), -60.0f + 40.0f * static_cast<float>(k / 10)}, Vec2{}, 0);
        }
        if (t == 200 && a.kind == kArStruct) {
            const StructKind k = static_cast<StructKind>(a.id);
            const Vec2 at = ship + Vec2{k == StructKind::Pylon ? 0.0f : 420.0f, 0.0f};
            if (k == StructKind::Bumper) {
                for (u32 j = 0; j < 3; ++j) {
                    const i32 s = w.placeStructureNow(k, ship + Vec2{420.0f + 160.0f * static_cast<float>(j % 2), -300.0f + 300.0f * static_cast<float>(j)}, 0.0f);
                    if (s >= 0) w.arenaBodyMut(static_cast<u32>(s)).tone = static_cast<u8>(j);
                }
            } else if (k == StructKind::Current) {
                w.placeStructureNow(k, at, 0.0f);
                w.placeStructureNow(k, at, 1.4f);
                for (u32 j = 0; j < 12; ++j) w.spawnEnemy(SpeciesTable::index(Family::Drifter, 1, kVariantA), at + Vec2{-500.0f + 90.0f * static_cast<float>(j), 30.0f}, 0.0f, land);
            } else {
                const i32 s = w.placeStructureNow(k, at, k == StructKind::Lens ? 1.2f : 0.0f);
                if (s >= 0 && k == StructKind::Rift) w.arenaBodyMut(static_cast<u32>(s)).from = music1_;
            }
            if (k == StructKind::Pylon || k == StructKind::Lens)
                for (u32 j = 0; j < 14; ++j)
                    w.spawnEnemy(SpeciesTable::index(Family::Drifter, 2, kVariantA), ship + Vec2{700.0f + 40.0f * static_cast<float>(j % 3), -330.0f + 50.0f * static_cast<float>(j)}, 3.14f, land);
        }
        if (t == 120 && a.kind == kArEvent) w.startEventNow(static_cast<EventKind>(a.id));
        if (t == 200 && a.kind == kArEvent)  // (a crowd for the event to fall on)
            for (u32 j = 0; j < 16; ++j)
                w.spawnEnemy(SpeciesTable::index(Family::Drifter, 2, kVariantA), ship + Vec2{std::cos(j * 0.39f) * 560.0f, std::sin(j * 0.39f) * 300.0f}, 0.0f, land);
        if (t == 210) {
            for (u32 i = 0; i < e.count; ++i)
                if (!(e.flags[i] & kFlagDead) && !(e.traits[i] & kEliteBit)) w.setEnemyHp(i, 400.0f);
        }
        if (a.fire && t > 220) {
            // The stream at the nearest body, or ahead.
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
            in.aim = best >= 0 && a.kind != kArStruct ? Vec2{e.x[best], e.y[best]} - ship : Vec2{1.0f, 0.02f};
            in.aimActive = true;
        }
        w.closeTrioNow(0);
        w.playerMut(0).levelsBanked = 0;
        if (captureFrom_ >= 0) return true;
        bool go = a.at ? t >= a.at : false;
        if (!a.at)
            for (u32 k = 0; k < w.eventCount() && !go; ++k) go = w.events()[k].type == EventType::Kill && w.events()[k].a == 2;
        if (!go && t > 60u * 60u) go = true;
        if (go) {
            captureFrom_ = now + 1 + (a.at ? 0 : a.after);
            RF_LOG("play: picture '%s' captures from frame %lld (music %.1f)", opt_.picture, static_cast<long long>(captureFrom_), music1_);
        }
        return true;
    }
    if (m5Pic_ >= kUpBase) {
        // M5 part 3: an upgrade's scene round a still ship.
        const UpPic& u = kUpPics[m5Pic_ - kUpBase];
        const u32 t = scriptTick_;
        const i64 now = static_cast<i64>(frameNo_);
        const double music = music1_;
        const Vec2 ship = controller_.pose().pos;
        const Enemies& e = w.enemies();
        in = StickInput{};
        in.assist = false;
        if (t == 120) {
            for (u8 k : u.ups) {
                if (k == kNoUpgrade) break;
                w.grantUpgrade(0, k);
            }
            w.playerMut(0).loadout.fusions = u.fusions;
        }
        if (t == 200) {
            // The group lands on the next beat: ahead of the ship, round it, or a crowd across the frame.
            const double land = (std::floor(music / kTicksPerBeat) + 1.0) * kTicksPerBeat;
            const u16 sp = SpeciesTable::index(Family::Drifter, 2, kVariantA);
            for (u32 k = 0; k < u.count; ++k) {
                const float f = static_cast<float>(k) / static_cast<float>(u.count);
                Vec2 at;
                if (u.layout == kUpAhead) at = ship + Vec2{360.0f + 50.0f * static_cast<float>(k % 4), -180.0f + 360.0f * f};
                else if (u.layout == kUpRing) at = ship + Vec2{std::cos(kTau * f) * 300.0f, std::sin(kTau * f) * 220.0f};
                else at = ship + Vec2{-700.0f + 1400.0f * static_cast<float>(k % 8) / 7.0f + 40.0f * static_cast<float>((k / 8) % 2),
                                      -300.0f + 600.0f * static_cast<float>(k / 8) / std::fmax(1.0f, static_cast<float>((u.count - 1) / 8))};
                if (u.layout == kUpCrowd && length(at - ship) < 120.0f) at = at + Vec2{0.0f, 160.0f};
                w.spawnEnemy(sp, at, angleOf(ship - at), land);
            }
        }
        if (t == 210) {
            for (u32 i = 0; i < e.count; ++i)
                if (!(e.flags[i] & kFlagDead)) w.setEnemyHp(i, u.hp);
            for (u8 b : u.boosts)
                if (b != kNoBoost && b != kBlackHole) w.grantBoost(0, b);
        }
        // (The owner's play of f0624e5: Black Hole goes off as it is taken, so it is taken where the Nova used to be pressed.)
        if (t == 300)
            for (u8 b : u.boosts)
                if (b == kBlackHole) w.grantBoost(0, b);
        if (u.fire && t > (u.hp < 50.0f ? 296u : 220u)) {  // (a weak crowd: the stream after the Track card has gone)
            // The stream at the nearest of them (the ring's scenes turn with the clock).
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
            in.aim = best >= 0 ? Vec2{e.x[best], e.y[best]} - ship : Vec2{1.0f, 0.0f};
            in.aimActive = true;
        } else if (t < 160) {
            // (the Riffs' and Bar Line's scenes: the aim set along the crowd early, then the stick idle: no stream)
            in.aim = Vec2{1.0f, -0.25f};
            in.aimActive = true;
        }
        if (u.action == kUpDash && t >= 300 && t < 304) {
            in.dash = true;
            in.move = Vec2{1.0f, 0.0f};
        }
        if (u.action == kUpNova && t >= 300 && t < 304) in.nova = true;
        w.closeTrioNow(0);
        w.playerMut(0).levelsBanked = 0;
        if (captureFrom_ >= 0 || t < (u.waitFx == 0xFF ? 300u : static_cast<u32>(u.at))) return true;
        bool go = false;
        if (u.waitFx == 0xFF) {
            go = t >= u.at;
        } else {
            for (u32 k = 0; k < w.eventCount() && !go; ++k) {
                const Event& ev = w.events()[k];
                go = ev.type == EventType::Upgrade && ev.b == u.waitFx && (u.waitC == 0xFF || ev.c == u.waitC) && ev.a == 0;
            }
            if (go) {
                captureFrom_ = now + 1 + u.after;
                RF_LOG("play: picture '%s' captures from frame %lld (music %.1f)", opt_.picture, static_cast<long long>(captureFrom_), music);
                return true;
            }
        }
        if (!go && t > 60u * 60u) {
            RF_LOG("play: picture '%s': its moment did not come in 60 s; taken as it is", opt_.picture);
            go = true;
        }
        if (go) {
            captureFrom_ = now + 1;
            RF_LOG("play: picture '%s' captures from frame %lld (music %.1f)", opt_.picture, static_cast<long long>(captureFrom_), music);
        }
        return true;
    }
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
    if (fightKind_ == BossKind::Reprise && w.boss().active) {
        // (the Reprise's pictures: the ship kept off the arena's walls, 500 wu from the wheel, so the wheel is in the frame)
        const Vec2 ship = controller_.pose().pos, wheel = w.boss().pos;
        const Vec2 rel = ship - wheel;
        const float l = length(rel);
        const Vec2 want = wheel + normalizeOr(Vec2{} - wheel, Vec2{1.0f, 0.0f}) * 500.0f;
        if (l > 700.0f || std::fabs(ship.x) > kArenaHalfW - 700.0f || std::fabs(ship.y) > kArenaHalfH - 500.0f)
            in.move = normalizeOr(want - ship, Vec2{1.0f, 0.0f});
    }
    if (std::strcmp(kPics[m5Pic_].name, "lw_unravel") == 0 && w.boss().active && w.boss().kind == BossKind::Longwave) {
        // (lw_unravel: the ship holds fire and keeps 400 wu off the body's seventh segment, so the head and most of the whole body
        // are in the frame when it dies)
        const Vec2 ship = controller_.pose().pos, mid = w.boss().body[1 + 6];
        const Vec2 want = mid + normalizeOr(ship - mid, Vec2{0.0f, 1.0f}) * 400.0f;
        in.move = length(want - ship) > 120.0f ? normalizeOr(want - ship, Vec2{1.0f, 0.0f}) : Vec2{};
        in.aimActive = false;
        in.assist = false;
    }
    if (fightKind_ == BossKind::DeepWell && w.boss().active && w.boss().kind == BossKind::DeepWell && !w.wellChoked(music1_)) {
        // (the Deep Well's pictures: a stream's worth fed in the middle of each bar, past its stomach for the choke's picture;
        // the bot's own stream, aimed at what it can hurt, rarely feeds it this much)
        const double in = music1_ - std::floor(music1_ / kTicksPerBar) * kTicksPerBar;
        if (in >= 0.5 * kTicksPerBar && in < 0.5 * kTicksPerBar + 3.3 && w.music() >= w.boss().fightTick)
            w.wellFeedNow((kPics[m5Pic_].moment == Moment::DwChoke ? 1.3f : 0.75f) * w.wellStomach());
    }
    if ((kPics[m5Pic_].moment == Moment::MmCopy || fightKind_ == BossKind::MirrorMimic) && w.boss().active) {
        // (the Mimic's copy: the ship stays near the mirror point, so the Mimic is in the frame, and dashes for it to copy)
        const Vec2 toPoint = w.boss().mini.ringAt - controller_.pose().pos;
        if (length(toPoint) > 260.0f) in.move = normalizeOr(toPoint, Vec2{1.0f, 0.0f}) * 0.8f;
        if (kPics[m5Pic_].moment == Moment::MmCopy && scriptTick_ % 150u == 0u) {
            in.dash = true;
            in.move = Vec2{0.0f, 1.0f};
        }
    }
    // (the review's fixes, H5: the Nulls' pictures: the ship presses a Nova every five seconds of the fight, with one in stock)
    if ((kPics[m5Pic_].moment == Moment::FbTape || kPics[m5Pic_].moment == Moment::FbNull) && w.boss().active && !w.boss().dying &&
        w.music() > w.boss().fightTick + 2.0 * kTicksPerBar && scriptTick_ % 300u == 0u) {
        PlayerState& pl = w.playerMut(0);
        if (pl.novas == 0) pl.novas = 1;
        in.nova = true;
    }
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
        // (within a frame of the beat: at 144 BPM a 1/60 s frame is 3.84 ticks and lands on the same 25 places of every beat)
        // (lw_unravel: once the head and the seventh segment are in the frame, or after 40 s)
        const bool framed = std::strcmp(pic.name, "lw_unravel") != 0 || (seen(b.body[0]) && seen(b.body[1 + 6])) || t > 2400u;
        if (b.active && !b.dying && inBeat < 5.0 && framed) w.killBossNextTick(0, true);
        double after = b.kind == BossKind::Choir ? 0.6 : (b.kind == BossKind::Longwave ? 0.45 : (b.kind == BossKind::EventHorizon ? 0.5 : (b.kind == BossKind::Polyrhythm ? 0.4 : (b.kind == BossKind::Feedback ? 0.4 : 0.08))));
        // (lw_unravel: the strip from the kill on, a frame a beat: 24 segments a sixteenth each, then the head, then its Shards)
        if (std::strcmp(pic.name, "lw_unravel") == 0) after = 0.0;
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
            // Event Horizon: the lanes walking with a meal under way; a gasp; a fed bar's eruption; the tidal wave; the jets.
            case Moment::HzLanes: go = music > b.fightTick + kTicksPerBar && b.hz.meal >= 2 && b.hz.eruptLeft == 0 && inBar > 0.6 * kTicksPerBar && seen(b.pos); break;
            case Moment::HzStarved: go = !b.hz.shield && music > b.fightTick + kTicksPerBar && inBar > 0.3 * kTicksPerBar && seen(b.pos); break;
            case Moment::HzFed: go = b.hz.shield && b.hz.eruptLeft > 0 && music > b.fightTick + kTicksPerBar && inBar > (pic.strip ? 0.0 : 3.0 * kTicksPerStep) && seen(b.pos); break;
            case Moment::HzWave: go = music - b.hz.waveTick > 1.2 * kTicksPerBeat && music - b.hz.waveTick < 1.4 * kTicksPerBeat && seen(b.pos); break;  // (~340 wu out)
            case Moment::HzJetsShown: go = b.hz.phase == 2 && music > fightPhaseAt_ + kTicksPerBar && inBar > 2.5 * kTicksPerBeat && seen(b.pos); break;
            case Moment::HzJets: go = music >= b.hz.jetFrom + 20.0 && music < b.hz.jetUntil && seen(b.pos); break;
            // Polyrhythm: the One (its rings' gaps on the link), the rings at work, the link's dashes, the burn, the Solo.
            case Moment::CdFans: go = music > fightPhaseAt_ + 2.0 * kTicksPerBar && inBar > (pic.strip ? 1.9 : 2.4) * kTicksPerBeat && inBar < 2.6 * kTicksPerBeat && seen(b.pos); break;
            // Arpeggiator: the lanes full (beat 3 of a bar), the shutter open on the One, the next side dashed late in its bar.
            case Moment::ApBlocks: go = music > fightPhaseAt_ + 2.0 * kTicksPerBar && inBar > (pic.strip ? 1.0 : 2.4) * kTicksPerBeat && inBar < 2.6 * kTicksPerBeat && seen(b.pos); break;
            case Moment::ApLoad: go = music > fightPhaseAt_ + 2.0 * kTicksPerBar && inBar > 0.45 * kTicksPerBeat && inBar < 0.6 * kTicksPerBeat && seen(b.pos); break;
            case Moment::ApMove: go = b.ap.moveTick >= 0.0 && music > b.ap.moveTick - 0.4 * kTicksPerBar && seen(b.pos); break;
            // Fata Morgana: her fans after the kick on step 10; her crossing at the middle of the water; the mirage lit and out; the
            // next oasis dashed half a bar before the leap; a third of a bar after a leap (the corridor).
            case Moment::MgDrift: go = music > fightPhaseAt_ + 2.0 * kTicksPerBar && !w.morganaResting(music) && inBar > 0.70 * kTicksPerBar && inBar < 0.75 * kTicksPerBar && seen(b.pos); break;
            case Moment::MgRest: go = w.morganaResting(music) && inBar > 0.40 * kTicksPerBar && inBar < 0.45 * kTicksPerBar; break;
            case Moment::MgMirage: {
                Vec2 mp;
                bool lit = false;
                go = w.morganaMirage(music, mp, lit) && lit && music > b.mg.mirageTick + 0.6 * kTicksPerBar && inBar > 0.65 * kTicksPerBar;
                break;
            }
            case Moment::MgBlink: {
                Vec2 mp;
                bool lit = false;
                go = w.morganaMirage(music, mp, lit) && !lit && music > b.mg.mirageTick + kTicksPerBar;
                break;
            }
            case Moment::MgNext: go = b.mg.nextShown && b.mg.leapTick >= 0.0 && music > b.mg.nextTick - 0.5 * kTicksPerBar && music < b.mg.nextTick - 0.4 * kTicksPerBar; break;
            // Kaleidos, Reprise: the fans on the snare of beat two; a solo's second bar (its arm: the picture's phase - 10); Last Light.
            case Moment::RpFans: go = music > b.fightTick + 2.0 * kTicksPerBar && inBar > 1.4 * kTicksPerBeat && inBar < 1.6 * kTicksPerBeat && seen(b.pos); break;
            case Moment::RpSolo:
                go = b.rp.phase == 1 && b.rp.solo == static_cast<u8>(pic.phase - 10) && music > b.rp.soloTick + (pic.strip ? 0.1 : (pic.phase == 11 ? 0.35 : 1.3)) * kTicksPerBar;
                break;
            case Moment::RpLast: go = b.rp.phase == 2 && b.rp.lastTick >= 0.0 && music > b.rp.lastTick + 2.6 * kTicksPerBar; break;
            // Mini-bosses batch 3.
            case Moment::MmAnswer: go = music > b.fightTick + 2.0 * kTicksPerBar && !w.mimicCopying(music) && w.bullets().count >= 4 && seen(b.body[0]); break;
            case Moment::MmCopy: go = w.mimicCopying(music) && music > b.mini.dashCopy + 0.25 * kTicksPerBeat && seen(b.body[0]); break;
            case Moment::TtRope: {
                Vec2 b0, b1;
                go = music > b.fightTick + kTicksPerBar && w.tetherBeam(b0, b1) && seen(b.body[0]) && length(b.body[0] - ship) < 420.0f;
                break;
            }
            case Moment::TtSlack: go = w.tetherSlack(music) && seen(b.body[0]); break;
            case Moment::WvCage: go = b.mini.cageTick >= 0.0 && music > b.mini.cageTick + (pic.strip ? 1.0 : 2.6) * kTicksPerBar && seen(b.mini.cage); break;
            case Moment::WvKnot: go = b.mini.cageTick >= 0.0 && music > b.mini.cageTick + kTicksPerBar && w.weaverKnot(0, music) && inBar > 0.3 * kTicksPerBeat; break;
            case Moment::WvBurst: go = b.mini.cageTick > b.fightTick + kTicksPerBar && music > b.mini.cageTick + 2.0 && music < b.mini.cageTick + 20.0; break;
            case Moment::DwFill: go = !w.wellChoked(music) && w.wellStomach() > 0.0f && b.mini.fill > 0.6f * w.wellStomach() && seen(b.body[0]); break;
            case Moment::DwChoke: go = w.wellChoked(music) && music > b.mini.chokeTick + 0.4 * kTicksPerBar && seen(b.body[0]); break;
            case Moment::DwRing: {
                u32 near = 0;
                for (u32 i = 0; i < w.bullets().count; ++i) near += lengthSq(Vec2{w.bullets().x[i], w.bullets().y[i]} - b.body[0]) < 420.0f * 420.0f ? 1u : 0u;
                go = near >= 16 && inBar > 0.15 * kTicksPerBar && inBar < 0.3 * kTicksPerBar && seen(b.body[0]);
                break;
            }
            case Moment::MgLeap: go = b.mg.leapTick >= 0.0 && music > b.mg.leapTick + (pic.strip ? 0.05 : 0.3) * kTicksPerBar && music < b.mg.leapTick + 0.45 * kTicksPerBar; break;
            case Moment::PrOne: go = music > b.fightTick + 2.0 * kTicksPerBar && inBar > (pic.strip ? 0.0 : 0.22 * kTicksPerBar) && inBar < 0.3 * kTicksPerBar; break;
            case Moment::PrRings: go = music > b.fightTick + 3.0 * kTicksPerBar && inBar > 0.7 * kTicksPerBar && inBar < 0.75 * kTicksPerBar; break;
            case Moment::PrBurnShown: go = w.polyLinkBurn(music) == 1 && music > fightPhaseAt_ + 2.0 * kTicksPerBar && inBar > 0.6 * kTicksPerBar; break;
            case Moment::PrBurn: go = w.polyLinkBurn(music) == 2 && w.polyLinkBurn(music - 20.0) == 2; break;
            // Feedback: the Echo at work (its shots in the air), the Head open, both Echoes, Howl's burning Loop.
            case Moment::FbEchoes: {
                u32 n = 0;
                for (u32 i = 0; i < w.bullets().count; ++i) n += w.bullets().echo[i] && !w.bullets().dead[i] ? 1u : 0u;
                go = b.fb.active && n >= (pic.strip ? 4u : 12u) && music > b.fightTick + 2.0 * kTicksPerBar;
                break;
            }
            case Moment::FbOpen: go = b.fb.active && b.fb.open >= 0.999f && music > b.fightTick + 2.0 * kTicksPerBar && inBar > 0.3 * kTicksPerBar; break;
            case Moment::FbTwo: go = (b.fb.active & 0x3u) == 0x3u && music > fightPhaseAt_ + 2.0 * kTicksPerBar; break;
            case Moment::FbHowl: go = b.fb.phase == 2 && music > fightPhaseAt_ + 3.0 * kTicksPerBar && inBar > 0.5 * kTicksPerBar; break;
            case Moment::FbTape: {
                const FeedbackTape::Lane& l = w.tape().lanes[0];
                for (u32 k = 0; k < l.novaCount; ++k) {
                    // (its star on the Loop, as the view places it, in the frame)
                    const double when = l.novas[k] + w.feedbackDelay(0);
                    const double step = std::fmod(std::floor(when / kTicksPerStep), 16.0);
                    const float a = -0.5f * kPi + kTau * static_cast<float>((step + 0.5) / 16.0);
                    const Vec2 mark = b.pos + Vec2{std::cos(a), std::sin(a)} * b.fb.loopR;
                    go = go || (when > music + 0.15 * kTicksPerBar && when < music + 0.8 * kTicksPerBar && seen(mark));
                }
                break;
            }
            case Moment::FbNull:
                for (const FeedbackTape::Null& n : w.tape().nulls)
                    go = go || (music < n.tick && music > n.tick - (pic.strip ? 0.6 : 0.5) * kTicksPerBeat && seen(n.at));
                break;
            case Moment::CdTurn: go = b.cd.turnWarn >= 0.0 && music > b.cd.turnWarn - (pic.strip ? 0.6 * kTicksPerBeat : 0.5 * kTicksPerBar) && seen(b.pos); break;
            // Mini-bosses batch 2.
            case Moment::MqCourt: go = m.ringTick >= 0.0 && music - m.ringTick > 1.3 * kTicksPerBar && music - m.ringTick < 1.5 * kTicksPerBar && seen(m.ringAt); break;
            case Moment::MqOpen: go = w.mothOpen(music) && inBar > (pic.strip ? 0.0 : 0.3 * kTicksPerBar) && seen(b.body[0]); break;
            case Moment::BmNest: {
                u32 eggs = 0, young = 0;
                for (u32 i = 0; i < w.enemies().count; ++i) {
                    if ((w.enemies().flags[i] & kFlagDead) || !(w.enemies().flags[i] & kFlagCalled)) continue;
                    if (w.enemies().state[i] == kEgg) ++eggs;
                    else ++young;
                }
                go = eggs >= 3 && young >= 3 && seen(b.body[0]);
                break;
            }
            case Moment::BmExposed: go = !m.shut && music > b.fightTick + 2.0 * kTicksPerBar && seen(b.body[0]); break;
            case Moment::BsDraw: go = w.blinkEdges(music) == 3 && (pic.strip || inBeat > 40.0); break;
            case Moment::BsFire: go = w.blinkFiring(music) && inBeat > 20.0 && inBeat < 60.0; break;
            case Moment::BsDazed: go = w.blinkDazed(music) && inBeat > 30.0; break;
            case Moment::SaClock: go = music > fightPhaseAt_ + kTicksPerBar && inBeat > (pic.strip ? 0.0 : 40.0) && inBeat < (pic.strip ? 4.0 : 56.0); break;
            case Moment::PrSolo: go = b.pr.phase == 2 && music > fightPhaseAt_ + kTicksPerBar && inBar > 0.2 * kTicksPerBar && inBar < 0.3 * kTicksPerBar; break;
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

bool Play::scriptEventsM5() { return m5Pic_ != 0xFFFF; }

}  // namespace rf
