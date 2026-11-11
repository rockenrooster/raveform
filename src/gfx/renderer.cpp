#include "gfx/renderer.h"

#include <cstddef>

#include "core/png.h"
#include "core/time.h"

namespace rf {

namespace {

const u32 kCapsuleVert[] =
#include "capsule.vert.inc"
    ;
const u32 kCapsuleFrag[] =
#include "capsule.frag.inc"
    ;

// Layout shared with the push_constant block in shaders/capsule.*.
struct PushConstants {
    float view[4];  // logical width, height, 2/width, 2/height
    float rot[4];   // pre-rotation rows (r00, r01, r10, r11)
    float misc[4];  // x: encode sRGB in the shader, y: time
};
static_assert(sizeof(PushConstants) <= 128, "push constants must fit the guaranteed 128 bytes");

// Clip-space rotation that pre-rotates logical landscape content by `deg` clockwise (y down).
void rotationMatrix(int deg, float out[4]) {
    switch (deg) {
        case 90: out[0] = 0; out[1] = -1; out[2] = 1; out[3] = 0; break;
        case 180: out[0] = -1; out[1] = 0; out[2] = 0; out[3] = -1; break;
        case 270: out[0] = 0; out[1] = 1; out[2] = -1; out[3] = 0; break;
        default: out[0] = 1; out[1] = 0; out[2] = 0; out[3] = 1; break;
    }
}

VkShaderModule makeShader(VkDevice device, const u32* code, size_t bytes) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = bytes;
    info.pCode = code;
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) return VK_NULL_HANDLE;
    return module;
}

}  // namespace

bool Renderer::init(SDL_Window* window, const RendererDesc& desc) {
    window_ = window;
    maxCapsules_ = desc.maxCapsules;
    pacer_.configure(desc.swappy, desc.targetHz);
    if (!gpu_.createInstance(desc.validation)) return false;
    if (!swapchain_.createSurface(gpu_, window)) {
        RF_ERR("renderer: no window surface");
        return false;
    }
    if (!gpu_.createDevice(swapchain_.surface, pacer_)) return false;

    for (Frame& f : frames_) {
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool.queueFamilyIndex = gpu_.queueFamily;
        RF_VK_TRY(vkCreateCommandPool(gpu_.device, &pool, nullptr, &f.pool));
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool = f.pool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;
        RF_VK_TRY(vkAllocateCommandBuffers(gpu_.device, &alloc, &f.cmd));
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        RF_VK_TRY(vkCreateFence(gpu_.device, &fence, nullptr, &f.fence));
        VkSemaphoreCreateInfo sem{};
        sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        RF_VK_TRY(vkCreateSemaphore(gpu_.device, &sem, nullptr, &f.acquired));
    }

    // One persistently mapped buffer, one region per frame in flight: no staging, no per-frame allocation.
    frameBytes_ = VkDeviceSize(maxCapsules_) * sizeof(CapsuleInstance);
    if (!gpu_.createBuffer(frameBytes_ * kFramesInFlight, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instances_))
        return false;
    RF_LOG("renderer: instance buffer %u KB x %u frames (%s)", static_cast<u32>(frameBytes_ / 1024), kFramesInFlight,
           instances_.deviceLocal ? "device local" : "system memory");

    if (!swapchain_.create(gpu_, pacer_, window_)) {
        RF_ERR("renderer: could not create the first swapchain");
        return false;
    }
    if (!createRenderPasses() || !createPipeline() || !createImageResources()) return false;
    return true;
}

