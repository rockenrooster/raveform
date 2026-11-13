// M5: the sound of the content (design/music/sound_m5.json, audio/cues.h): the game's cues, the fights' recipes and
// held voices, the timed low-pass; and the patterned sound of the upgrades (conductor.json "mods", "riffs", "boosts", which
// came from sound_m5.json; sound_m5.json "fusions"), played on the band's own clock from each player's loadout. Audio
// thread, like the rest of the engine: no allocation, lock, log or file access.
// The cues and recipes play on the effects bus (not ducked by the kick, sent to the reverb with the effects); the upgrades'
// patterned layers play in the music, on the bus of the voice they are named for (stab, clap, snare, pulse, lead, bass), or
// on the music's effects bus (swells, impacts, whooshes, drones), like the Riffs of the first slice.

#include <cmath>
#include <cstdlib>
#include <new>

#include "audio/engine.h"
#include "audio/engine_parts.h"

namespace rf::audio {

namespace {

// Each recipe's level in the effects mix (sound_m5.json gives levels relative to "the voice's own mix level"; these
// are the voices': engine starting values, the user tunes them by ear at the M5 play test).
float recipeLevel(Recipe r, const SoundParams& s) {
    switch (r) {
        case Recipe::Tom: return s.mix.pulse;         // the pulse voice
        case Recipe::SubDrop: return s.mix.impact;    // the impact's sine
        case Recipe::Gong: return s.mix.impact * 0.45f;  // as the boss's iris gong (M3)
        case Recipe::Bell: return 0.41f;              // the boss's glass bells: 0.55 x velocity 0.75 (M3)
        case Recipe::Zap: return sfxLevel(Sfx::KillMedium);
        case Recipe::Glass: return 0.15f;
        case Recipe::SawGlide: return 0.25f;
        case Recipe::Sweep: return sfxLevel(Sfx::Dash);
        case Recipe::RevCymbal: return s.mix.impact * 0.3f;
        case Recipe::Woodblock: return 0.2f;
        case Recipe::Chirp: return sfxLevel(Sfx::Shard);
        case Recipe::Drone: return 0.2f;              // the Sweeper's drone level (conductor.json riffs.sweeper)
        case Recipe::PadVoice: return s.mix.pad;
        case Recipe::Crackle: return 0.08f;           // Burn's crackle level (conductor.json mods.burn)
        case Recipe::Whoosh: return 0.3f;             // the Comet's whoosh level (riffs.comet)
        case Recipe::Roll: return s.mix.snare;
        default: return 0.2f;
    }
}

// Content voices: the game's held slots, the one-shot SawGlides, a Burn crackle a player, then the upgrades' own: the
// Sweeper's drone, a Beam drone a player, Standing Wave's held notes.
constexpr u32 kOneShotGlides = 8;
constexpr u32 kBurnVoices = kMaxPlayers;
constexpr u32 kSweeperVoice = kRecipeVoices + kOneShotGlides + kBurnVoices;
constexpr u32 kBeamVoice = kSweeperVoice + 1;
constexpr u32 kStandVoice = kBeamVoice + kMaxPlayers;
constexpr u32 kStandVoices = 8;
constexpr u32 kContentVoices = kStandVoice + kStandVoices;
constexpr float kBurnMaxDensity = 60.0f;  // bursts a second at thirty burning enemies (engine choice)
constexpr u32 kKnot = 32;                 // control rate: every 32 samples of a voice's age, whatever the callbacks

// Engine choices where the design gives no number (starting values, for the user's ear at the M5 play test):
constexpr float kDroneGlideS = 0.02f;        // a drone follows the chord's change over 20 ms
constexpr float kBeamCutoff = 850.0f;        // boosts.beam: the Sweeper's drone held about the middle of its sweep (300-2400 Hz)
constexpr float kStandingWave = 0.5f;        // fusions.standing_wave: the held pad note at half the pad voice's level
constexpr float kCannonStab = 1.2f, kCannonImpact = 0.5f;  // fusions.downbeat_cannon (as written)
constexpr float kMurmuration = 0.25f;        // fusions.murmuration (as written: the trill at 0.25, of the bell's own level)
constexpr float kQuartet = 0.4f;             // fusions.quartet (as written)
constexpr float kCollapseSwell = 0.5f;       // fusions.collapse_field: no level given (the Singularity's swell level)
constexpr float kSyncBlip = 0.3f;            // mods.syncopation's blip: no level given (Ricochet's blip, 0.3 of the lead)
constexpr float kSyncBlipLength = 0.3f;      // ... and as short (0.3 of the lead's length)
constexpr float kHeavyTailS = 0.15f;         // mods.heavy_step: the bass note's voice rings this long past its gate
constexpr float kStabMax = 1.0f;             // Accent's and Bar Line's stabs on one tick, summed, at most this velocity

float aggGain(u32 n) {  // sound_m5.json: 1 - e^(-count / 2.5), one event at its level, at most 1.6 x
    const float g = (1.0f - std::exp(-static_cast<float>(n) / 2.5f)) / (1.0f - std::exp(-1.0f / 2.5f));
    return g < 1.6f ? g : 1.6f;
}

constexpr u32 bit(Boost b) { return 1u << static_cast<u32>(b); }
constexpr u32 bit(Fusion f) { return 1u << static_cast<u32>(f); }
constexpr u32 ix(Mod m) { return static_cast<u32>(m); }
constexpr u32 ix(Riff r) { return static_cast<u32>(r); }

}  // namespace

struct Engine::ContentVoice {
    bool active = false, held = false, releasing = false;
    Recipe kind = Recipe::Drone;
    Bus bus = Bus::Sfx;   // M5: the upgrades' voices play in the music (Fx, Pad)
    u8 mode = 0;          // 1: the Sweeper's drone (its cutoff follows the arm round its turn)
    i16 note = -1;
    u32 delay = 0, age = 0, length = 0;  // one-shots end at length; held ones on stop
    i64 releaseAt = -1;   // the frame its release begins (-1: none planned)
    i64 retuneAt = -1;    // the frame it glides to retune[] (a drone following the chord)
    float retune[2] = {};
    float gl = 0.0f, gr = 0.0f, gain = 0.0f, gainTarget = 0.0f;
    float env = 0.0f, attackStep = 1.0f, releaseMul = 0.0f;
    float param = 0.0f, paramTarget = 0.0f;
    SawOsc saw[2];
    float glideMul = 1.0f;
    u32 glideLeft = 0;
    float glideTo[2] = {};  // > 0: the increments a glide lands on exactly
    SvfLowpass f;
    float k = 1.0f;
    SvfCoefs c{};
    // PadVoice: its filter's LFO; Crackle: the burst and its band-pass.
    float lfoW = 0.0f, fcBase = 0.0f, fcLfo = 0.0f;
    u32 burstLeft = 0, gapLeft = 0;
    float burstEnv = 0.0f, bp1 = 0.0f, bp2 = 0.0f;
    Random rng{1};
};

bool Engine::initContent() {
    content_ = new (std::nothrow) ContentVoice[kContentVoices];
    return content_ != nullptr;
}

void Engine::freeContent() {
    delete[] content_;
    content_ = nullptr;
}

void Engine::stopContent() {
    if (!content_) return;
    for (u32 i = 0; i < kContentVoices; ++i) content_[i].active = false;
    for (auto& row : cueCount_)
        for (u8& n : row) n = 0;
    for (float& b : burn_) b = 0.0f;
    bedFramesLeft_ = 0;
}

i32 Engine::chordRootMidi(i32 octave) const {
    const Chord& ch = currentChord();
    return 12 * (octave + 1) + ch.bass % 12;
}

float Engine::recipeGain(Recipe r) const { return recipeLevel(r, *sp_); }

float Engine::playerLevel(u32 player) const {
    const ConductorRules& r = music_->conductor;
    if (player == localPlayer_) return 1.0f;
    return player < kMaxPlayers && players_[player].distance > r.othersFarWu ? r.othersFarLevel : r.othersLevel;
}

// ---------------------------------------------------------------- notes against the chord and the scale

float Engine::scaleStep(float base, i32 degrees) const {
    const Track& t = *tr_;
    const i32 n = t.scaleCount ? static_cast<i32>(t.scaleCount) : 7;
    const i32 b = static_cast<i32>(std::lround(base));
    const i32 tonic = t.chords[t.progression[0]].bass % 12;
    const i32 pc = ((b - tonic) % 12 + 12) % 12;
    i32 idx = 0;
    for (i32 d = 0; d < n; ++d)
        if (t.scale[d] <= pc) idx = d;
    const i32 to = idx + degrees;
    const i32 oct = to >= 0 ? to / n : -((-to + n - 1) / n);
    return base + static_cast<float>(t.scale[to - oct * n] - t.scale[idx] + 12 * oct);
}

i32 Engine::chordTone(const Chord& ch, u32 k, i32 octave) const {
    // The chord's own tones above its bass (the pad and the stab): the third (a minor or major one), the fifth (perfect, else
    // diminished or augmented), the seventh (minor or major); where the chord has none, the Track's scale above the root.
    const i32 rootPc = ch.bass % 12;
    const i32 root = 12 * (octave + 1) + rootPc;
    u32 mask = 0;
    for (u32 i = 0; i < ch.padCount; ++i) mask |= 1u << (((ch.pad[i] - rootPc) % 12 + 12) % 12);
    for (u32 i = 0; i < ch.stabCount; ++i) mask |= 1u << (((ch.stab[i] - rootPc) % 12 + 12) % 12);
    const auto has = [&](i32 iv) { return (mask >> iv) & 1u; };
    switch (k) {
        case 0: return root;
        case 1: return has(3) ? root + 3 : (has(4) ? root + 4 : static_cast<i32>(scaleStep(static_cast<float>(root), 2)));
        case 2: return has(7) ? root + 7 : (has(6) ? root + 6 : (has(8) ? root + 8 : static_cast<i32>(scaleStep(static_cast<float>(root), 4))));
        case 3: return has(10) ? root + 10 : (has(11) ? root + 11 : static_cast<i32>(scaleStep(static_cast<float>(root), 6)));
        default: return root + 12;
    }
}

// ---------------------------------------------------------------- one-shots and held voices

void Engine::recipeShot(Recipe r, i32 note, float gain, float pan, i64 frame, Bus bus, u16 tag, float rate, double from) {
    const i32 id = bank_->recipe(r, note, tag);
    if (id < 0) return;
    const i32 baked = bank_->sample(id).midi;
    if (note >= 0 && baked >= 0 && baked != note) rate *= static_cast<float>(std::exp2((note - baked) / 12.0));
    playSample(id, bus, frame, gain, pan, rate, from);
}

void Engine::swellInto(double endTick, double startTick, float gain) {
    // The Track's bar-long reverse swell (conductor.json: the Break's), its last part over the span (stretched when the span
    // is longer), so it ends at full level on the tick it runs into.
    const i32 id = bank_->swellBar(track_);
    if (id < 0 || endTick <= startTick) return;
    const i64 f0 = frameOf(startTick);
    const double want = static_cast<double>(frameOf(endTick) - f0), have = bank_->sample(id).frames;
    const float rate = want > have ? static_cast<float>(have / want) : 1.0f;
    playSample(id, Bus::Fx, f0, gain, 0.0f, rate, want > have ? 0.0 : have - want);
}

void Engine::startContentVoice(ContentVoice& v, Recipe r, i32 midi, float gain, float pan, float param, float p1, float p2) {
    v = ContentVoice{};
    v.active = true;
    v.kind = r;
    v.note = static_cast<i16>(midi);
    v.rng.reseed(static_cast<u64>(frame_) * 31 + static_cast<u64>(r));
    const float sr = static_cast<float>(sr_);
    panGains(pan, gain * recipeLevel(r, *sp_), v.gl, v.gr);
    v.gain = v.gainTarget = 1.0f;
    v.attackStep = 1.0f / (0.005f * sr);
    v.releaseMul = std::exp(-1.0f / (0.15f * sr));
    v.param = v.paramTarget = param;
    const double beatS = 60.0 / trackBpm();
    switch (r) {
        case Recipe::SawGlide: {
            // A detuned saw pair through the lead's low-pass, gliding by p1 semitones over p2 beats, then let go.
            const double hz = midiHz(midi);
            const double det = std::pow(2.0, sp_->lead.detuneCents / 1200.0);
            v.saw[0].dt = static_cast<float>(hz / sr_);
            v.saw[1].dt = static_cast<float>(hz * det / sr_);
            v.saw[1].phase = 0.37f;
            const double glideS = (p2 > 0.0f ? p2 : 1.0f) * beatS;
            v.glideLeft = static_cast<u32>(glideS * sr);
            v.glideMul = static_cast<float>(std::exp2(static_cast<double>(p1) / 12.0 / (glideS * sr_)));
            v.length = v.glideLeft + framesFor(0.1, sr_);  // then let go (released over 150 ms)
            v.k = 1.0f / sp_->lead.q;
            v.param = v.paramTarget = sp_->lead.fcStart;
            break;
        }
        case Recipe::Drone: {
            // A detuned saw pair an octave under the bass (or the note given), low-passed at `param` Hz, held.
            const double hz = midiHz(midi);
            v.saw[0].dt = static_cast<float>(hz / sr_);
            v.saw[1].dt = static_cast<float>(hz * std::pow(2.0, 7.0 / 1200.0) / sr_);
            v.saw[1].phase = 0.5f;
            v.k = 1.0f / 0.9f;
            if (v.param <= 0.0f) v.param = v.paramTarget = 300.0f;
            v.attackStep = 1.0f / (0.05f * sr);
            v.releaseMul = std::exp(-1.0f / (0.3f * sr));
            break;
        }
        case Recipe::PadVoice: {
            // One note of the pad patch: its detuned pair, its LFO'd low-pass, its attack and release. M5 (choir.md): `param`
            // > 0 is the attack in seconds instead of the pad's own.
            const PadPatch& p = sp_->pad;
            const double hz = midiHz(midi);
            v.saw[0].dt = static_cast<float>(hz / sr_);
            v.saw[1].dt = static_cast<float>(hz * std::pow(2.0, p.detuneCents / 1200.0) / sr_);
            v.saw[1].phase = 0.21f;
            v.k = 1.0f / p.q;
            v.fcBase = p.fcBase;
            v.fcLfo = p.fcLfo;
            v.lfoW = static_cast<float>(2.0 * kPiD * p.lfoHz / sr_);
            v.attackStep = 1.0f / ((param > 0.0f ? param : p.attack) * sr);
            v.releaseMul = std::exp(-1.0f / (p.release * sr));
            break;
        }
        case Recipe::Crackle:
            // 5 ms bursts of 2-6 kHz noise, `param` bursts a second.
            v.k = 1.0f / 0.9f;
            v.c = svfCoefs(svfG(3500.0f, sr), v.k);
            break;
        default: v.active = false; break;
    }
}

void Engine::holdDrone(u32 slot, bool on, i32 midi, float cutoff, float gain, u8 mode, i64 frame) {
    ContentVoice& v = content_[slot];
    const bool sounding = v.active && !v.releasing && v.releaseAt < 0;
    if (!on) {
        if (sounding) v.releaseAt = frame;
        return;
    }
    if (!sounding) {
        startContentVoice(v, Recipe::Drone, midi, 1.0f, 0.0f, cutoff, 0.0f, 0.0f);
        panGains(0.0f, gain, v.gl, v.gr);
        v.bus = Bus::Fx;
        v.held = true;
        v.mode = mode;
        v.delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
        return;
    }
    panGains(0.0f, gain, v.gl, v.gr);
    if (midi != v.note) {
        const double hz = midiHz(midi);
        v.retune[0] = static_cast<float>(hz / sr_);
        v.retune[1] = static_cast<float>(hz * std::pow(2.0, 7.0 / 1200.0) / sr_);
        v.retuneAt = frame;
        v.note = static_cast<i16>(midi);
    }
}

void Engine::playRecipeAt(Recipe r, i32 midi, float gain, float pan, i64 frame, double tick, float p1, float p2) {
    const float level = recipeLevel(r, *sp_) * gain;
    const double beatS = 60.0 / trackBpm();
    const auto pitched = [&](i32 note, i32 bankNote) {
        const i32 id = bank_->recipe(r, bankNote);
        if (id < 0) return;
        const float rate = static_cast<float>(std::exp2((note - bank_->sample(id).midi) / 12.0));
        playSample(id, Bus::Sfx, frame, level, pan, rate);
    };
    switch (r) {
        case Recipe::Tom: {
            const i32 note = midi >= 0 ? midi : chordRootMidi(3);
            pitched(note, note);
            break;
        }
        case Recipe::Gong: {  // the bell two octaves under the note (default: the chord's root, octave 5)
            const i32 note = (midi >= 0 ? midi : chordRootMidi(5)) - 24;
            pitched(note, note);
            break;
        }
        case Recipe::Bell:
        case Recipe::Zap: {
            const i32 note = midi >= 0 ? midi : chordRootMidi(5);
            pitched(note, note);
            break;
        }
        case Recipe::Glass: {  // the note's pitch class in the seventh octave
            const i32 note = 96 + ((midi >= 0 ? midi : chordRootMidi(5)) % 12);
            pitched(note, note);
            break;
        }
        case Recipe::Woodblock: {
            const i32 id = bank_->recipe(r);
            if (id >= 0) playSample(id, Bus::Sfx, frame, level, pan, midi >= 0 ? static_cast<float>(std::exp2((midi - 83) / 12.0)) : 1.0f);
            break;
        }
        case Recipe::SubDrop: playSample(bank_->recipe(r), Bus::Sfx, frame, level, pan); break;
        case Recipe::Chirp: playSfx(Sfx::Shard, midi >= 0 ? midi : chordRootMidi(6), level, pan, frame, tick); break;
        case Recipe::Sweep: {
            if (p2 <= 0.0f) {  // the Dash itself
                playSample(bank_->sfx(Sfx::Dash), Bus::Sfx, frame, level, pan);
                break;
            }
            const i32 id = bank_->recipe(r, -1, p1 < 0.0f ? 1 : 0);  // baked over one second
            if (id >= 0) playSample(id, Bus::Sfx, frame, level, pan, static_cast<float>(1.0 / (p2 * beatS)));
            break;
        }
        case Recipe::Whoosh: {
            const bool back = p2 >= 1.0f;
            const i32 id = bank_->recipe(r, -1, back ? 1 : 0);  // one second out (and one back)
            if (id >= 0) playSample(id, Bus::Sfx, frame, level, pan, static_cast<float>(1.0 / ((p1 > 0.0f ? p1 : 1.0f) * beatS)));
            break;
        }
        case Recipe::RevCymbal: {
            // It ends on the frame given: the last p1 beats of the reversed crash (stretched when it is longer).
            const i32 id = bank_->recipe(r);
            if (id < 0) break;
            const double want = (p1 > 0.0f ? p1 : 1.0f) * beatS * sr_;
            const double have = bank_->sample(id).frames;
            const float rate = want > have ? static_cast<float>(have / want) : 1.0f;
            const double from = want > have ? 0.0 : have - want;
            playSample(id, Bus::Sfx, frame - static_cast<i64>(want), level, pan, rate, from);
            break;
        }
        case Recipe::Roll: {
            // The Build's roll over p1 beats ending on the frame given: four equal parts in quarters, eighths, sixteenths
            // and thirty-seconds, rising from 0.35 to 1 (conductor.json sections.build.snareRoll).
            const double beats = p1 > 0.0f ? p1 : 4.0;
            const double endTick = tick >= 0.0 ? tick : tempo_.tickAt(static_cast<double>(frame));
            const double startTick = endTick - beats * kPpq;
            static const double kDiv[4] = {96.0, 48.0, 24.0, 12.0};
            const ConductorRules& rules = music_->conductor;
            u32 hits = 0;  // each hit a sample voice waiting for its frame (at most 32 a roll)
            for (u32 part = 0; part < 4; ++part) {
                const double t0 = startTick + part * beats * kPpq / 4.0, t1 = t0 + beats * kPpq / 4.0;
                for (double t = t0; t < t1 - 1e-6 && hits < 32; t += kDiv[part], ++hits) {
                    const float u = static_cast<float>((t - startTick) / (beats * kPpq));
                    playSample(bank_->snare(), Bus::Sfx, frameOf(t), level * (rules.snareRollLo + (rules.snareRollHi - rules.snareRollLo) * u), pan);
                }
            }
            break;
        }
        case Recipe::SawGlide: {
            for (u32 i = kRecipeVoices; i < kRecipeVoices + kOneShotGlides; ++i) {
                ContentVoice& v = content_[i];
                if (v.active) continue;
                startContentVoice(v, r, midi >= 0 ? midi : chordRootMidi(3), gain, pan, 0.0f, p1, p2);
                v.delay = frame > frame_ ? static_cast<u32>(frame - frame_) : 0u;
                break;
            }
            break;
        }
        default: break;  // Drone, PadVoice, Crackle: held voices (startVoice)
    }
    emit(BeatKind::Sfx, frame, tick, static_cast<u8>(64 + static_cast<u32>(r)), 0, 0, level, 0, static_cast<double>(midi));
}

void Engine::applyContentCommand(const Command& c) {
    switch (c.type) {
        case CmdType::Cue: {
            if (c.a >= kCueCount || c.b >= kMaxPlayers) break;
            const Cue cue = static_cast<Cue>(c.a);
            // A Fusion replaces its ingredients' sounds (sound_m5.json fusions): Tesla Bloom takes Arc's zaps, Shard Halo
            // Shatter's glass, Collapse Field Gravity Rounds' thup.
            if ((cue == Cue::ArcChain && playerFusion(c.b, Fusion::TeslaBloom)) || (cue == Cue::ShatterBurst && playerFusion(c.b, Fusion::ShardHalo)) ||
                (cue == Cue::GravityHit && playerFusion(c.b, Fusion::CollapseField)))
                break;
            // "At most one sound of a kind on a 1/32 step" (sound_m5.json): the hits, bursts and strikes of every player are
            // counted together (in the first player's row); what follows a player's own lead, its Dash, its Nova, its hit,
            // its Burn stays the player's.
            const bool own = cue == Cue::ForkSplit || cue == Cue::RicochetBounce || cue == Cue::BurnCount || cue == Cue::AfterimageDecoy ||
                             cue == Cue::ShockstepDash || cue == Cue::LastLightNova || cue == Cue::CollectorBoost || cue == Cue::PlayerHit ||
                             cue == Cue::BlackHoleNova;
            const u8 slot = own ? c.b : 0;
            u8& n = cueCount_[slot][c.a];
            n = static_cast<u8>(n + c.c > 255 ? 255 : n + c.c);
            cueValue_[slot][c.a] = c.x;
            cuePan_[slot][c.a] = c.y;
            if (c.when == Timing::AtTick) flushCues(static_cast<i64>(-1) - static_cast<i64>(c.tick));  // at once, on its tick
            break;
        }
        case CmdType::Recipe: {
            if (c.a >= kRecipeCount) break;
            double tick = c.tick;
            if (c.when != Timing::AtTick) {
                tick = std::ceil(tempo_.tickAt(static_cast<double>(frame_)) / 12.0) * 12.0;
                // Without a tick, the ones that end on their tick start now and end p1 beats on.
                const Recipe r = static_cast<Recipe>(c.a);
                if (r == Recipe::Roll || r == Recipe::RevCymbal) tick += (c.z > 0.0f ? c.z : (r == Recipe::Roll ? 4.0 : 1.0)) * kPpq;
            }
            playRecipeAt(static_cast<Recipe>(c.a), c.c == 255 ? -1 : c.c, c.x, c.y, frameOf(tick), tick, c.z, c.level);
            break;
        }
        case CmdType::VoiceStart: {
            if (c.a >= kRecipeVoices || c.b >= kRecipeCount) break;
            const Recipe r = static_cast<Recipe>(c.b);
            i32 midi = c.c == 255 ? -1 : c.c;
            if (midi < 0) midi = r == Recipe::Drone ? chordRootMidi(1) : chordRootMidi(4);  // a drone: under the bass
            ContentVoice& v = content_[c.a];
            startContentVoice(v, r, midi, c.x, c.y, c.z, 0.0f, 0.0f);
            v.held = true;
            break;
        }
        case CmdType::VoiceSet:
            if (c.a < kRecipeVoices) {
                content_[c.a].paramTarget = c.x;
                if (c.y >= 0.0f) content_[c.a].gainTarget = c.y;
            }
            break;
        case CmdType::VoiceStop:
            if (c.a < kRecipeVoices && content_[c.a].active) {
                ContentVoice& v = content_[c.a];
                v.releasing = true;
                if (c.x > 0.0f) v.releaseMul = std::exp(-1.0f / (c.x * static_cast<float>(sr_)));
            }
            break;
        case CmdType::LowPassBed:
            muffleTarget_ = c.x > 0.0f ? 1.0f : 0.0f;
            if (c.x > 0.0f) muffleHz_ = c.x;
            muffleStep_ = 1.0f / (0.15f * static_cast<float>(sr_));
            bedFramesLeft_ = static_cast<i64>(static_cast<double>(c.y) * sr_);
            break;
        default: break;
    }
}

// The cues: on each 1/32 step (Storm Coil on each triplet eighth), one sound a kind a player, aggregated. A negative
// `tick` is a cue posted for a tick (AtTick): -1 - that tick, played at once. Levels: conductor.json "mods", "boosts".
void Engine::flushCues(i64 tickIn) {
    const bool atOnce = tickIn < 0;
    const double tick = atOnce ? static_cast<double>(-1 - tickIn) : static_cast<double>(tickIn);
    const bool on32 = atOnce || tickIn % 12 == 0, onTriplet = atOnce || tickIn % 32 == 0;
    const i64 f = frameOf(tick);
    const double beatTicks = static_cast<double>(kPpq);
    const Chord& ch = currentChord();
    const SoundParams& s = *sp_;
    const ConductorRules& r = music_->conductor;
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        for (u32 k = 0; k < kCueCount; ++k) {
            const u32 n = cueCount_[p][k];
            if (!n) continue;
            const Cue cue = static_cast<Cue>(k);
            if (cue == Cue::StormCoilStrike ? !onTriplet : !on32) continue;
            cueCount_[p][k] = 0;
            // (cost) The notes a teammate's lead adds (Fork's fifth, the bounce blips) are its own device's, like the extra
            // notes of its upgrades (leadUpgrades).
            if ((cue == Cue::ForkSplit || cue == Cue::RicochetBounce) && p != localPlayer_) continue;
            const float g = aggGain(n), value = cueValue_[p][k], pan = cuePan_[p][k];
            switch (cue) {
                case Cue::ForkSplit:  // the fifth above, a 1/32 later, at 0.4
                    playLead(static_cast<u8>(p), playerTimbre(p), static_cast<float>(playerLastNote(p) + 7), 0.8f, s.mix.lead * r.forkLevel * g, pan,
                             frameOf(tick + static_cast<double>(r.forkTicks)), 1.0f, 0.0f, f);
                    break;
                case Cue::HeavyStepBurst:
                case Cue::SupernovaImpact:  // the small kill's thud, at 0.5 / 0.3
                    playSfx(Sfx::KillSmall, ch.stab[0] + 12, sfxLevel(Sfx::KillSmall) * (cue == Cue::HeavyStepBurst ? r.heavyThud : r.supernovaLevel) * g,
                            pan, f, tick);
                    break;
                case Cue::ArcChain: {  // a zap one chord tone above the last, up to three (Tesla Bloom takes its part: posting)
                    const u32 step = value >= 1.0f ? static_cast<u32>(value) - 1u : 0u;
                    const u32 nn = ch.stabCount ? ch.stabCount : 1u;
                    playRecipeAt(Recipe::Zap, ch.stab[step % nn] + 12 * static_cast<i32>(step / nn), g, pan, f, tick, 0.0f, 0.0f);
                    break;
                }
                case Cue::BurnCount: {  // the crackle's density follows the burning (M5, cost: one crackle for every player's)
                    ContentVoice& v = content_[kRecipeVoices + kOneShotGlides];
                    burn_[p] = value > 0.0f ? value : 0.0f;
                    float burning = 0.0f;
                    for (float b : burn_) burning += b;
                    const float density = kBurnMaxDensity * (burning < 30.0f ? burning : 30.0f) / 30.0f;
                    if (density > 0.0f && !v.active) startContentVoice(v, Recipe::Crackle, -1, r.burnLevel / recipeLevel(Recipe::Crackle, s), 0.0f, density, 0.0f, 0.0f);
                    if (v.active) {
                        v.paramTarget = density;
                        v.held = true;
                        v.releasing = density <= 0.0f;
                    }
                    break;
                }
                case Cue::ShatterBurst:
                case Cue::FreezeShatter:  // a glass burst (Shatter at 0.25)
                    playRecipeAt(Recipe::Glass, ch.stab[0], (cue == Cue::ShatterBurst ? r.shatterLevel : 1.0f) * g, pan, f, tick, 0.0f, 0.0f);
                    break;
                case Cue::GravityHit: {  // the soft low thup at 0.15
                    const i32 id = bank_->recipe(Recipe::Tom, -1, 2);
                    if (id >= 0) playSample(id, Bus::Sfx, f, recipeLevel(Recipe::Tom, s) * r.gravityLevel * g, pan);
                    break;
                }
                case Cue::AfterimageDecoy:  // the Dash's sweep a beat later at half level, from the decoy
                    playSample(bank_->sfx(Sfx::Dash), Bus::Sfx, frameOf(tick + beatTicks), sfxLevel(Sfx::Dash) * 0.5f, pan);
                    break;
                case Cue::ShockstepDash:  // a tom on the root; a Perfect Dash a second a fifth above
                case Cue::ColossusRam:
                    playRecipeAt(Recipe::Tom, chordRootMidi(3), g, pan, f, tick, 0.0f, 0.0f);
                    if (cue == Cue::ShockstepDash && value >= 1.0f) playRecipeAt(Recipe::Tom, chordRootMidi(3) + 7, g, pan, f, tick, 0.0f, 0.0f);
                    break;
                case Cue::AegisBlock:  // conductor.json mods.aegis (f7cd516): 6 dB under the Shard chirp
                    playSfx(Sfx::Tick, -1, sfxLevel(Sfx::Shard) * std::pow(10.0f, -r.aegisDb / 20.0f) * g, pan, f, tick);
                    break;
                case Cue::LastLightNova: playSfx(Sfx::NovaBlast, -1, sfxLevel(Sfx::NovaBlast) * r.lastLightLevel, pan, f, tick); break;
                case Cue::CollectorBoost: playSfx(Sfx::Boost, -1, sfxLevel(Sfx::Boost), pan, f, tick); break;
                case Cue::RicochetBounce: {  // a blip a scale step above the last for each bounce (two under Ricochet Fever)
                    const u32 steps = static_cast<u32>(value > 1.0f ? value : 1.0f) * (playerBoost(p, Boost::RicochetFever) ? 2u : 1u);
                    const u32 sc = tr_->scaleCount ? tr_->scaleCount : 7;
                    const i32 tonic = tr_->chords[tr_->progression[0]].bass % 12;
                    const i32 pc = ((playerLastNote(p) - tonic) % 12 + 12) % 12;
                    u32 idx = 0;
                    for (u32 d = 0; d < sc; ++d)
                        if (tr_->scale[d] <= pc) idx = d;
                    const u32 to = idx + steps;
                    const float note = static_cast<float>(playerLastNote(p) + (tr_->scale[to % sc] - tr_->scale[idx]) + 12 * static_cast<i32>(to / sc));
                    playLead(static_cast<u8>(p), Timbre::P2Square, note, 0.8f, s.mix.lead * 0.3f * g, pan, f, 0.3f);
                    break;
                }
                case Cue::StormCoilStrike: {  // the stab triad and the root an octave up, in turn, an octave over the stab
                    const u32 nn = ch.stabCount ? ch.stabCount : 1u;
                    const u32 i = stormStep_++ % (nn + 1);
                    const i32 note = (i < nn ? ch.stab[i] : ch.stab[0] + 12) + 12;
                    playRecipeAt(Recipe::Zap, note, r.stormLevel * g, pan, f, tick, 0.0f, 0.0f);
                    break;
                }
                case Cue::ConfettiKill:  // a run of six Shard chirps up the scale, a 1/32 apart
                    for (u32 j = 0; j < 6; ++j)
                        playSfx(Sfx::Shard, bank_->shardScale(track_)[j], sfxLevel(Sfx::Shard) * g, pan, frameOf(tick + 12.0 * j), tick + 12.0 * j);
                    break;
                case Cue::BlackHoleNova: {  // the inhale over the singularity's three beats, then the blast with the sub drop
                    const i32 inh = bank_->inhale(track_, 8);  // a beat long at 8 thirty-seconds
                    if (inh >= 0) playSample(inh, Bus::Sfx, f, sfxLevel(Sfx::NovaInhale), 0.0f, 1.0f / 3.0f);
                    const i64 boom = frameOf(tick + 3.0 * beatTicks);
                    playSfx(Sfx::NovaBlast, -1, sfxLevel(Sfx::NovaBlast), 0.0f, boom, tick + 3.0 * beatTicks);
                    playSample(bank_->recipe(Recipe::SubDrop), Bus::Sfx, boom, recipeLevel(Recipe::SubDrop, s), 0.0f);
                    break;
                }
                case Cue::PlayerHit: crescendoHit(p); break;  // Crescendo: the lead's rise and the pad's opening fall at once
                default: break;
            }
        }
    }
}

