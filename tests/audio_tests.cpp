// Unit tests for the band (M2): the lock-free queues, the tempo map, sequencer timing (onsets on the grid,
// identical output whatever the device's burst sizes), the lead rule, variant changes on the bar line,
// the music data and allocation-free rendering. Run by scripts/test.ps1. Exit code: failures (0 = pass).
// M5: the ten Tracks of the set list: every Track's forms and lead, the nine Track changes from 120 to 174 BPM
// driven as the game's director drives them, and the Tracks without a kick on every beat (Afterglow, Mirage,
// Last Light): beats and bars from the clock, the sidechain from the pattern's kicks, the Build and the fills.

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "audio/bake.h"
#include "audio/engine.h"
#include "audio/music_data.h"
#include "audio/spsc.h"
#include "audio/tempo_map.h"
#include "content/palette.h"
#include "core/alloc_counter.h"

using namespace rf;
using namespace rf::audio;

namespace {

int g_checks = 0, g_failures = 0;
const char* g_test = "";

#define CHECK(cond)                                                                         \
    do {                                                                                    \
        ++g_checks;                                                                         \
        if (!(cond)) {                                                                      \
            ++g_failures;                                                                   \
            std::printf("  FAIL [%s] %s:%d: %s\n", g_test, __FILE__, __LINE__, #cond);    \
        }                                                                                   \
    } while (0)
#define CHECK_MSG(cond, ...)                                                                \
    do {                                                                                    \
        ++g_checks;                                                                         \
        if (!(cond)) {                                                                      \
            ++g_failures;                                                                   \
            std::printf("  FAIL [%s] %s:%d: %s: ", g_test, __FILE__, __LINE__, #cond);     \
            std::printf(__VA_ARGS__);                                                       \
            std::printf("\n");                                                              \
        }                                                                                   \
    } while (0)

constexpr u32 kRate = 48000;
MusicData* g_music = nullptr;
SampleBank* g_bank = nullptr;

Command cmd(CmdType t, u8 a = 0, u8 b = 0, u8 c = 0, float x = 0.0f) {
    Command k;
    k.type = t;
    k.a = a;
    k.b = b;
    k.c = c;
    k.x = x;
    return k;
}

// ---------------------------------------------------------------- queues

void testSpscSingleThread() {
    SpscRing<u32, 8> q;
    u32 v = 0;
    CHECK(!q.pop(v));
    for (u32 i = 0; i < 8; ++i) CHECK(q.push(i));
    CHECK(!q.push(99));  // full: dropped, not blocking
    CHECK(q.dropped() == 1);
    CHECK(q.size() == 8);
    for (u32 i = 0; i < 8; ++i) {
        CHECK(q.pop(v));
        CHECK(v == i);
    }
    CHECK(!q.pop(v));
    // Wrap-around many times.
    for (u32 i = 0; i < 1000; ++i) {
        CHECK(q.push(i));
        CHECK(q.pop(v) && v == i);
    }
}

void testSpscThreads() {
    static SpscRing<u64, 1024> q;
    constexpr u64 kCount = 2000000;
    std::atomic<bool> ok{true};
    std::thread consumer([&] {
        u64 expect = 0, v = 0;
        while (expect < kCount) {
            if (q.pop(v)) {
                if (v != expect) ok = false;
                ++expect;
            }
        }
    });
    for (u64 i = 0; i < kCount;) {
        if (q.push(i)) ++i;
    }
    consumer.join();
    CHECK(ok.load());
    CHECK(q.size() == 0);
}

void testSeqLatest() {
    struct Four {
        i64 a, b, c, d;
    };
    static SeqLatest<Four> s;
    std::atomic<bool> stop{false}, torn{false};
    std::atomic<u64> reads{0};
    std::thread reader([&] {
        Four f;
        while (!stop.load()) {
            if (s.read(f)) {
                if (f.a != f.b || f.b != f.c || f.c != f.d) torn = true;
                reads.fetch_add(1);
            }
        }
    });
    for (i64 i = 1; i < 2000000; ++i) s.publish({i, i, i, i});
    stop = true;
    reader.join();
    CHECK(!torn.load());
    CHECK(reads.load() > 0);
}

// ---------------------------------------------------------------- tempo map

void testTempoConstant() {
    TempoMap m;
    m.reset(120.0, kRate);
    CHECK(m.gridFrame(24.0) == 6000);         // a sixteenth at 120 BPM
    CHECK(m.gridFrame(384.0 * 24) == 2304000);  // 24 bars
    CHECK(std::fabs(m.tickAt(6000.0) - 24.0) < 1e-9);
    // 126 BPM: the prototype's int(round(t * SR)) for every sixteenth of 32 bars.
    m.reset(126.0, kRate);
    const double step = 60.0 / 126.0 / 4.0;
    bool all = true;
    for (u32 b = 0; b < 32; ++b)
        for (u32 s = 0; s < 16; ++s) {
            const double t = b * 4.0 * 60.0 / 126.0 + s * step;
            const i64 proto = static_cast<i64>(std::llround(t * kRate));
            if (m.gridFrame(static_cast<double>((b * 16 + s) * 24)) != proto) all = false;
        }
    CHECK(all);
    for (double f : {0.0, 1.5, 12345.25, 2.0e6})
        CHECK(std::fabs(m.frameAt(m.tickAt(f)) - f) < 1e-6);
}

void testTempoChange() {
    TempoMap m;
    m.reset(120.0, kRate);
    const double tpf = m.last().ticksPerFrame;
    m.changeAt(384.0 * 3, tpf * 0.85);  // the Build's last bar at 85%
    m.changeAt(384.0 * 4, tpf);         // back on the downbeat
    CHECK(m.count() == 3);
    CHECK(std::fabs(m.frameAt(384.0 * 3) - 3.0 * 96000.0) < 1e-6);
    CHECK(std::fabs(m.frameAt(384.0 * 4) - (3.0 * 96000.0 + 96000.0 / 0.85)) < 1e-6);
    CHECK(std::fabs(m.tickAt(3.0 * 96000.0 + 96000.0 / 0.85) - 384.0 * 4) < 1e-9);
    // Continuity: ticks never go back across a change.
    double prev = -1.0;
    bool mono = true;
    for (double f = 280000.0; f < 420000.0; f += 37.0) {
        const double t = m.tickAt(f);
        if (t < prev) mono = false;
        prev = t;
    }
    CHECK(mono);
    // Capacity: the newest segments survive.
    for (u32 i = 0; i < TempoMap::kCapacity + 10; ++i) m.changeAt(384.0 * (5 + i), tpf * (i % 2 ? 1.0 : 0.9));
    CHECK(m.count() == TempoMap::kCapacity);
    CHECK(m.last().tick == 384.0 * (5 + TempoMap::kCapacity + 9));
}

// ---------------------------------------------------------------- engine timing

struct Capture {
    float* out = nullptr;
    u64 frames = 0;
    BeatEvent* events = nullptr;
    u32 eventCount = 0;
    u64 allocs = 0;
};

// Renders `frames` with the given burst pattern (0: random 1..1000), draining events as a main thread would.
void runEngine(Engine& e, Capture& cap, u64 frames, u32 burst, u32 seed = 7) {
    cap.out = static_cast<float*>(std::calloc(frames * 2, sizeof(float)));
    cap.events = static_cast<BeatEvent*>(std::calloc(20000, sizeof(BeatEvent)));
    cap.frames = frames;
    Random rng(seed);
    u64 done = 0;
    while (done < frames) {
        u32 n = burst ? burst : 1 + rng.below(1000);
        if (done + n > frames) n = static_cast<u32>(frames - done);
        const u64 a0 = heapAllocCountThisThread();
        e.render(cap.out + done * 2, n, 2);
        cap.allocs += heapAllocCountThisThread() - a0;
        done += n;
        BeatEvent ev;
        while (e.popEvent(ev))
            if (cap.eventCount < 20000) cap.events[cap.eventCount++] = ev;
    }
}

void freeCapture(Capture& c) {
    std::free(c.out);
    std::free(c.events);
}

// M5: an engine driven the way the main thread drives it: rendered in bursts, its events drained after each burst
// into a log, the tempo map mirrored from the Tempo events (Band::drainEvents) and every onset checked against the
// mirror as it arrives (old segments leave the mirror's 64 as the song goes on). Optionally the sidechain envelope
// frame by frame (bursts of one block).
struct Session {
    Engine e;
    BeatEvent* log = nullptr;
    u32 count = 0, cap = 0;
    TempoMap mirror;
    bool mirrored = false;
    u64 frame = 0, allocs = 0;
    u32 onsets = 0, offGrid = 0;
    float* duck = nullptr;  // per frame (record = true)
    u64 duckCap = 0;
    bool ok = false;

    explicit Session(u32 logCap = 200000, u64 duckFrames = 0) : cap(logCap), duckCap(duckFrames) {
        log = static_cast<BeatEvent*>(std::calloc(logCap, sizeof(BeatEvent)));
        if (duckFrames) duck = static_cast<float*>(std::calloc(duckFrames, sizeof(float)));
        ok = log && (!duckFrames || duck) && e.init(*g_music, *g_bank, kRate);
    }
    ~Session() {
        std::free(log);
        std::free(duck);
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    void post(const Command& c) { e.post(c); }

    // Renders `frames`; onEvent(ev) sees each drained event (after the mirror took it).
    template <typename F>
    void run(u64 frames, u32 burst, F&& onEvent) {
        static float buf[2 * 1024];
        const u64 end = frame + frames;
        while (frame < end) {
            u32 n = burst;
            if (frame + n > end) n = static_cast<u32>(end - frame);
            const u64 a0 = heapAllocCountThisThread();
            e.render(buf, n, 2);
            allocs += heapAllocCountThisThread() - a0;
            if (duck && n == kBlock && e.duckBlockFrames() == kBlock && frame + kBlock <= duckCap)
                std::memcpy(duck + frame, e.duckBlock(), sizeof(float) * kBlock);
            frame += n;
            BeatEvent ev;
            while (e.popEvent(ev)) {
                if (ev.kind == BeatKind::Tempo) {
                    TempoSegment s{static_cast<double>(ev.frame), ev.tick, ev.x};
                    if (ev.a == 1) {
                        mirror.appendSegment(s);
                    } else if (ev.tick == 0.0) {
                        mirror = TempoMap{};
                        mirror.appendSegment(s);
                        mirrored = true;
                    } else {
                        mirror.changeAt(ev.tick, ev.x);
                    }
                } else if (ev.tick >= 0.0 && mirrored) {
                    ++onsets;
                    if (ev.frame != mirror.gridFrame(ev.tick)) ++offGrid;
                }
                if (count < cap) log[count++] = ev;
                onEvent(ev);
            }
        }
    }
    void run(u64 frames, u32 burst = 256) {
        run(frames, burst, [](const BeatEvent&) {});
    }
};

double barFrames(u32 track) { return 240.0 / g_music->tracks[track].bpm * kRate; }
u32 patternHits(u32 track, Pat p) {
    u32 n = 0;
    for (float v : g_music->tracks[track].pattern[static_cast<u32>(p)]) n += v > 0.0f;
    return n;
}

void testStagedBurstIndependence() {
    // Each Track's song rendered in bursts of 192 and in random bursts: identical samples and events.
    const u64 frames = static_cast<u64>(kRate) * 20;
    for (u32 t = 0; t < g_music->trackCount; ++t) {
        Capture a, b;
        {
            Engine e;
            CHECK(e.init(*g_music, *g_bank, kRate));
            e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Staged), static_cast<u8>(t), 0));
            runEngine(e, a, frames, 192);
        }
        {
            Engine e;
            CHECK(e.init(*g_music, *g_bank, kRate));
            e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Staged), static_cast<u8>(t), 0));
            runEngine(e, b, frames, 0);
        }
        u64 diff = 0, first = ~0ull;
        float worst = 0.0f;
        for (u64 i = 0; i < frames * 2; ++i) {
            if (a.out[i] == b.out[i]) continue;
            ++diff;
            if (first == ~0ull) first = i / 2;
            worst = std::fmax(worst, std::fabs(a.out[i] - b.out[i]));
        }
        if (diff) std::printf("  %s: first difference at frame %llu, largest %.3g\n", g_music->tracks[t].id, first, worst);
        CHECK_MSG(diff == 0, "%s: %llu samples differ between burst patterns", g_music->tracks[t].id,
                  static_cast<unsigned long long>(diff));
        CHECK(a.eventCount == b.eventCount && a.eventCount > 100);
        bool same = a.eventCount == b.eventCount;
        for (u32 i = 0; same && i < a.eventCount; ++i)
            same = a.events[i].frame == b.events[i].frame && a.events[i].kind == b.events[i].kind;
        CHECK(same);
        CHECK_MSG(a.allocs == 0 && b.allocs == 0, "render allocated %llu / %llu times",
                  static_cast<unsigned long long>(a.allocs), static_cast<unsigned long long>(b.allocs));
        // Every onset on its grid position: frame == round(tick * frames per tick) (constant tempo).
        const double fpt = 60.0 * kRate / (g_music->tracks[t].bpm * 96.0);
        u32 checked = 0, off = 0;
        for (u32 i = 0; i < a.eventCount; ++i) {
            const BeatEvent& e = a.events[i];
            if (e.tick < 0.0 || e.kind == BeatKind::Tempo) continue;
            ++checked;
            if (std::llabs(e.frame - static_cast<i64>(std::llround(e.tick * fpt))) > 0) ++off;
        }
        CHECK_MSG(checked > 300 && off == 0, "%s: %u of %u onsets off the grid", g_music->tracks[t].id, off, checked);
        freeCapture(a);
        freeCapture(b);
    }
}

