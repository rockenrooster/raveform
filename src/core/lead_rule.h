#pragma once
// The auto-aim assist's lead rule (GAME_DESIGN 2): which steps of the player's grid it fires on. Shared by the simulation
// (World::spawnShots), the band (Band::computeShots) and the engine's lead (Engine), which must fire and play the same steps.

#include "core/types.h"

namespace rf {

// Whether the assist skips step `stepInBar` of a grid `gridTicks` long (96 ticks a beat). On the 1/16 and 1/16-triplet grids it
// fires every other step and always the off-beat's (the review's fixes, H3: the "and" of the beat is an odd step on the triplet
// grid): 2 and 4 shots a beat. On the 1/32 grid and faster it skips one step in four (the owner, 2026-11-15: on every other step
// 1/32 came to 4 a beat like the triplet grid, so Double-Time's second rank, the Drop's second Surge gear and Afterparty gave the
// assist nothing): 6 a beat, under Hyper 9 and 12. The beat's and the off-beat's steps are never skipped.
inline bool assistSkipsStep(u32 gridTicks, u32 stepInBar) {
    if (gridTicks <= 12u) return (stepInBar & 3u) == 3u;
    return (stepInBar & 1u) != 0u && (stepInBar * gridTicks) % 96u != 48u;
}

}  // namespace rf