void Renderer::shutdown() {
    if (!gpu_.device) {
        gpu_.destroy(pacer_);
        return;
    }
    vkDeviceWaitIdle(gpu_.device);
    destroyImageResources();
    swapchain_.destroy(gpu_, pacer_);
    swapchain_.destroySurface(gpu_);
    if (capture_.buffer.buffer) gpu_.destroyBuffer(capture_.buffer);
    gpu_.destroyBuffer(instances_);
    if (pipeline_) vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    if (layout_) vkDestroyPipelineLayout(gpu_.device, layout_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    destroyRenderPasses();
    for (Frame& f : frames_) {
        if (f.acquired) vkDestroySemaphore(gpu_.device, f.acquired, nullptr);
        if (f.fence) vkDestroyFence(gpu_.device, f.fence, nullptr);
        if (f.pool) vkDestroyCommandPool(gpu_.device, f.pool, nullptr);
        f = Frame{};
    }
    gpu_.destroy(pacer_);
}

bool Renderer::createRenderPasses() {
    for (int i = 0; i < 2; ++i) {
        VkAttachmentDescription color{};
        color.format = swapchain_.format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = i == 0 ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &ref;

        // Identical dependencies in both passes keep them compatible (only the final layout differs).
        VkSubpassDependency deps[2]{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;  // after the acquire semaphore wait
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].srcAccessMask = 0;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass = 0;  // before a screenshot copy
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &color;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 2;
        info.pDependencies = deps;
        RF_VK_TRY(vkCreateRenderPass(gpu_.device, &info, nullptr, i == 0 ? &passPresent_ : &passCapture_));
    }
    return true;
}

void Renderer::destroyRenderPasses() {
    if (passPresent_) vkDestroyRenderPass(gpu_.device, passPresent_, nullptr);
    if (passCapture_) vkDestroyRenderPass(gpu_.device, passCapture_, nullptr);
    passPresent_ = passCapture_ = VK_NULL_HANDLE;
}

bool Renderer::createPipeline() {
    if (!layout_) {
        VkPushConstantRange range{};
        range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        range.offset = 0;
        range.size = sizeof(PushConstants);
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &range;
        RF_VK_TRY(vkCreatePipelineLayout(gpu_.device, &layoutInfo, nullptr, &layout_));
    }

    VkShaderModule vert = makeShader(gpu_.device, kCapsuleVert, sizeof kCapsuleVert);
    VkShaderModule frag = makeShader(gpu_.device, kCapsuleFrag, sizeof kCapsuleFrag);
    if (!vert || !frag) {
        RF_ERR("renderer: shader module creation failed");
        if (vert) vkDestroyShaderModule(gpu_.device, vert, nullptr);
        if (frag) vkDestroyShaderModule(gpu_.device, frag, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{0, sizeof(CapsuleInstance), VK_VERTEX_INPUT_RATE_INSTANCE};
    VkVertexInputAttributeDescription attributes[4] = {
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<u32>(offsetof(CapsuleInstance, p0x))},
        {1, 0, VK_FORMAT_R16G16_SFLOAT, static_cast<u32>(offsetof(CapsuleInstance, radius))},
        {2, 0, VK_FORMAT_R8G8B8A8_UNORM, static_cast<u32>(offsetof(CapsuleInstance, color))},
        {3, 0, VK_FORMAT_R32_UINT, static_cast<u32>(offsetof(CapsuleInstance, flags))},
    };
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 4;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // The scene is additive light: order never matters (TECH_SPEC 5.1).
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                           VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blend;

    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pColorBlendState = &colorBlend;
    info.pDynamicState = &dynamic;
    info.layout = layout_;
    info.renderPass = passPresent_;
    info.subpass = 0;
    const VkResult r = vkCreateGraphicsPipelines(gpu_.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_);
    vkDestroyShaderModule(gpu_.device, vert, nullptr);
    vkDestroyShaderModule(gpu_.device, frag, nullptr);
    if (r != VK_SUCCESS) {
        RF_ERR("vkCreateGraphicsPipelines: %s", vkResultName(r));
        return false;
    }
    return true;
}

bool Renderer::createImageResources() {
    for (u32 i = 0; i < swapchain_.imageCount; ++i) {
        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = passPresent_;
        fb.attachmentCount = 1;
        fb.pAttachments = &swapchain_.views[i];
        fb.width = swapchain_.extent.width;
        fb.height = swapchain_.extent.height;
        fb.layers = 1;
        RF_VK_TRY(vkCreateFramebuffer(gpu_.device, &fb, nullptr, &framebuffers_[i]));
        // One render-done semaphore per image: a present may still wait on the previous one.
        VkSemaphoreCreateInfo sem{};
        sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        RF_VK_TRY(vkCreateSemaphore(gpu_.device, &sem, nullptr, &renderDone_[i]));
    }
    return true;
}

void Renderer::destroyImageResources() {
    for (u32 i = 0; i < Swapchain::kMaxImages; ++i) {
        if (framebuffers_[i]) vkDestroyFramebuffer(gpu_.device, framebuffers_[i], nullptr);
        if (renderDone_[i]) vkDestroySemaphore(gpu_.device, renderDone_[i], nullptr);
        framebuffers_[i] = VK_NULL_HANDLE;
        renderDone_[i] = VK_NULL_HANDLE;
    }
}

bool Renderer::rebuildSwapchain() {
    vkDeviceWaitIdle(gpu_.device);
    destroyImageResources();
    const VkFormat oldFormat = swapchain_.format;
    swapchain_.destroy(gpu_, pacer_);
    if (!swapchain_.create(gpu_, pacer_, window_)) return false;  // minimized or lost: retry later
    if (swapchain_.format != oldFormat) {
        RF_LOG("renderer: swapchain format changed, rebuilding passes and pipeline");
        vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
        destroyRenderPasses();
        if (!createRenderPasses() || !createPipeline()) return false;
    }
    if (!createImageResources()) return false;
    rebuildPending_ = false;
    ++rebuilds_;
    return true;
}

void Renderer::releaseSurface() {
    if (!gpu_.device) return;
    vkDeviceWaitIdle(gpu_.device);
    destroyImageResources();
    swapchain_.destroy(gpu_, pacer_);
    swapchain_.destroySurface(gpu_);
    capture_.pending = false;
}

bool Renderer::restoreSurface() {
    if (!swapchain_.hasSurface() && !swapchain_.createSurface(gpu_, window_)) return false;
    return rebuildSwapchain();
}

bool Renderer::beginFrame(CapsuleBatch& batch) {
    if (surfaceLost_) {
        RF_WARN("renderer: surface lost, recreating");
        releaseSurface();
        surfaceLost_ = false;
    }
    if (!swapchain_.hasSurface() && !restoreSurface()) return false;
    if ((rebuildPending_ || !swapchain_.valid()) && !rebuildSwapchain()) return false;

    Frame& f = frames_[frameIndex_];
    const u64 t0 = nowNs();
    VkResult r = vkWaitForFences(gpu_.device, 1, &f.fence, VK_TRUE, UINT64_MAX);
    const u64 t1 = nowNs();
    if (r != VK_SUCCESS) {
        RF_ERR("vkWaitForFences: %s", vkResultName(r));
        return false;
    }
    r = vkAcquireNextImageKHR(gpu_.device, swapchain_.handle, UINT64_MAX, f.acquired, VK_NULL_HANDLE, &f.imageIndex);
    timings_.fenceWaitNs = t1 - t0;
    timings_.acquireNs = nowNs() - t1;
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        rebuildPending_ = true;
        return false;
    }
    if (r == VK_ERROR_SURFACE_LOST_KHR) {
        surfaceLost_ = true;
        return false;
    }
    if (r == VK_SUBOPTIMAL_KHR) {
        rebuildPending_ = true;  // still render and present this image, rebuild afterwards
    } else if (r != VK_SUCCESS) {
        RF_ERR("vkAcquireNextImageKHR: %s", vkResultName(r));
        return false;
    }
    vkResetFences(gpu_.device, 1, &f.fence);  // only once we know this frame will be submitted
    batch.begin(reinterpret_cast<CapsuleInstance*>(static_cast<u8*>(instances_.mapped) + frameIndex_ * frameBytes_),
                maxCapsules_);
    return true;
}

bool Renderer::ensureCaptureBuffer(VkDeviceSize bytes) {
    if (capture_.buffer.buffer && capture_.buffer.size >= bytes) return true;
    if (capture_.buffer.buffer) gpu_.destroyBuffer(capture_.buffer);
    return gpu_.createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             capture_.buffer);
}