// The conductor through a whole phrase cycle of every Track (M5: all ten; the Build's 85% bar in each): the form
// (Track 1 Short 8 / 4 / 8 / 4, the others Standard 12 / 4 / 16 / 4), the tempo dip and its return, a lead note on
// every grid step, and every onset on the grid of the tempo map mirrored from the Tempo events, as the main thread
// sees it. Random bursts; no allocation, no voice stolen.
void testConductorForms() {
    for (u32 t = 0; t < g_music->trackCount; ++t) {
        const char* id = g_music->tracks[t].id;
        auto* s = new Session;
        CHECK(s->ok);
        s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(t), 0));
        s->post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
        s->post(cmd(CmdType::SetFire, 0, 1));
        const bool shortForm = t == 0;
        const u32 want[4] = {shortForm ? 8u : 12u, 4u, shortForm ? 8u : 16u, 4u};
        const u32 cycle = want[0] + want[1] + want[2] + want[3];
        const double bpm = g_music->tracks[t].bpm;
        // The cycle (the Build's last bar at 85%) and two bars of the next Groove.
        const u64 frames = static_cast<u64>((cycle + 2) * barFrames(t) + barFrames(t) / 0.85);
        u32 sections = 0, leads = 0, tempos = 0;
        bool formOk = true, tempoOk = true;
        Random rng(t + 11);
        u64 end = s->frame + frames;
        while (s->frame < end) {
            const u32 burst = 1 + rng.below(1000);
            s->run(burst < end - s->frame ? burst : end - s->frame, burst, [&](const BeatEvent& ev) {
                if (ev.kind == BeatKind::Section) {
                    const u32 k = sections++;
                    if (k < 5 && (ev.a != k % 4 || ev.b != want[k % 4])) formOk = false;
                }
                if (ev.kind == BeatKind::Lead) ++leads;
                if (ev.kind == BeatKind::Tempo && ev.tick > 0.0) {
                    ++tempos;
                    // The dip on the Build's last bar, then back on the Drop's downbeat.
                    const double ratio = ev.value / bpm;
                    if (std::fabs(ratio - 0.85) > 1e-4 && std::fabs(ratio - 1.0) > 1e-4) tempoOk = false;
                }
            });
        }
        CHECK_MSG(formOk && sections >= 5, "%s: the cycle's sections or lengths differ (%u sections)", id, sections);
        CHECK_MSG(tempoOk && tempos == 2, "%s: %u tempo changes in the cycle (the dip and the return)", id, tempos);
        // A note on every 1/16 (the Groove, Build to its half-bar stop, Drop's Surge in two gears, the Break's echo).
        CHECK_MSG(leads > 16 * cycle / 2, "%s: %u lead notes", id, leads);
        CHECK_MSG(s->onsets > 500 && s->offGrid == 0, "%s: %u of %u conductor onsets off the grid", id, s->offGrid, s->onsets);
        CHECK_MSG(s->allocs == 0, "%s: %llu allocations", id, static_cast<unsigned long long>(s->allocs));
        CHECK_MSG(s->e.stats().steals.load() == 0, "%s: %u voices stolen", id, s->e.stats().steals.load());
        delete s;
    }
}

