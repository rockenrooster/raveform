// raveform_headless: the band without a window (TECH_SPEC 6.7). Runs the same engine against a simulated
// device (fixed or random burst sizes) and a scripted MixState timeline, and writes a WAV plus an event
// log; also the loudness calibration and the listening sheets.
//
//   raveform_headless --render staged --track track01 --sound club --out x.wav [--events x.csv] [--stems dir]
//   raveform_headless --render mock|sfx|leads|full|surge|medley --out x.wav [--pcm16]
//     (surge: the Surge listening test, Groove 4 / Build 4 / Drop 8 twice, Surge one grid step then two;
//      full: the load test on --track; medley (M5): about 20 s of every Track's Drop with the real Track
//      changes between them, Break -> Build -> Drop, driven bar by bar as the game's director does)
//   raveform_headless --render recipes|cues|riffs|mods|boosts|fusions --out x.wav   (M5 listening sheets, with x.txt)
//   raveform_headless --render max --track 10 [--profile]   (M5: the upgrades' worst case, as --band max on a device)
//   raveform_headless --render upgrade-levels               (M5: each sheet entry's own level in the mix)
//   raveform_headless --calibrate [--write src/audio/loudness_calibration.json] [--check]
//
// Exit codes: 0 ok, 1 failure, 2 bad arguments, 3 calibration out of date (--check).

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "audio/bake.h"
#include "audio/engine.h"
#include "audio/fft.h"
#include "audio/reverb.h"
#include "audio/music_data.h"
#include "audio/wav.h"
#include "core/alloc_counter.h"
#include "core/log.h"

using namespace rf;
using namespace rf::audio;

