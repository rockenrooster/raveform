#pragma once
// M1 renderer (TECH_SPEC 5.2, design/look.md §3, §7, §9). Per frame:
//   scene      W x H      additive HDR: capsules, instanced glyphs, stateless sparks, sparkles (soft
//                         light under 16 ref px), glass triangles
//   pools      W/4        additive HDR: soft light of 16 ref px and up (never full-resolution fill)
//   afterglow  W/2 or W/4 history = max(scene, previous x decay) re-aimed by the camera; the excess
//   distortion W/8 or W/4 displacement from shock rings and gravity wells
//   bloom      W/2 ... W/64: one blur-and-halve pass per level, then accumulation back up to W/2
//   composite  (swapchain): scene(uv + distortion) + pools + bloom + afterglow + background ->
//              vignette -> exposure -> tone map, then the threat layer (rims, then cores) and the HUD,
//              then sRGB with dither.
// Only the scene and the composite are full resolution. Two frames in flight; one persistently mapped
// buffer holds every frame's dynamic instances, one the stateless spark ring, one the static glyph
// meshes, one the replayed scene file (if any).

#include "core/types.h"
#include "gfx/capsule.h"
#include "gfx/frame_pacer.h"
#include "gfx/gpu_context.h"
#include "gfx/swapchain.h"

struct SDL_Window;

