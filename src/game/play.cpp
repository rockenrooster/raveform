#include "game/play.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

#include "audio/music_data.h"
#include "core/log.h"
#include "core/time.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kPadDeadZone = 0.20f;     // gamepad sticks (chosen; touch uses GAME_DESIGN 2's 8%)
constexpr float kPadAimActive = 0.30f;    // the right stick fires beyond this deflection
constexpr u32 kShardRunMax = 8;           // Shard notes waiting for their 1/32 step, at most
constexpr double kShardStepTicks = 12.0;  // one Shard note per 1/32 (the engine quantizes to it)
constexpr float kIntensityStep = 0.02f;   // the band hears a new intensity when it moved this much

bool is(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
Vec2 deadZone(Vec2 v, float dz) {
    const float l = length(v);
    if (l <= dz) return Vec2{};
    const float m = clampf((l - dz) / (1.0f - dz), 0.0f, 1.0f);
    return v * (m / l);
}
}  // namespace

Play::~Play() {
    delete world_;
    delete species_;
}

bool Play::init(const PlayOptions& opt, const Look& look, const Palettes& palettes, const Palette& palette, const GlyphSet& glyphs,
                const VectorFont& font, const TierSettings& tier, Band* band) {
    opt_ = opt;
    look_ = &look;
    palettes_ = &palettes;
    palette_ = &palette;
    glyphs_ = &glyphs;
    font_ = &font;
    band_ = band;
    species_ = new (std::nothrow) SpeciesTable;
    world_ = new (std::nothrow) World;
    if (!species_ || !world_ || !species_->load()) return false;
    world_->init(species_);
    bandDrives_ = band && band->active() && band->gameMode() && !opt.fixedStep;

    // The Track's tempo (stage A plays Track 1): from the band, or from the music data when it is off.
    float bpm = 120.0f;
    if (band && band->active()) {
        bpm = band->music().tracks[band->track()].bpm;
    } else if (auto* md = new (std::nothrow) audio::MusicData) {
        if (md->load() && md->trackCount) bpm = md->tracks[0].bpm;
        delete md;
    }
    clock_.reset(bpm, 0.0);

    const bool picture = opt.picture[0] != 0;
    Spawner::Mode mode = Spawner::Mode::Slice;
    if (opt.kind == PlayOptions::Kind::Stress) mode = Spawner::Mode::Stress;
    if (picture) mode = Spawner::Mode::Off;
    world_->spawner().mode = mode;
    world_->rules().godMode = opt.kind == PlayOptions::Kind::Stress;
    world_->rules().novas = opt.kind != PlayOptions::Kind::Stress;
    world_->resetRun(opt.seed, 0.0, 1);
    controller_.reset(world_->player(0).pos);
    bot_.reset(opt.seed, opt.kind == PlayOptions::Kind::Stress ? Bot::Policy::Stress : Bot::Policy::Play);
    if (!view_.init(look, palettes, palette, glyphs, tier)) return false;
    if (picture) scriptSetup();
    if (bandDrives_) band->setIntensity(tuning::kIntensityFloor);

    // --fast-forward: the game runs on the simulated clock without drawing (pictures at 2 and 5 minutes).
    if (opt.fastForwardS > 0.0f) {
        const u32 ticks = static_cast<u32>(opt.fastForwardS * 60.0f);
        const bool drives = bandDrives_;
        bandDrives_ = false;
        const PlayOptions saved = opt_;
        opt_.fixedStep = true;
        for (u32 k = 0; k < ticks; ++k) frame(kDt);
        opt_ = saved;
        bandDrives_ = drives;
        view_.resetEffects();
        frameNo_ = 0;
        RF_LOG("play: fast-forwarded %.1f s (%u ticks): %u enemies, %u kills, score %llu", opt.fastForwardS, ticks,
               world_->enemies().count, world_->counters().kills, static_cast<unsigned long long>(world_->player(0).score));
    }
    RF_LOG("play: %s%s, seed %llu, %.0f BPM, %s clock, spawner %s", opt.kind == PlayOptions::Kind::Stress ? "bench_stress" : (opt.kind == PlayOptions::Kind::BenchPlay ? "bench_play" : "play"),
           opt.bot ? " (bot)" : "", static_cast<unsigned long long>(opt.seed), bpm, bandDrives_ ? "the band's heard" : "the simulated",
           mode == Spawner::Mode::Stress ? "stress" : (mode == Spawner::Mode::Off ? "off (picture script)" : "first slice"));
    return true;
}

