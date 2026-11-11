#include "gfx/gpu_context.h"

#include <SDL3/SDL_vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <vulkan/vk_enum_string_helper.h>

#include "gfx/frame_pacer.h"

namespace rf {

const char* vkResultName(VkResult result) { return string_VkResult(result); }

namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

bool hasLayer(const char* name) {
    u32 count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    if (count == 0) return false;
    auto* layers = static_cast<VkLayerProperties*>(std::malloc(sizeof(VkLayerProperties) * count));
    vkEnumerateInstanceLayerProperties(&count, layers);
    bool found = false;
    for (u32 i = 0; i < count && !found; ++i) found = std::strcmp(layers[i].layerName, name) == 0;
    std::free(layers);
    return found;
}

bool hasInstanceExtension(const char* layer, const char* name) {
    u32 count = 0;
    vkEnumerateInstanceExtensionProperties(layer, &count, nullptr);
    if (count == 0) return false;
    auto* exts = static_cast<VkExtensionProperties*>(std::malloc(sizeof(VkExtensionProperties) * count));
    vkEnumerateInstanceExtensionProperties(layer, &count, exts);
    bool found = false;
    for (u32 i = 0; i < count && !found; ++i) found = std::strcmp(exts[i].extensionName, name) == 0;
    std::free(exts);
    return found;
}

void formatDriverVersion(const VkPhysicalDeviceProperties& p, char* out, size_t cap) {
    const u32 v = p.driverVersion;
    if (p.vendorID == 0x10DE) {  // NVIDIA: 10.8.8.6
        std::snprintf(out, cap, "%u.%u.%u.%u", v >> 22, (v >> 14) & 0xFFu, (v >> 6) & 0xFFu, v & 0x3Fu);
    } else {  // VK_MAKE_VERSION layout (10.10.12); Qualcomm's major (e.g. 512) needs all 10 bits
        std::snprintf(out, cap, "%u.%u.%u", v >> 22, (v >> 12) & 0x3FFu, v & 0xFFFu);
    }
}

const char* deviceTypeName(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
        default: return "other";
    }
}

}  // namespace

VKAPI_ATTR VkBool32 VKAPI_CALL GpuContext::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                         VkDebugUtilsMessageTypeFlagsEXT,
                                                         const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                         void* user) {
    auto* self = static_cast<GpuContext*>(user);
    const char* id = data && data->pMessageIdName ? data->pMessageIdName : "-";
    const char* text = data && data->pMessage ? data->pMessage : "";
    // The loader reports its own environment problems (e.g. stale layer registrations) through the
    // same messenger. They are not findings about our API usage, so they are counted separately.
    if (std::strcmp(id, "Loader Message") == 0) {
        if (self) self->loaderMessages_.fetch_add(1, std::memory_order_relaxed);
        RF_WARN("[vulkan loader] %s", text);
        return VK_FALSE;
    }
    if (self) self->validationMessages_.fetch_add(1, std::memory_order_relaxed);
    const char* level = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" : "WARNING";
    RF_ERR("[vulkan %s] %s: %s", level, id, text);
    return VK_FALSE;
}

