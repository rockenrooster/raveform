#pragma once
// Scene files (design/look.md §9): flat lists of the six engine primitives that the look-dev tool
// recorded, replayed so the renderer can be compared with design/previews/*_ref.png. Loaded once into
// malloc'd arrays and uploaded to the GPU; never touched in the frame.

#include "core/types.h"
#include "gfx/renderer.h"

namespace rf {

struct RfScene {
    RfScene() = default;
    RfScene(const RfScene&) = delete;
    RfScene& operator=(const RfScene&) = delete;
    ~RfScene();

    StaticScene view;          // what the renderer uploads (points into the arrays below)
    float lookBloom = 0.0f;    // informative values from the file
    float lookExposure = 1.0f;
    CapsuleGpu* capsules = nullptr;
    GlowGpu* sparkles = nullptr;
    GlowGpu* pools = nullptr;
    TriVertexGpu* tris = nullptr;
    CapsuleGpu* rims = nullptr;
    CapsuleGpu* cores = nullptr;
};

// Glows with a radius of `poolMinRadius` canvas pixels and up go to the pool target.
bool loadRfScene(const char* path, float poolMinRadius, RfScene& out);

}  // namespace rf