// The lead rule on the audio side: a note on every write-grid step while fire is held, none without (M5: on every
// Track, 120 to 174 BPM), each note the arpeggio's for its step.
void testLeadFollowsFire() {
    for (u32 t = 0; t < g_music->trackCount; ++t) {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(t), 0));
        e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 0.0f));
        Capture c;
        // Bar 0 without fire; then fire held for bars 1-2; then released.
        const u64 bar = static_cast<u64>(barFrames(t));
        c.out = static_cast<float*>(std::calloc((bar + 64) * 4 * 2, sizeof(float)));
        c.events = static_cast<BeatEvent*>(std::calloc(20000, sizeof(BeatEvent)));
        u64 at = 0;
        const auto renderTo = [&](u64 to) {
            for (; at < to; at += 64) {
                e.render(c.out + at * 2, 64, 2);
                BeatEvent ev;
                while (e.popEvent(ev)) c.events[c.eventCount++] = ev;
            }
        };
        renderTo(bar);
        e.post(cmd(CmdType::SetFire, 0, 1));
        renderTo(bar * 3);
        e.post(cmd(CmdType::SetFire, 0, 0));
        renderTo(bar * 4);
        u32 perBar[4] = {};
        bool notesOk = true;
        const Track& tr = g_music->tracks[t];
        for (u32 i = 0; i < c.eventCount; ++i)
            if (c.events[i].kind == BeatKind::Lead) {
                const u32 b = static_cast<u32>(c.events[i].tick / 384.0);
                if (b < 4) ++perBar[b];
                const u32 st = static_cast<u32>(std::fmod(c.events[i].tick, 384.0) / 24.0);
                if (c.events[i].a != tr.chordAtBar(b).arp[st]) notesOk = false;
            }
        // Fire arrives with the first callback after the post: within the first step of bar 1.
        CHECK_MSG(perBar[0] == 0, "%s bar 0: %u notes", tr.id, perBar[0]);
        CHECK_MSG(perBar[1] >= 15 && perBar[1] <= 16, "%s bar 1: %u notes", tr.id, perBar[1]);
        CHECK_MSG(perBar[2] == 16, "%s bar 2: %u notes", tr.id, perBar[2]);
        CHECK_MSG(perBar[3] <= 1, "%s bar 3: %u notes", tr.id, perBar[3]);
        CHECK_MSG(notesOk, "%s: a lead note is not its step's arpeggio note", tr.id);
        freeCapture(c);
    }
}

// M3: the auto-aim assist's half-rate grid (SetFire b = 2): a note on every other step, on the even ones
// (the same steps the game's computeShots and the sim fire on), and the Nova blast on the game's beat.
void testLeadHalfRateAndNovaTick() {
    Engine e;
    CHECK(e.init(*g_music, *g_bank, kRate));
    e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
    e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 0.0f));
    Capture c;
    const u64 bar = 2 * kRate;  // 120 BPM
    c.out = static_cast<float*>(std::calloc(bar * 3 * 2, sizeof(float)));
    c.events = static_cast<BeatEvent*>(std::calloc(20000, sizeof(BeatEvent)));
    const auto renderTo = [&](u64 from, u64 to) {
        for (u64 f = from; f < to; f += 64) {
            e.render(c.out + f * 2, 64, 2);
            BeatEvent ev;
            while (e.popEvent(ev)) c.events[c.eventCount++] = ev;
        }
    };
    renderTo(0, bar / 2);
    e.post(cmd(CmdType::SetFire, 0, 2));
    // A Nova pressed now (tick ~96) asking for the blast on beat 7 (tick 672), a beat later than the
    // engine's own next-beat rule would pick.
    Command nova = cmd(CmdType::Nova);
    nova.when = Timing::AtTick;
    nova.tick = 672.0;
    nova.x = 1.0f;
    e.post(nova);
    renderTo(bar / 2, bar * 3);
    u32 notes = 0, odd = 0;
    double blastTick = -1.0;
    for (u32 i = 0; i < c.eventCount; ++i) {
        const BeatEvent& ev = c.events[i];
        if (ev.kind == BeatKind::Sfx && ev.a == static_cast<u8>(Sfx::NovaBlast)) blastTick = ev.tick;
        if (ev.kind != BeatKind::Lead) continue;
        const u32 b = static_cast<u32>(ev.tick / 384.0);
        if (b != 1 && b != 2) continue;
        ++notes;
        if (static_cast<u32>(std::fmod(ev.tick, 384.0) / 24.0) % 2 != 0) ++odd;
    }
    CHECK_MSG(notes == 16, "bars 1-2 at half rate: %u notes", notes);
    CHECK_MSG(odd == 0, "%u notes on odd steps", odd);
    CHECK_MSG(blastTick == 672.0, "Nova blast at tick %.1f", blastTick);
    freeCapture(c);
}

// A sound variant takes hold on the next bar line.
void testVariantOnBarLine() {
    Engine e;
    CHECK(e.init(*g_music, *g_bank, kRate));
    e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
    float buf[2 * 256];
    u64 done = 0;
    const u64 halfBar = kRate;  // 120 BPM: a bar is 96000 frames
    while (done < halfBar + 300) {
        e.render(buf, 256, 2);
        done += 256;
    }
    BeatEvent ev;
    while (e.popEvent(ev)) {
    }
    e.post(cmd(CmdType::SetVariant, 1));
    i64 variantFrame = -1;
    while (done < 3 * halfBar) {
        e.render(buf, 256, 2);
        done += 256;
        while (e.popEvent(ev))
            if (ev.kind == BeatKind::Variant) variantFrame = ev.frame;
    }
    CHECK_MSG(variantFrame == 96000, "variant applied at frame %lld", static_cast<long long>(variantFrame));
    CHECK(e.stats().variant.load() == 1);
}

// A Track change in the phrase cycle: an eight-bar Break starts on the next bar line, the tempo ramps bar
// by bar to the new Track's, the new Track (key, kit, sound) takes over on the Break's fifth bar, and the
// next Groove is at the new tempo.
void testTrackChange() {
    Engine e;
    CHECK(e.init(*g_music, *g_bank, kRate));
    e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
    float buf[2 * 256];
    u64 done = 0;
    const auto run = [&](u64 frames, BeatEvent* log, u32& count) {
        for (u64 f = 0; f < frames; f += 256) {
            e.render(buf, 256, 2);
            done += 256;
            BeatEvent ev;
            while (e.popEvent(ev))
                if (count < 4096) log[count++] = ev;
        }
    };
    BeatEvent* log = static_cast<BeatEvent*>(std::calloc(4096, sizeof(BeatEvent)));
    u32 count = 0;
    run(2 * 96000 + 1000, log, count);  // two bars and a little
    e.post(cmd(CmdType::SetTrack, 1));
    count = 0;
    run(12 * 96000, log, count);        // the Break and the start of the next Groove
    i32 breakBar = -1, trackBar = -1;
    double lastBpm = 0.0;
    u32 tempoChanges = 0;
    for (u32 i = 0; i < count; ++i) {
        const BeatEvent& ev = log[i];
        if (ev.kind == BeatKind::Section && ev.a == static_cast<u8>(Section::Break) && breakBar < 0) breakBar = static_cast<i32>(ev.index);
        if (ev.kind == BeatKind::Track) trackBar = static_cast<i32>(ev.tick / 384.0);
        if (ev.kind == BeatKind::Tempo) {
            ++tempoChanges;
            lastBpm = ev.value;
        }
    }
    CHECK_MSG(breakBar == 3, "the Break began on bar %d", breakBar);
    CHECK_MSG(trackBar == breakBar + 4, "the Track changed on bar %d (Break on %d)", trackBar, breakBar);
    CHECK_MSG(tempoChanges >= 6, "%u tempo steps", tempoChanges);
    CHECK_MSG(std::fabs(lastBpm - g_music->tracks[1].bpm) < 0.01, "ended at %.3f BPM", lastBpm);
    CHECK(e.stats().track.load() == 1);
    std::free(log);
}