namespace rf {

struct Look;
struct LookVariant;
class GlyphSet;

struct RendererDesc {
    bool validation = false;
    bool swappy = true;
    float targetHz = 0.0f;
    u32 maxCapsules = 65536;  // per frame, dynamic
    u32 maxSparkles = 8192;
    u32 maxPools = 4096;  // (1,024 until every enemy bullet got a pooled glow, 2026-11-14)
    u32 maxTriVertices = 12288;  // (6,144 until the Reprise's death glow, 2026-11-14: up to 3,700 of its own at once)
    u32 maxThreat = 16384;    // rims and cores each
    u32 maxHud = 16384;       // (M7: the credits screen's licence text, a screenful at cap 10)
    u32 maxGlyphs = 12288;    // scene glyph instances per frame
    u32 maxThreatGlyphs = 4096;  // threat-layer glyph instances per frame, rims and cores each
    u32 particleRing = 131072;   // stateless sparks (TECH_SPEC 5.4: the Ultra ring)
};

// Effect-tier switches the renderer owns (TECH_SPEC 5.4, design/look.md §10).
struct RenderTier {
    u32 bloomLevels = 6;      // 4..6, coarsest dropped first, weights renormalised
    u32 afterglowShift = 1;   // 0 off, 1 half resolution, 2 quarter
    u32 distortionShift = 3;  // 0 off, 3 = W/8, 2 = W/4
    bool aberration = false;
    u32 backgroundShift = 2;  // 2 = W/4, 1 = W/2 (Ultra)
};

// GPU pass times from timestamp queries (TECH_SPEC 9), milliseconds, two frames late.
struct GpuPassTimes {
    // Total is the work (every pass); Wait is the gap before the swapchain pass (image not yet free).
    enum Pass : u32 { Scene, Pools, Background, Bloom, Afterglow, Distortion, Composite, Wait, Total, Count };
    float ms[Count] = {};
    bool valid = false;
    u64 frame = 0;  // renderer frame these times belong to
};

// Instanced glyphs of one frame (TECH_SPEC 5.3). Instances are staged and grouped by shape when the
// frame is recorded: one instanced draw per shape (and per hull-only range) in use.
class GlyphBatch {
public:
    void begin(GlyphInstanceGpu* staging, u16* keys, u32 capacity) {
        mem_ = staging;
        keys_ = keys;
        cap_ = capacity;
        count_ = 0;
        dropped_ = 0;
    }
    // `hullOnly`: draw layer 0 only (white copies, the low-detail LOD).
    GlyphInstanceGpu* push(u32 glyph, bool hullOnly) {
        if (count_ >= cap_) {
            ++dropped_;
            return nullptr;
        }
        keys_[count_] = static_cast<u16>(glyph * 2u + (hullOnly ? 1u : 0u));
        return mem_ + count_++;
    }
    u32 count() const { return count_; }
    u32 dropped() const { return dropped_; }
    const GlyphInstanceGpu* data() const { return mem_; }
    const u16* keys() const { return keys_; }

private:
    GlyphInstanceGpu* mem_ = nullptr;
    u16* keys_ = nullptr;
    u32 cap_ = 0;
    u32 count_ = 0;
    u32 dropped_ = 0;
};

// The stateless spark ring (TECH_SPEC 5.3): sparks are written once, at birth, straight into GPU
// memory that persists across frames. A slot is reused only after every frame that can still draw it
// has finished.
class ParticleRing {
public:
    ParticleGpu* spawn() {
        if (!mem_ || count_ >= free_) {
            ++dropped_;
            return nullptr;
        }
        ParticleGpu* p = mem_ + head_;
        head_ = head_ + 1 == cap_ ? 0 : head_ + 1;
        ++count_;
        return p;
    }
    u32 spawned() const { return count_; }
    u32 dropped() const { return dropped_; }

private:
    friend class Renderer;
    ParticleGpu* mem_ = nullptr;
    u32 cap_ = 0;
    u32 head_ = 0;
    u32 free_ = 0;   // slots that may be written this frame
    u32 count_ = 0;  // written this frame
    u32 dropped_ = 0;
};

struct FrameTimings {
    u64 fenceWaitNs = 0;  // waiting for the frame slot (GPU two frames back)
    u64 acquireNs = 0;    // vkAcquireNextImageKHR
    u64 presentNs = 0;    // inside the present call (Swappy may wait here)
    // Phase boundaries of the last frame (nowNs()), for the benchmark's per-frame attribution.
    u64 fenceStart = 0;
    u64 fenceEnd = 0;
    u64 acquireEnd = 0;
    u64 beginEnd = 0;
    u64 recordStart = 0;
    u64 recordEnd = 0;
    u64 submitEnd = 0;
    u64 presentStart = 0;
    u64 presentEnd = 0;
};

// What one frame draws. Capsule, glow and triangle batches are in logical pixels (content transform =
// identity); glyphs and sparks are in world units (FrameParams::world).
struct FrameDraw {
    CapsuleBatch scene;        // additive HDR lines
    GlowBatch sparkles;        // soft light under 16 ref px, full resolution
    GlowBatch pools;           // soft light of 16 ref px and up, quarter resolution
    TriBatch glass;            // flat additive triangles
    GlyphBatch glyphs;         // instanced glyphs, additive HDR
    ParticleRing* particles = nullptr;  // stateless sparks born this frame
    CapsuleBatch threatRims;   // after tone mapping, black, premultiplied over (drawn first)
    CapsuleBatch threatCores;  // after tone mapping, display-linear colour, premultiplied over
    GlyphBatch threatGlyphRims;   // priority layer (look.md §9): every rim, ships' and threats' (kGlyphFlat)
    GlyphBatch priorityShipCores; // then the ships' cores
    GlyphBatch threatGlyphCores;  // then the threats' cores
    CapsuleBatch hud;          // after tone mapping, additive (dev overlay)
};

// World-to-logical-pixel transform: logical = world * scale + offset.
struct WorldView {
    float scale = 1.0f;
    float offsetX = 0.0f, offsetY = 0.0f;
};

// Background (look.md §7, §9): a radial gradient plus soft clouds, added after the bloom.
struct Background {
    Vec3 edge;
    Vec3 centre;
    float cx = 0.5f, cy = 0.5f, radius = 0.8f, power = 1.5f;
    struct Cloud {
        float x, y, radius;
        Vec3 colour;
    } clouds[8];
    u32 cloudCount = 0;
};

// Screen-space distortion sources (look.md §7), logical pixels. Drawn into a low-resolution
// displacement buffer; the composite samples the scene through it.
struct DistortionRing {
    float x = 0.0f, y = 0.0f;  // centre
    float radius = 0.0f;       // where the Gaussian profile peaks
    float width = 0.0f;        // profile width (the look's "ring width")
    float amount = 0.0f;       // peak displacement, pixels (outward)
};
struct DistortionWell {
    float x = 0.0f, y = 0.0f;
    float peakRadius = 0.0f;  // strongest pull here
    float goneRadius = 0.0f;  // nothing beyond
    float amount = 0.0f;      // peak displacement, pixels (inward)
};

// Composite controls for one frame. Stills leave every time-driven field at its default.
struct FrameParams {
    const LookVariant* look = nullptr;  // required
    bool bloom = true;                  // false: no bloom passes, strength 0 (acceptance "bloom off")
    float bloomPump = 1.0f;             // beat and event multipliers on the bloom strength
    float exposurePump = 1.0f;
    bool vignette = false;              // time-driven effects are off in stills
    float vignetteAmount = -1.0f;       // < 0: the look's amount; the Build bar tightens it
    bool dither = true;
    u32 frameIndex = 0;                 // dither seed
    float dtSeconds = 0.0f;             // frame time (afterglow decay)
    // Afterglow (look.md §7): re-aimed by the camera. The previous frame's logical pixel p is at
    // p * cameraScale + cameraShift this frame.
    bool afterglow = false;
    float afterglowGain = 1.0f;  // look.md v2.2: x the light budget's swarm gain (look.json afterglow.gain)
    float cameraScale = 1.0f;
    float cameraShiftX = 0.0f, cameraShiftY = 0.0f;
    // Distortion and chromatic aberration (look.md §7).
    DistortionRing rings[8];
    u32 ringCount = 0;
    DistortionWell wells[4];
    u32 wellCount = 0;
    bool aberration = false;
    float aberrationSnarePx = 0.0f;  // + look.md snare kick, logical pixels
    float drain = 0.0f;              // Build bar: scene colours drained toward white (0..1)
    // World content (glyphs, sparks): the scene's view includes camera shake, the threat layer's does not.
    WorldView world;
    WorldView worldThreat;
    Vec3 accent;                     // palette accent (glyph ornaments)
    float enemySwell = 1.0f;         // look.md §7: enemy glyph scale x (1 + kickSwell x kick)
    float particleTime = 0.0f;       // stage clock, seconds (spark birth times are on it)
    float particleMaxLife = 2.0f;    // no spark lives longer
    Vec2 dimAt;                      // GAME_DESIGN 11.3: sparks near the local player are dimmed
    float dimRadius = 0.0f;          // world units (0: off)
    float dimFactor = 1.0f;
    // Profiling: N extra full-screen draws before the timed passes, so the GPU is saturated and runs at
    // its highest clock; the timed passes then show their cost at that clock (not shown, not timed).
    u32 gpuBurn = 0;
};

// A replayed scene file (design/look.md §9): content pixels on a canvas, uploaded once.
struct StaticScene {
    float canvasW = 2400.0f, canvasH = 1080.0f;
    const CapsuleGpu* capsules = nullptr;
    u32 capsuleCount = 0;
    const GlowGpu* sparkles = nullptr;
    u32 sparkleCount = 0;
    const GlowGpu* pools = nullptr;
    u32 poolCount = 0;
    const TriVertexGpu* triVertices = nullptr;
    u32 triVertexCount = 0;
    const CapsuleGpu* threatRims = nullptr;
    const CapsuleGpu* threatCores = nullptr;
    u32 threatCount = 0;
    Background background;
    bool hasBackground = false;
};

class Renderer {
public:
    static constexpr u32 kFramesInFlight = 2;
    static constexpr u32 kMaxBloomLevels = 6;