// ---------------------------------------------------------------- the upgrades' patterned sound

void Engine::upgradeBar() {
    BarPlan& p = *plan_;
    const ConductorRules& r = music_->conductor;
    // What anyone owns. A Fusion replaces its ingredients' sounds (sound_m5.json fusions): Downbeat Cannon takes Lancer's
    // part, Murmuration Swarm's, Quartet Mirror's (the other ingredients are taken where they play).
    u8 riff[kRiffCount] = {};
    u16 boosts = 0;
    u8 fusions = 0;
    u32 echo = 0;
    bool kick = false, any = false;
    for (u32 i = 0; i < playerCount_; ++i) {
        const Player& pl = players_[i];
        boosts = static_cast<u16>(boosts | pl.boosts);
        fusions = static_cast<u8>(fusions | pl.fusions);
        for (u32 k = 0; k < kRiffCount; ++k) {
            u8 lv = pl.riff[k];
            if ((k == ix(Riff::Lancer) && (pl.fusions & bit(Fusion::DownbeatCannon))) || (k == ix(Riff::Swarm) && (pl.fusions & bit(Fusion::Murmuration))) ||
                (k == ix(Riff::Mirror) && (pl.fusions & bit(Fusion::Quartet))))
                lv = 0;
            if (lv > riff[k]) riff[k] = lv;
            any = any || (k >= ix(Riff::Lancer) && pl.riff[k]);
        }
        for (u32 m = 0; m < kModCount; ++m) {
            const Mod mm = static_cast<Mod>(m);
            const bool firstSlice = mm == Mod::Split || mm == Mod::DoubleTime || mm == Mod::Pierce || mm == Mod::Overcharge || mm == Mod::OnBeatCrit;
            any = any || (pl.mod[m] && !firstSlice);
        }
        if (!(pl.fusions & bit(Fusion::StandingWave)) && pl.mod[ix(Mod::Echo)] > echo) echo = pl.mod[ix(Mod::Echo)];
        kick = kick || pl.mod[ix(Mod::KickPulse)];
        // Crescendo: one more bar without a hit (a hit sets it back: crescendoHit).
        if (pl.mod[ix(Mod::Crescendo)]) {
            if (crescBars_[i] < 255) ++crescBars_[i];
        } else {
            crescBars_[i] = 0;
        }
    }
    const u16 firstSliceBoosts = static_cast<u16>(bit(Boost::Prism) | bit(Boost::Hyper) | bit(Boost::Halo) | bit(Boost::MagnetStorm));
    any = any || (boosts & ~firstSliceBoosts) || fusions;
    p.upgrades = any;
    for (u32 k = 0; k < kRiffCount; ++k) p.riff[k] = riff[k];
    p.boosts = boosts;
    p.fusions = fusions;
    const i64 barF = frameOf(static_cast<double>(p.tick));
    if (static_cast<i32>(echo) != (echoAt_ >= 0 ? echoPending_ : static_cast<i32>(echo_))) {  // Echo: on the bar line
        echoPending_ = static_cast<i32>(echo);
        echoAt_ = barF;
    }
    kickPulse_ = kick;
    if (!any) {
        stretchBar_ = 0;
        mineArmed_ = singArmed_ = false;
        holdDrone(kSweeperVoice, false, 0, 0.0f, 0.0f, 0, barF);
        for (u32 i = 0; i < kMaxPlayers; ++i) holdDrone(kBeamVoice + i, false, 0, 0.0f, 0.0f, 0, barF);
        return;
    }
    // Crescendo opens the pad with the local player's rise.
    if (localPlayer_ < playerCount_ && players_[localPlayer_].mod[ix(Mod::Crescendo)]) {
        const float db = std::fmin(r.crescendoMaxDb, r.crescendoDbPerBar * static_cast<float>(crescBars_[localPlayer_]));
        p.padOpen = std::pow(10.0f, db / 20.0f);
    }
    // Time Stretch: the drums' pattern over two bars.
    if (boosts & bit(Boost::TimeStretch)) {
        p.timeStretch = true;
        p.stretchHalf = static_cast<u8>(stretchBar_ % 2);
        ++stretchBar_;
    } else {
        stretchBar_ = 0;
    }
    if (!p.riffsOn) mineArmed_ = singArmed_ = false;  // the Riffs rest in the Break: no mine or well is left over
    const Chord& ch = *p.chord;
    // The Sweeper's drone, on the chord's bass note an octave down (recipes.drone), opening as the arm comes round.
    holdDrone(kSweeperVoice, p.riffsOn && riff[ix(Riff::Sweeper)], ch.bass - 12, r.sweepCutLo, r.sweepLevel, 1, barF);
    // Beam: each player's drone on the arpeggio's first note (conductor.json boosts.beam).
    for (u32 i = 0; i < kMaxPlayers; ++i) {
        const bool on = i < playerCount_ && playerBoost(i, Boost::Beam) && !(hostOnly_ && i != localPlayer_);
        holdDrone(kBeamVoice + i, on, ch.arp[0], kBeamCutoff, recipeLevel(Recipe::Drone, *sp_) * (on ? playerLevel(i) : 0.0f), 0, barF);
    }
}