namespace {

constexpr u32 kRate = 48000;

struct Options {
    const char* render = "";
    const char* track = "track01";
    const char* sound = nullptr;
    const char* out = nullptr;
    const char* events = nullptr;
    const char* stems = nullptr;
    const char* meta = nullptr;
    const char* write = nullptr;
    const char* dumpSamples = nullptr;
    const char* dumpReverb = nullptr;
    const char* seedTarget = nullptr;  // --seed-search kick --target "sub,bass,lowmid,mid,himid,high"
    bool calibrate = false;
    bool check = false;
    bool pcm16 = false;
    bool randomBursts = false;
    bool raw = false;
    bool profile = false;
    i32 timbre = -1;        // M5: --timbre 0..3: every player of --render full on this lead (profiling)
    u32 burst = 192;
    float seconds = 0.0f;
    u64 seed = 1;
};

bool eq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

// ---------------------------------------------------------------- simulated device + main thread

struct Script;
using ScriptFn = void (*)(Script& s, double songSeconds);

struct Stems {
    FILE* f[kBusCount + 1] = {};
    u64 frames = 0;
};

void stemTap(void* user, const float* const* busL, const float* const* busR, const float* wetL, const float* wetR, u32 n) {
    auto* s = static_cast<Stems*>(user);
    float tmp[2 * kBlock];
    for (u32 b = 0; b <= kBusCount; ++b) {
        if (!s->f[b]) continue;
        const float* l = b < kBusCount ? busL[b] : wetL;
        const float* r = b < kBusCount ? busR[b] : wetR;
        for (u32 i = 0; i < n; ++i) {
            tmp[2 * i] = l[i];
            tmp[2 * i + 1] = r[i];
        }
        std::fwrite(tmp, sizeof(float), 2 * n, s->f[b]);
    }
    s->frames += n;
}

struct Render {
    Engine* engine = nullptr;
    float* out = nullptr;
    u64 frames = 0;
    u64 done = 0;
    FILE* events = nullptr;
    u64 eventCount = 0;
    Random rng{12345};
    Options opt;
    double nextMainS = 0.0;
    // What the engine planned, from its events (scripts that react to the bar line).
    i32 lastBar = -1;      // the last bar planned
    i32 scriptedBar = -1;  // the last bar a script reacted to
    struct SectionStart {
        Section section;
        u32 bar;
        double seconds;
    } sections[64];
    u32 sectionCount = 0;
    // M5 (medley): a command posted when the render reaches this frame (-1: none).
    i64 postAt = -1;
    Command postCmd;
    // M5 (medley): where each Track took over (Track events) and its tempo.
    struct TrackStart {
        u32 track, bar;
        double seconds;
    } trackStarts[16];
    u32 trackStartCount = 0;
    // M5 (render max): lead voices per player, summed and the most, sampled once a 1/60 s.
    u64 leadSum[4] = {};
    u32 leadMax[4] = {};
    u64 leadSamples = 0;
};

void drainEvents(Render& r) {
    BeatEvent e;
    while (r.engine->popEvent(e)) {
        ++r.eventCount;
        if (e.kind == BeatKind::Bar) r.lastBar = static_cast<i32>(e.tick / kTicksPerBar);
        if (e.kind == BeatKind::Section && r.sectionCount < countOf(r.sections))
            r.sections[r.sectionCount++] = {static_cast<Section>(e.a), static_cast<u32>(e.tick / kTicksPerBar),
                                            static_cast<double>(e.frame) / kRate};
        if (e.kind == BeatKind::Track && r.trackStartCount < countOf(r.trackStarts))
            r.trackStarts[r.trackStartCount++] = {e.a, static_cast<u32>(e.tick / kTicksPerBar), static_cast<double>(e.frame) / kRate};
        // x in full precision: a tempo row's ticks per frame rebuilds the tempo map (audio_check.py tempo-onsets).
        if (r.events)
            std::fprintf(r.events, "%lld,%.6f,%s,%u,%u,%u,%.6f,%u,%.17g\n", static_cast<long long>(e.frame), e.tick,
                         beatKindName(e.kind), e.a, e.b, e.c, e.value, e.index, e.x);
    }
}

// Renders up to `untilFrame` in device bursts; the "main thread" drains events every 1/60 s.
void renderTo(Render& r, u64 untilFrame) {
    if (untilFrame > r.frames) untilFrame = r.frames;
    while (r.done < untilFrame) {
        u32 n = r.opt.randomBursts ? 1 + r.rng.below(1024) : r.opt.burst;
        if (r.done + n > untilFrame) n = static_cast<u32>(untilFrame - r.done);
        r.engine->render(r.out + r.done * 2, n, 2);
        r.done += n;
        if (static_cast<double>(r.done) / kRate >= r.nextMainS) {
            drainEvents(r);
            r.nextMainS += 1.0 / 60.0;
        }
    }
}

Command cmd(CmdType t, u8 a = 0, u8 b = 0, u8 c = 0, float x = 0.0f, float y = 0.0f) {
    Command k;
    k.type = t;
    k.a = a;
    k.b = b;
    k.c = c;
    k.x = x;
    k.y = y;
    return k;
}

// ---------------------------------------------------------------- the sheets (sfxproto.py)

struct SheetItem {
    Sfx kind;
    i32 midi;
    double offset;
};

// sfxproto.sfx_sheet: every effect once, in order, at the sheet's group gains.
void sheetSfx(Render& r, const SampleBank& bank, double& lengthS, bool dryRun) {
    struct Group {
        const char* name;
        SheetItem items[8];
        u32 count;
        float gain;
    };
    const Group groups[] = {
        {"kill, small (three pitches)", {{Sfx::KillSmall, 81, 0.0}, {Sfx::KillSmall, 84, 0.18}, {Sfx::KillSmall, 88, 0.36}}, 3, 0.5f},
        {"kill, medium", {{Sfx::KillMedium, 69, 0.0}}, 1, 0.6f},
        {"kill, large", {{Sfx::KillLarge, 57, 0.0}}, 1, 0.7f},
        {"Shard run (eight, rising)",
         {{Sfx::Shard, 81, 0.0}, {Sfx::Shard, 84, 0.0625}, {Sfx::Shard, 86, 0.125}, {Sfx::Shard, 88, 0.1875},
          {Sfx::Shard, 91, 0.25}, {Sfx::Shard, 93, 0.3125}, {Sfx::Shard, 96, 0.375}, {Sfx::Shard, 98, 0.4375}},
         8, 0.35f},
        {"Boost picked up", {{Sfx::Boost, -1, 0.0}}, 1, 0.6f},
        {"upgrade taken", {{Sfx::Upgrade, -1, 0.0}}, 1, 0.6f},
        {"Dash", {{Sfx::Dash, -1, 0.0}}, 1, 0.6f},
        {"Perfect Dash", {{Sfx::PerfectDash, -1, 0.0}}, 1, 0.6f},
        {"hit taken", {{Sfx::Hit, -1, 0.0}}, 1, 0.7f},
        {"Nova: inhale, then blast on the beat", {{Sfx::NovaInhale, 8, 0.0}, {Sfx::NovaBlast, -1, 0.5}}, 2, 0.7f},
        {"enemy warp-in (two)", {{Sfx::Spawn, 57, 0.0}, {Sfx::Spawn, 64, 0.5}}, 2, 0.4f},
        {"telegraph tick, then enemy shot (x3)",
         {{Sfx::Tick, -1, 0.0}, {Sfx::EnemyShot, -1, 0.25}, {Sfx::Tick, -1, 0.5}, {Sfx::EnemyShot, -1, 0.75},
          {Sfx::Tick, -1, 1.0}, {Sfx::EnemyShot, -1, 1.25}},
         6, 0.5f},
        {"elite arrives", {{Sfx::Elite, -1, 0.0}}, 1, 0.6f},
        {"interface: tick, confirm, back", {{Sfx::UiTick, -1, 0.0}, {Sfx::UiConfirm, -1, 0.35}, {Sfx::UiBack, -1, 0.85}}, 3, 0.5f},
        {"Shield cell restored", {{Sfx::Shield, -1, 0.0}}, 1, 0.5f},
        {"game over", {{Sfx::GameOver, -1, 0.0}}, 1, 0.6f},
    };
    double t = 0.3;
    for (const Group& g : groups) {
        if (dryRun) std::printf("  %5.1f s  %s\n", t, g.name);
        double longest = 0.0;
        for (u32 i = 0; i < g.count; ++i) {
            const SheetItem& it = g.items[i];
            const i32 id = it.kind == Sfx::NovaInhale ? bank.inhale(0, 8) : bank.sfx(it.kind, it.midi);
            const double len = id >= 0 ? static_cast<double>(bank.sample(id).frames) / kRate : 0.0;
            if (it.offset + len > longest) longest = it.offset + len;
            if (dryRun) continue;
            renderTo(r, static_cast<u64>(std::llround((t + it.offset) * kRate)));
            Command c = cmd(CmdType::Sfx, static_cast<u8>(it.kind), it.midi > 0 ? static_cast<u8>(it.midi) : 0, 0,
                            g.gain / sfxLevel(it.kind), 0.0f);
            r.engine->post(c);
        }
        t += longest + 0.7;
    }
    lengthS = t + 0.5;
}

// M5: the listening sheets of sound_m5.json for the user's ear: every recipe (and held voice), then every cue, one after
// the other on Track 1 (A minor, 120 BPM), dry like the effect sheet. `list` gets "seconds  what" lines.
struct SheetStep {
    double at;          // seconds
    Command c;
    const char* what;   // nullptr: a continuation of the line before
};
u32 sheetM5(bool cues, SheetStep* out, u32 cap, double& endS) {
    u32 n = 0;
    double t = 0.5;
    const auto add = [&](double dt, Command c, const char* what) {
        if (n < cap) out[n++] = {t + dt, c, what};
    };
    const auto rec = [](Recipe r, i32 midi, float p1 = 0.0f, float p2 = 0.0f) {
        Command k = cmd(CmdType::Recipe, static_cast<u8>(r), 0, 255, 1.0f, 0.0f);
        encodeNote(k, midi);  // M5: a MIDI note or a chordNote()
        k.when = Timing::Next32;
        k.z = p1;
        k.level = p2;
        return k;
    };
    const auto cue = [](Cue c, u8 count, float value = 0.0f) {
        Command k = cmd(CmdType::Cue, static_cast<u8>(c), 0, count, value, 0.0f);
        k.when = Timing::Next32;
        return k;
    };
    if (!cues) {
        add(0.0, rec(Recipe::Tom, 45), "tom: root, third, fifth, octave (A2 C3 E3 A3)");
        add(0.5, rec(Recipe::Tom, 48), nullptr);
        add(1.0, rec(Recipe::Tom, 52), nullptr);
        add(1.5, rec(Recipe::Tom, 57), nullptr);
        t += 2.5;
        add(0.0, rec(Recipe::SubDrop, -1), "sub drop (120 -> 38 Hz)");
        t += 2.5;
        add(0.0, rec(Recipe::Gong, 69), "gong (A2) then gong (E2)");
        add(2.5, rec(Recipe::Gong, 64), nullptr);
        t += 5.5;
        add(0.0, rec(Recipe::Bell, 69), "bell: A4 C5 E5 A5");
        add(0.25, rec(Recipe::Bell, 72), nullptr);
        add(0.5, rec(Recipe::Bell, 76), nullptr);
        add(0.75, rec(Recipe::Bell, 81), nullptr);
        t += 1.8;
        add(0.0, rec(Recipe::Zap, 81), "zap: A5 C6 E6 (Arc's chain)");
        add(0.25, rec(Recipe::Zap, 84), nullptr);
        add(0.5, rec(Recipe::Zap, 88), nullptr);
        t += 1.5;
        add(0.0, rec(Recipe::Glass, 69), "glass: on A, C, E (seventh octave)");
        add(0.3, rec(Recipe::Glass, 72), nullptr);
        add(0.6, rec(Recipe::Glass, 76), nullptr);
        t += 1.5;
        add(0.0, rec(Recipe::SawGlide, 45, 7.0f, 1.0f), "saw glide: up a fifth over a beat (the Alpha's howl), then down an octave over two");
        add(1.2, rec(Recipe::SawGlide, 57, -12.0f, 2.0f), nullptr);
        t += 3.6;
        add(0.0, rec(Recipe::Sweep, -1), "sweep: the Dash; opening over a beat; closing over two");
        add(0.6, rec(Recipe::Sweep, -1, 1.0f, 1.0f), nullptr);
        add(1.4, rec(Recipe::Sweep, -1, -1.0f, 2.0f), nullptr);
        t += 3.0;
        add(0.5, rec(Recipe::RevCymbal, -1, 1.0f), "reversed cymbal: a beat, then a bar (each ends on its grid line)");
        add(2.5, rec(Recipe::RevCymbal, -1, 4.0f), nullptr);
        t += 3.5;
        add(0.0, rec(Recipe::Woodblock, -1), "woodblock: 1 kHz, then up the scale A5 B5 C6 D6");
        add(0.4, rec(Recipe::Woodblock, 81), nullptr);
        add(0.6, rec(Recipe::Woodblock, 83), nullptr);
        add(0.8, rec(Recipe::Woodblock, 84), nullptr);
        add(1.0, rec(Recipe::Woodblock, 86), nullptr);
        t += 1.6;
        add(0.0, rec(Recipe::Chirp, 81), "chirp: A5 C6 D6 E6");
        add(0.12, rec(Recipe::Chirp, 84), nullptr);
        add(0.24, rec(Recipe::Chirp, 86), nullptr);
        add(0.36, rec(Recipe::Chirp, 88), nullptr);
        t += 1.0;
        add(0.0, rec(Recipe::Whoosh, -1, 1.0f), "whoosh: out over a beat; out and back (the Comet)");
        add(1.0, rec(Recipe::Whoosh, -1, 1.0f, 1.0f), nullptr);
        t += 2.6;
        add(0.0, rec(Recipe::Roll, -1, 4.0f), "roll: four beats (it ends on the grid line it was given)");
        t += 2.6;
        Command v = cmd(CmdType::VoiceStart, 0, static_cast<u8>(Recipe::Drone), 255, 1.0f, 0.0f);
        v.z = 300.0f;
        add(0.0, v, "drone (held 4 s): low-pass opening 300 -> 2400 Hz and closing (the Sweeper)");
        for (u32 k = 1; k <= 6; ++k) add(0.4 * k, cmd(CmdType::VoiceSet, 0, 0, 0, k <= 3 ? 300.0f + 700.0f * k : 2400.0f - 700.0f * (k - 3), -1.0f), nullptr);
        add(4.0, cmd(CmdType::VoiceStop, 0, 0, 0, 0.3f), nullptr);
        t += 5.0;
        add(0.0, cmd(CmdType::VoiceStart, 1, static_cast<u8>(Recipe::PadVoice), 69, 1.0f, -0.3f), "pad voices (held 3 s): A4, then C5 joins");
        add(1.0, cmd(CmdType::VoiceStart, 2, static_cast<u8>(Recipe::PadVoice), 72, 1.0f, 0.3f), nullptr);
        add(3.0, cmd(CmdType::VoiceStop, 1, 0, 0, 0.7f), nullptr);
        add(3.0, cmd(CmdType::VoiceStop, 2, 0, 0, 0.7f), nullptr);
        t += 4.5;
        Command cr = cmd(CmdType::VoiceStart, 3, static_cast<u8>(Recipe::Crackle), 255, 1.0f, 0.0f);
        cr.z = 5.0f;
        add(0.0, cr, "crackle (held 3 s): 5 -> 60 bursts a second");
        add(1.0, cmd(CmdType::VoiceSet, 3, 0, 0, 25.0f, -1.0f), nullptr);
        add(2.0, cmd(CmdType::VoiceSet, 3, 0, 0, 60.0f, -1.0f), nullptr);
        add(3.0, cmd(CmdType::VoiceStop, 3, 0, 0, 0.1f), nullptr);
        t += 3.8;
        // M5, appended (the content engineer's eight; Track 1's Am, 120 BPM: a sixteenth is 0.125 s).
        add(0.0, rec(Recipe::RubberString, chordNote(0, 3)), "rubber string (the bass patch, twice its decay): Longwave's run, scale degrees 0-7 above "
                                                            "the chord's root in octave 3 (chordNote), a sixteenth each");
        for (i32 k = 1; k < 8; ++k) add(0.125 * k, rec(Recipe::RubberString, chordNote(k, 3)), nullptr);
        t += 1.6;
        add(0.0, rec(Recipe::Impact, -1), "impact as a one-shot: as it is, then a fifth up (the whip, p1 = 7)");
        add(1.6, rec(Recipe::Impact, -1, 7.0f), nullptr);
        t += 3.6;
        add(0.0, rec(Recipe::Hat, -1), "closed hats on the game's own grid, tightening (the Hive's Ring): 0.25 s, 0.18, 0.12, 0.08, 0.05 apart; then "
                                       "the open hat (p1 = 1)");
        {
            double h = 0.0;
            for (double gap : {0.25, 0.25, 0.18, 0.18, 0.12, 0.12, 0.12, 0.08, 0.08, 0.08, 0.08, 0.05, 0.05, 0.05, 0.05, 0.05}) {
                h += gap;
                add(h, rec(Recipe::Hat, -1), nullptr);
            }
            add(h + 0.5, rec(Recipe::Hat, -1, 1.0f), nullptr);
            t += h + 1.2;
        }
        add(0.0, rec(Recipe::TomRoll, -1, 2.0f), "tom roll rising over two beats into its tick (Battering Ram's lock), then over four beats up a fifth");
        add(1.6, rec(Recipe::TomRoll, -1, 4.0f, 7.0f), nullptr);
        t += 4.4;
        add(0.0, rec(Recipe::DrivenKick, -1, 1.0f), "the kick, then driven (p1 = 3, the default) and harder (6): Battering Ram's charge");
        add(0.6, rec(Recipe::DrivenKick, -1, 3.0f), nullptr);
        add(1.2, rec(Recipe::DrivenKick, -1, 6.0f), nullptr);
        t += 2.0;
        add(0.0, rec(Recipe::RimShot, chordNote(0, 5, NoteFrom::ChordTone)), "rim shots on the chord's tones (Carom King's walls: left root, top "
                                                                              "third, right fifth, bottom seventh), then the Princes' octave up");
        for (i32 k = 1; k < 4; ++k) add(0.3 * k, rec(Recipe::RimShot, chordNote(k, 5, NoteFrom::ChordTone)), nullptr);
        for (i32 k = 0; k < 4; ++k) add(1.4 + 0.15 * k, rec(Recipe::RimShot, chordNote(k, 6, NoteFrom::ChordTone)), nullptr);
        t += 2.6;
        {
            Command pv = cmd(CmdType::VoiceStart, 4, static_cast<u8>(Recipe::PadVoice), 255, 1.0f, 0.0f);
            encodeNote(pv, chordNote(4, 4, NoteFrom::ChordTone));  // the chord's root an octave up (A4)
            pv.z = 0.02f;                                          // choir.md: attack 20 ms
            add(0.0, pv, "a held pad voice (attack 20 ms) gliding: down a fourth over a beat (a Voice's death), then from A4 up an octave over "
                         "a bar (the Choir's held note)");
            Command fall = cmd(CmdType::VoiceSet, 4, 0, 0, 0.0f, -1.0f);
            encodeNote(fall, 64);
            fall.z = 0.5f;
            add(0.8, fall, nullptr);
            add(1.8, cmd(CmdType::VoiceStop, 4, 0, 0, 0.5f), nullptr);
            Command held = cmd(CmdType::VoiceStart, 5, static_cast<u8>(Recipe::PadVoice), 69, 1.0f, 0.0f);
            held.z = 0.02f;
            add(2.8, held, nullptr);
            Command rise = cmd(CmdType::VoiceSet, 5, 0, 0, 0.0f, -1.0f);
            encodeNote(rise, 81);
            rise.z = 2.0f;
            add(3.8, rise, nullptr);
            add(6.6, cmd(CmdType::VoiceStop, 5, 0, 0, 0.5f), nullptr);
            t += 7.8;
        }
    } else {
        add(0.0, cue(Cue::ForkSplit, 1), "Fork: a forked shot (the fifth above the last lead note, a 1/32 later), then four at once");
        add(0.5, cue(Cue::ForkSplit, 4), nullptr);
        t += 1.2;
        add(0.0, cue(Cue::HeavyStepBurst, 1), "Heavy Step: an orb bursts (the small kill's thud)");
        t += 0.8;
        add(0.0, cue(Cue::ArcChain, 1, 1.0f), "Arc: a chain of three jumps (a zap a chord tone up each)");
        add(0.13, cue(Cue::ArcChain, 1, 2.0f), nullptr);
        add(0.26, cue(Cue::ArcChain, 1, 3.0f), nullptr);
        t += 1.2;
        add(0.0, cue(Cue::BurnCount, 1, 5.0f), "Burn: 5 enemies burning, then 30, then none");
        add(1.0, cue(Cue::BurnCount, 1, 30.0f), nullptr);
        add(2.5, cue(Cue::BurnCount, 1, 0.0f), nullptr);
        t += 3.2;
        add(0.0, cue(Cue::ShatterBurst, 1), "Shatter: one kill throws shrapnel, then six");
        add(0.4, cue(Cue::ShatterBurst, 6), nullptr);
        t += 1.0;
        add(0.0, cue(Cue::GravityHit, 1), "Gravity Rounds: a hit, then three");
        add(0.3, cue(Cue::GravityHit, 3), nullptr);
        t += 0.9;
        add(0.0, cue(Cue::AfterimageDecoy, 1), "Afterimage: the Dash's sweep a beat later at half level");
        t += 1.3;
        add(0.0, cue(Cue::ShockstepDash, 1, 0.0f), "Shockstep: a Dash (tom on the root), a Perfect Dash (and a fifth above)");
        add(0.6, cue(Cue::ShockstepDash, 1, 1.0f), nullptr);
        t += 1.3;
        add(0.0, cue(Cue::AegisBlock, 1), "Aegis: a bullet stopped, then five");
        add(0.3, cue(Cue::AegisBlock, 5), nullptr);
        t += 0.8;
        add(0.0, cue(Cue::LastLightNova, 1), "Last Light: the free mini-Nova");
        t += 2.6;
        add(0.0, cue(Cue::CollectorBoost, 1), "Collector: a Boost spawns (the Boost bells)");
        t += 1.4;
        add(0.0, cue(Cue::RicochetBounce, 1, 1.0f), "Ricochet: three bounces climbing the scale");
        add(0.13, cue(Cue::RicochetBounce, 1, 2.0f), nullptr);
        add(0.26, cue(Cue::RicochetBounce, 1, 3.0f), nullptr);
        t += 1.0;
        add(0.0, cue(Cue::StormCoilStrike, 1), "Storm Coil: strikes on six triplet eighths");
        for (u32 k = 1; k < 6; ++k) add(0.167 * k, cue(Cue::StormCoilStrike, 1), nullptr);
        t += 1.5;
        add(0.0, cue(Cue::SupernovaImpact, 1), "Supernova Rounds: an impact, then eight");
        add(0.3, cue(Cue::SupernovaImpact, 8), nullptr);
        t += 0.9;
        add(0.0, cue(Cue::ColossusRam, 1), "Colossus: an enemy rammed, then four");
        add(0.4, cue(Cue::ColossusRam, 4), nullptr);
        t += 1.0;
        add(0.0, cue(Cue::FreezeShatter, 1), "Freeze Frame: an enemy shatters, then ten");
        add(0.4, cue(Cue::FreezeShatter, 10), nullptr);
        t += 1.0;
        add(0.0, cue(Cue::ConfettiKill, 1), "Confetti: a kill bursts (six chirps up the scale)");
        t += 1.0;
        add(0.0, cue(Cue::BlackHoleNova, 1), "Black Hole: the three-beat inhale, then the blast with the sub drop");
        t += 4.5;
    }
    endS = t;
    return n;
}

// M5: the upgrades' listening sheets (riffs, mods, boosts, fusions), through the conductor: each entry is a few bars without
// the upgrade, then a few with it, on a Track that shows it well, one player (firing where the upgrade rides on the lead),
// the Groove held at the entry's intensity (or the Drop, bar by bar as the director names it). What the game would post
// meanwhile (Storm Coil's strikes, Arc's chains, a hit) is fed every 1/60 s. The .txt list says when each part begins.
enum class Feed : u8 { None, StormCoil, Arc, Shatter, Gravity, Supernova, Colossus, Freeze, BlackHole, Ricochet, Confetti, Hit, Shards };
struct UpEntry {
    const char* what;
    u8 track;          // 0-based
    float intensity;
    bool fire;
    u8 barsOff, barsOn;
    PlayerLoadout off, on;
    Feed feed;
    bool feedOff;      // the feed in the "without" bars too (a Fusion's ingredients)
    bool drop;         // the Drop, named bar by bar (its Surge in its first half), after a bar of the Groove
};
struct LoadoutBuilder {
    PlayerLoadout l;
    LoadoutBuilder& riff(Riff r, u8 lv) {
        l.riffs[static_cast<u32>(r)] = lv;
        return *this;
    }
    LoadoutBuilder& mod(Mod m, u8 st) {
        l.mods[static_cast<u32>(m)] = st;
        return *this;
    }
    LoadoutBuilder& boost(Boost b) {
        l.boosts = static_cast<u16>(l.boosts | (1u << static_cast<u32>(b)));
        return *this;
    }
    LoadoutBuilder& fusion(Fusion f) {
        l.fusions = static_cast<u8>(l.fusions | (1u << static_cast<u32>(f)));
        return *this;
    }
    operator PlayerLoadout() const { return l; }
};
LoadoutBuilder L() { return {}; }

u32 upgradeSheetPlan(const char* kind, UpEntry* e, u32 cap) {
    u32 n = 0;
    const auto add = [&](const char* what, u8 track, float intensity, bool fire, u8 off, u8 on, PlayerLoadout a, PlayerLoadout b, Feed feed = Feed::None,
                         bool feedOff = false, bool drop = false) {
        if (n < cap) e[n++] = {what, track, intensity, fire, off, on, a, b, feed, feedOff, drop};
    };
    if (eq(kind, "riffs")) {
        add("Lancer, level 1: the full chord on beat 4, charged by a reverse swell from the One (Track 1)", 0, 0.6f, false, 2, 4, L(), L().riff(Riff::Lancer, 1));
        add("Lancer, level 1 then 3: on beats 2 and 4, each charged over the beat before (Track 1)", 0, 0.6f, false, 2, 4, L().riff(Riff::Lancer, 1),
            L().riff(Riff::Lancer, 3));
        add("Minelayer: a closed hat as each mine is laid, the clap and the impact on the One (Track 5)", 4, 0.6f, false, 2, 4, L(), L().riff(Riff::Minelayer, 1));
        add("Storm Coil: a zap on each triplet eighth something is struck (Track 4)", 3, 0.6f, false, 2, 4, L(), L().riff(Riff::StormCoil, 1), Feed::StormCoil);
        add("Swarm, level 1: the volley, eight bells up the arp from the One (Track 2)", 1, 0.6f, false, 2, 4, L(), L().riff(Riff::Swarm, 1));
        add("Swarm, level 1 then 5: twenty-four (Track 2)", 1, 0.6f, false, 2, 4, L().riff(Riff::Swarm, 1), L().riff(Riff::Swarm, 5));
        add("Singularity: every four bars a reverse swell while it pulls, the boom on the next One (Track 6)", 5, 0.6f, false, 2, 8, L(),
            L().riff(Riff::Singularity, 1));
        add("Mirror, level 1 then 5: the player's lead an octave up, 0.4 then 0.7 (Track 3, firing)", 2, 0.4f, true, 2, 4, L().riff(Riff::Mirror, 1),
            L().riff(Riff::Mirror, 5));
        add("Mirror: without, then level 1 (Track 3, firing)", 2, 0.4f, true, 2, 4, L(), L().riff(Riff::Mirror, 1));
        add("Comet: a whoosh out on the One, back on three (Track 8)", 7, 0.6f, false, 2, 4, L(), L().riff(Riff::Comet, 1));
        add("Sweeper: a drone on the bass note, opening as the arm passes twelve o'clock, once in two bars (Track 7)", 6, 0.35f, false, 2, 6, L(),
            L().riff(Riff::Sweeper, 1));
        add("Flak, level 1: a snare with each burst on two and four (Track 10)", 9, 0.5f, false, 2, 4, L(), L().riff(Riff::Flak, 1));
        add("Flak, level 1 then 4: a clap a 1/32 after each (Track 10)", 9, 0.5f, false, 2, 4, L().riff(Riff::Flak, 1), L().riff(Riff::Flak, 4));
    } else if (eq(kind, "mods")) {
        add("Seeker, 1 stack: each lead note glides in from the last over 12 ms (Track 3, firing)", 2, 0.35f, true, 2, 4, L(), L().mod(Mod::Seeker, 1));
        add("Seeker, 3 stacks: over 36 ms (Track 3, firing)", 2, 0.35f, true, 2, 4, L(), L().mod(Mod::Seeker, 3));
        add("Heavy Step: every fourth lead step doubled an octave down on the bass (Track 5, firing)", 4, 0.35f, true, 2, 4, L(), L().mod(Mod::HeavyStep, 1));
        add("Accent, 1 stack: beat 1 at 1.4 with the stab under it (Track 1, firing)", 0, 0.35f, true, 2, 4, L(), L().mod(Mod::Accent, 1));
        add("Accent, 2 then 3 stacks: beats 1 and 3, then every beat (Track 1, firing)", 0, 0.35f, true, 2, 4, L().mod(Mod::Accent, 2), L().mod(Mod::Accent, 3));
        add("Echo, 1 stack: the lead's delay a beat long, its mix +0.15 (Track 2, firing)", 1, 0.35f, true, 2, 4, L(), L().mod(Mod::Echo, 1));
        add("Echo, 2 stacks: +0.3 (Track 2, firing)", 1, 0.35f, true, 2, 4, L(), L().mod(Mod::Echo, 2));
        add("Wave, 1 stack: vibrato 5.5 Hz, 18 cents (Track 9, firing)", 8, 0.35f, true, 2, 4, L(), L().mod(Mod::Wave, 1));
        add("Wave, 2 stacks: 36 cents (Track 9, firing)", 8, 0.35f, true, 2, 4, L(), L().mod(Mod::Wave, 2));
        add("Rear Guard, 1 stack: two degrees below the arp on the off-beat eighths (Track 4, firing)", 3, 0.35f, true, 2, 4, L(), L().mod(Mod::RearGuard, 1));
        add("Rear Guard, 2 stacks: and five below on steps 1 and 9 (Track 4, firing)", 3, 0.35f, true, 2, 4, L().mod(Mod::RearGuard, 1),
            L().mod(Mod::RearGuard, 2));
        add("Syncopation, 1 stack: off-beat notes 0.3 louder with a square blip an octave up (Track 10, firing)", 9, 0.35f, true, 2, 4, L(),
            L().mod(Mod::Syncopation, 1));
        add("Bar Line: the chord's stab and a zap on every One (Track 6, firing)", 5, 0.35f, true, 2, 4, L(), L().mod(Mod::BarLine, 1));
        add("Crescendo: +0.2 dB a bar to +2 dB, the pad opening with it; a hit on the tenth bar drops both (Track 1, firing)", 0, 0.7f, true, 2, 12, L(),
            L().mod(Mod::Crescendo, 1), Feed::Hit);
        add("Kick Pulse: a tom an octave above the kick with each kick: Afterglow's half-time kick (Track 7)", 6, 0.35f, false, 2, 4, L(), L().mod(Mod::KickPulse, 1));
        add("Kick Pulse on Track 1's four to the floor", 0, 0.35f, false, 2, 4, L(), L().mod(Mod::KickPulse, 1));
        add("Afterparty: the Drop without (the Surge: 1/16 triplets, then 1/32 for four bars), then with: 1/32 from the start, two bars longer (Track 10, "
            "firing)",
            9, 1.0f, true, 9, 8, L(), L().mod(Mod::Afterparty, 1), Feed::None, false, true);
    } else if (eq(kind, "boosts")) {
        add("Colossus: the bass's octave below (Track 8)", 7, 0.6f, false, 2, 4, L(), L().boost(Boost::Colossus));
        add("Colossus: and a tom on the root for each enemy rammed (Track 8)", 7, 0.6f, false, 2, 4, L().boost(Boost::Colossus), L().boost(Boost::Colossus),
            Feed::Colossus);
        add("Time Stretch: the drums at half rate, the lead and the Riffs on their grid (Track 1, firing, Orbitals)", 0, 0.6f, true, 2, 4,
            L().riff(Riff::Orbitals, 1), L().riff(Riff::Orbitals, 1).boost(Boost::TimeStretch));
        add("Freeze Frame: a glass chord as it starts, the music high-passed for its two bars, glass for each enemy shattered (Track 3)", 2, 0.7f, false, 2, 2,
            L(), L().boost(Boost::FreezeFrame), Feed::Freeze);
        add("Chain Storm: on every beat a zap on each of the chord's stab notes (Track 4)", 3, 0.6f, false, 2, 4, L(), L().boost(Boost::ChainStorm));
        add("Chorus: the lead doubled a fifth above and an octave below (Track 2, firing)", 1, 0.35f, true, 2, 4, L(), L().boost(Boost::Chorus));
        add("Beam: the lead holds, four times the decay, softly, over a drone on the arp's first note (Track 9, firing)", 8, 0.35f, true, 2, 4, L(),
            L().boost(Boost::Beam));
        add("Fractal: each note followed by its octave and double octave, a 1/32 apart (Track 6, firing)", 5, 0.35f, true, 2, 4, L(), L().boost(Boost::Fractal));
        add("Carousel: a tom on every beat, root, third, fifth, octave (Track 5)", 4, 0.6f, false, 2, 4, L(), L().boost(Boost::Carousel));
        add("Hyper (first slice): the lead and every Riff at double rate, here with Lancer and Flak (Track 10, firing)", 9, 0.5f, true, 2, 4,
            L().riff(Riff::Lancer, 1).riff(Riff::Flak, 1), L().riff(Riff::Lancer, 1).riff(Riff::Flak, 1).boost(Boost::Hyper));
        add("Supernova Rounds: each impact the small kill's thud (Track 1, firing)", 0, 0.35f, true, 2, 4, L(), L().boost(Boost::SupernovaRounds), Feed::Supernova);
        add("Black Hole: the Nova's inhale over three beats, then the blast with the sub drop (Track 1)", 0, 0.35f, false, 2, 4, L(), L().boost(Boost::BlackHole),
            Feed::BlackHole);
        add("Ricochet Fever: the bounce blips two scale steps a bounce (without: Ricochet's one) (Track 3, firing)", 2, 0.35f, true, 2, 4, L().mod(Mod::Ricochet, 1),
            L().mod(Mod::Ricochet, 1).boost(Boost::RicochetFever), Feed::Ricochet, true);
        add("Confetti: each kill a run of six Shard chirps up the scale (Track 2)", 1, 0.35f, false, 2, 4, L(), L().boost(Boost::Confetti), Feed::Confetti);
        add("Magnet Storm (first slice): the Shard runs climb twice as fast (Track 1)", 0, 0.35f, false, 2, 4, L(), L().boost(Boost::MagnetStorm), Feed::Shards,
            true);
    } else if (eq(kind, "fusions")) {
        add("Helix Array (Split 2 + Wave 2, then fused): the harmony sways against the lead (Track 3, firing)", 2, 0.35f, true, 3, 4,
            L().mod(Mod::Split, 2).mod(Mod::Wave, 2), L().mod(Mod::Split, 2).mod(Mod::Wave, 2).fusion(Fusion::HelixArray));
        add("Standing Wave (Echo 2 + Accent 3, then fused): each accented note held a bar as a pad voice (Track 1, firing)", 0, 0.35f, true, 3, 4,
            L().mod(Mod::Echo, 2).mod(Mod::Accent, 3), L().mod(Mod::Echo, 2).mod(Mod::Accent, 3).fusion(Fusion::StandingWave));
        add("Tesla Bloom (Pulse Ring 5 + Arc 3, then fused): each ring's tom carries zaps; Arc's chains are quiet (Track 4)", 3, 0.6f, false, 3, 4,
            L().riff(Riff::PulseRing, 5).mod(Mod::Arc, 3), L().riff(Riff::PulseRing, 5).mod(Mod::Arc, 3).fusion(Fusion::TeslaBloom), Feed::Arc, true);
        add("Shard Halo (Orbitals 5 + Shatter 3, then fused): glass ticks double the shaker; Shatter's bursts are quiet (Track 2)", 1, 0.6f, false, 3, 4,
            L().riff(Riff::Orbitals, 5).mod(Mod::Shatter, 3), L().riff(Riff::Orbitals, 5).mod(Mod::Shatter, 3).fusion(Fusion::ShardHalo), Feed::Shatter, true);
        add("Downbeat Cannon (Lancer 5 + On-Beat Crit 3, then fused): the chord at 1.2 and the impact on every One (Track 10, firing)", 9, 0.5f, true, 3, 4,
            L().riff(Riff::Lancer, 5).mod(Mod::OnBeatCrit, 3), L().riff(Riff::Lancer, 5).mod(Mod::OnBeatCrit, 3).fusion(Fusion::DownbeatCannon));
        add("Collapse Field (Minelayer 5 + Gravity Rounds 2, then fused): a swell over the last beat into the mines; the thups are quiet (Track 5)", 4, 0.6f,
            false, 3, 4, L().riff(Riff::Minelayer, 5).mod(Mod::GravityRounds, 2),
            L().riff(Riff::Minelayer, 5).mod(Mod::GravityRounds, 2).fusion(Fusion::CollapseField), Feed::Gravity, true);
        add("Murmuration (Swarm 5 + Seeker 3, then fused): the run never stops, a 1/32 trill on the third and fifth (Track 6, firing)", 5, 0.35f, true, 3, 4,
            L().riff(Riff::Swarm, 5).mod(Mod::Seeker, 3), L().riff(Riff::Swarm, 5).mod(Mod::Seeker, 3).fusion(Fusion::Murmuration));
        add("Quartet (Mirror 5 + Rear Guard 2, then fused): the lead in four parts (Track 9, firing)", 8, 0.35f, true, 3, 4,
            L().riff(Riff::Mirror, 5).mod(Mod::RearGuard, 2), L().riff(Riff::Mirror, 5).mod(Mod::RearGuard, 2).fusion(Fusion::Quartet));
    }
    return n;
}

void postLoadout(Engine& e, const PlayerLoadout& l) {
    for (u32 first = 0; first < kModCount; first += 8) {
        Command c = cmd(CmdType::SetModStacks, 0, static_cast<u8>(first), static_cast<u8>(kModCount - first < 8 ? kModCount - first : 8));
        for (u32 k = 0; k < c.c; ++k) c.mods[k] = l.mods[first + k];
        e.post(c);
    }
    for (u32 r = 0; r < kRiffCount; ++r) e.post(cmd(CmdType::SetRiff, 0, static_cast<u8>(r), l.riffs[r]));
    for (u32 b = 0; b < kBoostCount; ++b) e.post(cmd(CmdType::SetBoost, 0, static_cast<u8>(b), static_cast<u8>((l.boosts >> b) & 1u)));
    e.post(cmd(CmdType::SetFusions, 0, l.fusions));
}

// What the game would post in a 1/60 s frame (number `frame` of the part).
void feedFrame(Engine& e, Feed feed, u32 frame, u32 bar, u32 hitBar, bool& hitDone) {
    const auto cue = [&](Cue c, u8 count, float value = 0.0f) {
        Command k = cmd(CmdType::Cue, static_cast<u8>(c), 0, count, value, static_cast<float>((frame * 37 % 11) / 5.0 - 1.0) * 0.5f);
        k.when = Timing::Next32;
        e.post(k);
    };
    switch (feed) {
        case Feed::StormCoil:
            if (frame % 3 != 2) cue(Cue::StormCoilStrike, 1);
            break;
        case Feed::Arc:
            if (frame % 20 == 0) cue(Cue::ArcChain, 1, 1.0f);
            if (frame % 20 == 4) cue(Cue::ArcChain, 1, 2.0f);
            if (frame % 20 == 8) cue(Cue::ArcChain, 1, 3.0f);
            break;
        case Feed::Shatter:
            if (frame % 9 == 0) cue(Cue::ShatterBurst, 1 + frame % 3);
            break;
        case Feed::Gravity:
            if (frame % 5 == 0) cue(Cue::GravityHit, 1 + frame % 2);
            break;
        case Feed::Supernova:
            if (frame % 6 == 0) cue(Cue::SupernovaImpact, 1 + frame % 3);
            break;
        case Feed::Colossus:
            if (frame % 17 == 0) cue(Cue::ColossusRam, 1 + frame % 2);
            break;
        case Feed::Freeze:
            if (frame % 10 == 5) cue(Cue::FreezeShatter, 1 + frame % 4);
            break;
        case Feed::BlackHole:
            if (frame == 30) cue(Cue::BlackHoleNova, 1);
            break;
        case Feed::Ricochet:
            if (frame % 30 < 9 && frame % 3 == 0) cue(Cue::RicochetBounce, 1, static_cast<float>(1 + (frame % 30) / 3));
            break;
        case Feed::Confetti:
            if (frame % 45 == 0) cue(Cue::ConfettiKill, 1);
            break;
        case Feed::Hit:
            if (bar >= hitBar && !hitDone) {
                cue(Cue::PlayerHit, 1);
                e.post(cmd(CmdType::Sfx, static_cast<u8>(Sfx::Hit), 0, 0, 1.0f));
                hitDone = true;
            }
            break;
        case Feed::Shards:
            if (frame % 40 < 6 && frame % 40 % 1 == 0) e.post(cmd(CmdType::Shard, 0, 0, 0, 1.0f));
            break;
        default: break;
    }
}

// M5: how loud each upgrade of the four sheets is in the mix: the entry's "with" bars rendered twice from the same start,
// with the upgrade and without (the "without" loadout), and the difference measured against the music (RMS and peak, dB).
// raveform_headless --render upgrade-levels
int upgradeLevels(const MusicData& music, const SampleBank& bank) {
    static UpEntry plan[64];
    std::printf("%-8s %-96s %8s %8s %8s %8s\n", "sheet", "entry", "music", "its rms", "its peak", "re music");
    for (const char* kind : {"riffs", "mods", "boosts", "fusions"}) {
        const u32 n = upgradeSheetPlan(kind, plan, 64);
        for (u32 i = 0; i < n; ++i) {
            const UpEntry& u = plan[i];
            const u32 bars = u.drop ? u.barsOn + 1 : u.barsOn;
            const double barS = 240.0 / music.tracks[u.track].bpm;
            const u64 frames = static_cast<u64>((bars * barS + 1.0) * kRate);
            float* out[2] = {static_cast<float*>(std::calloc(frames * 2, sizeof(float))), static_cast<float*>(std::calloc(frames * 2, sizeof(float)))};
            for (u32 k = 0; k < 2; ++k) {
                Engine e;
                if (!e.init(music, bank, kRate) || !out[k]) return 1;
                e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), u.track, 0));
                e.post(cmd(CmdType::SetPlayers, 1, 0));
                e.post(cmd(CmdType::SetIntensity, 0, 0, 0, u.intensity));
                e.post(cmd(CmdType::SetForm, static_cast<u8>(Form::Short)));
                if (!u.drop) e.post(cmd(CmdType::HoldSection, 1));
                e.post(cmd(CmdType::SetFire, 0, u.fire ? 1 : 0));
                postLoadout(e, k ? u.on : u.off);
                u64 done = 0;
                u32 frame = 0, named = 1;
                bool hitDone = false;
                i32 lastBar = -1;
                while (done < frames) {
                    BeatEvent ev;
                    while (e.popEvent(ev))
                        if (ev.kind == BeatKind::Bar) lastBar = static_cast<i32>(ev.tick / kTicksPerBar);
                    if (u.drop && named < bars && lastBar >= static_cast<i32>(named) - 1) {
                        e.post(cmd(CmdType::SetPhrase, static_cast<u8>(Section::Drop), static_cast<u8>((named - 1) % 8), 8));
                        ++named;
                    }
                    if (u.feed != Feed::None && (k || u.feedOff)) feedFrame(e, u.feed, frame, static_cast<u32>(lastBar < 0 ? 0 : lastBar), 10, hitDone);
                    ++frame;
                    const u64 step = kRate / 60 < frames - done ? kRate / 60 : frames - done;
                    for (u64 j = 0; j < step;) {
                        const u32 b = static_cast<u32>(step - j < 192 ? step - j : 192);
                        e.render(out[k] + (done + j) * 2, b, 2);
                        j += b;
                    }
                    done += step;
                }
            }
            double music2 = 0.0, diff2 = 0.0, peak = 0.0;
            for (u64 j = 0; j < frames * 2; ++j) {
                const double d = static_cast<double>(out[1][j]) - out[0][j];
                music2 += static_cast<double>(out[0][j]) * out[0][j];
                diff2 += d * d;
                peak = std::fabs(d) > peak ? std::fabs(d) : peak;
            }
            const double m = 10.0 * std::log10(music2 / (frames * 2) + 1e-20), d = 10.0 * std::log10(diff2 / (frames * 2) + 1e-20);
            std::printf("%-8s %-96.96s %8.1f %8.1f %8.1f %+8.1f\n", kind, u.what, m, d, 20.0 * std::log10(peak + 1e-12), d - m);
            std::free(out[0]);
            std::free(out[1]);
        }
    }
    return 0;
}