void Play::setLayout(float w, float h, Vec2 insetMin, Vec2 insetMax) { touch_.layout(w, h, insetMin, insetMax, opt_.pxPerMm); }

// ---------------------------------------------------------------------------------------------- frame

void Play::buildInput(StickInput& in) {
    in = StickInput{};
    if (opt_.picture[0]) {
        scriptInput(in);
        return;
    }
    if (opt_.bot) {
        in = bot_.think(*world_, 0, controller_, clock_.heardNow());
        return;
    }
    // Keyboard: WASD or the arrows.
    Vec2 mv{static_cast<float>((keys_[3] || keys_[7]) - (keys_[1] || keys_[5])),
            static_cast<float>((keys_[2] || keys_[6]) - (keys_[0] || keys_[4]))};
    if (lengthSq(mv) > 0.0f) in.move = normalizeOr(mv, Vec2{});
    // Mouse: aim at the pointer, fire while the left button is held.
    if (mouseFire_ && mouseSeen_) {
        in.aim = view_.toWorld(mousePos_) - controller_.pose().pos;
        in.aimActive = lengthSq(in.aim) > 1.0f;
    }
    // Gamepad.
    const Vec2 pm = deadZone(padMove_, kPadDeadZone);
    if (lengthSq(pm) > lengthSq(in.move)) in.move = pm;
    if (length(padAim_) > kPadAimActive) {
        in.aim = padAim_;
        in.aimActive = true;
    }
    touch_.apply(in);
    in.nova = in.nova || keyNova_ || mouseNova_ || padNova_;
    in.dash = in.dash || keyDash_ || padDash_;
}

void Play::frame(float realDt) {
    ++frameNo_;
    const bool fixed = opt_.fixedStep;
    frameDt_ = fixed ? kDt : (realDt > 0.1f ? 0.1f : realDt);
    const BeatView* bv = (bandDrives_ && band_) ? &band_->view() : nullptr;
    clock_.beginFrame(bv, realDt, fixed);

    StickInput in;
    buildInput(in);
    controller_.update(in, frameDt_, clock_.heardNow(), clock_.bpm(), *world_, 0);
    if (bandDrives_) band_->setFireMode(controller_.pose().fire);

    stats_.ticksLastFrame = 0;
    if (fixed) {
        tickEnd_ = clock_.simTime();
        runTick(music1_, clock_.musicAt(tickEnd_), true);
        alpha_ = 1.0f;
    } else {
        u32 n = 0;
        while (clock_.simTime() - tickEnd_ >= static_cast<double>(kDt) - 1e-9 && n < 3) {
            tickEnd_ += kDt;
            runTick(music1_, clock_.musicAt(tickEnd_), n == 0);
            ++n;
        }
        // Beyond three ticks a frame, drop time rather than spiral (TECH_SPEC 4).
        if (clock_.simTime() - tickEnd_ > 3.0 * kDt) tickEnd_ = clock_.simTime() - 0.5 * kDt;
        alpha_ = clampf(static_cast<float>((clock_.simTime() - tickEnd_) / kDt), 0.0f, 1.0f);
    }
    bandFrame();

    // Picture runs: capture `stripFrames_` frames from the scripted moment.
    captureNow_ = false;
    if (opt_.picture[0]) {
        scriptEvents();
        ++scriptTick_;
        if (captureFrom_ >= 0 && static_cast<i64>(frameNo_) >= captureFrom_) {
            const u64 k = frameNo_ - static_cast<u64>(captureFrom_);
            if (k < stripFrames_) {
                captureNow_ = true;
                captureIndex_ = static_cast<u32>(k);
            } else {
                pictureDone_ = true;
            }
        }
        if (scriptTick_ > 60u * 30u) pictureDone_ = true;  // a script that never triggers ends after 30 s
    }

    const World& w = *world_;
    const PlayerState& p = w.player(0);
    if (bandDrives_ && clock_.fromBand() && frameNo_ % 1200 == 0) {
        // Every 20 s (60 Hz): how the sim's musical clock sits against the band's heard one.
        RF_LOG("play: frame %llu: music %.1f, heard %.1f (sim ahead by %.1f ticks), %u band shots, %u fired, %u kills, "
               "%u enemies, intensity %.2f, latency %.1f ticks",
               static_cast<unsigned long long>(frameNo_), music1_, clock_.heardNow(), music1_ - clock_.heardNow(), bandShots_,
               p.shotsFired, w.counters().kills, w.enemies().count, intensity_, clock_.latencyTicks());
    }
    stats_.enemies = w.enemies().count;
    stats_.bullets = w.bullets().count;
    stats_.shots = w.shots().count;
    stats_.shards = w.shards().count;
    stats_.kills = w.counters().kills;
    stats_.deaths = w.counters().deaths;
    stats_.restarts = w.counters().restarts;
    stats_.score = p.score;
    stats_.chain = p.chain;
    stats_.bestChain = p.bestChain > stats_.bestChain ? p.bestChain : stats_.bestChain;
    const WorldCounters& c = w.counters();
    stats_.overflow = c.enemiesDropped + c.bulletsDropped + c.shotsDropped + c.shardsDropped + c.eventsDropped;
    stats_.ticks = w.tickCount();
}

