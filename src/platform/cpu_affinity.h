#pragma once
// Thread placement on big.LITTLE phones (Android/Linux). Left to the scheduler, the light main thread
// of the M0 build lived on the Moto's little cores next to SurfaceFlinger, the composer and the audio
// server, and the frames that missed vsync were the ones in which it waited 4-13 ms for a CPU
// (PROGRESS.md, M0 pacing). Pinning it to the faster cores keeps it away from that traffic.

#include "core/types.h"

namespace rf {

// Pins the calling thread to every CPU whose capacity is above the device's lowest, i.e. all but the
// little cluster (capacity from /sys/devices/system/cpu/cpuN/cpu_capacity, else cpuinfo_max_freq).
// Threads it creates afterwards inherit the mask. Android resets affinities when it moves the app
// between cpusets (background / foreground), so call it again after every resume.
// Returns the number of CPUs in the new mask; 0 = unchanged (symmetric CPUs, unreadable topology,
// the cores are offline, or not Linux). `desc` receives the CPU list, e.g. "6,7".
u32 pinToFastCores(char* desc, size_t descCapacity);

}  // namespace rf