    bool init(SDL_Window* window, const RendererDesc& desc, const Look& look);
    void shutdown();

    void releaseSurface();
    bool restoreSurface();
    bool hasSurface() const { return swapchain_.hasSurface(); }
    bool hasSwapchain() const { return swapchain_.valid(); }
    void requestRebuild() { rebuildPending_ = true; }
    // TECH_SPEC 5.2 (M6): the VIVID look (Display-P3) where the surface offers it; a change rebuilds the swapchain.
    void setWideColour(bool on) {
        if (on == swapchain_.wantP3) return;
        swapchain_.wantP3 = on;
        rebuildPending_ = true;
    }
    bool wideColourOffered() const { return swapchain_.p3Offered; }
    bool wideColourOn() const { return swapchain_.colorSpace == VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT; }

    // Uploads a replayed scene (once, at load). Drawn every frame before the dynamic batches.
    bool setStaticScene(const StaticScene& scene);
    // Uploads the glyph meshes (once, at load); `glyphs` must outlive the renderer.
    bool setGlyphs(const GlyphSet& glyphs);
    // The background used when no static scene provides one (palette gradient and clouds).
    void setBackground(const Background& bg) { background_ = bg; }
    // Effect tier (waits for the GPU and rebuilds targets when a resolution changes).
    bool setTier(const RenderTier& tier);
    const RenderTier& tier() const { return tier_; }
    const GpuPassTimes& gpuTimes() const { return gpuTimes_; }
    u64 frameCount() const { return frameCounter_; }  // frames recorded so far (GpuPassTimes::frame counts the same)
    // Forgets every live spark (waits for the GPU): for a stage clock that starts again (start-up only).
    void resetParticles();
    bool gpuTimersAvailable() const { return timestampPool_ != VK_NULL_HANDLE; }
    u32 lastGlyphCount() const { return lastGlyphs_; }
    u32 liveParticles() const { return lastLiveParticles_; }

