#include "platform/android/android_glue.h"

#include <SDL3/SDL_system.h>
#include <sys/system_properties.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>

namespace rf::android {

namespace {
std::mutex g_surfaceMutex;
std::condition_variable g_surfaceReleased;
bool g_surfaceInUse = false;
jobject g_activity = nullptr;
}  // namespace

jobject activityRef() {
    if (!g_activity) {
        auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        auto local = static_cast<jobject>(SDL_GetAndroidActivity());
        if (env && local) {
            g_activity = env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
        }
    }
    return g_activity;
}

void setSurfaceInUse(bool inUse) {
    {
        std::lock_guard<std::mutex> lock(g_surfaceMutex);
        g_surfaceInUse = inUse;
    }
    g_surfaceReleased.notify_all();
}

float displayRefreshHz() {
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = activityRef();
    if (!env || !activity) return 0.0f;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetStaticMethodID(cls, "getDisplayRefreshRate", "()F");
    float hz = 0.0f;
    if (method) hz = env->CallStaticFloatMethod(cls, method);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        hz = 0.0f;
    }
    env->DeleteLocalRef(cls);
    return hz;
}

void systemProperty(const char* name, char* out, size_t capacity) {
    char value[PROP_VALUE_MAX] = {};
    __system_property_get(name, value);
    std::snprintf(out, capacity, "%s", value);
}

}  // namespace rf::android

// RaveformActivity.nativeWaitSurfaceReleased(int): called on the UI thread from surfaceDestroyed().
extern "C" JNIEXPORT jboolean JNICALL Java_dev_raveform_game_RaveformActivity_nativeWaitSurfaceReleased(JNIEnv*, jclass,
                                                                                                       jint timeoutMs) {
    std::unique_lock<std::mutex> lock(rf::android::g_surfaceMutex);
    const bool released = rf::android::g_surfaceReleased.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                                                  [] { return !rf::android::g_surfaceInUse; });
    return released ? JNI_TRUE : JNI_FALSE;
}