// Effects and every mode render without touching the heap.
void testNoAllocations() {
    Engine e;
    CHECK(e.init(*g_music, *g_bank, kRate));
    float buf[2 * 192];
    u64 allocs = 0;
    const auto run = [&](u32 callbacks) {
        for (u32 i = 0; i < callbacks; ++i) {
            const u64 a0 = heapAllocCountThisThread();
            e.render(buf, 192, 2);
            allocs += heapAllocCountThisThread() - a0;
            BeatEvent ev;
            while (e.popEvent(ev)) {
            }
        }
    };
    u32 k = 0;
    for (Mode m : {Mode::Conductor, Mode::Staged, Mode::Metronome, Mode::Script, Mode::Silent, Mode::Conductor, Mode::Staged}) {
        // M5: on Tracks 2, 7, 10, 3, 9, 6 and 1 (half time, broken kick and two-step among them), a Track change in each.
        static const u8 kTracks[] = {1, 6, 9, 2, 8, 5, 0};
        e.post(cmd(CmdType::SetMode, static_cast<u8>(m), kTracks[k], 1));
        e.post(cmd(CmdType::SetTrack, static_cast<u8>((kTracks[k] + 1) % g_music->trackCount)));
        ++k;
        e.post(cmd(CmdType::SetFire, 0, 1));
        e.post(cmd(CmdType::SetPlayers, 4, 0));
        for (u8 p = 0; p < 4; ++p) e.post(cmd(CmdType::SetFire, p, 1));
        e.post(cmd(CmdType::Kill, 0, 9));
        e.post(cmd(CmdType::Kill, 2, 1));
        e.post(cmd(CmdType::Shard, 0, 0, 0, 1.0f));
        e.post(cmd(CmdType::Nova, 0, 0, 0, 1.0f));
        e.post(cmd(CmdType::Sfx, static_cast<u8>(Sfx::Hit), 0, 0, 1.0f));
        e.post(cmd(CmdType::SetVariant, static_cast<u8>(k % 3)));
        run(1500);
    }
    CHECK_MSG(allocs == 0, "%llu allocations while rendering", static_cast<unsigned long long>(allocs));
}

// ---------------------------------------------------------------- data

void testMusicData() {
    const MusicData& m = *g_music;
    CHECK(m.trackCount == 10);
    CHECK(m.variantCount == 3);
    // M5: the set list, 120 to 174 BPM in steps of six; each Track by id and by number; every Track x variant
    // calibrated; each arrangement the same 24 staged bars.
    for (u32 t = 0; t < m.trackCount; ++t) {
        char id[16], num[4];
        std::snprintf(id, sizeof id, "track%02u", t + 1);
        std::snprintf(num, sizeof num, "%u", t + 1);
        CHECK_MSG(std::strcmp(m.tracks[t].id, id) == 0 && m.trackIndex(id) == static_cast<i32>(t) && m.trackIndex(num) == static_cast<i32>(t),
                  "Track %u is '%s'", t + 1, m.tracks[t].id);
        CHECK_MSG(m.tracks[t].bpm == 120.0f + 6.0f * static_cast<float>(t), "%s at %.1f BPM", id, m.tracks[t].bpm);
        CHECK_MSG(m.tracks[t].arrangementBars == 24 && m.tracks[t].progressionCount == 4, "%s: arrangement %u bars", id,
                  m.tracks[t].arrangementBars);
        for (u32 v = 0; v < m.variantCount; ++v) CHECK_MSG(m.calibration.have[t][v], "%s/%s not calibrated", id, m.variants[v].id);
    }
    CHECK(m.trackIndex("11") == -1 && m.trackIndex("track11") == -1);
    // The overrides the later Tracks carry: duck (Afterglow's pad, Arcadia's bass), the half-time / broken / two-step
    // kicks, Mirage's C over B flat, Last Light's long room.
    const i32 club = m.variantIndex("club"), velvet = m.variantIndex("velvet");
    CHECK(m.sound[6][club].duck[static_cast<u32>(Bus::Pad)] == 0.7f && m.sound[6][velvet].duck[static_cast<u32>(Bus::Pad)] == 0.7f);
    CHECK(m.sound[6][club].duck[static_cast<u32>(Bus::Bass)] == 0.80f);  // patches.json
    CHECK(m.sound[7][club].duck[static_cast<u32>(Bus::Bass)] == 0.45f && m.sound[7][club].mix.pad == 0.35f);
    CHECK(m.sound[6][club].mix.clap == 0.9f && m.sound[6][club].kickHz == 51.9f && m.sound[6][club].reverb.rt60 == 2.2f);
    CHECK(m.sound[9][club].reverb.rt60 == 2.4f && m.sound[9][velvet].reverb.rt60 == 2.4f);  // the Track wins over Velvet
    CHECK(m.sound[8][club].lead.length == 0.42f && m.sound[8][club].kickHz == 46.25f);
    CHECK(patternHits(6, Pat::Kick) == 2 && m.tracks[6].pattern[static_cast<u32>(Pat::Kick)][10] > 0.0f);
    CHECK(patternHits(8, Pat::Kick) == 3 && m.tracks[8].pattern[static_cast<u32>(Pat::Kick)][6] > 0.0f);
    CHECK(patternHits(9, Pat::Kick) == 3 && std::fabs(m.tracks[9].pattern[static_cast<u32>(Pat::Kick)][13] - 3.0f / 9.0f) < 1e-6f);
    CHECK(m.tracks[8].chords[m.tracks[8].progression[1]].bass == 46);
    CHECK(m.tracks[6].longestBassSteps == 5.0f);
    CHECK(std::strcmp(m.variants[m.defaultVariant].id, "club") == 0);
    const i32 t2 = m.trackIndex("track02"), arcade = m.variantIndex("arcade");
    CHECK(t2 == 1 && arcade >= 0 && club >= 0);
    CHECK(m.trackIndex("track03") == 2 && m.trackIndex("3") == 2);
    // Order of application: patches.json, then the variant, then the Track.
    CHECK(m.sound[0][club].lead.fcStart == 4200.0f);   // patches.json
    CHECK(m.sound[0][arcade].lead.fcStart == 7000.0f); // variant
    CHECK(m.sound[1][arcade].lead.fcStart == 5000.0f); // the Track wins over the variant
    CHECK(m.sound[1][club].kickHz == 43.65f);
    CHECK(m.sound[1][club].delay.steps == 2.0f);
    CHECK(m.sound[0][club].sectionGain[static_cast<u32>(Section::Build)][1] == 0.66f);
    CHECK(m.tracks[0].arrangementBars == 24);
    CHECK(m.tracks[0].longestBassSteps == 1.5f);
    // Track 3 carries mix, send and patch overrides; each wins over the variant.
    const SoundParams& s3 = m.sound[2][arcade];
    CHECK(m.tracks[2].bpm == 132.0f && m.tracks[2].arrangementBars == 24);
    CHECK(m.tracks[2].longestBassSteps == 0.8f);
    CHECK(s3.mix.lead == 0.33f && s3.mix.stab == 0.80f && s3.mix.shaker == 0.08f && s3.mix.bass == 0.58f);
    CHECK(s3.send[static_cast<u32>(Bus::Stab)] == 0.42f && s3.send[static_cast<u32>(Bus::Lead)] == 0.26f);
    CHECK(m.sound[2][arcade].lead.q == 2.8f && m.sound[2][club].lead.q == 2.8f);
    CHECK(m.sound[2][club].reverb.rt60 == 1.7f && m.sound[2][club].delay.steps == 4.0f);
    CHECK(m.sound[2][club].stab.length == 0.34f && m.sound[2][club].pad.lfoHz == 0.18f);
    CHECK(m.sound[2][club].kick.sweepTau == 0.016f && m.sound[2][club].kickHz == 55.0f);
    // conductor.json: structured numbers and the numbers stated in its sentences.
    const ConductorRules& r = m.conductor;
    CHECK(r.gridTicks[0] == 24 && r.gridTicks[1] == 16 && r.gridTicks[2] == 12);
    CHECK(r.snareRollLo == 0.35f && r.snareRollHi == 1.0f);
    CHECK(r.buildLastBarSpeed == 0.85f);
    CHECK(r.bedCount == 7 && r.hysteresis == 0.05f);
    CHECK(r.breakSparseSteps[1] == 6);
    CHECK(m.calibrationCurrent);
}

