// M5: the sound of the content (design/music/sound_m5.json, audio/cues.h): the game's cues, the fights' recipes and
// held voices, the timed low-pass. Audio thread, like the rest of the engine: no allocation, lock, log or file access.
// Everything here plays on the effects bus (not ducked by the kick, sent to the reverb with the effects).

#include <cmath>
#include <new>

#include "audio/engine.h"

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
        case Recipe::Drone: return 0.2f;              // the Sweeper's drone level (sound_m5.json riffs.sweeper)
        case Recipe::PadVoice: return s.mix.pad;
        case Recipe::Crackle: return 0.08f;           // Burn's crackle level (sound_m5.json mods.burn)
        case Recipe::Whoosh: return 0.3f;             // the Comet's whoosh level (riffs.comet)
        case Recipe::Roll: return s.mix.snare;
        default: return 0.2f;
    }
}

constexpr u32 kOneShotGlides = 8;    // one-shot SawGlides after the game's held slots
constexpr u32 kBurnVoices = kMaxPlayers;  // one Burn crackle a player
constexpr u32 kContentVoices = kRecipeVoices + kOneShotGlides + kBurnVoices;
constexpr float kBurnMaxDensity = 60.0f;  // bursts a second at thirty burning enemies (engine choice)

float aggGain(u32 n) {  // sound_m5.json: 1 - e^(-count / 2.5), one event at its level, at most 1.6 x
    const float g = (1.0f - std::exp(-static_cast<float>(n) / 2.5f)) / (1.0f - std::exp(-1.0f / 2.5f));
    return g < 1.6f ? g : 1.6f;
}

}  // namespace

struct Engine::ContentVoice {
    bool active = false, held = false, releasing = false;
    Recipe kind = Recipe::Drone;
    u32 delay = 0, age = 0, length = 0;  // one-shots end at length; held ones on stop
    float gl = 0.0f, gr = 0.0f, gain = 0.0f, gainTarget = 0.0f;
    float env = 0.0f, attackStep = 1.0f, releaseMul = 0.0f;
    float param = 0.0f, paramTarget = 0.0f;
    SawOsc saw[2];
    float glideMul = 1.0f;
    u32 glideLeft = 0;
    SvfLowpass f;
    float k = 1.0f;
    SvfCoefs c{};
    // PadVoice: its filter's LFO; Crackle: the burst and its band-pass.
    float lfoW = 0.0f, fcBase = 0.0f, fcLfo = 0.0f;
    u32 burstLeft = 0;
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
    bedFramesLeft_ = 0;
}

i32 Engine::chordRootMidi(i32 octave) const {
    const Chord& ch = currentChord();
    return 12 * (octave + 1) + ch.bass % 12;
}

