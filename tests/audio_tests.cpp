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
#include <chrono>
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
    i64 i = 1;
    for (; i < 2000000; ++i) s.publish({i, i, i, i});
    // (M5, the lead's rule for this flaky check: under load the reader may not have been scheduled yet; the writer goes on
    // publishing until the reader has read, rather than the test being rerun)
    const auto t0 = std::chrono::steady_clock::now();
    while (reads.load() < 1000 && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(20)) s.publish({i, i, i, i}), ++i;
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
            const bool held = recipeHeld(r);  // M5 audio: and the sub sine
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
            if (recipeHeld(r)) {
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

// M5 (start-up): the later Tracks' risers baked behind the game are missing until bakeDeferred, then the same samples.
void testDeferredRisers() {
    auto* b = new SampleBank;
    CHECK(b->bake(*g_music, kRate, true));
    CHECK(b->riser(0, 4) >= 0 && b->riser(1, 4) < 0 && b->riser(9, 4) < 0 && !b->complete());
    // M5: and the later Tracks' drum kits (the first Track's at once).
    CHECK(b->kit(0, 0).kick >= 0 && b->kit(1, 0).kick < 0 && b->kit(9, 2).hat < 0);
    CHECK(b->bakeDeferred(*g_music, kRate) && b->complete());
    bool same = true;
    for (u32 t = 0; t < g_music->trackCount; ++t) {
        const Sample& x = b->sample(b->riser(t, 4));
        const Sample& y = g_bank->sample(g_bank->riser(t, 4));
        same = same && x.frames == y.frames && std::memcmp(x.data, y.data, sizeof(float) * x.frames * 2) == 0;
        for (u32 v = 0; v < g_music->variantCount; ++v)
            for (const i32* ids : {&b->kit(t, v).kick, &b->kit(t, v).hat, &b->kit(t, v).ohat}) {
                const i32 other = ids == &b->kit(t, v).kick ? g_bank->kit(t, v).kick : (ids == &b->kit(t, v).hat ? g_bank->kit(t, v).hat : g_bank->kit(t, v).ohat);
                const Sample& kx = b->sample(*ids);
                const Sample& ky = g_bank->sample(other);
                same = same && kx.frames == ky.frames && kx.frames && std::memcmp(kx.data, ky.data, sizeof(float) * kx.frames) == 0;
            }
    }
    CHECK_MSG(same, "a riser or a drum kit baked behind the game differs from the one baked at start");
    // A run that starts in another Track bakes that one at once.
    auto* c = new SampleBank;
    CHECK(c->bake(*g_music, kRate, true, 9) && c->kit(9, 0).kick >= 0 && c->riser(9, 4) >= 0 && c->kit(0, 0).kick < 0);
    delete c;
    delete b;
}

// ---------------------------------------------------------------- M5: the upgrades' sound (conductor.json mods, riffs,
// boosts; sound_m5.json fusions)

using U = UpgradeSound;

// One player's loadout as Band::setLoadout sends it.
void postLoadout(Session& s, u8 player, const PlayerLoadout& l) {
    for (u32 first = 0; first < kModCount; first += 8) {
        Command c = cmd(CmdType::SetModStacks, player, static_cast<u8>(first), static_cast<u8>(kModCount - first < 8 ? kModCount - first : 8));
        for (u32 k = 0; k < c.c; ++k) c.mods[k] = l.mods[first + k];
        s.post(c);
    }
    for (u32 r = 0; r < kRiffCount; ++r) s.post(cmd(CmdType::SetRiff, player, static_cast<u8>(r), l.riffs[r]));
    for (u32 b = 0; b < kBoostCount; ++b) s.post(cmd(CmdType::SetBoost, player, static_cast<u8>(b), static_cast<u8>((l.boosts >> b) & 1u)));
    s.post(cmd(CmdType::SetFusions, player, l.fusions));
}

PlayerLoadout withRiff(Riff r, u8 level, PlayerLoadout l = {}) {
    l.riffs[static_cast<u32>(r)] = level;
    return l;
}
PlayerLoadout withMod(Mod m, u8 stacks, PlayerLoadout l = {}) {
    l.mods[static_cast<u32>(m)] = stacks;
    return l;
}
PlayerLoadout withBoost(Boost b, PlayerLoadout l = {}) {
    l.boosts = static_cast<u16>(l.boosts | (1u << static_cast<u32>(b)));
    return l;
}
PlayerLoadout withFusion(Fusion f, PlayerLoadout l = {}) {
    l.fusions = static_cast<u8>(l.fusions | (1u << static_cast<u32>(f)));
    return l;
}

// A conductor session: a Track, the section held from bar 1 (bar 0 is the Groove), one player with a loadout, firing or
// not, at an intensity (0: the kick alone); `bars` rendered.
Session* upgradeRun(const PlayerLoadout& l, u32 bars, bool fire, Section sec = Section::Groove, float intensity = 1.0f, u32 track = 0,
                    bool hold = true) {
    auto* s = new Session(200000);
    s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(track), 0));
    s->post(cmd(CmdType::SetPlayers, 1, 0));
    s->post(cmd(CmdType::SetIntensity, 0, 0, 0, intensity));
    if (hold) {
        s->post(cmd(CmdType::SetSection, static_cast<u8>(sec)));
        s->post(cmd(CmdType::HoldSection, 1));
    } else {
        s->post(cmd(CmdType::SetForm, static_cast<u8>(Form::Short)));
    }
    s->post(cmd(CmdType::SetFire, 0, fire ? 1 : 0));
    postLoadout(*s, 0, l);
    if (bars) s->run(static_cast<u64>(barFrames(track) * bars + 0.5));
    return s;
}