void Engine::upgradeStep(i64 tick, i64 inBar) {
    const BarPlan& p = *plan_;
    const ConductorRules& r = music_->conductor;
    const SoundParams& s = *sp_;
    const Chord& ch = *p.chord;
    const double tk = static_cast<double>(tick);
    const i64 f = frameOf(tk);
    // Bar Line (conductor.json mods.bar_line): every bar, the cross is the chord's stab with a zap on the One.
    if (inBar == 0) {
        float zap = 0.0f;
        for (u32 i = 0; i < playerCount_; ++i) {
            if (!players_[i].mod[ix(Mod::BarLine)] || (hostOnly_ && i != localPlayer_)) continue;
            const float lv = playerLevel(i);
            stabAcc_ += r.barLineStab * lv;
            zap = std::fmax(zap, lv);
        }
        if (zap > 0.0f) {
            recipeShot(Recipe::Zap, chordTone(ch, 0, 5), recipeGain(Recipe::Zap) * zap, 0.0f, f, Bus::Pulse);
            upgradeEvent(UpgradeSound::BarLineZap, f, tk, recipeGain(Recipe::Zap) * zap, chordTone(ch, 0, 5));
        }
    }
    if (p.riffsOn) {
        // Riff patterns at double rate under Hyper: rt is the tick in the pattern's 16 steps.
        const i64 rate = p.riffDouble ? 2 : 1;
        const i64 rt = (inBar * rate) % kTicksPerBar;
        const bool step = rt % kTicksPerStep == 0;
        const u32 st = static_cast<u32>(rt / kTicksPerStep);
        // Lancer (riffs.lancer): the full chord on beat 4 (from level 3 on beats 2 and 4), twice the stab's length; its
        // charge a reverse swell into each shot from a beat after the last one (the three beats before, at level 1).
        if (const u32 lv = p.riff[ix(Riff::Lancer)]) {
            const float* pat = lv >= r.lancerFastFrom ? r.lancerFastPattern : r.lancerPattern;
            if (step && pat[st] > 0.0f) {  // (one stab a tick with the Mods' stabs: flushStabs)
                stabAcc_ += pat[st] * r.lancerLevel;
                stabLength_ = std::fmax(stabLength_, r.lancerLength);
                emit(BeatKind::Stab, f, tk, ch.stab[0], 1, 0, pat[st]);
                upgradeEvent(UpgradeSound::LancerChord, f, tk, pat[st] * r.lancerLevel, ch.stab[0]);
            }
            if (rt % 12 == 0) {
                for (u32 h = 0; h < 16; ++h) {
                    if (pat[h] <= 0.0f) continue;
                    u32 prev = h;
                    do prev = (prev + 15) % 16;
                    while (pat[prev] <= 0.0f && prev != h);
                    i64 gap = static_cast<i64>((h + 16 - prev) % 16) * kTicksPerStep;
                    if (gap == 0) gap = kTicksPerBar;
                    const i64 charge = gap > kPpq ? gap - kPpq : gap / 2;
                    const i64 start = ((static_cast<i64>(h) * kTicksPerStep - charge) % kTicksPerBar + kTicksPerBar) % kTicksPerBar;
                    if (start == rt) {
                        swellInto(tk + static_cast<double>(charge / rate), tk, s.mix.swell * r.lancerSwell);
                        upgradeEvent(UpgradeSound::LancerCharge, f, tk, s.mix.swell * r.lancerSwell, static_cast<double>(charge / rate));
                    }
                }
            }
        }
        // Minelayer (riffs.minelayer): a closed hat as each mine is laid; the last bar's mines go off on the One, a clap
        // and the impact. Collapse Field: a reverse swell over the last beat runs into the detonation.
        if (p.riff[ix(Riff::Minelayer)]) {
            if (step && r.minePattern[st] > 0.0f && mineArmed_) {
                const float v = r.minePattern[st];
                playSample(bank_->clap(), Bus::Clap, f, s.mix.clap * r.mineClap * v, 0.0f);
                playSample(bank_->impact(), Bus::Fx, f, s.mix.impact * r.mineImpact * v, 0.0f);
                upgradeEvent(UpgradeSound::MineBlast, f, tk, s.mix.impact * r.mineImpact * v);
                mineArmed_ = false;
            }
            if (step && st == r.mineHatStep) {
                playSample(bank_->kit(track_, variant_).hat, Bus::Drums, f, s.mix.hat, 0.0f);
                upgradeEvent(UpgradeSound::MineLaid, f, tk, s.mix.hat);
                mineArmed_ = true;
            }
            if ((p.fusions & bit(Fusion::CollapseField)) && mineArmed_ && rt == kTicksPerBar - kPpq) {
                swellInto(tk + static_cast<double>(kPpq / rate), tk, s.mix.swell * kCollapseSwell);
                upgradeEvent(UpgradeSound::CollapseSwell, f, tk, s.mix.swell * kCollapseSwell, static_cast<double>(kPpq / rate));
            }
        }
        // Swarm (riffs.swarm): the volley, one bell a missile from the One on the 1/32 grid, the chord's arp from its step.
        if (const u32 lv = p.riff[ix(Riff::Swarm)]) {
            const u32 count = r.swarmCount[(lv < 5 ? lv : 5) - 1];
            if (r.swarmGridTicks && rt % r.swarmGridTicks == 0) {
                const u32 kk = static_cast<u32>(rt / r.swarmGridTicks);
                if (kk < count) {
                    const u32 a = r.swarmFrom + kk;
                    const float pan = 0.6f * (2.0f * static_cast<float>(kk) / static_cast<float>(count > 1 ? count - 1 : 1) - 1.0f);
                    const i32 note = ch.arp[a % 16] + 12 * static_cast<i32>(a / 16);
                    recipeShot(Recipe::Bell, note, recipeGain(Recipe::Bell) * r.swarmLevel, pan, f, Bus::Stab);
                    upgradeEvent(UpgradeSound::SwarmBell, f, tk, recipeGain(Recipe::Bell) * r.swarmLevel, note);
                }
            }
        }
        // Murmuration (Swarm + Seeker): the run does not stop, a 1/32 trill between the chord's third and fifth.
        if ((p.fusions & bit(Fusion::Murmuration)) && inBar % 12 == 0) {
            const i32 note = chordTone(ch, (inBar / 12) % 2 ? 2 : 1, 5);
            recipeShot(Recipe::Bell, note, recipeGain(Recipe::Bell) * kMurmuration, (inBar / 12) % 2 ? 0.25f : -0.25f, f, Bus::Stab);
            upgradeEvent(UpgradeSound::MurmurationTrill, f, tk, recipeGain(Recipe::Bell) * kMurmuration, note);
        }
        // Singularity (riffs.singularity): every four bars a reverse swell over the last three beats while it pulls, the boom
        // on the next One (on bars whose song index is a multiple of four).
        if (p.riff[ix(Riff::Singularity)]) {
            const u32 every = r.singEveryBars ? r.singEveryBars : 1;
            const i64 swellTicks = static_cast<i64>(r.singSwellBeats) * kPpq;
            if (inBar == kTicksPerBar - swellTicks && (p.index + 1) % every == 0) {
                swellInto(static_cast<double>(p.tick + kTicksPerBar), tk, s.mix.swell * r.singSwell);
                upgradeEvent(UpgradeSound::SingularitySwell, f, tk, s.mix.swell * r.singSwell, static_cast<double>(swellTicks));
                singArmed_ = true;
            }
        }
        if (inBar == 0 && singArmed_) {
            playSample(bank_->impact(), Bus::Fx, f, s.mix.impact * r.singImpact, 0.0f);
            upgradeEvent(UpgradeSound::SingularityBoom, f, tk, s.mix.impact * r.singImpact);
            singArmed_ = false;
        }
        // Comet (riffs.comet): a whoosh out on the One, back on three, each over the time to the next.
        if (p.riff[ix(Riff::Comet)] && step && r.cometPattern[st] > 0.0f) {
            u32 nth = 0;
            for (u32 h = 0; h < st; ++h) nth += r.cometPattern[h] > 0.0f;
            u32 next = st;
            do next = (next + 1) % 16;
            while (r.cometPattern[next] <= 0.0f && next != st);
            const u32 gapSteps = (next + 16 - st) % 16 ? (next + 16 - st) % 16 : 16;
            const double secs = static_cast<double>(frameOf(tk + static_cast<double>(gapSteps * kTicksPerStep / rate)) - f) / sr_;
            const float rateW = static_cast<float>(1.0 / (secs > 0.01 ? secs : 0.01));
            recipeShot(Recipe::Whoosh, -1, r.cometLevel * r.cometPattern[st], nth % 2 ? 0.3f : -0.3f, f, Bus::Fx, nth % 2 ? 2 : 0, rateW);
            upgradeEvent(nth % 2 ? UpgradeSound::CometBack : UpgradeSound::CometOut, f, tk, r.cometLevel * r.cometPattern[st], rateW);
        }
        // Flak (riffs.flak): a snare with each burst on two and four; from level 4 a clap a 1/32 after it.
        if (const u32 lv = p.riff[ix(Riff::Flak)]; lv && step && r.flakPattern[st] > 0.0f) {
            const float v = r.flakPattern[st];
            playSample(bank_->snare(), Bus::Snare, f, s.mix.snare * r.flakLevel * v, 0.0f);
            emit(BeatKind::Snare, f, tk, 0, 1, 0, r.flakLevel * v);
            upgradeEvent(UpgradeSound::FlakSnare, f, tk, s.mix.snare * r.flakLevel * v);
            if (lv >= r.flakClapFrom) {
                const double ct = tk + static_cast<double>(r.flakClapTicks) / static_cast<double>(rate);
                playSample(bank_->clap(), Bus::Clap, frameOf(ct), s.mix.clap * r.flakLevel * v, 0.0f);
                upgradeEvent(UpgradeSound::FlakClap, frameOf(ct), ct, s.mix.clap * r.flakLevel * v);
            }
        }
        // Downbeat Cannon (Lancer + On-Beat Crit): on every One the full chord stab at 1.2 with the impact at 0.5.
        if ((p.fusions & bit(Fusion::DownbeatCannon)) && inBar == 0) {
            stabAcc_ += kCannonStab;
            stabMax_ = kCannonStab;  // the Cannon's chord may be louder than the Mods' stabs
            playSample(bank_->impact(), Bus::Fx, f, s.mix.impact * kCannonImpact, 0.0f);
            upgradeEvent(UpgradeSound::CannonOne, f, tk, kCannonStab, ch.stab[0]);
        }
    }
    // The Boosts' patterns (any player's), on the beats; the Build's last half bar keeps its silence.
    if (inBar % kPpq == 0 && !(p.leadStopsHalf && inBar >= kTicksPerBar / 2)) {
        const u32 beat = static_cast<u32>(inBar / kPpq);
        if (p.boosts & bit(Boost::ChainStorm)) {  // a zap on each of the chord's three stab notes
            static constexpr float kPan[3] = {0.0f, -0.4f, 0.4f};
            for (u32 k = 0; k < ch.stabCount && k < 3; ++k) {
                recipeShot(Recipe::Zap, ch.stab[k], recipeGain(Recipe::Zap) * r.chainStormLevel, kPan[k], f, Bus::Pulse);
                upgradeEvent(UpgradeSound::ChainStormZap, f, tk, recipeGain(Recipe::Zap) * r.chainStormLevel, ch.stab[k]);
            }
        }
        if (p.boosts & bit(Boost::Carousel)) {  // each launch a tom: root, third, fifth and octave on beats 1 to 4
            static constexpr u32 kTone[4] = {0, 1, 2, 4};
            const i32 note = chordTone(ch, kTone[beat & 3u], 3);
            recipeShot(Recipe::Tom, note, s.mix.pulse, 0.0f, f, Bus::Pulse);
            upgradeEvent(UpgradeSound::CarouselTom, f, tk, s.mix.pulse, note);
        }
    }
}