bool Renderer::endFrame(const CapsuleBatch& batch, ClearColor clear, float timeSec, bool capture) {
    Frame& f = frames_[frameIndex_];
    const VkExtent2D extent = swapchain_.extent;
    const bool doCapture = capture && swapchain_.canCapture &&
                           ensureCaptureBuffer(VkDeviceSize(extent.width) * extent.height * 4);
    if (capture && !doCapture) RF_WARN("renderer: this swapchain cannot be captured");

    vkResetCommandPool(gpu_.device, f.pool, 0);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd, &begin);

    VkClearValue clearValue{};
    clearValue.color.float32[0] = clear.r;
    clearValue.color.float32[1] = clear.g;
    clearValue.color.float32[2] = clear.b;
    clearValue.color.float32[3] = 1.0f;
    VkRenderPassBeginInfo pass{};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass.renderPass = doCapture ? passCapture_ : passPresent_;
    pass.framebuffer = framebuffers_[f.imageIndex];
    pass.renderArea.extent = extent;
    pass.clearValueCount = 1;
    pass.pClearValues = &clearValue;
    vkCmdBeginRenderPass(f.cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport viewport{0.0f, 0.0f, float(extent.width), float(extent.height), 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(f.cmd, 0, 1, &viewport);
    vkCmdSetScissor(f.cmd, 0, 1, &scissor);

    if (batch.count() > 0) {
        const float w = float(swapchain_.logical.width);
        const float h = float(swapchain_.logical.height);
        PushConstants pc{};
        pc.view[0] = w;
        pc.view[1] = h;
        pc.view[2] = 2.0f / w;
        pc.view[3] = 2.0f / h;
        rotationMatrix(swapchain_.rotationDeg, pc.rot);
        pc.misc[0] = swapchain_.srgbFormat ? 0.0f : 1.0f;
        pc.misc[1] = timeSec;
        vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdPushConstants(f.cmd, layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof pc, &pc);
        const VkDeviceSize offset = frameIndex_ * frameBytes_;
        vkCmdBindVertexBuffers(f.cmd, 0, 1, &instances_.buffer, &offset);
        vkCmdDraw(f.cmd, 4, batch.count(), 0, 0);
    }
    vkCmdEndRenderPass(f.cmd);

    if (doCapture) {
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {extent.width, extent.height, 1};
        vkCmdCopyImageToBuffer(f.cmd, swapchain_.images[f.imageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               capture_.buffer.buffer, 1, &region);

        VkImageMemoryBarrier toPresent{};
        toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toPresent.srcAccessMask = 0;  // the copy only read the image
        toPresent.dstAccessMask = 0;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image = swapchain_.images[f.imageIndex];
        toPresent.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkBufferMemoryBarrier toHost{};
        toHost.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        toHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        toHost.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toHost.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toHost.buffer = capture_.buffer.buffer;
        toHost.offset = 0;
        toHost.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1,
                             &toHost, 1, &toPresent);
    }
    vkEndCommandBuffer(f.cmd);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &f.acquired;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &f.cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderDone_[f.imageIndex];
    VkResult r = vkQueueSubmit(gpu_.queue, 1, &submit, f.fence);
    if (r != VK_SUCCESS) {
        RF_ERR("vkQueueSubmit: %s", vkResultName(r));
        return false;
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderDone_[f.imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_.handle;
    present.pImageIndices = &f.imageIndex;
    const u64 t0 = nowNs();
    r = pacer_.present(gpu_.queue, present);
    timings_.presentNs = nowNs() - t0;
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        rebuildPending_ = true;
    } else if (r == VK_ERROR_SURFACE_LOST_KHR) {
        surfaceLost_ = true;
    } else if (r != VK_SUCCESS) {
        RF_ERR("present: %s", vkResultName(r));
    }

    if (doCapture) {
        capture_.pending = true;
        capture_.slot = frameIndex_;
        capture_.extent = extent;
        capture_.rotationDeg = swapchain_.rotationDeg;
        capture_.format = swapchain_.format;
    }
    frameIndex_ = (frameIndex_ + 1) % kFramesInFlight;
    return true;
}

bool Renderer::saveCapture(const char* path) {
    if (!capture_.pending) {
        RF_ERR("renderer: no captured frame to save");
        return false;
    }
    capture_.pending = false;
    vkWaitForFences(gpu_.device, 1, &frames_[capture_.slot].fence, VK_TRUE, UINT64_MAX);
    gpu_.invalidate(capture_.buffer);
    const PixelLayout layout = (capture_.format == VK_FORMAT_B8G8R8A8_SRGB || capture_.format == VK_FORMAT_B8G8R8A8_UNORM)
                                   ? PixelLayout::BGRA8
                                   : PixelLayout::RGBA8;
    const bool ok = writePngUpright(path, static_cast<const u8*>(capture_.buffer.mapped), capture_.extent.width,
                                    capture_.extent.height, layout, capture_.rotationDeg);
    if (ok) RF_LOG("screenshot: %s (%ux%u image, rotation %d undone)", path, capture_.extent.width,
                   capture_.extent.height, capture_.rotationDeg);
    return ok;
}

}  // namespace rf