void Play::runTick(double music0, double music1, bool firstOfFrame) {
    TickInput in;
    in.music0 = music0;
    in.music1 = music1;
    in.bpm = clock_.bpm();
    in.latencyTicks = clock_.latencyTicks();
    in.players[0] = controller_.pose();
    in.retry = retryRequest_;
    retryRequest_ = false;
    // While the band plays, the shots are the lead rule's heard steps (TECH_SPEC 6.3), each advanced by
    // the time since its step; without it, the sim fires on its own grid.
    in.bandShots = bandDrives_ && clock_.fromBand();
    if (in.bandShots && firstOfFrame) {
        const BeatView& v = band_->view();
        const float tps = clock_.ticksPerSecond();
        for (u32 k = 0; k < v.shotCount && in.shotCount < countOf(in.shots); ++k) {
            ShotRequest& r = in.shots[in.shotCount++];
            r.tick = v.shots[k].tick;
            r.player = 0;
            const float adv = static_cast<float>((music1 - v.shots[k].tick) / tps);
            r.advanceS = adv < 0.0f ? 0.0f : (adv > 0.1f ? 0.1f : adv);
        }
        bandShots_ += in.shotCount;
    }
    const u64 t0 = nowNs();
    world_->tick(in);
    const float ms = static_cast<float>(nsToMs(nowNs() - t0));
    if (stats_.ticksLastFrame < countOf(stats_.tickMs)) stats_.tickMs[stats_.ticksLastFrame] = ms;
    ++stats_.ticksLastFrame;
    controller_.clearEdges();
    musicPrev_ = music1_;
    music1_ = music1;
    afterTick();
}

