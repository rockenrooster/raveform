#pragma once
// Vulkan through volk (no link-time dependency on the loader). 1.1 core only (PLAN 3.2).

#include <volk.h>

#include "core/log.h"

namespace rf {
const char* vkResultName(VkResult result);
}

// For functions returning bool: logs the failing call and returns false.
#define RF_VK_TRY(expr)                                                                                \
    do {                                                                                               \
        const VkResult rf_vk_result_ = (expr);                                                         \
        if (rf_vk_result_ != VK_SUCCESS) {                                                             \
            RF_ERR("%s failed: %s (%s:%d)", #expr, ::rf::vkResultName(rf_vk_result_), __FILE__, __LINE__); \
            return false;                                                                              \
        }                                                                                              \
    } while (0)
