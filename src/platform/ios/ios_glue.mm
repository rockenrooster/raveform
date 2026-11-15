// iOS glue (docs/IOS.md), Objective-C++: UIKit's haptics, the device's model, the Documents folder (the refresh rate is SDL's).
// Built only for iOS (CMakeLists.txt, RF_IOS); never on the main thread's hot path except the haptic taps, which UIKit takes
// from the main thread (the game's loop runs there under SDL on iOS).

#include "platform/ios/ios_glue.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <sys/sysctl.h>

#include <cstdio>
#include <cstring>

#include <vulkan/vulkan_core.h>

// MoltenVK is linked statically (docs/IOS.md): SDL's Vulkan loader finds its entry point in the app's own image (as it does for
// a static MoltenVK), and this reference (with the link's -u) keeps the linker from leaving it out.
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName);
__attribute__((used)) static PFN_vkGetInstanceProcAddr const kMoltenVkEntry = &vkGetInstanceProcAddr;

namespace rf::ios {

namespace {
UIImpactFeedbackGenerator* g_gen[3] = {nil, nil, nil};

UIImpactFeedbackGenerator* generator(int style) {
    if (style < 0) style = 0;
    if (style > 2) style = 2;
    if (!g_gen[style]) {
        const UIImpactFeedbackStyle s = style == 0 ? UIImpactFeedbackStyleLight : (style == 1 ? UIImpactFeedbackStyleMedium : UIImpactFeedbackStyleHeavy);
        g_gen[style] = [[UIImpactFeedbackGenerator alloc] initWithStyle:s];
        [g_gen[style] prepare];
    }
    return g_gen[style];
}
}  // namespace

void haptic(int kind, bool strong) {
    const int style = strong ? (kind < 2 ? kind + 1 : 2) : kind;
    UIImpactFeedbackGenerator* g = generator(style);
    [g impactOccurredWithIntensity:(strong ? 1.0 : 0.8)];
    [g prepare];
}

void deviceModel(char* out, size_t capacity) {
    if (!out || !capacity) return;
    out[0] = '\0';
    size_t size = 0;
    if (sysctlbyname("hw.machine", nullptr, &size, nullptr, 0) != 0 || size == 0) return;
    char buf[65] = {};  // (one more than sysctl may fill: always terminated)
    size = size < sizeof buf - 1 ? size : sizeof buf - 1;
    if (sysctlbyname("hw.machine", buf, &size, nullptr, 0) != 0) return;
    std::snprintf(out, capacity, "%s", buf);
}

void documentsPath(char* out, size_t capacity) {
    if (!out || !capacity) return;
    out[0] = '\0';
    @autoreleasepool {
        NSArray<NSString*>* dirs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        if (dirs.count == 0) return;
        std::snprintf(out, capacity, "%s/", [dirs[0] fileSystemRepresentation]);
    }
}

}  // namespace rf::ios