// sfxproto.lead_timbres: the same two bars on each player's lead, a quiet kick on the beats.
void sheetLeads(Render& r, const MusicData& music) {
    const Track& tr = music.tracks[0];
    const double step = 60.0 / tr.bpm / 4.0;
    const char* const chordNames[2] = {"Am", "F"};
    for (u32 k = 0; k < 4; ++k) {
        const double t0 = 0.3 + k * 4.5;
        u32 i = 0;
        for (u32 b = 0; b < 2; ++b) {
            const Chord* ch = &tr.chords[0];
            for (u32 c = 0; c < tr.chordCount; ++c)
                if (eq(tr.chords[c].name, chordNames[b])) ch = &tr.chords[c];
            for (u32 s = 0; s < 16; ++s, ++i) {
                const double t = t0 + (b * 16 + s) * step;
                renderTo(r, static_cast<u64>(std::llround(t * kRate)));
                if (s % 4 == 0) r.engine->post(cmd(CmdType::Drum, static_cast<u8>(Pat::Kick), 0, 0, 0.35f, 0.0f));
                Command n = cmd(CmdType::Note, static_cast<u8>(k), ch->arp[s], 0, s % 4 == 0 ? 1.0f : 0.78f,
                                0.2f * std::sin(i * 0.9f));
                n.level = 0.34f;
                r.engine->post(n);
            }
        }
    }
}

