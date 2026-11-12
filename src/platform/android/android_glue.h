#pragma once
// Android-only helpers around SDL's activity: JNI handles, the surface hand-shake with
// RaveformActivity.RaveformSurface.surfaceDestroyed(), and device facts for logs and benchmarks.

#if defined(__ANDROID__)

#include <jni.h>

#include <cstddef>

namespace rf::android {

// Global reference to the SDLActivity instance (created on first use, kept for the process lifetime).
jobject activityRef();

// The render thread reports whether it holds a Vulkan surface on the ANativeWindow. The UI thread's
// surfaceDestroyed() blocks until this is false, so Android never destroys a surface we still use.
void setSurfaceInUse(bool inUse);

// Display.getRefreshRate() through RaveformActivity. JNI: never call from the frame loop.
float displayRefreshHz();

// __system_property_get, e.g. "ro.product.model".
void systemProperty(const char* name, char* out, size_t capacity);

// M4 stage B (TECH_SPEC 8.3), through RaveformActivity (JNI: the lobby's screens only, never the run): the Wi-Fi
// multicast lock while the join screen listens for rooms; the local network's permission on Android 17 for an app
// that targets it (0 allowed or not needed, 1 to be asked, 2 refused, 3 the system's dialog is up) and its request.
void setMulticastLock(bool on);
int localNetworkState();
void requestLocalNetwork();

}  // namespace rf::android

#endif
