#include "gfx/renderer.h"

#include <cmath>
#include <cstddef>
#include <cstring>

#include <cstdlib>

#include "content/glyphs.h"
#include "content/look.h"
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
const u32 kGlowVert[] =
#include "glow.vert.inc"
    ;
const u32 kGlowFrag[] =
#include "glow.frag.inc"
    ;
const u32 kTriVert[] =
#include "tri.vert.inc"
    ;
const u32 kTriFrag[] =
#include "tri.frag.inc"
    ;
const u32 kFullscreenVert[] =
#include "fullscreen.vert.inc"
    ;
const u32 kBloomDownFrag[] =
#include "bloom_down.frag.inc"
    ;
const u32 kBloomUpFrag[] =
#include "bloom_up.frag.inc"
    ;
const u32 kCompositeFrag[] =
#include "composite.frag.inc"
    ;
const u32 kGlyphVert[] =
#include "glyph.vert.inc"
    ;
const u32 kParticleVert[] =
#include "particle.vert.inc"
    ;
const u32 kAfterglowFrag[] =
#include "afterglow.frag.inc"
    ;
const u32 kDistortFrag[] =
#include "distort.frag.inc"
    ;
const u32 kBackgroundFrag[] =
#include "background.frag.inc"
    ;
const u32 kBloomFinalFrag[] =
#include "bloom_final.frag.inc"
    ;

struct GeoPush {
    float view[4];
    float rot[4];
    float xform[4];
    float misc[4];
    float extra0[4];
    float extra1[4];
};
struct PostPush {
    float rot[4];
    float p0[4];
    float p1[4];
    float p2[4];
};
static_assert(sizeof(GeoPush) <= 128 && sizeof(PostPush) <= 128, "push constants must fit 128 bytes");

// std140 mirror of the Composite block in shaders/composite.frag.
struct CompositeUbo {
    float view[4];
    float bgEdge[4];
    float bgCentre[4];
    float bgShape[4];
    float canvas[4];
    float tone[4];
    float tone2[4];
    float vignette[4];
    float nebCount[4];
    float neb[16][4];
    float post[4];
    float post2[4];
    float dcount[4];
    float ring[16][4];
    float well[8][4];
    float grade[4];  // saturation, vibrance, contrast, on (look.md §3 Grade)
};

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

VkDeviceSize alignUp(VkDeviceSize v, VkDeviceSize a) { return (v + a - 1) / a * a; }

enum class Blend { None, Add, Over };
enum class Input { None, Capsule, Glow, TriVertex, Glyph, Particle };

struct PipelineSpec {
    const u32* vert;
    size_t vertBytes;
    const u32* frag;
    size_t fragBytes;
    Input input;
    VkPrimitiveTopology topology;
    Blend blend;
    VkRenderPass pass;
    VkPipelineLayout layout;
    u32 colorAttachments = 1;
};

VkPipeline makePipeline(VkDevice device, const PipelineSpec& s) {
    VkShaderModule vert = makeShader(device, s.vert, s.vertBytes);
    VkShaderModule frag = makeShader(device, s.frag, s.fragBytes);
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vert && frag) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = "main";

        VkVertexInputBindingDescription bindings[2]{};
        VkVertexInputBindingDescription& binding = bindings[0];
        u32 bindingCount = 1;
        VkVertexInputAttributeDescription attrs[7]{};
        u32 attrCount = 0;
        switch (s.input) {
            case Input::Glyph:
                bindings[0] = {0, sizeof(GlyphVertexGpu), VK_VERTEX_INPUT_RATE_VERTEX};
                bindings[1] = {1, sizeof(GlyphInstanceGpu), VK_VERTEX_INPUT_RATE_INSTANCE};
                bindingCount = 2;
                attrs[0] = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<u32>(offsetof(GlyphVertexGpu, x0))};
                attrs[1] = {1, 0, VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<u32>(offsetof(GlyphVertexGpu, info))};
                attrs[2] = {2, 1, VK_FORMAT_R32G32_SFLOAT, static_cast<u32>(offsetof(GlyphInstanceGpu, x))};
                attrs[3] = {3, 1, VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<u32>(offsetof(GlyphInstanceGpu, rot))};
                attrs[4] = {4, 1, VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<u32>(offsetof(GlyphInstanceGpu, shape))};
                attrs[5] = {5, 1, VK_FORMAT_R8G8B8A8_UNORM, static_cast<u32>(offsetof(GlyphInstanceGpu, a))};
                attrs[6] = {6, 1, VK_FORMAT_R8G8B8A8_UNORM, static_cast<u32>(offsetof(GlyphInstanceGpu, b))};
                attrCount = 7;
                break;
            case Input::Particle:
                binding = {0, sizeof(ParticleGpu), VK_VERTEX_INPUT_RATE_INSTANCE};
                attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<u32>(offsetof(ParticleGpu, x))};
                attrs[1] = {1, 0, VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<u32>(offsetof(ParticleGpu, motion))};
                attrs[2] = {2, 0, VK_FORMAT_R16G16_SFLOAT, static_cast<u32>(offsetof(ParticleGpu, light))};
                attrs[3] = {3, 0, VK_FORMAT_R8G8B8A8_UNORM, static_cast<u32>(offsetof(ParticleGpu, colour))};
                attrCount = 4;
                break;
            case Input::Capsule:
                binding = {0, sizeof(CapsuleGpu), VK_VERTEX_INPUT_RATE_INSTANCE};
                attrs[0] = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<u32>(offsetof(CapsuleGpu, p0x))};
                attrs[1] = {1, 0, VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<u32>(offsetof(CapsuleGpu, c0))};
                attrs[2] = {2, 0, VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<u32>(offsetof(CapsuleGpu, c1))};
                attrCount = 3;
                break;
            case Input::Glow:
                binding = {0, sizeof(GlowGpu), VK_VERTEX_INPUT_RATE_INSTANCE};
                attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<u32>(offsetof(GlowGpu, x))};
                attrs[1] = {1, 0, VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<u32>(offsetof(GlowGpu, rgb))};
                attrCount = 2;
                break;
            case Input::TriVertex:
                binding = {0, sizeof(TriVertexGpu), VK_VERTEX_INPUT_RATE_VERTEX};
                attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<u32>(offsetof(TriVertexGpu, x))};
                attrs[1] = {1, 0, VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<u32>(offsetof(TriVertexGpu, rgb))};
                attrCount = 2;
                break;
            case Input::None:
                break;
        }
        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = attrCount ? bindingCount : 0u;
        vertexInput.pVertexBindingDescriptions = bindings;
        vertexInput.vertexAttributeDescriptionCount = attrCount;
        vertexInput.pVertexAttributeDescriptions = attrs;

        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = s.topology;
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
        VkPipelineColorBlendAttachmentState blends[2]{};
        VkPipelineColorBlendAttachmentState& blend = blends[0];
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                               VK_COLOR_COMPONENT_A_BIT;
        if (s.blend == Blend::Add) {
            blend.blendEnable = VK_TRUE;
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.colorBlendOp = VK_BLEND_OP_ADD;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.alphaBlendOp = VK_BLEND_OP_ADD;
        } else if (s.blend == Blend::Over) {
            // Premultiplied colour over the target. A fixed-point target clamps the source to [0, 1]
            // before blending and the result after it: exactly mix-then-clip for colours above 1.
            blend.blendEnable = VK_TRUE;
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorBlendOp = VK_BLEND_OP_ADD;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        blends[1] = blends[0];
        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = s.colorAttachments;
        colorBlend.pAttachments = blends;
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
        info.layout = s.layout;
        info.renderPass = s.pass;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
            pipeline = VK_NULL_HANDLE;
    }
    if (vert) vkDestroyShaderModule(device, vert, nullptr);
    if (frag) vkDestroyShaderModule(device, frag, nullptr);
    return pipeline;
}

void setViewport(VkCommandBuffer cmd, VkExtent2D e) {
    const VkViewport vp{0.0f, 0.0f, float(e.width), float(e.height), 0.0f, 1.0f};
    const VkRect2D sc{{0, 0}, e};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
}

// Variance of the 4-tap blur-and-halve kernel along one axis, taps at +-a source texels from a texel
// corner, each a bilinear fetch: a^2 for the tap spread plus f(1 - f) for the bilinear weights.
double tapVariance(double a) {
    const double f = (a + 0.5) - std::floor(a + 0.5);
    return a * a + f * (1.0 - f);
}

}  // namespace