void Engine::leadUpgrades(u32 i, i64 tick, i64 inBar, u32 k, u8 note, float vel, float gain, float pan, float scale, float drive,
                          float level, float glideFrom) {
    const Player& pl = players_[i];
    const ConductorRules& r = music_->conductor;
    const BarPlan& p = *plan_;
    const double tk = static_cast<double>(tick);
    const i64 f = frameOf(tk);
    const u8 pid = static_cast<u8>(i);
    const auto lead = [&](UpgradeSound what, float midi, float g, double atTick) {
        const i64 at = frameOf(atTick);
        bendLead(playLead(pid, pl.timbre, midi, vel, g, pan, at, scale, drive, f), i, glideFrom, false);
        upgradeEvent(what, at, atTick, g, midi);
    };
    const float n = static_cast<float>(note);
    // (cost) A teammate's lead on this device is its note and its harmony (Split, Prism, On-Beat Crit), as loud as
    // conductor.json lead.others says; the extra notes and voices its upgrades bring are its own device's. Only the stab
    // its Accent asks for joins this device's (one stab a tick).
    if (i != localPlayer_) {
        if (const u32 acc = pl.mod[ix(Mod::Accent)]) {
            const i64 every = acc >= 3 ? kPpq : (acc == 2 ? kTicksPerBar / 2 : kTicksPerBar);
            if (inBar % every == 0 && !playerFusion(i, Fusion::StandingWave)) stabAcc_ += r.accentStab * level;
        }
        return;
    }
    // Mirror (riffs.mirror): the wingman is the player's own lead an octave up, louder each level. Quartet takes its part:
    // the note in four parts, the third, fifth and octave above at 0.4.
    if (p.riffsOn) {
        if (const u32 lv = pl.riff[ix(Riff::Mirror)]; lv && !playerFusion(i, Fusion::Quartet))
            lead(UpgradeSound::MirrorNote, n + static_cast<float>(r.mirrorTranspose), gain * r.mirrorLevel[(lv < 5 ? lv : 5) - 1], tk);
        if (playerFusion(i, Fusion::Quartet)) {
            lead(UpgradeSound::QuartetNote, scaleStep(n, 2), gain * kQuartet, tk);
            lead(UpgradeSound::QuartetNote, scaleStep(n, 4), gain * kQuartet, tk);
            lead(UpgradeSound::QuartetNote, n + 12.0f, gain * kQuartet, tk);
        }
    }
    // Chorus (boosts.chorus): a fifth above and an octave below.
    if (playerBoost(i, Boost::Chorus)) {
        lead(UpgradeSound::ChorusNote, scaleStep(n, 4), gain * r.chorusLevel, tk);
        lead(UpgradeSound::ChorusNote, n - 12.0f, gain * r.chorusLevel, tk);
    }
    // Fractal (boosts.fractal): the octave a 1/32 later, the double octave a 1/32 after that.
    if (playerBoost(i, Boost::Fractal)) {
        lead(UpgradeSound::FractalNote, n + 12.0f, gain * r.fractalOctave, tk + static_cast<double>(r.fractalTicks));
        lead(UpgradeSound::FractalNote, n + 24.0f, gain * r.fractalDouble, tk + 2.0 * static_cast<double>(r.fractalTicks));
    }
    // Syncopation (mods.syncopation): an off-beat note carries a short square blip an octave up.
    if (pl.mod[ix(Mod::Syncopation)] && inBar % kPpq != 0) {
        bendLead(playLead(pid, Timbre::P2Square, n + 12.0f, vel, gain * kSyncBlip, pan, f, kSyncBlipLength), i, glideFrom, false);
        upgradeEvent(UpgradeSound::SyncBlip, f, tk, gain * kSyncBlip, n + 12.0f);
    }
    // Heavy Step (mods.heavy_step): every fourth lead step doubled an octave down on the bass patch.
    if (pl.mod[ix(Mod::HeavyStep)] && r.heavyEvery && k % r.heavyEvery == r.heavyAt) {
        const u32 grid = p.leadGrid[i] ? p.leadGrid[i] : r.gridTicks[0];
        const float steps = static_cast<float>(grid) / static_cast<float>(kTicksPerStep);
        const double gateS = 60.0 / tr_->bpm / 4.0 * steps;
        fastBass(playBass(n - 12.0f, r.heavyLevel * level, steps, f, 1.0f, static_cast<float>(gateS) + kHeavyTailS));
        upgradeEvent(UpgradeSound::HeavyStepBass, f, tk, r.heavyLevel * level, n - 12.0f);
    }
    // Accent (mods.accent): the accented note's stab under it (the note itself plays louder: leadStep). Standing Wave
    // (Echo + Accent) takes its part: the accented note held for a bar as a pad voice on the same pitch.
    if (const u32 acc = pl.mod[ix(Mod::Accent)]) {
        const i64 every = acc >= 3 ? kPpq : (acc == 2 ? kTicksPerBar / 2 : kTicksPerBar);
        if (inBar % every == 0) {
            if (playerFusion(i, Fusion::StandingWave)) {
                ContentVoice& v = content_[kStandVoice + standNext_++ % kStandVoices];
                startContentVoice(v, Recipe::PadVoice, note, kStandingWave * level, pan, 0.0f, 0.0f, 0.0f);
                v.bus = Bus::Pad;
                v.delay = f > frame_ ? static_cast<u32>(f - frame_) : 0u;
                v.length = static_cast<u32>(frameOf(tk + static_cast<double>(kTicksPerBar)) - f);
                upgradeEvent(UpgradeSound::StandingWave, f, tk, kStandingWave * level, note);
            } else {
                stabAcc_ += r.accentStab * level;
            }
        }
    }
}

