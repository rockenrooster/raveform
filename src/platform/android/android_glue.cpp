#include "platform/android/android_glue.h"

#include <SDL3/SDL_system.h>
#include <sys/system_properties.h>

#include <atomic>
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
std::atomic<int> g_latestBuild{0};  // (the updater: set on RaveformActivity's thread, read by the title)
std::atomic<int> g_updatePercent{-1};  // (the in-app updater's download: set on Android's main thread, read by the title)
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

namespace {
// A static method of RaveformActivity: "(Z)V", "()I" or "()V".
template <typename F>
void callActivity(const char* name, const char* sig, F&& call) {
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = activityRef();
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetStaticMethodID(cls, name, sig);
    if (method) call(env, cls, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
}
}  // namespace

void setMulticastLock(bool on) {
    callActivity("setMulticastLock", "(Z)V", [&](JNIEnv* env, jclass cls, jmethodID m) { env->CallStaticVoidMethod(cls, m, on ? JNI_TRUE : JNI_FALSE); });
}

int localNetworkState() {
    int state = 0;
    callActivity("localNetworkState", "()I", [&](JNIEnv* env, jclass cls, jmethodID m) { state = env->CallStaticIntMethod(cls, m); });
    return state;
}

void requestLocalNetwork() {
    callActivity("requestLocalNetwork", "()V", [&](JNIEnv* env, jclass cls, jmethodID m) { env->CallStaticVoidMethod(cls, m); });
}

void checkForUpdate() {
    callActivity("checkForUpdate", "()V", [&](JNIEnv* env, jclass cls, jmethodID m) { env->CallStaticVoidMethod(cls, m); });
}

int latestBuild() { return g_latestBuild.load(std::memory_order_relaxed); }

void startUpdate() {
    callActivity("startUpdate", "()V", [&](JNIEnv* env, jclass cls, jmethodID m) { env->CallStaticVoidMethod(cls, m); });
}

int updatePercent() { return g_updatePercent.load(std::memory_order_relaxed); }

}  // namespace rf::android

// RaveformActivity.nativeUpdateProgress(float): the in-app update's download, 0..1, below 0 when none runs (a failure).
extern "C" JNIEXPORT void JNICALL Java_dev_raveform_game_RaveformActivity_nativeUpdateProgress(JNIEnv*, jclass, jfloat fraction) {
    rf::android::g_updatePercent.store(fraction < 0.0f ? -1 : static_cast<int>(fraction * 100.0f), std::memory_order_relaxed);
}

// RaveformActivity.nativeUpdateFound(int): the latest GitHub release's build number, from its update thread.
extern "C" JNIEXPORT void JNICALL Java_dev_raveform_game_RaveformActivity_nativeUpdateFound(JNIEnv*, jclass, jint build) {
    rf::android::g_latestBuild.store(static_cast<int>(build), std::memory_order_relaxed);
}

// RaveformActivity.nativeWaitSurfaceReleased(int): called on the UI thread from surfaceDestroyed().
extern "C" JNIEXPORT jboolean JNICALL Java_dev_raveform_game_RaveformActivity_nativeWaitSurfaceReleased(JNIEnv*, jclass,
                                                                                                       jint timeoutMs) {
    std::unique_lock<std::mutex> lock(rf::android::g_surfaceMutex);
    const bool released = rf::android::g_surfaceReleased.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                                                  [] { return !rf::android::g_surfaceInUse; });
    return released ? JNI_TRUE : JNI_FALSE;
}