    // False: skip this frame (no surface, minimized, swapchain out of date).
    bool beginFrame(FrameDraw& draw);
    bool endFrame(const FrameDraw& draw, const FrameParams& params, bool capture);
    // (audit 2026-11-15) The GPU device is lost or a submit failed: nothing more can be drawn (a submit that failed left its
    // frame's fence unsignalled, and the next wait on it would hang). The app closes instead of freezing.
    bool fatal() const { return fatal_; }
    bool saveCapture(const char* path);

    const GpuContext& gpu() const { return gpu_; }
    const Swapchain& swapchain() const { return swapchain_; }
    FramePacer& pacer() { return pacer_; }
    const FramePacer& pacer() const { return pacer_; }
    const FrameTimings& timings() const { return timings_; }
    u32 rebuildCount() const { return rebuilds_; }
    const char* hdrFormatName() const { return hdrFormatName_; }
    u32 lastCapsuleCount() const { return lastCapsules_; }

private:
    struct Target {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkExtent2D size{};
    };
    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;   // offscreen passes: start at once
        VkCommandBuffer post = VK_NULL_HANDLE;  // swapchain pass: waits for the acquired image
        VkFence fence = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE;
        u32 imageIndex = 0;
    };
    struct Region {  // per-frame slice of the dynamic buffer
        VkDeviceSize offset = 0;
        u32 capacity = 0;
    };

    bool createRenderPasses();
    void destroyRenderPasses();
    bool createPipelines();
    void destroyPipelines();
    bool createSamplersAndLayouts();
    bool createTargets(VkExtent2D logical);
    void destroyTargets();
    bool createTarget(Target& t, VkExtent2D size) { return createTarget(t, size, hdrFormat_, passHdr_); }
    // `pass` == VK_NULL_HANDLE: no framebuffer (the caller makes one).
    bool createTarget(Target& t, VkExtent2D size, VkFormat format, VkRenderPass pass);
    bool createSwapchainResources();
    void destroySwapchainResources();
    bool rebuildSwapchain();
    bool ensureCaptureBuffer(VkDeviceSize bytes);
    void updateDescriptors();
    void solveBloom();
    bool createTimestamps();
    void readTimestamps(u32 slot);
    // Sorts one glyph batch by shape into mapped memory and records the draw ranges.
    struct GlyphRun {
        u32 key = 0;
        u32 first = 0;
        u32 count = 0;
    };
    u32 sortGlyphs(const GlyphBatch& batch, GlyphInstanceGpu* dst, GlyphRun* runs, u32 maxRuns);
    void drawGlyphRuns(VkCommandBuffer cmd, VkDeviceSize instanceOffset, const GlyphRun* runs, u32 runCount);

