#pragma once
// Monotonic wall clock for the main thread (frame pacing and measurement).
// The audio callback uses std::chrono::steady_clock directly so audio code does not depend on SDL.

#include <SDL3/SDL_timer.h>

#include "core/types.h"

namespace rf {

inline u64 nowNs() { return SDL_GetTicksNS(); }
inline double nsToMs(u64 ns) { return static_cast<double>(ns) * 1e-6; }
inline double nsToSec(u64 ns) { return static_cast<double>(ns) * 1e-9; }

}  // namespace rf