void Play::afterTick() {
    const World& w = *world_;
    view_.onEvents(w, tickEnd_);
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        switch (e.type) {
            case EventType::Death:
                if (opt_.bot || opt_.kind != PlayOptions::Kind::Play) retryRequest_ = true;  // the bot taps retry
                if (bandDrives_) band_->playSfx(audio::Sfx::GameOver);
                break;
            case EventType::Restart:
                controller_.reset(w.player(0).pos);
                touch_.reset();
                break;
            case EventType::Kill:
                if (!e.c && e.a < 3) ++killsPending_[e.a];
                break;
            case EventType::Pickup:
                if (shardsPending_ < kShardRunMax) ++shardsPending_;
                break;
            case EventType::WarpStart:
                // One swell per landing beat, ending as the enemies become solid.
                if (bandDrives_ && e.tick != lastWarpTick_) {
                    const float pan = clampf((e.x - w.player(0).pos.x) / 800.0f, -1.0f, 1.0f) * 0.6f;
                    band_->warp(e.tick, pan);
                    lastWarpTick_ = e.tick;
                }
                break;
            case EventType::Telegraph:
                // The tell one beat ahead and, for a shooter, the shot's blip on the beat it resolves on
                // (sfxproto's telegraph tick and enemy shot), once per beat.
                if (bandDrives_ && e.tick != lastTellTick_) {
                    lastTellTick_ = e.tick;
                    band_->playSfx(audio::Sfx::Tick, 0.7f);
                    if (w.species().get(e.species).ranged()) band_->sfxAt(audio::Sfx::EnemyShot, e.tick);
                }
                break;
            case EventType::Hit:
                if (bandDrives_) band_->playSfx(audio::Sfx::Hit);
                break;
            case EventType::NovaPress:
                if (bandDrives_) band_->nova(e.tick);
                break;
            case EventType::Dash:
                if (bandDrives_) band_->playSfx(e.b ? audio::Sfx::PerfectDash : audio::Sfx::Dash);
                break;
            default: break;
        }
    }
}

void Play::bandFrame() {
    if (!bandDrives_) {
        killsPending_[0] = killsPending_[1] = killsPending_[2] = 0;
        shardsPending_ = 0;
        return;
    }
    // Kills: the engine aggregates them per 1/32 step (one cluster sound per size class).
    for (u8 s = 0; s < 3; ++s) {
        if (killsPending_[s]) band_->kill(s, killsPending_[s]);
        killsPending_[s] = 0;
    }
    // Shards: each pickup plays the next note of the run, at most one per 1/32 step, so a streak of
    // pickups is a rising run rather than a chord.
    const double heard = clock_.heardNow();
    if (shardsPending_ && heard >= lastShardTick_ + kShardStepTicks) {
        band_->shard();
        --shardsPending_;
        lastShardTick_ = heard;
    }
    // Stage A: the band stays in Groove and its intensity follows the threat on screen.
    const float target = clampf(tuning::kIntensityFloor + world_->threatInView() * tuning::kIntensityPerThreat, 0.0f, 1.0f);
    intensity_ += (target - intensity_) * (1.0f - std::exp(-frameDt_ / tuning::kIntensityTau));
    if (std::fabs(intensity_ - intensityPosted_) >= kIntensityStep) {
        band_->setIntensity(intensity_);
        intensityPosted_ = intensity_;
    }
}

// ---------------------------------------------------------------------------------------------- render

void Play::render(FrameDraw& d, FrameParams& params, Background& bg, const LookVariant& variant, float W, float H, Vec2 insetMin,
                  Vec2 insetMax) {
    const World& w = *world_;
    ViewInput vi;
    vi.world = &w;
    vi.alpha = alpha_;
    vi.time = tickEnd_ - (1.0 - static_cast<double>(alpha_)) * kDt;
    vi.dt = frameDt_;
    vi.music = musicPrev_ + (music1_ - musicPrev_) * static_cast<double>(alpha_);
    vi.bpm = clock_.bpm();
    vi.beat = (bandDrives_ && band_ && clock_.fromBand()) ? &band_->view() : nullptr;
    const PlayerPose& pose = controller_.pose();
    vi.shipPos = pose.pos;
    vi.shipVel = pose.vel;
    vi.shipAim = pose.aim;
    vi.shipHeading = pose.heading;
    vi.shipFiring = pose.fire != 0;
    view_.render(d, params, bg, W, H, vi);

    HudState hs;
    const PlayerState& p = w.player(0);
    hs.score = p.score;
    hs.chain = p.chain;
    hs.chainPulse = view_.chainPulse(vi.time);
    hs.novas = p.novas;
    hs.novaCap = p.novaCap;
    hs.dashCharge = controller_.dashCharge(clock_.heardNow());
    hs.alive = p.alive;
    hs.retryPending = w.restartPending();
    hs.kick = view_.kick();
    hs.showTouch = touch_.used();
    drawHud(d, *font_, *look_, variant, *palettes_, *palette_, hs, touch_, W, H, insetMin, insetMax);
}

