#include "sim/director.h"

#include <cmath>

#include "sim/boss_prose.h"
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

float bpmOfTrack(u32 track, u32 songs) {
    const u32 n = songs < 1 ? 1u : (songs > kMaxSongs ? kMaxSongs : songs);
    if (track <= 1) return kTrackBpm[0];
    if (track <= n) return kTrackBpm[track - 1];
    const float b = kTrackBpm[n - 1] + kEndlessBpmStep * static_cast<float>(track - n);
    return b < kEndlessBpmCap ? b : kEndlessBpmCap;
}

const char* songName(u32 song) {
    // GAME_DESIGN 4.2 (quoted): the ten Tracks of the set list.
    static const char* const kNames[kMaxSongs] = {"FIRST LIGHT", "SOLAR WIND", "REEF",     "AURORA", "FURNACE",
                                                  "ZENITH",      "AFTERGLOW",  "ARCADIA",  "MIRAGE", "LAST LIGHT"};
    // (M8 part 4: the bonus Tracks' songs after them, design/modes/README.md 11)
    if (song == kMaxSongs) return "MIRRORBALL";
    if (song == kMaxSongs + 1u) return "TOYBOX";
    return kNames[song % kMaxSongs];
}

Phrase phraseAt(const DirectorState& d, u32 bar) {
    Phrase p;
    p.bar = bar;
    if (d.outroBar != kNoBar && bar >= d.outroBar) {
        // M8 (one_track.md): the outro, a Break on the Track's own tempo after its boss; past its last bar the run is over.
        const u32 in = bar - d.outroBar;
        p.track = d.track;
        p.song = songOfTrack(d.track, d.songs);
        p.cycle = 0;
        p.cycles = static_cast<u8>(cyclesOfTrack(d.track));
        p.section = Section::Break;
        p.sectionBar = static_cast<u16>(in < 0xFFFFu ? in : 0xFFFFu);
        p.sectionBars = d.outroBars;
        p.outro = true;
        p.ended = in >= d.outroBars;
        p.trackBpm = p.bpm = bpmOfTrack(d.track, d.songs);
        return p;
    }
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
        p.song = songOfTrack(next, d.songs);
        p.cycle = 0;
        p.cycles = static_cast<u8>(cyclesOfTrack(next));
        p.section = Section::Break;
        p.sectionBar = static_cast<u16>(in);
        p.sectionBars = kTrackChangeBars;
        p.trackChange = true;
        const float from = bpmOfTrack(d.track, d.songs), to = bpmOfTrack(next, d.songs);
        const float u = static_cast<float>(in < kTrackChangeBars ? in : kTrackChangeBars - 1) / static_cast<float>(kTrackChangeBars - 1);
        p.bpm = from + (to - from) * u;
        p.trackBpm = to;
        return p;
    }
    if (d.jam) {
        // M8 JAM: the room's Groove in eight-bar phrases, the pressed DROP's Build and Drop, a Track pad's Break into the new Track.
        if (d.jamChangeBar != kNoBar && bar >= d.jamChangeBar + kTrackChangeBars) {
            DirectorState n = d;
            n.track = d.jamNextCrowd;
            n.bonus = d.jamNextBonus;
            n.bonusBpm = d.jamNextBonus ? d.jamNextBpm : 0.0f;
            n.jamTrack = d.jamNext;
            n.jamSong = d.jamNextSong;
            n.jamBpm = d.jamNextBpm;
            n.trackBar = d.jamChangeBar + kTrackChangeBars;
            n.jamChangeBar = kNoBar;
            return phraseAt(n, bar);
        }
        p.track = d.track;
        p.song = d.jamSong;
        p.cycle = p.cycles = 1;
        p.trackBpm = p.bpm = d.jamBpm;
        if (d.jamChangeBar != kNoBar && bar >= d.jamChangeBar) {
            const u32 in = bar - d.jamChangeBar;
            p.track = d.jamNextCrowd;
            p.song = d.jamNextSong;
            p.cycle = 0;
            p.section = Section::Break;
            p.sectionBar = static_cast<u16>(in);
            p.sectionBars = kTrackChangeBars;
            p.trackChange = true;
            const float u = static_cast<float>(in < kTrackChangeBars ? in : kTrackChangeBars - 1) / static_cast<float>(kTrackChangeBars - 1);
            p.bpm = d.jamBpm + (d.jamNextBpm - d.jamBpm) * u;
            p.trackBpm = d.jamNextBpm;
            return p;
        }
        const u32 dropLen = d.jamDropHold ? 0xFFFFu : static_cast<u32>(d.jamDropBuild) + d.jamDropLen;
        if (d.jamDropBar != kNoBar && bar >= d.jamDropBar && bar < d.jamDropBar + dropLen) {
            const u32 in = bar - d.jamDropBar;
            if (in < d.jamDropBuild) {
                p.section = Section::Build;
                p.sectionBar = static_cast<u16>(in);
                p.sectionBars = d.jamDropBuild;
            } else {
                p.section = Section::Drop;
                p.sectionBar = static_cast<u16>(in - d.jamDropBuild);
                p.sectionBars = d.jamDropLen;
            }
        } else {
            const u32 from = d.jamDropBar != kNoBar && bar >= d.jamDropBar + dropLen ? d.jamDropBar + dropLen : d.trackBar;
            p.section = Section::Groove;
            p.sectionBar = static_cast<u16>((bar >= from ? bar - from : 0u) % 8u);
            p.sectionBars = 8;
        }
        if (p.buildLastBar()) p.bpm = p.trackBpm * kBuildLastBarSpeed;
        return p;
    }
    if (d.formCount) {
        // M8 (README.md 6): a timed mode's fixed form, the Track at its second cycle (its budget and crowd), each Build and Drop a
        // wave; past the last bar the run is over (the last section's last bar, `ended`).
        const u32 off = bar >= d.trackBar ? bar - d.trackBar : 0u;
        u32 total = 0, from = 0, drops = 0, k = 0;
        for (u32 j = 0; j < d.formCount; ++j) total += d.formBars[j];
        for (; k < d.formCount; ++k) {
            if (off < from + d.formBars[k] || k == d.formHold) break;  // (a held section holds every bar from its start)
            const FormKind fk = static_cast<FormKind>(d.formKind[k]);
            if (fk == FormKind::Drop || fk == FormKind::BossDrop) ++drops;
            from += d.formBars[k];
        }
        if (k >= d.formCount) {
            k = d.formCount - 1u;
            from = total - d.formBars[k];
            p.ended = true;
            const FormKind last = static_cast<FormKind>(d.formKind[k]);
            if (drops && (last == FormKind::Drop || last == FormKind::BossDrop)) --drops;
        }
        const FormKind fk = static_cast<FormKind>(d.formKind[k]);
        p.track = d.track;
        p.song = d.bonus ? static_cast<u8>(d.bonus - 1u) : songOfTrack(d.track, d.songs);  // (M8 part 4: a bonus Track's own music)
        p.cycle = p.cycles = 2;
        p.trackBpm = p.bpm = d.bonus ? d.bonusBpm : bpmOfTrack(d.track, d.songs);
        p.section = fk == FormKind::Build ? Section::Build
                    : (fk == FormKind::Drop || fk == FormKind::BossDrop) ? Section::Drop
                    : (fk == FormKind::Break || fk == FormKind::TrackChange) ? Section::Break
                                                                               : Section::Groove;
        const u32 in = off - from;
        p.sectionBars = d.formBars[k];
        p.sectionBar = static_cast<u16>(p.ended ? (p.sectionBars ? p.sectionBars - 1u : 0u) : in);
        if (k == d.formHold && in >= p.sectionBars) {
            // (held: past its length it stays its last half, so a Drop's Surge does not come back)
            p.held = true;
            p.sectionBar = static_cast<u16>(p.sectionBars / 2u + (in - p.sectionBars) % (p.sectionBars - p.sectionBars / 2u ? p.sectionBars - p.sectionBars / 2u : 1u));
        }
        p.wave = (p.section == Section::Build || p.section == Section::Drop) ? static_cast<u8>(drops + 1u) : 0u;
        p.miniDrop = p.section == Section::Drop && p.wave == 3;  // (FULL BLAST: wave 3, THE MINI)
        p.barsLeft = static_cast<u16>(k == d.formHold ? 99u : (off < total ? total - off : 0u));
        if (p.buildLastBar()) p.bpm = p.trackBpm * kBuildLastBarSpeed;
        return p;
    }
    if (d.actForm) {
        // M8 HEADLINERS (headliners.md "The run"): an act: act 1's Groove, the Build, the boss's Drop held while it lives.
        const u32 off = bar >= d.trackBar ? bar - d.trackBar : 0u;
        const u32 groove = d.track == 1 ? d.actGroove : 0u;
        p.track = d.track;
        p.song = songOfTrack(d.track, d.songs);
        p.cycle = p.cycles = 1;
        p.trackBpm = p.bpm = bpmOfTrack(d.track, d.songs);
        if (off < groove) {
            p.section = Section::Groove;
            p.sectionBar = static_cast<u16>(off);
            p.sectionBars = static_cast<u16>(groove);
        } else if (off - groove < d.actBuild) {
            p.section = Section::Build;
            p.sectionBar = static_cast<u16>(off - groove);
            p.sectionBars = d.actBuild;
            if (p.buildLastBar()) p.bpm = p.trackBpm * kBuildLastBarSpeed;
        } else {
            const u32 in = off - groove - d.actBuild;
            p.section = Section::Drop;
            p.bossDrop = true;
            p.sectionBar = static_cast<u16>(in < 0xFFFFu ? in : 0xFFFFu);
            p.sectionBars = static_cast<u16>(kSectionBars[0][2]);
        }
        return p;
    }
    const u32 form = formOfTrack(d.track, d.songs);
    const u32* len = kSectionBars[form];
    const u32 cycleBars = len[0] + len[1] + len[2] + len[3];
    const u32 cycles = cyclesOfTrack(d.track);
    p.track = d.track;
    p.song = songOfTrack(d.track, d.songs);
    p.cycles = static_cast<u8>(cycles);
    p.trackBpm = bpmOfTrack(d.track, d.songs);
    p.bpm = p.trackBpm;
    // M5 part 1 (minibosses.md): from Track 2 on the first cycle's Drop (c2f113a: every Track is two cycles; it was the middle
    // one's) brings a mini-boss and holds while it lives, at most kMiniHoldBars past its length (open-ended up to the cap while it
    // lives; then the bars it was held). The second cycle's Drop is the boss's.
    const bool minis = d.track >= 2 && cycles >= 2;
    const u32 mid = 0;
    const u32 maxHold = static_cast<u32>(prose::kMiniHoldBars);
    const u32 hold = !minis ? 0u : (d.midHolding ? maxHold : (d.midHold < maxHold ? d.midHold : maxHold));
    u32 off = bar >= d.trackBar ? bar - d.trackBar : 0;
    u32 c = 0;
    for (;;) {
        const u32 cl = cycleBars + (c == mid ? hold : 0u);
        if (c + 1 >= cycles || off < cl || c + 1 == d.holdDropCycle) break;  // past the last cycle: the boss's held Drop
        off -= cl;
        ++c;
    }
    p.cycle = static_cast<u8>(c + 1);
    const bool last = c + 1 == cycles;
    const u32 dropLen = len[2] + (c == mid ? hold : 0u);
    if (off < len[0]) {
        p.section = Section::Groove;
    } else if ((off -= len[0]) < len[1]) {
        p.section = Section::Build;
    } else if ((off -= len[1]) < dropLen || last || c + 1 == d.holdDropCycle) {  // (M7: a benchmark's held Drop too)
        // The last cycle's Drop is the boss's: it is held while the boss lives (the band holds it too),
        // so it keeps counting past its length. The middle one's is the mini-boss's (M5), held the same way.
        p.section = Section::Drop;
        p.bossDrop = last;
        p.miniDrop = minis && c == mid;
        p.held = p.miniDrop && off >= len[2];
    } else {
        off -= dropLen;
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

float baseBudget(const SpeciesTable& s, u32 track, u32 cycle, u32 songs) {
    // species.json firstSlice.threatBudgetPerBar by Track and cycle, for the set list's Tracks (M5: up to ten); past the
    // set list (the loop) the last Track's last value, rising a step per cycle played since (the loop multiplies on top,
    // World). With three Tracks in the set list this is the first slice's rule, unchanged.
    if (cycle < 1) cycle = 1;
    const u32 data = songs < s.budgetTracks ? songs : s.budgetTracks;
    if (data == 0) return 1.0f;
    if (track >= 1 && track <= data) {
        const u32 t = track - 1;
        const u32 n = s.budgetCycles[t];
        if (n == 0) return 1.0f;
        return s.budget[t][(cycle <= n ? cycle : n) - 1] * kTrackEnemyScale[track < 11 ? track : 0];
    }
    const u32 nl = s.budgetCycles[data - 1];
    const float last = nl ? s.budget[data - 1][nl - 1] : 1.0f;
    u32 cyclesSince = 0;
    for (u32 t = data + 1; t < track; ++t) cyclesSince += cyclesOfTrack(t);
    cyclesSince += cycle;
    return last + kEndlessBudgetStep * static_cast<float>(cyclesSince);
}

float crowdTarget(const SpeciesTable::Crowd& c, u32 track, u32 cycle, u32 songs) {
    // firstSlice.crowd.aliveTarget likewise: the set list's Tracks from the data, the loop a step a Track past its last.
    if (cycle < 1) cycle = 1;
    const u32 data = songs < c.targetTracks ? songs : c.targetTracks;
    float v = 0.0f;
    if (data == 0) return 0.0f;
    if (track >= 1 && track <= data) {
        const u32 t = track - 1;
        const u32 n = c.targetCycles[t];
        v = n ? c.target[t][(cycle <= n ? cycle : n) - 1] * kTrackEnemyScale[track < 11 ? track : 0] : 0.0f;
    } else {
        const u32 nl = c.targetCycles[data - 1];
        v = (nl ? c.target[data - 1][nl - 1] : 0.0f) + c.loopStep * static_cast<float>(track - data);
    }
    return v < c.max ? v : c.max;
}
float crowdScale(const SpeciesTable::Crowd& c, const Phrase& p) {
    const float n = static_cast<float>(p.sectionBars > 1 ? p.sectionBars - 1 : 1);
    const float u = std::fmin(static_cast<float>(p.sectionBar) / n, 1.0f);
    switch (p.section) {
        case Section::Groove: return c.groove;
        case Section::Build: return c.build[0] + (c.build[1] - c.build[0]) * u;
        case Section::Drop:
            // The boss's Drop: species.json's share (Kaleidos; the Choir's page: "the crowd drops to half"), Longwave's page
            // its own ("the director's budget drops to 30% and only fodder families spawn").
            // Event Horizon's (horizon.md Numbers: "fodder only, and only along the lanes"): the director's crowd is off; the
            // Well's own stream walks the lanes (world_horizon.cpp).
            if (p.bossDrop) {
                const BossKind k = bossOfSong(p.song);
                if (k == BossKind::EventHorizon) return 0.0f;
                return k == BossKind::Longwave ? prose::kLwCrowdShare : c.bossDrop;
            }
            return (c.drop[0] + (c.drop[1] - c.drop[0]) * u) * (p.miniDrop ? c.miniDrop : 1.0f);
        case Section::Break: return c.breakScale;
    }
    return 1.0f;
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
