#include "audio/platform_audio.h"

#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#include <jni.h>

#include "core/log.h"
#include "platform/android/android_glue.h"
#endif

namespace rf {

bool isBluetoothDeviceType(int type) {
    // AudioDeviceInfo: TYPE_BLUETOOTH_SCO 7, TYPE_BLUETOOTH_A2DP 8, TYPE_HEARING_AID 23, TYPE_BLE_HEADSET 26,
    // TYPE_BLE_SPEAKER 27, TYPE_BLE_BROADCAST 30.
    return type == 7 || type == 8 || type == 23 || type == 26 || type == 27 || type == 30;
}

#if defined(__ANDROID__)

namespace {
jclass g_class = nullptr;
jmethodID g_haptic = nullptr;
jmethodID g_deviceType = nullptr;

JNIEnv* env() { return static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv()); }

void clearException(JNIEnv* e) {
    if (e->ExceptionCheck()) e->ExceptionClear();
}
}  // namespace

bool platformAudioInit() {
    if (g_class) return true;
    JNIEnv* e = env();
    jobject activity = android::activityRef();
    if (!e || !activity) return false;
    jclass local = e->GetObjectClass(activity);
    g_class = static_cast<jclass>(e->NewGlobalRef(local));
    e->DeleteLocalRef(local);
    g_haptic = e->GetStaticMethodID(g_class, "haptic", "(I)V");
    clearException(e);
    g_deviceType = e->GetStaticMethodID(g_class, "audioDeviceType", "(I)I");
    clearException(e);
    if (!g_haptic || !g_deviceType) RF_WARN("audio: RaveformActivity lacks the haptic / audioDeviceType helpers");
    return g_haptic != nullptr;
}

void platformHaptic(HapticKind kind, bool strong) {
    if (!g_haptic) return;
    JNIEnv* e = env();
    if (!e) return;
    e->CallStaticVoidMethod(g_class, g_haptic, static_cast<jint>(static_cast<int>(kind) + (strong ? 3 : 0)));
    clearException(e);
}

int platformAudioDeviceType(i32 deviceId) {
    if (!g_deviceType) return 0;
    JNIEnv* e = env();
    if (!e) return 0;
    const jint t = e->CallStaticIntMethod(g_class, g_deviceType, static_cast<jint>(deviceId));
    if (e->ExceptionCheck()) {
        e->ExceptionClear();
        return 0;
    }
    return static_cast<int>(t);
}

#else

bool platformAudioInit() { return false; }
void platformHaptic(HapticKind, bool) {}
int platformAudioDeviceType(i32) { return 0; }

#endif

}  // namespace rf