// ---------------------------------------------------------------------------------------------- input

bool Play::key(int k, bool down) {
    const bool dead = !world_->player(0).alive;
    switch (k) {
        case SDLK_W: keys_[0] = down; return true;
        case SDLK_A: keys_[1] = down; return true;
        case SDLK_S: keys_[2] = down; return true;
        case SDLK_D: keys_[3] = down; return true;
        case SDLK_UP: keys_[4] = down; return true;
        case SDLK_LEFT: keys_[5] = down; return true;
        case SDLK_DOWN: keys_[6] = down; return true;
        case SDLK_RIGHT: keys_[7] = down; return true;
        case SDLK_E:
            keyNova_ = down;
            if (down && dead) retryRequest_ = true;
            return true;
        case SDLK_SPACE:
            keyDash_ = down;
            if (down && dead) retryRequest_ = true;
            return true;
        case SDLK_RETURN:
            if (down && dead) retryRequest_ = true;
            return true;
        default: return false;
    }
}

void Play::mouseButton(int button, bool down, Vec2 pos) {
    mousePos_ = pos;
    mouseSeen_ = true;
    if (button == SDL_BUTTON_LEFT) mouseFire_ = down;
    if (button == SDL_BUTTON_RIGHT) mouseNova_ = down;
    if (down && !world_->player(0).alive) retryRequest_ = true;
}

void Play::mouseMove(Vec2 pos) {
    mousePos_ = pos;
    mouseSeen_ = true;
}

void Play::touchDown(u64 id, Vec2 pos) {
    if (!world_->player(0).alive) {
        retryRequest_ = true;  // "tap to retry"
        return;
    }
    touch_.down(id, pos);
}

void Play::touchMove(u64 id, Vec2 pos) { touch_.move(id, pos); }
void Play::touchUp(u64 id) { touch_.up(id); }

void Play::padAxis(int axis, float v) {
    switch (axis) {
        case SDL_GAMEPAD_AXIS_LEFTX: padMove_.x = v; break;
        case SDL_GAMEPAD_AXIS_LEFTY: padMove_.y = v; break;
        case SDL_GAMEPAD_AXIS_RIGHTX: padAim_.x = v; break;
        case SDL_GAMEPAD_AXIS_RIGHTY: padAim_.y = v; break;
        default: break;
    }
}

void Play::padButton(int button, bool down) {
    switch (button) {
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: padNova_ = down; break;  // GAME_DESIGN 2: RB / R1
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: padDash_ = down; break;   // LB / L1
        case SDL_GAMEPAD_BUTTON_SOUTH:
        case SDL_GAMEPAD_BUTTON_START:
            if (down && !world_->player(0).alive) retryRequest_ = true;
            break;
        default: break;
    }
}

void Play::padRemoved() {
    padMove_ = padAim_ = Vec2{};
    padNova_ = padDash_ = false;
}

// ---------------------------------------------------------------------------------------------- pictures

// Scripted scenes for the lead's review strips (docs/PROGRESS.md M3): fixed 1/60 s steps, no band, the
// spawner off; each places its enemies, drives the ship and names the moment the strip starts.
namespace {
enum ScriptFlag : u32 { kPressed = 1, kSpawned = 2 };
}