// ---------------------------------------------------------------- M5: the set list

// Afterglow (half time: kick 0 and 10), Mirage (broken: 0, 6, 10) and Last Light (two-step: 0, 10, ghost 13): in a
// held Drop, a Beat event on every beat and a Bar on every bar line, on the clock's grid, whether a kick plays there
// or not; Kick events exactly on the pattern's kicks (velocity in x); the sidechain envelope, frame by frame, rising to
// 1 on each of those kicks and nowhere else (the beats without a kick only carry the last kick's decay); the Nova's
// blast on the beat asked for.
void testKicklessBeats() {
    for (u32 t : {6u, 8u, 9u}) {
        const Track& tr = g_music->tracks[t];
        const float* kick = tr.pattern[static_cast<u32>(Pat::Kick)];
        const u32 bars = 6;
        const u64 frames = static_cast<u64>(bars * barFrames(t)) / kBlock * kBlock;
        auto* s = new Session(200000, frames);
        CHECK(s->ok);
        s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(t), 0));
        s->post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
        s->post(cmd(CmdType::SetSection, static_cast<u8>(Section::Drop)));
        s->post(cmd(CmdType::HoldSection, 1));
        Command nova = cmd(CmdType::Nova, 0, 0, 0, 1.0f);
        nova.when = Timing::AtTick;
        nova.tick = 5.0 * kTicksPerBar + 2.0 * kPpq;  // bar 5, beat 3 (no kick there on any of the three)
        s->run(static_cast<u64>(4.5 * barFrames(t)) / kBlock * kBlock, kBlock);
        s->post(nova);
        s->run(frames - s->frame, kBlock);
        // From bar 1 on (the held Drop): beats, bars, kicks.
        u32 beats = 0, barLines = 0, kicks = 0, kickOffPattern = 0, kickVelocityOff = 0, beatsWithoutKick = 0, beatsOffClock = 0;
        i64 kickFrames[64];
        u32 nk = 0;
        double blast = -1.0;
        for (u32 i = 0; i < s->count; ++i) {
            const BeatEvent& ev = s->log[i];
            if (ev.kind == BeatKind::Sfx && ev.a == static_cast<u8>(Sfx::NovaBlast)) blast = ev.tick;
            if (ev.tick < kTicksPerBar) continue;
            const u32 st = static_cast<u32>(std::fmod(ev.tick, static_cast<double>(kTicksPerBar)) / kTicksPerStep);
            if (ev.kind == BeatKind::Beat) {
                ++beats;
                if (std::fmod(ev.tick, static_cast<double>(kPpq)) != 0.0 || ev.frame != s->mirror.gridFrame(ev.tick)) ++beatsOffClock;
                if (kick[st] <= 0.0f) ++beatsWithoutKick;
            }
            if (ev.kind == BeatKind::Bar && std::fmod(ev.tick, static_cast<double>(kTicksPerBar)) == 0.0) ++barLines;
            if (ev.kind == BeatKind::Kick) {
                ++kicks;
                if (std::fmod(ev.tick, static_cast<double>(kTicksPerStep)) != 0.0 || kick[st] <= 0.0f) ++kickOffPattern;
                if (std::fabs(ev.x - kick[st]) > 1e-6) ++kickVelocityOff;
                if (nk < countOf(kickFrames)) kickFrames[nk++] = ev.frame;
            }
        }
        const u32 heardBars = bars - 1;  // bars 1-5 (bar 0 is the first Groove bar before the Drop)
        CHECK_MSG(beats == 4 * heardBars && beatsOffClock == 0, "%s: %u beats (%u off the clock's grid) in %u bars", tr.id, beats,
                  beatsOffClock, heardBars);
        CHECK_MSG(barLines == heardBars, "%s: %u bar lines", tr.id, barLines);
        CHECK_MSG(kicks == heardBars * patternHits(t, Pat::Kick) && kickOffPattern == 0 && kickVelocityOff == 0,
                  "%s: %u kicks (%u off the pattern, %u with another velocity)", tr.id, kicks, kickOffPattern, kickVelocityOff);
        CHECK_MSG(beatsWithoutKick == 3 * heardBars, "%s: %u beats without a kick", tr.id, beatsWithoutKick);
        CHECK_MSG(blast == nova.tick, "%s: the Nova's blast on tick %.1f", tr.id, blast);
        // The sidechain, frame by frame: e^(-age / tau) from the newest kick for 0.6 s, 0 beyond; 1 only on kicks.
        const double tauN = static_cast<double>(g_music->sound[t][g_music->defaultVariant].sidechainTau) * kRate;
        const i64 duckLen = static_cast<i64>(framesFor(0.6, kRate));
        u32 duckOff = 0, onesOffKick = 0;
        u32 k = 0;
        const i64 from = static_cast<i64>(barFrames(t)) + 64;  // the Drop: every kick from the pattern
        for (i64 f = from; f < static_cast<i64>(frames); ++f) {
            while (k + 1 < nk && kickFrames[k + 1] <= f) ++k;
            const i64 age = (nk && kickFrames[k] <= f) ? f - kickFrames[k] : -1;
            const double want = (age >= 0 && age < duckLen) ? std::exp(-static_cast<double>(age) / tauN) : 0.0;
            if (std::fabs(s->duck[f] - want) > 1e-5) ++duckOff;
            if (s->duck[f] >= 0.9999f && age != 0) ++onesOffKick;
        }
        CHECK_MSG(duckOff == 0 && onesOffKick == 0, "%s: the sidechain differs from the kicks' envelope on %u frames (%u full on no kick)",
                  tr.id, duckOff, onesOffKick);
        // At a beat with no kick the envelope is only the last kick's decay (the beat does not trigger it).
        CHECK_MSG(s->offGrid == 0 && s->allocs == 0, "%s: %u onsets off the grid, %llu allocations", tr.id, s->offGrid,
                  static_cast<unsigned long long>(s->allocs));
        delete s;
    }
}

