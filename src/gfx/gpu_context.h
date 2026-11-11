#pragma once
// Vulkan instance, device, the single graphics+present queue, and a minimal memory helper.
// No VMA: the renderer needs a few dozen allocations, each made once at load.

#include <atomic>

#include "core/types.h"
#include "gfx/vk.h"

namespace rf {

class FramePacer;

struct GpuBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;  // persistently mapped when host visible
    bool coherent = true;
    bool deviceLocal = false;
};

class GpuContext {
public:
    // Instance through SDL's loader entry point; validation adds VK_LAYER_KHRONOS_validation,
    // VK_EXT_debug_utils and synchronization validation when available.
    bool createInstance(bool validation);
    // Picks a Vulkan 1.1 GPU that can present to `surface` and creates the device and queue.
    bool createDevice(VkSurfaceKHR surface, FramePacer& pacer);
    void destroy(FramePacer& pacer);

    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags required,
                      VkMemoryPropertyFlags preferred, GpuBuffer& out);
    void destroyBuffer(GpuBuffer& buffer);
    // Makes device writes visible to the host for non-coherent memory.
    void invalidate(const GpuBuffer& buffer) const;

    // Messages from the validation layer (warnings and errors). Loader messages are counted apart.
    u32 validationMessages() const { return validationMessages_.load(std::memory_order_relaxed); }
    u32 loaderMessages() const { return loaderMessages_.load(std::memory_order_relaxed); }
    bool validationEnabled() const { return validation_; }

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    u32 queueFamily = 0;
    u32 loaderVersion = 0;
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceMemoryProperties memoryProps{};
    char apiVersionText[24] = {};
    char driverVersionText[32] = {};

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                        VkDebugUtilsMessageTypeFlagsEXT types,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                        void* user);
    i32 findMemoryType(u32 typeBits, VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred) const;

    VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
    bool validation_ = false;
    std::atomic<u32> validationMessages_{0};
    std::atomic<u32> loaderMessages_{0};
};

}  // namespace rf