struct Hit {
    double tick;
    float value;
    double x;
};
// The upgrade events of a kind in a session's log (at most `cap` copied); the count.
u32 upEvents(const Session& s, U k, Hit* out, u32 cap) {
    u32 n = 0;
    for (u32 i = 0; i < s.count; ++i) {
        const BeatEvent& e = s.log[i];
        if (e.kind != BeatKind::Sfx || e.a != kUpgradeEvent + static_cast<u8>(k)) continue;
        if (n < cap) out[n] = {e.tick, e.value, e.x};
        ++n;
    }
    return n;
}
u32 inBarOf(double tick) { return static_cast<u32>(std::llround(std::fmod(tick, 384.0))); }
u32 barOf(double tick) { return static_cast<u32>(tick / 384.0 + 1e-9); }
// The local player's Lead event (its note) on a tick, -1 when none.
i32 leadAt(const Session& s, double tick) {
    for (u32 i = 0; i < s.count; ++i)
        if (s.log[i].kind == BeatKind::Lead && s.log[i].b == 0 && std::fabs(s.log[i].tick - tick) < 0.5) return s.log[i].a;
    return -1;
}
u32 countKind(const Session& s, BeatKind k, u32 fromBar = 0) {
    u32 n = 0;
    for (u32 i = 0; i < s.count; ++i) n += s.log[i].kind == k && s.log[i].tick >= 384.0 * fromBar;
    return n;
}
// The engine's own rules, for the notes a test expects.
float scaleStepT(const Track& t, float base, i32 degrees) {
    const i32 n = static_cast<i32>(t.scaleCount);
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
i32 chordToneT(const Chord& ch, u32 k, i32 octave) {
    const i32 rootPc = ch.bass % 12, root = 12 * (octave + 1) + rootPc;
    u32 mask = 0;
    for (u32 i = 0; i < ch.padCount; ++i) mask |= 1u << (((ch.pad[i] - rootPc) % 12 + 12) % 12);
    for (u32 i = 0; i < ch.stabCount; ++i) mask |= 1u << (((ch.stab[i] - rootPc) % 12 + 12) % 12);
    if (k == 1) return (mask >> 3) & 1u ? root + 3 : root + 4;
    if (k == 2) return root + 7;
    return k == 4 ? root + 12 : root;
}
double midiOf(double hz) { return 69.0 + 12.0 * std::log2(hz / 440.0); }

Hit g_hits[8192];

// Riffs 4-12: each one's instrument on its pattern, from the band's clock (Track 1, 120 BPM, the Groove held).
void testRiffsPatterned() {
    constexpr u32 kBars = 9;
    const Track& t1 = g_music->tracks[0];
    const auto atSteps = [&](const Session& s, U k, std::initializer_list<u32> inBars, u32 perBar, u32 bars, const char* what) {
        const u32 n = upEvents(s, k, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n && i < 8192; ++i) {
            bool ok = false;
            for (u32 x : inBars) ok = ok || inBarOf(g_hits[i].tick) == x;
            wrong += !ok;
        }
        CHECK_MSG(n == perBar * bars && wrong == 0, "%s: %u events (want %u), %u off their steps", what, n, perBar * bars, wrong);
    };
    {  // Lancer, level 1: the chord on beat 4; its charge, a reverse swell from the One, three beats long.
        Session* s = upgradeRun(withRiff(Riff::Lancer, 1), kBars, false);
        atSteps(*s, U::LancerChord, {288}, 1, kBars, "Lancer 1 chord");
        atSteps(*s, U::LancerCharge, {0}, 1, kBars, "Lancer 1 charge");
        CHECK(upEvents(*s, U::LancerCharge, g_hits, 1) && std::fabs(g_hits[0].x - 288.0) < 1e-9);
        delete s;
        s = upgradeRun(withRiff(Riff::Lancer, 3), kBars, false);  // from level 3 on beats 2 and 4, each charged a beat
        atSteps(*s, U::LancerChord, {96, 288}, 2, kBars, "Lancer 3 chord");
        atSteps(*s, U::LancerCharge, {0, 192}, 2, kBars, "Lancer 3 charge");
        CHECK(upEvents(*s, U::LancerCharge, g_hits, 1) && std::fabs(g_hits[0].x - 96.0) < 1e-9);
        delete s;
        s = upgradeRun(withBoost(Boost::Hyper, withRiff(Riff::Lancer, 1)), kBars, false);  // Hyper: the pattern twice a bar
        atSteps(*s, U::LancerChord, {144, 336}, 2, kBars, "Lancer 1 under Hyper");
        delete s;
    }
    {  // Minelayer: a hat as the mine is laid (step 8), the last bar's mines go off on the One.
        Session* s = upgradeRun(withRiff(Riff::Minelayer, 1), kBars, false);
        atSteps(*s, U::MineLaid, {192}, 1, kBars, "Minelayer laid");
        atSteps(*s, U::MineBlast, {0}, 1, kBars - 1, "Minelayer blast");
        delete s;
    }
    {  // Swarm: a bell a missile on the 1/32 grid from the One, the chord's arp; 8 at level 1, 24 at level 5.
        for (u8 lv : {1, 5}) {
            Session* s = upgradeRun(withRiff(Riff::Swarm, lv), kBars, false);
            const u32 count = g_music->conductor.swarmCount[lv - 1];
            const u32 n = upEvents(*s, U::SwarmBell, g_hits, 8192);
            u32 wrong = 0;
            for (u32 i = 0; i < n; ++i) {
                const u32 k = inBarOf(g_hits[i].tick) / 12;
                const Chord& ch = t1.chordAtBar(barOf(g_hits[i].tick));
                wrong += inBarOf(g_hits[i].tick) % 12 != 0 || k >= count ||
                         std::lround(g_hits[i].x) != ch.arp[k % 16] + 12 * static_cast<i32>(k / 16);
            }
            CHECK_MSG(n == count * kBars && wrong == 0, "Swarm %u: %u bells (want %u), %u wrong", lv, n, count * kBars, wrong);
            delete s;
        }
    }
    {  // Singularity: a three-beat swell in bars 3 and 7, the boom on the One of bars 4 and 8.
        Session* s = upgradeRun(withRiff(Riff::Singularity, 1), kBars, false);
        const u32 n = upEvents(*s, U::SingularitySwell, g_hits, 8192);
        CHECK_MSG(n == 2 && barOf(g_hits[0].tick) == 3 && inBarOf(g_hits[0].tick) == 96 && barOf(g_hits[1].tick) == 7, "Singularity: %u swells", n);
        const u32 b = upEvents(*s, U::SingularityBoom, g_hits, 8192);
        CHECK_MSG(b == 2 && barOf(g_hits[0].tick) == 4 && inBarOf(g_hits[0].tick) == 0 && barOf(g_hits[1].tick) == 8, "Singularity: %u booms", b);
        delete s;
    }
    {  // Comet: out on the One, back on three, each over two beats (a second at 120 BPM: rate 1).
        Session* s = upgradeRun(withRiff(Riff::Comet, 1), kBars, false);
        atSteps(*s, U::CometOut, {0}, 1, kBars, "Comet out");
        atSteps(*s, U::CometBack, {192}, 1, kBars, "Comet back");
        CHECK(upEvents(*s, U::CometOut, g_hits, 1) && std::fabs(g_hits[0].x - 1.0) < 1e-3);
        delete s;
    }
    {  // Flak: a snare on two and four; from level 4 a clap a 1/32 after each.
        Session* s = upgradeRun(withRiff(Riff::Flak, 1), kBars, false);
        atSteps(*s, U::FlakSnare, {96, 288}, 2, kBars, "Flak 1 snare");
        CHECK(upEvents(*s, U::FlakClap, g_hits, 1) == 0);
        delete s;
        s = upgradeRun(withRiff(Riff::Flak, 4), kBars, false);
        atSteps(*s, U::FlakClap, {108, 300}, 2, kBars, "Flak 4 clap");
        delete s;
    }
    {  // Mirror, level 3: every lead note doubled an octave up at 0.55 of the lead.
        Session* s = upgradeRun(withRiff(Riff::Mirror, 3), kBars, true, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::MirrorNote, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i)
            wrong += std::lround(g_hits[i].x) != leadAt(*s, g_hits[i].tick) + 12 ||
                     std::fabs(g_hits[i].value - g_music->sound[0][g_music->defaultVariant].mix.lead * 0.55f) > 1e-5f;
        CHECK_MSG(n == countKind(*s, BeatKind::Lead) && n > 0 && wrong == 0, "Mirror: %u notes for %u lead notes, %u wrong", n,
                  countKind(*s, BeatKind::Lead), wrong);
        delete s;
    }
    {  // The Riffs rest in the Break (bar 0 is the Groove).
        PlayerLoadout l;
        for (u32 r = 0; r < kRiffCount; ++r) l.riffs[r] = 5;
        Session* s = upgradeRun(l, 4, true, Section::Break);
        u32 inBreak = 0;
        for (u32 i = 0; i < s->count; ++i) {
            const BeatEvent& e = s->log[i];
            inBreak += e.kind == BeatKind::Sfx && e.a >= kUpgradeEvent && e.tick >= 384.0;
        }
        CHECK_MSG(inBreak == 0, "%u Riff sounds in the Break", inBreak);
        delete s;
    }
    {
        // Sweeper: the drone (a render with it less one without, Track 1 with the kick alone) is brighter as the arm passes
        // twelve o'clock (the One of every second bar) than half a turn later.
        const u64 frames = static_cast<u64>(barFrames(0) * 4);
        Capture c[2];
        for (u32 k = 0; k < 2; ++k) {
            Engine e;
            CHECK(e.init(*g_music, *g_bank, kRate));
            e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
            e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 0.0f));
            e.post(cmd(CmdType::HoldSection, 1));
            if (k) e.post(cmd(CmdType::SetRiff, 0, static_cast<u8>(Riff::Sweeper), 1));
            runEngine(e, c[k], frames, 256);
        }
        const auto bright = [&](double atS) {
            double d = 0.0, s2 = 0.0, prev = 0.0;
            for (u64 i = static_cast<u64>((atS - 0.15) * kRate); i < static_cast<u64>((atS + 0.15) * kRate); ++i) {
                const double x = c[1].out[2 * i] - c[0].out[2 * i];
                d += (x - prev) * (x - prev);
                s2 += x * x;
                prev = x;
            }
            return s2 > 0.0 ? d / s2 : 0.0;
        };
        const double open = bright(4.0), closed = bright(6.0);  // the One of bar 2 (twelve o'clock), of bar 3 (six)
        CHECK_MSG(open > 3.0 * closed && closed > 0.0, "Sweeper: brightness %.6f at twelve o'clock, %.6f at six", open, closed);
        freeCapture(c[0]);
        freeCapture(c[1]);
    }
}