// A gameplay-style mock: the conductor driven by a scripted MixState timeline (one Short phrase cycle of
// Track 1 and two more Groove bars), with the effects of a simulated run on top (sfxproto.gameplay_mock).
void mockStep(Render& r, const MusicData& music, u32 frameIndex, Random& rng) {
    Engine& e = *r.engine;
    const double bpm = music.tracks[0].bpm;
    const double barS = 240.0 / bpm;
    const double t = static_cast<double>(r.done) / kRate;
    const double prevT = t - 1.0 / 60.0;
    const auto crossed = [&](double at) { return prevT < at && t >= at; };
    const auto atBar = [&](double bars) { return crossed(bars * barS); };
    if (frameIndex == 0) {
        Command m = cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0);
        e.post(m);
        e.post(cmd(CmdType::SetForm, static_cast<u8>(Form::Short)));
        e.post(cmd(CmdType::SetPlayers, 1, 0));
        e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 0.25f));
    }
    if (atBar(1.75)) e.post(cmd(CmdType::Sfx, static_cast<u8>(Sfx::Boost), 0, 0, 1.0f, 0.0f));
    if (atBar(2.0)) e.post(cmd(CmdType::SetFire, 0, 1));
    if (atBar(2.5)) {
        Command c = cmd(CmdType::Sfx, static_cast<u8>(Sfx::PerfectDash), 0, 0, (sfxLevel(Sfx::PerfectDash) + 0.5f * sfxLevel(Sfx::Dash)) / sfxLevel(Sfx::PerfectDash));
        e.post(c);
    }
    if (atBar(3.0)) e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 0.5f));
    if (atBar(3.8)) e.post(cmd(CmdType::Sfx, static_cast<u8>(Sfx::Upgrade), 0, 0, 1.0f));
    if (atBar(4.0)) e.post(cmd(CmdType::SetRiff, 0, static_cast<u8>(Riff::Sentinel), 1));
    if (atBar(5.0)) e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 0.8f));
    if (atBar(6.0)) e.post(cmd(CmdType::SetRiff, 0, static_cast<u8>(Riff::PulseRing), 1));
    if (atBar(7.0)) e.post(cmd(CmdType::SetRiff, 0, static_cast<u8>(Riff::Orbitals), 1));
    if (atBar(9.375)) e.post(cmd(CmdType::Sfx, static_cast<u8>(Sfx::Hit), 0, 0, 1.0f));  // the Build: a hit
    if (atBar(14.325)) e.post(cmd(CmdType::Sfx, static_cast<u8>(Sfx::Dash), 0, 0, 1.0f));
    if (atBar(16.0)) e.post(cmd(CmdType::Sfx, static_cast<u8>(Sfx::Elite), 0, 0, 1.0f));
    if (atBar(17.575)) e.post(cmd(CmdType::Nova));
    if (atBar(20.0)) e.post(cmd(CmdType::SetFire, 0, 0));  // the Break: nobody fires, the bed echoes the arp
    if (atBar(21.5)) e.post(cmd(CmdType::Sfx, static_cast<u8>(Sfx::Upgrade), 0, 0, 1.0f));
    if (atBar(24.0)) e.post(cmd(CmdType::SetFire, 0, 1));
    // Kills on the 1/32 grid: more of them as the run heats up (the engine aggregates per step).
    const double bar = t / barS;
    const bool build = bar >= 8.0 && bar < 12.0, drop = bar >= 12.0 && bar < 20.0, brk = bar >= 20.0 && bar < 24.0;
    if (!brk && !(bar >= 11.5 && bar < 12.0)) {
        // sfxproto's chances per 1/32 step (small) and per eighth (medium), as chances per 1/60 s frame.
        const double pSmall = drop ? 0.55 : (build ? 0.35 : 0.22);
        if (rng.uniform() < pSmall * 32.0 / (60.0 * barS)) {
            const u32 count = 1 + static_cast<u32>(-std::log(1.0 - rng.uniform()) * (drop ? 3.5 : (build ? 1.6 : 0.8)));
            e.post(cmd(CmdType::Kill, 0, static_cast<u8>(count > 255 ? 255 : count)));
        }
        if (rng.uniform() < (drop ? 0.45 : 0.18) * 8.0 / (60.0 * barS)) {
            e.post(cmd(CmdType::Kill, 1, 1));
            const u32 run = 3 + rng.below(6);
            for (u32 k = 0; k < run; ++k) e.post(cmd(CmdType::Shard, 0, 0, 0, 1.0f));
        }
    }
    if (drop) {
        for (double b : {13.5, 15.5, 17.5, 18.5})
            if (atBar(b)) e.post(cmd(CmdType::Kill, 2, 1));
    }
    // Warp-ins land on beats 1 and 3: posted a bar ahead with the beat they end on.
    if (!brk && crossed(std::floor(bar) * barS + 0.001)) {
        const double nextBar = (std::floor(bar) + 1.0) * kTicksPerBar;
        for (double beat : {0.0, 2.0}) {
            Command w = cmd(CmdType::Warp, 0, 0, 0, 1.0f, static_cast<float>(rng.uniform() * 1.4 - 0.7));
            w.tick = nextBar + beat * kPpq;
            e.post(w);
        }
        if (drop) {
            for (double beat : {1.0, 3.0}) {
                Command tick = cmd(CmdType::Sfx, static_cast<u8>(Sfx::Tick), 0, 0, 1.0f, 0.5f);
                tick.when = Timing::AtTick;
                tick.tick = nextBar + (beat - 1.0) * kPpq;
                e.post(tick);
                Command shot = cmd(CmdType::Sfx, static_cast<u8>(Sfx::EnemyShot), 0, 0, 1.0f, 0.5f);
                shot.when = Timing::AtTick;
                shot.tick = nextBar + beat * kPpq;
                e.post(shot);
            }
        }
    }
}