void Engine::rearGuardStep(u32 i, i64 tick, i64 inBar, float gain, float level) {
    // Rear Guard (conductor.json mods.rear_guard): a counter-melody two scale degrees below the arpeggio on the off-beat
    // eighths; the second stack adds the note five degrees below on steps 1 and 9. Quartet takes its part.
    (void)level;
    const Player& pl = players_[i];
    if (playerFusion(i, Fusion::Quartet)) return;
    const ConductorRules& r = music_->conductor;
    const BarPlan& p = *plan_;
    const u32 st = static_cast<u32>(inBar / kTicksPerStep);
    i32 degrees = 0;
    for (u8 x : r.rearSteps) degrees = x == st ? -2 : degrees;
    if (pl.mod[ix(Mod::RearGuard)] >= 2)
        for (u8 x : r.rearSteps2) degrees = x == st ? -5 : degrees;
    if (!degrees) return;
    const double tk = static_cast<double>(tick);
    const float vel = st % 2 == 0 ? r.accentEighth : r.accentOther;
    const float pan = -r.panDepth * std::sin(static_cast<float>(st) * r.panRate + static_cast<float>(p.index));  // behind: the other side
    const float scale = std::pow(r.pierceScale, static_cast<float>(pl.pierce));
    const float drive = pl.overcharge ? 1.0f + r.overchargeDrive * pl.overcharge : 0.0f;
    const float note = scaleStep(static_cast<float>(p.chord->arp[st]), degrees);
    bendLead(playLead(static_cast<u8>(i), pl.timbre, note, vel, gain * r.rearLevel, pan, frameOf(tk), scale, drive), i, 0.0f, false);
    upgradeEvent(UpgradeSound::RearGuardNote, frameOf(tk), tk, gain * r.rearLevel, note);
}