// The Mods that change the lead or add to the mix (conductor.json mods): Track 1, the kick alone, one player firing on
// the 1/16 grid.
void testModsOnLead() {
    constexpr u32 kBars = 6;
    const Track& t1 = g_music->tracks[0];
    const float leadMix = g_music->sound[0][g_music->defaultVariant].mix.lead;
    static LeadVoiceInfo v[128];
    {  // Seeker: each note glides in from the one before, 12 ms a stack.
        Session* s = upgradeRun(withMod(Mod::Seeker, 2), 0, true, Section::Groove, 0.0f);
        u32 pairs = 0, wrong = 0;
        for (u32 step = 0; step < 40; ++step) {
            s->run(6000);  // a sixteenth at 120 BPM
            const u32 n = s->e.leadVoices(v, 128);
            for (u32 a = 0; a < n; ++a)
                for (u32 b = 0; b < n; ++b) {
                    if (v[b].start - v[a].start != 6000 || std::fabs(v[a].hz - v[b].hz) < 0.5f) continue;
                    ++pairs;
                    const double want = midiOf(v[a].hz) - midiOf(v[b].hz);
                    wrong += std::fabs(v[b].glideSemis - want) > 0.01 || v[b].glideN != 1152;
                }
        }
        CHECK_MSG(pairs > 10 && wrong == 0, "Seeker: %u glides checked, %u wrong", pairs, wrong);
        delete s;
    }
    {  // Wave: the vibrato's depth (18 cents a stack) and its phase from the song's clock; Helix Array swings the harmony
       // half a cycle the other way.
        PlayerLoadout l = withMod(Mod::Split, 3, withMod(Mod::Wave, 2));
        Session* s = upgradeRun(l, 0, true, Section::Groove, 0.0f);
        Session* h = upgradeRun(withFusion(Fusion::HelixArray, l), 0, true, Section::Groove, 0.0f);
        s->run(static_cast<u64>(barFrames(0) * 2));
        h->run(static_cast<u64>(barFrames(0) * 2));
        const auto phaseAt = [](i64 start) {
            const double t = 5.5 * static_cast<double>(start) / kRate;
            return t - std::floor(t);
        };
        const auto offBy = [](double a, double b) {
            const double d = std::fabs(a - b);
            return std::fmin(d, 1.0 - d);
        };
        u32 n = s->e.leadVoices(v, 128), wrong = 0;
        for (u32 i = 0; i < n; ++i) wrong += std::fabs(v[i].vibSemis - 0.36f) > 1e-5f || offBy(v[i].vibPhase, phaseAt(v[i].start)) > 1e-4;
        CHECK_MSG(n > 0 && wrong == 0, "Wave: %u of %u voices off", wrong, n);
        n = h->e.leadVoices(v, 128);
        u32 harmony = 0;
        wrong = 0;
        for (u32 i = 0; i < n; ++i) {
            float lowest = v[i].hz;  // the lead's note: the lowest of the voices that started with it
            for (u32 j = 0; j < n; ++j)
                if (v[j].start == v[i].start && v[j].hz < lowest) lowest = v[j].hz;
            const bool isHarmony = v[i].hz > lowest + 0.5f;
            harmony += isHarmony;
            wrong += offBy(v[i].vibPhase, phaseAt(v[i].start) + (isHarmony ? 0.5 : 0.0)) > 1e-4;
        }
        CHECK_MSG(harmony > 0 && wrong == 0, "Helix Array: %u harmony voices, %u in the wrong phase", harmony, wrong);
        delete s;
        delete h;
    }
    {  // Heavy Step: every fourth lead step (3, 7, 11, 15) doubled an octave down on the bass.
        Session* s = upgradeRun(withMod(Mod::HeavyStep, 1), kBars, true, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::HeavyStepBass, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i)
            wrong += (inBarOf(g_hits[i].tick) / 24) % 4 != 3 || std::lround(g_hits[i].x) != leadAt(*s, g_hits[i].tick) - 12;
        CHECK_MSG(n == 4 * kBars && wrong == 0, "Heavy Step: %u notes, %u wrong", n, wrong);
        delete s;
    }
    {  // Accent: the stab under the accented note (beat 1; 1 and 3; every beat), the note itself 1.4 louder.
        for (u8 st : {1, 2, 3}) {
            Session* s = upgradeRun(withMod(Mod::Accent, st), kBars, true, Section::Groove, 0.0f);
            const u32 n = upEvents(*s, U::ModStab, g_hits, 8192);
            u32 wrong = 0;
            for (u32 i = 0; i < n; ++i) wrong += inBarOf(g_hits[i].tick) % (st == 3 ? 96 : (st == 2 ? 192 : 384)) != 0;
            const u32 want = (st == 3 ? 4 : st) * kBars;
            CHECK_MSG(n == want && wrong == 0, "Accent %u: %u stabs (want %u), %u wrong", st, n, want, wrong);
            delete s;
        }
        Session* s = upgradeRun(withMod(Mod::Accent, 1), 0, true, Section::Groove, 0.0f);
        s->run(static_cast<u64>(barFrames(0) * 2) + 100);  // just after the One of bar 2
        float accented = 0.0f, plain = 0.0f;
        u32 n = s->e.leadVoices(v, 128);
        for (u32 i = 0; i < n; ++i)
            if (v[i].start == static_cast<i64>(barFrames(0) * 2)) accented = v[i].gain;
        s->run(24000);  // just after beat 2
        n = s->e.leadVoices(v, 128);
        for (u32 i = 0; i < n; ++i)
            if (v[i].start == static_cast<i64>(barFrames(0) * 2) + 24000) plain = v[i].gain;
        CHECK_MSG(plain > 0.0f && std::fabs(accented / plain - 1.4f) < 1e-4f, "Accent: the accented note at %.4f, the next beat's at %.4f", accented, plain);
        delete s;
    }
    {  // Echo: the lead's delay a beat long (24000 frames at 120 BPM; three sixteenths without).
        Session* s = upgradeRun(withMod(Mod::Echo, 1), 2, true, Section::Groove, 0.0f);
        Session* o = upgradeRun(PlayerLoadout{}, 2, true, Section::Groove, 0.0f);
        CHECK_MSG(s->e.delayFrames() == 24000 && o->e.delayFrames() == 18000, "Echo: delay %u frames (without %u)", s->e.delayFrames(),
                  o->e.delayFrames());
        delete s;
        delete o;
    }
    {  // Rear Guard: two scale degrees below the arp on steps 2, 6, 10, 14; the second stack five below on steps 1 and 9.
        for (u8 st : {1, 2}) {
            Session* s = upgradeRun(withMod(Mod::RearGuard, st), kBars, true, Section::Groove, 0.0f);
            const u32 n = upEvents(*s, U::RearGuardNote, g_hits, 8192);
            u32 wrong = 0;
            for (u32 i = 0; i < n; ++i) {
                const u32 step = inBarOf(g_hits[i].tick) / 24;
                const i32 deg = (step == 1 || step == 9) ? -5 : -2;
                const bool where = step == 2 || step == 6 || step == 10 || step == 14 || (st == 2 && (step == 1 || step == 9));
                const Chord& ch = t1.chordAtBar(barOf(g_hits[i].tick));
                wrong += !where || std::lround(g_hits[i].x) != std::lround(scaleStepT(t1, ch.arp[step], deg)) ||
                         std::fabs(g_hits[i].value - leadMix * 0.5f) > 1e-5f;
            }
            CHECK_MSG(n == (st == 2 ? 6u : 4u) * kBars && wrong == 0, "Rear Guard %u: %u notes, %u wrong", st, n, wrong);
            delete s;
        }
    }
    {  // Syncopation: an off-beat note 0.3 louder with a square blip an octave up; none on the beat.
        Session* s = upgradeRun(withMod(Mod::Syncopation, 1), kBars, true, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::SyncBlip, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i) wrong += inBarOf(g_hits[i].tick) % 96 == 0 || std::lround(g_hits[i].x) != leadAt(*s, g_hits[i].tick) + 12;
        CHECK_MSG(n == 12 * kBars && wrong == 0, "Syncopation: %u blips, %u wrong", n, wrong);
        delete s;
        s = upgradeRun(withMod(Mod::Syncopation, 1), 0, true, Section::Groove, 0.0f);
        s->run(static_cast<u64>(barFrames(0) * 2) + 100);
        float onBeat = 0.0f, off = 0.0f;
        u32 nv = s->e.leadVoices(v, 128);
        for (u32 i = 0; i < nv; ++i)
            if (v[i].start == static_cast<i64>(barFrames(0) * 2) && v[i].timbre == 0) onBeat = v[i].gain;
        s->run(6000);
        nv = s->e.leadVoices(v, 128);
        for (u32 i = 0; i < nv; ++i)
            if (v[i].start == static_cast<i64>(barFrames(0) * 2) + 6000 && v[i].timbre == 0) off = v[i].gain;
        CHECK_MSG(onBeat > 0.0f && std::fabs(off / onBeat - 1.3f) < 1e-4f, "Syncopation: off-beat %.4f, on the beat %.4f", off, onBeat);
        delete s;
    }
    {  // Bar Line: on every One the chord's stab and a zap on its root.
        Session* s = upgradeRun(withMod(Mod::BarLine, 1), kBars, false, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::BarLineZap, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i)
            wrong += inBarOf(g_hits[i].tick) != 0 || std::lround(g_hits[i].x) != chordToneT(t1.chordAtBar(barOf(g_hits[i].tick)), 0, 5);
        CHECK_MSG(n == kBars && wrong == 0 && upEvents(*s, U::ModStab, g_hits, 8192) == kBars, "Bar Line: %u zaps, %u wrong", n, wrong);
        delete s;
    }
    {  // Crescendo: the lead 0.2 dB louder a bar without a hit, to +2 dB, the pad opening with it; a hit drops both at once.
        Session* s = upgradeRun(withMod(Mod::Crescendo, 1), 0, true, Section::Groove, 1.0f);
        Session* o = upgradeRun(PlayerLoadout{}, 0, true, Section::Groove, 1.0f);
        const auto newest = [&](Session& x) {
            const u32 n = x.e.leadVoices(v, 128);
            i64 best = -1;
            float g = 0.0f;
            for (u32 i = 0; i < n; ++i)
                if (v[i].start > best) {
                    best = v[i].start;
                    g = v[i].gain;
                }
            return g;
        };
        const u64 at = static_cast<u64>(barFrames(0) * 3) + 100;  // bar 3: four bar lines counted, +0.8 dB
        s->run(at);
        o->run(at);
        const float r3 = newest(*s) / newest(*o);
        CHECK_MSG(std::fabs(r3 - std::pow(10.0f, 0.8f / 20.0f)) < 1e-4f, "Crescendo after 4 bars: x%.4f", r3);
        CHECK(std::fabs(s->e.padOpenNow() - std::pow(10.0f, 0.8f / 20.0f)) < 1e-4f);
        s->run(static_cast<u64>(barFrames(0) * 10));
        o->run(static_cast<u64>(barFrames(0) * 10));
        const float r13 = newest(*s) / newest(*o);
        CHECK_MSG(std::fabs(r13 - std::pow(10.0f, 2.0f / 20.0f)) < 1e-4f, "Crescendo at its top: x%.4f", r13);
        Command hit = cmd(CmdType::Cue, static_cast<u8>(Cue::PlayerHit), 0, 1);
        hit.when = Timing::Next32;
        s->post(hit);
        s->run(6000);
        o->run(6000);
        const float after = newest(*s) / newest(*o);
        CHECK_MSG(std::fabs(after - 1.0f) < 1e-4f && std::fabs(s->e.padOpenNow() - 1.0f) < 1e-6f, "Crescendo after a hit: x%.4f, pad %.4f", after,
                  s->e.padOpenNow());
        delete s;
        delete o;
    }
    {  // Kick Pulse on Afterglow (half time: kicks on 1 and the and of 3): a tom with each kick, none on the other beats.
        Session* s = upgradeRun(withMod(Mod::KickPulse, 1), kBars, false, Section::Groove, 0.0f, 6);
        u32 kicks = 0, toms = 0, matched = 0;
        for (u32 i = 0; i < s->count; ++i) {
            const BeatEvent& e = s->log[i];
            if (e.kind == BeatKind::Kick) {
                ++kicks;
                for (u32 j = 0; j < s->count; ++j)
                    matched += s->log[j].kind == BeatKind::Sfx && s->log[j].a == kUpgradeEvent + static_cast<u8>(U::KickPulseTom) && s->log[j].frame == e.frame;
            }
            toms += e.kind == BeatKind::Sfx && e.a == kUpgradeEvent + static_cast<u8>(U::KickPulseTom);
        }
        const i32 note = static_cast<i32>(std::lround(69.0 + 12.0 * std::log2(g_music->sound[6][g_music->defaultVariant].kickHz / 440.0))) + 12;
        CHECK(upEvents(*s, U::KickPulseTom, g_hits, 1) && std::lround(g_hits[0].x) == note);
        CHECK_MSG(kicks == 2 * kBars && toms == kicks && matched == kicks, "Kick Pulse: %u kicks, %u toms, %u together", kicks, toms, matched);
        delete s;
    }
    {  // Afterparty: the Surge (the Drop's first half: bars 12-15 of the Short form) holds its second gear two bars longer,
       // a step faster (1/32 at once).
        const auto grids = [&](const PlayerLoadout& l, u32* out) {
            Session* s = upgradeRun(l, 21, true, Section::Groove, 0.0f, 0, false);
            for (u32 b = 0; b < 21; ++b) out[b] = 0;
            for (u32 i = 0; i < s->count; ++i)
                if (s->log[i].kind == BeatKind::Lead && s->log[i].c) out[barOf(s->log[i].tick)] = s->log[i].c;  // c: the grid
            delete s;
        };
        u32 plain[21], party[21];
        grids(PlayerLoadout{}, plain);
        grids(withMod(Mod::Afterparty, 1), party);
        CHECK_MSG(plain[12] == 16 && plain[14] == 12 && plain[16] == 24 && plain[18] == 24, "the Surge: %u %u %u %u", plain[12], plain[14], plain[16], plain[18]);
        CHECK_MSG(party[12] == 12 && party[14] == 12 && party[16] == 12 && party[17] == 12 && party[18] == 24, "Afterparty: %u %u %u %u %u", party[12],
                  party[14], party[16], party[17], party[18]);
    }
}