// The full band for the load test: Drop held, all four players firing with mods, every Riff (M5: on --track).
void fullStep(Render& r, u32 track, u32 frameIndex) {
    Engine& e = *r.engine;
    if (frameIndex != 0) return;
    e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(track), 0));
    e.post(cmd(CmdType::SetPlayers, 4, 0));
    e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
    e.post(cmd(CmdType::SetSection, static_cast<u8>(Section::Drop)));
    e.post(cmd(CmdType::HoldSection, 1));
    for (u8 p = 0; p < 4; ++p) {
        e.post(cmd(CmdType::SetFire, p, 1));
        e.post(cmd(CmdType::SetTimbre, p, r.opt.timbre >= 0 ? static_cast<u8>(r.opt.timbre) : p));  // M5: --timbre (profiling)
        for (u8 k = 0; k < 3; ++k) e.post(cmd(CmdType::SetRiff, p, k, 2));
        Command m = cmd(CmdType::SetMods, p);
        m.mods[0] = 1;  // split (the device load test --band full)
        m.mods[1] = 1;  // double time
        e.post(m);
    }
}

// M5: the upgrades' worst case (--band max on a device): the full band with every Riff at level 5 among four players, the
// heaviest Mods and a Boost each, the effects and the upgrades' cues every 1/60 s (engine.h maxLoadSetup / maxLoadFrame).
void maxStep(Render& r, u32 track, u32 frameIndex) {
    Engine& e = *r.engine;
    if (frameIndex == 0) {
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(track), 0));
        Command setup[64];
        const u32 n = maxLoadSetup(setup, 64);
        for (u32 i = 0; i < n; ++i) e.post(setup[i]);
    }
    Command feed[40];
    const u32 n = maxLoadFrame(frameIndex, feed, 40);
    for (u32 i = 0; i < n; ++i) e.post(feed[i]);
}

