#include "audio/engine.h"

#include "audio/engine_parts.h"

#include <cmath>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <new>

namespace rf::audio {

namespace {

constexpr u32 kLeadVoices = 96;  // M5: 64 before the upgrades' extra notes (Fractal's start a 1/32 later, Syncopation's blips)
constexpr u32 kLeadPerPlayer = 12;  // per player (a 1/32 line and its harmony, or four lines at 1/16)
// M5 (cost): a teammate's lead, mixed at 0.35 or less (conductor.json lead.others), holds at most this many: at 174 BPM
// every player reached 12 in the full band (48 lead voices against 33 at 120 BPM). It was 8; the upgrades' worst case on
// the Moto (four players, every Mod at its cap) took it to its melody alone: the arpeggio note on its grid, four notes
// held (the whole note at 1/16 at 174 BPM, a beat of them at 1/32); its harmony (Split, Prism, On-Beat Crit) and every
// note its other upgrades add are its own device's (leadStep, leadUpgrades).
constexpr u32 kLeadPerOther = 4;
constexpr u32 kLeadFadeOut = 240;   // frames: a replaced note fades instead of clicking
constexpr u32 kGateRamp = 96;       // M5: a gated one-shot's edges (2 ms; Phasing's fifth)
// M5: room for the upgrades (Heavy Step and Colossus double the bass; Lancer, Accent, Bar Line and the Cannon stab; the
// Riffs' one-shots). A larger pool changes nothing where the smaller one never stole (the full band never did).
constexpr u32 kBassVoices = 16;
constexpr u32 kStabVoices = 16;
constexpr u32 kPadVoices = 4;
constexpr u32 kSampleVoices = 128;
constexpr u32 kPendingMax = 64;
constexpr i64 kGrid = 2;  // sequencer scan resolution in ticks (every grid in use is a multiple)
static_assert(kLeadVoices <= 128 && kSampleVoices <= 128 && kBassVoices <= 128 && kStabVoices <= 128 && kPadVoices <= 128,
              "Engine::Order holds 128 voices a pool");

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
constexpr float kBuildBrightLo = 1.0f, kBuildBrightHi = 1.9f, kDropBright = 1.5f, kBreakBright = 0.55f;
// The lead timbres of sfxproto.py (P2-P4 are code there too).
constexpr float kP2Detune = 6.0f, kP2Pre = 0.42f, kP2Q = 1.8f, kP2FcBase = 700.0f, kP2FcSweep = 3100.0f, kP2FcTau = 0.060f,
                kP2Attack = 0.002f, kP2Decay = 0.15f;
constexpr float kP3Len = 0.36f, kP3Ratio = 2.0f, kP3Index = 2.4f, kP3ModTau = 0.15f, kP3Gain = 0.8f, kP3Attack = 0.002f;
constexpr float kP4Len = 0.30f, kP4Detune = 12.0f, kP4Oct = 0.3f, kP4Pre = 0.6f, kP4Q = 1.4f, kP4FcBase = 900.0f,
                kP4FcSweep = 2400.0f, kP4FcTau = 0.050f, kP4Attack = 0.002f, kP4Decay = 0.12f;
constexpr float kLeadKeytrack = 0.5f;  // P2 and P4: (f / 440) ^ 0.5 like the P1 patch
// sfxproto gameplay mock.
constexpr float kKillSmallBoost = 1.6f;  // (the effects' reverb send, kSfxReverbSend: engine_parts.h)
constexpr float kKillPanSmall = 0.6f, kKillPanMedium = 0.5f, kShardPan = 0.3f;
constexpr double kShardRunGapS = 0.5;  // a pause this long starts a new Shard run (engine choice)
constexpr float kMetronomeBpm = 120.0f;  // design/screens.md sync calibration
constexpr float kClickGain = 0.5f;
// Play test 1 / M4 (engine choices): a LeadNote's velocity; the beacon (look.md 6: 12 dB under the leads, an eighth
// tremolo, alternating these velocities); the bed's ramp per block under "music from host only".
constexpr float kLeadNoteVel = 0.9f;
constexpr float kBeaconGain = 0.2512f;  // -12 dB
constexpr float kBeaconVelOn = 0.85f, kBeaconVelOff = 0.5f;
constexpr float kBedRampPerBlock = 0.05f;
// M5 (sound_m5.json fusions.tesla_bloom: "each ring's tom carries a zap on all three stab notes", no level given): Chain
// Storm's zap level (conductor.json boosts.chain_storm), an engine choice.
constexpr float kTeslaZap = 0.35f;
// M5 (conductor.json boosts.beam: "each step retriggers softly", no number given): the velocity under Beam, an engine choice.
constexpr float kBeamVel = 0.6f;
// Output stage: a soft knee that leaves the music bed (peaks near -5 dBFS) untouched and keeps
// music plus effects below -1 dBFS true peak (the ceiling leaves room for inter-sample overshoot).
// M5 audio: the look-ahead limiter (engine.h) keeps everything under 0.8 first; the knee is a safety above it.
constexpr float kLimitKnee = 0.85f, kLimitCeiling = 0.95f;

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
    static const char* const kNames[] = {"lead saw", "lead square", "lead bell", "lead stab", "bass",   "stab",      "pad",
                                         "samples",  "bus fx",      "delay",     "reverb",    "sfx reverb", "master", "content"};
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
    i64 start = 0;  // M5: the frame it starts on, and its allocation number (the order it is summed in)
    u32 seq = 0;
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
    // M5 (cost): the knot interval the last segment rendered in, with its coefficients and where its envelopes stood, so a
    // segment that goes on in the same interval (the next block) takes them up instead of computing them again
    // (bit for bit the same values: each is the interval's exact start times the same per-sample steps).
    SvfCoefs cCur{};
    u32 runKnot = ~0u;
    float decRun = 0.0f, modRun = 0.0f;
    // M5 (Seeker, Wave, Helix Array): the pitch moves at the control rate (once a knot, from the voice's age): a glide from
    // `glideSemis` to the note over `glideN` frames, and a vibrato of `vibSemis` from `vibPhase` (turns) by `vibStep` a frame.
    bool bend = false;
    float glideSemis = 0.0f, vibSemis = 0.0f, vibPhase = 0.0f, vibStep = 0.0f;
    u32 glideN = 0;
    u32 bendKnot = ~0u;   // the knot interval bendRatio was computed for
    float bendRatio = 1.0f;
    bool culled = false;  // cut short by the player's cap (playLead)
    SawOsc saw[4];
    SquareOsc sq[2];
    SvfLowpass svf;
};

struct Engine::BassVoice {
    bool active = false;
    Bus bus = Bus::Bass;  // M5: the rubber string plays on the effects bus
    bool fastDrive = false;  // M5 (cost): the upgrades' bass notes (Heavy Step, Colossus, the rubber string) saturate by fastTanh
    i64 start = 0;
    u32 seq = 0;
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
    i64 start = 0;
    u32 seq = 0;
    u32 delay = 0, age = 0, length = 0, fade = 240;
    u32 oscCount = 0;
    float vel = 0.0f, gl = 0.0f, gr = 0.0f, k = 1.0f, fcStart = 0.0f, fcEnd = 0.0f, fcTauN = 1.0f, attackN = 1.0f,
          decayN = 1.0f, norm = 1.0f;
    SawOsc saw[10];  // M5: up to five notes (the Blink Star's stab)
    SvfLowpass svf;
};

struct Engine::PadVoice {
    bool active = false;
    i64 start = 0;
    u32 seq = 0;
    u32 delay = 0, age = 0, length = 0, hold = 0, fade = 480;
    u32 oscCount = 0;  // per channel
    float level = 0.0f, gain = 0.0f, bright = 1.0f, k = 1.0f, fcBase = 0.0f, fcLfo = 0.0f, lfoW = 0.0f, attackN = 1.0f,
          releaseN = 1.0f, norm = 1.0f;
    float open = 1.0f;  // M5 (Crescendo): the local player's rise opens the pad (1: as the bar asked)
    SawOsc saw[2][12];
    SvfLowpass svf[2];
    // M5 (cost), per channel: the knot interval's coefficients and the next knot's, and where the release stood, taken up
    // by a segment that goes on in the same interval (as the lead's).
    u32 knot[2] = {~0u, ~0u};
    SvfCoefs cCur[2] = {}, cNext[2] = {};
    float relRun[2] = {};
};

struct Engine::SampleVoice {
    bool active = false;
    i64 start = 0;
    u32 seq = 0;
    const float* data = nullptr;
    u32 frames = 0;
    u8 channels = 1;
    Bus bus = Bus::Drums;
    u32 delay = 0;
    u32 age = 0;  // frames played (voice stealing)
    double pos = 0.0;
    float rate = 1.0f;
    float gl = 0.0f, gr = 0.0f;
    float drive = 0.0f;  // M5 (the driven kick): > 0: tanh(drive x) / tanh(drive), a mono voice at its own rate
    // M5 (Phasing's fifth, gated on the eighths): > 0: sounding for gateN frames from gateOrigin, silent for as many, and so on
    // (each edge ramped over kGateRamp frames).
    u32 gateN = 0;
    i64 gateOrigin = 0;
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
    i8 lead = -1;      // play test 1 (LeadNote): a lead note of this player's ...
    Timbre timbre = Timbre::P1Saw;
    float note = 0.0f; // ... at this pitch (MIDI)
};

// Player and BarPlan: audio/engine_parts.h (M5: shared with content_sound.cpp).

namespace {
// The order voices start in (and are summed in): start frame, then allocation number.
template <typename V>
bool startsBefore(const V& a, const V& b) {
    return a.start < b.start || (a.start == b.start && a.seq < b.seq);
}
}  // namespace

template <typename V>
V* Engine::allocVoice(V* pool, u32 count, Order& order) {
    for (u32 i = 0; i < count; ++i)
        if (!pool[i].active) return &pool[i];
    // Full: the voice that started first gives way (by start frame, not by age, which depends on the callbacks).
    V* oldest = &pool[0];
    for (u32 i = 1; i < count; ++i)
        if (startsBefore(pool[i], *oldest)) oldest = &pool[i];
    stats_.steals.fetch_add(1, std::memory_order_relaxed);
    const u8 slot = static_cast<u8>(oldest - pool);
    u32 w = 0;
    for (u32 k = 0; k < order.n; ++k)
        if (order.idx[k] != slot) order.idx[w++] = order.idx[k];
    order.n = w;
    return oldest;
}

template <typename V>
void Engine::placeVoice(V* pool, Order& order, V* v, i64 frame) {
    if (!v->active || order.n >= countOf(order.idx)) return;
    v->start = frame > frame_ ? frame : frame_;
    v->seq = ++voiceSeq_;
    const u8 slot = static_cast<u8>(v - pool);
    u32 p = order.n++;
    while (p > 0 && startsBefore(*v, pool[order.idx[p - 1]])) {
        order.idx[p] = order.idx[p - 1];
        --p;
    }
    order.idx[p] = slot;
}

template <typename V>
void Engine::compactOrder(Order& order, const V* pool) {
    u32 w = 0;
    for (u32 k = 0; k < order.n; ++k)
        if (pool[order.idx[k]].active) order.idx[w++] = order.idx[k];
    order.n = w;
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
    freeContent();
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
    if (!initContent()) return false;  // M5
    for (float& g : limReq_) g = 1.0f;  // M5 audio: the output limiter, open
    limRelease_ = 1.0f - std::exp(-static_cast<float>(kLimSeg) / (kLimReleaseS * static_cast<float>(sampleRate)));
    duckLen_ = framesFor(kDuckS, sampleRate);
    variant_ = music.defaultVariant;
    track_ = 0;
    hatsLp_[0].setLowpass(kHatsBreakLowpass, sampleRate);
    hatsLp_[1] = hatsLp_[0];
    masterHp_[0].setHighpass(kMasterHighpass, sampleRate);
    masterHp_[1] = masterHp_[0];
    freezeHp_[0].setHighpass(music.conductor.freezeHz, sampleRate);  // M5: Freeze Frame
    freezeHp_[1] = freezeHp_[0];
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
    applyDelay();
    outGainTarget_ = std::pow(10.0f, music_->outputGainDb(track_, variant_) / 20.0f);
    // M5: the key the A-minor effects move to: the Track's root (the first chord's bass) within -6..+5 semitones of A,
    // major where the scale has no minor third.
    {
        const i32 root = tr_->progressionCount ? tr_->chords[tr_->progression[0]].bass % 12 : 9;
        i32 shift = ((root - 9) % 12 + 12) % 12;
        if (shift > 5) shift -= 12;
        keyRate_ = static_cast<float>(std::exp2(shift / 12.0));
        bool minorThird = tr_->scaleCount == 0;
        for (u32 i = 0; i < tr_->scaleCount; ++i) minorThird = minorThird || tr_->scale[i] == 3;
        keyMajor_ = !minorThird;
        bool second = tr_->scaleCount == 0;  // the ninth (the major second) in the scale?
        for (u32 i = 0; i < tr_->scaleCount; ++i) second = second || tr_->scale[i] == 2;
        keyNoNinth_ = !second;
    }
    stats_.track.store(static_cast<u8>(track_), std::memory_order_relaxed);
    stats_.variant.store(static_cast<u8>(variant_), std::memory_order_relaxed);
}

void Engine::applyDelay() {
    // Ping-pong delay (musicproto.ping_pong): taps at k x d, gain mix x feedback^(k-1). M5 (conductor.json mods.echo): with
    // Echo the delay is one beat long and its mix rises by echoMix a stack.
    const double stepS = 60.0 / tr_->bpm / 4.0;
    const float steps = echo_ ? 4.0f : sp_->delay.steps;
    const float mix = echo_ ? sp_->delay.mix + music_->conductor.echoMix * static_cast<float>(echo_) : sp_->delay.mix;
    const u32 d = framesFor(steps * stepS, sr_);
    if (d != delayD_) {
        delayDOld_ = delayD_;
        delayD_ = d;
        delayFade_ = delayDOld_ ? 256 : 0;
    }
    delayRepeats_ = static_cast<u32>(sp_->delay.repeats + 0.5f);
    // M5 (DelayBoost: Feedback's Howl turns the delay's own feedback up, the echoes smearing into each other; under 0.8 and
    // band-limited by the taps' count, so never a squeal).
    float feedback = sp_->delay.feedback;
    if (delayBoost_ > 0.0f) {
        feedback = std::fmin(feedback + delayBoost_, 0.8f);
        if (delayBoostRepeats_ > delayRepeats_) delayRepeats_ = delayBoostRepeats_;
    }
    if (delayRepeats_ > countOf(delayTap_)) delayRepeats_ = countOf(delayTap_);
    while (delayRepeats_ && static_cast<u64>(delayRepeats_) * delayD_ > delayMask_) --delayRepeats_;
    for (u32 k = 1; k <= delayRepeats_; ++k) delayTap_[k - 1] = mix * std::pow(feedback, static_cast<float>(k - 1));
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
    bpmOverride_ = 0.0;
    phraseSection_ = -1;
    boss_ = BossBar{};
    bossPending_ = false;
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
    gainPredictedF_ = -1.0;
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
            if (c.a < music_->trackCount) {
                pendingTrack_ = c.a;
                trackNow_ = c.b == 1;
                pendingBpm_ = c.x > 1.0f ? c.x : 0.0;
            }
            break;
        case CmdType::SetPhrase:
            if (c.a < static_cast<u8>(Section::Count)) {
                phraseSection_ = c.a;
                phraseBar_ = c.b;
                phraseBars_ = c.c ? c.c : 1u;
            }
            break;
        case CmdType::SetRate:
            rateScale_ = c.x < 0.99f ? 0.99 : (c.x > 1.01f ? 1.01 : static_cast<double>(c.x));
            break;
        case CmdType::HoldBar:
            holdFrames_ = c.x > 0.0f ? static_cast<double>(c.x < 4.0f ? c.x : 4.0f) * sr_ : 0.0;
            break;
        case CmdType::SetBoss:
            bossNext_.mask = static_cast<u8>(c.a & 7u);
            bossNext_.sixteenths = (c.a & 16u) != 0;
            bossNext_.beats = static_cast<u8>(c.b & 15u);
            bossNext_.flags = c.c;
            bossPending_ = true;
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
            if (p) p->fire = c.b > 2 ? 1 : c.b;
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
                p->mod[static_cast<u32>(Mod::Split)] = c.mods[0];
                p->mod[static_cast<u32>(Mod::DoubleTime)] = c.mods[1];
                p->mod[static_cast<u32>(Mod::Pierce)] = c.mods[2];
                p->mod[static_cast<u32>(Mod::Overcharge)] = c.mods[3];
                p->mod[static_cast<u32>(Mod::OnBeatCrit)] = c.mods[4];
            }
            break;
        case CmdType::SetBoost:
            if (p && c.b < kBoostCount) {
                if (c.b == static_cast<u8>(Boost::Prism)) p->prism = c.c != 0;
                if (c.b == static_cast<u8>(Boost::Hyper)) p->hyper = c.c != 0;
                const bool was = (p->boosts >> c.b) & 1u;
                p->boosts = static_cast<u16>(c.c ? (p->boosts | (1u << c.b)) : (p->boosts & ~(1u << c.b)));
                if (was != (c.c != 0)) boostChanged(c.a, static_cast<Boost>(c.b), c.c != 0);  // M5
            }
            break;
        case CmdType::SetModStacks:
            if (p) {
                for (u32 k = 0; k < c.c && k < 8 && c.b + k < kModCount; ++k) p->mod[c.b + k] = c.mods[k];
                p->split = p->mod[static_cast<u32>(Mod::Split)];
                p->doubleTime = p->mod[static_cast<u32>(Mod::DoubleTime)];
                p->pierce = p->mod[static_cast<u32>(Mod::Pierce)];
                p->overcharge = p->mod[static_cast<u32>(Mod::Overcharge)];
                p->crit = p->mod[static_cast<u32>(Mod::OnBeatCrit)];
            }
            break;
        case CmdType::SetFusions:
            if (p) p->fusions = c.b;
            break;
        case CmdType::Cue:
        case CmdType::Recipe:
        case CmdType::VoiceStart:
        case CmdType::VoiceSet:
        case CmdType::VoiceStop:
        case CmdType::LowPassBed:
        case CmdType::CutBed:
        case CmdType::HighPassBed:
        case CmdType::PadTone:
        case CmdType::DelayBoost:
        case CmdType::LeadEcho:
            applyContentCommand(c);
            break;
        case CmdType::SetDistance:
            if (p) p->distance = c.x;
            break;
        case CmdType::Sfx: {
            const Sfx kind = static_cast<Sfx>(c.a);
            const i32 midi = c.b ? c.b : -1;  // pitched effects: the note (Nova inhale: its length in 1/32 steps)
            if (c.when == Timing::Now) {
                playSfx(kind, midi, sfxLevel(kind) * c.x, c.y, frame_, -1.0, c.level > 0.0f ? c.level : 1.0f);
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
                q.rate = c.level > 0.0f ? c.level : 1.0f;
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
            // M5 (conductor.json boosts.magnet_storm): the local player's Magnet Storm makes the run climb twice as fast.
            shardRun_ += playerBoost(localPlayer_, Boost::MagnetStorm) ? 2u : 1u;
            break;
        }
        case CmdType::Nova: {
            // GAME_DESIGN R6: the inhale from the press to the next beat, the blast on that beat.
            const double now = tempo_.tickAt(static_cast<double>(frame_));
            double beat = std::ceil(now / kPpq) * kPpq;
            if (beat - now < 6.0) beat += kPpq;  // too close: the following beat
            // M3: the game names the beat (the same rule from heard time plus the output latency, so the
            // blast it draws and the one heard coincide); used while it is still six ticks ahead.
            if (c.when == Timing::AtTick && c.tick - now >= 6.0) beat = c.tick;
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
        case CmdType::LeadNote: {
            // Play test 1 (the Trio's chime and its hold) and M4 (the revive's fill): a note of a player's lead on the
            // Track's root, now or on a tick.
            const u8 slot = c.a < kMaxPlayers ? c.a : 0;
            const Timbre tb = c.c < static_cast<u8>(Timbre::Count) ? static_cast<Timbre>(c.c) : players_[slot].timbre;
            const float note = leadNoteMidi(c.b);
            const float gain = sp_->mix.lead * c.x;
            if (c.when == Timing::AtTick && pendingCount_ < kPendingMax) {
                Pending& q = pending_[pendingCount_++];
                q = Pending{};
                q.frame = frameOf(c.tick);
                q.tick = c.tick;
                q.lead = static_cast<i8>(slot);
                q.timbre = tb;
                q.note = note;
                q.gain = gain;
                q.pan = c.y;
            } else {
                playLead(slot, tb, note, kLeadNoteVel, gain, c.y, frame_);
            }
            break;
        }
        case CmdType::SetBeacon:
            if (p) {
                p->beacon = c.b != 0;
                p->beaconPan = c.y;
            }
            break;
        case CmdType::SetHostOnly: hostOnly_ = c.a != 0; break;
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
        case CmdType::SetSurge: surgeSteps_[0] = surgeSteps_[1] = c.a; break;
        case CmdType::SetMuffle:
            muffleTarget_ = c.x > 0.0f ? 1.0f : 0.0f;
            if (c.x > 0.0f) muffleHz_ = c.x;
            muffleStep_ = c.y > 0.0f ? 1.0f / (c.y * static_cast<float>(sr_)) : 1.0f;
            break;
        case CmdType::Reset:
            for (u32 i = 0; i < kLeadVoices; ++i) lead_[i].active = false;
            for (u32 i = 0; i < kBassVoices; ++i) bass_[i].active = false;
            for (u32 i = 0; i < kStabVoices; ++i) stab_[i].active = false;
            for (u32 i = 0; i < kPadVoices; ++i) pad_[i].active = false;
            for (u32 i = 0; i < kSampleVoices; ++i) smp_[i].active = false;
            for (Order& o : order_) o.n = 0;
            stopContent();
            mineArmed_ = singArmed_ = false;
            stabAcc_ = 0.0f;
            freezeMix_ = freezeTarget_ = 0.0f;
            // M5 audio: the limiter open and empty; the fights' bus effects all lifted.
            std::memset(limBuf_, 0, sizeof limBuf_);
            for (float& g : limReq_) g = 1.0f;
            limPeak_ = 0.0f;
            limPrev_ = limCur_ = 1.0f;
            cutMask_ = 0;
            cutHats_ = false;
            cutTo_ = -1;
            for (float& g : cutGain_) g = 1.0f;
            bedHpTo_ = -1;
            bedHpMix_ = 0.0f;
            padTone_ = padToneTarget_ = padGain_ = padGainTarget_ = 1.0f;
            if (delayBoost_ > 0.0f) {
                delayBoost_ = 0.0f;
                delayBoostRepeats_ = 0;
                if (tr_) applyDelay();
            }
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

void Engine::driveSample(SampleVoice* v, float drive) {
    if (v && drive > 0.0f) v->drive = drive;
}

void Engine::gateSample(SampleVoice* v, u32 gateN, i64 origin) {
    if (!v || v->channels != 1 || v->drive > 0.0f) return;  // (the gated path is mono and undriven)
    v->gateN = gateN;
    v->gateOrigin = origin;
}

void Engine::shapeLead(LeadVoice* v, float bend, float cutoff, bool reverse) {
    if (!v || !v->active) return;
    if (bend != 0.0f) {  // it plays the note it lands on and glides in from `bend` semitones off
        v->glideSemis = bend;
        v->glideN = v->length;
        v->bend = true;
    }
    if (cutoff > 0.0f && v->timbre != Timbre::P3Bell) {  // held at the cutoff, without the patch's sweep or key tracking
        v->fcStart = v->fcEnd = cutoff;
        v->kt = 1.0f;
    }
    if (reverse) {
        // The pluck backwards: rising over its length to an edge, no decay, its filter held where it would settle.
        v->attackN = static_cast<float>(v->length > v->fade ? v->length - v->fade : v->length);
        v->decayN = 1e9f;
        v->decMul = 1.0f;
        if (v->timbre != Timbre::P3Bell) v->fcStart = v->fcEnd;
    }
}

Engine::SampleVoice* Engine::playSample(i32 id, Bus bus, i64 frame, float gain, float pan, float rate, double from) {
    if (id < 0) return nullptr;
    const Sample& s = bank_->sample(id);
    SampleVoice* v = allocVoice(smp_, kSampleVoices, order_[PoolSample]);
    v->active = true;
    v->data = s.data;
    v->frames = s.frames;
    v->channels = s.channels;
    v->bus = bus;
    v->delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
    v->age = 0;
    v->pos = from;  // M5: a start inside the sample (the reversed cymbal ends on its tick)
    v->rate = rate;
    v->drive = 0.0f;
    v->gateN = 0;
    if (s.channels == 2) {
        v->gl = v->gr = gain;
    } else {
        panGains(pan, gain, v->gl, v->gr);
    }
    placeVoice(smp_, order_[PoolSample], v, frame);
    return v;
}

void Engine::playKick(float gain, float velocity, i64 frame, double tick) {
    // Every kick the pattern really has, and only those, sets the sidechain (musicproto: kick_times). M5: a kick is
    // not a beat (Afterglow, Mirage and Last Light have none on some beats); the beat is the clock's (onTick).
    playSample(bank_->kit(track_, variant_).kick, Bus::Drums, frame, gain, 0.0f);
    if (frame > lastKick_) {
        prevKick_ = lastKick_;
        lastKick_ = frame;
    }
    emit(BeatKind::Kick, frame, tick, 0, 0, 0, gain, 0, velocity);
    if (kickPulse_) kickUpgrades(velocity, frame, tick);  // M5: Kick Pulse rings with the kick, not the beat
}

void Engine::playSfx(Sfx kind, i32 midi, float gain, float pan, i64 frame, double tick, float rateMul) {
    i32 id = -1;
    float rate = rateMul;  // M3 stage C: a playback rate on top (the boss's growl an octave down)
    if (kind == Sfx::NovaInhale) {
        id = bank_->inhale(track_, midi > 0 ? static_cast<u32>(midi) : 8u);
        midi = -1;
    } else {
        id = bank_->sfx(kind, midi);
        // Pitched effects are baked on the Tracks' chord tones; any other note is the nearest one played
        // faster or slower (at most a fifth away).
        const i32 baked = id >= 0 ? bank_->sample(id).midi : -1;
        if (midi >= 0 && baked >= 0 && baked != midi && std::abs(midi - baked) <= 7)
            rate *= static_cast<float>(std::exp2((midi - baked) / 12.0));
        // M5: the effects written in A minor (sfxproto) follow the Track's key: moved to its root (by a playback rate,
        // at most six semitones), with the major-third variant on the major and lydian Tracks.
        if (midi < 0 && (kind == Sfx::Boost || kind == Sfx::Upgrade || kind == Sfx::GameOver || kind == Sfx::Elite ||
                         kind == Sfx::PerfectDash || kind == Sfx::Shield)) {
            if (keyMajor_ && bank_->sfxMajor(kind) >= 0) id = bank_->sfxMajor(kind);
            if (kind == Sfx::Upgrade && keyNoNinth_ && bank_->upgradeNoNinth() >= 0) id = bank_->upgradeNoNinth();
            rate *= keyRate_;
        }
    }
    if (id < 0) return;
    playSample(id, Bus::Sfx, frame, gain, pan, rate);
    // x: a pitched effect's frequency, else its playback rate (M5: the key's); b = 1: the major-third variant.
    emit(BeatKind::Sfx, frame, tick, static_cast<u8>(kind), static_cast<u8>(id == bank_->sfxMajor(kind) ? 1 : 0), 0, gain, 0,
         midi >= 0 ? midiHz(midi) : static_cast<double>(rate));
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

Engine::LeadVoice* Engine::playLead(u8 player, Timbre timbre, float midi, float vel, float gain, float pan, i64 frame,
                                    float lengthScale, float drive, i64 cullAt) {
    // Each player's lead holds at most kLeadPerPlayer notes (the prototype's notes overlap: about three
    // at 1/16, six at 1/32). Past that, the player's oldest note fades out over a few milliseconds, from the new
    // note's frame (M5: decided at that frame, not at the block being planned, so the callbacks cannot change it;
    // or from `cullAt` for a note that starts later).
    const i64 start = frame > frame_ ? frame : frame_;
    const i64 at = cullAt >= 0 && cullAt < start ? (cullAt > frame_ ? cullAt : frame_) : start;
    u32 mine = 0;
    LeadVoice* oldest = nullptr;
    for (u32 i = 0; i < kLeadVoices; ++i) {
        LeadVoice& o = lead_[i];
        // M5: a note already cut short for another (`culled`) is not counted again, though it may still sound when that
        // other note starts later (Fractal's and Fork's start a 1/32 on): without this the cap let a player's notes pile
        // up (the same old note cut again for each new one). Without notes that start later it changes nothing.
        if (!o.active || o.player != player || o.culled || static_cast<i64>(o.length) <= at - o.start + kLeadFadeOut) continue;
        ++mine;
        if (!oldest || startsBefore(o, *oldest)) oldest = &o;
    }
    // M5: a fight's own lead notes (LeadEcho: Feedback's echoes, the Mimic's answers, Event Horizon's count) hold at most
    // kFightLeadVoices among them.
    const u32 cap = player == kFightLeadPlayer ? kFightLeadVoices
                                               : ((player < kMaxPlayers && player != localPlayer_) ? kLeadPerOther : kLeadPerPlayer);
    if (mine >= cap && oldest && at - oldest->start >= static_cast<i64>(oldest->age)) {
        oldest->length = static_cast<u32>(at - oldest->start) + kLeadFadeOut;
        oldest->fade = kLeadFadeOut;
        oldest->culled = true;
    }
    LeadVoice* v = allocVoice(lead_, kLeadVoices, order_[PoolLead]);
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
            v->sq[1].phase = 0.5f;  // M5: the sub square's second edge, half a cycle on (LeadRun)
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
            v->saw[0].phase = v->saw[1].phase = 0.5f;  // M5: both squares' second edges, half a cycle on (LeadRun)
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
    placeVoice(lead_, order_[PoolLead], v, frame);
    return v;
}

void Engine::fastBass(BassVoice* v) {
    if (v) v->fastDrive = true;
}

Engine::BassVoice* Engine::playBass(float midi, float vel, float steps, i64 frame, float decayScale, float lengthS, Bus bus) {
    BassVoice* v = allocVoice(bass_, kBassVoices, order_[PoolBass]);
    *v = BassVoice{};
    const BassPatch& p = sp_->bass;
    const float sr = static_cast<float>(sr_);
    const double hz = midiHz(midi);
    const double stepS = 60.0 / tr_->bpm / 4.0;
    v->active = true;
    v->bus = bus;
    v->delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
    v->length = lengthS > 0.0f ? framesFor(lengthS, sr_) : framesFor(tr_->longestBassSteps * stepS + kBassTailS, sr_);
    v->gate = static_cast<u32>(steps * stepS * sr);  // t < gate (prototype compares times)
    v->fade = 240;
    v->vel = vel;
    v->gain = sp_->mix.bass;
    v->k = 1.0f / p.q;
    v->fcStart = p.fcStart;
    v->fcEnd = p.fcEnd;
    v->fcTauN = p.fcTau * sr;
    v->attackN = p.attack * sr;
    v->decayN = decayScale != 1.0f ? p.decay * decayScale * sr : p.decay * sr;
    v->releaseN = p.release * sr;
    v->sqMix = p.squareMix;
    v->sub = p.sub;
    v->drive = p.drive;
    v->saw.dt = static_cast<float>(hz / sr_);
    v->sq.dt = static_cast<float>(hz / sr_);
    v->subDt = static_cast<float>(hz * 0.5 / sr_);
    placeVoice(bass_, order_[PoolBass], v, frame);
    return v;
}

void Engine::playStab(const Chord& chord, float vel, float pan, i64 frame, float lengthScale) {
    playStabNotes(chord.stab, chord.stabCount < 4 ? chord.stabCount : 4, vel, pan, frame, lengthScale);
}

void Engine::playStabNotes(const u8* stab, u32 count, float vel, float pan, i64 frame, float lengthScale) {
    StabVoice* v = allocVoice(stab_, kStabVoices, order_[PoolStab]);
    *v = StabVoice{};
    const StabPatch& p = sp_->stab;
    const float sr = static_cast<float>(sr_);
    const double det = std::pow(2.0, p.detuneCents / 1200.0);
    v->active = true;
    v->delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
    // M5 (Lancer's chord: conductor.json riffs.lancer.length): length and decay scaled.
    v->length = lengthScale != 1.0f ? framesFor(p.length * lengthScale, sr_) : framesFor(p.length, sr_);
    v->fade = 240;
    v->vel = vel;
    panGains(pan, sp_->mix.stab, v->gl, v->gr);
    v->k = 1.0f / p.q;
    v->fcStart = p.fcStart;
    v->fcEnd = p.fcEnd;
    v->fcTauN = p.fcTau * sr;
    v->attackN = p.attack * sr;
    v->decayN = lengthScale != 1.0f ? p.decay * lengthScale * sr : p.decay * sr;
    const u32 notes = count < 5 ? count : 5;  // M5: five (the Blink Star's stab); a Track's chords have three or four
    for (u32 i = 0; i < notes; ++i) {
        const double hz = midiHz(stab[i]);
        v->saw[2 * i].dt = static_cast<float>(hz / sr_);
        v->saw[2 * i + 1].dt = static_cast<float>(hz * det / sr_);
    }
    v->oscCount = notes * 2;
    v->norm = notes ? 1.0f / (2.0f * notes) : 0.0f;
    placeVoice(stab_, order_[PoolStab], v, frame);
}

void Engine::playPad(const Chord& chord, float level, float bright, i64 frame) {
    PadVoice* v = allocVoice(pad_, kPadVoices, order_[PoolPad]);
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
    v->open = plan_ ? plan_->padOpen : 1.0f;  // M5: Crescendo
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
    placeVoice(pad_, order_[PoolPad], v, frame);
}

// M5 (conductor.json mods.seeker, mods.wave; sound_m5.json fusions.helix_array): a lead note glides in from the player's
// last note over Seeker's 12 ms a stack, and sways with Wave's vibrato (5.5 Hz, 18 cents a stack) on the song's clock, so
// every note of the player sways together; `opposite`: half a cycle on (Helix Array's harmony). Murmuration takes
// Seeker's part.
Engine::LeadVoice* Engine::bendLead(LeadVoice* v, u32 player, float glideFrom, bool opposite) {
    if (!v || !v->active || player >= kMaxPlayers) return v;
    const Player& pl = players_[player];
    const ConductorRules& r = music_->conductor;
    const u32 seeker = pl.mod[static_cast<u32>(Mod::Seeker)], wave = pl.mod[static_cast<u32>(Mod::Wave)];
    if (seeker && glideFrom != 0.0f && !playerFusion(player, Fusion::Murmuration)) {
        v->glideSemis = glideFrom;
        v->glideN = framesFor(r.seekerMs * 0.001 * seeker, sr_);
        v->bend = v->glideN > 0;
    }
    if (wave) {
        v->vibSemis = r.waveCents * 0.01f * static_cast<float>(wave);
        v->vibStep = r.waveHz / static_cast<float>(sr_);
        const double turns = static_cast<double>(r.waveHz) * static_cast<double>(v->start - songStartFrame_) / sr_ + (opposite ? 0.5 : 0.0);
        v->vibPhase = static_cast<float>(turns - std::floor(turns));
        v->bend = true;
    }
    return v;
}

u32 Engine::leadVoices(LeadVoiceInfo* out, u32 cap) const {
    u32 n = 0;
    for (u32 i = 0; i < kLeadVoices && n < cap; ++i) {
        const LeadVoice& v = lead_[i];
        if (!v.active) continue;
        LeadVoiceInfo& o = out[n++];
        o.player = v.player;
        o.timbre = static_cast<u8>(v.timbre);
        o.start = v.start;
        o.hz = v.hz;
        o.vel = v.vel;
        o.gain = std::sqrt(v.gl * v.gl + v.gr * v.gr) / 1.4142f;  // the gain whatever the pan (panGains)
        o.length = v.length;
        o.glideSemis = v.glideSemis;
        o.glideN = v.glideN;
        o.vibSemis = v.vibSemis;
        o.vibPhase = v.vibPhase;
    }
    return n;
}

float Engine::padOpenNow() const {
    float open = 0.0f;
    for (u32 i = 0; i < kPadVoices; ++i)
        if (pad_[i].active && pad_[i].start <= frame_) open = std::fmax(open, pad_[i].open);
    return open;
}

// M5 (Crescendo): a hit drops the player's rise at once, and the pad it opened (the local player's).
void Engine::crescendoHit(u32 player) {
    if (player >= kMaxPlayers) return;
    crescBars_[player] = 0;
    if (player != localPlayer_) return;
    plan_->padOpen = 1.0f;
    for (u32 i = 0; i < kPadVoices; ++i) pad_[i].open = 1.0f;
}

// M5 (content_sound.cpp): what the content's sound reads of the bar and the players.
const Chord& Engine::currentChord() const { return plan_ && plan_->chord ? *plan_->chord : tr_->chordAtBar(0); }
i32 Engine::playerLastNote(u32 p) const { return p < kMaxPlayers ? players_[p].lastNote : 69; }
Timbre Engine::playerTimbre(u32 p) const { return p < kMaxPlayers ? players_[p].timbre : Timbre::P1Saw; }
bool Engine::playerBoost(u32 p, Boost b) const { return p < kMaxPlayers && ((players_[p].boosts >> static_cast<u32>(b)) & 1u); }
bool Engine::playerFusion(u32 p, Fusion f) const { return p < kMaxPlayers && ((players_[p].fusions >> static_cast<u32>(f)) & 1u); }

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
    if (tick % 12 == 0 || tick % 32 == 0) flushCues(tick);  // M5: the cues on 1/32 steps, Storm Coil on triplet eighths
    if (tick % 12 == 0) flushFamily(tick);                    // M5 audio: the kill family's step (GAME_DESIGN 6.6)
    if (tick % 12 == 0 && aggCount_) flushAgg(static_cast<double>(tick));  // M5: the fights' aggregated recipes
}

void Engine::planBar(i64 barTick) {
    if (holdFrames_ > 0.0) {
        // M4 re-seek (TECH_SPEC 8.2): the song holds on this bar line, then goes on from it (what rings, rings out).
        const double f = tempo_.frameAt(static_cast<double>(barTick));
        const double tpf = tempo_.last().ticksPerFrame;
        tempo_.appendSegment({f, static_cast<double>(barTick), 0.0});
        tempo_.appendSegment({f + holdFrames_, static_cast<double>(barTick), tpf});
        emit(BeatKind::Tempo, static_cast<i64>(f), static_cast<double>(barTick), 1, 0, 0, 0.0f, 0, 0.0);
        emit(BeatKind::Tempo, static_cast<i64>(f + holdFrames_), static_cast<double>(barTick), 1, 0, 0,
             static_cast<float>(tpf * 60.0 * sr_ / static_cast<double>(kPpq)), 0, tpf);
        holdFrames_ = 0.0;
    }
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
        // M5: and the next part's first point now (np.interp ramps across the bar line from this part's last point to
        // it): added only when the next part is planned, the 30 ms before its downbeat held the old gain, and how much of
        // the ramp a block saw depended on where the callbacks fell (found on Tracks 6-10, whose 20 s tests reach a Drop).
        if (part + 1 < t.partCount || loop_) {
            const u32 np = (part + 1) % t.partCount;
            addSectionGain(endF + kSectionRampS * sr_, sp_->sectionGain[static_cast<u32>(t.parts[np].section)][0]);
        }
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

double Engine::trackBpm() const { return bpmOverride_ > 0.0 ? bpmOverride_ : (tr_ ? tr_->bpm : 120.0); }

bool Engine::advanceCycle() {
    const u32 len[2][4] = {{8, 4, 8, 4}, {12, 4, 16, 4}};  // Short, Standard (play test 2: it was 16 / 8 / 16 / 8)
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
        sectionBars_ = form_ == Form::Short ? 8 : 12;  // the Groove (play test 2: Standard is 12 / 4 / 16 / 4)
    } else {
        if (trackNow_ && pendingTrack_ >= 0) {
            // M3: a new run starts on this bar line in the Track (and tempo) the game asks for, no Break.
            track_ = static_cast<u32>(pendingTrack_);
            bpmOverride_ = pendingBpm_;
            pendingTrack_ = -1;
            trackNow_ = false;
            ramping_ = false;
            applyParams();
            chordBar_ = 0;
            if (!formSet_) form_ = track_ == 0 ? Form::Short : Form::Standard;
            emit(BeatKind::Track, frameOf(static_cast<double>(barTick)), static_cast<double>(barTick), static_cast<u8>(track_));
        }
        // A Track change starts with a Break on the next bar line (conductor.json trackChange).
        if (pendingTrack_ >= 0 && !ramping_) forceSection_ = static_cast<i32>(Section::Break);
        if (phraseSection_ >= 0) {
            // M3: the game's director says where this bar is in the cycle; the band follows it exactly.
            const Section next = static_cast<Section>(phraseSection_);
            changed = next != section_ || phraseBar_ == 0;
            if (changed) buildFollowed_ = section_ == Section::Build && next == Section::Drop;
            section_ = next;
            sectionBar_ = phraseBar_;
            sectionBars_ = phraseBars_;
            if (changed && phraseBar_ != 0) changed = false;  // joined mid-section (after a pause): no section start
            phraseSection_ = -1;
            forceSection_ = -1;
            nextSection_ = false;
        } else {
            changed = advanceCycle();
        }
    }
    if (bossPending_) {
        boss_ = bossNext_;
        bossPending_ = false;
    } else if (boss_.flags & 8u) {
        boss_.flags = static_cast<u8>(boss_.flags & ~(2u | 4u));  // a beam's sounds belong to the bar they were planned for
    }
    const bool sectionStart = changed;
    if (sectionStart && section_ == Section::Break && pendingTrack_ >= 0 && !ramping_) {
        // The new Track's Break: eight bars, the tempo ramps linearly across it, the key changes on bar 5.
        sectionBars_ = 8;
        nextTrackAtBar_ = 4;
        ramping_ = true;
        rampFrom_ = ticksPerFrameFor(trackBpm(), sr_);
        rampTo_ = ticksPerFrameFor(pendingBpm_ > 0.0 ? pendingBpm_ : music_->tracks[pendingTrack_].bpm, sr_);
    }
    if (ramping_ && section_ != Section::Break) ramping_ = false;
    if (ramping_ && static_cast<i32>(sectionBar_) == nextTrackAtBar_ && pendingTrack_ >= 0) {
        track_ = static_cast<u32>(pendingTrack_);
        bpmOverride_ = pendingBpm_;
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
    const SoundParams& s = *sp_;

    // Tempo: the Build's last bar runs at 85% (the game slows with it and returns on the downbeat); a
    // Track change ramps bar by bar across its Break; otherwise the Track's tempo.
    {
        const double target = ticksPerFrameFor(trackBpm(), sr_);
        const double cur = tempo_.last().ticksPerFrame;
        double want = target;
        if (section_ == Section::Build && last) want = target * r.buildLastBarSpeed;
        if (ramping_) {
            const double u = static_cast<double>(sectionBar_ < sectionBars_ ? sectionBar_ : sectionBars_ - 1) /
                             static_cast<double>(sectionBars_ > 1 ? sectionBars_ - 1 : 1);
            want = rampFrom_ + (rampTo_ - rampFrom_) * u;
        }
        want *= rateScale_;  // M4: a co-op client's rate correction (1 otherwise)
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
    if (!sectionStart && gainPredictedF_ >= 0.0) {
        // The section went on after all (held, or the director said so): the point planned for the next one keeps this
        // one's gain instead.
        addSectionGain(gainPredictedF_, sg[1]);
    }
    gainPredictedF_ = -1.0;
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
    if (last) {
        const double endF = tempo_.frameAt(static_cast<double>(barTick + kTicksPerBar));
        if (section_ != Section::Build) addSectionGain(endF - kSectionRampS * sr_, sg[1]);
        // M5: the next section's first point now, so the ramp across the bar line is whole and the same whatever the
        // callbacks (the cycle's next section, or the Track change's Break; a director that chooses otherwise replaces
        // it on the bar line).
        const Section next = (pendingTrack_ >= 0 && !ramping_) ? Section::Break : static_cast<Section>((static_cast<u32>(section_) + 1) % 4);
        gainPredictedF_ = endF + kSectionRampS * sr_;
        addSectionGain(gainPredictedF_, s.sectionGain[static_cast<u32>(next)][0]);
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
    // The risers are baked at the Track's own tempo; a run that plays the Track at another (bpmOverride_) plays
    // them faster or slower by the ratio, so they still span their bars (M5: risers at each tempo).
    const double tempoRatio = trackBpm() / static_cast<double>(tr_->bpm);
    if (sectionStart && section_ == Section::Build) {
        // The riser spans the whole Build: stretched to cover the last bar's dip (and to another Build
        // length when the form was changed from the Track's own).
        const i32 id = bank_->riser(track_, sectionBars_);
        const double baked = id >= 0 ? static_cast<double>(bank_->sample(id).tag) : static_cast<double>(sectionBars_);
        const double actual = (sectionBars_ - 1) + 1.0 / r.buildLastBarSpeed;
        pend(id, static_cast<double>(barTick), s.mix.riser, BeatKind::Riser, static_cast<float>(baked / actual * tempoRatio));
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
    if (boss_.flags & 8u) {
        // M3, the boss: the iris opening is a deep gong on beat 1 (the impact, an octave down); a beam's
        // telegraph bar is a rising tone (the Build's riser, sped up to fill one bar).
        if (boss_.flags & 1u) pend(bank_->impact(), static_cast<double>(barTick), s.mix.impact * 0.45f, BeatKind::Sfx, 0.5f);
        if (boss_.flags & 2u) {
            const i32 id = bank_->riser(track_, 4);
            const double baked = id >= 0 ? static_cast<double>(bank_->sample(id).tag) : 4.0;
            pend(id, static_cast<double>(barTick), s.mix.riser * 0.7f, BeatKind::Sfx, static_cast<float>(baked * tempoRatio));
        }
    }

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
        p.riffsOn = true;
    }
    // The lead grid of every player: Double-Time stacks and the Surge (first half of the Drop) move it
    // along the grid list (capped at the last); Hyper halves it.
    // The Surge runs in two gears (conductor.json drop.surgeSteps): its first half one step along, its
    // second half two. M5: Afterparty holds the second gear two bars longer a stack, one step further (leadGridIndex).
    for (u32 i = 0; i < playerCount_; ++i) {
        const u32 idx = leadGridIndex(surgeSteps_, players_[i].doubleTime, players_[i].mod[static_cast<u32>(Mod::Afterparty)], section_,
                                      sectionBar_, sectionBars_);
        u32 g = r.gridTicks[idx];
        if (players_[i].hyper && g % 2 == 0) g /= 2;
        p.leadGrid[i] = g;
        p.sparse[i] = section_ == Section::Break;
    }
    upgradeBar();  // M5
    followChordVoices(frameOf(static_cast<double>(p.tick)));  // M5: the fights' held voices that follow the chord (a Rift's drone)
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
        if (float v = vel(Pat::Kick); v > 0.0f) playKick(s.mix.kick * v, v, f, tk);
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
    // M3: the boss's glass bells (kaleidos.md "Sound"): one P3 bell voice per firing pair on every step it
    // fires (A the chord's root, B its third, C its fifth), so the resting pair is a hole in the ostinato;
    // a beam's hard stab on beat 1.
    if ((boss_.flags & 8u) && inBar % kTicksPerStep == 0 && p.chord) {
        const u32 st = static_cast<u32>(inBar / kTicksPerStep);
        const bool step = boss_.sixteenths || st % 2 == 0;
        if (boss_.mask && step && (boss_.beats & (1u << (st / 4)))) {
            static constexpr float kBellPan[3] = {0.0f, -0.45f, 0.45f};
            const Chord& ch = *p.chord;
            for (u32 k = 0; k < 3; ++k) {
                if (!(boss_.mask & (1u << k))) continue;
                const u8 note = ch.stabCount > k ? ch.stab[k] : (ch.padCount > k ? ch.pad[k] : ch.bass);
                playLead(7, Timbre::P3Bell, static_cast<float>(note + 12), 0.75f, 0.55f, kBellPan[k], f, 0.7f);
            }
        }
        if ((boss_.flags & 4u) && st == 0) playStab(*p.chord, 1.0f, 0.0f, f);
    }
    // Riff patterns at double rate under Hyper: the sixteen steps in half a bar.
    const bool riffStep = p.riffDouble ? inBar % 12 == 0 : inBar % kTicksPerStep == 0;
    const u32 riffSt = static_cast<u32>(p.riffDouble ? (inBar / 12) % 16 : inBar / kTicksPerStep);
    // M5 (conductor.json boosts.time_stretch): the drums at half rate, their pattern stretched over two bars.
    bool drumStep = inBar % kTicksPerStep == 0;
    u32 drumSt = static_cast<u32>(inBar / kTicksPerStep);
    if (p.timeStretch) {
        const i64 pos = static_cast<i64>(p.stretchHalf) * kTicksPerBar + inBar;
        drumStep = pos % (2 * kTicksPerStep) == 0;
        drumSt = static_cast<u32>(pos / (2 * kTicksPerStep)) % 16;
    }
    if (drumStep) {
        const u32 st = drumSt;
        const auto on = [&](Pat pat) { return p.pat[static_cast<u32>(pat)] > 0.0f; };
        if (on(Pat::Kick) && t.pattern[static_cast<u32>(Pat::Kick)][st] > 0.0f)
            playKick(s.mix.kick * t.pattern[static_cast<u32>(Pat::Kick)][st], t.pattern[static_cast<u32>(Pat::Kick)][st], f, tk);
        if (on(Pat::Clap)) {
            float v = t.pattern[static_cast<u32>(Pat::Clap)][st];
            if (p.fill && st == music_->conductor.fillPickupStep && v <= 0.0f) v = 3.0f / 9.0f;  // the fill's pick-up
            if (v > 0.0f) {
                playSample(bank_->clap(), Bus::Clap, f, s.mix.clap * v, kPanClap);
                emit(BeatKind::Clap, f, tk, 0, 0, 0, v);
            }
        }
        const bool hats = !hatsCut(f);  // M5 (CutBed: the Moth Queen open, the bed down to kick and pad)
        if (on(Pat::HatOff) && hats) {
            float v = t.pattern[static_cast<u32>(Pat::HatOff)][st];
            if (p.fill && st >= 8 && v <= 0.0f) v = t.pattern[static_cast<u32>(Pat::Hat16)][st];  // hats double
            if (v > 0.0f) {
                playSample(kit.hat, p.hatOffBreak ? Bus::HatsBreak : Bus::Drums, f,
                           s.mix.hat * v * p.pat[static_cast<u32>(Pat::HatOff)], kPanHatOff);
                emit(BeatKind::Hat, f, tk, 0, 0, 0, v);
            }
        }
        if (on(Pat::Hat16) && hats && t.pattern[static_cast<u32>(Pat::Hat16)][st] > 0.0f) {
            const float v = t.pattern[static_cast<u32>(Pat::Hat16)][st];
            playSample(kit.hat, Bus::Drums, f, s.mix.hat * v, st % 2 == 0 ? -kPanHat16 : kPanHat16);
            emit(BeatKind::Hat, f, tk, 0, 0, 0, v);
        }
        if (on(Pat::Ohat) && hats && t.pattern[static_cast<u32>(Pat::Ohat)][st] > 0.0f) {
            const float v = t.pattern[static_cast<u32>(Pat::Ohat)][st];
            playSample(kit.ohat, Bus::Drums, f, s.mix.ohat * v, kPanOhat);
            emit(BeatKind::Hat, f, tk, 0, 1, 0, v);
        }
        if (inBar == 0 && p.pad > 0.0f) {  // (a drum step in every bar, stretched or not)
            playPad(*p.chord, p.pad, p.padBright, f);
            emit(BeatKind::Pad, f, tk, p.chord->pad[0], 0, 0, p.pad);
        }
    }
    if (riffStep) {
        const auto on = [&](Pat pat) { return p.pat[static_cast<u32>(pat)] > 0.0f; };
        if (on(Pat::Shaker) && !hatsCut(f) && t.pattern[static_cast<u32>(Pat::Shaker)][riffSt] > 0.0f) {
            const float v = t.pattern[static_cast<u32>(Pat::Shaker)][riffSt];
            playSample(bank_->shaker(), Bus::Drums, f, s.mix.shaker * v, riffSt % 2 ? kPanShaker : -kPanShaker);
            emit(BeatKind::Shaker, f, tk, 0, 0, 0, v);
            // M5 (sound_m5.json fusions.shard_halo): the Orbitals' shaker doubled by glass ticks an octave apart (in turn).
            if ((p.fusions >> static_cast<u32>(Fusion::ShardHalo)) & 1u && p.riff[static_cast<u32>(Riff::Orbitals)]) {
                const i32 note = 96 + p.chord->bass % 12 - 12 * static_cast<i32>(haloTick_++ % 2);
                recipeShot(Recipe::Glass, note, recipeGain(Recipe::Glass) * v, riffSt % 2 ? kPanShaker : -kPanShaker, f, Bus::Drums);
                upgradeEvent(UpgradeSound::HaloGlass, f, tk, recipeGain(Recipe::Glass) * v, note);
            }
        }
        if (on(Pat::Pulse)) {
            float v = t.pattern[static_cast<u32>(Pat::Pulse)][riffSt];
            if (p.pulseEveryBeat && riffSt % 4 == 0 && v <= 0.0f) v = 1.0f;
            if (v > 0.0f) {
                playSample(bank_->pulse(), Bus::Pulse, f, s.mix.pulse * v, 0.0f);
                emit(BeatKind::Pulse, f, tk, 0, 0, 0, v);
                // M5 (fusions.tesla_bloom): each ring's tom carries a zap on all three stab notes.
                if ((p.fusions >> static_cast<u32>(Fusion::TeslaBloom)) & 1u && p.riff[static_cast<u32>(Riff::PulseRing)]) {
                    for (u32 k = 0; k < p.chord->stabCount && k < 3; ++k)
                        recipeShot(Recipe::Zap, p.chord->stab[k], recipeGain(Recipe::Zap) * v * kTeslaZap, k == 0 ? 0.0f : (k == 1 ? -0.3f : 0.3f),
                                   f, Bus::Pulse);
                    upgradeEvent(UpgradeSound::TeslaZap, f, tk, recipeGain(Recipe::Zap) * v * kTeslaZap, p.chord->stab[0]);
                }
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
            // M5 (conductor.json boosts.colossus): the bass gains its octave below.
            if ((p.boosts >> static_cast<u32>(Boost::Colossus)) & 1u) {
                fastBass(playBass(midi - 12.0f, n.vel * p.bassFade * r.colossusLevel, n.steps, f));
                upgradeEvent(UpgradeSound::ColossusBass, f, tk, n.vel * p.bassFade * r.colossusLevel, midi - 12.0f);
            }
        }
    }
    if (p.rollDiv && inBar % p.rollDiv == 0 && !(p.lastBuildBar && inBar >= kTicksPerBar / 2)) {
        const float ts = static_cast<float>(inBar) / kTicksPerStep;
        const float level = p.rollLo + (p.rollHi - p.rollLo) * ((p.rollBar + ts / 16.0f) / static_cast<float>(p.rollBars));
        playSample(bank_->snare(), Bus::Snare, f, s.mix.snare * level, 0.0f);
        emit(BeatKind::Snare, f, tk, 0, 0, 0, level);
    }
    if (p.upgrades) upgradeStep(tick, inBar);  // M5: Riffs 4-12, the patterned Boosts and Fusions
    leadStep(tick, inBar);
    if (stabAcc_ > 0.0f) flushStabs(f);  // M5: Accent's and Bar Line's stabs, one voice a tick
}

float Engine::leadNoteMidi(u8 degree) const {
    // The Track's root in the lead's register: the tonic at or below the bar's first arpeggio note.
    const Track& t = *tr_;
    const Chord& ch = plan_ && plan_->chord ? *plan_->chord : t.chordAtBar(0);
    const i32 tonic = t.chords[t.progression[0]].bass % 12;
    const i32 top = ch.arp[0];
    const i32 root = top - (((top - tonic) % 12) + 12) % 12;
    static constexpr i32 kDegree[5] = {0, 7, 12, -12, 24};
    return static_cast<float>(root + kDegree[degree < 5 ? degree : 0]);
}

void Engine::leadStep(i64 tick, i64 inBar) {
    const BarPlan& p = *plan_;
    const ConductorRules& r = music_->conductor;
    const SoundParams& s = *sp_;
    const Track& t = *tr_;
    const double tk = static_cast<double>(tick);
    for (u32 i = 0; i < playerCount_; ++i) {
        const Player& pl = players_[i];
        // M4 co-op, "music from host only": the other players' leads are their own phones'.
        if (hostOnly_ && i != localPlayer_) continue;
        const float level = i == localPlayer_ ? 1.0f : (pl.distance > r.othersFarWu ? r.othersFarLevel : r.othersLevel);
        // M4 (look.md 6): a downed player's beacon is its lead holding the Track's root an octave down, an eighth-note
        // tremolo 12 dB under the leads, panned by where it is.
        if (pl.beacon && inBar % (kPpq / 2) == 0) {
            const i64 f = frameOf(tk);
            const bool onBeat = inBar % kPpq == 0;
            playLead(static_cast<u8>(i), pl.timbre, leadNoteMidi(3), onBeat ? kBeaconVelOn : kBeaconVelOff, s.mix.lead * level * kBeaconGain,
                     pl.beaconPan, f, 0.5f);
        }
        const u32 grid = p.leadGrid[i] ? p.leadGrid[i] : r.gridTicks[0];
        if (pl.fire) {
            // M5 (conductor.json mods.rear_guard): the counter-melody on its own sixteenths while fire is held.
            if (p.upgrades && i == localPlayer_ && pl.mod[static_cast<u32>(Mod::RearGuard)] && inBar % kTicksPerStep == 0 &&
                !(p.leadStopsHalf && inBar >= kTicksPerBar / 2))
                rearGuardStep(i, tick, inBar, s.mix.lead * level * std::pow(10.0f, r.overchargeDb * pl.overcharge / 20.0f), level);
            if (inBar % grid != 0 || (p.leadStopsHalf && inBar >= kTicksPerBar / 2)) continue;
            const u32 k = static_cast<u32>(inBar / grid);
            if (pl.fire == 2 && (k & 1u) != 0) continue;  // M3: the assist fires on every other step
            const u8 note = p.chord->arp[k % 16];
            const float accent = inBar % kPpq == 0 ? r.accentBeat : (inBar % (kPpq / 2) == 0 ? r.accentEighth : r.accentOther);
            const float pan = r.panDepth * std::sin(k * r.panRate + static_cast<float>(p.index));
            float scale = std::pow(r.pierceScale, static_cast<float>(pl.pierce));
            // M5 (cost): a teammate's lead is not driven on this device (Overcharge's tanh, a sample a voice); its level
            // still rises with the stacks.
            const float drive = pl.overcharge && i == localPlayer_ ? 1.0f + r.overchargeDrive * pl.overcharge : 0.0f;
            float gain = s.mix.lead * level * std::pow(10.0f, r.overchargeDb * pl.overcharge / 20.0f);
            const i64 f = frameOf(tk);
            // M5: what the Mods and Boosts do to this player's notes (conductor.json mods / boosts, sound_m5.json fusions).
            float vel = accent, mainGain = gain;
            const bool m5 = p.upgrades;
            const float glideFrom = static_cast<float>(pl.lastNote) - static_cast<float>(note);
            const bool helix = playerFusion(i, Fusion::HelixArray);
            if (m5) {
                const u8* m = pl.mod;
                if (m[static_cast<u32>(Mod::Crescendo)]) {  // +0.2 dB a bar without a hit, to +2 dB
                    const float db = std::fmin(r.crescendoMaxDb, r.crescendoDbPerBar * static_cast<float>(crescBars_[i]));
                    gain *= std::pow(10.0f, db / 20.0f);
                }
                mainGain = gain;
                if (playerBoost(i, Boost::Beam)) {  // the lead holds: four times the decay, retriggered softly
                    scale *= 4.0f;
                    vel *= kBeamVel;
                }
                const u32 acc = m[static_cast<u32>(Mod::Accent)];
                const i64 accEvery = acc >= 3 ? kPpq : (acc == 2 ? kTicksPerBar / 2 : kTicksPerBar);
                if (acc && inBar % accEvery == 0 && !playerFusion(i, Fusion::StandingWave)) mainGain *= r.accentLevel;
                if (m[static_cast<u32>(Mod::Syncopation)] && inBar % kPpq != 0)
                    mainGain *= 1.0f + r.syncLouder * static_cast<float>(m[static_cast<u32>(Mod::Syncopation)]);
            }
            players_[i].lastNote = note;
            markLead(i, tk, note);  // M5: what a fight may play again (LeadEcho)
            LeadVoice* main = playLead(static_cast<u8>(i), pl.timbre, note, vel, mainGain, pan, f, scale, drive);
            if (m5) bendLead(main, i, glideFrom, false);
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
            // M5: every note of the player bends with its lead (Seeker, Wave); Helix Array swings the harmony the other way.
            const auto bend = [&](LeadVoice* v, bool opposite) {
                if (m5) bendLead(v, i, glideFrom, opposite);
            };
            // M5 (cost): a teammate's harmony is its own device's (kLeadPerOther).
            const bool own = i == localPlayer_;
            const u32 splitN = own ? pl.split : 0u;
            for (u32 st = 0; st < splitN && st < r.splitDegreeCount; ++st)
                bend(playLead(static_cast<u8>(i), pl.timbre, degreeAbove(note, r.splitDegrees[st]), vel, gain * r.splitLevel, pan, f,
                              scale, drive),
                     helix);
            if (pl.prism && own) {
                bend(playLead(static_cast<u8>(i), pl.timbre, degreeAbove(note, 4), vel, gain * r.prismLevel, pan, f, scale, drive), false);
                bend(playLead(static_cast<u8>(i), pl.timbre, note + 12.0f, vel, gain * r.prismLevel, pan, f, scale, drive), false);
            }
            // (Downbeat Cannon takes On-Beat Crit's part: sound_m5.json fusions.)
            if (pl.crit && inBar % kPpq == 0 && !playerFusion(i, Fusion::DownbeatCannon) && own)
                bend(playLead(static_cast<u8>(i), pl.timbre, note + 12.0f, vel, gain * r.critLevel, pan, f, scale, drive), false);
            if (m5) leadUpgrades(i, tick, inBar, k, note, vel, gain, pan, scale, drive, level, glideFrom);
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

void Engine::flushKills(i64 /*tick*/) {
    // Kills are aggregated (README): one sound per size class per 1/32 step, its gain following the
    // count, pitched to a chord tone (small an octave up, medium at pitch, large an octave down). M5 audio: into the kill
    // family of the step (flushFamily plays them, on the tick).
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
        familyAdd(false, static_cast<u8>(kind), midi, gain, pan);  // M5 audio: the kill family (flushFamily plays the step)
    }
}

void Engine::firePending(i64 frameEnd) {
    // In the order they were planned (M5: the list keeps its order, so effects due on the same frame always start
    // in the same order, whichever callback they fall in).
    u32 keep = 0;
    for (u32 i = 0; i < pendingCount_; ++i) {
        const Pending q = pending_[i];
        if (q.frame >= frameEnd) {
            pending_[keep++] = q;
            continue;
        }
        const i64 f = q.frame < frame_ ? frame_ : q.frame;
        if (q.lead >= 0) {
            playLead(static_cast<u8>(q.lead), q.timbre, q.note, kLeadNoteVel, q.gain, q.pan, f);
        } else if (q.sample >= 0) {
            playSample(q.sample, q.bus, f, q.gain, q.pan, q.rate);
            emit(q.kind, f, q.tick, 0, 0, 0, q.gain, 0, q.rate);
        } else if (q.sfx != Sfx::Count) {
            playSfx(q.sfx, q.midi, q.gain, q.pan, f, q.tick, q.rate);
        }
    }
    pendingCount_ = keep;
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

// One lead voice's rendering state. Control-rate values at fixed ages (knots every 32 samples), so the result does not
// depend on how callbacks split the time. The attack ramp and the tail fade only touch the first and last few knots:
// the rest takes the plain path. The hot state lives in the struct (locals of the caller): the outputs are float
// pointers, so the compiler could not otherwise keep oscillator phases and filter state in registers.
template <Timbre T>
struct Engine::LeadRun {
    static constexpr u32 kKnot = 32;
    static constexpr float kInvKnot = 1.0f / kKnot;
    LeadVoice& v;
    const float decMul, invAtt, invFade, gl, gr, pre, sub, drive;
    const u32 attEnd, fadeStart;
    const bool driven;
    // The segment (a run of samples inside one knot interval).
    u32 phase = 0, kIndex = 0, a = 0;
    bool plain = true;
    float dec = 0.0f, mod = 0.0f, modMul = 0.0f;
    float c0a1 = 0.0f, c0a2 = 0.0f, c0a3 = 0.0f, d1 = 0.0f, d2 = 0.0f, d3 = 0.0f;
    SawOsc s0, s1, s2, s3;
    SquareOsc q0, q1;
    SvfLowpass f;
    // M5 (cost): every PolyBLEP the timbre needs a sample, in one vector: P1 the two saws and the square's two edges
    // (its phase and the phase half a cycle on), P2 both squares' edges, P4 the four saws.
    f4 ph = 0.5f, dt4 = 0.0f, inv4 = 0.0f;
    // M5 (Seeker, Wave): the note's own increments; a bent voice runs at them times the knot's pitch ratio.
    f4 dt0 = 0.0f, inv0 = 0.0f;
    float bellDt0 = 0.0f, bellDt1 = 0.0f;

    explicit LeadRun(LeadVoice& voice)
        : v(voice),
          decMul(voice.decMul),
          invAtt(voice.attackN > 0.0f ? 1.0f / voice.attackN : 0.0f),
          invFade(voice.fade > 1 ? 1.0f / static_cast<float>(voice.fade - 1) : 0.0f),
          gl(voice.gl * voice.vel),
          gr(voice.gr * voice.vel),
          pre(voice.pre),
          sub(voice.sub),
          drive(voice.drive),
          attEnd(static_cast<u32>(voice.attackN) + 1),
          fadeStart(voice.length - voice.fade),
          driven(voice.drive > 0.0f),
          s0(voice.saw[0]),
          s1(voice.saw[1]),
          s2(voice.saw[2]),
          s3(voice.saw[3]),
          q0(voice.sq[0]),
          q1(voice.sq[1]),
          f(voice.svf) {
        // The squares' second edge runs as a phase of its own (started half a cycle on by playLead), kept in an
        // oscillator slot the timbre does not use, so it does not depend on where the blocks fell.
        if constexpr (T == Timbre::P1Saw) {
            ph = f4{s0.phase, s1.phase, q0.phase, q1.phase};
            dt4 = f4{s0.dt, s1.dt, q0.dt, q0.dt};
        } else if constexpr (T == Timbre::P2Square) {
            ph = f4{q0.phase, s0.phase, q1.phase, s1.phase};
            dt4 = f4{q0.dt, q0.dt, q1.dt, q1.dt};
        } else if constexpr (T == Timbre::P4Stab) {
            ph = f4{s0.phase, s1.phase, s2.phase, s3.phase};
            dt4 = f4{s0.dt, s1.dt, s2.dt, s3.dt};
        }
        inv4 = invDt4(dt4);
        dt0 = dt4;
        inv0 = inv4;
        bellDt0 = s0.dt;
        bellDt1 = s1.dt;
    }

    float fcAt(float age) const { return (v.fcEnd + (v.fcStart - v.fcEnd) * std::exp(-age / v.fcTauN)) * v.kt; }

    // M5: the pitch of a bent voice over the knot interval from age k0 (Seeker's glide toward the note, Wave's vibrato), as
    // a frequency ratio; the same for every segment of the interval, wherever the callbacks fell.
    void bendAt(u32 k0) {
        if (v.bendKnot != k0) {  // once a knot interval (a block boundary splits most of them in two)
            float semis = 0.0f;
            if (k0 < v.glideN) semis += v.glideSemis * (1.0f - static_cast<float>(k0) / static_cast<float>(v.glideN));
            if (v.vibSemis != 0.0f) semis += v.vibSemis * sinTurns(v.vibPhase + v.vibStep * static_cast<float>(k0));
            v.bendRatio = fastExp2(semis * (1.0f / 12.0f));
            v.bendKnot = k0;
        }
        const float ratio = v.bendRatio;
        if constexpr (T == Timbre::P3Bell) {
            s0.dt = bellDt0 * ratio;
            s1.dt = bellDt1 * ratio;
        } else {
            dt4 = dt0 * ratio;
            inv4 = inv0 * (1.0f / ratio);
        }
    }

    // A segment of `len` samples from the voice's age `age` (inside one knot interval).
    void begin(u32 age, u32 len, float sr) {
        a = age;
        phase = age % kKnot;
        const u32 k0 = age - phase;
        kIndex = k0 / kKnot;
        if (v.bend) bendAt(k0);
        plain = age >= attEnd && age + len <= fadeStart;  // no attack ramp, no tail fade
        const bool goesOn = kIndex == v.runKnot;  // the last segment rendered the start of this interval
        dec = v.decRun;
        if (!goesOn) {
            dec = std::exp(-static_cast<float>(k0) / v.decayN);
            for (u32 j = 0; j < phase; ++j) dec *= decMul;
        }
        if constexpr (T == Timbre::P3Bell) {
            // sfxproto.bell(hz, 0.36, 2.0, 2.4): FM, the index decaying with 0.15 s.
            modMul = std::exp(-1.0f / v.fcTauN);
            mod = v.modRun;
            if (!goesOn) {
                mod = kP3Index * std::exp(-static_cast<float>(k0) / v.fcTauN);
                for (u32 j = 0; j < phase; ++j) mod *= modMul;
            }
        } else {
            // Filter coefficients exact at the knots, linear in between (no division per sample). The next knot's
            // coefficients are kept for the following interval; this interval's for its next segment.
            SvfCoefs c0;
            if (kIndex == v.knot) {
                c0 = v.cCur;
            } else {
                c0 = (v.knot != ~0u && kIndex == v.knot + 1) ? v.cNext : svfCoefs(svfG(fcAt(static_cast<float>(k0)), sr), v.k);
                v.cNext = svfCoefs(svfG(fcAt(static_cast<float>(k0 + kKnot)), sr), v.k);
                v.knot = kIndex;
                v.cCur = c0;
            }
            const SvfCoefs c1 = v.cNext;
            c0a1 = c0.a1;
            c0a2 = c0.a2;
            c0a3 = c0.a3;
            d1 = (c1.a1 - c0.a1) * kInvKnot;
            d2 = (c1.a2 - c0.a2) * kInvKnot;
            d3 = (c1.a3 - c0.a3) * kInvKnot;
        }
    }

    // Sample j of the segment, enveloped (before the pan gains).
    inline __attribute__((always_inline)) float next(u32 j) {
        if constexpr (T == Timbre::P3Bell) {
            constexpr float kInvTau = 1.0f / (2.0f * kPiF);
            const float m = sinTurns(s1.phase) * mod;
            float y = sinTurns(s0.phase + m * kInvTau) * dec;
            if (!plain) {
                const u32 aj = a + j;
                if (aj < attEnd) y *= std::fmin(1.0f, static_cast<float>(aj) * invAtt);
                if (aj >= fadeStart) y *= 1.0f - static_cast<float>(aj - fadeStart) * invFade;
            }
            s0.phase += s0.dt;
            s0.phase -= std::floor(s0.phase);
            s1.phase += s1.dt;
            s1.phase -= std::floor(s1.phase);
            dec *= decMul;
            mod *= modMul;
            return y;
        } else {
            const f4 t = ph;
            const f4 b = polyBlep4(t, dt4, inv4);
            advance4(ph, dt4);
            float x;
            if constexpr (T == Timbre::P1Saw) {
                const f4 saw = 2.0f * t - 1.0f - b;
                x = saw.x + saw.y + sub * ((t.z < 0.5f ? 1.0f : -1.0f) + b.z - b.w);
            } else if constexpr (T == Timbre::P2Square) {
                x = ((t.x < 0.5f ? 1.0f : -1.0f) + b.x - b.y) + ((t.z < 0.5f ? 1.0f : -1.0f) + b.z - b.w);
            } else {
                const f4 saw = 2.0f * t - 1.0f - b;
                x = (saw.x + saw.y + saw.z) * (1.0f / 3.0f) + kP4Oct * saw.w;
            }
            const float p = static_cast<float>(phase + j);
            float y = f.tick(x * pre, c0a1 + d1 * p, c0a2 + d2 * p, c0a3 + d3 * p);
            if (driven) y = fastTanh(drive * y);  // M5 (cost): Overcharge's drive
            y *= dec;
            if (!plain) {
                const u32 aj = a + j;
                if (aj < attEnd) y *= std::fmin(1.0f, static_cast<float>(aj) * invAtt);
                if (aj >= fadeStart) y *= 1.0f - static_cast<float>(aj - fadeStart) * invFade;
            }
            dec *= decMul;
            return y;
        }
    }

    void end() {
        v.decRun = dec;
        v.runKnot = kIndex;
        if constexpr (T == Timbre::P3Bell) v.modRun = mod;
    }
    void store() {
        if constexpr (T == Timbre::P1Saw) {
            s0.phase = ph.x;
            s1.phase = ph.y;
            q0.phase = ph.z;
            q1.phase = ph.w;
        } else if constexpr (T == Timbre::P2Square) {
            q0.phase = ph.x;
            s0.phase = ph.y;
            q1.phase = ph.z;
            s1.phase = ph.w;
        } else if constexpr (T == Timbre::P4Stab) {
            s0.phase = ph.x;
            s1.phase = ph.y;
            s2.phase = ph.z;
            s3.phase = ph.w;
        }
        if constexpr (T == Timbre::P3Bell) {  // M5: the note's own increments stay in the voice (a bend is per knot)
            s0.dt = bellDt0;
            s1.dt = bellDt1;
        }
        v.saw[0] = s0;
        v.saw[1] = s1;
        v.saw[2] = s2;
        v.saw[3] = s3;
        v.sq[0] = q0;
        v.sq[1] = q1;
        v.svf = f;
    }
};

template <Timbre T>
void Engine::renderLead(LeadVoice& v, float* __restrict L, float* __restrict R, u32 cnt, float sr) {
    LeadRun<T> run(v);
    const float gl = run.gl, gr = run.gr;
    for (u32 i = 0; i < cnt;) {
        const u32 a = v.age + i, phase = a % LeadRun<T>::kKnot;
        const u32 len = LeadRun<T>::kKnot - phase < cnt - i ? LeadRun<T>::kKnot - phase : cnt - i;
        run.begin(a, len, sr);
        float* l = L + i;
        float* r = R + i;
        for (u32 j = 0; j < len; ++j) {
            const float y = run.next(j);
            l[j] += y * gl;
            r[j] += y * gr;
        }
        run.end();
        i += len;
    }
    run.store();
}

void Engine::renderBlock(float* out, u32 n, u32 channels) {
    const float sr = static_cast<float>(sr_);
    auto stamp = profile_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto mark = [&](u32 stage) {
        if (!profile_) return;
        const auto now = std::chrono::steady_clock::now();
        profile_->ns[stage] += static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - stamp).count());
        stamp = now;
    };
    std::memset(bus_, 0, sizeof bus_);
    std::memset(wet_, 0, sizeof wet_);
    if (sendUsed_) {  // M5: the content voices' own reverb send
        std::memset(sendL_, 0, sizeof sendL_);
        std::memset(sendR_, 0, sizeof sendR_);
        sendUsed_ = false;
    }
    if (padTone_ != padToneTarget_ || padGain_ != padGainTarget_) stepPadTone(n);  // M5 (PadTone)
    duckFrames_ = 0;
    u32 active = 0;

    // Voices. Control-rate values (filter coefficients, envelope anchors) are taken at fixed ages of each
    // voice (every 32 samples since its start) and stepped per sample in between, so the output does
    // not depend on how the device splits time into callbacks (tested: random bursts render identically).
    constexpr u32 kKnot = 32;
    constexpr float kInvKnot = 1.0f / kKnot;
    // Lead voices (each pool in the order its voices start: M5, the sums do not depend on the slots).
    const auto leadTime = [&](Timbre t, std::chrono::steady_clock::time_point from) {
        if (!profile_) return;
        profile_->ns[EngineProfile::Lead + static_cast<u32>(t)] +=
            static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - from).count());
    };
    const Order& leadOrder = order_[PoolLead];
    for (u32 vk = 0; vk < leadOrder.n; ++vk) {
        LeadVoice& v = lead_[leadOrder.idx[vk]];
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
        const auto tv = profile_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        switch (v.timbre) {
            case Timbre::P1Saw: renderLead<Timbre::P1Saw>(v, L, R, cnt, sr); break;
            case Timbre::P2Square: renderLead<Timbre::P2Square>(v, L, R, cnt, sr); break;
            case Timbre::P3Bell: renderLead<Timbre::P3Bell>(v, L, R, cnt, sr); break;
            default: renderLead<Timbre::P4Stab>(v, L, R, cnt, sr); break;
        }
        leadTime(v.timbre, tv);
        v.age += cnt;
        if (v.age >= v.length) v.active = false;
    }
    compactOrder(order_[PoolLead], lead_);
    if (active > stats_.leadPeak.load(std::memory_order_relaxed)) stats_.leadPeak.store(active, std::memory_order_relaxed);
    if (profile_) stamp = std::chrono::steady_clock::now();  // the lead was timed voice by voice
    // Bass.
    for (u32 vk = 0; vk < order_[PoolBass].n; ++vk) {
        BassVoice& v = bass_[order_[PoolBass].idx[vk]];
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
        float* L = bus_[static_cast<u32>(v.bus)][0] + i0;
        float* R = bus_[static_cast<u32>(v.bus)][1] + i0;
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
                y += v.sub * sinTurns(subPhase);  // M5 (cost): not a libm sinf a sample
                subPhase += v.subDt;
                subPhase -= std::floor(subPhase);
                const float att = aj < attEnd ? static_cast<float>(aj) * invAtt : 1.0f;
                const float driven = v.drive * y * att * dec * rel;
                float o = (v.fastDrive ? fastTanh(driven) : std::tanh(driven)) * v.vel * v.gain;  // M5: the upgrades' bass notes
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
    compactOrder(order_[PoolBass], bass_);

    mark(EngineProfile::Bass);
    // Stabs.
    for (u32 vk = 0; vk < order_[PoolStab].n; ++vk) {
        StabVoice& v = stab_[order_[PoolStab].idx[vk]];
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
        const u32 stabBus = static_cast<u32>(mode_ == Mode::Script ? Bus::Sfx : Bus::Stab);  // M5: a sheet's stab (Recipe::Stab), dry
        float* L = bus_[stabBus][0] + i0;
        float* R = bus_[stabBus][1] + i0;
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
            SawOsc osc[10];
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
    compactOrder(order_[PoolStab], stab_);

    mark(EngineProfile::Stab);
    // Pads.
    for (u32 vk = 0; vk < order_[PoolPad].n; ++vk) {
        PadVoice& v = pad_[order_[PoolPad].idx[vk]];
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
        const float gain = v.level * v.gain * padGain_;  // M5 (PadTone: Feedback's Head ducks the pad while it is closed)
        const float open = v.open * padTone_;           // ... and opens or dulls its filter
        for (u32 ch = 0; ch < 2; ++ch) {
            float* out = bus_[static_cast<u32>(Bus::Pad)][ch] + i0;
            const auto coefs = [&](u32 a) {
                const float lfo = 0.5f + 0.5f * std::sin(v.lfoW * static_cast<float>(a) + 0.9f * static_cast<float>(ch));
                return svfCoefs(svfG((v.fcBase + v.fcLfo * lfo) * v.bright * open, sr), v.k);
            };
            SawOsc* osc = v.saw[ch];
            // M5 (cost): the channel's saws four at a time (a lane past the last saw stands still at phase 0.5, dt 0,
            // which reads 0).
            const u32 nosc = v.oscCount;
            const u32 nvec = (nosc + 3) / 4;
            f4 ph[3], dt[3], inv[3];
            for (u32 q = 0; q < 3; ++q) {
                ph[q] = 0.5f;
                dt[q] = 0.0f;
            }
            for (u32 o = 0; o < nosc && o < 12; ++o) {
                ph[o / 4][o % 4] = osc[o].phase;
                dt[o / 4][o % 4] = osc[o].dt;
            }
            for (u32 q = 0; q < nvec; ++q) inv[q] = invDt4(dt[q]);
            SvfLowpass f = v.svf[ch];
            for (u32 i = 0; i < cnt;) {
                const u32 a = v.age + i, phase = a % kKnot, k0 = a - phase;
                const u32 len = kKnot - phase < cnt - i ? kKnot - phase : cnt - i;
                const u32 kIndex = k0 / kKnot;
                const bool goesOn = kIndex == v.knot[ch];  // this interval's start was rendered by the last segment
                if (!goesOn) {
                    v.cCur[ch] = (v.knot[ch] != ~0u && kIndex == v.knot[ch] + 1) ? v.cNext[ch] : coefs(k0);
                    v.cNext[ch] = coefs(k0 + kKnot);
                    v.knot[ch] = kIndex;
                }
                const SvfCoefs c0 = v.cCur[ch], c1 = v.cNext[ch];
                const float d1 = (c1.a1 - c0.a1) * kInvKnot, d2 = (c1.a2 - c0.a2) * kInvKnot, d3 = (c1.a3 - c0.a3) * kInvKnot;
                float rel = v.relRun[ch];
                if (!goesOn) {
                    rel = k0 > v.hold ? std::exp(-static_cast<float>(k0 - v.hold) / v.releaseN) : 1.0f;
                    for (u32 j = 0; j < phase; ++j)
                        if (k0 + j >= v.hold) rel *= relMul;
                }
                for (u32 j = 0; j < len; ++j) {
                    const u32 aj = a + j;
                    f4 acc = 0.0f;
                    for (u32 q = 0; q < nvec; ++q) {
                        const f4 t = ph[q];
                        acc += 2.0f * t - 1.0f - polyBlep4(t, dt[q], inv[q]);
                        advance4(ph[q], dt[q]);
                    }
                    const float x = (acc.x + acc.y) + (acc.z + acc.w);
                    float y = f.tick(x * v.norm, c0.a1 + d1 * static_cast<float>(phase + j), c0.a2 + d2 * static_cast<float>(phase + j), c0.a3 + d3 * static_cast<float>(phase + j));
                    float s = aj < attEnd ? static_cast<float>(aj) * invAtt : 1.0f;
                    if (aj >= fadeStart) s *= 1.0f - static_cast<float>(aj - fadeStart) * invFade;
                    y *= s * rel * gain;
                    if (aj >= v.hold) rel *= relMul;
                    out[i + j] += y;
                }
                v.relRun[ch] = rel;
                i += len;
            }
            for (u32 o = 0; o < nosc && o < 12; ++o) osc[o].phase = ph[o / 4][o % 4];
            v.svf[ch] = f;
        }
        v.age += cnt;
        if (v.age >= v.length) v.active = false;
    }
    compactOrder(order_[PoolPad], pad_);
    mark(EngineProfile::Pad);
    // Samples.
    for (u32 vk = 0; vk < order_[PoolSample].n; ++vk) {
        SampleVoice& v = smp_[order_[PoolSample].idx[vk]];
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
        if (v.gateN) {
            // M5 (Phasing's fifth, gated on the eighths): a mono voice at its rate under an on-off gate (a fight's, rare).
            double pos = v.pos;
            const u32 last = v.frames ? v.frames - 1 : 0;
            bool ended = v.frames == 0;
            const i64 period = 2 * static_cast<i64>(v.gateN);
            const float ramp = static_cast<float>(kGateRamp);
            for (u32 i = i0; i < n; ++i) {
                const u32 k = static_cast<u32>(pos);
                if (k >= last) {
                    ended = true;
                    break;
                }
                const i64 g = ((frame_ + static_cast<i64>(i) - v.gateOrigin) % period + period) % period;
                float env = 0.0f;
                if (g < static_cast<i64>(v.gateN)) {
                    const float up = static_cast<float>(g) / ramp, down = static_cast<float>(static_cast<i64>(v.gateN) - g) / ramp;
                    env = up < down ? (up < 1.0f ? up : 1.0f) : (down < 1.0f ? down : 1.0f);
                }
                const float fr = static_cast<float>(pos - k);
                const float s = (v.data[k] + (v.data[k + 1] - v.data[k]) * fr) * env;
                L[i] += s * v.gl;
                R[i] += s * v.gr;
                pos += v.rate;
            }
            v.pos = pos;
            if (ended) v.active = false;
        } else if (v.rate == 1.0f) {
            u32 pos = static_cast<u32>(v.pos);
            u32 cnt = n - i0;
            if (v.frames - pos < cnt) cnt = v.frames - pos;
            if (v.channels == 2) {
                const float* d = v.data + static_cast<size_t>(pos) * 2;
                for (u32 i = 0; i < cnt; ++i) {
                    L[i0 + i] += d[2 * i] * v.gl;
                    R[i0 + i] += d[2 * i + 1] * v.gr;
                }
            } else if (v.drive > 0.0f) {  // M5: the driven kick
                const float* d = v.data + pos;
                const float k = v.drive, norm = 1.0f / fastTanh(k);
                for (u32 i = 0; i < cnt; ++i) {
                    const float s = fastTanh(k * d[i]) * norm;
                    L[i0 + i] += s * v.gl;
                    R[i0 + i] += s * v.gr;
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
            // M5 (cost): the voice's state in locals (the bus stores could alias its gains), mono and stereo apart; the
            // same arithmetic in the same order as before.
            double pos = v.pos;
            const double rate = v.rate;
            const float* d = v.data;
            const float gl = v.gl, gr = v.gr;
            const u32 last = v.frames ? v.frames - 1 : 0;  // k + 1 >= frames
            bool ended = v.frames == 0;
            if (v.channels == 2) {
                for (u32 i = i0; i < n; ++i) {
                    const u32 k = static_cast<u32>(pos);
                    if (k >= last) {
                        ended = true;
                        break;
                    }
                    const float fr = static_cast<float>(pos - k);
                    L[i] += (d[2 * k] + (d[2 * k + 2] - d[2 * k]) * fr) * gl;
                    R[i] += (d[2 * k + 1] + (d[2 * k + 3] - d[2 * k + 1]) * fr) * gr;
                    pos += rate;
                }
            } else {
                for (u32 i = i0; i < n; ++i) {
                    const u32 k = static_cast<u32>(pos);
                    if (k >= last) {
                        ended = true;
                        break;
                    }
                    const float fr = static_cast<float>(pos - k);
                    const float s = d[k] + (d[k + 1] - d[k]) * fr;
                    L[i] += s * gl;
                    R[i] += s * gr;
                    pos += rate;
                }
            }
            v.pos = pos;
            if (ended) v.active = false;
        }
    }
    compactOrder(order_[PoolSample], smp_);
    mark(EngineProfile::Samples);
    renderContent(n);  // M5: live recipe voices and the upgrades' held voices
    mark(EngineProfile::Content);
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

    // M5 (CutBed): the buses a fight silences for a while (Event Horizon starved, the Moth Queen open).
    if (cutMask_) applyCuts(n);
    // Pad high-pass (musicproto: per chord before placing; linear, so on the bus).
    for (u32 ch = 0; ch < 2; ++ch) {
        float* x = bus_[static_cast<u32>(Bus::Pad)][ch];
        for (u32 i = 0; i < n; ++i) x[i] = padHp_[ch].tick(x[i]);
    }
    // Sidechain from the kick: e^(-t / tau) for 0.6 s after each kick the pattern plays (not each beat), depth per bus.
    {
        float* duck = duck_;
        duckFrames_ = n;
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
            if (echoAt_ >= 0 && frame_ + static_cast<i64>(i) >= echoAt_) {  // M5: Echo takes over the delay on its bar line
                echo_ = static_cast<u32>(echoPending_);
                echoAt_ = -1;
                applyDelay();
            }
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
    // M4 co-op, "music from host only": the bed (every music bus but the lead) ramps to silence on this device.
    {
        const float target = hostOnly_ ? 0.0f : 1.0f;
        if (bedGain_ != target || target < 1.0f) {
            const float from = bedGain_;
            bedGain_ = target > from ? std::fmin(target, from + kBedRampPerBlock) : std::fmax(target, from - kBedRampPerBlock);
            const float step = (bedGain_ - from) / static_cast<float>(n);
            for (u32 b = 0; b < kBusCount; ++b) {
                if (b == static_cast<u32>(Bus::Lead) || b == static_cast<u32>(Bus::Sfx)) continue;
                for (u32 ch = 0; ch < 2; ++ch) {
                    float* x = bus_[b][ch];
                    for (u32 i = 0; i < n; ++i) x[i] *= from + step * static_cast<float>(i + 1);
                }
            }
        }
    }
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
            if (sendUsed_) {  // M5: a content voice's send above the effects' (summed already scaled by its extra share)
                const float own = fx / kSfxReverbSend;
                for (u32 i = 0; i < n; ++i) {
                    inL[i] += sendL_[i] * own;
                    inR[i] += sendR_[i] * own;
                }
            }
            reverb_.process(inL, inR, wet_[0], wet_[1], n);
        }
    }
    mark(EngineProfile::Reverb);    // Master: sum, 30 Hz high-pass, section gain, soft clip, output gain; effects on top; limiter.
    const float drive = sp_->masterDrive;
    const u32 gc = gainCount_;
    // The pause's low-pass (M3 stage C): the cutoff runs from 18 kHz to muffleHz_ on a log scale as the
    // amount ramps; the coefficient is set once a block (a 150 ms ramp is fifteen 10 ms blocks).
    const bool muffled = muffle_ > 0.0f || muffleTarget_ > 0.0f;
    float muffleK = 1.0f;
    if (muffled) {
        const float u = muffle_ + (muffleTarget_ > muffle_ ? 0.5f : -0.5f) * std::fmin(muffleStep_ * static_cast<float>(n), std::fabs(muffleTarget_ - muffle_));
        const float hz = std::exp(std::log(18000.0f) + (std::log(muffleHz_) - std::log(18000.0f)) * u);
        muffleK = 1.0f - std::exp(-6.28318531f * hz / static_cast<float>(sr_));
    }
    const bool frozen = freezeMix_ > 0.0f || freezeTarget_ > 0.0f;  // M5: Freeze Frame's high-pass (50 ms fades)
    const float freezeStep = 1.0f / (0.05f * sr);
    if (frozen && freezeMix_ <= 0.0f) {
        freezeHp_[0].reset();
        freezeHp_[1].reset();
    }
    float stageIn[2][kBlock], stageOut[2][kBlock];  // (the stage tap's, offline)
    // M5 (HighPassBed: Cadence's Inversion): the music high-passed for its window, faded in and out over 50 ms.
    const bool bedHp = bedHpMix_ > 0.0f || (bedHpTo_ > frame_ && bedHpFrom_ < frame_ + static_cast<i64>(n));
    if (bedHp && bedHpMix_ <= 0.0f) {
        bedHp_[0].reset();
        bedHp_[1].reset();
    }
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
        if (muffled) {
            muffleZ_[0][0] += (l - muffleZ_[0][0]) * muffleK;
            muffleZ_[0][1] += (muffleZ_[0][0] - muffleZ_[0][1]) * muffleK;
            muffleZ_[1][0] += (r - muffleZ_[1][0]) * muffleK;
            muffleZ_[1][1] += (muffleZ_[1][0] - muffleZ_[1][1]) * muffleK;
            l = muffleZ_[0][1];
            r = muffleZ_[1][1];
        } else {
            muffleZ_[0][0] = muffleZ_[0][1] = l;  // the filter follows, so it engages without a step
            muffleZ_[1][0] = muffleZ_[1][1] = r;
        }
        if (frozen) {
            // M5 (conductor.json boosts.freeze_frame): the music high-passed while a Freeze Frame lasts, faded in and out.
            freezeMix_ = freezeTarget_ > freezeMix_ ? std::fmin(freezeTarget_, freezeMix_ + freezeStep) : std::fmax(freezeTarget_, freezeMix_ - freezeStep);
            const float hl = freezeHp_[0].tick(l), hr = freezeHp_[1].tick(r);
            l += (hl - l) * freezeMix_;
            r += (hr - r) * freezeMix_;
        }
        if (bedHp) {
            const i64 x = frame_ + static_cast<i64>(i);
            const float target = x >= bedHpFrom_ && x < bedHpTo_ ? 1.0f : 0.0f;
            bedHpMix_ = target > bedHpMix_ ? std::fmin(target, bedHpMix_ + freezeStep) : std::fmax(target, bedHpMix_ - freezeStep);
            const float hl = bedHp_[0].tick(l), hr = bedHp_[1].tick(r);
            l += (hl - l) * bedHpMix_;
            r += (hr - r) * bedHpMix_;
        }
        l += bus_[static_cast<u32>(Bus::Sfx)][0][i] * sfxGain_;
        r += bus_[static_cast<u32>(Bus::Sfx)][1][i] * sfxGain_;
        if (stageFn_) {
            stageIn[0][i] = l;
            stageIn[1][i] = r;
        }
        if (!rawOutput_) {
            // M5 audio: the look-ahead limiter (engine.h): in at frame t, out the frame kLimDelay before it.
            const i64 t = frame_ + static_cast<i64>(i);
            const u32 w = static_cast<u32>(t) & (kLimRing - 1);
            limBuf_[0][w] = l;
            limBuf_[1][w] = r;
            const float a = std::fmax(std::fabs(l), std::fabs(r));
            limPeak_ = a > limPeak_ ? a : limPeak_;
            const u32 phase = static_cast<u32>(t) & (kLimSeg - 1);
            if (phase == 0) {
                // An output segment begins: its gain, from the segments kLimHold before it to kLimAhead after it (all heard).
                const i64 seg = (t - static_cast<i64>(kLimDelay)) / static_cast<i64>(kLimSeg);
                float target = 1.0f;
                for (i64 s = seg - static_cast<i64>(kLimHold); s <= seg + static_cast<i64>(kLimAhead); ++s)
                    target = std::fmin(target, limReq_[static_cast<u32>(s) & (kLimReqs - 1)]);
                limPrev_ = limCur_;
                limCur_ = target < limCur_ ? target : limCur_ + (target - limCur_) * limRelease_;
            }
            const float g = limPrev_ + (limCur_ - limPrev_) * static_cast<float>(phase + 1) * (1.0f / static_cast<float>(kLimSeg));
            const u32 o = static_cast<u32>(t - static_cast<i64>(kLimDelay)) & (kLimRing - 1);
            l = limit(limBuf_[0][o] * g);
            r = limit(limBuf_[1][o] * g);
            if (phase == kLimSeg - 1) {  // an input segment is complete: the gain it may take
                limReq_[static_cast<u32>(t / static_cast<i64>(kLimSeg)) & (kLimReqs - 1)] = limPeak_ > kLimCeiling ? kLimCeiling / limPeak_ : 1.0f;
                limPeak_ = 0.0f;
            }
        }
        float* o = out + static_cast<size_t>(i) * channels;
        if (channels == 1) {
            o[0] = 0.5f * (l + r);
        } else {
            o[0] = l;
            o[1] = r;
            for (u32 c = 2; c < channels; ++c) o[c] = 0.0f;
        }
        if (stageFn_) {
            stageOut[0][i] = l;
            stageOut[1][i] = r;
        }
        peak_ = std::fmax(peak_, std::fmax(std::fabs(l), std::fabs(r)));
    }
    if (stageFn_) stageFn_(stageUser_, stageIn[0], stageIn[1], stageOut[0], stageOut[1], n);
    mark(EngineProfile::Master);
    if (muffle_ != muffleTarget_) {
        const float stepN = muffleStep_ * static_cast<float>(n);
        muffle_ = muffleTarget_ > muffle_ ? std::fmin(muffle_ + stepN, muffleTarget_) : std::fmax(muffle_ - stepN, muffleTarget_);
    }
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