void Play::scriptSetup() {
    World& w = *world_;
    const SpeciesTable& s = *species_;
    const char* p = opt_.picture;
    const auto put = [&](Family f, u32 tier, u32 variant, Vec2 at, float hp) {
        const i32 i = w.spawnEnemy(SpeciesTable::index(f, tier, variant), at, angleOf(Vec2{} - at), 0.0);
        if (i >= 0 && hp > 0.0f) w.setEnemyHp(static_cast<u32>(i), hp);
        return i;
    };
    if (is(p, "fire")) {
        for (u32 k = 0; k < 5; ++k) put(Family::Drifter, 2, kVariantA, Vec2{430.0f + 55.0f * k, -120.0f + 60.0f * k}, 60.0f);
    } else if (is(p, "kill_small")) {
        put(Family::Drifter, 1, kVariantA, Vec2{300.0f, 0.0f}, 0.0f);
    } else if (is(p, "kill_medium")) {
        put(Family::Drifter, 3, kVariantA, Vec2{300.0f, 0.0f}, 1.0f);
    } else if (is(p, "kill_large")) {
        put(Family::Ram, 5, kVariantA, Vec2{340.0f, 0.0f}, 1.0f);
    } else if (is(p, "shards")) {
        for (u32 k = 0; k < 44; ++k) {
            const float x = 110.0f + 300.0f * static_cast<float>(k % 11) / 10.0f;
            const float y = -70.0f + 140.0f * static_cast<float>(k / 11) / 3.0f;
            w.spawnShard(Vec2{x, y}, Vec2{}, 0);
        }
    } else if (is(p, "hit")) {
        put(Family::Hound, 1, kVariantA, Vec2{170.0f, 20.0f}, 0.0f);
        w.playerMut(0).chain = 24;  // so the halving shows
    } else if (is(p, "nova") || is(p, "nova_front")) {
        for (u32 k = 0; k < 26; ++k) {
            const float a = kTau * static_cast<float>(k) / 26.0f;
            const float r = 280.0f + 160.0f * static_cast<float>(k % 3) / 2.0f;
            put(k % 4 == 0 ? Family::Hound : Family::Drifter, 1 + k % 2, kVariantA, Vec2{std::cos(a) * r * 1.6f, std::sin(a) * r * 0.75f}, 0.0f);
        }
        put(Family::Sentry, 1, kVariantB, Vec2{-520.0f, 200.0f}, 100.0f);
        put(Family::Sentry, 1, kVariantB, Vec2{560.0f, -210.0f}, 100.0f);
    } else if (is(p, "sentry")) {
        put(Family::Sentry, 1, kVariantB, Vec2{380.0f, -90.0f}, 100.0f);
    }
    (void)s;
}