    SDL_Window* window_ = nullptr;
    const Look* look_ = nullptr;
    RendererDesc desc_;
    GpuContext gpu_;
    FramePacer pacer_;
    Swapchain swapchain_;
    VkFormat hdrFormat_ = VK_FORMAT_UNDEFINED;
    const char* hdrFormatName_ = "";

    VkRenderPass passHdr_ = VK_NULL_HANDLE;      // offscreen HDR targets
    VkRenderPass passPresent_ = VK_NULL_HANDLE;  // swapchain, final layout PRESENT_SRC
    VkRenderPass passCapture_ = VK_NULL_HANDLE;  // identical, final layout TRANSFER_SRC
    VkPipelineLayout geoLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout postLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout postSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkSampler linearBorder_ = VK_NULL_HANDLE;
    VkSampler linearEdge_ = VK_NULL_HANDLE;
    VkPipeline capsuleAdd_ = VK_NULL_HANDLE;   // scene (HDR pass)
    VkPipeline glowAdd_ = VK_NULL_HANDLE;      // scene and pools (HDR pass)
    VkPipeline triAdd_ = VK_NULL_HANDLE;       // scene (HDR pass)
    VkPipeline bloomDown_ = VK_NULL_HANDLE;    // HDR pass
    VkPipeline bloomUp_ = VK_NULL_HANDLE;      // HDR pass
    VkPipeline composite_ = VK_NULL_HANDLE;    // swapchain pass
    VkPipeline threat_ = VK_NULL_HANDLE;       // swapchain pass, premultiplied over
    VkPipeline hudAdd_ = VK_NULL_HANDLE;       // swapchain pass, additive
    VkPipeline glyphAdd_ = VK_NULL_HANDLE;     // scene (HDR pass)
    VkPipeline glyphThreat_ = VK_NULL_HANDLE;  // swapchain pass, premultiplied over
    VkPipeline particleAdd_ = VK_NULL_HANDLE;  // scene (HDR pass)
    VkPipeline afterglow_ = VK_NULL_HANDLE;    // afterglow pass (two targets)
    VkPipeline distort_ = VK_NULL_HANDLE;      // distortion pass
    VkPipeline backgroundPass_ = VK_NULL_HANDLE;  // background pass (HDR)
    VkPipeline bloomFinal_ = VK_NULL_HANDLE;      // last bloom level, gathers the low-resolution terms
    VkRenderPass passAfterglow_ = VK_NULL_HANDLE;
    VkRenderPass passDistort_ = VK_NULL_HANDLE;

    Target scene_;
    Target pools_;
    Target down_[kMaxBloomLevels + 1];  // index 1..6
    Target up_[kMaxBloomLevels + 1];    // index 1..5
    Target agHistory_[2];               // afterglow history, ping-pong by frame parity
    Target agExcess_;                   // what the composite adds
    VkFramebuffer agFramebuffers_[2] = {};
    Target distortion_;                 // R16G16_SFLOAT displacement
    Target bgTarget_;                   // gradient and clouds at W/4 (W/2 on Ultra)
    static constexpr u32 kSetBindings = 7;
    u32 bloomLevels_ = 0;
    float tapOffset_[kMaxBloomLevels + 1] = {};  // in source texels, per level
    VkDescriptorSet downSets_[kMaxBloomLevels + 1] = {};
    VkDescriptorSet upSets_[kMaxBloomLevels + 1] = {};       // coarse = up_{k+1}
    VkDescriptorSet upFirstSets_[kMaxBloomLevels + 1] = {};  // index L: coarse = down_L, level = down_{L-1}
    VkDescriptorSet compositeSets_[kFramesInFlight] = {};
    VkDescriptorSet afterglowSets_[2] = {};
    VkDescriptorSet finalSets_[2] = {};  // bloom_final: coarse = up_2, or down_2 with two levels
    VkDescriptorSet distortSets_[kFramesInFlight] = {};
    VkExtent2D targetsFor_{};
    RenderTier tier_;
    u32 agParity_ = 0;

