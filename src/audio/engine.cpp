#include "audio/engine.h"

#include <cmath>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <new>

namespace rf::audio {

namespace {

constexpr u32 kLeadVoices = 64;
constexpr u32 kLeadPerPlayer = 12;  // per player (a 1/32 line and its harmony, or four lines at 1/16)
constexpr u32 kLeadFadeOut = 240;   // frames: a replaced note fades instead of clicking
constexpr u32 kBassVoices = 4;
constexpr u32 kStabVoices = 8;
constexpr u32 kPadVoices = 4;
constexpr u32 kSampleVoices = 96;
constexpr u32 kPendingMax = 64;
constexpr i64 kGrid = 2;  // sequencer scan resolution in ticks (every grid in use is a multiple)

// musicproto._render: numbers that live in the prototype's code rather than in the JSON.
constexpr double kSectionRampS = 0.03;  // section gains change 30 ms either side of the bar line
constexpr double kDuckS = 0.6;          // length of the kick's duck segment
constexpr float kPanClap = 0.08f, kPanHatOff = -0.25f, kPanHat16 = 0.25f, kPanOhat = 0.2f, kPanShaker = 0.45f,
                kPanStab = 0.3f;
constexpr float kDoubleVelOn = 0.85f, kDoubleVelOff = 0.7f, kDoublePan = 0.25f, kDoublePanRate = 0.9f;
constexpr float kSparseVel = 0.8f, kSparsePanMid = 0.3f, kSparsePanOther = -0.2f;
constexpr float kAccentBeat = 1.0f, kAccentEighth = 0.82f, kAccentOther = 0.7f;  // staged lead (prototype)
constexpr float kLeadPanDepth = 0.25f, kLeadPanRate = 0.9f;
constexpr double kHatsBreakLowpass = 5000.0;
constexpr double kMasterHighpass = 30.0;
constexpr float kBreakSwell = 0.7f;
constexpr double kBassTailS = 0.12;
constexpr float kRollLo = 0.35f, kRollHi = 1.0f;          // staged snare roll level
constexpr float kBuildBrightLo = 1.0f, kBuildBrightHi = 1.9f, kDropBright = 1.5f, kBreakBright = 0.55f;
// The lead timbres of sfxproto.py (P2-P4 are code there too).
constexpr float kP2Detune = 6.0f, kP2Pre = 0.42f, kP2Q = 1.8f, kP2FcBase = 700.0f, kP2FcSweep = 3100.0f, kP2FcTau = 0.060f,
                kP2Attack = 0.002f, kP2Decay = 0.15f;
constexpr float kP3Len = 0.36f, kP3Ratio = 2.0f, kP3Index = 2.4f, kP3ModTau = 0.15f, kP3Gain = 0.8f, kP3Attack = 0.002f;
constexpr float kP4Len = 0.30f, kP4Detune = 12.0f, kP4Oct = 0.3f, kP4Pre = 0.6f, kP4Q = 1.4f, kP4FcBase = 900.0f,
                kP4FcSweep = 2400.0f, kP4FcTau = 0.050f, kP4Attack = 0.002f, kP4Decay = 0.12f;
constexpr float kLeadKeytrack = 0.5f;  // P2 and P4: (f / 440) ^ 0.5 like the P1 patch
// sfxproto gameplay mock.
constexpr float kKillSmallBoost = 1.6f;
constexpr float kSfxReverbSend = 0.18f;
constexpr float kKillPanSmall = 0.6f, kKillPanMedium = 0.5f, kShardPan = 0.3f;
constexpr double kShardRunGapS = 0.5;  // a pause this long starts a new Shard run (engine choice)
constexpr float kMetronomeBpm = 120.0f;  // design/screens.md sync calibration
constexpr float kClickGain = 0.5f;
// Output stage: a soft knee that leaves the music bed (peaks near -5 dBFS) untouched and keeps
// music plus effects below -1 dBFS true peak (the ceiling leaves room for inter-sample overshoot).
constexpr float kLimitKnee = 0.66f, kLimitCeiling = 0.8f;

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }


inline float limit(float x) {
    const float a = std::fabs(x);
    if (a <= kLimitKnee) return x;
    const float y = kLimitKnee + (kLimitCeiling - kLimitKnee) * std::tanh((a - kLimitKnee) / (kLimitCeiling - kLimitKnee));
    return x < 0.0f ? -y : y;
}

inline i64 floorDiv(i64 a, i64 b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

}  // namespace

const char* EngineProfile::name(u32 s) {
    static const char* const kNames[] = {"lead", "bass", "stab", "pad", "samples", "bus fx", "delay", "reverb", "sfx reverb", "master"};
    return s < countOf(kNames) ? kNames[s] : "?";
}

const char* beatKindName(BeatKind k) {
    static const char* const kNames[] = {"kick",  "snare", "clap",  "hat",   "shaker",  "bass",  "lead",
                                         "stab",  "pulse", "pad",   "riser", "swell",   "impact", "bar",
                                         "beat",  "section", "click", "sfx", "tempo", "variant", "track"};
    const u32 i = static_cast<u32>(k);
    return i < countOf(kNames) ? kNames[i] : "?";
}

// ---------------------------------------------------------------- voices

struct Engine::LeadVoice {
    bool active = false;
    Timbre timbre = Timbre::P1Saw;
    u8 player = 0;
    u32 delay = 0, age = 0, length = 0, fade = 240;
    float vel = 0.0f, gl = 0.0f, gr = 0.0f, drive = 0.0f;
    float pre = 0.5f, sub = 0.0f, k = 1.0f;
    float fcStart = 0.0f, fcEnd = 0.0f, kt = 1.0f;
    float fcTauN = 1.0f, attackN = 1.0f, decayN = 1.0f;
    float hz = 0.0f;
    float decMul = 1.0f;
    // Control-rate cache: the coefficients at the next knot become the current knot's.
    u32 knot = ~0u;
    SvfCoefs cNext{};
    SawOsc saw[4];
    SquareOsc sq[2];
    SvfLowpass svf;
};

struct Engine::BassVoice {
    bool active = false;
    u32 delay = 0, age = 0, length = 0, gate = 0, fade = 240;
    float vel = 0.0f, k = 1.0f, fcStart = 0.0f, fcEnd = 0.0f, fcTauN = 1.0f, attackN = 1.0f, decayN = 1.0f,
          releaseN = 1.0f, sqMix = 0.5f, sub = 0.0f, drive = 1.0f, gain = 1.0f;
    float subPhase = 0.0f, subDt = 0.0f;
    SawOsc saw;
    SquareOsc sq;
    SvfLowpass svf;
};

struct Engine::StabVoice {
    bool active = false;
    u32 delay = 0, age = 0, length = 0, fade = 240;
    u32 oscCount = 0;
    float vel = 0.0f, gl = 0.0f, gr = 0.0f, k = 1.0f, fcStart = 0.0f, fcEnd = 0.0f, fcTauN = 1.0f, attackN = 1.0f,
          decayN = 1.0f, norm = 1.0f;
    SawOsc saw[8];
    SvfLowpass svf;
};

struct Engine::PadVoice {
    bool active = false;
    u32 delay = 0, age = 0, length = 0, hold = 0, fade = 480;
    u32 oscCount = 0;  // per channel
    float level = 0.0f, gain = 0.0f, bright = 1.0f, k = 1.0f, fcBase = 0.0f, fcLfo = 0.0f, lfoW = 0.0f, attackN = 1.0f,
          releaseN = 1.0f, norm = 1.0f;
    SawOsc saw[2][12];
    SvfLowpass svf[2];
};

struct Engine::SampleVoice {
    bool active = false;
    const float* data = nullptr;
    u32 frames = 0;
    u8 channels = 1;
    Bus bus = Bus::Drums;
    u32 delay = 0;
    u32 age = 0;  // frames played (voice stealing)
    double pos = 0.0;
    float rate = 1.0f;
    float gl = 0.0f, gr = 0.0f;
};

struct Engine::Pending {
    i64 frame = 0;
    double tick = -1.0;
    i32 sample = -1;   // a bank sample on a bus, or an effect
    Bus bus = Bus::Fx;
    Sfx sfx = Sfx::Count;
    BeatKind kind = BeatKind::Sfx;
    i32 midi = -1;
    float gain = 0.0f, pan = 0.0f, rate = 1.0f;
};

struct Engine::Player {
    bool fire = false;
    Timbre timbre = Timbre::P1Saw;
    u8 riff[static_cast<u32>(Riff::Count)] = {};
    u8 split = 0, doubleTime = 0, pierce = 0, overcharge = 0, crit = 0;
    bool prism = false, hyper = false;
    float distance = 0.0f;
};

struct Engine::BarPlan {
    i64 tick = 0;
    u32 index = 0;
    bool silent = true;
    Section section = Section::Groove;
    u32 inSection = 0, sectionBars = 1;
    const Chord* chord = nullptr;
    float pat[kPatCount] = {};       // per-layer level (0: off)
    bool hatOffBreak = false;
    BassKey bass = BassKey::None;
    float bassFade = 1.0f;
    LeadMode lead = LeadMode::Off;   // Staged
    float pad = 0.0f, padBright = 1.0f;
    i64 rollDiv = 0;                 // 0: no snare roll
    u32 rollBar = 0, rollBars = 1;
    float rollLo = kRollLo, rollHi = kRollHi;
    bool lastBuildBar = false;
    bool fill = false;
    bool pulseEveryBeat = false;
    bool riffDouble = false;         // hyper: Riff patterns at double rate
    bool sparse[kMaxPlayers] = {};   // Break: the bed echoes this player's arp
    u32 leadGrid[kMaxPlayers] = {};
    bool leadStopsHalf = false;      // Build's last bar: the lead stops after beat 2
};

template <typename V>
V* Engine::allocVoice(V* pool, u32 count) {
    V* oldest = &pool[0];
    for (u32 i = 0; i < count; ++i) {
        if (!pool[i].active) return &pool[i];
        if (pool[i].age > oldest->age) oldest = &pool[i];
    }
    stats_.steals.fetch_add(1, std::memory_order_relaxed);
    return oldest;
}

Engine::~Engine() {
    delete[] lead_;
    delete[] bass_;
    delete[] stab_;
    delete[] pad_;
    delete[] smp_;
    delete[] pending_;
    delete[] players_;
    delete plan_;
    std::free(delayLine_);
}

bool Engine::init(const MusicData& music, const SampleBank& bank, u32 sampleRate, u64 seed) {
    music_ = &music;
    bank_ = &bank;
    sr_ = sampleRate;
    rng_.reseed(seed);
    sfxRng_.reseed(seed * 7 + 3);
    lead_ = new (std::nothrow) LeadVoice[kLeadVoices];
    bass_ = new (std::nothrow) BassVoice[kBassVoices];
    stab_ = new (std::nothrow) StabVoice[kStabVoices];
    pad_ = new (std::nothrow) PadVoice[kPadVoices];
    smp_ = new (std::nothrow) SampleVoice[kSampleVoices];
    pending_ = new (std::nothrow) Pending[kPendingMax];
    players_ = new (std::nothrow) Player[kMaxPlayers];
    plan_ = new (std::nothrow) BarPlan;
    u32 cap = 1;
    while (cap < static_cast<u32>(6.0 * sampleRate)) cap <<= 1;  // lead delay history (several seconds)
    delayLine_ = static_cast<float*>(std::calloc(cap, sizeof(float)));
    delayMask_ = cap - 1;
    if (!lead_ || !bass_ || !stab_ || !pad_ || !smp_ || !pending_ || !players_ || !plan_ || !delayLine_) return false;
    ReverbPatch sets[FdnReverb::kMaxSets];
    for (u32 t = 0; t < MusicData::kMaxTracks; ++t)
        for (u32 v = 0; v < MusicData::kMaxVariants; ++v)
            sets[t * MusicData::kMaxVariants + v] =
                (t < music.trackCount && v < music.variantCount) ? music.sound[t][v].reverb : music.sound[0][0].reverb;
    if (!reverb_.init(sampleRate, sets, FdnReverb::kMaxSets)) return false;
    duckLen_ = framesFor(kDuckS, sampleRate);
    variant_ = music.defaultVariant;
    track_ = 0;
    hatsLp_[0].setLowpass(kHatsBreakLowpass, sampleRate);
    hatsLp_[1] = hatsLp_[0];
    masterHp_[0].setHighpass(kMasterHighpass, sampleRate);
    masterHp_[1] = masterHp_[0];
    tempo_.reset(music.tracks[0].bpm, sampleRate);
    nominalTpf_ = tempo_.last().ticksPerFrame;
    applyParams();
    outGain_ = outGainTarget_;
    return true;
}

// ---------------------------------------------------------------- parameters and modes

void Engine::applyParams() {
    tr_ = &music_->tracks[track_];
    sp_ = &music_->sound[track_][variant_];
    padHp_[0].setHighpass(sp_->pad.highpass, sr_);
    padHp_[1].b0 = padHp_[0].b0;
    padHp_[1].b1 = padHp_[0].b1;
    padHp_[1].b2 = padHp_[0].b2;
    padHp_[1].a1 = padHp_[0].a1;
    padHp_[1].a2 = padHp_[0].a2;
    reverb_.select(track_ * MusicData::kMaxVariants + variant_);
    // Ping-pong delay (musicproto.ping_pong): taps at k x d, gain mix x feedback^(k-1).
    const double stepS = 60.0 / tr_->bpm / 4.0;
    const u32 d = framesFor(sp_->delay.steps * stepS, sr_);
    if (d != delayD_) {
        delayDOld_ = delayD_;
        delayD_ = d;
        delayFade_ = delayDOld_ ? 256 : 0;
    }
    delayRepeats_ = static_cast<u32>(sp_->delay.repeats + 0.5f);
    if (delayRepeats_ > countOf(delayTap_)) delayRepeats_ = countOf(delayTap_);
    while (delayRepeats_ && static_cast<u64>(delayRepeats_) * delayD_ > delayMask_) --delayRepeats_;
    for (u32 k = 1; k <= delayRepeats_; ++k) delayTap_[k - 1] = sp_->delay.mix * std::pow(sp_->delay.feedback, static_cast<float>(k - 1));
    outGainTarget_ = std::pow(10.0f, music_->outputGainDb(track_, variant_) / 20.0f);
    stats_.track.store(static_cast<u8>(track_), std::memory_order_relaxed);
    stats_.variant.store(static_cast<u8>(variant_), std::memory_order_relaxed);
}

void Engine::startMode(Mode m, u32 track, bool loop) {
    mode_ = m;
    loop_ = loop;
    // A new song takes the requested Track and any pending sound variant at once.
    if (track < music_->trackCount) track_ = track;
    if (pendingVariant_ >= 0) {
        variant_ = static_cast<u32>(pendingVariant_);
        pendingVariant_ = -1;
    }
    applyParams();
    outGain_ = outGainTarget_;
    // The song clock restarts at the next frame: tick 0 is the first downbeat.
    songStartFrame_ = frame_;
    const float bpm = m == Mode::Metronome ? kMetronomeBpm : tr_->bpm;
    tempo_.reset(bpm, sr_, static_cast<double>(frame_), 0.0);
    nominalTpf_ = tempo_.last().ticksPerFrame;
    emit(BeatKind::Tempo, frame_, 0.0, 0, 0, 0, bpm, 0, nominalTpf_);
    nextTick_ = 0;
    bar_ = 0;
    chordBar_ = 0;
    stabCount_ = 0;
    section_ = Section::Groove;
    sectionBar_ = 0;
    sectionBars_ = 1;
    nextSection_ = false;
    forceSection_ = -1;
    buildFollowed_ = false;
    pendingTrack_ = -1;
    nextTrackAtBar_ = -1;
    ramping_ = false;
    gainHead_ = gainCount_ = 0;
    for (bool& b : bedOn_) b = false;
    if (!formSet_) form_ = track_ == 0 ? Form::Short : Form::Standard;
    stats_.mode.store(static_cast<u8>(m), std::memory_order_relaxed);
    updateBpmStat();
}

void Engine::updateBpmStat() {
    stats_.bpm.store(static_cast<float>(tempo_.bpmAt(static_cast<double>(frame_), sr_)), std::memory_order_relaxed);
}

void Engine::applyCommand(const Command& c) {
    Player* p = c.a < kMaxPlayers ? &players_[c.a] : nullptr;
    switch (c.type) {
        case CmdType::SetMode: startMode(static_cast<Mode>(c.a), c.b, c.c != 0); break;
        case CmdType::SetTrack:
            if (c.a < music_->trackCount) pendingTrack_ = c.a;
            break;
        case CmdType::SetVariant:
            if (c.a < music_->variantCount) pendingVariant_ = c.a;
            break;
        case CmdType::SetIntensity:
            intensity_ = clamp01(c.x);
            stats_.intensity.store(intensity_, std::memory_order_relaxed);
            break;
        case CmdType::NextSection: nextSection_ = true; break;
        case CmdType::SetSection:
            if (c.a < static_cast<u8>(Section::Count)) forceSection_ = c.a;
            break;
        case CmdType::HoldSection: hold_ = c.a != 0; break;
        case CmdType::SetForm:
            form_ = static_cast<Form>(c.a);
            formSet_ = true;
            break;
        case CmdType::SetFire:
            if (p) p->fire = c.b != 0;
            break;
        case CmdType::SetTimbre:
            if (p && c.b < static_cast<u8>(Timbre::Count)) p->timbre = static_cast<Timbre>(c.b);
            break;
        case CmdType::SetPlayers:
            playerCount_ = c.a < 1 ? 1 : (c.a > kMaxPlayers ? kMaxPlayers : c.a);
            localPlayer_ = c.b < kMaxPlayers ? c.b : 0;
            break;
        case CmdType::SetRiff:
            if (p && c.b < static_cast<u8>(Riff::Count)) p->riff[c.b] = c.c;
            break;
        case CmdType::SetMods:
            if (p) {
                p->split = c.mods[0];
                p->doubleTime = c.mods[1];
                p->pierce = c.mods[2];
                p->overcharge = c.mods[3];
                p->crit = c.mods[4];
            }
            break;
        case CmdType::SetBoost:
            if (p) {
                if (c.b == 0) p->prism = c.c != 0;
                if (c.b == 1) p->hyper = c.c != 0;
            }
            break;
        case CmdType::SetDistance:
            if (p) p->distance = c.x;
            break;
        case CmdType::Sfx: {
            const Sfx kind = static_cast<Sfx>(c.a);
            const i32 midi = c.b ? c.b : -1;  // pitched effects: the note (Nova inhale: its length in 1/32 steps)
            if (c.when == Timing::Now) {
                playSfx(kind, midi, sfxLevel(kind) * c.x, c.y, frame_, -1.0);
            } else if (pendingCount_ < kPendingMax) {
                double tick = c.tick;
                if (c.when == Timing::Next32) tick = std::ceil(tempo_.tickAt(static_cast<double>(frame_)) / 12.0) * 12.0;
                Pending& q = pending_[pendingCount_++];
                q = Pending{};
                q.frame = frameOf(tick);
                q.tick = tick;
                q.sfx = kind;
                q.midi = midi;
                q.gain = sfxLevel(kind) * c.x;
                q.pan = c.y;
            }
            break;
        }
        case CmdType::Kill:
            if (c.a < 3) killCount_[c.a] += c.b ? c.b : 1;
            break;
        case CmdType::Shard: {
            if (static_cast<double>(frame_ - lastShardFrame_) > kShardRunGapS * sr_) shardRun_ = 0;
            lastShardFrame_ = frame_;
            if (pendingCount_ < kPendingMax) {
                const double tick = std::ceil(tempo_.tickAt(static_cast<double>(frame_)) / 12.0) * 12.0;
                Pending& q = pending_[pendingCount_++];
                q = Pending{};
                q.frame = frameOf(tick);
                q.tick = tick;
                q.sfx = Sfx::Shard;
                q.midi = bank_->shardScale(track_)[shardRun_ < 10 ? shardRun_ : 9];
                q.gain = sfxLevel(Sfx::Shard) * c.x;
                q.pan = static_cast<float>((sfxRng_.uniform() * 2.0 - 1.0) * kShardPan);
            }
            ++shardRun_;
            break;
        }
        case CmdType::Nova: {
            // GAME_DESIGN R6: the inhale from the press to the next beat, the blast on that beat.
            const double now = tempo_.tickAt(static_cast<double>(frame_));
            double beat = std::ceil(now / kPpq) * kPpq;
            if (beat - now < 6.0) beat += kPpq;  // too close: the following beat
            const u32 steps = static_cast<u32>(std::lround((beat - now) / 12.0));
            const i32 id = bank_->inhale(track_, steps);
            playSample(id, Bus::Sfx, frame_, sfxLevel(Sfx::NovaInhale) * c.x, 0.0f);
            emit(BeatKind::Sfx, frame_, -1.0, static_cast<u8>(Sfx::NovaInhale), 0, 0, sfxLevel(Sfx::NovaInhale));
            if (pendingCount_ < kPendingMax) {
                Pending& q = pending_[pendingCount_++];
                q = Pending{};
                q.frame = frameOf(beat);
                q.tick = beat;
                q.sfx = Sfx::NovaBlast;
                q.gain = sfxLevel(Sfx::NovaBlast) * c.x;
            }
            break;
        }
        case CmdType::Warp: {
            // The warp-in swell ends on the beat the enemy becomes solid.
            if (pendingCount_ < kPendingMax) {
                // The chord of the bar the enemy lands in (chordBar_ already counts the current bar).
                const i64 ahead = floorDiv(static_cast<i64>(c.tick), kTicksPerBar) - plan_->tick / kTicksPerBar;
                const Chord& ch = tr_->chordAtBar(static_cast<u32>(static_cast<i64>(chordBar_) - 1 + (ahead > 0 ? ahead : 0)));
                const i32 id = bank_->sfx(Sfx::Spawn, ch.bass + 12);
                Pending& q = pending_[pendingCount_++];
                q = Pending{};
                q.frame = frameOf(c.tick) - (id >= 0 ? static_cast<i64>(bank_->sample(id).frames) : 0);
                if (q.frame < frame_) q.frame = frame_;
                q.tick = -1.0;  // the swell ends on c.tick
                q.sfx = Sfx::Spawn;
                q.midi = ch.bass + 12;
                q.gain = sfxLevel(Sfx::Spawn) * c.x;
                q.pan = c.y;
            }
            break;
        }
        case CmdType::Note: {
            const Timbre t = c.a < static_cast<u8>(Timbre::Count) ? static_cast<Timbre>(c.a) : Timbre::P1Saw;
            playLead(0, t, c.b, c.x, c.level, c.y, frame_);
            break;
        }
        case CmdType::Drum: {
            const DrumKit& kit = bank_->kit(track_, variant_);
            const Pat pat = static_cast<Pat>(c.a);
            const i32 id = pat == Pat::Kick ? kit.kick : (pat == Pat::Ohat ? kit.ohat : (pat == Pat::Clap ? bank_->clap() : kit.hat));
            playSample(id, mode_ == Mode::Script ? Bus::Sfx : Bus::Drums, frame_, c.x, c.y);
            break;
        }
        case CmdType::SetGains:
            musicGain_ = c.x;
            sfxGain_ = c.y;
            break;
        case CmdType::Reset:
            for (u32 i = 0; i < kLeadVoices; ++i) lead_[i].active = false;
            for (u32 i = 0; i < kBassVoices; ++i) bass_[i].active = false;
            for (u32 i = 0; i < kStabVoices; ++i) stab_[i].active = false;
            for (u32 i = 0; i < kPadVoices; ++i) pad_[i].active = false;
            for (u32 i = 0; i < kSampleVoices; ++i) smp_[i].active = false;
            pendingCount_ = 0;
            std::memset(delayLine_, 0, sizeof(float) * (delayMask_ + 1));
            reverb_.clear();
            mode_ = Mode::Silent;
            break;
    }
}

// ---------------------------------------------------------------- events and voices

void Engine::emit(BeatKind k, i64 frame, double tick, u8 a, u8 b, u8 c, float value, u32 index, double x) {
    BeatEvent e;
    e.frame = frame;
    e.tick = tick;
    e.kind = k;
    e.a = a;
    e.b = b;
    e.c = c;
    e.value = value;
    e.index = index;
    e.x = x;
    events_.push(e);
}

void Engine::playSample(i32 id, Bus bus, i64 frame, float gain, float pan, float rate) {
    if (id < 0) return;
    const Sample& s = bank_->sample(id);
    SampleVoice* v = allocVoice(smp_, kSampleVoices);
    v->active = true;
    v->data = s.data;
    v->frames = s.frames;
    v->channels = s.channels;
    v->bus = bus;
    v->delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
    v->age = 0;
    v->pos = 0.0;
    v->rate = rate;
    if (s.channels == 2) {
        v->gl = v->gr = gain;
    } else {
        panGains(pan, gain, v->gl, v->gr);
    }
}

void Engine::playKick(float gain, i64 frame, double tick) {
    playSample(bank_->kit(track_, variant_).kick, Bus::Drums, frame, gain, 0.0f);
    if (frame > lastKick_) {
        prevKick_ = lastKick_;
        lastKick_ = frame;
    }
    emit(BeatKind::Kick, frame, tick, 0, 0, 0, gain);
}

void Engine::playSfx(Sfx kind, i32 midi, float gain, float pan, i64 frame, double tick) {
    i32 id = -1;
    float rate = 1.0f;
    if (kind == Sfx::NovaInhale) {
        id = bank_->inhale(track_, midi > 0 ? static_cast<u32>(midi) : 8u);
        midi = -1;
    } else {
        id = bank_->sfx(kind, midi);
        // Pitched effects are baked on the Tracks' chord tones; any other note is the nearest one played
        // faster or slower (at most a fifth away).
        const i32 baked = id >= 0 ? bank_->sample(id).midi : -1;
        if (midi >= 0 && baked >= 0 && baked != midi && std::abs(midi - baked) <= 7)
            rate = static_cast<float>(std::exp2((midi - baked) / 12.0));
    }
    if (id < 0) return;
    playSample(id, Bus::Sfx, frame, gain, pan, rate);
    emit(BeatKind::Sfx, frame, tick, static_cast<u8>(kind), 0, 0, gain, 0, midi >= 0 ? midiHz(midi) : 0.0);
    // sfxproto: hits and blasts duck the music (depth, time constant), nothing else does.
    float depth = 0.0f, tau = 0.0f;
    if (kind == Sfx::Hit) {
        depth = 0.5f;
        tau = 0.18f;
    } else if (kind == Sfx::KillLarge) {
        depth = 0.25f;
        tau = 0.12f;
    } else if (kind == Sfx::NovaBlast) {
        depth = 0.45f;
        tau = 0.25f;
    }
    if (depth > 0.0f) {
        Dip* d = nullptr;
        if (dipCount_ < countOf(dips_)) {
            d = &dips_[dipCount_++];
        } else {
            d = &dips_[0];
            for (Dip& x : dips_)
                if (x.frame < d->frame) d = &x;
        }
        *d = {frame, depth, tau};
    }
}

void Engine::playLead(u8 player, Timbre timbre, float midi, float vel, float gain, float pan, i64 frame, float lengthScale,
                      float drive) {
    // Each player's lead holds at most kLeadPerPlayer notes (the prototype's notes overlap: about three
    // at 1/16, six at 1/32). Past that, the player's oldest note fades out over a few milliseconds.
    u32 mine = 0;
    LeadVoice* oldest = nullptr;
    for (u32 i = 0; i < kLeadVoices; ++i) {
        LeadVoice& o = lead_[i];
        if (!o.active || o.player != player || o.length <= o.age + kLeadFadeOut) continue;
        ++mine;
        if (!oldest || o.age > oldest->age) oldest = &o;
    }
    if (mine >= kLeadPerPlayer && oldest) {
        oldest->length = oldest->age + kLeadFadeOut;
        oldest->fade = kLeadFadeOut;
    }
    LeadVoice* v = allocVoice(lead_, kLeadVoices);
    *v = LeadVoice{};
    const float sr = static_cast<float>(sr_);
    const double hz = midiHz(midi);
    v->active = true;
    v->timbre = timbre;
    v->player = player;
    v->delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
    v->vel = vel;
    v->drive = drive;
    v->hz = static_cast<float>(hz);
    panGains(pan, gain, v->gl, v->gr);
    const LeadPatch& lp = sp_->lead;
    switch (timbre) {
        case Timbre::P1Saw: {
            // musicproto.render_plucks: two saws (random phases) + a quiet square an octave down.
            const double det = std::pow(2.0, lp.detuneCents / 1200.0);
            v->saw[0].phase = static_cast<float>(rng_.uniform());
            v->saw[1].phase = static_cast<float>(rng_.uniform());
            v->saw[0].dt = static_cast<float>(hz / sr_);
            v->saw[1].dt = static_cast<float>(hz * det / sr_);
            v->sq[0].dt = static_cast<float>(hz * 0.5 / sr_);
            v->sub = lp.sub;
            v->pre = 0.5f;
            v->k = 1.0f / lp.q;
            v->fcStart = lp.fcStart;
            v->fcEnd = lp.fcEnd;
            v->fcTauN = lp.fcTau * sr;
            v->kt = static_cast<float>(std::pow(hz / 440.0, static_cast<double>(lp.keytrack)));
            v->attackN = lp.attack * sr;
            v->decayN = lp.decay * lengthScale * sr;
            v->length = framesFor(lp.length * lengthScale, sr_);
            v->fade = 240;
            break;
        }
        case Timbre::P2Square: {
            v->sq[0].dt = static_cast<float>(hz / sr_);
            v->sq[1].dt = static_cast<float>(hz * std::pow(2.0, kP2Detune / 1200.0) / sr_);
            v->pre = kP2Pre;
            v->k = 1.0f / kP2Q;
            v->fcStart = kP2FcBase + kP2FcSweep;
            v->fcEnd = kP2FcBase;
            v->fcTauN = kP2FcTau * sr;
            v->kt = static_cast<float>(std::pow(hz / 440.0, static_cast<double>(kLeadKeytrack)));
            v->attackN = kP2Attack * sr;
            v->decayN = kP2Decay * lengthScale * sr;
            v->length = framesFor(lp.length * lengthScale, sr_);
            v->fade = 240;
            break;
        }
        case Timbre::P3Bell: {
            v->saw[0].phase = 0.0f;  // carrier phase (cycles)
            v->saw[1].phase = 0.0f;  // modulator phase
            v->saw[0].dt = static_cast<float>(hz / sr_);
            v->saw[1].dt = static_cast<float>(hz * kP3Ratio / sr_);
            v->attackN = kP3Attack * sr;
            v->decayN = (kP3Len / 3.5f) * lengthScale * sr;
            v->fcTauN = kP3ModTau * sr;  // modulation index decay
            v->length = framesFor(kP3Len * lengthScale, sr_);
            v->fade = framesFor(0.008, sr_);
            v->vel = vel * kP3Gain;
            break;
        }
        case Timbre::P4Stab: {
            const double cents[3] = {-kP4Detune, 0.0, kP4Detune};
            for (u32 i = 0; i < 3; ++i) v->saw[i].dt = static_cast<float>(hz * std::pow(2.0, cents[i] / 1200.0) / sr_);
            v->saw[3].dt = static_cast<float>(hz * 2.0 / sr_);
            v->pre = kP4Pre;
            v->k = 1.0f / kP4Q;
            v->fcStart = kP4FcBase + kP4FcSweep;
            v->fcEnd = kP4FcBase;
            v->fcTauN = kP4FcTau * sr;
            v->kt = static_cast<float>(std::pow(hz / 440.0, static_cast<double>(kLeadKeytrack)));
            v->attackN = kP4Attack * sr;
            v->decayN = kP4Decay * lengthScale * sr;
            v->length = framesFor(kP4Len * lengthScale, sr_);
            v->fade = 240;
            break;
        }
        default: v->active = false; break;
    }
    v->decMul = std::exp(-1.0f / v->decayN);
}

void Engine::playBass(float midi, float vel, float steps, i64 frame) {
    BassVoice* v = allocVoice(bass_, kBassVoices);
    *v = BassVoice{};
    const BassPatch& p = sp_->bass;
    const float sr = static_cast<float>(sr_);
    const double hz = midiHz(midi);
    const double stepS = 60.0 / tr_->bpm / 4.0;
    v->active = true;
    v->delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
    v->length = framesFor(tr_->longestBassSteps * stepS + kBassTailS, sr_);
    v->gate = static_cast<u32>(steps * stepS * sr);  // t < gate (prototype compares times)
    v->fade = 240;
    v->vel = vel;
    v->gain = sp_->mix.bass;
    v->k = 1.0f / p.q;
    v->fcStart = p.fcStart;
    v->fcEnd = p.fcEnd;
    v->fcTauN = p.fcTau * sr;
    v->attackN = p.attack * sr;
    v->decayN = p.decay * sr;
    v->releaseN = p.release * sr;
    v->sqMix = p.squareMix;
    v->sub = p.sub;
    v->drive = p.drive;
    v->saw.dt = static_cast<float>(hz / sr_);
    v->sq.dt = static_cast<float>(hz / sr_);
    v->subDt = static_cast<float>(hz * 0.5 / sr_);
}

void Engine::playStab(const Chord& chord, float vel, float pan, i64 frame) {
    StabVoice* v = allocVoice(stab_, kStabVoices);
    *v = StabVoice{};
    const StabPatch& p = sp_->stab;
    const float sr = static_cast<float>(sr_);
    const double det = std::pow(2.0, p.detuneCents / 1200.0);
    v->active = true;
    v->delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
    v->length = framesFor(p.length, sr_);
    v->fade = 240;
    v->vel = vel;
    panGains(pan, sp_->mix.stab, v->gl, v->gr);
    v->k = 1.0f / p.q;
    v->fcStart = p.fcStart;
    v->fcEnd = p.fcEnd;
    v->fcTauN = p.fcTau * sr;
    v->attackN = p.attack * sr;
    v->decayN = p.decay * sr;
    const u32 notes = chord.stabCount < 4 ? chord.stabCount : 4;
    for (u32 i = 0; i < notes; ++i) {
        const double hz = midiHz(chord.stab[i]);
        v->saw[2 * i].dt = static_cast<float>(hz / sr_);
        v->saw[2 * i + 1].dt = static_cast<float>(hz * det / sr_);
    }
    v->oscCount = notes * 2;
    v->norm = notes ? 1.0f / (2.0f * notes) : 0.0f;
}

void Engine::playPad(const Chord& chord, float level, float bright, i64 frame) {
    PadVoice* v = allocVoice(pad_, kPadVoices);
    *v = PadVoice{};
    const PadPatch& p = sp_->pad;
    const float sr = static_cast<float>(sr_);
    const double barS = 240.0 / tr_->bpm;
    v->active = true;
    v->delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
    v->hold = static_cast<u32>(barS * sr);
    v->length = framesFor(barS + p.release * 4.0, sr_);
    v->fade = 480;
    v->level = level;
    v->gain = sp_->mix.pad;
    v->bright = bright;
    v->k = 1.0f / p.q;
    v->fcBase = p.fcBase;
    v->fcLfo = p.fcLfo;
    v->lfoW = static_cast<float>(2.0 * kPiD * p.lfoHz / sr_);
    v->attackN = p.attack * sr;
    v->releaseN = p.release * sr;
    const u32 notes = chord.padCount < 6 ? chord.padCount : 6;
    // musicproto.render_pads: the left channel detunes up, the right down; the same random phases for
    // every chord on a channel (rng(31 + ch)).
    Random phases[2] = {Random(31), Random(32)};
    for (u32 ch = 0; ch < 2; ++ch) {
        const double det = std::pow(2.0, (ch == 0 ? 1.0 : -1.0) * p.detuneCents / 1200.0);
        for (u32 i = 0; i < notes; ++i) {
            const double hz = midiHz(chord.pad[i]);
            v->saw[ch][i].dt = static_cast<float>(hz / sr_);
            v->saw[ch][i].phase = static_cast<float>(phases[ch].uniform());
        }
        for (u32 i = 0; i < notes; ++i) {
            const double hz = midiHz(chord.pad[i]);
            v->saw[ch][notes + i].dt = static_cast<float>(hz * det / sr_);
            v->saw[ch][notes + i].phase = static_cast<float>(phases[ch].uniform());
        }
    }
    v->oscCount = notes * 2;
    v->norm = notes ? 1.0f / (2.0f * notes) : 0.0f;
}

// ---------------------------------------------------------------- sequencer

void Engine::addSectionGain(double frame, float gain) {
    if (gainCount_ && gainPts_[(gainHead_ + gainCount_ - 1) % countOf(gainPts_)].frame >= frame) {
        // Out of order (a forced change): drop the points that lie at or after it.
        while (gainCount_ && gainPts_[(gainHead_ + gainCount_ - 1) % countOf(gainPts_)].frame >= frame) --gainCount_;
    }
    if (gainCount_ == countOf(gainPts_)) {
        gainHead_ = (gainHead_ + 1) % countOf(gainPts_);
        --gainCount_;
    }
    gainPts_[(gainHead_ + gainCount_) % countOf(gainPts_)] = {frame, gain};
    ++gainCount_;
}

void Engine::onTick(i64 tick) {
    const i64 inBar = tick % kTicksPerBar;
    if (inBar == 0) planBar(tick);
    if (tick % kPpq == 0) {
        emit(BeatKind::Beat, frameOf(static_cast<double>(tick)), static_cast<double>(tick), static_cast<u8>(inBar / kPpq), 0, 0,
             0.0f, plan_->index);
    }
    switch (mode_) {
        case Mode::Staged: stagedStep(tick, inBar); break;
        case Mode::Conductor: conductorStep(tick, inBar); break;
        case Mode::Metronome:
            if (tick % kPpq == 0) {
                const i64 f = frameOf(static_cast<double>(tick));
                playSample(bank_->sfx(Sfx::UiTick), Bus::Sfx, f, kClickGain, 0.0f);
                emit(BeatKind::Click, f, static_cast<double>(tick), static_cast<u8>(inBar / kPpq), 0, 0, kClickGain, plan_->index);
            }
            break;
        default: break;
    }
    if (tick % 12 == 0) flushKills(tick);
}

void Engine::planBar(i64 barTick) {
    BarPlan& p = *plan_;
    p = BarPlan{};
    p.tick = barTick;
    p.index = bar_;
    // A new sound variant takes hold on the bar line (the user's rule).
    if (pendingVariant_ >= 0) {
        variant_ = static_cast<u32>(pendingVariant_);
        pendingVariant_ = -1;
        applyParams();
        emit(BeatKind::Variant, frameOf(static_cast<double>(barTick)), static_cast<double>(barTick), static_cast<u8>(variant_));
    }
    if (mode_ == Mode::Staged) {
        if (pendingTrack_ >= 0) {
            const u32 t = static_cast<u32>(pendingTrack_);
            pendingTrack_ = -1;
            track_ = t;
            applyParams();
            // The new arrangement starts on this bar line at the new tempo.
            tempo_.changeAt(static_cast<double>(barTick), ticksPerFrameFor(tr_->bpm, sr_));
            nominalTpf_ = tempo_.last().ticksPerFrame;
            const TempoSegment& s = tempo_.last();
            emit(BeatKind::Tempo, frameOf(s.tick), s.tick, 0, 0, 0, tr_->bpm, 0, s.ticksPerFrame);
            emit(BeatKind::Track, frameOf(static_cast<double>(barTick)), static_cast<double>(barTick), static_cast<u8>(track_));
            bar_ = 0;
            p.index = 0;
            chordBar_ = 0;
            stabCount_ = 0;
        }
        planStagedBar(barTick);
    } else if (mode_ == Mode::Conductor) {
        planConductorBar(barTick);
    } else {
        p.silent = true;
        p.chord = &tr_->chordAtBar(chordBar_);
        if (pendingTrack_ >= 0) {
            track_ = static_cast<u32>(pendingTrack_);
            pendingTrack_ = -1;
            applyParams();
        }
    }
    emit(BeatKind::Bar, frameOf(static_cast<double>(barTick)), static_cast<double>(barTick), static_cast<u8>(p.section),
         static_cast<u8>(p.sectionBars), static_cast<u8>(p.inSection), 0.0f, p.index);
    stats_.bar.store(p.index, std::memory_order_relaxed);
    stats_.section.store(static_cast<u8>(p.section), std::memory_order_relaxed);
    stats_.sectionBar.store(static_cast<u8>(p.inSection), std::memory_order_relaxed);
    stats_.sectionBars.store(static_cast<u8>(p.sectionBars), std::memory_order_relaxed);
    ++bar_;
    ++chordBar_;
}

void Engine::planStagedBar(i64 barTick) {
    BarPlan& p = *plan_;
    const Track& t = *tr_;
    u32 g = p.index;
    if (g >= t.arrangementBars) {
        if (!loop_ || !t.arrangementBars) {
            p.silent = true;
            p.chord = &t.chordAtBar(g);
            return;
        }
        g %= t.arrangementBars;
    }
    // Which part of the arrangement, and which bar of it.
    u32 part = 0, first = 0;
    while (part + 1 < t.partCount && first + t.parts[part].bars <= g) first += t.parts[part++].bars;
    const ArrangementPart& a = t.parts[part];
    const u32 b = g - first;
    p.silent = false;
    p.section = a.section;
    p.inSection = b;
    p.sectionBars = a.bars;
    p.chord = &t.chordAtBar(g);
    const bool isBuild = a.section == Section::Build, isBreak = a.section == Section::Break;
    p.lastBuildBar = isBuild && b == a.bars - 1;
    const float buildU = isBuild ? static_cast<float>(b) / static_cast<float>(a.bars > 1 ? a.bars - 1 : 1) : 0.0f;
    for (u32 i = 0; i < kPatCount; ++i) p.pat[i] = a.pat[i] != 0.0f ? 1.0f : 0.0f;
    if (p.lastBuildBar) {
        p.pat[static_cast<u32>(Pat::Kick)] = 0.0f;
        p.pat[static_cast<u32>(Pat::HatOff)] = 0.0f;
        p.pat[static_cast<u32>(Pat::Stab)] = 0.0f;
    }
    p.pat[static_cast<u32>(Pat::HatOff)] *= a.pat[static_cast<u32>(Pat::HatOff)];  // the only layer scaled by its level
    p.hatOffBreak = isBreak;
    if (isBuild) {
        static const i64 kDiv[4] = {96, 48, 24, 12};  // quarter, eighth, sixteenth, thirty-second
        p.rollDiv = kDiv[b < 3 ? b : 3];
        p.rollBar = b;
        p.rollBars = a.bars;
    }
    if (a.bass != BassKey::None && !(isBuild && b >= 2)) {
        p.bass = (a.bass == BassKey::Groove && (g % 2) == 1) ? BassKey::GrooveAlt : a.bass;
        p.bassFade = 1.0f - 0.5f * buildU;
    }
    p.lead = a.lead;
    p.leadStopsHalf = p.lastBuildBar;
    if (a.pad > 0.0f) {
        p.pad = a.pad;
        p.padBright = isBreak ? kBreakBright
                              : (isBuild ? kBuildBrightLo + (kBuildBrightHi - kBuildBrightLo) * buildU
                                         : (a.section == Section::Drop ? kDropBright : 1.0f));
    }
    if (b == 0) {
        // Section gains and the section's one-shots (musicproto: once per arrangement entry).
        const float* sg = sp_->sectionGain[static_cast<u32>(a.section)];
        const double startF = tempo_.frameAt(static_cast<double>(barTick));
        const double endTick = static_cast<double>(barTick + static_cast<i64>(a.bars) * kTicksPerBar);
        const double endF = tempo_.frameAt(endTick);
        addSectionGain(startF + kSectionRampS * sr_, sg[0]);
        addSectionGain(endF - kSectionRampS * sr_, sg[1]);
        const auto pend = [&](i32 id, double tick, float gain, BeatKind kind) {
            if (id < 0 || pendingCount_ >= kPendingMax) return;
            Pending& q = pending_[pendingCount_++];
            q = Pending{};
            q.frame = frameOf(tick);
            q.tick = tick;
            q.sample = id;
            q.bus = Bus::Fx;
            q.gain = gain;
            q.kind = kind;
        };
        const u32 trackIdx = track_;
        if (isBuild) {
            pend(bank_->riser(trackIdx, a.bars), static_cast<double>(barTick), sp_->mix.riser, BeatKind::Riser);
            pend(bank_->swellHalfBar(trackIdx), endTick - kTicksPerBar / 2, sp_->mix.swell, BeatKind::Swell);
        }
        if (a.section == Section::Drop && part > 0 && t.parts[part - 1].section == Section::Build)
            pend(bank_->impact(), static_cast<double>(barTick), sp_->mix.impact, BeatKind::Impact);
        if (isBreak) pend(bank_->swellBar(trackIdx), endTick - kTicksPerBar, sp_->mix.swell * kBreakSwell, BeatKind::Swell);
        emit(BeatKind::Section, frameOf(static_cast<double>(barTick)), static_cast<double>(barTick), static_cast<u8>(a.section),
             static_cast<u8>(a.bars), 0, 0.0f, p.index);
    }
}

bool Engine::advanceCycle() {
    const u32 len[2][4] = {{8, 4, 8, 4}, {16, 8, 16, 8}};
    const auto lengthOf = [&](Section s) { return len[form_ == Form::Short ? 0 : 1][static_cast<u32>(s)]; };
    Section next = section_;
    bool change = false;
    if (forceSection_ >= 0) {
        next = static_cast<Section>(forceSection_);
        forceSection_ = -1;
        change = true;
    } else if (nextSection_ || (!hold_ && sectionBar_ + 1 >= sectionBars_)) {
        next = static_cast<Section>((static_cast<u32>(section_) + 1) % 4);
        change = true;
    }
    nextSection_ = false;
    if (!change) {
        ++sectionBar_;  // a held section keeps counting (derived values are clamped)
        return false;
    }
    buildFollowed_ = section_ == Section::Build && next == Section::Drop;
    section_ = next;
    sectionBar_ = 0;
    sectionBars_ = lengthOf(next);
    return true;
}

void Engine::planConductorBar(i64 barTick) {
    BarPlan& p = *plan_;
    const ConductorRules& r = music_->conductor;
    const Section before = section_;
    const bool first = p.index == 0;
    bool changed = first;
    if (first) {
        section_ = Section::Groove;
        sectionBar_ = 0;
        sectionBars_ = form_ == Form::Short ? 8 : 16;
    } else {
        // A Track change starts with a Break on the next bar line (conductor.json trackChange).
        if (pendingTrack_ >= 0 && !ramping_) forceSection_ = static_cast<i32>(Section::Break);
        changed = advanceCycle();
    }
    const bool sectionStart = changed;
    if (sectionStart && section_ == Section::Break && pendingTrack_ >= 0 && !ramping_) {
        // The new Track's Break: eight bars, the tempo ramps linearly across it, the key changes on bar 5.
        sectionBars_ = 8;
        nextTrackAtBar_ = 4;
        ramping_ = true;
        rampFrom_ = ticksPerFrameFor(tr_->bpm, sr_);
        rampTo_ = ticksPerFrameFor(music_->tracks[pendingTrack_].bpm, sr_);
    }
    if (ramping_ && section_ != Section::Break) ramping_ = false;
    if (ramping_ && static_cast<i32>(sectionBar_) == nextTrackAtBar_ && pendingTrack_ >= 0) {
        track_ = static_cast<u32>(pendingTrack_);
        pendingTrack_ = -1;
        applyParams();
        chordBar_ = 0;
        if (!formSet_) form_ = track_ == 0 ? Form::Short : Form::Standard;
        emit(BeatKind::Track, frameOf(static_cast<double>(barTick)), static_cast<double>(barTick), static_cast<u8>(track_));
    }
    p.silent = false;
    p.section = section_;
    p.inSection = sectionBar_;
    p.sectionBars = sectionBars_;
    p.chord = &tr_->chordAtBar(chordBar_);
    const bool last = !hold_ && sectionBar_ + 1 == sectionBars_;
    const Track& t = *tr_;
    const SoundParams& s = *sp_;

    // Tempo: the Build's last bar runs at 85% (the game slows with it and returns on the downbeat); a
    // Track change ramps bar by bar across its Break; otherwise the Track's tempo.
    {
        const double target = ticksPerFrameFor(t.bpm, sr_);
        const double cur = tempo_.last().ticksPerFrame;
        double want = target;
        if (section_ == Section::Build && last) want = target * r.buildLastBarSpeed;
        if (ramping_) {
            const double u = static_cast<double>(sectionBar_ < sectionBars_ ? sectionBar_ : sectionBars_ - 1) /
                             static_cast<double>(sectionBars_ > 1 ? sectionBars_ - 1 : 1);
            want = rampFrom_ + (rampTo_ - rampFrom_) * u;
        }
        if (std::fabs(want - cur) > 1e-15) {
            tempo_.changeAt(static_cast<double>(barTick), want);
            const TempoSegment& sgm = tempo_.last();
            emit(BeatKind::Tempo, frameOf(sgm.tick), sgm.tick, 0, 0, 0,
                 static_cast<float>(want * 60.0 * sr_ / static_cast<double>(kPpq)), 0, sgm.ticksPerFrame);
        }
        nominalTpf_ = target;
    }
    // Section gain and the section's one-shots.
    const float* sg = s.sectionGain[static_cast<u32>(section_)];
    const double startF = tempo_.frameAt(static_cast<double>(barTick));
    if (sectionStart) {
        if (!first && before != section_) addSectionGain(startF - kSectionRampS * sr_, s.sectionGain[static_cast<u32>(before)][1]);
        addSectionGain(startF + kSectionRampS * sr_, sg[0]);
        if (section_ == Section::Build) {
            // The ramp's end: bars x nominal bar, the last bar stretched by the tempo dip.
            const double barF = static_cast<double>(kTicksPerBar) / nominalTpf_;
            const double endF = startF + barF * (sectionBars_ - 1) + barF / r.buildLastBarSpeed;
            addSectionGain(endF - kSectionRampS * sr_, sg[1]);
        }
        emit(BeatKind::Section, frameOf(static_cast<double>(barTick)), static_cast<double>(barTick), static_cast<u8>(section_),
             static_cast<u8>(sectionBars_), 0, 0.0f, p.index);
    }
    if (last && section_ != Section::Build) {
        const double endF = tempo_.frameAt(static_cast<double>(barTick + kTicksPerBar));
        addSectionGain(endF - kSectionRampS * sr_, sg[1]);
    }
    const auto pend = [&](i32 id, double tick, float gain, BeatKind kind, float rate = 1.0f) {
        if (id < 0 || pendingCount_ >= kPendingMax) return;
        Pending& q = pending_[pendingCount_++];
        q = Pending{};
        q.frame = frameOf(tick);
        q.tick = tick;
        q.sample = id;
        q.bus = Bus::Fx;
        q.gain = gain;
        q.kind = kind;
        q.rate = rate;
    };
    if (sectionStart && section_ == Section::Build) {
        // The riser spans the whole Build: stretched to cover the last bar's dip (and to another Build
        // length when the form was changed from the Track's own).
        const i32 id = bank_->riser(track_, sectionBars_);
        const double baked = id >= 0 ? static_cast<double>(bank_->sample(id).tag) : static_cast<double>(sectionBars_);
        const double actual = (sectionBars_ - 1) + 1.0 / r.buildLastBarSpeed;
        pend(id, static_cast<double>(barTick), s.mix.riser, BeatKind::Riser, static_cast<float>(baked / actual));
    }
    // The swells end exactly on the next downbeat (the Build's last bar is stretched by the dip).
    const auto endingOnDownbeat = [&](i32 id, float gain) {
        if (id < 0) return;
        const double endTick = static_cast<double>(barTick + kTicksPerBar);
        pend(id, endTick, gain, BeatKind::Swell);
        pending_[pendingCount_ - 1].frame = frameOf(endTick) - static_cast<i64>(bank_->sample(id).frames);
        pending_[pendingCount_ - 1].tick = -1.0;  // it ends on the grid, it does not start on it
    };
    if (section_ == Section::Build && last) endingOnDownbeat(bank_->swellHalfBar(track_), s.mix.swell);
    if (sectionStart && section_ == Section::Drop && buildFollowed_)
        pend(bank_->impact(), static_cast<double>(barTick), s.mix.impact, BeatKind::Impact);
    if (section_ == Section::Break && last) endingOnDownbeat(bank_->swellBar(track_), s.mix.swell * kBreakSwell);

    // Layers.
    const u32 kick = static_cast<u32>(Pat::Kick), clap = static_cast<u32>(Pat::Clap), hatOff = static_cast<u32>(Pat::HatOff),
              hat16 = static_cast<u32>(Pat::Hat16), ohat = static_cast<u32>(Pat::Ohat), shaker = static_cast<u32>(Pat::Shaker),
              stab = static_cast<u32>(Pat::Stab), pulse = static_cast<u32>(Pat::Pulse);
    // The bed by intensity (Groove), with hysteresis.
    float bedPat[kPatCount] = {};
    bool bedBass = false;
    float bedPad = 0.0f;
    for (u32 i = 0; i < r.bedCount && i < countOf(bedOn_); ++i) {
        const ConductorRules::BedLayer& L = r.bed[i];
        if (intensity_ >= L.from) bedOn_[i] = true;
        else if (intensity_ < L.from - r.hysteresis) bedOn_[i] = false;
        if (!bedOn_[i]) continue;
        if (L.isBass) {
            bedBass = true;
        } else if (L.isPad) {
            const float u = clamp01((intensity_ - r.padAtLo) / (r.padAtHi - r.padAtLo > 1e-6f ? r.padAtHi - r.padAtLo : 1.0f));
            bedPad = r.padLevelLo + (r.padLevelHi - r.padLevelLo) * u;
        } else {
            bedPat[static_cast<u32>(L.pat)] = 1.0f;
        }
    }
    // Riffs (any player owns them): Orbitals = shaker, Pulse Ring = pulse, Sentinel = stab.
    u8 riffLevel[static_cast<u32>(Riff::Count)] = {};
    bool hyper = false;
    for (u32 i = 0; i < playerCount_; ++i) {
        for (u32 k = 0; k < static_cast<u32>(Riff::Count); ++k)
            if (players_[i].riff[k] > riffLevel[k]) riffLevel[k] = players_[i].riff[k];
        hyper = hyper || players_[i].hyper;
    }
    switch (section_) {
        case Section::Groove:
            for (u32 i = 0; i < kPatCount; ++i) p.pat[i] = bedPat[i];
            if (bedBass) p.bass = (chordBar_ % 2 == 1) ? BassKey::GrooveAlt : BassKey::Groove;
            p.pad = bedPad;
            p.padBright = 1.0f;
            p.fill = r.fillPhraseBars && (sectionBar_ % r.fillPhraseBars) == r.fillPhraseBars - 1;
            // Snapshot for the Build.
            for (u32 i = 0; i < kPatCount; ++i) bedSnapshotPat_[i] = p.pat[i];
            bedSnapshotBass_ = bedBass;
            bedSnapshotPad_ = bedPad;
            break;
        case Section::Build: {
            for (u32 i = 0; i < kPatCount; ++i) p.pat[i] = bedSnapshotPat_[i];
            const float u = clamp01(static_cast<float>(sectionBar_) / static_cast<float>(sectionBars_ > 1 ? sectionBars_ - 1 : 1));
            if (bedSnapshotBass_ && sectionBar_ < sectionBars_ / 2) {
                p.bass = (chordBar_ % 2 == 1) ? BassKey::GrooveAlt : BassKey::Groove;
                p.bassFade = 1.0f - 0.5f * u;
            }
            p.pad = bedSnapshotPad_;
            p.padBright = r.buildPadBrightLo + (r.buildPadBrightHi - r.buildPadBrightLo) * u;
            const u32 part = sectionBar_ * 4 / (sectionBars_ ? sectionBars_ : 1);
            static const i64 kDiv[4] = {96, 48, 24, 12};
            p.rollDiv = kDiv[part < 3 ? part : 3];
            p.rollBar = sectionBar_;
            p.rollBars = sectionBars_;
            p.rollLo = r.snareRollLo;
            p.rollHi = r.snareRollHi;
            p.lastBuildBar = last;
            p.leadStopsHalf = last;
            if (last) p.pat[kick] = p.pat[hatOff] = p.pat[hat16] = p.pat[ohat] = p.pat[stab] = 0.0f;
            break;
        }
        case Section::Drop:
            p.pat[kick] = p.pat[clap] = p.pat[hat16] = p.pat[ohat] = p.pat[shaker] = 1.0f;
            p.bass = BassKey::Drop;
            p.pad = 1.0f;
            p.padBright = r.dropPadBright;
            break;
        case Section::Break:
            p.pat[hatOff] = 0.5f;  // "at half level"
            p.hatOffBreak = true;
            p.pad = r.breakPadLevel;
            p.padBright = r.breakPadBright;
            break;
        default: break;
    }
    if (section_ != Section::Break && !(section_ == Section::Build && last)) {
        if (riffLevel[static_cast<u32>(Riff::Orbitals)]) p.pat[shaker] = 1.0f;
        if (riffLevel[static_cast<u32>(Riff::PulseRing)]) {
            p.pat[pulse] = 1.0f;
            p.pulseEveryBeat = riffLevel[static_cast<u32>(Riff::PulseRing)] >= r.pulseRingEveryBeatFrom;
        }
        if (riffLevel[static_cast<u32>(Riff::Sentinel)]) p.pat[stab] = 1.0f;
        p.riffDouble = hyper;
    }
    // The lead grid of every player: Double-Time stacks and the Surge (first half of the Drop) move it
    // along the grid list (capped at the last); Hyper halves it.
    const bool surge = section_ == Section::Drop && sectionBar_ < sectionBars_ / 2;
    for (u32 i = 0; i < playerCount_; ++i) {
        u32 idx = players_[i].doubleTime + (surge ? 1u : 0u);
        if (idx > 2) idx = 2;
        u32 g = r.gridTicks[idx];
        if (players_[i].hyper && g % 2 == 0) g /= 2;
        p.leadGrid[i] = g;
        p.sparse[i] = section_ == Section::Break;
    }
}

void Engine::stagedStep(i64 tick, i64 inBar) {
    const BarPlan& p = *plan_;
    if (p.silent) return;
    const SoundParams& s = *sp_;
    const Track& t = *tr_;
    const double tk = static_cast<double>(tick);
    const i64 f = frameOf(tk);
    const DrumKit& kit = bank_->kit(track_, variant_);
    if (inBar % kTicksPerStep == 0) {
        const u32 st = static_cast<u32>(inBar / kTicksPerStep);
        const auto vel = [&](Pat pat) { return p.pat[static_cast<u32>(pat)] > 0.0f ? t.pattern[static_cast<u32>(pat)][st] : 0.0f; };
        if (float v = vel(Pat::Kick); v > 0.0f) playKick(s.mix.kick * v, f, tk);
        if (float v = vel(Pat::Clap); v > 0.0f) {
            playSample(bank_->clap(), Bus::Clap, f, s.mix.clap * v, kPanClap);
            emit(BeatKind::Clap, f, tk, 0, 0, 0, v);
        }
        if (float v = vel(Pat::HatOff); v > 0.0f) {
            playSample(kit.hat, p.hatOffBreak ? Bus::HatsBreak : Bus::Drums, f, s.mix.hat * v * p.pat[static_cast<u32>(Pat::HatOff)],
                       kPanHatOff);
            emit(BeatKind::Hat, f, tk, 0, 0, 0, v);
        }
        if (float v = vel(Pat::Hat16); v > 0.0f) {
            playSample(kit.hat, Bus::Drums, f, s.mix.hat * v, st % 2 == 0 ? -kPanHat16 : kPanHat16);
            emit(BeatKind::Hat, f, tk, 0, 0, 0, v);
        }
        if (float v = vel(Pat::Ohat); v > 0.0f) {
            playSample(kit.ohat, Bus::Drums, f, s.mix.ohat * v, kPanOhat);
            emit(BeatKind::Hat, f, tk, 0, 1, 0, v);
        }
        if (float v = vel(Pat::Shaker); v > 0.0f) {
            playSample(bank_->shaker(), Bus::Drums, f, s.mix.shaker * v, st % 2 ? kPanShaker : -kPanShaker);
            emit(BeatKind::Shaker, f, tk, 0, 0, 0, v);
        }
        if (float v = vel(Pat::Pulse); v > 0.0f) {
            playSample(bank_->pulse(), Bus::Pulse, f, s.mix.pulse * v, 0.0f);
            emit(BeatKind::Pulse, f, tk, 0, 0, 0, v);
        }
        if (float v = vel(Pat::Stab); v > 0.0f) {
            playStab(*p.chord, v, (stabCount_ % 2) ? kPanStab : -kPanStab, f);
            ++stabCount_;
            emit(BeatKind::Stab, f, tk, p.chord->stab[0], 0, 0, v);
        }
        if (st == 0 && p.pad > 0.0f) {
            playPad(*p.chord, p.pad, p.padBright, f);
            emit(BeatKind::Pad, f, tk, p.chord->pad[0], 0, 0, p.pad);
        }
        if (p.lead == LeadMode::Normal && !(p.leadStopsHalf && st >= 8)) {
            const float accent = st % 4 == 0 ? kAccentBeat : (st % 2 == 0 ? kAccentEighth : kAccentOther);
            const float pan = kLeadPanDepth * std::sin(st * kLeadPanRate + static_cast<float>(p.index));
            playLead(0, Timbre::P1Saw, p.chord->arp[st], accent, s.mix.lead, pan, f);
            emit(BeatKind::Lead, f, tk, p.chord->arp[st], 0, 0, accent, st, midiHz(p.chord->arp[st]));
        } else if (p.lead == LeadMode::Sparse && (st == 0 || st == 6 || st == 12)) {
            playLead(0, Timbre::P1Saw, p.chord->arp[st], kSparseVel, s.mix.lead, st == 6 ? kSparsePanMid : kSparsePanOther, f);
            emit(BeatKind::Lead, f, tk, p.chord->arp[st], 0, 0, kSparseVel, st, midiHz(p.chord->arp[st]));
        }
    }
    if (p.bass != BassKey::None) {
        const BassPattern& bp = t.bass[static_cast<u32>(p.bass)];
        for (u32 i = 0; i < bp.count; ++i) {
            const BassNote& n = bp.note[i];
            if (static_cast<i64>(std::llround(n.step * kTicksPerStep)) != inBar) continue;
            const float midi = p.chord->bass + n.semis;
            playBass(midi, n.vel * p.bassFade, n.steps, f);
            emit(BeatKind::Bass, f, tk, static_cast<u8>(midi), 0, 0, n.vel * p.bassFade, 0, midiHz(midi));
        }
    }
    if (p.lead == LeadMode::Double && inBar % 12 == 0) {
        const u32 k = static_cast<u32>(inBar / 12);
        const float v = k % 2 == 0 ? kDoubleVelOn : kDoubleVelOff;
        const float pan = kDoublePan * std::sin(k * kDoublePanRate);
        const u8 note = p.chord->arp[k % 16];
        playLead(0, Timbre::P1Saw, note, v, s.mix.lead, pan, f);
        emit(BeatKind::Lead, f, tk, note, 0, 12, v, k, midiHz(note));
    }
    if (p.rollDiv && inBar % p.rollDiv == 0 && !(p.lastBuildBar && inBar >= kTicksPerBar / 2)) {
        const float ts = static_cast<float>(inBar) / kTicksPerStep;
        const float level = p.rollLo + (p.rollHi - p.rollLo) * ((p.rollBar + ts / 16.0f) / static_cast<float>(p.rollBars));
        playSample(bank_->snare(), Bus::Snare, f, s.mix.snare * level, 0.0f);
        emit(BeatKind::Snare, f, tk, 0, 0, 0, level);
    }
}

void Engine::conductorStep(i64 tick, i64 inBar) {
    const BarPlan& p = *plan_;
    if (p.silent) return;
    const SoundParams& s = *sp_;
    const Track& t = *tr_;
    const ConductorRules& r = music_->conductor;
    const double tk = static_cast<double>(tick);
    const i64 f = frameOf(tk);
    const DrumKit& kit = bank_->kit(track_, variant_);
    // Riff patterns at double rate under Hyper: the sixteen steps in half a bar.
    const bool riffStep = p.riffDouble ? inBar % 12 == 0 : inBar % kTicksPerStep == 0;
    const u32 riffSt = static_cast<u32>(p.riffDouble ? (inBar / 12) % 16 : inBar / kTicksPerStep);
    if (inBar % kTicksPerStep == 0) {
        const u32 st = static_cast<u32>(inBar / kTicksPerStep);
        const auto on = [&](Pat pat) { return p.pat[static_cast<u32>(pat)] > 0.0f; };
        if (on(Pat::Kick) && t.pattern[static_cast<u32>(Pat::Kick)][st] > 0.0f)
            playKick(s.mix.kick * t.pattern[static_cast<u32>(Pat::Kick)][st], f, tk);
        if (on(Pat::Clap)) {
            float v = t.pattern[static_cast<u32>(Pat::Clap)][st];
            if (p.fill && st == music_->conductor.fillPickupStep && v <= 0.0f) v = 3.0f / 9.0f;  // the fill's pick-up
            if (v > 0.0f) {
                playSample(bank_->clap(), Bus::Clap, f, s.mix.clap * v, kPanClap);
                emit(BeatKind::Clap, f, tk, 0, 0, 0, v);
            }
        }
        if (on(Pat::HatOff)) {
            float v = t.pattern[static_cast<u32>(Pat::HatOff)][st];
            if (p.fill && st >= 8 && v <= 0.0f) v = t.pattern[static_cast<u32>(Pat::Hat16)][st];  // hats double
            if (v > 0.0f) {
                playSample(kit.hat, p.hatOffBreak ? Bus::HatsBreak : Bus::Drums, f,
                           s.mix.hat * v * p.pat[static_cast<u32>(Pat::HatOff)], kPanHatOff);
                emit(BeatKind::Hat, f, tk, 0, 0, 0, v);
            }
        }
        if (on(Pat::Hat16) && t.pattern[static_cast<u32>(Pat::Hat16)][st] > 0.0f) {
            const float v = t.pattern[static_cast<u32>(Pat::Hat16)][st];
            playSample(kit.hat, Bus::Drums, f, s.mix.hat * v, st % 2 == 0 ? -kPanHat16 : kPanHat16);
            emit(BeatKind::Hat, f, tk, 0, 0, 0, v);
        }
        if (on(Pat::Ohat) && t.pattern[static_cast<u32>(Pat::Ohat)][st] > 0.0f) {
            const float v = t.pattern[static_cast<u32>(Pat::Ohat)][st];
            playSample(kit.ohat, Bus::Drums, f, s.mix.ohat * v, kPanOhat);
            emit(BeatKind::Hat, f, tk, 0, 1, 0, v);
        }
        if (st == 0 && p.pad > 0.0f) {
            playPad(*p.chord, p.pad, p.padBright, f);
            emit(BeatKind::Pad, f, tk, p.chord->pad[0], 0, 0, p.pad);
        }
    }
    if (riffStep) {
        const auto on = [&](Pat pat) { return p.pat[static_cast<u32>(pat)] > 0.0f; };
        if (on(Pat::Shaker) && t.pattern[static_cast<u32>(Pat::Shaker)][riffSt] > 0.0f) {
            const float v = t.pattern[static_cast<u32>(Pat::Shaker)][riffSt];
            playSample(bank_->shaker(), Bus::Drums, f, s.mix.shaker * v, riffSt % 2 ? kPanShaker : -kPanShaker);
            emit(BeatKind::Shaker, f, tk, 0, 0, 0, v);
        }
        if (on(Pat::Pulse)) {
            float v = t.pattern[static_cast<u32>(Pat::Pulse)][riffSt];
            if (p.pulseEveryBeat && riffSt % 4 == 0 && v <= 0.0f) v = 1.0f;
            if (v > 0.0f) {
                playSample(bank_->pulse(), Bus::Pulse, f, s.mix.pulse * v, 0.0f);
                emit(BeatKind::Pulse, f, tk, 0, 0, 0, v);
            }
        }
        if (on(Pat::Stab) && t.pattern[static_cast<u32>(Pat::Stab)][riffSt] > 0.0f) {
            const float v = t.pattern[static_cast<u32>(Pat::Stab)][riffSt];
            playStab(*p.chord, v, (stabCount_ % 2) ? kPanStab : -kPanStab, f);
            ++stabCount_;
            emit(BeatKind::Stab, f, tk, p.chord->stab[0], 0, 0, v);
        }
    }
    if (p.bass != BassKey::None) {
        const BassPattern& bp = t.bass[static_cast<u32>(p.bass)];
        for (u32 i = 0; i < bp.count; ++i) {
            const BassNote& n = bp.note[i];
            if (static_cast<i64>(std::llround(n.step * kTicksPerStep)) != inBar) continue;
            const float midi = p.chord->bass + n.semis;
            playBass(midi, n.vel * p.bassFade, n.steps, f);
            emit(BeatKind::Bass, f, tk, static_cast<u8>(midi), 0, 0, n.vel * p.bassFade, 0, midiHz(midi));
        }
    }
    if (p.rollDiv && inBar % p.rollDiv == 0 && !(p.lastBuildBar && inBar >= kTicksPerBar / 2)) {
        const float ts = static_cast<float>(inBar) / kTicksPerStep;
        const float level = p.rollLo + (p.rollHi - p.rollLo) * ((p.rollBar + ts / 16.0f) / static_cast<float>(p.rollBars));
        playSample(bank_->snare(), Bus::Snare, f, s.mix.snare * level, 0.0f);
        emit(BeatKind::Snare, f, tk, 0, 0, 0, level);
    }
    leadStep(tick, inBar);
    (void)r;
}

void Engine::leadStep(i64 tick, i64 inBar) {
    const BarPlan& p = *plan_;
    const ConductorRules& r = music_->conductor;
    const SoundParams& s = *sp_;
    const Track& t = *tr_;
    const double tk = static_cast<double>(tick);
    for (u32 i = 0; i < playerCount_; ++i) {
        const Player& pl = players_[i];
        const float level = i == localPlayer_ ? 1.0f : (pl.distance > r.othersFarWu ? r.othersFarLevel : r.othersLevel);
        const u32 grid = p.leadGrid[i] ? p.leadGrid[i] : r.gridTicks[0];
        if (pl.fire) {
            if (inBar % grid != 0 || (p.leadStopsHalf && inBar >= kTicksPerBar / 2)) continue;
            const u32 k = static_cast<u32>(inBar / grid);
            const u8 note = p.chord->arp[k % 16];
            const float accent = inBar % kPpq == 0 ? r.accentBeat : (inBar % (kPpq / 2) == 0 ? r.accentEighth : r.accentOther);
            const float pan = r.panDepth * std::sin(k * r.panRate + static_cast<float>(p.index));
            const float scale = std::pow(r.pierceScale, static_cast<float>(pl.pierce));
            const float drive = pl.overcharge ? 1.0f + r.overchargeDrive * pl.overcharge : 0.0f;
            const float gain = s.mix.lead * level * std::pow(10.0f, r.overchargeDb * pl.overcharge / 20.0f);
            const i64 f = frameOf(tk);
            playLead(static_cast<u8>(i), pl.timbre, note, accent, gain, pan, f, scale, drive);
            emit(BeatKind::Lead, f, tk, note, static_cast<u8>(i), static_cast<u8>(grid), accent, k, midiHz(note));
            // Harmony: Split stacks, Prism, On-Beat Crit (conductor.json mods / boosts).
            const auto degreeAbove = [&](u8 base, u32 degrees) -> float {
                const u32 n = t.scaleCount ? t.scaleCount : 7;
                const i32 tonic = t.chords[t.progression[0]].bass % 12;
                const i32 pc = ((base - tonic) % 12 + 12) % 12;
                u32 idx = 0;
                for (u32 d = 0; d < n; ++d)
                    if (t.scale[d] <= pc) idx = d;
                const u32 to = idx + degrees;
                return static_cast<float>(base + (t.scale[to % n] - t.scale[idx]) + 12 * static_cast<i32>(to / n));
            };
            for (u32 st = 0; st < pl.split && st < r.splitDegreeCount; ++st)
                playLead(static_cast<u8>(i), pl.timbre, degreeAbove(note, r.splitDegrees[st]), accent, gain * r.splitLevel, pan, f,
                         scale, drive);
            if (pl.prism) {
                playLead(static_cast<u8>(i), pl.timbre, degreeAbove(note, 4), accent, gain * r.prismLevel, pan, f, scale, drive);
                playLead(static_cast<u8>(i), pl.timbre, note + 12.0f, accent, gain * r.prismLevel, pan, f, scale, drive);
            }
            if (pl.crit && inBar % kPpq == 0)
                playLead(static_cast<u8>(i), pl.timbre, note + 12.0f, accent, gain * r.critLevel, pan, f, scale, drive);
        } else if (p.sparse[i] && inBar % kTicksPerStep == 0) {
            // Break: while a player is not firing, the bed echoes their arpeggio sparsely.
            const u32 st = static_cast<u32>(inBar / kTicksPerStep);
            bool hit = false;
            for (u8 x : r.breakSparseSteps) hit = hit || x == st;
            if (!hit) continue;
            const u8 note = p.chord->arp[st];
            const i64 f = frameOf(tk);
            playLead(static_cast<u8>(i), pl.timbre, note, kSparseVel, s.mix.lead * level, st == 6 ? kSparsePanMid : kSparsePanOther, f);
            emit(BeatKind::Lead, f, tk, note, static_cast<u8>(i), 0, kSparseVel, st, midiHz(note));
        }
    }
}

void Engine::flushKills(i64 tick) {
    // Kills are aggregated (README): one sound per size class per 1/32 step, its gain following the
    // count, pitched to a chord tone (small an octave up, medium at pitch, large an octave down).
    const Chord& ch = plan_->chord ? *plan_->chord : tr_->chordAtBar(0);
    for (u32 size = 0; size < 3; ++size) {
        const u32 n = killCount_[size];
        if (!n) continue;
        killCount_[size] = 0;
        const float agg = 1.0f - std::exp(-static_cast<float>(n) / 2.5f);
        float gain = 0.0f, pan = 0.0f;
        i32 midi = 0;
        Sfx kind = Sfx::KillSmall;
        const u8 tone = ch.stab[sfxRng_.below(ch.stabCount ? ch.stabCount : 1)];
        if (size == 0) {
            kind = Sfx::KillSmall;
            midi = tone + 12;
            gain = sfxLevel(kind) * agg * kKillSmallBoost;
            pan = static_cast<float>((sfxRng_.uniform() * 2.0 - 1.0) * kKillPanSmall);
        } else if (size == 1) {
            kind = Sfx::KillMedium;
            midi = tone;
            gain = sfxLevel(kind) * std::fmin(kKillSmallBoost, agg / (1.0f - std::exp(-1.0f / 2.5f)));
            pan = static_cast<float>((sfxRng_.uniform() * 2.0 - 1.0) * kKillPanMedium);
        } else {
            kind = Sfx::KillLarge;
            midi = ch.stab[0] - 12;
            gain = sfxLevel(kind) * std::fmin(kKillSmallBoost, agg / (1.0f - std::exp(-1.0f / 2.5f)));
        }
        playSfx(kind, midi, gain, pan, frameOf(static_cast<double>(tick)), static_cast<double>(tick));
    }
}

void Engine::firePending(i64 frameEnd) {
    for (u32 i = 0; i < pendingCount_;) {
        Pending& q = pending_[i];
        if (q.frame >= frameEnd) {
            ++i;
            continue;
        }
        const i64 f = q.frame < frame_ ? frame_ : q.frame;
        if (q.sample >= 0) {
            playSample(q.sample, q.bus, f, q.gain, q.pan, q.rate);
            emit(q.kind, f, q.tick, 0, 0, 0, q.gain);
        } else if (q.sfx != Sfx::Count) {
            playSfx(q.sfx, q.midi, q.gain, q.pan, f, q.tick);
        }
        pending_[i] = pending_[--pendingCount_];
    }
}

// ---------------------------------------------------------------- rendering

void Engine::render(float* out, u32 frames, u32 channels) {
    Command c;
    while (commands_.pop(c)) applyCommand(c);
    u32 done = 0;
    while (done < frames) {
        const u32 n = frames - done < kBlock ? frames - done : kBlock;
        const i64 blockEnd = frame_ + n;
        for (;;) {
            const i64 f = frameOf(static_cast<double>(nextTick_));
            if (f >= blockEnd) break;
            onTick(nextTick_);
            nextTick_ += kGrid;
        }
        firePending(blockEnd);
        renderBlock(out + static_cast<size_t>(done) * channels, n, channels);
        frame_ += n;
        done += n;
    }
    writeFrame_.store(frame_, std::memory_order_release);
    updateBpmStat();
    float pk = stats_.masterPeak.load(std::memory_order_relaxed);
    if (peak_ > pk) stats_.masterPeak.store(peak_, std::memory_order_relaxed);
    peak_ = 0.0f;
}

// One lead voice for `cnt` samples from its current age. Control-rate values at fixed ages (knots every
// 32 samples), so the result does not depend on how callbacks split the time. The attack ramp and the
// tail fade only touch the first and last few knots: the rest takes the plain loop.
template <Timbre T>
void Engine::renderLead(LeadVoice& v, float* __restrict L, float* __restrict R, u32 cnt, float sr) {
    constexpr u32 kKnot = 32;
    constexpr float kInvKnot = 1.0f / kKnot;
    const float decMul = v.decMul;
    const float invAtt = v.attackN > 0.0f ? 1.0f / v.attackN : 0.0f;
    const u32 attEnd = static_cast<u32>(v.attackN) + 1;
    const u32 fadeStart = v.length - v.fade;
    const float invFade = v.fade > 1 ? 1.0f / static_cast<float>(v.fade - 1) : 0.0f;
    const float gl = v.gl * v.vel, gr = v.gr * v.vel;
    const auto fcAt = [&v](float a) { return (v.fcEnd + (v.fcStart - v.fcEnd) * std::exp(-a / v.fcTauN)) * v.kt; };
    for (u32 i = 0; i < cnt;) {
        const u32 a = v.age + i, phase = a % kKnot, k0 = a - phase;
        const u32 len = kKnot - phase < cnt - i ? kKnot - phase : cnt - i;
        const bool plain = a >= attEnd && a + len <= fadeStart;  // no attack ramp, no tail fade
        float dec = std::exp(-static_cast<float>(k0) / v.decayN);
        for (u32 j = 0; j < phase; ++j) dec *= decMul;
        float* l = L + i;
        float* r = R + i;
        if constexpr (T == Timbre::P3Bell) {
            // sfxproto.bell(hz, 0.36, 2.0, 2.4): FM, the index decaying with 0.15 s.
            const float modMul = std::exp(-1.0f / v.fcTauN);
            float mod = kP3Index * std::exp(-static_cast<float>(k0) / v.fcTauN);
            for (u32 j = 0; j < phase; ++j) mod *= modMul;
            for (u32 j = 0; j < len; ++j) {
                const float m = std::sin(2.0f * kPiF * v.saw[1].phase) * mod;
                float y = std::sin(2.0f * kPiF * v.saw[0].phase + m) * dec;
                if (!plain) {
                    const u32 aj = a + j;
                    if (aj < attEnd) y *= std::fmin(1.0f, static_cast<float>(aj) * invAtt);
                    if (aj >= fadeStart) y *= 1.0f - static_cast<float>(aj - fadeStart) * invFade;
                }
                v.saw[0].phase += v.saw[0].dt;
                v.saw[0].phase -= std::floor(v.saw[0].phase);
                v.saw[1].phase += v.saw[1].dt;
                v.saw[1].phase -= std::floor(v.saw[1].phase);
                dec *= decMul;
                mod *= modMul;
                l[j] += y * gl;
                r[j] += y * gr;
            }
        } else {
            // Filter coefficients exact at the knots, linear in between (no division per sample). The
            // next knot's coefficients are kept for the following interval.
            const u32 kIndex = k0 / kKnot;
            SvfCoefs c0;
            if (v.knot != ~0u && kIndex == v.knot + 1) {
                c0 = v.cNext;
            } else {
                c0 = svfCoefs(svfG(fcAt(static_cast<float>(k0)), sr), v.k);
            }
            if (kIndex != v.knot) {
                v.cNext = svfCoefs(svfG(fcAt(static_cast<float>(k0 + kKnot)), sr), v.k);
                v.knot = kIndex;
            } else {
                c0 = svfCoefs(svfG(fcAt(static_cast<float>(k0)), sr), v.k);
            }
            const SvfCoefs c1 = v.cNext;
            const float d1 = (c1.a1 - c0.a1) * kInvKnot, d2 = (c1.a2 - c0.a2) * kInvKnot, d3 = (c1.a3 - c0.a3) * kInvKnot;
            const bool driven = v.drive > 0.0f;
            // The hot state in locals: the outputs are float pointers, so the compiler could not otherwise
            // keep oscillator phases and filter state in registers.
            SawOsc s0 = v.saw[0], s1 = v.saw[1], s2 = v.saw[2], s3 = v.saw[3];
            SquareOsc q0 = v.sq[0], q1 = v.sq[1];
            SvfLowpass f = v.svf;
            const float pre = v.pre, sub = v.sub;
            for (u32 j = 0; j < len; ++j) {
                float x;
                if constexpr (T == Timbre::P1Saw) {
                    x = s0.next() + s1.next() + sub * q0.next();
                } else if constexpr (T == Timbre::P2Square) {
                    x = q0.next() + q1.next();
                } else {
                    x = (s0.next() + s1.next() + s2.next()) * (1.0f / 3.0f) + kP4Oct * s3.next();
                }
                const float p = static_cast<float>(phase + j);
                float y = f.tick(x * pre, c0.a1 + d1 * p, c0.a2 + d2 * p, c0.a3 + d3 * p);
                if (driven) y = std::tanh(v.drive * y);
                y *= dec;
                if (!plain) {
                    const u32 aj = a + j;
                    if (aj < attEnd) y *= std::fmin(1.0f, static_cast<float>(aj) * invAtt);
                    if (aj >= fadeStart) y *= 1.0f - static_cast<float>(aj - fadeStart) * invFade;
                }
                dec *= decMul;
                l[j] += y * gl;
                r[j] += y * gr;
            }
            v.saw[0] = s0;
            v.saw[1] = s1;
            v.saw[2] = s2;
            v.saw[3] = s3;
            v.sq[0] = q0;
            v.sq[1] = q1;
            v.svf = f;
        }
        i += len;
    }
}
void Engine::renderBlock(float* out, u32 n, u32 channels) {
    const float sr = static_cast<float>(sr_);
    auto stamp = std::chrono::steady_clock::now();
    const auto mark = [&](u32 stage) {
        if (!profile_) return;
        const auto now = std::chrono::steady_clock::now();
        profile_->ns[stage] += static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - stamp).count());
        stamp = now;
    };
    std::memset(bus_, 0, sizeof bus_);
    std::memset(wet_, 0, sizeof wet_);
    u32 active = 0;

    // Voices. Control-rate values (filter coefficients, envelope anchors) are taken at fixed ages of each
    // voice (every 32 samples since its start) and stepped per sample in between, so the output does
    // not depend on how the device splits time into callbacks (tested: random bursts render identically).
    constexpr u32 kKnot = 32;
    constexpr float kInvKnot = 1.0f / kKnot;
    // Lead voices.
    for (u32 vi = 0; vi < kLeadVoices; ++vi) {
        LeadVoice& v = lead_[vi];
        if (!v.active) continue;
        ++active;
        if (v.delay >= n) {
            v.delay -= n;
            continue;
        }
        const u32 i0 = v.delay;
        v.delay = 0;
        u32 cnt = n - i0;
        if (v.length - v.age < cnt) cnt = v.length - v.age;
        const Bus target = mode_ == Mode::Script ? Bus::Sfx : Bus::Lead;
        float* L = bus_[static_cast<u32>(target)][0] + i0;
        float* R = bus_[static_cast<u32>(target)][1] + i0;
        switch (v.timbre) {
            case Timbre::P1Saw: renderLead<Timbre::P1Saw>(v, L, R, cnt, sr); break;
            case Timbre::P2Square: renderLead<Timbre::P2Square>(v, L, R, cnt, sr); break;
            case Timbre::P3Bell: renderLead<Timbre::P3Bell>(v, L, R, cnt, sr); break;
            default: renderLead<Timbre::P4Stab>(v, L, R, cnt, sr); break;
        }
        v.age += cnt;
        if (v.age >= v.length) v.active = false;
    }    mark(EngineProfile::Lead);
    // Bass.
    for (u32 vi = 0; vi < kBassVoices; ++vi) {
        BassVoice& v = bass_[vi];
        if (!v.active) continue;
        ++active;
        if (v.delay >= n) {
            v.delay -= n;
            continue;
        }
        const u32 i0 = v.delay;
        v.delay = 0;
        u32 cnt = n - i0;
        if (v.length - v.age < cnt) cnt = v.length - v.age;
        float* L = bus_[static_cast<u32>(Bus::Bass)][0] + i0;
        float* R = bus_[static_cast<u32>(Bus::Bass)][1] + i0;
        const float decMul = std::exp(-1.0f / v.decayN), relMul = std::exp(-1.0f / v.releaseN);
        const float invAtt = v.attackN > 0.0f ? 1.0f / v.attackN : 0.0f;
        const u32 attEnd = static_cast<u32>(v.attackN), fadeStart = v.length - v.fade;
        const float invFade = v.fade > 1 ? 1.0f / static_cast<float>(v.fade - 1) : 0.0f;
        const auto coefs = [&](u32 a) {
            return svfCoefs(svfG(v.fcEnd + (v.fcStart - v.fcEnd) * std::exp(-static_cast<float>(a) / v.fcTauN), sr), v.k);
        };
        for (u32 i = 0; i < cnt;) {
            const u32 a = v.age + i, phase = a % kKnot, k0 = a - phase;
            const u32 len = kKnot - phase < cnt - i ? kKnot - phase : cnt - i;
            const SvfCoefs c0 = coefs(k0), c1 = coefs(k0 + kKnot);
            const float d1 = (c1.a1 - c0.a1) * kInvKnot, d2 = (c1.a2 - c0.a2) * kInvKnot, d3 = (c1.a3 - c0.a3) * kInvKnot;
            float dec = std::exp(-static_cast<float>(k0) / v.decayN);
            float rel = k0 > v.gate ? std::exp(-static_cast<float>(k0 - v.gate) / v.releaseN) : 1.0f;
            for (u32 j = 0; j < phase; ++j) {
                dec *= decMul;
                if (k0 + j >= v.gate) rel *= relMul;
            }
            SawOsc saw = v.saw;
            SquareOsc sq = v.sq;
            SvfLowpass f = v.svf;
            float subPhase = v.subPhase;
            for (u32 j = 0; j < len; ++j) {
                const u32 aj = a + j;
                const float x = (1.0f - v.sqMix) * saw.next() + v.sqMix * sq.next();
                float y = f.tick(x, c0.a1 + d1 * static_cast<float>(phase + j), c0.a2 + d2 * static_cast<float>(phase + j), c0.a3 + d3 * static_cast<float>(phase + j));
                y += v.sub * std::sin(2.0f * kPiF * subPhase);
                subPhase += v.subDt;
                subPhase -= std::floor(subPhase);
                const float att = aj < attEnd ? static_cast<float>(aj) * invAtt : 1.0f;
                float o = std::tanh(v.drive * y * att * dec * rel) * v.vel * v.gain;
                if (aj >= fadeStart) o *= 1.0f - static_cast<float>(aj - fadeStart) * invFade;
                dec *= decMul;
                if (aj >= v.gate) rel *= relMul;
                L[i + j] += o;  // musicproto places the bass at pan 0: unity on both channels
                R[i + j] += o;
            }
            v.saw = saw;
            v.sq = sq;
            v.svf = f;
            v.subPhase = subPhase;
            i += len;
        }
        v.age += cnt;
        if (v.age >= v.length) v.active = false;
    }

    mark(EngineProfile::Bass);
    // Stabs.
    for (u32 vi = 0; vi < kStabVoices; ++vi) {
        StabVoice& v = stab_[vi];
        if (!v.active) continue;
        ++active;
        if (v.delay >= n) {
            v.delay -= n;
            continue;
        }
        const u32 i0 = v.delay;
        v.delay = 0;
        u32 cnt = n - i0;
        if (v.length - v.age < cnt) cnt = v.length - v.age;
        float* L = bus_[static_cast<u32>(Bus::Stab)][0] + i0;
        float* R = bus_[static_cast<u32>(Bus::Stab)][1] + i0;
        const float decMul = std::exp(-1.0f / v.decayN);
        const float invAtt = v.attackN > 0.0f ? 1.0f / v.attackN : 0.0f;
        const u32 attEnd = static_cast<u32>(v.attackN), fadeStart = v.length - v.fade;
        const float invFade = v.fade > 1 ? 1.0f / static_cast<float>(v.fade - 1) : 0.0f;
        const auto coefs = [&](u32 a) {
            return svfCoefs(svfG(v.fcEnd + (v.fcStart - v.fcEnd) * std::exp(-static_cast<float>(a) / v.fcTauN), sr), v.k);
        };
        for (u32 i = 0; i < cnt;) {
            const u32 a = v.age + i, phase = a % kKnot, k0 = a - phase;
            const u32 len = kKnot - phase < cnt - i ? kKnot - phase : cnt - i;
            const SvfCoefs c0 = coefs(k0), c1 = coefs(k0 + kKnot);
            const float d1 = (c1.a1 - c0.a1) * kInvKnot, d2 = (c1.a2 - c0.a2) * kInvKnot, d3 = (c1.a3 - c0.a3) * kInvKnot;
            float dec = std::exp(-static_cast<float>(k0) / v.decayN);
            for (u32 j = 0; j < phase; ++j) dec *= decMul;
            SawOsc osc[8];
            const u32 nosc = v.oscCount;
            for (u32 o = 0; o < nosc; ++o) osc[o] = v.saw[o];
            SvfLowpass f = v.svf;
            for (u32 j = 0; j < len; ++j) {
                const u32 aj = a + j;
                float x = 0.0f;
                for (u32 o = 0; o < nosc; ++o) x += osc[o].next();
                float y = f.tick(x * v.norm, c0.a1 + d1 * static_cast<float>(phase + j), c0.a2 + d2 * static_cast<float>(phase + j), c0.a3 + d3 * static_cast<float>(phase + j));
                float s = aj < attEnd ? static_cast<float>(aj) * invAtt : 1.0f;
                if (aj >= fadeStart) s *= 1.0f - static_cast<float>(aj - fadeStart) * invFade;
                y *= s * dec * v.vel;
                dec *= decMul;
                L[i + j] += y * v.gl;
                R[i + j] += y * v.gr;
            }
            for (u32 o = 0; o < nosc; ++o) v.saw[o] = osc[o];
            v.svf = f;
            i += len;
        }
        v.age += cnt;
        if (v.age >= v.length) v.active = false;
    }

    mark(EngineProfile::Stab);
    // Pads.
    for (u32 vi = 0; vi < kPadVoices; ++vi) {
        PadVoice& v = pad_[vi];
        if (!v.active) continue;
        ++active;
        if (v.delay >= n) {
            v.delay -= n;
            continue;
        }
        const u32 i0 = v.delay;
        v.delay = 0;
        u32 cnt = n - i0;
        if (v.length - v.age < cnt) cnt = v.length - v.age;
        const float relMul = std::exp(-1.0f / v.releaseN);
        const float invAtt = v.attackN > 0.0f ? 1.0f / v.attackN : 0.0f;
        const u32 attEnd = static_cast<u32>(v.attackN), fadeStart = v.length - v.fade;
        const float invFade = v.fade > 1 ? 1.0f / static_cast<float>(v.fade - 1) : 0.0f;
        const float gain = v.level * v.gain;
        for (u32 ch = 0; ch < 2; ++ch) {
            float* out = bus_[static_cast<u32>(Bus::Pad)][ch] + i0;
            const auto coefs = [&](u32 a) {
                const float lfo = 0.5f + 0.5f * std::sin(v.lfoW * static_cast<float>(a) + 0.9f * static_cast<float>(ch));
                return svfCoefs(svfG((v.fcBase + v.fcLfo * lfo) * v.bright, sr), v.k);
            };
            SawOsc* osc = v.saw[ch];
            for (u32 i = 0; i < cnt;) {
                const u32 a = v.age + i, phase = a % kKnot, k0 = a - phase;
                const u32 len = kKnot - phase < cnt - i ? kKnot - phase : cnt - i;
                const SvfCoefs c0 = coefs(k0), c1 = coefs(k0 + kKnot);
                const float d1 = (c1.a1 - c0.a1) * kInvKnot, d2 = (c1.a2 - c0.a2) * kInvKnot, d3 = (c1.a3 - c0.a3) * kInvKnot;
                float rel = k0 > v.hold ? std::exp(-static_cast<float>(k0 - v.hold) / v.releaseN) : 1.0f;
                for (u32 j = 0; j < phase; ++j)
                    if (k0 + j >= v.hold) rel *= relMul;
                SawOsc bank[12];
                const u32 nosc = v.oscCount;
                for (u32 o = 0; o < nosc; ++o) bank[o] = osc[o];
                SvfLowpass f = v.svf[ch];
                for (u32 j = 0; j < len; ++j) {
                    const u32 aj = a + j;
                    float x = 0.0f;
                    for (u32 o = 0; o < nosc; ++o) x += bank[o].next();
                    float y = f.tick(x * v.norm, c0.a1 + d1 * static_cast<float>(phase + j), c0.a2 + d2 * static_cast<float>(phase + j), c0.a3 + d3 * static_cast<float>(phase + j));
                    float s = aj < attEnd ? static_cast<float>(aj) * invAtt : 1.0f;
                    if (aj >= fadeStart) s *= 1.0f - static_cast<float>(aj - fadeStart) * invFade;
                    y *= s * rel * gain;
                    if (aj >= v.hold) rel *= relMul;
                    out[i + j] += y;
                }
                for (u32 o = 0; o < nosc; ++o) osc[o] = bank[o];
                v.svf[ch] = f;
                i += len;
            }
        }
        v.age += cnt;
        if (v.age >= v.length) v.active = false;
    }
    mark(EngineProfile::Pad);
    // Samples.
    for (u32 vi = 0; vi < kSampleVoices; ++vi) {
        SampleVoice& v = smp_[vi];
        if (!v.active) continue;
        ++active;
        if (v.delay >= n) {
            v.delay -= n;
            continue;
        }
        const u32 i0 = v.delay;
        v.delay = 0;
        v.age += n - i0;
        const Bus bus = mode_ == Mode::Script ? Bus::Sfx : v.bus;
        float* L = bus_[static_cast<u32>(bus)][0];
        float* R = bus_[static_cast<u32>(bus)][1];
        if (v.rate == 1.0f) {
            u32 pos = static_cast<u32>(v.pos);
            u32 cnt = n - i0;
            if (v.frames - pos < cnt) cnt = v.frames - pos;
            if (v.channels == 2) {
                const float* d = v.data + static_cast<size_t>(pos) * 2;
                for (u32 i = 0; i < cnt; ++i) {
                    L[i0 + i] += d[2 * i] * v.gl;
                    R[i0 + i] += d[2 * i + 1] * v.gr;
                }
            } else {
                const float* d = v.data + pos;
                for (u32 i = 0; i < cnt; ++i) {
                    L[i0 + i] += d[i] * v.gl;
                    R[i0 + i] += d[i] * v.gr;
                }
            }
            pos += cnt;
            v.pos = pos;
            if (pos >= v.frames) v.active = false;
        } else {
            for (u32 i = i0; i < n; ++i) {
                const u32 k = static_cast<u32>(v.pos);
                if (k + 1 >= v.frames) {
                    v.active = false;
                    break;
                }
                const float fr = static_cast<float>(v.pos - k);
                if (v.channels == 2) {
                    L[i] += (v.data[2 * k] + (v.data[2 * k + 2] - v.data[2 * k]) * fr) * v.gl;
                    R[i] += (v.data[2 * k + 1] + (v.data[2 * k + 3] - v.data[2 * k + 1]) * fr) * v.gr;
                } else {
                    const float s = v.data[k] + (v.data[k + 1] - v.data[k]) * fr;
                    L[i] += s * v.gl;
                    R[i] += s * v.gr;
                }
                v.pos += v.rate;
            }
        }
    }
    mark(EngineProfile::Samples);
    stats_.voices.store(active, std::memory_order_relaxed);
    if (active > stats_.voicesPeak.load(std::memory_order_relaxed)) stats_.voicesPeak.store(active, std::memory_order_relaxed);

    if (mode_ == Mode::Script) {
        // Effect sheets: dry, through sfxproto.finish()'s tanh.
        const float* L = bus_[static_cast<u32>(Bus::Sfx)][0];
        const float* R = bus_[static_cast<u32>(Bus::Sfx)][1];
        for (u32 i = 0; i < n; ++i) {
            const float l = std::tanh(L[i]), r = std::tanh(R[i]);
            float* o = out + static_cast<size_t>(i) * channels;
            o[0] = l;
            if (channels > 1) o[1] = r;
            for (u32 c = 2; c < channels; ++c) o[c] = 0.0f;
            peak_ = std::fmax(peak_, std::fmax(std::fabs(l), std::fabs(r)));
        }
        return;
    }

    // Pad high-pass (musicproto: per chord before placing; linear, so on the bus).
    for (u32 ch = 0; ch < 2; ++ch) {
        float* x = bus_[static_cast<u32>(Bus::Pad)][ch];
        for (u32 i = 0; i < n; ++i) x[i] = padHp_[ch].tick(x[i]);
    }
    // Sidechain from the kick: e^(-t / tau) for 0.6 s after each kick, depth per bus.
    {
        float duck[kBlock];
        const double tauN = static_cast<double>(sp_->sidechainTau) * sr_;
        for (u32 i = 0; i < n; ++i) {
            const i64 x = frame_ + i;
            // A kick starting later in this block does not duck the samples before it: they still
            // belong to the previous kick (musicproto takes the maximum of the overlapping segments).
            const i64 age = x - (x >= lastKick_ ? lastKick_ : prevKick_);
            duck[i] = (age >= 0 && age < static_cast<i64>(duckLen_)) ? static_cast<float>(std::exp(-static_cast<double>(age) / tauN)) : 0.0f;
        }
        for (u32 b = 0; b < kBusCount; ++b) {
            const float depth = sp_->duck[b];
            if (depth <= 0.0f) continue;
            for (u32 ch = 0; ch < 2; ++ch) {
                float* x = bus_[b][ch];
                for (u32 i = 0; i < n; ++i) x[i] *= 1.0f - depth * duck[i];
            }
        }
    }
    // Hats in the Break: low-pass.
    for (u32 ch = 0; ch < 2; ++ch) {
        float* x = bus_[static_cast<u32>(Bus::HatsBreak)][ch];
        for (u32 i = 0; i < n; ++i) x[i] = hatsLp_[ch].tick(x[i]);
    }
    mark(EngineProfile::BusFx);
    // Lead ping-pong delay (musicproto.ping_pong: a finite set of taps of the dry mono lead).
    {
        float* L = bus_[static_cast<u32>(Bus::Lead)][0];
        float* R = bus_[static_cast<u32>(Bus::Lead)][1];
        for (u32 i = 0; i < n; ++i) {
            delayLine_[delayPos_ & delayMask_] = 0.5f * (L[i] + R[i]);
            float l = 0.0f, r = 0.0f;
            const float w = delayFade_ ? static_cast<float>(delayFade_) / 256.0f : 0.0f;
            for (u32 k = 1; k <= delayRepeats_; ++k) {
                float s = delayLine_[(delayPos_ - k * delayD_) & delayMask_];
                if (delayFade_) s = s * (1.0f - w) + delayLine_[(delayPos_ - k * delayDOld_) & delayMask_] * w;
                const float g = delayTap_[k - 1];
                if (k % 2) {
                    r += s * g * 1.3f;
                    l += s * g * 0.35f;
                } else {
                    l += s * g * 1.3f;
                    r += s * g * 0.35f;
                }
            }
            if (delayFade_) --delayFade_;
            L[i] += l;
            R[i] += r;
            ++delayPos_;
        }
    }
    mark(EngineProfile::Delay);
    // Sends -> reverb. The effects' send (sfxproto: reverb(sfx x 0.18), mixed after the music) joins the
    // music's sends, divided by the master's present gain (section gain x drive x output gain, the
    // soft clip is near linear at reverb levels), so one reverb serves both at the prototype's level.
    {
        float inL[kBlock] = {}, inR[kBlock] = {};
        for (u32 b = 0; b < kBusCount; ++b) {
            const float amount = sp_->send[b];
            if (amount <= 0.0f || b == static_cast<u32>(Bus::Sfx)) continue;
            for (u32 i = 0; i < n; ++i) {
                inL[i] += bus_[b][0][i] * amount;
                inR[i] += bus_[b][1][i] * amount;
            }
        }
        if (stemFn_) {
            // Stems want the music's own reverb return: the effects' share is left out of the tap.
            const float* ls[kBusCount];
            const float* rs[kBusCount];
            for (u32 b = 0; b < kBusCount; ++b) {
                ls[b] = bus_[b][0];
                rs[b] = bus_[b][1];
            }
            reverb_.process(inL, inR, wet_[0], wet_[1], n);
            stemFn_(stemUser_, ls, rs, wet_[0], wet_[1], n);
        } else {
            float g = 1.0f;
            if (gainCount_) g = gainPts_[gainHead_].gain;
            const float masterGain = g * sp_->masterDrive * outGain_ * musicGain_;
            const float fx = masterGain > 1e-3f ? kSfxReverbSend * sfxGain_ / masterGain : 0.0f;
            const float* sL = bus_[static_cast<u32>(Bus::Sfx)][0];
            const float* sR = bus_[static_cast<u32>(Bus::Sfx)][1];
            for (u32 i = 0; i < n; ++i) {
                inL[i] += sL[i] * fx;
                inR[i] += sR[i] * fx;
            }
            reverb_.process(inL, inR, wet_[0], wet_[1], n);
        }
    }
    mark(EngineProfile::Reverb);    // Master: sum, 30 Hz high-pass, section gain, soft clip, output gain; effects on top; limiter.
    const float drive = sp_->masterDrive;
    const u32 gc = gainCount_;
    for (u32 i = 0; i < n; ++i) {
        float l = wet_[0][i], r = wet_[1][i];
        for (u32 b = 0; b < kBusCount; ++b) {
            if (b == static_cast<u32>(Bus::Sfx)) continue;
            l += bus_[b][0][i];
            r += bus_[b][1][i];
        }
        l = masterHp_[0].tick(l);
        r = masterHp_[1].tick(r);
        // Section gain: piecewise linear between the planned points (np.interp).
        float g = 1.0f;
        if (gc) {
            const double x = static_cast<double>(frame_ + i);
            while (gainCount_ > 1 && gainPts_[(gainHead_ + 1) % countOf(gainPts_)].frame <= x) {
                gainHead_ = (gainHead_ + 1) % countOf(gainPts_);
                --gainCount_;
            }
            const GainPoint& a = gainPts_[gainHead_];
            if (gainCount_ == 1 || x <= a.frame) {
                g = a.gain;
            } else {
                const GainPoint& b = gainPts_[(gainHead_ + 1) % countOf(gainPts_)];
                const double u = (x - a.frame) / (b.frame - a.frame);
                g = static_cast<float>(a.gain + (b.gain - a.gain) * u);
            }
        }
        outGain_ += (outGainTarget_ - outGain_) * 0.002f;
        const float mg = rawOutput_ ? 1.0f : outGain_ * musicGain_;
        l = std::tanh(l * g * drive) * mg;
        r = std::tanh(r * g * drive) * mg;
        // Effect ducks on the music.
        if (dipCount_) {
            float d = 1.0f;
            const i64 x = frame_ + i;
            for (u32 k = 0; k < dipCount_; ++k) {
                const i64 age = x - dips_[k].frame;
                if (age < 0) continue;
                const float t = static_cast<float>(age) / sr;
                if (t > 6.0f * dips_[k].tau) continue;
                d = std::fmin(d, 1.0f - dips_[k].depth * std::exp(-t / dips_[k].tau));
            }
            l *= d;
            r *= d;
        }
        l += bus_[static_cast<u32>(Bus::Sfx)][0][i] * sfxGain_;
        r += bus_[static_cast<u32>(Bus::Sfx)][1][i] * sfxGain_;
        if (!rawOutput_) {
            l = limit(l);
            r = limit(r);
        }
        float* o = out + static_cast<size_t>(i) * channels;
        if (channels == 1) {
            o[0] = 0.5f * (l + r);
        } else {
            o[0] = l;
            o[1] = r;
            for (u32 c = 2; c < channels; ++c) o[c] = 0.0f;
        }
        peak_ = std::fmax(peak_, std::fmax(std::fabs(l), std::fabs(r)));
    }
    mark(EngineProfile::Master);
    // Forget dips that are over.
    for (u32 k = 0; k < dipCount_;) {
        if (static_cast<float>(frame_ + n - dips_[k].frame) / sr > 6.0f * dips_[k].tau) {
            dips_[k] = dips_[--dipCount_];
        } else {
            ++k;
        }
    }
}

}  // namespace rf::audio
