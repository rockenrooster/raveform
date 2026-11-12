// Unit tests for the band (M2): the lock-free queues, the tempo map, sequencer timing (onsets on the grid,
// identical output whatever the device's burst sizes), the lead rule, variant changes on the bar line,
// the music data and allocation-free rendering. Run by scripts/test.ps1. Exit code: failures (0 = pass).

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
        u64 diff = 0;
        for (u64 i = 0; i < frames * 2; ++i) diff += a.out[i] != b.out[i];
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

// The conductor through a whole Short cycle (with the Build's 85% bar): every onset sits on the grid of
// the tempo map mirrored from the Tempo events, as the main thread sees it.
void testConductorGrid() {
    Engine e;
    CHECK(e.init(*g_music, *g_bank, kRate));
    e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
    e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 1.0f));
    e.post(cmd(CmdType::SetFire, 0, 1));
    Capture c;
    const u64 frames = static_cast<u64>(kRate) * 50;  // 24 bars at 120 BPM and a little more
    runEngine(e, c, frames, 0, 3);
    TempoMap mirror;
    bool haveMirror = false;
    u32 checked = 0, off = 0, tempos = 0, leads = 0, sections = 0;
    for (u32 i = 0; i < c.eventCount; ++i) {
        const BeatEvent& ev = c.events[i];
        if (ev.kind == BeatKind::Tempo) {
            if (ev.tick == 0.0) {
                mirror = TempoMap{};
                mirror.appendSegment({static_cast<double>(ev.frame), 0.0, ev.x});
                haveMirror = true;
            } else {
                mirror.changeAt(ev.tick, ev.x);  // the main thread's mirror: exact start frames
            }
            ++tempos;
            continue;
        }
        if (ev.kind == BeatKind::Lead) ++leads;
        if (ev.kind == BeatKind::Section) ++sections;
        if (ev.tick < 0.0 || !haveMirror) continue;
        ++checked;
        if (ev.frame != mirror.gridFrame(ev.tick)) ++off;
    }
    CHECK_MSG(tempos >= 3, "tempo segments %u (the start, the dip, the return)", tempos);
    CHECK_MSG(sections >= 4, "sections %u", sections);
    CHECK_MSG(checked > 500 && off == 0, "%u of %u conductor onsets off the grid", off, checked);
    CHECK(leads > 100);
    CHECK(c.allocs == 0);
    freeCapture(c);
}

// The lead rule on the audio side: a note on every write-grid step while fire is held, none without.
void testLeadFollowsFire() {
    Engine e;
    CHECK(e.init(*g_music, *g_bank, kRate));
    e.post(cmd(CmdType::SetMode, static_cast<u8>(Mode::Conductor), 0, 0));
    e.post(cmd(CmdType::SetIntensity, 0, 0, 0, 0.0f));
    Capture c;
    // Bar 0 without fire; then fire held for bars 1-2; then released.
    const u64 bar = 2 * kRate;  // 120 BPM
    c.out = static_cast<float*>(std::calloc(bar * 4 * 2, sizeof(float)));
    c.events = static_cast<BeatEvent*>(std::calloc(20000, sizeof(BeatEvent)));
    const auto renderTo = [&](u64 from, u64 to) {
        for (u64 f = from; f < to; f += 64) {
            e.render(c.out + f * 2, 64, 2);
            BeatEvent ev;
            while (e.popEvent(ev)) c.events[c.eventCount++] = ev;
        }
    };
    renderTo(0, bar);
    e.post(cmd(CmdType::SetFire, 0, 1));
    renderTo(bar, bar * 3);
    e.post(cmd(CmdType::SetFire, 0, 0));
    renderTo(bar * 3, bar * 4);
    u32 perBar[4] = {};
    for (u32 i = 0; i < c.eventCount; ++i)
        if (c.events[i].kind == BeatKind::Lead) {
            const u32 b = static_cast<u32>(c.events[i].tick / 384.0);
            if (b < 4) ++perBar[b];
        }
    // Fire arrives with the first callback after the post: within the first step of bar 1.
    CHECK_MSG(perBar[0] == 0, "bar 0: %u notes", perBar[0]);
    CHECK_MSG(perBar[1] >= 15 && perBar[1] <= 16, "bar 1: %u notes", perBar[1]);
    CHECK_MSG(perBar[2] == 16, "bar 2: %u notes", perBar[2]);
    CHECK_MSG(perBar[3] <= 1, "bar 3: %u notes", perBar[3]);
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
    for (Mode m : {Mode::Conductor, Mode::Staged, Mode::Metronome, Mode::Script, Mode::Silent}) {
        e.post(cmd(CmdType::SetMode, static_cast<u8>(m), 1, 1));
        e.post(cmd(CmdType::SetFire, 0, 1));
        e.post(cmd(CmdType::SetPlayers, 4, 0));
        for (u8 p = 0; p < 4; ++p) e.post(cmd(CmdType::SetFire, p, 1));
        e.post(cmd(CmdType::Kill, 0, 9));
        e.post(cmd(CmdType::Kill, 2, 1));
        e.post(cmd(CmdType::Shard, 0, 0, 0, 1.0f));
        e.post(cmd(CmdType::Nova, 0, 0, 0, 1.0f));
        e.post(cmd(CmdType::Sfx, static_cast<u8>(Sfx::Hit), 0, 0, 1.0f));
        e.post(cmd(CmdType::SetVariant, 2));
        run(600);
    }
    CHECK_MSG(allocs == 0, "%llu allocations while rendering", static_cast<unsigned long long>(allocs));
}

// ---------------------------------------------------------------- data

void testMusicData() {
    const MusicData& m = *g_music;
    CHECK(m.trackCount == 3);
    CHECK(m.variantCount == 3);
    CHECK(std::strcmp(m.variants[m.defaultVariant].id, "club") == 0);
    const i32 t2 = m.trackIndex("track02"), arcade = m.variantIndex("arcade"), club = m.variantIndex("club");
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

struct Test {
    const char* name;
    void (*fn)();
};

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
        {"staged: burst independence, grid, no allocation", testStagedBurstIndependence},
        {"conductor: grid through the Build dip", testConductorGrid},
        {"lead follows fire", testLeadFollowsFire},
        {"variant on the bar line", testVariantOnBarLine},
        {"track change through the Break", testTrackChange},
        {"no allocations in any mode", testNoAllocations},
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