bool GpuContext::createInstance(bool wantValidation) {
    auto getProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (!getProc) {
        RF_ERR("SDL_Vulkan_GetVkGetInstanceProcAddr failed: %s", SDL_GetError());
        return false;
    }
    volkInitializeCustom(getProc);
    loaderVersion = volkGetInstanceVersion();
    if (loaderVersion < VK_API_VERSION_1_1) {
        RF_ERR("Vulkan loader reports %u.%u; RAVEFORM needs 1.1", VK_API_VERSION_MAJOR(loaderVersion),
               VK_API_VERSION_MINOR(loaderVersion));
        return false;
    }

    const char* extensions[16];
    u32 extensionCount = 0;
    Uint32 sdlCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlCount);
    for (Uint32 i = 0; i < sdlCount && extensionCount < 12; ++i) extensions[extensionCount++] = sdlExtensions[i];

    const char* layers[1];
    u32 layerCount = 0;
    bool layerSettings = false;
    if (wantValidation) {
        if (hasLayer(kValidationLayer) && hasInstanceExtension(nullptr, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            layers[layerCount++] = kValidationLayer;
            extensions[extensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            if (hasInstanceExtension(kValidationLayer, VK_EXT_LAYER_SETTINGS_EXTENSION_NAME)) {
                extensions[extensionCount++] = VK_EXT_LAYER_SETTINGS_EXTENSION_NAME;
                layerSettings = true;
            }
            validation_ = true;
        } else {
            RF_WARN("validation requested, but %s or %s is not available", kValidationLayer,
                    VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "RAVEFORM";
    app.applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1);
    app.pEngineName = "raveform";
    app.engineVersion = VK_MAKE_API_VERSION(0, 0, 0, 1);
    app.apiVersion = VK_API_VERSION_1_1;

    // Synchronization validation on top of the core checks: hazards here become real bugs on tilers.
    const VkBool32 enabled = VK_TRUE;
    VkLayerSettingEXT setting{};
    setting.pLayerName = kValidationLayer;
    setting.pSettingName = "validate_sync";
    setting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
    setting.valueCount = 1;
    setting.pValues = &enabled;
    VkLayerSettingsCreateInfoEXT settingsInfo{};
    settingsInfo.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
    settingsInfo.settingCount = 1;
    settingsInfo.pSettings = &setting;

    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = debugCallback;
    debugInfo.pUserData = this;

    // The chained messenger also reports problems inside vkCreateInstance / vkDestroyInstance.
    VkDebugUtilsMessengerCreateInfoEXT chainedDebug = debugInfo;
    chainedDebug.pNext = layerSettings ? &settingsInfo : nullptr;

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pNext = validation_ ? &chainedDebug : nullptr;
    info.pApplicationInfo = &app;
    info.enabledLayerCount = layerCount;
    info.ppEnabledLayerNames = layers;
    info.enabledExtensionCount = extensionCount;
    info.ppEnabledExtensionNames = extensions;
    RF_VK_TRY(vkCreateInstance(&info, nullptr, &instance));
    volkLoadInstance(instance);

    if (validation_) {
        RF_VK_TRY(vkCreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &messenger_));
        RF_LOG("vulkan: validation on (%s%s)", kValidationLayer, layerSettings ? " + sync" : "");
    }
    RF_LOG("vulkan: loader %u.%u.%u", VK_API_VERSION_MAJOR(loaderVersion), VK_API_VERSION_MINOR(loaderVersion),
           VK_API_VERSION_PATCH(loaderVersion));
    return true;
}

bool GpuContext::createDevice(VkSurfaceKHR surface, FramePacer& pacer) {
    VkPhysicalDevice devices[8];
    u32 count = countOf(devices);
    const VkResult enumResult = vkEnumeratePhysicalDevices(instance, &count, devices);
    if (enumResult != VK_SUCCESS && enumResult != VK_INCOMPLETE) {
        RF_ERR("vkEnumeratePhysicalDevices: %s", vkResultName(enumResult));
        return false;
    }

    int bestScore = -1;
    for (u32 d = 0; d < count; ++d) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(devices[d], &p);
        if (p.apiVersion < VK_API_VERSION_1_1) continue;
        VkQueueFamilyProperties families[16];
        u32 familyCount = countOf(families);
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &familyCount, families);
        for (u32 f = 0; f < familyCount; ++f) {
            if (!(families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(devices[d], f, surface, &present);
            if (!present) continue;
            const int score = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? 3
                              : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2
                                                                                       : 1;
            if (score > bestScore) {
                bestScore = score;
                physical = devices[d];
                queueFamily = f;
            }
            break;
        }
    }
    if (bestScore < 0) {
        RF_ERR("no Vulkan 1.1 GPU with a graphics queue that can present to this window");
        return false;
    }

    vkGetPhysicalDeviceProperties(physical, &props);
    vkGetPhysicalDeviceMemoryProperties(physical, &memoryProps);
    std::snprintf(apiVersionText, sizeof apiVersionText, "%u.%u.%u", VK_API_VERSION_MAJOR(props.apiVersion),
                  VK_API_VERSION_MINOR(props.apiVersion), VK_API_VERSION_PATCH(props.apiVersion));
    formatDriverVersion(props, driverVersionText, sizeof driverVersionText);
    RF_LOG("vulkan: GPU '%s' (%s), Vulkan %s, driver %s, vendor 0x%04x, queue family %u", props.deviceName,
           deviceTypeName(props.deviceType), apiVersionText, driverVersionText, props.vendorID, queueFamily);

    u32 availableCount = 0;
    vkEnumerateDeviceExtensionProperties(physical, nullptr, &availableCount, nullptr);
    auto* available = static_cast<VkExtensionProperties*>(std::malloc(sizeof(VkExtensionProperties) * (availableCount + 1)));
    vkEnumerateDeviceExtensionProperties(physical, nullptr, &availableCount, available);

    const char* enabled[8];
    u32 enabledCount = 0;
    enabled[enabledCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    enabledCount += pacer.deviceExtensions(physical, available, availableCount, enabled + enabledCount,
                                           countOf(enabled) - enabledCount);
    std::free(available);
    for (u32 i = 0; i < enabledCount; ++i) RF_LOG("vulkan: device extension %s", enabled[i]);

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    // No optional features: the Moto's Adreno 619 and the tablet's Mali lack many (TECH_SPEC 12.20).
    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queueInfo;
    info.enabledExtensionCount = enabledCount;
    info.ppEnabledExtensionNames = enabled;
    RF_VK_TRY(vkCreateDevice(physical, &info, nullptr, &device));
    volkLoadDevice(device);
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
    pacer.onDeviceCreated(physical, device, queue, queueFamily);
    return true;
}

void GpuContext::destroy(FramePacer& pacer) {
    if (device) {
        vkDeviceWaitIdle(device);
        pacer.onDeviceDestroying(device);
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (messenger_) {
        vkDestroyDebugUtilsMessengerEXT(instance, messenger_, nullptr);
        messenger_ = VK_NULL_HANDLE;
    }
    if (instance) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

i32 GpuContext::findMemoryType(u32 typeBits, VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred) const {
    i32 fallback = -1;
    for (u32 i = 0; i < memoryProps.memoryTypeCount; ++i) {
        if (!(typeBits & (1u << i))) continue;
        const VkMemoryPropertyFlags flags = memoryProps.memoryTypes[i].propertyFlags;
        if ((flags & required) != required) continue;
        if ((flags & preferred) == preferred) return static_cast<i32>(i);
        if (fallback < 0) fallback = static_cast<i32>(i);
    }
    return fallback;
}

bool GpuContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags required,
                              VkMemoryPropertyFlags preferred, GpuBuffer& out) {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    RF_VK_TRY(vkCreateBuffer(device, &info, nullptr, &out.buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, out.buffer, &req);
    const i32 type = findMemoryType(req.memoryTypeBits, required, preferred);
    if (type < 0) {
        RF_ERR("no memory type for buffer (bits 0x%x, required 0x%x)", req.memoryTypeBits, required);
        vkDestroyBuffer(device, out.buffer, nullptr);
        out.buffer = VK_NULL_HANDLE;
        return false;
    }
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = static_cast<u32>(type);
    RF_VK_TRY(vkAllocateMemory(device, &alloc, nullptr, &out.memory));
    RF_VK_TRY(vkBindBufferMemory(device, out.buffer, out.memory, 0));

    const VkMemoryPropertyFlags flags = memoryProps.memoryTypes[type].propertyFlags;
    out.size = size;
    out.coherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
    out.deviceLocal = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
    if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        RF_VK_TRY(vkMapMemory(device, out.memory, 0, VK_WHOLE_SIZE, 0, &out.mapped));
    }
    return true;
}

void GpuContext::destroyBuffer(GpuBuffer& b) {
    if (b.mapped) vkUnmapMemory(device, b.memory);
    if (b.buffer) vkDestroyBuffer(device, b.buffer, nullptr);
    if (b.memory) vkFreeMemory(device, b.memory, nullptr);
    b = GpuBuffer{};
}

void GpuContext::invalidate(const GpuBuffer& b) const {
    if (b.coherent || !b.mapped) return;
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = b.memory;
    range.offset = 0;
    range.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(device, 1, &range);
}

}  // namespace rf
