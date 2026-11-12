#include "sim/director.h"

#include <cmath>

#include "sim/tuning.h"

namespace rf::sim {

using namespace tuning;

const char* sectionLabel(Section s) {
    switch (s) {
        case Section::Groove: return "GROOVE";
        case Section::Build: return "BUILD";
        case Section::Drop: return "DROP";
        case Section::Break: return "BREAK";
    }
    return "?";
}

float bpmOfTrack(u32 track) {
    if (track <= 1) return kTrackBpm[0];
    if (track == 2) return kTrackBpm[1];
    const float b = kTrackBpm[1] + kEndlessBpmStep * static_cast<float>(track - 2);
    return b < kEndlessBpmCap ? b : kEndlessBpmCap;
}

Phrase phraseAt(const DirectorState& d, u32 bar) {
    Phrase p;
    p.bar = bar;
    if (d.changeBar != kNoBar && bar >= d.changeBar) {
        // The Track-change Break: eight bars into the next Track; the tempo ramps linearly bar by bar
        // (as the band's engine does), from the old Track's tempo on its first bar to the new one's on
        // its last.
        const u32 in = bar - d.changeBar;
        const u32 next = static_cast<u32>(d.track) + 1;
        if (in >= kTrackChangeBars) {
            // Past the Break (asked before the world reached it): the new Track has begun.
            DirectorState n = d;
            n.track = static_cast<u8>(next);
            n.trackBar = d.changeBar + kTrackChangeBars;
            n.changeBar = kNoBar;
            return phraseAt(n, bar);
        }
        p.track = static_cast<u8>(next);
        p.song = songOfTrack(next);
        p.cycle = 0;
        p.cycles = static_cast<u8>(cyclesOfTrack(next));
        p.section = Section::Break;
        p.sectionBar = static_cast<u16>(in);
        p.sectionBars = kTrackChangeBars;
        p.trackChange = true;
        const float from = bpmOfTrack(d.track), to = bpmOfTrack(next);
        const float u = static_cast<float>(in < kTrackChangeBars ? in : kTrackChangeBars - 1) / static_cast<float>(kTrackChangeBars - 1);
        p.bpm = from + (to - from) * u;
        p.trackBpm = to;
        return p;
    }
    const u32 form = formOfTrack(d.track);
    const u32* len = kSectionBars[form];
    const u32 cycleBars = len[0] + len[1] + len[2] + len[3];
    const u32 cycles = cyclesOfTrack(d.track);
    p.track = d.track;
    p.song = songOfTrack(d.track);
    p.cycles = static_cast<u8>(cycles);
    p.trackBpm = bpmOfTrack(d.track);
    p.bpm = p.trackBpm;
    const u32 rel = bar >= d.trackBar ? bar - d.trackBar : 0;
    u32 c = rel / cycleBars;
    if (c >= cycles) c = cycles - 1;  // past the last cycle: the boss's held Drop
    u32 off = rel - c * cycleBars;
    p.cycle = static_cast<u8>(c + 1);
    const bool last = c + 1 == cycles;
    if (off < len[0]) {
        p.section = Section::Groove;
    } else if ((off -= len[0]) < len[1]) {
        p.section = Section::Build;
    } else if ((off -= len[1]) < len[2] || last) {
        // The last cycle's Drop is the boss's: it is held while the boss lives (the band holds it too),
        // so it keeps counting past its length.
        p.section = Section::Drop;
        p.bossDrop = last;
    } else {
        off -= len[2];
        p.section = Section::Break;
    }
    p.sectionBar = static_cast<u16>(off < 0xFFFFu ? off : 0xFFFFu);
    p.sectionBars = static_cast<u16>(len[static_cast<u32>(p.section)]);
    if (p.buildLastBar()) p.bpm = p.trackBpm * kBuildLastBarSpeed;
    return p;
}

float tempoAt(const DirectorState& d, double tick) {
    const double rel = (tick - d.startTick) / kTicksPerBar;
    const u32 bar = rel > 0.0 ? static_cast<u32>(rel) : 0u;
    return phraseAt(d, bar).bpm;
}

float baseBudget(const float budget[2][8], const u32 budgetCycles[2], u32 track, u32 cycle) {
    if (cycle < 1) cycle = 1;
    if (track <= 2 && track >= 1) {
        const u32 t = track - 1;
        const u32 n = budgetCycles[t];
        if (n == 0) return 1.0f;
        return budget[t][(cycle <= n ? cycle : n) - 1];
    }
    // Past the data (the endless loop): the last value, rising a step per cycle played since.
    const float last = budgetCycles[1] ? budget[1][budgetCycles[1] - 1] : 1.0f;
    u32 cyclesSince = 0;
    for (u32 t = 3; t < track; ++t) cyclesSince += cyclesOfTrack(t);
    cyclesSince += cycle;
    return last + kEndlessBudgetStep * static_cast<float>(cyclesSince);
}

float sectionScale(const Phrase& p) {
    const float n = static_cast<float>(p.sectionBars > 1 ? p.sectionBars - 1 : 1);
    const float u = std::fmin(static_cast<float>(p.sectionBar) / n, 1.0f);
    switch (p.section) {
        case Section::Groove: return 1.0f;
        case Section::Build: return 1.0f + (kBuildScaleEnd - 1.0f) * u;
        case Section::Drop: return p.bossDrop ? kBossBudgetScale : kDropScaleStart + (kDropScaleEnd - kDropScaleStart) * u;
        case Section::Break: return 0.0f;
    }
    return 1.0f;
}

}  // namespace rf::sim
