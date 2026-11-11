#pragma once
// M0 test content (a placeholder until the M1 renderer): a rotating capsule figure, touch markers
// with a high-contrast pressed state, the tone pulse and a frame-interval graph.

#include "core/math.h"
#include "core/stats.h"
#include "core/types.h"
#include "gfx/capsule.h"

namespace rf {

class VectorFont;

struct TouchPoint {
    u64 id = 0;
    Vec2 pos;
    bool active = false;
};

using FrameHistory = RecentSamples<240>;

struct SceneInput {
    float width = 0.0f;   // logical pixels
    float height = 0.0f;
    float scale = 1.0f;   // reference pixels: height / 1080
    float time = 0.0f;    // seconds
    Vec2 insetMin;        // safe-area insets, pixels
    Vec2 insetMax;
    const TouchPoint* touches = nullptr;
    u32 touchCount = 0;
    const FrameHistory* intervals = nullptr;
    float targetMs = 16.667f;
    float pulseAge = 1e9f;  // seconds since the last tone trigger
    Vec2 pulseAt;
    bool toneHold = false;
};

void drawM0Scene(CapsuleBatch& batch, const VectorFont& font, const SceneInput& in);

}  // namespace rf
