#pragma once
// iOS glue (docs/IOS.md): what UIKit gives that SDL does not. Compiled only in the iOS build (ios_glue.mm, Objective-C++).

#include <cstddef>

#include "core/platform.h"

#if RF_IOS
namespace rf::ios {

// The haptics of GAME_DESIGN 11.4 (audio/platform_audio.h HapticKind: 0 light, 1 medium, 2 heavy), as UIImpactFeedbackGenerator
// taps; `strong` raises each one step (heavy stays heavy, at full intensity).
void haptic(int kind, bool strong);
// The device's model identifier ("iPhone15,2"), for the log and the benchmark's result.
void deviceModel(char* out, size_t capacity);
// The app's Documents folder with a trailing slash (shared with Finder and the Files app: screenshots, benchmark results).
void documentsPath(char* out, size_t capacity);

}  // namespace rf::ios
#endif
