#pragma once
// The band's few platform calls (main thread only, never the audio callback): beat haptics and the type
// of the output device, through RaveformActivity on Android (JNI); no-ops on the desktop.

#include "core/types.h"

namespace rf {

enum class HapticKind : int { Light = 0, Medium = 1, Heavy = 2 };  // + 3 over JNI for STRONG

// Caches the JNI handles (start-up). False when the helper is missing (haptics then stay off).
bool platformAudioInit();
// One haptic tick, posted to the UI thread (RaveformActivity.haptic). M3 stage C: strong is the STRONG setting,
// played with more amplitude where the vibrator has amplitude control, else longer.
void platformHaptic(HapticKind kind, bool strong = false);
// android.media.AudioDeviceInfo type of an output device id (0 when unknown or on the desktop).
int platformAudioDeviceType(i32 deviceId);
// Bluetooth (A2DP, SCO, LE, hearing aid) by AudioDeviceInfo type.
bool isBluetoothDeviceType(int type);

}  // namespace rf