// The Surge listening test: the conductor with the local player firing throughout, the Groove bed full.
// Groove 4 bars, Build 4, Drop 8 with Surge as conductor.json has it (one grid step: 1/16 -> 1/16
// triplet); then the same sixteen bars with Surge moved two steps (1/32, the staged arrangement's
// "double" lead). Section changes are posted once the bar before them is planned.
constexpr u32 kSurgeBars = 32;
void surgeStep(Render& r, u32 track, u32 frameIndex) {
    Engine& e = *r.engine;
    if (frameIndex == 0) {
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(track), 0));
        e.post(cmd(CmdType::SetForm, static_cast<u8>(Form::Short)));  // Build 4 bars, Drop 8
        e.post(cmd(CmdType::SetPlayers, 1, 0));
        e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
        e.post(cmd(CmdType::SetFire, 0, 1));
    }
    if (r.lastBar < 0 || r.lastBar == r.scriptedBar) return;
    r.scriptedBar = r.lastBar;
    const i32 next = r.lastBar + 1;
    if (next == 4 || next == 20) e.post(cmd(CmdType::SetSection, static_cast<u8>(Section::Build)));
    if (next == 16) {
        e.post(cmd(CmdType::SetSection, static_cast<u8>(Section::Groove)));
        e.post(cmd(CmdType::SetSurge, 2));
    }
}

// M5: the set list as a medley. The conductor driven bar by bar the way the game's director drives it (SetPhrase for
// the next bar, posted while the bar before plays; SetTrack with the first bar of a Track-change Break): Track 1's
// Groove (2 bars) and Build (4), then about 20 s of each Track's Drop, and between two Tracks the real change: the
// eight-bar Break (tempo ramp, key on its fifth bar) and the new Track's Build (its riser at its tempo, the 85% bar)
// into its Drop (impact, Surge). Track 10 ends in four bars of its Break. One player fires in the Builds and Drops and
// rests in the Breaks (the bed echoes the arp); Sentinel, Pulse Ring and Orbitals at level 1, intensity 1.
constexpr double kMedleyDropS = 20.0;
struct MedleyBar {
    u8 track, section, bar, bars;
    bool change;  // the first bar of a Track-change Break
};
constexpr u32 kMedleyMaxBars = 400;

u32 medleyPlan(const MusicData& music, MedleyBar* plan, double& seconds) {
    u32 n = 0;
    seconds = 0.0;
    const auto add = [&](u32 track, Section s, u32 bars, bool change, double bpmA, double bpmB) {
        for (u32 b = 0; b < bars && n < kMedleyMaxBars; ++b) {
            plan[n++] = {static_cast<u8>(track), static_cast<u8>(s), static_cast<u8>(b), static_cast<u8>(bars), change && b == 0};
            seconds += 240.0 / (bpmA < bpmB ? bpmA : bpmB) / (s == Section::Build && b + 1 == bars ? 0.85 : 1.0);
        }
    };
    for (u32 t = 0; t < music.trackCount; ++t) {
        const double bpm = music.tracks[t].bpm;
        if (t == 0) {
            add(0, Section::Groove, 2, false, bpm, bpm);
        } else {
            add(t, Section::Break, 8, true, music.tracks[t - 1].bpm, bpm);
        }
        add(t, Section::Build, 4, false, bpm, bpm);
        add(t, Section::Drop, static_cast<u32>(std::lround(kMedleyDropS * bpm / 240.0)), false, bpm, bpm);
    }
    const u32 last = music.trackCount - 1;
    add(last, Section::Break, 4, false, music.tracks[last].bpm, music.tracks[last].bpm);
    return n;
}

void medleyStep(Render& r, const MedleyBar* plan, u32 planBars, u32 frameIndex) {
    Engine& e = *r.engine;
    if (frameIndex == 0) {
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
        e.post(cmd(CmdType::SetForm, static_cast<u8>(Form::Short)));
        e.post(cmd(CmdType::SetPlayers, 1, 0));
        e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
        e.post(cmd(CmdType::SetFire, 0, 1));
        // The first slice's three Riffs, as above (M5: Riff::Count is twelve now; the medley keeps its sound).
        for (u8 k = 0; k < 3; ++k) e.post(cmd(CmdType::SetRiff, 0, k, 1));
    }
    if (r.lastBar < 0 || r.lastBar == r.scriptedBar) return;
    r.scriptedBar = r.lastBar;
    const u32 next = static_cast<u32>(r.lastBar + 1);
    if (next >= planBars) return;
    const MedleyBar& b = plan[next];
    if (b.change) {
        e.post(cmd(CmdType::SetTrack, b.track, 0));  // through the Break, at the Track's own tempo
        e.post(cmd(CmdType::SetForm, static_cast<u8>(Form::Standard)));
    }
    e.post(cmd(CmdType::SetPhrase, b.section, b.bar, b.bars));
    if (b.bar == 0) {
        // Fire changes exactly on the bar line (the player lets go as the Break begins): posted when the render
        // reaches the frame of that downbeat (the current bar's tempo says when it comes).
        r.postAt = e.tempo().gridFrame(static_cast<double>(static_cast<i64>(next) * kTicksPerBar));
        r.postCmd = cmd(CmdType::SetFire, 0, b.section == static_cast<u8>(Section::Break) ? 0 : 1);
    }
}

void writeMeta(const char* path, const MusicData& music, u32 t, u32 v, const char* kind, u64 frames, const Options& opt) {
    FILE* f = std::fopen(path, "w");
    if (!f) return;
    const Track& tr = music.tracks[t];
    std::fprintf(f, "{\"render\": \"%s\", \"track\": \"%s\", \"sound\": \"%s\", \"bpm\": %.4f, \"rate\": %u, \"frames\": %llu, "
                    "\"burst\": %u, \"random_bursts\": %s, \"sections\": [",
                 kind, tr.id, music.variants[v].id, tr.bpm, kRate, static_cast<unsigned long long>(frames), opt.burst,
                 opt.randomBursts ? "true" : "false");
    u32 first = 0;
    for (u32 i = 0; i < tr.partCount; ++i) {
        std::fprintf(f, "%s[\"%s\", %u, %u]", i ? ", " : "", sectionName(tr.parts[i].section), first, first + tr.parts[i].bars);
        first += tr.parts[i].bars;
    }
    std::fprintf(f, "]}\n");
    std::fclose(f);
}