void Engine::startContentVoice(ContentVoice& v, Recipe r, i32 midi, float gain, float pan, float param, float p1, float p2) {
    v = ContentVoice{};
    v.active = true;
    v.kind = r;
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
            // One note of the pad patch: its detuned pair, its LFO'd low-pass, its attack and release.
            const PadPatch& p = sp_->pad;
            const double hz = midiHz(midi);
            v.saw[0].dt = static_cast<float>(hz / sr_);
            v.saw[1].dt = static_cast<float>(hz * std::pow(2.0, p.detuneCents / 1200.0) / sr_);
            v.saw[1].phase = 0.21f;
            v.k = 1.0f / p.q;
            v.fcBase = p.fcBase;
            v.fcLfo = p.fcLfo;
            v.lfoW = static_cast<float>(2.0 * kPiD * p.lfoHz / sr_);
            v.attackStep = 1.0f / (p.attack * sr);
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
            u8& n = cueCount_[c.b][c.a];
            n = static_cast<u8>(n + c.c > 255 ? 255 : n + c.c);
            cueValue_[c.b][c.a] = c.x;
            cuePan_[c.b][c.a] = c.y;
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
// `tick` is a cue posted for a tick (AtTick): -1 - that tick, played at once.
void Engine::flushCues(i64 tickIn) {
    const bool atOnce = tickIn < 0;
    const double tick = atOnce ? static_cast<double>(-1 - tickIn) : static_cast<double>(tickIn);
    const bool on32 = atOnce || tickIn % 12 == 0, onTriplet = atOnce || tickIn % 32 == 0;
    const i64 f = frameOf(tick);
    const double beatTicks = static_cast<double>(kPpq);
    const Chord& ch = currentChord();
    const SoundParams& s = *sp_;
    for (u32 p = 0; p < kMaxPlayers; ++p) {
        for (u32 k = 0; k < kCueCount; ++k) {
            const u32 n = cueCount_[p][k];
            if (!n) continue;
            const Cue cue = static_cast<Cue>(k);
            if (cue == Cue::StormCoilStrike ? !onTriplet : !on32) continue;
            cueCount_[p][k] = 0;
            const float g = aggGain(n), value = cueValue_[p][k], pan = cuePan_[p][k];
            switch (cue) {
                case Cue::ForkSplit:  // the fifth above, a 1/32 later, at 0.4
                    playLead(static_cast<u8>(p), playerTimbre(p), static_cast<float>(playerLastNote(p) + 7), 0.8f, s.mix.lead * 0.4f * g, pan,
                             frameOf(tick + 12.0));
                    break;
                case Cue::HeavyStepBurst:
                case Cue::SupernovaImpact:  // the small kill's thud, at 0.5 / 0.3
                    playSfx(Sfx::KillSmall, ch.stab[0] + 12, sfxLevel(Sfx::KillSmall) * (cue == Cue::HeavyStepBurst ? 0.5f : 0.3f) * g, pan, f, tick);
                    break;
                case Cue::ArcChain: {  // a zap one chord tone above the last, up to three
                    const u32 step = value >= 1.0f ? static_cast<u32>(value) - 1u : 0u;
                    const u32 nn = ch.stabCount ? ch.stabCount : 1u;
                    playRecipeAt(Recipe::Zap, ch.stab[step % nn] + 12 * static_cast<i32>(step / nn), g, pan, f, tick, 0.0f, 0.0f);
                    break;
                }
                case Cue::BurnCount: {  // the crackle's density follows the burning
                    ContentVoice& v = content_[kRecipeVoices + kOneShotGlides + p];
                    const float density = kBurnMaxDensity * (value < 30.0f ? value : 30.0f) / 30.0f;
                    if (density > 0.0f && !v.active) startContentVoice(v, Recipe::Crackle, -1, 1.0f, pan, density, 0.0f, 0.0f);
                    if (v.active) {
                        v.paramTarget = density;
                        v.held = true;
                        v.releasing = density <= 0.0f;
                    }
                    break;
                }
                case Cue::ShatterBurst:
                case Cue::FreezeShatter:  // a glass burst (Shatter at 0.25)
                    playRecipeAt(Recipe::Glass, ch.stab[0], (cue == Cue::ShatterBurst ? 0.25f : 1.0f) * g, pan, f, tick, 0.0f, 0.0f);
                    break;
                case Cue::GravityHit: {  // the soft low thup at 0.15
                    const i32 id = bank_->recipe(Recipe::Tom, -1, 2);
                    if (id >= 0) playSample(id, Bus::Sfx, f, recipeLevel(Recipe::Tom, s) * 0.15f * g, pan);
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
                case Cue::AegisBlock:  // sound_m5.json (f7cd516): 6 dB under the Shard chirp
                    playSfx(Sfx::Tick, -1, sfxLevel(Sfx::Shard) * 0.5012f * g, pan, f, tick);
                    break;
                case Cue::LastLightNova: playSfx(Sfx::NovaBlast, -1, sfxLevel(Sfx::NovaBlast) * 0.6f, pan, f, tick); break;
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
                    playRecipeAt(Recipe::Zap, note, 0.25f * g, pan, f, tick, 0.0f, 0.0f);
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
                case Cue::PlayerHit:  // Crescendo falls back (the lead's and the pad's rise: with the Mods)
                default: break;
            }
        }
    }
}

void Engine::renderContent(float* L, float* R, u32 n) {
    if (bedFramesLeft_ > 0) {
        bedFramesLeft_ -= n;
        if (bedFramesLeft_ <= 0) muffleTarget_ = 0.0f;  // the low-pass bed opens again
    }
    const float sr = static_cast<float>(sr_);
    for (u32 vi = 0; vi < kContentVoices; ++vi) {
        ContentVoice& v = content_[vi];
        if (!v.active) continue;
        if (v.delay >= n) {
            v.delay -= n;
            continue;
        }
        const u32 i0 = v.delay;
        v.delay = 0;
        // Control rate once a block: the parameter glides toward its target, the filter follows.
        v.param += (v.paramTarget - v.param) * 0.1f;
        v.gain += (v.gainTarget - v.gain) * 0.1f;
        if (v.kind == Recipe::Drone) v.c = svfCoefs(svfG(v.param, sr), v.k);
        if (v.kind == Recipe::SawGlide) {
            const LeadPatch& lp = sp_->lead;
            const float fc = lp.fcEnd + (lp.fcStart - lp.fcEnd) * std::exp(-static_cast<float>(v.age) / (lp.fcTau * sr));
            v.c = svfCoefs(svfG(fc, sr), v.k);
        }
        if (v.kind == Recipe::PadVoice) {
            const float lfo = 0.5f + 0.5f * std::sin(v.lfoW * static_cast<float>(v.age));
            v.c = svfCoefs(svfG(v.fcBase + v.fcLfo * lfo, sr), v.k);
        }
        for (u32 i = i0; i < n; ++i) {
            float x = 0.0f;
            if (v.kind == Recipe::Crackle) {
                if (!v.burstLeft && v.rng.uniform() < v.param / sr) {
                    v.burstLeft = static_cast<u32>(0.005f * sr);
                    v.burstEnv = 1.0f;
                }
                if (v.burstLeft) {
                    --v.burstLeft;
                    const float in = static_cast<float>(v.rng.gaussian()) * v.burstEnv;
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