bool Renderer::init(SDL_Window* window, const RendererDesc& desc, const Look& look) {
    window_ = window;
    desc_ = desc;
    look_ = &look;
    pacer_.configure(desc.swappy, desc.targetHz);
    if (!gpu_.createInstance(desc.validation)) return false;
    if (!swapchain_.createSurface(gpu_, window)) {
        RF_ERR("renderer: no window surface");
        return false;
    }
    if (!gpu_.createDevice(swapchain_.surface, pacer_)) return false;

    // HDR targets (TECH_SPEC 5.2): B10G11R11 when it can be rendered to, blended and filtered.
    const VkFormatFeatureFlags need = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    VkFormatProperties fp;
    vkGetPhysicalDeviceFormatProperties(gpu_.physical, VK_FORMAT_B10G11R11_UFLOAT_PACK32, &fp);
    if ((fp.optimalTilingFeatures & need) == need) {
        hdrFormat_ = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        hdrFormatName_ = "B10G11R11";
    } else {
        hdrFormat_ = VK_FORMAT_R16G16B16A16_SFLOAT;
        hdrFormatName_ = "R16G16B16A16";
    }

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
        RF_VK_TRY(vkAllocateCommandBuffers(gpu_.device, &alloc, &f.post));
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        RF_VK_TRY(vkCreateFence(gpu_.device, &fence, nullptr, &f.fence));
        VkSemaphoreCreateInfo sem{};
        sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        RF_VK_TRY(vkCreateSemaphore(gpu_.device, &sem, nullptr, &f.acquired));
    }

    // One persistently mapped buffer: per frame in flight, one region per batch.
    VkDeviceSize at = 0;
    const auto region = [&at](Region& r, u32 count, size_t stride) {
        r.offset = at;
        r.capacity = count;
        at = alignUp(at + VkDeviceSize(count) * stride, 256);
    };
    region(regScene_, desc.maxCapsules, sizeof(CapsuleGpu));
    region(regSparkles_, desc.maxSparkles, sizeof(GlowGpu));
    region(regPools_, desc.maxPools, sizeof(GlowGpu));
    region(regTris_, desc.maxTriVertices, sizeof(TriVertexGpu));
    region(regRims_, desc.maxThreat, sizeof(CapsuleGpu));
    region(regCores_, desc.maxThreat, sizeof(CapsuleGpu));
    region(regHud_, desc.maxHud, sizeof(CapsuleGpu));
    region(regGlyphs_, desc.maxGlyphs, sizeof(GlyphInstanceGpu));
    region(regThreatGlyphRims_, desc.maxThreatGlyphs, sizeof(GlyphInstanceGpu));
    region(regThreatGlyphCores_, desc.maxThreatGlyphs, sizeof(GlyphInstanceGpu));
    region(regPriorityShips_, 64, sizeof(GlyphInstanceGpu));
    frameBytes_ = at;
    if (!gpu_.createBuffer(frameBytes_ * kFramesInFlight, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dynamic_))
        return false;
    // Glyph instances are staged on the CPU and grouped by shape when the frame is recorded.
    const u32 glyphCaps[kGlyphBatches] = {desc.maxGlyphs, desc.maxThreatGlyphs, desc.maxThreatGlyphs, 64};
    for (u32 i = 0; i < kGlyphBatches; ++i) {
        glyphStaging_[i] = static_cast<GlyphInstanceGpu*>(std::malloc(sizeof(GlyphInstanceGpu) * glyphCaps[i]));
        glyphKeys_[i] = static_cast<u16*>(std::malloc(sizeof(u16) * glyphCaps[i]));
        if (!glyphStaging_[i] || !glyphKeys_[i]) return false;
    }
    // The stateless spark ring persists across frames.
    if (!gpu_.createBuffer(VkDeviceSize(desc.particleRing) * sizeof(ParticleGpu), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, particles_))
        return false;
    std::memset(particles_.mapped, 0, VkDeviceSize(desc.particleRing) * sizeof(ParticleGpu));
    ring_.mem_ = static_cast<ParticleGpu*>(particles_.mapped);
    ring_.cap_ = desc.particleRing;
    createTimestamps();
    uniformStride_ = alignUp(sizeof(CompositeUbo), gpu_.props.limits.minUniformBufferOffsetAlignment);
    if (!gpu_.createBuffer(uniformStride_ * kFramesInFlight, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, uniforms_))
        return false;
    RF_LOG("renderer: dynamic buffer %u KB x %u frames (%s), HDR targets %s", static_cast<u32>(frameBytes_ / 1024),
           kFramesInFlight, dynamic_.deviceLocal ? "device local" : "system memory", hdrFormatName_);

    if (!swapchain_.create(gpu_, pacer_, window_)) {
        RF_ERR("renderer: could not create the first swapchain");
        return false;
    }
    if (!createRenderPasses() || !createSamplersAndLayouts() || !createPipelines() || !createSwapchainResources())
        return false;
    if (!createTargets(swapchain_.logical)) return false;
    return true;
}