// The same three Tracks through an eight-bar Groove phrase, its Build and into the Drop (Standard form, intensity
// 1): the fill on the phrase's last bar (the clap's pick-up on step 15, more hats in the second half) whatever the
// kick does; the Build's snare roll on the clock (quarters, eighths, sixteenths, thirty-seconds to the last bar's
// half), its riser from the Build's first bar at its stretch (4 / (3 + 1 / 0.85)), the kick silent in the last bar
// and the sidechain released there, the swell ending on the Drop's downbeat, the impact on it.
void testKicklessBuild() {
    for (u32 t : {6u, 8u, 9u}) {
        const Track& tr = g_music->tracks[t];
        auto* s = new Session;
        CHECK(s->ok);
        s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(t), 0));
        s->post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
        bool posted = false;
        // Groove bars 0-7, the Build from bar 8 (12 - 4 bars early), the Drop from bar 12.
        s->run(static_cast<u64>(15.5 * barFrames(t)), 256, [&](const BeatEvent& ev) {
            if (ev.kind == BeatKind::Bar && ev.index == 7 && !posted) {
                s->post(cmd(CmdType::SetSection, static_cast<u8>(Section::Build)));
                posted = true;
            }
        });
        const double buildTick = 8.0 * kTicksPerBar, dropTick = 12.0 * kTicksPerBar;
        u32 fillClap = 0, fillHats = 0, plainHats = 0, snares[4] = {}, kicksLast = 0, impacts = 0;
        double riserTick = -1.0, riserRate = 0.0;
        i64 swellEnd = -1;
        bool snareOffGrid = false;
        for (u32 i = 0; i < s->count; ++i) {
            const BeatEvent& ev = s->log[i];
            const i32 bar = ev.tick >= 0.0 ? static_cast<i32>(ev.tick / kTicksPerBar) : -1;
            const u32 st = ev.tick >= 0.0 ? static_cast<u32>(std::fmod(ev.tick, static_cast<double>(kTicksPerBar)) / kTicksPerStep) : 0;
            if (ev.kind == BeatKind::Clap && bar == 7 && st == 15) ++fillClap;
            if (ev.kind == BeatKind::Hat && st >= 8 && (bar == 7 || bar == 6)) (bar == 7 ? fillHats : plainHats) += 1;
            if (ev.kind == BeatKind::Snare && bar >= 8 && bar < 12) {
                ++snares[bar - 8];
                if (std::fmod(ev.tick, 12.0) != 0.0) snareOffGrid = true;
            }
            if (ev.kind == BeatKind::Kick && bar == 11) ++kicksLast;
            if (ev.kind == BeatKind::Riser) {
                riserTick = ev.tick;
                riserRate = ev.x;
            }
            if (ev.kind == BeatKind::Swell && ev.frame > static_cast<i64>(8 * barFrames(t))) {
                const i32 id = g_bank->swellHalfBar(t);
                swellEnd = ev.frame + (id >= 0 ? static_cast<i64>(g_bank->sample(id).frames) : 0);
            }
            if (ev.kind == BeatKind::Impact && ev.tick == dropTick) ++impacts;
        }
        CHECK_MSG(fillClap == 1, "%s: the fill's pick-up clap %u", tr.id, fillClap);
        CHECK_MSG(fillHats > plainHats, "%s: fill bar hats %u, plain bar %u (second halves)", tr.id, fillHats, plainHats);
        CHECK_MSG(snares[0] == 4 && snares[1] == 8 && snares[2] == 16 && snares[3] == 16 && !snareOffGrid,
                  "%s: snare roll %u / %u / %u / %u", tr.id, snares[0], snares[1], snares[2], snares[3]);
        const double wantRate = 4.0 / (3.0 + 1.0 / 0.85);
        CHECK_MSG(riserTick == buildTick && std::fabs(riserRate - wantRate) < 1e-6, "%s: riser at tick %.0f, rate %.6f", tr.id, riserTick, riserRate);
        CHECK_MSG(kicksLast == 0, "%s: %u kicks in the Build's last bar", tr.id, kicksLast);
        CHECK_MSG(swellEnd == s->mirror.gridFrame(dropTick), "%s: the swell ends on frame %lld, the Drop starts on %lld", tr.id,
                  static_cast<long long>(swellEnd), static_cast<long long>(s->mirror.gridFrame(dropTick)));
        CHECK_MSG(impacts == 1, "%s: %u impacts on the Drop's downbeat", tr.id, impacts);
        CHECK_MSG(s->offGrid == 0 && s->onsets > 300 && s->allocs == 0, "%s: %u of %u onsets off the grid", tr.id, s->offGrid, s->onsets);
        delete s;
    }
}

// The whole set list in one song, driven bar by bar the way the game's director drives the band (SetPhrase for the
// next bar while the bar before plays; SetTrack with the first bar of the Track-change Break): Track 1's Groove,
// then for each of the nine changes an eight-bar Break, a Build and two Drop bars. At every boundary: the Break
// starts on the bar asked for and is eight bars; the tempo ramps bar by bar from the old Track's to the new one's
// (the main thread's mirror agrees); the Track (key, kit, sound) changes on the Break's fifth bar; the Build runs at
// the new tempo with its 85% bar, its riser from the new Track (baked at that tempo, played at the stretch alone);
// the Drop lands at the new tempo. Every onset on the mirrored grid, no allocation, no voice stolen, no event lost.
void testSetListTrackChanges() {
    const u32 n = g_music->trackCount;
    struct PlanBar {
        u8 track, section, bar, bars;
        bool change;
    };
    static PlanBar plan[256];
    u32 planBars = 0;
    const auto add = [&](u32 track, Section sec, u32 bars, bool change) {
        for (u32 b = 0; b < bars; ++b) plan[planBars++] = {static_cast<u8>(track), static_cast<u8>(sec), static_cast<u8>(b), static_cast<u8>(bars), change && b == 0};
    };
    add(0, Section::Groove, 2, false);
    u32 breakStart[kMaxTracks] = {};
    for (u32 t = 1; t < n; ++t) {
        breakStart[t] = planBars;
        add(t, Section::Break, 8, true);
        add(t, Section::Build, 4, false);
        add(t, Section::Drop, 2, false);
    }
    double seconds = 0.0;  // a Break bar at the slower (old) tempo, the rest at their own; the 85% bars in the slack
    for (u32 i = 0; i < planBars; ++i) {
        const u32 t = plan[i].section == static_cast<u8>(Section::Break) ? plan[i].track - 1u : plan[i].track;
        seconds += 240.0 / g_music->tracks[t].bpm;
    }
    auto* s = new Session(400000);
    CHECK(s->ok);
    s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
    s->post(cmd(CmdType::SetForm, static_cast<u8>(Form::Short)));
    s->post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
    s->post(cmd(CmdType::SetFire, 0, 1));
    for (u8 k = 0; k < static_cast<u8>(Riff::Count); ++k) s->post(cmd(CmdType::SetRiff, 0, k, 1));
    // Per song bar: its tempo as planned (Tempo events) and the section that began.
    static double bpmAt[256];
    static i32 trackChangeBar[kMaxTracks];
    for (u32 i = 0; i < 256; ++i) bpmAt[i] = -1.0;
    for (i32& b : trackChangeBar) b = -1;
    double riserRate[kMaxTracks] = {};
    i32 riserBar[kMaxTracks];
    for (i32& b : riserBar) b = -1;
    u32 sectionErrors = 0;
    u32 track = 0;
    double curBpm = g_music->tracks[0].bpm;
    s->run(static_cast<u64>((seconds + 4.0) * kRate), 256, [&](const BeatEvent& ev) {
        const i32 bar = static_cast<i32>(ev.tick / kTicksPerBar);
        if (ev.kind == BeatKind::Tempo) {
            curBpm = ev.value;
            if (bar >= 0 && bar < 256) bpmAt[bar] = ev.value;
        }
        if (ev.kind == BeatKind::Track && ev.a < kMaxTracks) {
            trackChangeBar[ev.a] = bar;
            track = ev.a;
        }
        if (ev.kind == BeatKind::Riser && track < kMaxTracks) {
            riserRate[track] = ev.x;
            riserBar[track] = bar;
        }
        if (ev.kind == BeatKind::Section && bar > 0 && static_cast<u32>(bar) < planBars) {  // bar 0: the song's own start
            const PlanBar& p = plan[bar];
            if (ev.a != p.section || p.bar != 0 || ev.b != p.bars) ++sectionErrors;
        }
        if (ev.kind != BeatKind::Bar) return;
        if (bar >= 0 && bar < 256 && bpmAt[bar] < 0.0) bpmAt[bar] = curBpm;  // no Tempo event: the tempo goes on
        const u32 next = static_cast<u32>(bar + 1);
        if (next >= planBars) return;
        const PlanBar& p = plan[next];
        if (p.change) s->post(cmd(CmdType::SetTrack, p.track, 0));
        s->post(cmd(CmdType::SetPhrase, p.section, p.bar, p.bars));
    });
    CHECK_MSG(sectionErrors == 0, "%u sections differ from the plan", sectionErrors);
    for (u32 t = 1; t < n; ++t) {
        const char* id = g_music->tracks[t].id;
        const double from = g_music->tracks[t - 1].bpm, to = g_music->tracks[t].bpm;
        const u32 b0 = breakStart[t];
        CHECK_MSG(trackChangeBar[t] == static_cast<i32>(b0 + 4), "%s: the key changed on bar %d (Break from %u)", id, trackChangeBar[t], b0);
        bool ramp = true;
        for (u32 k = 0; k < 8; ++k) {
            const double want = from + (to - from) * k / 7.0;
            if (std::fabs(bpmAt[b0 + k] - want) > 1e-3) ramp = false;
        }
        CHECK_MSG(ramp, "%s: the Break's tempo %.3f %.3f ... %.3f, want %.1f to %.1f", id, bpmAt[b0], bpmAt[b0 + 1], bpmAt[b0 + 7], from, to);
        const u32 build = b0 + 8, drop = b0 + 12;
        CHECK_MSG(std::fabs(bpmAt[build] - to) < 1e-3 && std::fabs(bpmAt[build + 3] - 0.85 * to) < 1e-3 && std::fabs(bpmAt[drop] - to) < 1e-3,
                  "%s: Build %.3f, its last bar %.3f, Drop %.3f BPM", id, bpmAt[build], bpmAt[build + 3], bpmAt[drop]);
        CHECK_MSG(riserBar[t] == static_cast<i32>(build) && std::fabs(riserRate[t] - 4.0 / (3.0 + 1.0 / 0.85)) < 1e-6,
                  "%s: riser on bar %d at rate %.6f", id, riserBar[t], riserRate[t]);
        // The riser the Build plays is the new Track's, baked four bars at its tempo.
        const i32 rid = g_bank->riser(t, 4);
        CHECK_MSG(rid >= 0 && g_bank->sample(rid).frames == framesFor(4 * 240.0 / to, kRate), "%s: riser %d", id, rid);
    }
    CHECK_MSG(std::fabs(s->mirror.bpmAt(static_cast<double>(s->frame), kRate) - g_music->tracks[n - 1].bpm) < 1e-6,
              "the main thread's mirror ends at %.4f BPM", s->mirror.bpmAt(static_cast<double>(s->frame), kRate));
    CHECK_MSG(s->e.stats().track.load() == n - 1, "the set list ends on Track %u", s->e.stats().track.load() + 1);
    CHECK_MSG(s->onsets > 5000 && s->offGrid == 0, "%u of %u onsets off the mirrored grid", s->offGrid, s->onsets);
    CHECK_MSG(s->allocs == 0 && s->e.stats().steals.load() == 0 && s->e.eventsDropped() == 0, "allocations %llu, steals %u, events dropped %u",
              static_cast<unsigned long long>(s->allocs), s->e.stats().steals.load(), s->e.eventsDropped());
    delete s;
}