void Play::scriptInput(StickInput& in) {
    const World& w = *world_;
    const char* p = opt_.picture;
    const u32 t = scriptTick_;
    const double music = music1_;
    const float tpt = clock_.ticksPerSecond() * kDt;  // musical ticks per sim tick
    in.assist = false;
    if (is(p, "fire")) {
        if (t >= 10) {
            in.aim = Vec2{1.0f, 0.05f};
            in.aimActive = true;
        }
    } else if (is(p, "kill_small") || is(p, "kill_medium") || is(p, "kill_large")) {
        // Aim at the target where it is (it wanders); fire from tick 20.
        const Enemies& e = w.enemies();
        if (t >= 20 && e.count) {
            in.aim = Vec2{e.x[0], e.y[0]} - controller_.pose().pos;
            in.aimActive = true;
        }
    } else if (is(p, "shards")) {
        if (t >= 10) in.move = Vec2{1.0f, 0.0f};
    } else if (is(p, "nova") || is(p, "nova_front")) {
        // Press seven frames before a beat, once the Sentries are firing.
        const double toBeat = std::ceil(music / kTicksPerBeat) * kTicksPerBeat - music;
        if (t > 200 && !(scriptFlags_ & kPressed) && toBeat > 6.5 * tpt && toBeat <= 7.5 * tpt) {
            in.nova = true;
            scriptFlags_ |= kPressed;
        }
        in.move = Vec2{0.25f, -0.1f};
    } else if (is(p, "dash") || is(p, "perfect_dash")) {
        in.move = Vec2{0.8f, -0.6f};
        const double inBeat = std::fmod(music, kTicksPerBeat);
        const double off = inBeat < 48.0 ? inBeat : kTicksPerBeat - inBeat;
        const bool perfect = is(p, "perfect_dash");
        if (t > 60 && !(scriptFlags_ & kPressed) && (perfect ? off < 0.6 * tpt : off > 0.3 * kTicksPerBeat)) {
            in.dash = true;
            scriptFlags_ |= kPressed;
        }
    } else if (is(p, "warp") || is(p, "warp_land")) {
        // Spawn on a beat: a hound pack, a Ram and a Shoal school warp in for exactly two beats.
        if (t > 30 && !(scriptFlags_ & kSpawned) && crossed(musicPrev_, music, kTicksPerBeat)) {
            World& wm = *world_;
            const double beat = std::floor(music / kTicksPerBeat) * kTicksPerBeat;
            const double land = beat + kWarpBeats * kTicksPerBeat;
            const auto at = [&](Family f, u32 tier, Vec2 pos) { wm.spawnEnemy(SpeciesTable::index(f, tier, kVariantA), pos, angleOf(Vec2{} - pos), land); };
            for (u32 k = 0; k < 3; ++k) at(Family::Hound, 1, Vec2{360.0f + 40.0f * k, -150.0f + 30.0f * k});
            at(Family::Ram, 1, Vec2{-380.0f, 120.0f});
            for (u32 k = 0; k < 8; ++k) at(Family::Shoal, 1, Vec2{-200.0f + 18.0f * (k % 4), -200.0f + 18.0f * (k / 4)});
            at(Family::Drifter, 2, Vec2{120.0f, 230.0f});
            scriptFlags_ |= kSpawned;
            // "warp": the first eight frames (the rings at their largest); "warp_land": the last frames of the
            // warp-in and the landing, from five frames before it. (Spawned between ticks, so no WarpStart
            // event reaches scriptEvents(): the moment is known here.)
            const i64 toLand = static_cast<i64>(std::llround((land - music) / tpt));
            captureFrom_ = static_cast<i64>(frameNo_) + (is(p, "warp") ? 0 : 1 + toLand - 5);
        }
    }
    (void)w;
}

void Play::scriptEvents() {
    if (captureFrom_ >= 0) return;
    const World& w = *world_;
    const char* p = opt_.picture;
    const float tpt = clock_.ticksPerSecond() * kDt;
    const i64 now = static_cast<i64>(frameNo_);
    if (is(p, "fire")) {
        if (scriptTick_ == 80) captureFrom_ = now + 1;
        return;
    }
    for (u32 k = 0; k < w.eventCount(); ++k) {
        const Event& e = w.events()[k];
        if ((is(p, "kill_small") || is(p, "kill_medium") || is(p, "kill_large")) && e.type == EventType::Kill) captureFrom_ = now;
        if (is(p, "shards") && e.type == EventType::Pickup) captureFrom_ = now;
        if (is(p, "hit") && e.type == EventType::Hit) captureFrom_ = now;
        if (is(p, "nova") && e.type == EventType::NovaPress) captureFrom_ = now;
        if (is(p, "nova_front") && e.type == EventType::NovaBlast) captureFrom_ = now;
        if ((is(p, "dash") || is(p, "perfect_dash")) && e.type == EventType::Dash) captureFrom_ = now;
        if (is(p, "sentry") && e.type == EventType::Telegraph) {
            // The end of the glint (one beat) and the first volley.
            const i64 toFire = static_cast<i64>(std::llround((e.tick - w.music()) / tpt));
            captureFrom_ = now + toFire - 4;
        }
        if (captureFrom_ >= 0) {
            RF_LOG("play: picture '%s' captures from frame %lld (event at frame %lld, tick %.1f)", p, static_cast<long long>(captureFrom_),
                   static_cast<long long>(now), w.music());
            return;
        }
    }
}

}  // namespace rf