void Engine::kickUpgrades(float velocity, i64 frame, double tick) {
    // Kick Pulse (conductor.json mods.kick_pulse): a tom an octave above the kick's pitch doubles each kick the pattern plays.
    const i32 note = static_cast<i32>(std::lround(69.0 + 12.0 * std::log2(static_cast<double>(sp_->kickHz) / 440.0))) + 12;
    recipeShot(Recipe::Tom, note, sp_->mix.pulse * music_->conductor.kickPulseLevel * velocity, 0.0f, frame, Bus::Pulse);
    upgradeEvent(UpgradeSound::KickPulseTom, frame, tick, sp_->mix.pulse * music_->conductor.kickPulseLevel * velocity, note);
}

void Engine::boostChanged(u32 player, Boost b, bool on) {
    (void)player;
    if (b != Boost::FreezeFrame) return;
    // Freeze Frame (conductor.json boosts.freeze_frame): a glass chord as it starts; the music high-passed while any lasts.
    bool any = false;
    for (u32 i = 0; i < kMaxPlayers; ++i) any = any || playerBoost(i, Boost::FreezeFrame);
    freezeTarget_ = any ? 1.0f : 0.0f;
    if (!on) return;
    const double tick = std::ceil(tempo_.tickAt(static_cast<double>(frame_)) / 12.0) * 12.0;
    const Chord& ch = currentChord();
    for (u32 k = 0; k < ch.stabCount && k < 3; ++k)
        playRecipeAt(Recipe::Glass, ch.stab[k], 1.0f, k == 0 ? 0.0f : (k == 1 ? -0.35f : 0.35f), frameOf(tick), tick, 0.0f, 0.0f);
    upgradeEvent(UpgradeSound::FreezeGlass, frameOf(tick), tick, recipeLevel(Recipe::Glass, *sp_), ch.stab[0]);
}