    // Glyphs: static meshes and per-frame instances (three batches: scene, threat rims, threat cores).
    const GlyphSet* glyphSet_ = nullptr;
    GpuBuffer glyphMesh_;  // GlyphVertexGpu x 4 per segment, then u16 indices x 6 per segment
    VkDeviceSize glyphIndexOffset_ = 0;
    // Batches: 0 scene, 1 priority rims, 2 threat cores, 3 priority ship cores.
    static constexpr u32 kGlyphBatches = 4;
    GlyphInstanceGpu* glyphStaging_[kGlyphBatches] = {};
    u16* glyphKeys_[kGlyphBatches] = {};
    static constexpr u32 kMaxGlyphRuns = 512;
    GlyphRun glyphRuns_[kGlyphBatches][kMaxGlyphRuns];
    u32 glyphRunCount_[kGlyphBatches] = {};
    u32 afterglowLevel_ = 3;  // the bloom level the afterglow reads (look.json afterglow.sourceSigmaRefPx)

    // Stateless sparks.
    GpuBuffer particles_;
    ParticleRing ring_;
    struct SpawnMark {  // where one frame's sparks start in the ring, and when they were born
        u32 head = 0;
        float time = 0.0f;
        u64 total = 0;  // sparks written before this frame
    };
    static constexpr u32 kSpawnMarks = 1024;
    SpawnMark spawnMarks_[kSpawnMarks];
    u32 markFirst_ = 0, markCount_ = 0;
    u32 frameStartHead_ = 0;
    u64 spawnedTotal_ = 0;
    float particleClock_ = 0.0f;  // the latest frame's particle time
    float particleLife_ = 2.0f;
    u32 lastLiveParticles_ = 0;
    u32 lastGlyphs_ = 0;

    // GPU timers.
    // 0 start, 1 scene, 2 pools, 3 background, 4 bloom down, 5 afterglow, 6 bloom up, 7 distortion
    // (first command buffer), 8 start of the swapchain pass (after the image wait), 9 end.
    static constexpr u32 kTimestamps = 10;
    VkQueryPool timestampPool_ = VK_NULL_HANDLE;
    bool timestampsWritten_[kFramesInFlight] = {};
    u64 frameCounter_ = 0;
    u64 timestampFrame_[kFramesInFlight] = {};
    double timestampNs_ = 1.0;
    u64 timestampMask_ = ~0ull;
    GpuPassTimes gpuTimes_;

    VkFramebuffer framebuffers_[Swapchain::kMaxImages] = {};
    VkSemaphore renderDone_[Swapchain::kMaxImages] = {};
    Frame frames_[kFramesInFlight];
    u32 frameIndex_ = 0;

    GpuBuffer dynamic_;   // every frame's batches, one slice per frame in flight
    VkDeviceSize frameBytes_ = 0;
    Region regScene_, regSparkles_, regPools_, regTris_, regRims_, regCores_, regHud_, regGlyphs_, regThreatGlyphRims_,
        regThreatGlyphCores_, regPriorityShips_;
    GpuBuffer uniforms_;  // composite constants, one slot per frame in flight
    VkDeviceSize uniformStride_ = 0;
    GpuBuffer static_;    // the replayed scene file
    StaticScene staticInfo_;
    VkDeviceSize staticCaps_ = 0, staticSparkles_ = 0, staticPools_ = 0, staticTris_ = 0, staticRims_ = 0,
                 staticCores_ = 0;
    Background background_;

    bool rebuildPending_ = false;
    bool surfaceLost_ = false;
    bool fatal_ = false;
    u32 rebuilds_ = 0;
    u32 lastCapsules_ = 0;
    FrameTimings timings_;

    struct Capture {
        GpuBuffer buffer;
        bool pending = false;
        u32 slot = 0;
        VkExtent2D extent{};
        int rotationDeg = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
    } capture_;
};

}  // namespace rf