int renderOne(const Options& opt, const MusicData& music, const SampleBank& bank, u32 t, u32 v, double* lufsOut = nullptr,
              float* peakOut = nullptr) {
    Engine engine;
    if (!engine.init(music, bank, kRate, opt.seed)) {
        RF_ERR("engine init failed");
        return 1;
    }
    Render r;
    r.engine = &engine;
    r.opt = opt;
    const Track& tr = music.tracks[t];
    const double barS = 240.0 / tr.bpm;
    double seconds = opt.seconds;
    const bool staged = eq(opt.render, "staged");
    const bool mock = eq(opt.render, "mock"), sfx = eq(opt.render, "sfx"), leads = eq(opt.render, "leads"),
               full = eq(opt.render, "full"), surge = eq(opt.render, "surge"), medley = eq(opt.render, "medley"),
               maxLoad = eq(opt.render, "max");
    const bool sheetRecipes = eq(opt.render, "recipes"), sheetCues = eq(opt.render, "cues");  // M5 listening sheets
    static SheetStep sheet[256];
    u32 sheetSteps = 0;
    if (sheetRecipes || sheetCues) {
        double endS = 0.0;
        sheetSteps = sheetM5(sheetCues, sheet, 256, endS);
        seconds = static_cast<float>(endS + 1.0);
    }
    // M5: the upgrades' sheets, through the conductor.
    const bool upSheet = eq(opt.render, "riffs") || eq(opt.render, "mods") || eq(opt.render, "boosts") || eq(opt.render, "fusions");
    static UpEntry upPlan[64];
    u32 upCount = 0;
    constexpr double kUpLeadS = 0.3, kUpTailS = 1.6;
    if (upSheet) {
        upCount = upgradeSheetPlan(opt.render, upPlan, 64);
        double s = kUpLeadS;
        for (u32 i = 0; i < upCount; ++i) s += (upPlan[i].barsOff + upPlan[i].barsOn) * 240.0 / music.tracks[upPlan[i].track].bpm + kUpTailS + 0.05;
        seconds = static_cast<float>(s + 0.5);
    }
    if (staged && seconds <= 0.0f) seconds = static_cast<float>(tr.arrangementBars * barS + 2.5);  // musicproto.Mixer length
    if (mock && seconds <= 0.0f) seconds = static_cast<float>(26.0 * barS + 2.5);
    if ((full || maxLoad) && seconds <= 0.0f) seconds = 60.0f;
    static MedleyBar plan[kMedleyMaxBars];
    u32 planBars = 0;
    constexpr double kMedleyTailS = 2.5, kMedleyFadeS = 2.0;
    if (medley) {
        double planS = 0.0;
        planBars = medleyPlan(music, plan, planS);
        seconds = static_cast<float>(planS * 1.05 + 10.0);  // room; cut to the plan's end below
    }
    constexpr double kSurgeTailS = 2.0, kSurgeFadeS = 1.5;  // into the Break that follows, faded out
    if (surge) {
        // Two Builds, each with its last bar at the conductor's slower speed.
        const double dip = barS / music.conductor.buildLastBarSpeed - barS;
        seconds = static_cast<float>(kSurgeBars * barS + 2.0 * dip + kSurgeTailS);
    }
    double sheetLen = 0.0;
    if (sfx) {
        sheetSfx(r, bank, sheetLen, true);
        seconds = static_cast<float>(sheetLen);
    }
    if (leads) seconds = static_cast<float>(4 * 4.5 + 0.5);
    if (seconds <= 0.0f) {
        RF_ERR("unknown render '%s' (staged, mock, sfx, leads, full, surge, medley, max, recipes, cues, riffs, mods, boosts, fusions, "
               "upgrade-levels)",
               opt.render);
        return 2;
    }
    r.frames = static_cast<u64>(seconds * kRate);
    r.out = static_cast<float*>(std::calloc(r.frames * 2, sizeof(float)));
    if (!r.out) return 1;
    if (opt.events) {
        r.events = std::fopen(opt.events, "w");
        if (r.events) std::fprintf(r.events, "frame,tick,kind,a,b,c,value,index,x\n");
    }
    Stems stems;
    if (opt.stems) {
        for (u32 b = 0; b <= kBusCount; ++b) {
            char path[512];
            std::snprintf(path, sizeof path, "%s/%s.f32", opt.stems, b < kBusCount ? busName(static_cast<Bus>(b)) : "reverb");
            stems.f[b] = std::fopen(path, "wb");
        }
        engine.setStemTap(stemTap, &stems);
    }
    engine.setRawOutput(opt.raw);
    EngineProfile profile;
    if (opt.profile) engine.setProfile(&profile);
    if (v != music.defaultVariant) engine.post(cmd(CmdType::SetVariant, static_cast<u8>(v)));
    const auto t0 = std::chrono::steady_clock::now();
    const u64 allocsBefore = heapAllocCountThisThread();
    if (staged) {
        engine.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Staged), static_cast<u8>(t), 0));
        renderTo(r, r.frames);
    } else if (sheetRecipes || sheetCues) {
        // Dry on the effects bus like the effect sheet, on the Track asked for (default Track 1), with the list.
        engine.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), static_cast<u8>(t), 0));
        FILE* list = nullptr;
        if (opt.out) {
            char path[512];
            std::snprintf(path, sizeof path, "%s", opt.out);
            char* dot = std::strrchr(path, '.');
            if (dot) std::snprintf(dot, sizeof path - static_cast<size_t>(dot - path), ".txt");
            list = std::fopen(path, "w");
        }
        for (u32 i = 0; i < sheetSteps; ++i) {
            renderTo(r, static_cast<u64>(sheet[i].at * kRate));
            engine.post(sheet[i].c);
            if (sheet[i].what) {
                const double m = std::floor(sheet[i].at / 60.0);
                if (list) std::fprintf(list, "%.0f:%05.2f  %s\n", m, sheet[i].at - 60.0 * m, sheet[i].what);
                std::printf("sheet %.0f:%05.2f  %s\n", m, sheet[i].at - 60.0 * m, sheet[i].what);
            }
        }
        if (list) std::fclose(list);
        renderTo(r, r.frames);
    } else if (upSheet) {
        FILE* list = nullptr;
        if (opt.out) {
            char path[512];
            std::snprintf(path, sizeof path, "%s", opt.out);
            char* dot = std::strrchr(path, '.');
            if (dot) std::snprintf(dot, sizeof path - static_cast<size_t>(dot - path), ".txt");
            list = std::fopen(path, "w");
        }
        const auto say = [&](u64 frame, const char* what, const char* part) {
            const double at = static_cast<double>(frame) / kRate, m = std::floor(at / 60.0);
            if (list) std::fprintf(list, "%.0f:%05.2f  %s%s\n", m, at - 60.0 * m, part, what);
            std::printf("sheet %.0f:%05.2f  %s%s\n", m, at - 60.0 * m, part, what);
        };
        u64 at = static_cast<u64>(kUpLeadS * kRate);
        for (u32 i = 0; i < upCount; ++i) {
            const UpEntry& u = upPlan[i];
            renderTo(r, at);
            engine.post(cmd(CmdType::Reset));
            engine.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), u.track, 0));
            engine.post(cmd(CmdType::SetPlayers, 1, 0));
            engine.post(cmd(CmdType::SetIntensity, 0, 0, 0, u.intensity));
            engine.post(cmd(CmdType::SetForm, static_cast<u8>(Form::Short)));
            if (!u.drop) engine.post(cmd(CmdType::HoldSection, 1));
            engine.post(cmd(CmdType::SetFire, 0, u.fire ? 1 : 0));
            postLoadout(engine, u.off);
            renderTo(r, at + 1);  // the song starts on `at`
            r.lastBar = 0;
            const auto barFrame = [&](u32 b) { return static_cast<u64>(engine.tempo().gridFrame(static_cast<double>(b) * kTicksPerBar)); };
            say(at, u.what, "");
            // Bar by bar: the Drop named by the director's SetPhrase a bar ahead; the upgrade taken on its bar line; the feed.
            u32 named = 1, frame = 0;
            bool switched = false, hitDone = false;
            const u32 bars = u.barsOff + u.barsOn;
            while (r.done < barFrame(bars)) {
                if (u.drop && named < bars && r.lastBar >= static_cast<i32>(named) - 1) {
                    engine.post(cmd(CmdType::SetPhrase, static_cast<u8>(Section::Drop), static_cast<u8>((named - 1) % 8), 8));
                    ++named;
                }
                if (!switched && r.done >= barFrame(u.barsOff)) {
                    postLoadout(engine, u.on);
                    say(r.done, "", "        with it");
                    switched = true;
                    frame = 0;
                }
                if (u.feed != Feed::None && (switched || u.feedOff))
                    feedFrame(engine, u.feed, frame, static_cast<u32>(r.lastBar < 0 ? 0 : r.lastBar), u.barsOff + 10, hitDone);
                ++frame;
                u64 until = r.done + kRate / 60;
                if (!switched && until > barFrame(u.barsOff)) until = barFrame(u.barsOff);  // the bar line exactly
                if (until > barFrame(bars)) until = barFrame(bars);
                renderTo(r, until);
            }
            engine.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Silent), u.track, 0));  // the tails ring out
            at = r.done + static_cast<u64>(kUpTailS * kRate);
        }
        if (list) std::fclose(list);
        renderTo(r, r.frames);
    } else if (sfx || leads) {
        engine.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), static_cast<u8>(t), 0));
        if (sfx) {
            double unused = 0.0;
            sheetSfx(r, bank, unused, false);
        } else {
            sheetLeads(r, music);
        }
        renderTo(r, r.frames);
    } else {
        Random rng(opt.seed + 99);
        u32 frameIndex = 0;
        u64 medleyEnd = 0;
        while (r.done < r.frames) {
            if (mock) mockStep(r, music, frameIndex, rng);
            if (full) fullStep(r, t, frameIndex);
            if (maxLoad) maxStep(r, t, frameIndex);
            if (surge) surgeStep(r, t, frameIndex);
            if (medley) {
                medleyStep(r, plan, planBars, frameIndex);
                if (!medleyEnd && r.lastBar >= static_cast<i32>(planBars) - 1) {
                    // The plan's last bar is playing: the medley ends a tail after its end.
                    const double endTick = static_cast<double>(static_cast<i64>(planBars) * kTicksPerBar);
                    medleyEnd = static_cast<u64>(engine.tempo().gridFrame(endTick)) + static_cast<u64>(kMedleyTailS * kRate);
                    if (medleyEnd < r.frames) r.frames = medleyEnd;
                }
            }
            ++frameIndex;
            if (maxLoad) {  // M5: the lead voices of each player, once a 1/60 s
                static LeadVoiceInfo info[128];
                const u32 nv = engine.leadVoices(info, 128);
                u32 per[8] = {};
                for (u32 i = 0; i < nv; ++i) ++per[info[i].player & 7u];
                for (u32 pl = 0; pl < 4; ++pl) {
                    r.leadSum[pl] += per[pl];
                    if (per[pl] > r.leadMax[pl]) r.leadMax[pl] = per[pl];
                }
                ++r.leadSamples;
            }
            const u64 until = r.done + kRate / 60;
            if (r.postAt >= 0 && static_cast<u64>(r.postAt) <= until) {
                renderTo(r, static_cast<u64>(r.postAt));
                engine.post(r.postCmd);
                r.postAt = -1;
            }
            renderTo(r, until);
        }
    }
    drainEvents(r);
    if (medley) {
        // Faded out over the last seconds (Track 10's Break), then where each Track's Drop began.
        const u64 fade = static_cast<u64>(kMedleyFadeS * kRate);
        for (u64 i = 0; i < fade && i < r.frames; ++i) {
            const float g = static_cast<float>(i) / static_cast<float>(fade);
            r.out[(r.frames - 1 - i) * 2] *= g;
            r.out[(r.frames - 1 - i) * 2 + 1] *= g;
        }
        seconds = static_cast<float>(static_cast<double>(r.frames) / kRate);
        u32 track = 0;
        for (u32 i = 0; i < r.sectionCount; ++i) {
            const auto& s = r.sections[i];
            for (u32 k = 0; k < r.trackStartCount; ++k)
                if (r.trackStarts[k].bar <= s.bar) track = r.trackStarts[k].track;
            const double m = std::floor(s.seconds / 60.0);
            std::printf("medley: %-6s bar %3u at %7.3f s (%.0f:%06.3f)  %s\n", sectionName(s.section), s.bar, s.seconds, m,
                        s.seconds - 60.0 * m, music.tracks[track].name);
        }
        for (u32 k = 0; k < r.trackStartCount; ++k)
            std::printf("medley: Track %u (%s) takes over on bar %u at %.3f s\n", r.trackStarts[k].track + 1,
                        music.tracks[r.trackStarts[k].track].name, r.trackStarts[k].bar, r.trackStarts[k].seconds);
    }
    if (surge) {
        // Faded out in the Break that follows the second Drop; then where each section began.
        const u64 fade = static_cast<u64>(kSurgeFadeS * kRate);
        for (u64 i = 0; i < fade && i < r.frames; ++i) {
            const float g = static_cast<float>(i) / static_cast<float>(fade);
            r.out[(r.frames - 1 - i) * 2] *= g;
            r.out[(r.frames - 1 - i) * 2 + 1] *= g;
        }
        for (u32 i = 0; i < r.sectionCount; ++i) {
            const auto& s = r.sections[i];
            const double m = std::floor(s.seconds / 60.0);
            std::printf("surge: %-6s from bar %2u at %6.3f s (%.0f:%06.3f)%s\n", sectionName(s.section), s.bar, s.seconds, m,
                        s.seconds - 60.0 * m, s.section == Section::Drop ? (s.bar < 16 ? "  Surge 1/16 triplet (the rule)" : "  Surge 1/32") : "");
        }
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const u64 allocs = heapAllocCountThisThread() - allocsBefore;
    for (FILE* f : stems.f)
        if (f) std::fclose(f);
    if (r.events) std::fclose(r.events);

    float peak = 0.0f;
    for (u64 i = 0; i < r.frames * 2; ++i) peak = std::fabs(r.out[i]) > peak ? std::fabs(r.out[i]) : peak;
    if (sfx || leads || sheetRecipes || sheetCues) {
        // sfxproto.finish(): after the tanh, scaled down to -1 dBFS peak when louder.
        const float limit = std::pow(10.0f, -1.0f / 20.0f);
        if (peak > limit)
            for (u64 i = 0; i < r.frames * 2; ++i) r.out[i] *= limit / peak;
    }
    const double lufs = integratedLoudness(r.out, r.frames, 2, kRate);
    if (lufsOut) *lufsOut = lufs;
    if (peakOut) *peakOut = peak;
    const EngineStats& st = engine.stats();
    std::printf("render %s %s/%s: %.2f s of audio in %.2f s (%.1fx real time), lufs %.2f, peak %.2f dBFS, events %llu "
                "(dropped %u), voices peak %u (lead %u), steals %u, heap allocations while rendering %llu\n",
                opt.render, tr.id, music.variants[v].id, seconds, elapsed, seconds / elapsed, lufs, 20.0 * std::log10(peak + 1e-12),
                static_cast<unsigned long long>(r.eventCount), engine.eventsDropped(), st.voicesPeak.load(), st.leadPeak.load(), st.steals.load(),
                static_cast<unsigned long long>(allocs));
    if (opt.out) {
        if (!writeWav(opt.out, r.out, r.frames, 2, kRate, opt.pcm16)) {
            RF_ERR("could not write %s", opt.out);
            std::free(r.out);
            return 1;
        }
        std::printf("wrote %s\n", opt.out);
    }
    if (opt.meta) writeMeta(opt.meta, music, t, v, opt.render, r.frames, opt);
    if (maxLoad && r.leadSamples) {
        std::printf("lead voices by player (mean / most):");
        for (u32 pl = 0; pl < 4; ++pl)
            std::printf("  %u: %.1f / %u", pl, static_cast<double>(r.leadSum[pl]) / static_cast<double>(r.leadSamples), r.leadMax[pl]);
        std::printf("\n");
    }
    if (opt.profile) {
        u64 total = 0;
        for (u64 ns : profile.ns) total += ns;
        std::printf("profile (%% of the engine's block time, %.1f ms per second of audio):", total * 1e-6 / seconds);
        for (u32 s = 0; s < EngineProfile::Count; ++s) std::printf("  %s %.1f", EngineProfile::name(s), total ? 100.0 * profile.ns[s] / total : 0.0);
        std::printf("\n");
    }
    std::free(r.out);
    return 0;
}