// The Boosts' sound (conductor.json boosts).
void testBoostsPatterned() {
    constexpr u32 kBars = 6;
    const Track& t1 = g_music->tracks[0];
    static LeadVoiceInfo v[128];
    {  // Time Stretch: the drums at half rate (Track 1's kick on every beat: on 1 and 3).
        Session* s = upgradeRun(withBoost(Boost::TimeStretch), kBars, false, Section::Groove, 0.2f);
        Session* o = upgradeRun(PlayerLoadout{}, kBars, false, Section::Groove, 0.2f);
        u32 wrong = 0;
        for (u32 i = 0; i < s->count; ++i) wrong += s->log[i].kind == BeatKind::Kick && inBarOf(s->log[i].tick) % 192 != 0;
        CHECK_MSG(countKind(*s, BeatKind::Kick) == 2 * kBars && countKind(*o, BeatKind::Kick) == 4 * kBars && wrong == 0, "Time Stretch: %u kicks (%u without)",
                  countKind(*s, BeatKind::Kick), countKind(*o, BeatKind::Kick));
        CHECK(countKind(*s, BeatKind::Lead) == countKind(*o, BeatKind::Lead));
        delete s;
        delete o;
    }
    {  // Freeze Frame: a glass chord as it starts, the music high-passed (much less below 150 Hz).
        Capture c[2];
        for (u32 k = 0; k < 2; ++k) {
            Engine e;
            CHECK(e.init(*g_music, *g_bank, kRate));
            e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
            e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
            e.post(cmd(CmdType::HoldSection, 1));
            if (k) e.post(cmd(CmdType::SetBoost, 0, static_cast<u8>(Boost::FreezeFrame), 1));
            runEngine(e, c[k], kRate * 4, 256);
        }
        const auto low = [&](const Capture& x) {
            double lp = 0.0, sum = 0.0;
            const double a = 1.0 - std::exp(-2.0 * kPiD * 150.0 / kRate);
            for (u64 i = kRate; i < x.frames; ++i) {
                lp += (x.out[2 * i] - lp) * a;
                sum += lp * lp;
            }
            return sum;
        };
        u32 glass = 0;
        for (u32 i = 0; i < c[1].eventCount; ++i) glass += c[1].events[i].kind == BeatKind::Sfx && c[1].events[i].a == kUpgradeEvent + static_cast<u8>(U::FreezeGlass);
        CHECK_MSG(glass == 1 && low(c[1]) < 0.25 * low(c[0]), "Freeze Frame: %u glass chords, low end %.3f of the open music's", glass, low(c[1]) / low(c[0]));
        freeCapture(c[0]);
        freeCapture(c[1]);
    }
    {  // Colossus: the bass's octave below with every bass note.
        Session* s = upgradeRun(withBoost(Boost::Colossus), kBars, false);
        const u32 n = upEvents(*s, U::ColossusBass, g_hits, 8192);
        u32 matched = 0;
        for (u32 i = 0; i < n; ++i)
            for (u32 j = 0; j < s->count; ++j)
                matched += s->log[j].kind == BeatKind::Bass && std::fabs(s->log[j].tick - g_hits[i].tick) < 0.5 && s->log[j].a == std::lround(g_hits[i].x) + 12;
        CHECK_MSG(n > 0 && n == countKind(*s, BeatKind::Bass) && matched == n, "Colossus: %u notes for %u bass notes, %u an octave under", n,
                  countKind(*s, BeatKind::Bass), matched);
        delete s;
    }
    {  // Chain Storm: on every beat a zap on each of the chord's three stab notes.
        Session* s = upgradeRun(withBoost(Boost::ChainStorm), kBars, false);
        const u32 n = upEvents(*s, U::ChainStormZap, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i) {
            const Chord& ch = t1.chordAtBar(barOf(g_hits[i].tick));
            bool stab = false;
            for (u32 k = 0; k < ch.stabCount; ++k) stab = stab || std::lround(g_hits[i].x) == ch.stab[k];
            wrong += inBarOf(g_hits[i].tick) % 96 != 0 || !stab;
        }
        CHECK_MSG(n == 12 * kBars && wrong == 0, "Chain Storm: %u zaps, %u wrong", n, wrong);
        delete s;
    }
    {  // Chorus: each lead note doubled a fifth above and an octave below at 0.4.
        Session* s = upgradeRun(withBoost(Boost::Chorus), kBars, true, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::ChorusNote, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i) {
            const i32 lead = leadAt(*s, g_hits[i].tick);
            wrong += std::lround(g_hits[i].x) != lead - 12 && std::lround(g_hits[i].x) != std::lround(scaleStepT(t1, static_cast<float>(lead), 4));
        }
        CHECK_MSG(n == 2 * countKind(*s, BeatKind::Lead) && wrong == 0, "Chorus: %u notes, %u wrong", n, wrong);
        delete s;
    }
    {  // Beam: the lead holds (four times the decay and length), retriggered softly.
        Session* s = upgradeRun(withBoost(Boost::Beam), 0, true, Section::Groove, 0.0f);
        Session* o = upgradeRun(PlayerLoadout{}, 0, true, Section::Groove, 0.0f);
        const u64 at = static_cast<u64>(barFrames(0) * 2) + 100;
        s->run(at);
        o->run(at);
        u32 n = s->e.leadVoices(v, 128);
        u32 len = 0;
        float vel = 0.0f;
        for (u32 i = 0; i < n; ++i)
            if (v[i].start == static_cast<i64>(barFrames(0) * 2)) {
                len = v[i].length;
                vel = v[i].vel;
            }
        n = o->e.leadVoices(v, 128);
        u32 len0 = 0;
        float vel0 = 0.0f;
        for (u32 i = 0; i < n; ++i)
            if (v[i].start == static_cast<i64>(barFrames(0) * 2)) {
                len0 = v[i].length;
                vel0 = v[i].vel;
            }
        CHECK_MSG(len0 && std::fabs(static_cast<float>(len) / static_cast<float>(len0) - 4.0f) < 0.01f && std::fabs(vel / vel0 - 0.6f) < 1e-5f,
                  "Beam: length %u (%u without), velocity %.3f (%.3f)", len, len0, vel, vel0);
        delete s;
        delete o;
    }
    {  // Fractal: each note followed a 1/32 apart by its octave (0.3) and its double octave (0.15).
        Session* s = upgradeRun(withBoost(Boost::Fractal), kBars, true, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::FractalNote, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i) {
            const i32 one = leadAt(*s, g_hits[i].tick - 12.0), two = leadAt(*s, g_hits[i].tick - 24.0);
            wrong += !((one >= 0 && std::lround(g_hits[i].x) == one + 12) || (two >= 0 && std::lround(g_hits[i].x) == two + 24));
        }
        CHECK_MSG(n + 2 >= 2 * countKind(*s, BeatKind::Lead) && n > 0 && wrong == 0, "Fractal: %u notes for %u lead notes, %u wrong", n,
                  countKind(*s, BeatKind::Lead), wrong);
        delete s;
    }
    {  // Carousel: a tom on every beat, root, third, fifth and octave of the chord.
        Session* s = upgradeRun(withBoost(Boost::Carousel), kBars, false);
        const u32 n = upEvents(*s, U::CarouselTom, g_hits, 8192);
        u32 wrong = 0;
        static constexpr u32 kTone[4] = {0, 1, 2, 4};
        for (u32 i = 0; i < n; ++i)
            wrong += inBarOf(g_hits[i].tick) % 96 != 0 ||
                     std::lround(g_hits[i].x) != chordToneT(t1.chordAtBar(barOf(g_hits[i].tick)), kTone[inBarOf(g_hits[i].tick) / 96], 3);
        CHECK_MSG(n == 4 * kBars && wrong == 0, "Carousel: %u toms, %u wrong", n, wrong);
        delete s;
    }
    {  // Magnet Storm: the Shard run climbs twice as fast.
        for (u32 k = 0; k < 2; ++k) {
            Session* s = upgradeRun(k ? withBoost(Boost::MagnetStorm) : PlayerLoadout{}, 1, false);
            for (u32 j = 0; j < 4; ++j) s->post(cmd(CmdType::Shard, 0, 0, 0, 1.0f));
            s->run(kRate / 2);
            u32 seen = 0, wrong = 0;
            for (u32 i = 0; i < s->count; ++i) {
                const BeatEvent& e = s->log[i];
                if (e.kind != BeatKind::Sfx || e.a != static_cast<u8>(Sfx::Shard)) continue;
                wrong += std::lround(midiOf(e.x)) != g_bank->shardScale(0)[seen * (k ? 2 : 1)];
                ++seen;
            }
            CHECK_MSG(seen == 4 && wrong == 0, "Magnet Storm %u: %u Shards, %u off the run", k, seen, wrong);
            delete s;
        }
    }
}