void Renderer::shutdown() {
    if (!gpu_.device) {
        gpu_.destroy(pacer_);
        return;
    }
    vkDeviceWaitIdle(gpu_.device);
    destroySwapchainResources();
    destroyTargets();
    swapchain_.destroy(gpu_, pacer_);
    swapchain_.destroySurface(gpu_);
    if (capture_.buffer.buffer) gpu_.destroyBuffer(capture_.buffer);
    if (static_.buffer) gpu_.destroyBuffer(static_);
    if (glyphMesh_.buffer) gpu_.destroyBuffer(glyphMesh_);
    if (particles_.buffer) gpu_.destroyBuffer(particles_);
    if (timestampPool_) vkDestroyQueryPool(gpu_.device, timestampPool_, nullptr);
    timestampPool_ = VK_NULL_HANDLE;
    for (u32 i = 0; i < kGlyphBatches; ++i) {
        std::free(glyphStaging_[i]);
        std::free(glyphKeys_[i]);
        glyphStaging_[i] = nullptr;
        glyphKeys_[i] = nullptr;
    }
    gpu_.destroyBuffer(uniforms_);
    gpu_.destroyBuffer(dynamic_);
    destroyPipelines();
    if (descriptorPool_) vkDestroyDescriptorPool(gpu_.device, descriptorPool_, nullptr);
    if (postSetLayout_) vkDestroyDescriptorSetLayout(gpu_.device, postSetLayout_, nullptr);
    if (geoLayout_) vkDestroyPipelineLayout(gpu_.device, geoLayout_, nullptr);
    if (postLayout_) vkDestroyPipelineLayout(gpu_.device, postLayout_, nullptr);
    if (linearBorder_) vkDestroySampler(gpu_.device, linearBorder_, nullptr);
    if (linearEdge_) vkDestroySampler(gpu_.device, linearEdge_, nullptr);
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
    // Offscreen HDR pass: cleared, then sampled by the next pass (also by the next frame's passes,
    // hence the write-after-read dependency on the way in).
    {
        VkAttachmentDescription color{};
        color.format = hdrFormat_;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &ref;
        VkSubpassDependency deps[2]{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &color;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 2;
        info.pDependencies = deps;
        RF_VK_TRY(vkCreateRenderPass(gpu_.device, &info, nullptr, &passHdr_));

        // The afterglow pass writes two HDR targets (history, excess); the distortion pass one RG16F.
        VkAttachmentDescription two[2] = {color, color};
        VkAttachmentReference refs[2] = {{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                                         {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
        two[0].loadOp = two[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // every pixel is written
        subpass.colorAttachmentCount = 2;
        subpass.pColorAttachments = refs;
        info.attachmentCount = 2;
        info.pAttachments = two;
        RF_VK_TRY(vkCreateRenderPass(gpu_.device, &info, nullptr, &passAfterglow_));
        VkAttachmentDescription rg = color;
        rg.format = VK_FORMAT_R16G16_SFLOAT;
        rg.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &ref;
        info.attachmentCount = 1;
        info.pAttachments = &rg;
        RF_VK_TRY(vkCreateRenderPass(gpu_.device, &info, nullptr, &passDistort_));
    }
    // Swapchain passes: the composite covers every pixel, so nothing is loaded. Identical dependencies
    // keep the present and capture variants compatible (only the final layout differs).
    for (int i = 0; i < 2; ++i) {
        VkAttachmentDescription color{};
        color.format = swapchain_.format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
        VkSubpassDependency deps[2]{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].srcAccessMask = 0;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass = 0;
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
    if (passHdr_) vkDestroyRenderPass(gpu_.device, passHdr_, nullptr);
    if (passPresent_) vkDestroyRenderPass(gpu_.device, passPresent_, nullptr);
    if (passCapture_) vkDestroyRenderPass(gpu_.device, passCapture_, nullptr);
    if (passAfterglow_) vkDestroyRenderPass(gpu_.device, passAfterglow_, nullptr);
    if (passDistort_) vkDestroyRenderPass(gpu_.device, passDistort_, nullptr);
    passHdr_ = passPresent_ = passCapture_ = passAfterglow_ = passDistort_ = VK_NULL_HANDLE;
}

bool Renderer::createSamplersAndLayouts() {
    VkSamplerCreateInfo s{};
    s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    s.magFilter = VK_FILTER_LINEAR;
    s.minFilter = VK_FILTER_LINEAR;
    s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    s.addressModeU = s.addressModeV = s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    s.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;  // the blur sees black beyond the edge
    s.maxLod = 0.0f;
    RF_VK_TRY(vkCreateSampler(gpu_.device, &s, nullptr, &linearBorder_));
    s.addressModeU = s.addressModeV = s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    RF_VK_TRY(vkCreateSampler(gpu_.device, &s, nullptr, &linearEdge_));

    // Bindings 0-2 and 4-6: images; 3: the composite uniform block.
    VkDescriptorSetLayoutBinding b[kSetBindings]{};
    for (u32 i = 0; i < kSetBindings; ++i) {
        b[i].binding = i;
        b[i].descriptorType = i == 3 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = kSetBindings;
    li.pBindings = b;
    RF_VK_TRY(vkCreateDescriptorSetLayout(gpu_.device, &li, nullptr, &postSetLayout_));

    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GeoPush)};
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &range;
    RF_VK_TRY(vkCreatePipelineLayout(gpu_.device, &pl, nullptr, &geoLayout_));
    range.size = sizeof(PostPush);
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &postSetLayout_;
    RF_VK_TRY(vkCreatePipelineLayout(gpu_.device, &pl, nullptr, &postLayout_));

    constexpr u32 kLevelSets = kMaxBloomLevels + 1;
    constexpr u32 kSets = 3 * kLevelSets + 2 * kFramesInFlight + 2 + 2;
    VkDescriptorPoolSize sizes[2] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kSets * (kSetBindings - 1)},
                                     {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kSets}};
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets = kSets;
    pi.poolSizeCount = 2;
    pi.pPoolSizes = sizes;
    RF_VK_TRY(vkCreateDescriptorPool(gpu_.device, &pi, nullptr, &descriptorPool_));
    VkDescriptorSetLayout layouts[kSets];
    for (VkDescriptorSetLayout& l : layouts) l = postSetLayout_;
    VkDescriptorSet all[kSets];
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = descriptorPool_;
    ai.descriptorSetCount = kSets;
    ai.pSetLayouts = layouts;
    RF_VK_TRY(vkAllocateDescriptorSets(gpu_.device, &ai, all));
    u32 n = 0;
    for (u32 i = 0; i < kLevelSets; ++i) downSets_[i] = all[n++];
    for (u32 i = 0; i < kLevelSets; ++i) upSets_[i] = all[n++];
    for (u32 i = 0; i < kLevelSets; ++i) upFirstSets_[i] = all[n++];
    for (u32 i = 0; i < kFramesInFlight; ++i) compositeSets_[i] = all[n++];
    for (u32 i = 0; i < kFramesInFlight; ++i) distortSets_[i] = all[n++];
    afterglowSets_[0] = all[n++];
    afterglowSets_[1] = all[n++];
    finalSets_[0] = all[n++];  // coarse = up_2
    finalSets_[1] = all[n++];  // coarse = down_2 (two bloom levels)
    return true;
}

bool Renderer::createPipelines() {
    const VkDevice d = gpu_.device;
    capsuleAdd_ = makePipeline(d, {kCapsuleVert, sizeof kCapsuleVert, kCapsuleFrag, sizeof kCapsuleFrag, Input::Capsule,
                                   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, Blend::Add, passHdr_, geoLayout_});
    glowAdd_ = makePipeline(d, {kGlowVert, sizeof kGlowVert, kGlowFrag, sizeof kGlowFrag, Input::Glow,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, Blend::Add, passHdr_, geoLayout_});
    triAdd_ = makePipeline(d, {kTriVert, sizeof kTriVert, kTriFrag, sizeof kTriFrag, Input::TriVertex,
                               VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::Add, passHdr_, geoLayout_});
    bloomDown_ = makePipeline(d, {kFullscreenVert, sizeof kFullscreenVert, kBloomDownFrag, sizeof kBloomDownFrag,
                                  Input::None, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::None, passHdr_, postLayout_});
    bloomUp_ = makePipeline(d, {kFullscreenVert, sizeof kFullscreenVert, kBloomUpFrag, sizeof kBloomUpFrag, Input::None,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::None, passHdr_, postLayout_});
    composite_ = makePipeline(d, {kFullscreenVert, sizeof kFullscreenVert, kCompositeFrag, sizeof kCompositeFrag,
                                  Input::None, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::None, passPresent_,
                                  postLayout_});
    threat_ = makePipeline(d, {kCapsuleVert, sizeof kCapsuleVert, kCapsuleFrag, sizeof kCapsuleFrag, Input::Capsule,
                               VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, Blend::Over, passPresent_, geoLayout_});
    hudAdd_ = makePipeline(d, {kCapsuleVert, sizeof kCapsuleVert, kCapsuleFrag, sizeof kCapsuleFrag, Input::Capsule,
                               VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, Blend::Add, passPresent_, geoLayout_});
    glyphAdd_ = makePipeline(d, {kGlyphVert, sizeof kGlyphVert, kCapsuleFrag, sizeof kCapsuleFrag, Input::Glyph,
                                 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::Add, passHdr_, geoLayout_});
    glyphThreat_ = makePipeline(d, {kGlyphVert, sizeof kGlyphVert, kCapsuleFrag, sizeof kCapsuleFrag, Input::Glyph,
                                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::Over, passPresent_, geoLayout_});
    particleAdd_ = makePipeline(d, {kParticleVert, sizeof kParticleVert, kCapsuleFrag, sizeof kCapsuleFrag,
                                    Input::Particle, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, Blend::Add, passHdr_, geoLayout_});
    afterglow_ = makePipeline(d, {kFullscreenVert, sizeof kFullscreenVert, kAfterglowFrag, sizeof kAfterglowFrag,
                                  Input::None, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::None, passAfterglow_,
                                  postLayout_, 2});
    distort_ = makePipeline(d, {kFullscreenVert, sizeof kFullscreenVert, kDistortFrag, sizeof kDistortFrag, Input::None,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::None, passDistort_, postLayout_});
    backgroundPass_ = makePipeline(d, {kFullscreenVert, sizeof kFullscreenVert, kBackgroundFrag, sizeof kBackgroundFrag,
                                       Input::None, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::None, passHdr_, postLayout_});
    bloomFinal_ = makePipeline(d, {kFullscreenVert, sizeof kFullscreenVert, kBloomFinalFrag, sizeof kBloomFinalFrag,
                                   Input::None, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Blend::None, passHdr_, postLayout_});
    if (!capsuleAdd_ || !glowAdd_ || !triAdd_ || !bloomDown_ || !bloomUp_ || !composite_ || !threat_ || !hudAdd_ ||
        !glyphAdd_ || !glyphThreat_ || !particleAdd_ || !afterglow_ || !distort_ || !backgroundPass_ || !bloomFinal_) {
        RF_ERR("renderer: pipeline creation failed");
        return false;
    }
    return true;
}

void Renderer::destroyPipelines() {
    VkPipeline* all[] = {&capsuleAdd_, &glowAdd_,     &triAdd_,      &bloomDown_, &bloomUp_,
                         &composite_,  &threat_,      &hudAdd_,      &glyphAdd_,  &glyphThreat_,
                         &particleAdd_, &afterglow_, &distort_, &backgroundPass_, &bloomFinal_};
    for (VkPipeline* p : all) {
        if (*p) vkDestroyPipeline(gpu_.device, *p, nullptr);
        *p = VK_NULL_HANDLE;
    }
}

bool Renderer::createTarget(Target& t, VkExtent2D size, VkFormat format, VkRenderPass pass) {
    t.size = size;
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent = {size.width, size.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    RF_VK_TRY(vkCreateImage(gpu_.device, &ii, nullptr, &t.image));
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(gpu_.device, t.image, &req);
    i32 type = -1;
    for (u32 i = 0; i < gpu_.memoryProps.memoryTypeCount && type < 0; ++i)
        if ((req.memoryTypeBits & (1u << i)) &&
            (gpu_.memoryProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            type = static_cast<i32>(i);
    if (type < 0) {
        RF_ERR("renderer: no device-local memory for a render target");
        return false;
    }
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = static_cast<u32>(type);
    RF_VK_TRY(vkAllocateMemory(gpu_.device, &ai, nullptr, &t.memory));
    RF_VK_TRY(vkBindImageMemory(gpu_.device, t.image, t.memory, 0));
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = t.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    RF_VK_TRY(vkCreateImageView(gpu_.device, &vi, nullptr, &t.view));
    if (!pass) return true;
    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = pass;
    fb.attachmentCount = 1;
    fb.pAttachments = &t.view;
    fb.width = size.width;
    fb.height = size.height;
    fb.layers = 1;
    RF_VK_TRY(vkCreateFramebuffer(gpu_.device, &fb, nullptr, &t.framebuffer));
    return true;
}

bool Renderer::createTargets(VkExtent2D logical) {
    destroyTargets();
    targetsFor_ = logical;
    const auto scaled = [&logical](u32 shift) {
        return VkExtent2D{(logical.width + (1u << shift) - 1) >> shift, (logical.height + (1u << shift) - 1) >> shift};
    };
    if (!createTarget(scene_, logical) || !createTarget(pools_, scaled(2))) return false;
    bloomLevels_ = look_->bloomScales < kMaxBloomLevels ? look_->bloomScales : kMaxBloomLevels;
    for (u32 k = 1; k <= bloomLevels_; ++k) {
        if (!createTarget(down_[k], scaled(k))) return false;
        if (k < bloomLevels_ && !createTarget(up_[k], scaled(k))) return false;
    }
    solveBloom();
    // Afterglow history (two, by frame parity) and excess at the size of the bloom level it reads
    // (look.md v2.1 §7: sigma 6 ref px, one eighth resolution); distortion at W/8 or W/4.
    afterglowLevel_ = bloomLevels_ >= 3 ? 3u : bloomLevels_;
    for (u32 k = 1; k <= bloomLevels_; ++k)
        if (std::fabs(look_->bloomSigmasRefPx[k - 1] - look_->afterglowSourceSigmaRefPx) < 0.01f) afterglowLevel_ = k;
    const u32 agShift = afterglowLevel_;
    for (Target& t : agHistory_)
        if (!createTarget(t, scaled(agShift), hdrFormat_, VK_NULL_HANDLE)) return false;
    if (!createTarget(agExcess_, scaled(agShift), hdrFormat_, VK_NULL_HANDLE)) return false;
    for (u32 i = 0; i < 2; ++i) {
        const VkImageView views[2] = {agHistory_[i].view, agExcess_.view};
        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = passAfterglow_;
        fb.attachmentCount = 2;
        fb.pAttachments = views;
        fb.width = agExcess_.size.width;
        fb.height = agExcess_.size.height;
        fb.layers = 1;
        RF_VK_TRY(vkCreateFramebuffer(gpu_.device, &fb, nullptr, &agFramebuffers_[i]));
    }
    const u32 distShift = tier_.distortionShift ? tier_.distortionShift : 3u;
    if (!createTarget(distortion_, scaled(distShift), VK_FORMAT_R16G16_SFLOAT, passDistort_)) return false;
    if (!createTarget(bgTarget_, scaled(tier_.backgroundShift ? tier_.backgroundShift : 2u))) return false;

    // Clear every target once and leave it readable: a pass that is skipped (bloom off) must still
    // be sampled from a defined layout with finite contents.
    {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pi.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pi.queueFamilyIndex = gpu_.queueFamily;
        RF_VK_TRY(vkCreateCommandPool(gpu_.device, &pi, nullptr, &pool));
        VkCommandBuffer cb = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        vkAllocateCommandBuffers(gpu_.device, &ai, &cb);
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &bi);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto clear = [&](const Target& t) {
            if (!t.image) return;
            VkImageMemoryBarrier b{};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = t.image;
            b.subresourceRange = range;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                 nullptr, 1, &b);
            const VkClearColorValue zero{};
            vkCmdClearColorImage(cb, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &range);
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                                 0, nullptr, 1, &b);
        };
        clear(scene_);
        clear(pools_);
        for (u32 k = 1; k <= bloomLevels_; ++k) {
            clear(down_[k]);
            clear(up_[k]);
        }
        clear(agHistory_[0]);
        clear(agHistory_[1]);
        clear(agExcess_);
        clear(distortion_);
        clear(bgTarget_);
        vkEndCommandBuffer(cb);
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        vkQueueSubmit(gpu_.queue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(gpu_.queue);
        vkDestroyCommandPool(gpu_.device, pool, nullptr);
    }
    updateDescriptors();
    agParity_ = 0;
    RF_LOG("renderer: targets %ux%u (%s), pools %ux%u, %u bloom levels (tier %u), afterglow %ux%u%s, distortion %ux%u%s",
           logical.width, logical.height, hdrFormatName_, pools_.size.width, pools_.size.height, bloomLevels_,
           tier_.bloomLevels, agExcess_.size.width, agExcess_.size.height, tier_.afterglowShift ? "" : " (off)",
           distortion_.size.width, distortion_.size.height, tier_.distortionShift ? "" : " (off)");
    return true;
}

void Renderer::destroyTargets() {
    if (!gpu_.device) return;
    for (VkFramebuffer& fb : agFramebuffers_) {
        if (fb) vkDestroyFramebuffer(gpu_.device, fb, nullptr);
        fb = VK_NULL_HANDLE;
    }
    Target* all[7 + 2 * (kMaxBloomLevels + 1)];
    u32 n = 0;
    all[n++] = &scene_;
    all[n++] = &pools_;
    all[n++] = &agHistory_[0];
    all[n++] = &agHistory_[1];
    all[n++] = &agExcess_;
    all[n++] = &distortion_;
    all[n++] = &bgTarget_;
    for (u32 k = 0; k <= kMaxBloomLevels; ++k) {
        all[n++] = &down_[k];
        all[n++] = &up_[k];
    }
    for (u32 i = 0; i < n; ++i) {
        Target& t = *all[i];
        if (t.framebuffer) vkDestroyFramebuffer(gpu_.device, t.framebuffer, nullptr);
        if (t.view) vkDestroyImageView(gpu_.device, t.view, nullptr);
        if (t.image) vkDestroyImage(gpu_.device, t.image, nullptr);
        if (t.memory) vkFreeMemory(gpu_.device, t.memory, nullptr);
        t = Target{};
    }
}

void Renderer::solveBloom() {
    // Every scale k of look.json is a Gaussian of sigma_k reference pixels. Level k is built from level
    // k-1 by the 4-tap blur-and-halve pass and reaches the screen through k bilinear 2x upsamples
    // (each adds 3/16 of its source texel squared). Choose each level's tap offset so that the
    // accumulated variance equals sigma_k^2 (TECH_SPEC 5.2 bloom chain; look.md §3).
    const double refPx = static_cast<double>(targetsFor_.height) / 1080.0;
    double chain = 0.0;
    for (u32 k = 1; k <= bloomLevels_; ++k) {
        const double sigma = look_->bloomSigmasRefPx[k - 1] * refPx;
        double up = 0.0;
        for (u32 j = 1; j <= k; ++j) up += 0.1875 * std::pow(4.0, static_cast<double>(j));
        const double wanted = sigma * sigma - up;
        const double texel2 = std::pow(4.0, static_cast<double>(k - 1));  // source texel, squared full px
        const double v = (wanted - chain) / texel2;
        double lo = 0.5, hi = 16.0;
        if (v <= tapVariance(lo)) {
            hi = lo;
        } else {
            for (int it = 0; it < 60; ++it) {
                const double mid = 0.5 * (lo + hi);
                (tapVariance(mid) < v ? lo : hi) = mid;
            }
        }
        tapOffset_[k] = static_cast<float>(hi);
        chain += tapVariance(hi) * texel2;
    }
}

void Renderer::updateDescriptors() {
    if (!scene_.view) return;
    const auto image = [](VkSampler s, VkImageView v) {
        return VkDescriptorImageInfo{s, v, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    };
    // Every set gets all its bindings (unused ones point at the scene) so no binding is ever undefined.
    const VkDescriptorImageInfo sceneB = image(linearBorder_, scene_.view);
    const VkDescriptorImageInfo bgE = image(linearEdge_, bgTarget_.view);
    const auto write = [this, &sceneB, &bgE](VkDescriptorSet set, VkDescriptorImageInfo a, VkDescriptorImageInfo b,
                                             VkDescriptorImageInfo c, VkDeviceSize uboOffset, VkDescriptorImageInfo e,
                                             VkDescriptorImageInfo f) {
        VkDescriptorImageInfo imgs[kSetBindings] = {a, b, c, sceneB, e, f, bgE};
        VkDescriptorBufferInfo buf{uniforms_.buffer, uboOffset, sizeof(CompositeUbo)};
        VkWriteDescriptorSet w[kSetBindings]{};
        for (u32 i = 0; i < kSetBindings; ++i) {
            w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w[i].dstSet = set;
            w[i].dstBinding = i;
            w[i].descriptorCount = 1;
            if (i != 3) {
                w[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[i].pImageInfo = &imgs[i];
            } else {
                w[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                w[i].pBufferInfo = &buf;
            }
        }
        vkUpdateDescriptorSets(gpu_.device, kSetBindings, w, 0, nullptr);
    };
    const VkDescriptorImageInfo poolsB = image(linearBorder_, pools_.view);
    for (u32 k = 1; k <= bloomLevels_; ++k) {
        const VkDescriptorImageInfo src = k == 1 ? sceneB : image(linearBorder_, down_[k - 1].view);
        write(downSets_[k], src, poolsB, sceneB, 0, sceneB, sceneB);
    }
    for (u32 k = 1; k + 1 < bloomLevels_; ++k)
        write(upSets_[k], image(linearBorder_, up_[k + 1].view), image(linearBorder_, down_[k].view), sceneB, 0, sceneB,
              sceneB);
    for (u32 L = 2; L <= bloomLevels_; ++L)
        write(upFirstSets_[L], image(linearBorder_, down_[L].view), image(linearBorder_, down_[L - 1].view), sceneB, 0,
              sceneB, sceneB);
    // The gathering pass (shaders/bloom_final.frag): bloom levels, pools, afterglow excess, background.
    const VkDescriptorImageInfo poolsE = image(linearEdge_, pools_.view);
    const VkDescriptorImageInfo agE = image(linearEdge_, agExcess_.view);
    if (bloomLevels_ >= 3)
        write(finalSets_[0], image(linearBorder_, up_[2].view), image(linearBorder_, down_[1].view), poolsE, 0, agE, sceneB);
    if (bloomLevels_ >= 2)
        write(finalSets_[1], image(linearBorder_, down_[2].view), image(linearBorder_, down_[1].view), poolsE, 0, agE,
              sceneB);
    for (u32 p = 0; p < 2; ++p)
        write(afterglowSets_[p], image(linearEdge_, down_[afterglowLevel_].view), image(linearEdge_, agHistory_[1 - p].view),
              sceneB, 0, sceneB, sceneB);
    const VkImageView bloomTop = bloomLevels_ > 1 ? up_[1].view : down_[1].view;
    for (u32 f = 0; f < kFramesInFlight; ++f) {
        write(compositeSets_[f], image(linearEdge_, scene_.view), image(linearEdge_, pools_.view),
              image(linearEdge_, bloomTop), uniformStride_ * f, image(linearEdge_, agExcess_.view),
              image(linearEdge_, distortion_.view));
        write(distortSets_[f], sceneB, sceneB, sceneB, uniformStride_ * f, sceneB, sceneB);
    }
}

bool Renderer::createTimestamps() {
    u32 families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu_.physical, &families, nullptr);
    VkQueueFamilyProperties props[16];
    if (families > 16) families = 16;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu_.physical, &families, props);
    const u32 bits = gpu_.queueFamily < families ? props[gpu_.queueFamily].timestampValidBits : 0u;
    if (bits == 0 || gpu_.props.limits.timestampPeriod <= 0.0f) {
        RF_WARN("renderer: no GPU timestamps on this queue; GPU pass times will not be reported");
        return false;
    }
    timestampNs_ = static_cast<double>(gpu_.props.limits.timestampPeriod);
    timestampMask_ = bits >= 64 ? ~0ull : ((1ull << bits) - 1ull);
    VkQueryPoolCreateInfo qi{};
    qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qi.queryCount = kTimestamps * kFramesInFlight;
    if (vkCreateQueryPool(gpu_.device, &qi, nullptr, &timestampPool_) != VK_SUCCESS) {
        timestampPool_ = VK_NULL_HANDLE;
        return false;
    }
    RF_LOG("renderer: GPU timestamps, %u valid bits, %.2f ns per tick", bits, timestampNs_);
    return true;
}

void Renderer::readTimestamps(u32 slot) {
    if (!timestampPool_ || !timestampsWritten_[slot]) return;
    u64 ts[kTimestamps] = {};
    const VkResult r = vkGetQueryPoolResults(gpu_.device, timestampPool_, slot * kTimestamps, kTimestamps, sizeof ts, ts,
                                             sizeof(u64), VK_QUERY_RESULT_64_BIT);
    if (r != VK_SUCCESS) return;  // not ready (cannot happen after the fence) or lost
    const auto ms = [this, &ts](u32 a, u32 b) {
        const u64 d = (ts[b] - ts[a]) & timestampMask_;
        return static_cast<float>(static_cast<double>(d) * timestampNs_ * 1e-6);
    };
    gpuTimes_.ms[GpuPassTimes::Scene] = ms(0, 1);
    gpuTimes_.ms[GpuPassTimes::Pools] = ms(1, 2);
    gpuTimes_.ms[GpuPassTimes::Background] = ms(2, 3);
    gpuTimes_.ms[GpuPassTimes::Bloom] = ms(3, 4) + ms(5, 6);  // down, then up and the gathering pass
    gpuTimes_.ms[GpuPassTimes::Afterglow] = ms(4, 5);
    gpuTimes_.ms[GpuPassTimes::Distortion] = ms(6, 7);
    gpuTimes_.ms[GpuPassTimes::Wait] = ms(7, 8);  // the swapchain image was not free yet (or the queue was idle)
    gpuTimes_.ms[GpuPassTimes::Composite] = ms(8, 9);
    gpuTimes_.ms[GpuPassTimes::Total] = ms(0, 7) + ms(8, 9);  // work only
    gpuTimes_.valid = true;
    gpuTimes_.frame = timestampFrame_[slot];
}

bool Renderer::setTier(const RenderTier& tier) {
    RenderTier t = tier;
    if (t.bloomLevels < 1) t.bloomLevels = 1;
    if (t.bloomLevels > bloomLevels_ && bloomLevels_ > 0) t.bloomLevels = bloomLevels_;
    const bool rebuild = t.afterglowShift != tier_.afterglowShift || t.distortionShift != tier_.distortionShift ||
                         t.backgroundShift != tier_.backgroundShift;
    tier_ = t;
    if (rebuild && scene_.image) {
        vkDeviceWaitIdle(gpu_.device);
        if (!createTargets(targetsFor_)) return false;
    }
    return true;
}

void Renderer::resetParticles() {
    if (gpu_.device) vkDeviceWaitIdle(gpu_.device);  // no frame in flight still reads the ring
    markFirst_ = markCount_ = 0;
    particleClock_ = 0.0f;
}

bool Renderer::setGlyphs(const GlyphSet& glyphs) {
    if (glyphMesh_.buffer) {
        vkDeviceWaitIdle(gpu_.device);
        gpu_.destroyBuffer(glyphMesh_);
    }
    glyphSet_ = &glyphs;
    const u32 segs = glyphs.segmentCount();
    if (segs * 4 > 65535) {
        RF_ERR("renderer: %u glyph segments exceed 16-bit indices", segs);
        return false;
    }
    const VkDeviceSize vbytes = VkDeviceSize(segs) * 4 * sizeof(GlyphVertexGpu);
    glyphIndexOffset_ = alignUp(vbytes, 256);
    const VkDeviceSize total = glyphIndexOffset_ + VkDeviceSize(segs) * 6 * sizeof(u16);
    if (!gpu_.createBuffer(total, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, glyphMesh_))
        return false;
    u8* base = static_cast<u8*>(glyphMesh_.mapped);
    std::memcpy(base, glyphs.vertices(), static_cast<size_t>(vbytes));
    u16* idx = reinterpret_cast<u16*>(base + glyphIndexOffset_);
    for (u32 s = 0; s < segs; ++s) {
        const u16 v = static_cast<u16>(s * 4);
        const u16 quad[6] = {v, static_cast<u16>(v + 1), static_cast<u16>(v + 2), static_cast<u16>(v + 2),
                             static_cast<u16>(v + 1), static_cast<u16>(v + 3)};
        std::memcpy(idx + s * 6, quad, sizeof quad);
    }
    RF_LOG("renderer: glyph meshes uploaded (%u shapes, %u segments, %u KB)", glyphs.count(), segs,
           static_cast<u32>(total / 1024));
    return true;
}

u32 Renderer::sortGlyphs(const GlyphBatch& batch, GlyphInstanceGpu* dst, GlyphRun* runs, u32 maxRuns) {
    const u32 n = batch.count();
    if (!n || !glyphSet_) return 0;
    constexpr u32 kKeys = GlyphSet::kMaxGlyphs * 2;
    u32 counts[kKeys] = {};
    const u16* keys = batch.keys();
    for (u32 i = 0; i < n; ++i) ++counts[keys[i] < kKeys ? keys[i] : 0];
    u32 offsets[kKeys];
    u32 at = 0, runCount = 0;
    for (u32 k = 0; k < kKeys; ++k) {
        offsets[k] = at;
        if (counts[k] && runCount < maxRuns) runs[runCount++] = {k, at, counts[k]};
        at += counts[k];
    }
    const GlyphInstanceGpu* src = batch.data();
    for (u32 i = 0; i < n; ++i) {
        const u32 k = keys[i] < kKeys ? keys[i] : 0;
        std::memcpy(dst + offsets[k]++, src + i, sizeof(GlyphInstanceGpu));
    }
    return runCount;
}

void Renderer::drawGlyphRuns(VkCommandBuffer cmd, VkDeviceSize instanceOffset, const GlyphRun* runs, u32 runCount) {
    if (!runCount || !glyphSet_) return;
    const VkBuffer buffers[2] = {glyphMesh_.buffer, dynamic_.buffer};
    const VkDeviceSize offsets[2] = {0, instanceOffset};
    vkCmdBindVertexBuffers(cmd, 0, 2, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, glyphMesh_.buffer, glyphIndexOffset_, VK_INDEX_TYPE_UINT16);
    for (u32 r = 0; r < runCount; ++r) {
        const GlyphDef& g = glyphSet_->def(runs[r].key / 2);
        const u32 segs = (runs[r].key & 1u) ? g.hullCount : g.segmentCount;
        if (!segs) continue;
        vkCmdDrawIndexed(cmd, segs * 6, runs[r].count, g.firstSegment * 6, 0, runs[r].first);
    }
}

bool Renderer::createSwapchainResources() {
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
        VkSemaphoreCreateInfo sem{};
        sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        RF_VK_TRY(vkCreateSemaphore(gpu_.device, &sem, nullptr, &renderDone_[i]));
    }
    return true;
}

void Renderer::destroySwapchainResources() {
    for (u32 i = 0; i < Swapchain::kMaxImages; ++i) {
        if (framebuffers_[i]) vkDestroyFramebuffer(gpu_.device, framebuffers_[i], nullptr);
        if (renderDone_[i]) vkDestroySemaphore(gpu_.device, renderDone_[i], nullptr);
        framebuffers_[i] = VK_NULL_HANDLE;
        renderDone_[i] = VK_NULL_HANDLE;
    }
}

bool Renderer::rebuildSwapchain() {
    vkDeviceWaitIdle(gpu_.device);
    destroySwapchainResources();
    const VkFormat oldFormat = swapchain_.format;
    swapchain_.destroy(gpu_, pacer_);
    if (!swapchain_.create(gpu_, pacer_, window_)) return false;  // minimized or lost: retry later
    if (swapchain_.format != oldFormat) {
        RF_LOG("renderer: swapchain format changed, rebuilding passes and pipelines");
        destroyPipelines();
        destroyRenderPasses();
        if (!createRenderPasses() || !createPipelines()) return false;
        destroyTargets();  // their framebuffers belong to the old HDR pass
    }
    if (!createSwapchainResources()) return false;
    // Targets live in logical space: a 180-degree turn keeps them, a resize rebuilds them.
    if (!scene_.image || swapchain_.logical.width != targetsFor_.width || swapchain_.logical.height != targetsFor_.height) {
        if (!createTargets(swapchain_.logical)) return false;
    }
    rebuildPending_ = false;
    ++rebuilds_;
    return true;
}

void Renderer::releaseSurface() {
    if (!gpu_.device) return;
    vkDeviceWaitIdle(gpu_.device);
    destroySwapchainResources();
    swapchain_.destroy(gpu_, pacer_);
    swapchain_.destroySurface(gpu_);
    capture_.pending = false;
}

bool Renderer::restoreSurface() {
    if (!swapchain_.hasSurface() && !swapchain_.createSurface(gpu_, window_)) return false;
    return rebuildSwapchain();
}

bool Renderer::setStaticScene(const StaticScene& s) {
    if (static_.buffer) {
        vkDeviceWaitIdle(gpu_.device);
        gpu_.destroyBuffer(static_);
    }
    staticInfo_ = s;
    VkDeviceSize at = 0;
    const auto place = [&at](VkDeviceSize bytes) {
        const VkDeviceSize o = at;
        at = alignUp(at + bytes, 256);
        return o;
    };
    staticCaps_ = place(VkDeviceSize(s.capsuleCount) * sizeof(CapsuleGpu));
    staticSparkles_ = place(VkDeviceSize(s.sparkleCount) * sizeof(GlowGpu));
    staticPools_ = place(VkDeviceSize(s.poolCount) * sizeof(GlowGpu));
    staticTris_ = place(VkDeviceSize(s.triVertexCount) * sizeof(TriVertexGpu));
    staticRims_ = place(VkDeviceSize(s.threatCount) * sizeof(CapsuleGpu));
    staticCores_ = place(VkDeviceSize(s.threatCount) * sizeof(CapsuleGpu));
    if (at == 0) at = 256;
    if (!gpu_.createBuffer(at, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, static_))
        return false;
    u8* base = static_cast<u8*>(static_.mapped);
    if (s.capsuleCount) std::memcpy(base + staticCaps_, s.capsules, s.capsuleCount * sizeof(CapsuleGpu));
    if (s.sparkleCount) std::memcpy(base + staticSparkles_, s.sparkles, s.sparkleCount * sizeof(GlowGpu));
    if (s.poolCount) std::memcpy(base + staticPools_, s.pools, s.poolCount * sizeof(GlowGpu));
    if (s.triVertexCount) std::memcpy(base + staticTris_, s.triVertices, s.triVertexCount * sizeof(TriVertexGpu));
    if (s.threatCount) {
        std::memcpy(base + staticRims_, s.threatRims, s.threatCount * sizeof(CapsuleGpu));
        std::memcpy(base + staticCores_, s.threatCores, s.threatCount * sizeof(CapsuleGpu));
    }
    if (s.hasBackground) background_ = s.background;
    RF_LOG("renderer: scene file uploaded (%u capsules, %u sparkles, %u pools, %u triangles, %u threat)", s.capsuleCount,
           s.sparkleCount, s.poolCount, s.triVertexCount / 3, s.threatCount);
    return true;
}

bool Renderer::beginFrame(FrameDraw& draw) {
    if (fatal_) return false;
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
        if (r == VK_ERROR_DEVICE_LOST) fatal_ = true;
        return false;
    }
    r = vkAcquireNextImageKHR(gpu_.device, swapchain_.handle, UINT64_MAX, f.acquired, VK_NULL_HANDLE, &f.imageIndex);
    const u64 t2 = nowNs();
    timings_.fenceStart = t0;
    timings_.fenceEnd = t1;
    timings_.acquireEnd = t2;
    timings_.fenceWaitNs = t1 - t0;
    timings_.acquireNs = t2 - t1;
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
        if (r == VK_ERROR_DEVICE_LOST) fatal_ = true;
        return false;
    }
    vkResetFences(gpu_.device, 1, &f.fence);  // only once we know this frame will be submitted
    readTimestamps(frameIndex_);              // this slot's previous frame has finished
    pacer_.onFrameStart(gpu_.queue, f.imageIndex);
    u8* base = static_cast<u8*>(dynamic_.mapped) + frameIndex_ * frameBytes_;
    draw.scene.begin(reinterpret_cast<CapsuleGpu*>(base + regScene_.offset), regScene_.capacity);
    draw.sparkles.begin(reinterpret_cast<GlowGpu*>(base + regSparkles_.offset), regSparkles_.capacity);
    draw.pools.begin(reinterpret_cast<GlowGpu*>(base + regPools_.offset), regPools_.capacity);
    draw.glass.begin(reinterpret_cast<TriVertexGpu*>(base + regTris_.offset), regTris_.capacity);
    draw.threatRims.begin(reinterpret_cast<CapsuleGpu*>(base + regRims_.offset), regRims_.capacity);
    draw.threatCores.begin(reinterpret_cast<CapsuleGpu*>(base + regCores_.offset), regCores_.capacity);
    draw.hud.begin(reinterpret_cast<CapsuleGpu*>(base + regHud_.offset), regHud_.capacity);
    draw.glyphs.begin(glyphStaging_[0], glyphKeys_[0], regGlyphs_.capacity);
    draw.threatGlyphRims.begin(glyphStaging_[1], glyphKeys_[1], regThreatGlyphRims_.capacity);
    draw.threatGlyphCores.begin(glyphStaging_[2], glyphKeys_[2], regThreatGlyphCores_.capacity);
    draw.priorityShipCores.begin(glyphStaging_[3], glyphKeys_[3], regPriorityShips_.capacity);

    // Spark ring: slots born more than a life (plus two frames' margin) ago may be reused.
    while (markCount_ > 0 && (spawnMarks_[markFirst_].time < particleClock_ - (particleLife_ + 0.25f) ||
                              markCount_ >= kSpawnMarks)) {
        markFirst_ = (markFirst_ + 1) % kSpawnMarks;
        --markCount_;
    }
    const u64 oldestTotal = markCount_ ? spawnMarks_[markFirst_].total : spawnedTotal_;
    const u64 inUse = spawnedTotal_ - oldestTotal;
    ring_.free_ = inUse + 1 < ring_.cap_ ? static_cast<u32>(ring_.cap_ - 1 - inUse) : 0u;
    ring_.count_ = 0;
    ring_.dropped_ = 0;
    frameStartHead_ = ring_.head_;
    draw.particles = &ring_;
    timings_.beginEnd = nowNs();
    return true;
}

bool Renderer::ensureCaptureBuffer(VkDeviceSize bytes) {
    if (capture_.buffer.buffer && capture_.buffer.size >= bytes) return true;
    if (capture_.buffer.buffer) gpu_.destroyBuffer(capture_.buffer);
    return gpu_.createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             capture_.buffer);
}

bool Renderer::endFrame(const FrameDraw& draw, const FrameParams& params, bool capture) {
    timings_.recordStart = nowNs();
    Frame& f = frames_[frameIndex_];
    const VkExtent2D extent = swapchain_.extent;
    const VkExtent2D logical = swapchain_.logical;
    const LookVariant& lv = *params.look;
    const bool doCapture = capture && swapchain_.canCapture &&
                           ensureCaptureBuffer(VkDeviceSize(extent.width) * extent.height * 4);
    if (capture && !doCapture) RF_WARN("renderer: this swapchain cannot be captured");

    const VkDeviceSize frameBase = frameIndex_ * frameBytes_;
    const float W = float(logical.width), H = float(logical.height);
    // Content transform of the replayed scene: scale by height / canvas height, centred horizontally.
    const float cs = H / staticInfo_.canvasH;
    const float cox = 0.5f * (W - staticInfo_.canvasW * cs);
    const float refPx = H / 1080.0f;

    // Sparks: note where this frame's births start; the live range is everything born within a life.
    if (markCount_ >= kSpawnMarks) {
        markFirst_ = (markFirst_ + 1) % kSpawnMarks;
        --markCount_;
    }
    spawnMarks_[(markFirst_ + markCount_) % kSpawnMarks] = {frameStartHead_, params.particleTime, spawnedTotal_};
    ++markCount_;
    spawnedTotal_ += ring_.count_;
    particleClock_ = params.particleTime;
    particleLife_ = params.particleMaxLife;
    u32 liveFirst = ring_.head_, liveCount = 0;
    for (u32 i = 0; i < markCount_; ++i) {
        const SpawnMark& m = spawnMarks_[(markFirst_ + i) % kSpawnMarks];
        if (m.time >= params.particleTime - params.particleMaxLife) {
            liveFirst = m.head;
            liveCount = static_cast<u32>(spawnedTotal_ - m.total);
            break;
        }
    }
    lastLiveParticles_ = liveCount;

    // Glyph instances, grouped by shape into this frame's slices.
    u8* frameMem = static_cast<u8*>(dynamic_.mapped) + frameBase;
    glyphRunCount_[0] = sortGlyphs(draw.glyphs, reinterpret_cast<GlyphInstanceGpu*>(frameMem + regGlyphs_.offset),
                                   glyphRuns_[0], kMaxGlyphRuns);
    glyphRunCount_[1] = sortGlyphs(draw.threatGlyphRims,
                                   reinterpret_cast<GlyphInstanceGpu*>(frameMem + regThreatGlyphRims_.offset),
                                   glyphRuns_[1], kMaxGlyphRuns);
    glyphRunCount_[2] = sortGlyphs(draw.threatGlyphCores,
                                   reinterpret_cast<GlyphInstanceGpu*>(frameMem + regThreatGlyphCores_.offset),
                                   glyphRuns_[2], kMaxGlyphRuns);
    glyphRunCount_[3] = sortGlyphs(draw.priorityShipCores,
                                   reinterpret_cast<GlyphInstanceGpu*>(frameMem + regPriorityShips_.offset),
                                   glyphRuns_[3], kMaxGlyphRuns);
    lastGlyphs_ = draw.glyphs.count();

    vkResetCommandPool(gpu_.device, f.pool, 0);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd, &begin);
    VkCommandBuffer cmd = f.cmd;  // the swapchain pass is recorded into f.post (see below)

    const u32 tsBase = frameIndex_ * kTimestamps;
    const auto stamp = [&](u32 i, VkPipelineStageFlagBits stage) {
        if (timestampPool_) vkCmdWriteTimestamp(cmd, stage, timestampPool_, tsBase + i);
    };
    if (timestampPool_) vkCmdResetQueryPool(cmd, timestampPool_, tsBase, kTimestamps);

    const VkClearValue black{};
    const auto beginPass = [&](VkRenderPass pass, VkFramebuffer fb, VkExtent2D size) {
        const VkClearValue clears[2] = {black, black};
        VkRenderPassBeginInfo rp{};
        rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass = pass;
        rp.framebuffer = fb;
        rp.renderArea.extent = size;
        rp.clearValueCount = 2;
        rp.pClearValues = clears;
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        setViewport(cmd, size);
    };
    const auto beginHdr = [&](const Target& t) { beginPass(passHdr_, t.framebuffer, t.size); };
    GeoPush geo{};
    geo.view[0] = W;
    geo.view[1] = H;
    geo.view[2] = 2.0f / W;
    geo.view[3] = 2.0f / H;
    rotationMatrix(0, geo.rot);
    const auto pushRaw = [&]() {
        vkCmdPushConstants(cmd, geoLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof geo, &geo);
    };
    const auto glowMisc = [&](float poolLevel, float poolThreshold) {
        geo.misc[0] = look_->glowCutoffRadii;
        geo.misc[1] = look_->glowRadiusOverSigma;
        geo.misc[2] = poolLevel;
        geo.misc[3] = poolThreshold;
    };
    glowMisc(1.0f, 1e9f);
    const auto pushGeo = [&](bool content, float lineWidth) {
        geo.xform[0] = content ? cs : 1.0f;
        geo.xform[1] = content ? cox : 0.0f;
        geo.xform[2] = 0.0f;
        geo.xform[3] = lineWidth;
        pushRaw();
    };
    // World content: logical = world x scale + offset; widths in ref px x the look's line width.
    const auto pushWorld = [&](const WorldView& v, float lineWidth) {
        geo.xform[0] = v.scale;
        geo.xform[1] = v.offsetX;
        geo.xform[2] = v.offsetY;
        geo.xform[3] = refPx * lineWidth;
        pushRaw();
    };
    const auto bindAndDraw = [&](const GpuBuffer& buf, VkDeviceSize offset, u32 vertices, u32 instances) {
        if (!instances || !vertices || !buf.buffer) return;
        vkCmdBindVertexBuffers(cmd, 0, 1, &buf.buffer, &offset);
        vkCmdDraw(cmd, vertices, instances, 0, 0);
    };
    const auto glyphMisc = [&]() {
        geo.misc[0] = 0.5f * look_->enemy.ornamentWidth;
        geo.misc[1] = 0.5f * look_->enemy.coreWidth;
        geo.misc[2] = look_->enemy.ornamentIntensityOfHull;
        geo.misc[3] = look_->enemy.coreIntensityOfHull;
        geo.extra0[0] = params.accent.x;
        geo.extra0[1] = params.accent.y;
        geo.extra0[2] = params.accent.z;
        geo.extra0[3] = params.enemySwell;
    };

    // ---- Profiling load (--gpu-burn): full-screen draws into the scene target, which the scene pass
    // clears; before the first timestamp, so it is neither shown nor timed.
    if (params.gpuBurn) {
        PostPush burn{};
        rotationMatrix(0, burn.rot);
        beginHdr(scene_);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, backgroundPass_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout_, 0, 1, &distortSets_[frameIndex_], 0,
                                nullptr);
        vkCmdPushConstants(cmd, postLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof burn, &burn);
        for (u32 i = 0; i < params.gpuBurn; ++i) vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
    stamp(0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    // ---- Scene: lines, glyphs, sparks, sparkles, glass (additive HDR, full resolution)
    beginHdr(scene_);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, capsuleAdd_);
    pushGeo(true, lv.lineWidth);
    bindAndDraw(static_, staticCaps_, 4, staticInfo_.capsuleCount);
    pushGeo(false, lv.lineWidth);
    bindAndDraw(dynamic_, frameBase + regScene_.offset, 4, draw.scene.count());
    if (glyphRunCount_[0]) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glyphAdd_);
        glyphMisc();
        pushWorld(params.world, lv.lineWidth);
        drawGlyphRuns(cmd, frameBase + regGlyphs_.offset, glyphRuns_[0], glyphRunCount_[0]);
    }
    if (liveCount) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, particleAdd_);
        geo.misc[0] = params.particleTime;
        geo.misc[1] = look_->boom.streakSeconds;
        geo.misc[2] = look_->boom.streakMaxWu;
        geo.misc[3] = look_->flareFadeExponent;
        geo.extra0[0] = params.dimAt.x;
        geo.extra0[1] = params.dimAt.y;
        geo.extra0[2] = params.dimRadius;
        geo.extra0[3] = params.dimFactor;
        pushWorld(params.world, lv.lineWidth);
        const VkDeviceSize zero = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &particles_.buffer, &zero);
        const u32 firstRun = liveFirst + liveCount <= ring_.cap_ ? liveCount : ring_.cap_ - liveFirst;
        vkCmdDraw(cmd, 4, firstRun, 0, liveFirst);
        if (firstRun < liveCount) vkCmdDraw(cmd, 4, liveCount - firstRun, 0, 0);
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glowAdd_);
    glowMisc(1.0f, 1e9f);
    pushGeo(true, 1.0f);
    bindAndDraw(static_, staticSparkles_, 4, staticInfo_.sparkleCount);
    pushGeo(false, 1.0f);
    bindAndDraw(dynamic_, frameBase + regSparkles_.offset, 4, draw.sparkles.count());
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triAdd_);
    pushGeo(true, 1.0f);
    bindAndDraw(static_, staticTris_, staticInfo_.triVertexCount - staticInfo_.triVertexCount % 3, 1);
    pushGeo(false, 1.0f);
    bindAndDraw(dynamic_, frameBase + regTris_.offset, draw.glass.count() - draw.glass.count() % 3, 1);
    vkCmdEndRenderPass(cmd);
    stamp(1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    lastCapsules_ = staticInfo_.capsuleCount + draw.scene.count();

    // ---- Pools: soft light of 16 ref px and up at quarter resolution, scaled by the look's pool level
    beginHdr(pools_);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glowAdd_);
    glowMisc(lv.poolLevel, 0.0f);
    pushGeo(true, 1.0f);
    bindAndDraw(static_, staticPools_, 4, staticInfo_.poolCount);
    pushGeo(false, 1.0f);
    bindAndDraw(dynamic_, frameBase + regPools_.offset, 4, draw.pools.count());
    vkCmdEndRenderPass(cmd);
    stamp(2, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    PostPush post{};
    rotationMatrix(0, post.rot);
    const auto pushPost = [&]() {
        vkCmdPushConstants(cmd, postLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof post, &post);
    };

    // ---- Background: gradient and clouds at low resolution (the uniform block is written below,
    // before the submission).
    beginHdr(bgTarget_);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, backgroundPass_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout_, 0, 1, &distortSets_[frameIndex_], 0, nullptr);
    pushPost();
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    stamp(3, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    // ---- Bloom pyramid: blur-and-halve down to W/2^L, then weighted accumulation back up to W/2.
    // Lower tiers drop the coarsest scales and renormalise the rest (look.md §3).
    const u32 levels = tier_.bloomLevels < bloomLevels_ ? tier_.bloomLevels : bloomLevels_;
    float weights[kMaxBloomLevels + 1] = {};
    float weightSum = 0.0f;
    for (u32 k = 1; k <= levels; ++k) weightSum += lv.bloomWeights[k - 1];
    for (u32 k = 1; k <= levels; ++k) weights[k] = weightSum > 0.0f ? lv.bloomWeights[k - 1] / weightSum : 0.0f;
    const bool bloomOn = params.bloom && levels >= 2;
    if (bloomOn) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomDown_);
        for (u32 k = 1; k <= levels; ++k) {
            const VkExtent2D src = k == 1 ? scene_.size : down_[k - 1].size;
            beginHdr(down_[k]);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout_, 0, 1, &downSets_[k], 0, nullptr);
            post.p0[0] = tapOffset_[k] / float(src.width);
            post.p0[1] = tapOffset_[k] / float(src.height);
            post.p0[2] = k == 1 ? 1.0f : 0.0f;
            post.p0[3] = 0.0f;
            pushPost();
            vkCmdDraw(cmd, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmd);
        }
    }
    stamp(4, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    // ---- Afterglow (look.md v2.1 §7): a soft tail on hot light. Source: the bloom level of sigma 6 ref
    // px less a knee; history = max(source, previous x decay), re-aimed by the camera.
    const bool afterglowOn = params.afterglow && bloomOn && levels >= afterglowLevel_ && tier_.afterglowShift > 0 &&
                             lv.afterglowMs > 0.0f;
    if (afterglowOn) {
        beginPass(passAfterglow_, agFramebuffers_[agParity_], agExcess_.size);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, afterglow_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout_, 0, 1, &afterglowSets_[agParity_], 0,
                                nullptr);
        post.p0[0] = params.cameraScale > 1e-6f ? params.cameraScale : 1.0f;
        post.p0[1] = params.cameraShiftX;
        post.p0[2] = params.cameraShiftY;
        post.p0[3] = std::exp(-params.dtSeconds * 1000.0f / lv.afterglowMs);
        post.p1[0] = W;
        post.p1[1] = H;
        post.p1[2] = 1.0f / W;
        post.p1[3] = 1.0f / H;
        post.p2[0] = look_->afterglowKnee;
        pushPost();
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
    stamp(5, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    // ---- Bloom up, ending in the pass that also gathers the pools, the afterglow excess and the
    // background, each divided by the bloom strength (shaders/bloom_final.frag): the composite then
    // reads one low-resolution texture. Without bloom only that last pass runs, with bloom weight 0.
    const float bloomStrength = bloomOn ? lv.bloom * params.bloomPump : 0.0f;
    const bool gather = bloomStrength > 1e-4f;
    const float compositeBloom = gather ? bloomStrength : 1.0f;
    if (bloomOn) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomUp_);
        for (u32 k = levels - 1; k >= 2; --k) {
            beginHdr(up_[k]);
            const bool first = k + 1 == levels;
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout_, 0, 1,
                                    first ? &upFirstSets_[levels] : &upSets_[k], 0, nullptr);
            post.p0[0] = first ? weights[k + 1] : 1.0f;
            post.p0[1] = weights[k];
            post.p0[2] = 0.0f;
            post.p0[3] = 0.0f;
            pushPost();
            vkCmdDraw(cmd, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmd);
        }
    }
    beginHdr(up_[1]);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomFinal_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout_, 0, 1, &finalSets_[levels <= 2 ? 1 : 0], 0,
                            nullptr);
    post.p0[0] = gather ? (levels <= 2 ? weights[2] : 1.0f) : 0.0f;
    post.p0[1] = gather ? weights[1] : 0.0f;
    post.p0[2] = 1.0f / compositeBloom;
    post.p0[3] = afterglowOn ? lv.afterglowStrength * params.afterglowGain / compositeBloom : 0.0f;
    post.p1[0] = lv.bgLevel / compositeBloom;
    post.p1[1] = post.p1[2] = post.p1[3] = 0.0f;
    pushPost();
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    stamp(6, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    // ---- Distortion field (look.md §7) at W/8 or W/4
    const bool distortionOn = tier_.distortionShift > 0 && (params.ringCount > 0 || params.wellCount > 0);
    if (distortionOn) {
        beginPass(passDistort_, distortion_.framebuffer, distortion_.size);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, distort_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout_, 0, 1, &distortSets_[frameIndex_], 0,
                                nullptr);
        pushPost();
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
    stamp(7, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    // ---- Composite constants
    CompositeUbo ubo{};
    ubo.view[0] = W;
    ubo.view[1] = H;
    ubo.view[2] = 1.0f / W;
    ubo.view[3] = 1.0f / H;
    const Background& bg = background_;
    const bool canvas = staticInfo_.hasBackground;
    ubo.bgEdge[0] = bg.edge.x;
    ubo.bgEdge[1] = bg.edge.y;
    ubo.bgEdge[2] = bg.edge.z;
    ubo.bgEdge[3] = lv.bgLevel;
    ubo.bgCentre[0] = bg.centre.x;
    ubo.bgCentre[1] = bg.centre.y;
    ubo.bgCentre[2] = bg.centre.z;
    ubo.bgCentre[3] = bg.power;
    ubo.bgShape[0] = bg.cx;
    ubo.bgShape[1] = bg.cy;
    ubo.bgShape[2] = bg.radius;
    ubo.bgShape[3] = canvas ? staticInfo_.canvasW / staticInfo_.canvasH : W / H;
    ubo.canvas[0] = canvas ? cs : 1.0f;
    ubo.canvas[1] = canvas ? cox : 0.0f;
    ubo.canvas[2] = 0.0f;
    ubo.canvas[3] = canvas ? staticInfo_.canvasH : H;
    // Reciprocals precomputed here (shaders/composite.frag tone map).
    ubo.tone[0] = lv.exposure * params.exposurePump;
    ubo.tone[1] = 1.0f / (lv.whitePoint * lv.whitePoint);
    ubo.tone[2] = lv.bleed;
    ubo.tone[3] = look_->bleedStart;
    ubo.tone2[0] = 1.0f / (look_->bleedEnd - look_->bleedStart);
    ubo.tone2[1] = compositeBloom;  // the gathered texture is pre-divided by it
    ubo.tone2[2] = swapchain_.srgbFormat ? 0.0f : 1.0f;
    ubo.tone2[3] = params.dither ? 1.0f : 0.0f;
    ubo.vignette[0] = params.vignetteAmount >= 0.0f ? params.vignetteAmount : look_->vignetteAmount;
    ubo.vignette[1] = look_->vignetteInner;
    ubo.vignette[2] = look_->vignetteOuter;
    ubo.vignette[3] = params.vignette ? 1.0f : 0.0f;
    ubo.post[0] = afterglowOn ? lv.afterglowStrength * params.afterglowGain : 0.0f;
    ubo.post[1] = params.aberration && tier_.aberration ? 1.0f : 0.0f;
    ubo.post[2] = look_->aberrationOfDistortion;
    ubo.post[3] = distortionOn ? 1.0f : 0.0f;
    ubo.post2[0] = look_->aberrationCornerRefPx * refPx;
    ubo.post2[1] = params.aberrationSnarePx;
    ubo.post2[2] = clampf(params.drain, 0.0f, 1.0f);
    ubo.grade[0] = look_->gradeSaturation;
    ubo.grade[1] = look_->gradeVibrance;
    ubo.grade[2] = look_->gradeContrast;
    ubo.grade[3] = (look_->gradeSaturation != 1.0f || look_->gradeVibrance != 0.0f || look_->gradeContrast != 0.0f) ? 1.0f : 0.0f;
    const u32 rings = params.ringCount < 8 ? params.ringCount : 8;
    const u32 wells = params.wellCount < 4 ? params.wellCount : 4;
    ubo.dcount[0] = static_cast<float>(rings);
    ubo.dcount[1] = static_cast<float>(wells);
    for (u32 i = 0; i < rings; ++i) {
        const DistortionRing& r = params.rings[i];
        ubo.ring[2 * i][0] = r.x;
        ubo.ring[2 * i][1] = r.y;
        ubo.ring[2 * i][2] = r.radius;
        ubo.ring[2 * i][3] = r.width > 0.0f ? r.width / 2.3548f : 1.0f;  // "wide" = full width at half maximum
        ubo.ring[2 * i + 1][0] = r.amount;
    }
    for (u32 i = 0; i < wells; ++i) {
        const DistortionWell& w = params.wells[i];
        ubo.well[2 * i][0] = w.x;
        ubo.well[2 * i][1] = w.y;
        ubo.well[2 * i][2] = w.peakRadius;
        ubo.well[2 * i][3] = w.goneRadius > w.peakRadius ? w.goneRadius : w.peakRadius + 1.0f;
        ubo.well[2 * i + 1][0] = w.amount;
    }
    ubo.nebCount[0] = static_cast<float>(bg.cloudCount);
    ubo.nebCount[1] = static_cast<float>(params.frameIndex & 0xFFFFu);
    for (u32 i = 0; i < bg.cloudCount && i < 8; ++i) {
        const Background::Cloud& c = bg.clouds[i];
        const float sigma = c.radius * look_->nebulaSigmaOverRadius;
        ubo.neb[2 * i][0] = c.x;
        ubo.neb[2 * i][1] = c.y;
        ubo.neb[2 * i][2] = sigma;
        ubo.neb[2 * i][3] = c.radius * 1.3f;  // the reference's cut-off box (lookdev.py nebula())
        ubo.neb[2 * i + 1][0] = c.colour.x;
        ubo.neb[2 * i + 1][1] = c.colour.y;
        ubo.neb[2 * i + 1][2] = c.colour.z;
    }
    std::memcpy(static_cast<u8*>(uniforms_.mapped) + uniformStride_ * frameIndex_, &ubo, sizeof ubo);

    // ---- Composite into the swapchain, then the threat layer and the HUD after tone mapping. A second
    // command buffer, submitted with the wait for the acquired image: the offscreen passes start at
    // once, and the GPU timers can tell work from waiting for the panel.
    vkEndCommandBuffer(cmd);
    cmd = f.post;
    vkBeginCommandBuffer(cmd, &begin);
    stamp(8, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    {
        VkRenderPassBeginInfo rp{};
        rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass = doCapture ? passCapture_ : passPresent_;
        rp.framebuffer = framebuffers_[f.imageIndex];
        rp.renderArea.extent = extent;
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        setViewport(cmd, extent);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, composite_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout_, 0, 1, &compositeSets_[frameIndex_], 0,
                                nullptr);
        rotationMatrix(swapchain_.rotationDeg, post.rot);
        vkCmdPushConstants(cmd, postLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof post, &post);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        // Threat layer: every rim (capsules, then glyphs), then every core; never shaken (look.md §7).
        rotationMatrix(swapchain_.rotationDeg, geo.rot);
        const auto threatGlyphs = [&](u32 batch, VkDeviceSize offset) {
            if (!glyphRunCount_[batch]) return;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glyphThreat_);
            glyphMisc();
            pushWorld(params.worldThreat, 1.0f);
            drawGlyphRuns(cmd, frameBase + offset, glyphRuns_[batch], glyphRunCount_[batch]);
        };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, threat_);
        pushGeo(true, 1.0f);
        bindAndDraw(static_, staticRims_, 4, staticInfo_.threatCount);
        pushGeo(false, 1.0f);
        bindAndDraw(dynamic_, frameBase + regRims_.offset, 4, draw.threatRims.count());
        threatGlyphs(1, regThreatGlyphRims_.offset);
        threatGlyphs(3, regPriorityShips_.offset);  // ships' cores before the threats' (look.md §9)
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, threat_);
        pushGeo(true, 1.0f);
        bindAndDraw(static_, staticCores_, 4, staticInfo_.threatCount);
        pushGeo(false, 1.0f);
        bindAndDraw(dynamic_, frameBase + regCores_.offset, 4, draw.threatCores.count());
        threatGlyphs(2, regThreatGlyphCores_.offset);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hudAdd_);
        pushGeo(false, 1.0f);
        bindAndDraw(dynamic_, frameBase + regHud_.offset, 4, draw.hud.count());
        vkCmdEndRenderPass(cmd);
    }
    stamp(9, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    timestampsWritten_[frameIndex_] = timestampPool_ != VK_NULL_HANDLE;
    timestampFrame_[frameIndex_] = frameCounter_;
    if (afterglowOn) agParity_ ^= 1u;

    if (doCapture) {
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {extent.width, extent.height, 1};
        vkCmdCopyImageToBuffer(cmd, swapchain_.images[f.imageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               capture_.buffer.buffer, 1, &region);
        VkImageMemoryBarrier toPresent{};
        toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
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
        toHost.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1,
                             &toHost, 1, &toPresent);
    }
    vkEndCommandBuffer(cmd);
    timings_.recordEnd = nowNs();

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkSubmitInfo submits[2]{};
    submits[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submits[0].commandBufferCount = 1;
    submits[0].pCommandBuffers = &f.cmd;
    submits[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submits[1].waitSemaphoreCount = 1;
    submits[1].pWaitSemaphores = &f.acquired;
    submits[1].pWaitDstStageMask = &waitStage;
    submits[1].commandBufferCount = 1;
    submits[1].pCommandBuffers = &f.post;
    submits[1].signalSemaphoreCount = 1;
    submits[1].pSignalSemaphores = &renderDone_[f.imageIndex];
    VkResult r = vkQueueSubmit(gpu_.queue, 2, submits, f.fence);
    timings_.submitEnd = nowNs();
    if (r != VK_SUCCESS) {
        RF_ERR("vkQueueSubmit: %s", vkResultName(r));
        fatal_ = true;  // (its fence was reset and will never signal: the next wait on it would hang)
        return false;
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderDone_[f.imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_.handle;
    present.pImageIndices = &f.imageIndex;
    timings_.presentStart = nowNs();
    r = pacer_.present(gpu_.queue, present);
    timings_.presentEnd = nowNs();
    timings_.presentNs = timings_.presentEnd - timings_.presentStart;
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        rebuildPending_ = true;
    } else if (r == VK_ERROR_SURFACE_LOST_KHR) {
        surfaceLost_ = true;
    } else if (r != VK_SUCCESS) {
        RF_ERR("present: %s", vkResultName(r));
        if (r == VK_ERROR_DEVICE_LOST) fatal_ = true;
    }
    if (doCapture) {
        capture_.pending = true;
        capture_.slot = frameIndex_;
        capture_.extent = extent;
        capture_.rotationDeg = swapchain_.rotationDeg;
        capture_.format = swapchain_.format;
    }
    frameIndex_ = (frameIndex_ + 1) % kFramesInFlight;
    ++frameCounter_;
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