void Engine::flushStabs(i64 frame) {
    // M5 (cost): the stabs the upgrades ask for on one tick (Lancer's chord, the Cannon's, Accent's and Bar Line's) are one
    // voice: their velocities summed (to kStabMax, or the Cannon's 1.2), the longest length asked.
    const float vel = std::fmin(stabAcc_, std::fmax(kStabMax, stabMax_));
    if (plan_ && plan_->chord) {
        playStab(*plan_->chord, vel, 0.0f, frame, stabLength_);
        upgradeEvent(UpgradeSound::ModStab, frame, tempo_.tickAt(static_cast<double>(frame)), vel, plan_->chord->stab[0]);
    }
    stabAcc_ = 0.0f;
    stabMax_ = 0.0f;
    stabLength_ = 1.0f;
}

// ---------------------------------------------------------------- the worst case (--band max)

static u32 g_maxDbg = 0;  // TEMP

u32 maxLoadSetup(Command* out, u32 cap) {
    u32 n = 0;
    const auto put = [&](const Command& c) {
        if (n < cap) out[n++] = c;
    };
    const auto cmd = [](CmdType t, u8 a = 0, u8 b = 0, u8 c = 0, float x = 0.0f) {
        Command k;
        k.type = t;
        k.a = a;
        k.b = b;
        k.c = c;
        k.x = x;
        return k;
    };
    put(cmd(CmdType::SetPlayers, 4, 0));
    put(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
    put(cmd(CmdType::SetSection, static_cast<u8>(Section::Drop)));
    put(cmd(CmdType::HoldSection, 1));
    // Four Riff slots a player (GAME_DESIGN 6.1): the eleven others shared out, Mirror with each (it doubles its own lead).
    static constexpr Riff kRiffs[4][4] = {{Riff::Orbitals, Riff::PulseRing, Riff::Sentinel, Riff::Mirror},
                                         {Riff::Lancer, Riff::Minelayer, Riff::StormCoil, Riff::Mirror},
                                         {Riff::Swarm, Riff::Singularity, Riff::Comet, Riff::Mirror},
                                         {Riff::Sweeper, Riff::Flak, Riff::Lancer, Riff::Mirror}};
    // The Mods that add sound, at their caps (GAME_DESIGN 6.2).
    struct Cap {
        Mod m;
        u8 stacks;
    };
    static constexpr Cap kMods[] = {{Mod::Split, 3},      {Mod::DoubleTime, 2}, {Mod::Pierce, 3},     {Mod::Overcharge, 5}, {Mod::OnBeatCrit, 3},
                                    {Mod::Seeker, 3},     {Mod::HeavyStep, 3},  {Mod::Accent, 3},     {Mod::Echo, 2},       {Mod::Wave, 2},
                                    {Mod::RearGuard, 2},  {Mod::Syncopation, 2}, {Mod::BarLine, 3},   {Mod::Crescendo, 2},  {Mod::KickPulse, 3},
                                    {Mod::Afterparty, 2}, {Mod::Fork, 2},       {Mod::Arc, 3},        {Mod::Burn, 3},       {Mod::Shatter, 3},
                                    {Mod::GravityRounds, 2}};
    // A Boost each: the heaviest on the lead (two more notes a step; Fractal's land later and overlap more).
    static constexpr Boost kBoost[4] = {Boost::Fractal, Boost::Fractal, Boost::Chorus, Boost::Prism};
    const char* dbg = std::getenv("RF_MAXDBG");  // TEMP
    const u32 dbgMask = dbg ? static_cast<u32>(std::atoi(dbg)) : 0u;
    g_maxDbg = dbgMask;
    if (dbgMask & 1u) out[0].a = 1;
    if (dbgMask & 256u) {  // the full band's own setup
        for (u8 p = 0; p < 4; ++p) {
            put(cmd(CmdType::SetFire, p, 1));
            put(cmd(CmdType::SetTimbre, p, p));
            for (u8 k = 0; k < 3; ++k) put(cmd(CmdType::SetRiff, p, k, 2));
            Command m = cmd(CmdType::SetMods, p);
            m.mods[0] = 1;
            m.mods[1] = 1;
            put(m);
        }
        return n;
    }
    for (u8 p = 0; p < 4; ++p) {
        if (dbgMask & 2u) {
            put(cmd(CmdType::SetFire, p, 1));
            put(cmd(CmdType::SetTimbre, p, p));
            continue;
        }
        if (dbgMask & 4u) {
            put(cmd(CmdType::SetFire, p, 1));
            put(cmd(CmdType::SetTimbre, p, p));
            u8 stacks[kModCount] = {};
            for (const Cap& c : kMods) stacks[static_cast<u32>(c.m)] = c.stacks;
            for (u32 first = 0; first < kModCount; first += 8) {
                Command m = cmd(CmdType::SetModStacks, p, static_cast<u8>(first), static_cast<u8>(kModCount - first < 8 ? kModCount - first : 8));
                for (u32 k = 0; k < m.c; ++k) m.mods[k] = stacks[first + k];
                put(m);
            }
            continue;
        }
        put(cmd(CmdType::SetFire, p, 1));
        put(cmd(CmdType::SetTimbre, p, p));
        for (const Riff rf : kRiffs[p]) put(cmd(CmdType::SetRiff, p, static_cast<u8>(rf), 5));
        u8 stacks[kModCount] = {};
        for (const Cap& c : kMods) stacks[static_cast<u32>(c.m)] = c.stacks;
        if (dbgMask & 8u) stacks[static_cast<u32>(Mod::Overcharge)] = 0;
        if (dbgMask & 16u) stacks[static_cast<u32>(Mod::Pierce)] = 0;
        if (dbgMask & 32u) {
            stacks[static_cast<u32>(Mod::DoubleTime)] = 1;
            stacks[static_cast<u32>(Mod::Afterparty)] = 0;
        }
        if (dbgMask & 64u) {
            stacks[static_cast<u32>(Mod::Wave)] = 0;
            stacks[static_cast<u32>(Mod::Seeker)] = 0;
        }
        if (dbgMask & 128u) stacks[static_cast<u32>(Mod::Split)] = 0;
        for (u32 first = 0; first < kModCount; first += 8) {
            Command m = cmd(CmdType::SetModStacks, p, static_cast<u8>(first), static_cast<u8>(kModCount - first < 8 ? kModCount - first : 8));
            for (u32 k = 0; k < m.c; ++k) m.mods[k] = stacks[first + k];
            put(m);
        }
        put(cmd(CmdType::SetBoost, p, static_cast<u8>(kBoost[p]), 1));
    }
    return n;
}

u32 maxLoadFrame(u32 frame, Command* out, u32 cap) {
    u32 n = 0;
    if (g_maxDbg & 1024u) return 0;  // TEMP
    const auto put = [&](CmdType t, u8 a, u8 b, u8 c, float x, Timing when = Timing::Now) {
        if (n >= cap) return;
        Command k;
        k.type = t;
        k.when = when;
        k.a = a;
        k.b = b;
        k.c = c;
        k.x = x;
        out[n++] = k;
    };
    // The full band's effects (Band::update's load test) ...
    put(CmdType::Kill, 0, 3, 0, 0.0f);
    if (frame % 4 == 0) {
        put(CmdType::Kill, 1, 1, 0, 0.0f);
        put(CmdType::Shard, 0, 0, 0, 1.0f);
    }
    if (frame % 30 == 0) put(CmdType::Kill, 2, 1, 0, 0.0f);
    if (g_maxDbg & 256u) return n;  // TEMP
    if (g_maxDbg & 512u) return n;  // TEMP
    // ... and the cues the upgrades bring: Storm Coil strikes, chains, forks, shrapnel, gravity hits, a crowd burning.
    put(CmdType::Cue, static_cast<u8>(Cue::StormCoilStrike), 1, 2, 0.0f, Timing::Next32);  // its owner's (maxLoadSetup)
    for (u8 p = 0; p < 4; ++p) {
        if ((frame + p) % 3 == 0) put(CmdType::Cue, static_cast<u8>(Cue::ArcChain), p, 1, static_cast<float>(1 + frame % 3), Timing::Next32);
        if ((frame + p) % 2 == 0) put(CmdType::Cue, static_cast<u8>(Cue::ForkSplit), p, 2, 0.0f, Timing::Next32);
        if ((frame + p) % 4 == 0) put(CmdType::Cue, static_cast<u8>(Cue::ShatterBurst), p, 2, 0.0f, Timing::Next32);
        if ((frame + p) % 2 == 1) put(CmdType::Cue, static_cast<u8>(Cue::GravityHit), p, 3, 0.0f, Timing::Next32);
        if (frame % 60 == p) put(CmdType::Cue, static_cast<u8>(Cue::BurnCount), p, 1, 20.0f, Timing::Next32);
    }
    return n;
}

// ---------------------------------------------------------------- rendering

void Engine::renderContent(u32 n) {
    if (bedFramesLeft_ > 0) {
        bedFramesLeft_ -= n;
        if (bedFramesLeft_ <= 0) muffleTarget_ = 0.0f;  // the low-pass bed opens again
    }
    const float sr = static_cast<float>(sr_);
    const ConductorRules& rules = music_->conductor;
    const bool script = mode_ == Mode::Script;
    for (u32 vi = 0; vi < kContentVoices; ++vi) {
        ContentVoice& v = content_[vi];
        if (!v.active) continue;
        if (v.delay >= n) {
            v.delay -= n;
            continue;
        }
        const u32 i0 = v.delay;
        v.delay = 0;
        const u32 bus = static_cast<u32>(script ? Bus::Sfx : v.bus);
        float* L = bus_[bus][0];
        float* R = bus_[bus][1];
        for (u32 i = i0; i < n; ++i) {
            const i64 at = frame_ + static_cast<i64>(i);
            if (v.retuneAt >= 0 && at >= v.retuneAt) {  // M5: a drone following the chord glides there over 20 ms
                const u32 glideN = framesFor(kDroneGlideS, sr_);
                v.glideMul = std::pow(v.retune[0] / v.saw[0].dt, 1.0f / static_cast<float>(glideN));
                v.glideLeft = glideN;
                v.glideTo[0] = v.retune[0];
                v.glideTo[1] = v.retune[1];
                v.retuneAt = -1;
            }
            if (v.releaseAt >= 0 && at >= v.releaseAt) {
                v.releasing = true;
                v.releaseAt = -1;
            }
            if (v.age % kKnot == 0) {
                // Control rate (M5: on the voice's own knots, so it renders the same whatever the callbacks): the parameter
                // glides toward its target, the filter follows; the Sweeper's cutoff from where its arm is.
                v.param += (v.paramTarget - v.param) * 0.1f;
                v.gain += (v.gainTarget - v.gain) * 0.1f;
                if (v.kind == Recipe::Drone) {
                    if (v.mode == 1) {
                        // riffs.sweeper: the arm turns once in turnBars bars and stands at twelve o'clock on the One of every
                        // turnBars-th bar of the song; the drone opens as it comes round and closes behind it.
                        const double turn = static_cast<double>(rules.sweepTurnBars ? rules.sweepTurnBars : 1) * kTicksPerBar;
                        const double u = std::fmod(tempo_.tickAt(static_cast<double>(at)), turn) / turn;
                        const double open = 0.5 + 0.5 * std::cos(2.0 * kPiD * u);
                        v.param = v.paramTarget = static_cast<float>(rules.sweepCutLo * std::pow(rules.sweepCutHi / rules.sweepCutLo, open * open));
                    }
                    v.c = svfCoefs(svfG(v.param, sr), v.k);
                } else if (v.kind == Recipe::SawGlide) {
                    const LeadPatch& lp = sp_->lead;
                    const float fc = lp.fcEnd + (lp.fcStart - lp.fcEnd) * std::exp(-static_cast<float>(v.age) / (lp.fcTau * sr));
                    v.c = svfCoefs(svfG(fc, sr), v.k);
                } else if (v.kind == Recipe::PadVoice) {
                    const float lfo = 0.5f + 0.5f * std::sin(v.lfoW * static_cast<float>(v.age));
                    v.c = svfCoefs(svfG(v.fcBase + v.fcLfo * lfo, sr), v.k);
                }
            }
            float x = 0.0f;
            if (v.kind == Recipe::Crackle) {
                // M5 (cost): the bursts as a Poisson process drawn burst by burst (the gap to the next one exponential at the
                // density), not a draw every sample.
                if (!v.burstLeft) {
                    if (v.gapLeft) {
                        --v.gapLeft;
                    } else if (v.param > 0.0f) {
                        v.burstLeft = static_cast<u32>(0.005f * sr);
                        v.burstEnv = 1.0f;
                        const double u = v.rng.uniform();
                        v.gapLeft = static_cast<u32>(-std::log(1.0 - u) * sr / v.param);
                    }
                }
                if (v.burstLeft) {
                    --v.burstLeft;
                    // M5 (cost): uniform noise at unit variance (a Gaussian draw cost a log, a square root and a sine a
                    // sample); band-passed, the same hiss.
                    const float in = static_cast<float>(v.rng.uniform() * 2.0 - 1.0) * 1.7320508f * v.burstEnv;
                    v.burstEnv *= 0.99f;
                    const float v3 = in - v.bp2, v1 = v.c.a1 * v.bp1 + v.c.a2 * v3, v2 = v.bp2 + v.c.a2 * v.bp1 + v.c.a3 * v3;
                    v.bp1 = 2.0f * v1 - v.bp1;
                    v.bp2 = 2.0f * v2 - v.bp2;
                    x = v1;  // the band-pass output
                }
            } else {
                const float raw = 0.5f * (v.saw[0].next() + v.saw[1].next());
                x = v.f.tick(raw, v.c.a1, v.c.a2, v.c.a3);
                if (v.glideLeft) {
                    --v.glideLeft;
                    v.saw[0].dt *= v.glideMul;
                    v.saw[1].dt *= v.glideMul;
                    if (!v.glideLeft && v.glideTo[0] > 0.0f) {  // landed: exactly on the note
                        v.saw[0].dt = v.glideTo[0];
                        v.saw[1].dt = v.glideTo[1];
                        v.glideTo[0] = 0.0f;
                    }
                }
            }
            // Envelope: attack, hold, release (held voices on stop; a glide 0.1 s after it arrives).
            const bool letGo = v.releasing || (v.length && v.age >= v.length);
            if (letGo) {
                v.env *= v.releaseMul;
            } else if (v.env < 1.0f) {
                v.env = v.env + v.attackStep < 1.0f ? v.env + v.attackStep : 1.0f;
            }
            const float y = x * v.env * v.gain;
            L[i] += y * v.gl;
            R[i] += y * v.gr;
            ++v.age;
            if (letGo && v.env < 1e-4f) {
                v.active = false;
                break;
            }
        }
    }
}

}  // namespace rf::audio