// The load test (--band full: the Drop held, four players on the 1/16-triplet grid with a Split harmony, every Riff at
// level 2, kills) on every Track: no voice stolen, no allocation; the peak voice count stays inside the pools.
void testFullBandEveryTrack() {
    for (u32 t = 0; t < g_music->trackCount; ++t) {
        auto* s = new Session(100000);
        CHECK(s->ok);
        s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(t), 0));
        s->post(cmd(CmdType::SetPlayers, 4, 0));
        s->post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
        s->post(cmd(CmdType::SetSection, static_cast<u8>(Section::Drop)));
        s->post(cmd(CmdType::HoldSection, 1));
        for (u8 p = 0; p < 4; ++p) {
            s->post(cmd(CmdType::SetFire, p, 1));
            s->post(cmd(CmdType::SetTimbre, p, p));
            for (u8 k = 0; k < 3; ++k) s->post(cmd(CmdType::SetRiff, p, k, 2));
            Command m = cmd(CmdType::SetMods, p);
            m.mods[0] = 1;
            m.mods[1] = 1;
            s->post(m);
        }
        for (u32 f = 0; f < 60 * 8; ++f) {  // eight seconds, effects every 1/60 s as Band::update feeds them
            s->post(cmd(CmdType::Kill, 0, 3));
            if (f % 4 == 0) {
                s->post(cmd(CmdType::Kill, 1, 1));
                s->post(cmd(CmdType::Shard, 0, 0, 0, 1.0f));
            }
            if (f % 30 == 0) s->post(cmd(CmdType::Kill, 2, 1));
            s->run(kRate / 60, 240);
        }
        const u32 peak = s->e.stats().voicesPeak.load();
        CHECK_MSG(s->e.stats().steals.load() == 0 && s->allocs == 0 && s->offGrid == 0,
                  "%s: steals %u, allocations %llu, onsets off the grid %u", g_music->tracks[t].id, s->e.stats().steals.load(),
                  static_cast<unsigned long long>(s->allocs), s->offGrid);
        std::printf("  full band %s: voices peak %u\n", g_music->tracks[t].id, peak);
        delete s;
    }
}

// The Shard run of every Track stays in its scale (M5: a major pentatonic on Afterglow, Arcadia and Mirage).
void testShardScales() {
    for (u32 t = 0; t < g_music->trackCount; ++t) {
        const Track& tr = g_music->tracks[t];
        const i32 tonic = tr.chords[tr.progression[0]].bass % 12;
        const u8* sc = g_bank->shardScale(t);
        u32 outside = 0;
        for (u32 i = 0; i < 10; ++i) {
            const i32 pc = ((sc[i] - tonic) % 12 + 12) % 12;
            bool in = false;
            for (u32 d = 0; d < tr.scaleCount; ++d) in = in || tr.scale[d] == pc;
            if (!in) ++outside;
            if (i && sc[i] <= sc[i - 1]) ++outside;  // rising
        }
        CHECK_MSG(outside == 0, "%s: %u Shard notes outside its scale (or not rising)", tr.id, outside);
    }
}

// M5 (the lead's rule: effects are part of the music): the effects written in A minor move to each Track's root and take
// their major third on Afterglow, Arcadia and Mirage; the Elite's fifth only moves.
void testEffectsInKey() {
    const double wantShift[10] = {0, -4, 5, -5, -2, 3, 4, -2, 1, 0};
    for (u32 t = 0; t < g_music->trackCount; ++t) {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), static_cast<u8>(t), 0));
        for (Sfx s : {Sfx::Boost, Sfx::Upgrade, Sfx::GameOver, Sfx::Elite}) e.post(cmd(CmdType::Sfx, static_cast<u8>(s), 0, 0, 1.0f));
        Capture c;
        runEngine(e, c, 4800, 480);
        const bool major = t == 6 || t == 7 || t == 8;
        u32 seen = 0, wrong = 0;
        for (u32 i = 0; i < c.eventCount; ++i) {
            const BeatEvent& ev = c.events[i];
            if (ev.kind != BeatKind::Sfx) continue;
            ++seen;
            const bool hasMajor = ev.a != static_cast<u8>(Sfx::Elite);
            if (std::fabs(ev.x - std::exp2(wantShift[t] / 12.0)) > 1e-5 || (ev.b == 1) != (major && hasMajor)) ++wrong;
        }
        CHECK_MSG(seen == 4 && wrong == 0, "%s: %u of %u effects not in the Track's key", g_music->tracks[t].id, wrong, seen);
        freeCapture(c);
    }
}