// The Fusions (sound_m5.json fusions): each one's sound, and its ingredients' sounds gone.
void testFusions() {
    constexpr u32 kBars = 6;
    const Track& t1 = g_music->tracks[0];
    static LeadVoiceInfo v[128];
    const auto count = [&](const PlayerLoadout& l, U k, bool fire = true) {
        Session* s = upgradeRun(l, kBars, fire, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, k, g_hits, 8192);
        delete s;
        return n;
    };
    {  // Standing Wave (Echo + Accent): each accented note held a bar as a pad voice; no stab, no Echo delay.
        const PlayerLoadout l = withFusion(Fusion::StandingWave, withMod(Mod::Echo, 2, withMod(Mod::Accent, 3)));
        Session* s = upgradeRun(l, kBars, true, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::StandingWave, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i) wrong += inBarOf(g_hits[i].tick) % 96 != 0 || std::lround(g_hits[i].x) != leadAt(*s, g_hits[i].tick);
        CHECK_MSG(n == 4 * kBars && wrong == 0 && upEvents(*s, U::ModStab, g_hits, 1) == 0 && s->e.delayFrames() == 18000,
                  "Standing Wave: %u held notes, %u wrong, delay %u", n, wrong, s->e.delayFrames());
        delete s;
    }
    {  // Tesla Bloom (Pulse Ring + Arc): each ring carries zaps; Arc's chain zaps are gone.
        const PlayerLoadout l = withFusion(Fusion::TeslaBloom, withRiff(Riff::PulseRing, 5, withMod(Mod::Arc, 3)));
        CHECK(count(l, U::TeslaZap, false) == 4 * kBars);  // a ring every beat from level 2
        Capture c[2];
        for (u32 k = 0; k < 2; ++k) {  // an Arc chain posted or not: the same render
            Engine e;
            CHECK(e.init(*g_music, *g_bank, kRate));
            e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
            Command f = cmd(CmdType::SetFusions, 0, static_cast<u8>(1u << static_cast<u32>(Fusion::TeslaBloom)));
            e.post(f);
            if (k) {
                Command a = cmd(CmdType::Cue, static_cast<u8>(Cue::ArcChain), 0, 3, 1.0f);
                a.tick = 384.0;
                a.when = Timing::AtTick;
                e.post(a);
            }
            runEngine(e, c[k], kRate * 2, 256);
        }
        CHECK_MSG(std::memcmp(c[0].out, c[1].out, sizeof(float) * c[0].frames * 2) == 0, "Tesla Bloom: Arc's zap still sounds");
        freeCapture(c[0]);
        freeCapture(c[1]);
    }
    {  // Shard Halo (Orbitals + Shatter): the shaker doubled by glass ticks an octave apart.
        const PlayerLoadout l = withFusion(Fusion::ShardHalo, withRiff(Riff::Orbitals, 5, withMod(Mod::Shatter, 3)));
        Session* s = upgradeRun(l, kBars, false, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::HaloGlass, g_hits, 8192);
        u32 octaves = 0;
        for (u32 i = 1; i < n; ++i) octaves += std::abs(std::lround(g_hits[i].x) - std::lround(g_hits[i - 1].x)) == 12;
        CHECK_MSG(n == countKind(*s, BeatKind::Shaker) && n > 0 && octaves + 1 >= n - kBars, "Shard Halo: %u glass ticks for %u shakers, %u octave leaps", n,
                  countKind(*s, BeatKind::Shaker), octaves);
        delete s;
    }
    {  // Downbeat Cannon (Lancer + On-Beat Crit): the chord at 1.2 and the impact on every One; no Lancer, no crit octave.
        const PlayerLoadout l = withFusion(Fusion::DownbeatCannon, withRiff(Riff::Lancer, 5, withMod(Mod::OnBeatCrit, 3)));
        Session* s = upgradeRun(l, kBars, true, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::CannonOne, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i) wrong += inBarOf(g_hits[i].tick) != 0 || std::fabs(g_hits[i].value - 1.2f) > 1e-6f;
        CHECK_MSG(n == kBars && wrong == 0 && upEvents(*s, U::LancerChord, g_hits, 1) == 0, "Downbeat Cannon: %u, %u wrong", n, wrong);
        s->run(static_cast<u64>(barFrames(0)) + 100 - static_cast<u64>(barFrames(0)) % 1);  // a beat note: no octave above it
        const u32 nv = s->e.leadVoices(v, 128);
        u32 octave = 0;
        for (u32 i = 0; i < nv; ++i)
            for (u32 j = 0; j < nv; ++j) octave += v[i].start == v[j].start && std::fabs(v[j].hz - 2.0f * v[i].hz) < 0.5f;
        CHECK_MSG(octave == 0, "Downbeat Cannon: %u crit octaves", octave);
        delete s;
    }
    {  // Collapse Field (Minelayer + Gravity Rounds): a swell over the last beat into the detonation; Gravity's thup gone.
        const PlayerLoadout l = withFusion(Fusion::CollapseField, withRiff(Riff::Minelayer, 5, withMod(Mod::GravityRounds, 2)));
        Session* s = upgradeRun(l, kBars, false, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::CollapseSwell, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i) wrong += inBarOf(g_hits[i].tick) != 288;
        CHECK_MSG(n == kBars && wrong == 0, "Collapse Field: %u swells, %u wrong", n, wrong);
        delete s;
        Capture c[2];
        for (u32 k = 0; k < 2; ++k) {
            Engine e;
            CHECK(e.init(*g_music, *g_bank, kRate));
            e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
            e.post(cmd(CmdType::SetFusions, 0, static_cast<u8>(1u << static_cast<u32>(Fusion::CollapseField))));
            if (k) {
                Command a = cmd(CmdType::Cue, static_cast<u8>(Cue::GravityHit), 0, 3);
                a.tick = 384.0;
                a.when = Timing::AtTick;
                e.post(a);
            }
            runEngine(e, c[k], kRate * 2, 256);
        }
        CHECK_MSG(std::memcmp(c[0].out, c[1].out, sizeof(float) * c[0].frames * 2) == 0, "Collapse Field: Gravity's thup still sounds");
        freeCapture(c[0]);
        freeCapture(c[1]);
    }
    {  // Murmuration (Swarm + Seeker): a 1/32 trill between the chord's third and fifth; no volley, no glide.
        const PlayerLoadout l = withFusion(Fusion::Murmuration, withRiff(Riff::Swarm, 5, withMod(Mod::Seeker, 3)));
        Session* s = upgradeRun(l, kBars, true, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::MurmurationTrill, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i) {
            const Chord& ch = t1.chordAtBar(barOf(g_hits[i].tick));
            const i32 want = chordToneT(ch, (inBarOf(g_hits[i].tick) / 12) % 2 ? 2 : 1, 5);
            wrong += std::lround(g_hits[i].x) != want;
        }
        CHECK_MSG(n == 32 * kBars && wrong == 0 && upEvents(*s, U::SwarmBell, g_hits, 1) == 0, "Murmuration: %u trill notes, %u wrong", n, wrong);
        const u32 nv = s->e.leadVoices(v, 128);
        u32 glides = 0;
        for (u32 i = 0; i < nv; ++i) glides += v[i].glideN != 0;
        CHECK_MSG(glides == 0, "Murmuration: %u notes still glide", glides);
        delete s;
    }
    {  // Quartet (Mirror + Rear Guard): the note in four parts; no Mirror octave, no counter-melody.
        const PlayerLoadout l = withFusion(Fusion::Quartet, withRiff(Riff::Mirror, 5, withMod(Mod::RearGuard, 2)));
        Session* s = upgradeRun(l, kBars, true, Section::Groove, 0.0f);
        const u32 n = upEvents(*s, U::QuartetNote, g_hits, 8192);
        u32 wrong = 0;
        for (u32 i = 0; i < n; ++i) {
            const i32 lead = leadAt(*s, g_hits[i].tick), x = static_cast<i32>(std::lround(g_hits[i].x));
            wrong += x != std::lround(scaleStepT(t1, static_cast<float>(lead), 2)) && x != std::lround(scaleStepT(t1, static_cast<float>(lead), 4)) &&
                     x != lead + 12;
        }
        CHECK_MSG(n == 3 * countKind(*s, BeatKind::Lead) && wrong == 0 && upEvents(*s, U::MirrorNote, g_hits, 1) == 0 &&
                      upEvents(*s, U::RearGuardNote, g_hits, 1) == 0,
                  "Quartet: %u notes, %u wrong", n, wrong);
        delete s;
    }
}