int calibrate(const Options& opt, const MusicData& music, const SampleBank& bank) {
    char text[4096];
    int n = std::snprintf(text, sizeof text,
                          "{\n \"note\": \"Written by raveform_headless --calibrate (scripts/calibrate-audio.ps1): the output gain that "
                          "puts each Track's staged arrangement at target_lufs in each sound variant, measured on the engine's own "
                          "render the way musicproto.py normalises its renders. 'hash' identifies the music data and DSP version "
                          "it was made from; the engine warns when they differ.\",\n \"hash\": \"%016llx\",\n \"dsp_version\": %u,\n "
                          "\"gain_db\": {",
                          static_cast<unsigned long long>(music.hash), kDspVersion);
    bool stale = !music.calibrationCurrent;
    for (u32 t = 0; t < music.trackCount; ++t) {
        n += std::snprintf(text + n, sizeof text - n, "%s\n  \"%s\": {", t ? "," : "", music.tracks[t].id);
        for (u32 v = 0; v < music.variantCount; ++v) {
            Options o = opt;
            o.render = "staged";
            o.out = nullptr;
            o.events = nullptr;
            o.stems = nullptr;
            o.meta = nullptr;
            o.raw = true;
            o.seconds = 0.0f;
            double lufs = 0.0;
            float peak = 0.0f;
            if (renderOne(o, music, bank, t, v, &lufs, &peak) != 0) return 1;
            const float target = music.sound[t][v].targetLufs;
            double gainDb = target - lufs;
            const double capDb = 20.0 * std::log10(0.891 / (peak > 0.0f ? peak : 1.0f));
            if (gainDb > capDb) gainDb = capDb;  // musicproto: min(gain, 0.891 / peak)
            const double had = music.outputGainDb(t, v);
            if (std::fabs(had - gainDb) > 0.05) stale = true;
            std::printf("calibration %s/%s: raw %.2f LUFS, peak %.3f -> gain %+.3f dB (stored %+.3f)\n", music.tracks[t].id,
                        music.variants[v].id, lufs, peak, gainDb, had);
            n += std::snprintf(text + n, sizeof text - n, "%s\"%s\": %.4f", v ? ", " : "", music.variants[v].id, gainDb);
        }
        n += std::snprintf(text + n, sizeof text - n, "}");
    }
    n += std::snprintf(text + n, sizeof text - n, "\n }\n}\n");
    if (opt.write) {
        FILE* f = std::fopen(opt.write, "w");
        if (!f || std::fwrite(text, 1, static_cast<size_t>(n), f) != static_cast<size_t>(n)) {
            RF_ERR("could not write %s", opt.write);
            if (f) std::fclose(f);
            return 1;
        }
        std::fclose(f);
        std::printf("wrote %s\n", opt.write);
    }
    if (opt.check && stale) {
        std::printf("calibration is out of date: run scripts\\calibrate-audio.ps1\n");
        return 3;
    }
    return 0;
}

}  // namespace

// M3: the simulation's headless runs (src/tools/sim_headless.cpp): --sim determinism | soak | stress.
int simHeadlessMain(int argc, char** argv);
// M4: co-op without a screen (src/tools/net_headless.cpp): --net soak | host | join | fuzz.
int netHeadlessMain(int argc, char** argv);

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--sim") == 0) return simHeadlessMain(argc, argv);
    if (argc > 1 && std::strcmp(argv[1], "--net") == 0) return netHeadlessMain(argc, argv);
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        const char* next = i + 1 < argc ? argv[i + 1] : nullptr;
        const auto take = [&](const char*& dst) {
            if (!next) return false;
            dst = next;
            ++i;
            return true;
        };
        bool ok = true;
        if (eq(a, "--render")) ok = take(opt.render);
        else if (eq(a, "--track")) ok = take(opt.track);
        else if (eq(a, "--sound")) ok = take(opt.sound);
        else if (eq(a, "--out")) ok = take(opt.out);
        else if (eq(a, "--events")) ok = take(opt.events);
        else if (eq(a, "--stems")) ok = take(opt.stems);
        else if (eq(a, "--meta")) ok = take(opt.meta);
        else if (eq(a, "--write")) ok = take(opt.write);
        else if (eq(a, "--dump-samples")) ok = take(opt.dumpSamples);
        else if (eq(a, "--dump-reverb")) ok = take(opt.dumpReverb);
        else if (eq(a, "--seed-search")) {
            const char* what = nullptr;
            ok = take(what) && eq(what, "kick");
        } else if (eq(a, "--target")) ok = take(opt.seedTarget);
        else if (eq(a, "--calibrate")) opt.calibrate = true;
        else if (eq(a, "--check")) opt.check = true;
        else if (eq(a, "--pcm16")) opt.pcm16 = true;
        else if (eq(a, "--raw")) opt.raw = true;
        else if (eq(a, "--profile")) opt.profile = true;
        else if (eq(a, "--random-bursts")) opt.randomBursts = true;
        else if (eq(a, "--burst") && next) {
            opt.burst = static_cast<u32>(std::atoi(next));
            ++i;
            ok = opt.burst > 0;
        } else if (eq(a, "--seconds") && next) {
            opt.seconds = static_cast<float>(std::atof(next));
            ++i;
        } else if (eq(a, "--timbre") && next) {
            opt.timbre = std::atoi(next);
            ++i;
            ok = opt.timbre >= 0 && opt.timbre < static_cast<i32>(Timbre::Count);
        } else if (eq(a, "--seed") && next) {
            opt.seed = static_cast<u64>(std::atoll(next));
            ++i;
        } else ok = false;
        if (!ok) {
            std::fprintf(stderr, "bad argument: %s\n", a);
            return 2;
        }
    }
    MusicData* music = new MusicData;
    if (!music->load()) return 1;
    SampleBank* bank = new SampleBank;
    if (!bank->bake(*music, kRate)) return 1;
    int code = 0;
    if (opt.seedTarget) {
        // The kick's click is a short burst of filtered noise: its spectrum depends on the draw. Find the
        // draws whose kick spectrum (six bands, dB of the 30 Hz-16 kHz total) is nearest the prototype's.
        float target[6];
        if (std::sscanf(opt.seedTarget, "%f,%f,%f,%f,%f,%f", &target[0], &target[1], &target[2], &target[3], &target[4], &target[5]) != 6)
            return 2;
        const double edges[7] = {30, 80, 250, 800, 2500, 6000, 16000};
        double best[5] = {1e9, 1e9, 1e9, 1e9, 1e9};
        u64 bestSeed[5] = {};
        for (u64 seed = 1; seed <= 3000; ++seed) {
            u32 n = 0;
            float* k = bakeKickWithSeed(music->sound[0][music->defaultVariant], kRate, seed, n);
            u32 m = 1;
            while (m < n) m <<= 1;
            Complex* spec = static_cast<Complex*>(std::calloc(m, sizeof(Complex)));
            for (u32 i = 0; i < n; ++i) spec[i].re = k[i];
            fftPow2(spec, m, false);
            double band[6] = {}, total = 0.0;
            for (u32 b = 1; b < m / 2; ++b) {
                const double f = static_cast<double>(b) * kRate / m, e = spec[b].re * spec[b].re + spec[b].im * spec[b].im;
                if (f >= 30.0 && f < 16000.0) total += e;
                for (u32 i = 0; i < 6; ++i)
                    if (f >= edges[i] && f < edges[i + 1]) band[i] += e;
            }
            double dist = 0.0;
            for (u32 i = 3; i < 6; ++i) {  // the click's bands: mid, high-mid, high
                const double d = 10.0 * std::log10(band[i] / total + 1e-30) - target[i];
                dist += d * d;
            }
            for (u32 r = 0; r < 5; ++r) {
                if (dist < best[r]) {
                    for (u32 q = 4; q > r; --q) {
                        best[q] = best[q - 1];
                        bestSeed[q] = bestSeed[q - 1];
                    }
                    best[r] = dist;
                    bestSeed[r] = seed;
                    break;
                }
            }
            std::free(spec);
            std::free(k);
        }
        for (u32 r = 0; r < 5; ++r) std::printf("seed %llu: rms band error %.2f dB\n", static_cast<unsigned long long>(bestSeed[r]), std::sqrt(best[r] / 3.0));
        return 0;
    }
    if (opt.dumpReverb) {
        // The reverb's impulse response, both channels (Track 1, the default sound): <file> as float32 L then R.
        FdnReverb* rv = new FdnReverb;
        const ReverbPatch rp = music->sound[0][music->defaultVariant].reverb;
        if (!rv->init(kRate, &rp, 1)) return 1;
        const u32 n = kRate * 3;
        float* ir = static_cast<float*>(std::calloc(2 * n, sizeof(float)));
        rv->impulseResponse(0, 0, ir, n);
        rv->impulseResponse(0, 1, ir + n, n);
        if (FILE* f = std::fopen(opt.dumpReverb, "wb")) {
            std::fwrite(ir, sizeof(float), 2 * n, f);
            std::fclose(f);
        }
        std::free(ir);
        delete rv;
        return 0;
    }
    if (opt.dumpSamples) {
        // Every baked one-shot as raw float32 (interleaved when stereo): <dir>/<name>.f32
        for (u32 i = 0; i < bank->count(); ++i) {
            const Sample& s = bank->sample(static_cast<i32>(i));
            char path[512];
            std::snprintf(path, sizeof path, "%s/%s.f32", opt.dumpSamples, s.name);
            if (FILE* f = std::fopen(path, "wb")) {
                std::fwrite(s.data, sizeof(float), static_cast<size_t>(s.frames) * s.channels, f);
                std::fclose(f);
            }
        }
        std::printf("dumped %u samples to %s (bake %.0f ms)\n", bank->count(), opt.dumpSamples, bank->bakeSeconds() * 1000.0);
        return 0;
    }
    if (opt.calibrate) {
        code = calibrate(opt, *music, *bank);
    } else if (eq(opt.render, "upgrade-levels")) {
        code = upgradeLevels(*music, *bank);  // M5
    } else {
        const i32 t = music->trackIndex(opt.track);
        const i32 v = opt.sound ? music->variantIndex(opt.sound) : static_cast<i32>(music->defaultVariant);
        if (t < 0 || v < 0) {
            std::fprintf(stderr, "unknown track '%s' or sound '%s'\n", opt.track, opt.sound ? opt.sound : "");
            return 2;
        }
        code = renderOne(opt, *music, *bank, static_cast<u32>(t), static_cast<u32>(v));
    }
    delete bank;
    delete music;
    return code;
}
