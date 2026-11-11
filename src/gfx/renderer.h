#pragma once
// M0 renderer: straight to the swapchain (the HDR chain is M1). Clear, then one instanced draw of
// additive capsules from a persistently mapped per-frame streaming buffer. Two frames in flight.

#include "core/types.h"
#include "gfx/capsule.h"
#include "gfx/frame_pacer.h"
#include "gfx/gpu_context.h"
#include "gfx/swapchain.h"

struct SDL_Window;

namespace rf {

struct RendererDesc {
    bool validation = false;
    bool swappy = true;
    float targetHz = 0.0f;
    u32 maxCapsules = 32768;  // per frame
};

struct FrameTimings {
    u64 fenceWaitNs = 0;  // waiting for the frame slot (GPU two frames back)
    u64 acquireNs = 0;    // vkAcquireNextImageKHR
    u64 presentNs = 0;    // inside the present call (Swappy may wait here)
};

struct ClearColor {
    float r = 0.0f, g = 0.0f, b = 0.0f;  // linear
};

class Renderer {
public:
    static constexpr u32 kFramesInFlight = 2;

    bool init(SDL_Window* window, const RendererDesc& desc);
    void shutdown();

    // Lifecycle: Android background drops the swapchain and the surface; foreground brings them back.
    void releaseSurface();
    bool restoreSurface();  // false while no native window exists yet: call again later
    bool hasSurface() const { return swapchain_.hasSurface(); }
    bool hasSwapchain() const { return swapchain_.valid(); }
    void requestRebuild() { rebuildPending_ = true; }

    // False: skip this frame (no surface, minimized, swapchain out of date).
    bool beginFrame(CapsuleBatch& batch);
    // Records, submits and presents. With `capture`, the image is also copied for saveCapture().
    bool endFrame(const CapsuleBatch& batch, ClearColor clear, float timeSec, bool capture);
    // Waits for the captured frame and writes an upright PNG.
    bool saveCapture(const char* path);

    const GpuContext& gpu() const { return gpu_; }
    const Swapchain& swapchain() const { return swapchain_; }
    FramePacer& pacer() { return pacer_; }
    const FramePacer& pacer() const { return pacer_; }
    const FrameTimings& timings() const { return timings_; }
    u32 rebuildCount() const { return rebuilds_; }
    u32 maxCapsules() const { return maxCapsules_; }

private:
    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE;
        u32 imageIndex = 0;
    };

    bool createRenderPasses();
    void destroyRenderPasses();
    bool createPipeline();
    bool createImageResources();  // framebuffers + per-image semaphores
    void destroyImageResources();
    bool rebuildSwapchain();
    bool ensureCaptureBuffer(VkDeviceSize bytes);

    SDL_Window* window_ = nullptr;
    GpuContext gpu_;
    FramePacer pacer_;
    Swapchain swapchain_;
    VkRenderPass passPresent_ = VK_NULL_HANDLE;  // final layout PRESENT_SRC
    VkRenderPass passCapture_ = VK_NULL_HANDLE;  // identical but final layout TRANSFER_SRC
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffers_[Swapchain::kMaxImages] = {};
    VkSemaphore renderDone_[Swapchain::kMaxImages] = {};
    Frame frames_[kFramesInFlight];
    u32 frameIndex_ = 0;
    GpuBuffer instances_;
    VkDeviceSize frameBytes_ = 0;
    u32 maxCapsules_ = 0;
    bool rebuildPending_ = false;
    bool surfaceLost_ = false;
    u32 rebuilds_ = 0;
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