// The content engineer's eight (cues.h, appended): notes named against the chord at the tick they play (scale steps,
// semitones, chord tones, from the chord's own notes: Aurora's B major has D#), a held pad note gliding an octave, the
// impact pitched, the driven kick louder than the kick.
void testChordNotesAndNewRecipes() {
    {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 3, 0));  // Aurora: Em, Am, C, B
        e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 0.0f));
        const auto at = [&](Recipe r, i32 midi, double tick) {
            Command k = cmd(CmdType::Recipe, static_cast<u8>(r), 0, 255, 1.0f);
            encodeNote(k, midi);
            k.when = Timing::AtTick;
            k.tick = tick;
            e.post(k);
        };
        Capture warm;
        runEngine(e, warm, 512, 256);  // bar 0 planned
        freeCapture(warm);
        // Posted during bar 0 for bar 3 (B major: B, D#, F#, no seventh: the scale's A) and bar 0 (E minor).
        for (i32 k = 0; k < 4; ++k) at(Recipe::RimShot, chordNote(k, 5, NoteFrom::ChordTone), 3.0 * 384.0 + 96.0 * k);
        at(Recipe::RimShot, chordNote(2, 4), 96.0);                      // two scale steps over E4: G4
        at(Recipe::RimShot, chordNote(-1, 4), 120.0);                    // one down: D4
        at(Recipe::RimShot, chordNote(7, 3, NoteFrom::Semitones), 144.0);  // a fifth over E3: B3
        Capture c;
        runEngine(e, c, static_cast<u64>(barFrames(3) * 4.5), 256);
        i32 got[7] = {-1, -1, -1, -1, -1, -1, -1};
        u32 n = 0;
        for (u32 i = 0; i < c.eventCount && n < 7; ++i)
            if (c.events[i].kind == BeatKind::Sfx && c.events[i].a == 64 + static_cast<u8>(Recipe::RimShot)) got[n++] = static_cast<i32>(std::lround(c.events[i].x));
        const i32 want[7] = {83, 87, 90, 93, 67, 62, 59};
        u32 wrong = 0;
        for (u32 i = 0; i < 7; ++i) wrong += got[i] != want[i];
        CHECK_MSG(n == 7 && wrong == 0, "chord notes: %d %d %d %d %d %d %d", got[0], got[1], got[2], got[3], got[4], got[5], got[6]);
        freeCapture(c);
    }
    // A pad voice from A4, gliding up an octave over half a second (Script: dry, alone).
    {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), 0, 0));
        Command s = cmd(CmdType::VoiceStart, 2, static_cast<u8>(Recipe::PadVoice), 69, 1.0f);
        s.z = 0.02f;
        e.post(s);
        Capture a, b;
        runEngine(e, a, kRate, 256);
        Command g = cmd(CmdType::VoiceSet, 2, 0, 0, 0.0f);
        g.y = -1.0f;
        encodeNote(g, 81);
        g.z = 0.5f;
        e.post(g);
        runEngine(e, b, kRate, 256);
        const auto at = [](const Capture& x, u64 from, double hz) {  // Goertzel power over 0.25 s
            const double w = 2.0 * kPiD * hz / kRate, k = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (u64 i = from; i < from + kRate / 4; ++i) {
                const double s0 = x.out[2 * i] + k * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - k * s1 * s2;
        };
        const double before = at(a, kRate / 2, 440.0) / (at(a, kRate / 2, 880.0) + 1e-12), after = at(b, kRate * 3 / 4 - kRate / 4, 880.0) / (at(b, kRate / 2, 440.0) + 1e-12);
        CHECK_MSG(before > 10.0 && after > 10.0, "pad glide: 440/880 before %.1f, 880/440 after %.1f", before, after);
        freeCapture(a);
        freeCapture(b);
    }
    // The impact a fifth up rings 2^(-7/12) as long; the driven kick is louder than the kick.
    const auto ring = [](Recipe r, float p1, double& rms) {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), 0, 0));
        Command k = cmd(CmdType::Recipe, static_cast<u8>(r), 0, 255, 1.0f);
        k.z = p1;
        e.post(k);
        Capture c;
        runEngine(e, c, kRate * 3, 256);
        u64 last = 0;
        double s = 0.0;
        for (u64 i = 0; i < c.frames; ++i) {
            if (std::fabs(c.out[2 * i]) > 1e-4f) last = i;
            s += static_cast<double>(c.out[2 * i]) * c.out[2 * i];
        }
        rms = s;
        freeCapture(c);
        return static_cast<double>(last);
    };
    double r0 = 0.0, r7 = 0.0, k1 = 0.0, k3 = 0.0;
    const double plain = ring(Recipe::Impact, 0.0f, r0), fifth = ring(Recipe::Impact, 7.0f, r7);
    CHECK_MSG(std::fabs(fifth / plain - std::exp2(-7.0 / 12.0)) < 0.05, "the impact a fifth up rings %.3f as long", fifth / plain);
    ring(Recipe::DrivenKick, 0.01f, k1);
    ring(Recipe::DrivenKick, 3.0f, k3);
    CHECK_MSG(k3 > 1.3 * k1, "the driven kick: %.3f x the energy of the kick", k3 / k1);
}