// M5 (sound_m5.json): every recipe and every cue sounds, held voices start, follow their parameter and stop, nothing
// allocates; in the conductor with the full bed playing.
void testRecipesAndCues() {
    const auto energy = [](const Capture& c, u64 from, u64 to) {
        double e = 0.0;
        for (u64 i = from * 2; i < to * 2 && i < c.frames * 2; ++i) e += static_cast<double>(c.out[i]) * c.out[i];
        return e;
    };
    u32 silent = 0, allocs = 0;
    for (u32 k = 0; k < kRecipeCount + kCueCount; ++k) {
        const bool isRecipe = k < kRecipeCount;
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), 6, 0));  // Afterglow: Db major, 156 BPM
        Command c;
        if (isRecipe) {
            const Recipe r = static_cast<Recipe>(k);
            const bool held = r == Recipe::Drone || r == Recipe::PadVoice || r == Recipe::Crackle;
            c = cmd(held ? CmdType::VoiceStart : CmdType::Recipe, held ? 5 : static_cast<u8>(r), held ? static_cast<u8>(r) : 0, 255, 1.0f);
            c.z = r == Recipe::Crackle ? 40.0f : (r == Recipe::Drone ? 800.0f : 1.0f);
            c.level = r == Recipe::SawGlide || r == Recipe::Sweep ? 1.0f : 0.0f;
            c.when = Timing::Next32;
        } else {
            c = cmd(CmdType::Cue, static_cast<u8>(k - kRecipeCount), 0, 3, 2.0f);
            c.when = Timing::Next32;
        }
        e.post(c);
        Capture cap;
        runEngine(e, cap, static_cast<u64>(kRate) * 4, 256);
        if (isRecipe) {
            const Recipe r = static_cast<Recipe>(k);
            if (r == Recipe::Drone || r == Recipe::PadVoice || r == Recipe::Crackle) {
                // Stop it: the voice releases to silence.
                e.post(cmd(CmdType::VoiceStop, 5, 0, 0, 0.1f));
                Capture tail;
                runEngine(e, tail, static_cast<u64>(kRate) * 2, 256);
                CHECK_MSG(energy(tail, kRate, kRate * 2) < 1e-9, "recipe %u still sounds after its stop", k);
                allocs += static_cast<u32>(tail.allocs);
                freeCapture(tail);
            }
        }
        const Cue cue = static_cast<Cue>(k >= kRecipeCount ? k - kRecipeCount : 0);
        const bool quietByDesign = !isRecipe && cue == Cue::PlayerHit;  // Crescendo's fall: no sound of its own
        if (!quietByDesign && energy(cap, 0, cap.frames) <= 1e-8) {
            ++silent;
            std::printf("  silent: %s %u\n", isRecipe ? "recipe" : "cue", isRecipe ? k : k - kRecipeCount);
        }
        allocs += static_cast<u32>(cap.allocs);
        freeCapture(cap);
    }
    CHECK_MSG(silent == 0 && allocs == 0, "%u silent, %u allocations", silent, allocs);
}

// All ten palettes load, one for each Track of the set list (design/palettes.json "track").
void testPalettes() {
    auto* p = new Palettes;
    CHECK(loadPalettes(*p));
    CHECK_MSG(p->count == 10, "%u palettes", p->count);
    for (u32 t = 1; t <= g_music->trackCount; ++t) {
        u32 found = 0;
        for (u32 i = 0; i < p->count; ++i) found += p->list[i].track == t;
        CHECK_MSG(found == 1, "Track %u has %u palettes", t, found);
    }
    CHECK(p->find("lastlight") && p->find("lastlight")->track == 10 && p->find("afterglow") && p->find("mirage"));
    delete p;
}

struct Test {
    const char* name;
    void (*fn)();
};

// M3 stage C: the pause's low-pass on the music bus (screens.md: about 600 Hz, 150 ms ramp) takes the
// brightness out of the music and gives it back when the pause ends; no allocation.
void testPauseMuffle() {
    Engine e;
    CHECK(e.init(*g_music, *g_bank, kRate));
    e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Staged), 0, 0));
    const auto muffle = [&](float hz) {
        Command k = cmd(CmdType::SetMuffle, 0, 0, 0, hz);
        k.y = 0.15f;
        e.post(k);
    };
    // Brightness: the energy of the first difference over the energy (high for hats, low under a low-pass).
    const auto brightness = [&](u32 seconds) {
        Capture c;
        runEngine(e, c, static_cast<u64>(kRate) * seconds, 480);
        double d = 0.0, s = 0.0;
        for (u64 i = 2; i < c.frames * 2; ++i) {
            const double x = c.out[i], dx = c.out[i] - c.out[i - 2];
            d += dx * dx;
            s += x * x;
        }
        CHECK(c.allocs == 0);
        freeCapture(c);
        return s > 0.0 ? d / s : 0.0;
    };
    brightness(4);  // past the song's first bars
    const double open = brightness(4);
    muffle(600.0f);
    brightness(1);  // the ramp
    const double shut = brightness(4);
    muffle(0.0f);
    brightness(1);
    const double back = brightness(4);
    CHECK_MSG(open > 0.0 && shut < 0.25 * open, "brightness open %.4f, under the low-pass %.4f", open, shut);
    CHECK_MSG(back > 0.6 * open, "brightness after the pause %.4f (open %.4f)", back, open);
}
// M3 stage C: an effect's playback rate (the boss pair's growl an octave down): at half rate the same
// one-shot rings twice as long.
void testSfxRate() {
    const auto ringFrames = [&](float rate) {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), 0, 0));
        Command k = cmd(CmdType::Sfx, static_cast<u8>(Sfx::Elite), 0, 0, 1.0f);
        k.level = rate;
        e.post(k);
        Capture c;
        runEngine(e, c, static_cast<u64>(kRate) * 6, 480);
        u64 last = 0;
        for (u64 i = 0; i < c.frames; ++i)
            if (std::fabs(c.out[i * 2]) > 1e-3f || std::fabs(c.out[i * 2 + 1]) > 1e-3f) last = i;
        freeCapture(c);
        return static_cast<double>(last);
    };
    const double one = ringFrames(1.0f), half = ringFrames(0.5f);
    CHECK_MSG(one > 0.0 && half > 1.6 * one && half < 2.4 * one, "the growl rings %.0f frames at rate 1, %.0f at 0.5", one, half);
}
}  // namespace

int main() {
    g_music = new MusicData;
    g_bank = new SampleBank;
    if (!g_music->load() || !g_bank->bake(*g_music, kRate)) {
        std::printf("FAIL: music data or baking\n");
        return 1;
    }
    const Test tests[] = {
        {"spsc single thread", testSpscSingleThread},
        {"spsc two threads", testSpscThreads},
        {"seq latest", testSeqLatest},
        {"tempo map constant", testTempoConstant},
        {"tempo map changes", testTempoChange},
        {"music data", testMusicData},
        {"palettes: ten, one for each Track (M5)", testPalettes},
        {"staged: burst independence, grid, no allocation", testStagedBurstIndependence},
        {"conductor: every Track's form, grid through the Build dip", testConductorForms},
        {"lead follows fire, every Track", testLeadFollowsFire},
        {"lead at half rate, Nova on the game's beat (M3)", testLeadHalfRateAndNovaTick},
        {"variant on the bar line", testVariantOnBarLine},
        {"track change through the Break", testTrackChange},
        {"no allocations in any mode", testNoAllocations},
        {"pause low-pass on the music (M3 stage C)", testPauseMuffle},
        {"effect playback rate (M3 stage C)", testSfxRate},
        {"no kick on the beat: beats from the clock, sidechain from the kicks (M5: Tracks 7, 9, 10)", testKicklessBeats},
        {"no kick on the beat: fill, Build, riser, swell (M5: Tracks 7, 9, 10)", testKicklessBuild},
        {"the set list: nine Track changes, 120 to 174 BPM (M5)", testSetListTrackChanges},
        {"the full band on every Track (M5)", testFullBandEveryTrack},
        {"Shard runs in each Track's scale (M5)", testShardScales},
        {"A-minor effects in each Track's key (M5)", testEffectsInKey},
        {"recipes, held voices and cues sound, stop and do not allocate (M5)", testRecipesAndCues},
    };
    for (const Test& t : tests) {
        g_test = t.name;
        const int before = g_failures;
        t.fn();
        std::printf("%s %s\n", g_failures == before ? "pass" : "FAIL", t.name);
    }
    std::printf("RFTEST {\"suite\":\"audio\",\"checks\":%d,\"failures\":%d}\n", g_checks, g_failures);
    delete g_bank;
    delete g_music;
    return g_failures;
}