// Everything owned at once (the four players of --band max, no cues): the same render in random bursts as in fixed ones,
// no allocation; with the game's events every 1/60 s: no voice stolen, no allocation, on every Track's tempo extreme.
void testUpgradesWorstCase() {
    Command setup[64];
    const u32 n = maxLoadSetup(setup, 64);
    Capture c[2];
    for (u32 k = 0; k < 2; ++k) {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 9, 0));
        // (Held in the first section, the Groove: a section forced on a bar line, as the load test forces the Drop, ramps its
        // gain from where the blocks fell; the game's director names its sections a bar ahead.)
        for (u32 i = 0; i < n; ++i)
            if (setup[i].type != CmdType::SetSection) e.post(setup[i]);
        runEngine(e, c[k], kRate * 6, k ? 0 : 256, 11);
    }
    CHECK_MSG(std::memcmp(c[0].out, c[1].out, sizeof(float) * c[0].frames * 2) == 0 && c[0].allocs == 0 && c[1].allocs == 0,
              "the worst case: random bursts differ from fixed ones, or allocations (%llu, %llu)", static_cast<unsigned long long>(c[0].allocs),
              static_cast<unsigned long long>(c[1].allocs));
    freeCapture(c[0]);
    freeCapture(c[1]);
    for (u32 t : {0u, 9u}) {
        auto* s = new Session(400000);
        s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), static_cast<u8>(t), 0));
        for (u32 i = 0; i < n; ++i) s->post(setup[i]);
        Command feed[64];
        for (u32 f = 0; f < 60 * 8; ++f) {
            const u32 m = maxLoadFrame(f, feed, 64);
            for (u32 i = 0; i < m; ++i) s->post(feed[i]);
            s->run(kRate / 60, 240);
        }
        CHECK_MSG(s->e.stats().steals.load() == 0 && s->allocs == 0 && s->offGrid == 0, "%s worst case: steals %u, allocations %llu, off the grid %u",
                  g_music->tracks[t].id, s->e.stats().steals.load(), static_cast<unsigned long long>(s->allocs), s->offGrid);
        std::printf("  worst case %s: voices peak %u (lead %u)\n", g_music->tracks[t].id, s->e.stats().voicesPeak.load(), s->e.stats().leadPeak.load());
        delete s;
    }
}

// M5 audio, the fights' pending sounds: the bus cut (the pads and bass out for its window, back after it), the timed
// high-pass, the pad's tone, a fight's lead notes (a recorded note an octave down, its mirror about the chord's root, a bend,
// their cap), the aggregated recipes (one shot a 1/32 step at 1 - e^(-n / 2.5)), the gated fifth (silent on the off-eighths),
// the stab of five notes; none allocates.
struct BusTap {
    double pad = 0.0, bass = 0.0, drums = 0.0;
    double low = 0.0, all = 0.0;  // the master: below ~150 Hz (a one-pole low-pass), and all of it
    float lp = 0.0f;
};
void busTap(void* user, const float* const* busL, const float* const*, const float*, const float*, u32 n) {
    auto* t = static_cast<BusTap*>(user);
    for (u32 i = 0; i < n; ++i) {
        t->pad += static_cast<double>(busL[static_cast<u32>(Bus::Pad)][i]) * busL[static_cast<u32>(Bus::Pad)][i];
        t->bass += static_cast<double>(busL[static_cast<u32>(Bus::Bass)][i]) * busL[static_cast<u32>(Bus::Bass)][i];
        t->drums += static_cast<double>(busL[static_cast<u32>(Bus::Drums)][i]) * busL[static_cast<u32>(Bus::Drums)][i];
    }
}
void testFightSounds() {
    const double bar = barFrames(0);
    // The cut and the pad's tone: Track 1's Drop, the pads and the bass measured on their buses (stems) bar by bar.
    {
        auto* s = new Session(100000);
        BusTap tap;
        s->e.setStemTap(busTap, &tap);
        s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
        s->post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
        s->post(cmd(CmdType::SetSection, static_cast<u8>(Section::Drop)));
        s->post(cmd(CmdType::HoldSection, 1));
        s->run(static_cast<u64>(bar * 2));
        const auto barOf = [&]() {
            tap = BusTap{};
            s->run(static_cast<u64>(bar));
            return tap;
        };
        const BusTap open = barOf();
        Command cut = cmd(CmdType::CutBed);
        cut.mods[0] = static_cast<u8>((1u << static_cast<u32>(Bus::Pad)) | (1u << static_cast<u32>(Bus::Bass)));
        cut.mods[1] = static_cast<u8>(((1u << static_cast<u32>(Bus::Pad)) | (1u << static_cast<u32>(Bus::Bass))) >> 8);
        cut.y = static_cast<float>(bar / kRate * 1.2);
        s->post(cut);
        const BusTap shut = barOf();
        s->run(static_cast<u64>(bar));
        const BusTap back = barOf();
        CHECK_MSG(open.pad > 0.0 && open.bass > 0.0 && shut.pad < 0.01 * open.pad && shut.bass < 0.01 * open.bass && shut.drums > 0.5 * open.drums,
                  "the cut: pad %.3g -> %.3g, bass %.3g -> %.3g, drums %.3g -> %.3g", open.pad, shut.pad, open.bass, shut.bass, open.drums, shut.drums);
        CHECK_MSG(back.pad > 0.5 * open.pad && back.bass > 0.5 * open.bass, "after the cut: pad %.3g, bass %.3g", back.pad, back.bass);
        Command tone = cmd(CmdType::PadTone, 0, 0, 0, 1.0f);
        tone.y = 0.5f;
        tone.z = 0.01f;
        s->post(tone);
        s->run(static_cast<u64>(bar));
        const BusTap ducked = barOf();
        CHECK_MSG(ducked.pad > 0.15 * back.pad && ducked.pad < 0.35 * back.pad, "the pad ducked by half: energy x %.3f", ducked.pad / back.pad);
        CHECK_MSG(s->allocs == 0, "allocations %llu", static_cast<unsigned long long>(s->allocs));
        delete s;
    }
    // The timed high-pass: the music's lows go for its window (Goertzel at 55 Hz, the kick's and the bass's).
    {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Staged), 0, 0));
        Capture warm, a, b;
        runEngine(e, warm, kRate * 8, 256);
        runEngine(e, a, kRate * 2, 256);
        Command hp = cmd(CmdType::HighPassBed, 0, 0, 0, 800.0f);
        hp.y = 3.0f;
        e.post(hp);
        runEngine(e, b, kRate * 2, 256);
        const auto lows = [](const Capture& c) {
            double s = 0.0, y = 0.0;
            for (u64 i = 0; i < c.frames; ++i) {
                y += (c.out[2 * i] - y) * 0.02;  // ~150 Hz one-pole
                s += y * y;
            }
            return s;
        };
        CHECK_MSG(lows(b) < 0.1 * lows(a), "the high-pass: lows x %.3f", lows(b) / (lows(a) + 1e-30));
        CHECK(a.allocs == 0 && b.allocs == 0);
        freeCapture(warm);
        freeCapture(a);
        freeCapture(b);
    }
    // A fight's lead: the player fires a bar; then its note of a bar before an octave down (Recorded), its mirror about the
    // chord's root (Mirror), a bend; at most kFightLeadVoices sound at once.
    {
        auto* s = new Session(100000);
        s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
        s->post(cmd(CmdType::SetIntensity, 0, 0, 0, 0.0f));
        s->post(cmd(CmdType::HoldSection, 1));
        s->post(cmd(CmdType::SetFire, 0, 1));
        s->run(static_cast<u64>(bar + 1000));  // a bar of the lead (and a little)
        s->post(cmd(CmdType::SetFire, 0, 0));
        const auto echo = [&](LeadEchoMode m, double back, i8 octaves, i8 bend) {
            Command k = cmd(CmdType::LeadEcho, 0, static_cast<u8>(m), 255, 1.0f);
            k.level = static_cast<float>(back);
            k.z = 1200.0f;
            k.mods[5] = 255;
            k.mods[6] = static_cast<u8>(octaves);
            k.mods[7] = static_cast<u8>(bend);
            s->post(k);
            s->run(256);
            LeadVoiceInfo info[128];
            const u32 n = s->e.leadVoices(info, 128);
            double hz = 0.0;
            i64 latest = -1;
            for (u32 i = 0; i < n; ++i)
                if (info[i].player == kFightLeadPlayer && info[i].start > latest) {
                    latest = info[i].start;
                    hz = info[i].hz;
                }
            return hz > 0.0 ? midiOf(hz) : -1.0;
        };
        // The note on the bar's first step (tick 0 at the song's start: the arpeggio's first note, A minor's).
        const Chord& am = g_music->tracks[0].chordAtBar(0);
        const double now = s->e.tempo().tickAt(static_cast<double>(s->frame));
        const double back = now - 0.0;
        const double rec = echo(LeadEchoMode::Recorded, back, -1, 0);
        CHECK_MSG(std::fabs(rec - (am.arp[0] - 12)) < 0.01, "recorded: %.2f, want %d", rec, am.arp[0] - 12);
        const double now2 = s->e.tempo().tickAt(static_cast<double>(s->frame));
        const double mir = echo(LeadEchoMode::Mirror, now2 - 24.0 * 2, 0, 0);  // step 2's note
        const i32 note = am.arp[2], rootPc = am.bass % 12, root = note - (((note - rootPc) % 12) + 12) % 12;
        CHECK_MSG(std::fabs(mir - (2 * root - note)) < 0.01, "mirror: %.2f, want %d (note %d, root %d)", mir, 2 * root - note, note, root);
        const double bent = echo(LeadEchoMode::Recorded, s->e.tempo().tickAt(static_cast<double>(s->frame)), 0, -7);
        CHECK_MSG(std::fabs(bent - (am.arp[0] - 7)) < 0.01, "bend: lands on %.2f, want %d", bent, am.arp[0] - 7);
        for (u32 k = 0; k < 8; ++k) echo(LeadEchoMode::Recorded, 384.0, 0, 0);
        LeadVoiceInfo info[128];
        const u32 n = s->e.leadVoices(info, 128);
        u32 fight = 0;
        for (u32 i = 0; i < n; ++i) fight += info[i].player == kFightLeadPlayer && info[i].length > static_cast<u32>(s->frame - info[i].start);
        CHECK_MSG(fight <= kFightLeadVoices + 1, "%u fight lead voices sounding (cap %u)", fight, kFightLeadVoices);
        CHECK_MSG(s->allocs == 0, "allocations %llu", static_cast<unsigned long long>(s->allocs));
        delete s;
    }
    // The aggregated recipes: five toms on one 1/32 step are one tom at 1 - e^(-5 / 2.5) of one (normalised), a second step's
    // one is one.
    {
        auto* s = new Session(100000);
        s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), 0, 0));
        s->run(4096);
        for (u32 k = 0; k < 5; ++k) {
            Command t = cmd(CmdType::Recipe, static_cast<u8>(Recipe::Tom), 1, 255, 1.0f);
            t.when = Timing::Next32;
            s->post(t);
        }
        s->run(kRate / 2);
        Command t = cmd(CmdType::Recipe, static_cast<u8>(Recipe::Tom), 1, 255, 1.0f);
        t.mods[7] = 1;
        s->post(t);
        s->run(kRate / 2);
        float v[4] = {};
        u32 got = 0;
        for (u32 i = 0; i < s->count; ++i)
            if (s->log[i].kind == BeatKind::Sfx && s->log[i].a == 64 + static_cast<u8>(Recipe::Tom) && got < 4) v[got++] = s->log[i].value;
        const float want = std::fmin(1.6f, (1.0f - std::exp(-5.0f / 2.5f)) / (1.0f - std::exp(-1.0f / 2.5f)));  // (the cues' cap: 1.6 x)
        CHECK_MSG(got == 2 && std::fabs(v[0] / v[1] - want) < 1e-3f, "aggregated: %u shots, ratio %.3f (want %.3f)", got, got == 2 ? v[0] / v[1] : 0.0f, want);
        delete s;
    }
    // The gated fifth: the off-eighths of each beat near silent against the on-eighths (Script, Track 1 at 120: an eighth is
    // 0.25 s).
    {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), 0, 0));
        Command g = cmd(CmdType::Recipe, static_cast<u8>(Recipe::EliteGated), 0, 255, 1.0f);
        e.post(g);
        Capture c;
        runEngine(e, c, kRate, 256);
        u64 first = 0;
        while (first < c.frames && std::fabs(c.out[2 * first]) < 1e-6f) ++first;
        double on = 0.0, off = 0.0;
        const u64 eighth = kRate / 4;
        for (u64 i = first; i < first + 4 * eighth && i < c.frames; ++i) {
            const u64 k = (i - first) / eighth;
            const u64 inK = (i - first) % eighth;
            if (inK < 200 || inK > eighth - 200) continue;  // (the ramps)
            (k % 2 ? off : on) += static_cast<double>(c.out[2 * i]) * c.out[2 * i];
        }
        CHECK_MSG(on > 0.0 && off < 1e-6 * on, "the gate: on %.3g, off %.3g", on, off);
        CHECK(c.allocs == 0);
        freeCapture(c);
    }
    // The stab of five notes (the Blink Star's) sounds in a sheet and does not allocate.
    {
        Engine e;
        CHECK(e.init(*g_music, *g_bank, kRate));
        e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), 0, 0));
        Command st = cmd(CmdType::Recipe, static_cast<u8>(Recipe::Stab), 0, 255, 1.0f);
        encodeNote(st, stabNotes(69, 72, 74, 76, 79));
        e.post(st);
        Capture c;
        runEngine(e, c, kRate / 2, 256);
        double en = 0.0;
        for (u64 i = 0; i < c.frames * 2; ++i) en += static_cast<double>(c.out[i]) * c.out[i];
        CHECK_MSG(en > 1e-4 && c.allocs == 0, "the five-note stab: energy %.3g, allocations %llu", en, static_cast<unsigned long long>(c.allocs));
        freeCapture(c);
    }
}

// M5 audio (GAME_DESIGN 6.6, play test 3): what kills set off is one family on a 1/32 step. A kill on each step of two beats:
// each further step of a beat at 0.7 of the one before, down to 0.3, again from 1 on the next beat; every size at once (and
// Shatter's, Freeze Frame's, Arc's and Supernova's) together no louder than one large kill.
void testKillFamily() {
    auto* s = new Session(100000);
    s->post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Script), 0, 0));  // Track 1, 120 BPM: a 1/32 step is 3000 frames
    s->run(3000 * 3);
    for (u32 k = 0; k < 16; ++k) {
        s->post(cmd(CmdType::Kill, 0, 1));
        s->run(3000, 200);
    }
    double tick[32] = {};
    float val[32] = {};
    u32 n = 0;
    for (u32 i = 0; i < s->count && n < 32; ++i)
        if (s->log[i].kind == BeatKind::Sfx && s->log[i].a == static_cast<u8>(Sfx::KillSmall)) {
            tick[n] = s->log[i].tick;
            val[n++] = s->log[i].value;
        }
    u32 wrong = 0;
    float first = 0.0f, want = 1.0f;
    i64 beat = -1;
    for (u32 i = 0; i < n; ++i) {
        const i64 b = static_cast<i64>(tick[i] / 96.0);
        if (b != beat) {
            beat = b;
            first = val[i];
            want = 1.0f;
        } else {
            want = std::fmax(0.3f, want * 0.7f);
        }
        wrong += std::fabs(val[i] - first * want) > 1e-5f * first;
    }
    CHECK_MSG(n == 16 && wrong == 0, "a chain: %u kills heard, %u off the 0.7 a step rule", n, wrong);
    // Everything at once on a fresh beat.
    s->run(3000 * 8);
    const u32 from = s->count;
    s->post(cmd(CmdType::Kill, 0, 8));
    s->post(cmd(CmdType::Kill, 1, 8));
    s->post(cmd(CmdType::Kill, 2, 8));
    for (Cue c : {Cue::ShatterBurst, Cue::FreezeShatter, Cue::ArcChain, Cue::SupernovaImpact}) {
        Command k = cmd(CmdType::Cue, static_cast<u8>(c), 0, 6, 1.0f);
        k.when = Timing::Next32;
        s->post(k);
    }
    s->run(3000, 200);
    double sum = 0.0;
    u32 sounds = 0;
    for (u32 i = from; i < s->count; ++i) {
        const BeatEvent& e = s->log[i];
        if (e.kind != BeatKind::Sfx) continue;
        const bool kill = e.a == static_cast<u8>(Sfx::KillSmall) || e.a == static_cast<u8>(Sfx::KillMedium) || e.a == static_cast<u8>(Sfx::KillLarge);
        const bool recipe = e.a == 64 + static_cast<u8>(Recipe::Glass) || e.a == 64 + static_cast<u8>(Recipe::Zap);
        if (kill || recipe) {
            sum += e.value;
            ++sounds;
        }
    }
    CHECK_MSG(sounds >= 6 && sum <= sfxLevel(Sfx::KillLarge) * 1.0001, "every size at once: %u sounds, %.4f together (one large kill %.4f)", sounds, sum,
              sfxLevel(Sfx::KillLarge));
    CHECK(s->allocs == 0);
    delete s;
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
        {"the later Tracks' risers baked behind the game (M5)", testDeferredRisers},
        {"Riffs 4-12 on their patterns (M5)", testRiffsPatterned},
        {"the Mods on the lead and in the mix (M5)", testModsOnLead},
        {"the Boosts' layers (M5)", testBoostsPatterned},
        {"the Fusions replace their ingredients (M5)", testFusions},
        {"the upgrades' worst case: bursts, allocations, steals (M5)", testUpgradesWorstCase},
        {"notes against the chord, the pad's glide, the eight new recipes (M5)", testChordNotesAndNewRecipes},
        {"the fights' sounds: the cut, the high-pass, the pad's tone, a fight's lead, aggregation, the gate, the stab (M5 audio)", testFightSounds},
        {"the kill family: one large kill at most a step, a chain at 0.7 a step (M5 audio, play test 3)", testKillFamily},
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
