#include "platform/cpu_affinity.h"

#include <cstdio>

#if defined(__linux__)
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#endif

namespace rf {

#if defined(__linux__)
namespace {

// One unsigned number from a sysfs file; 0 when missing or unreadable.
u64 readNumber(const char* path) {
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    char buf[32];
    const ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';
    u64 v = 0;
    for (const char* c = buf; *c >= '0' && *c <= '9'; ++c) v = v * 10 + static_cast<u64>(*c - '0');
    return v;
}

u64 cpuScore(int cpu, const char* file) {
    char path[96];
    std::snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%d/%s", cpu, file);
    return readNumber(path);
}

}  // namespace
#endif

u32 pinToFastCores(char* desc, size_t descCapacity) {
    if (descCapacity) desc[0] = '\0';
#if defined(__linux__)
    constexpr int kMaxCpus = 32;
    const long configured = sysconf(_SC_NPROCESSORS_CONF);
    const int n = configured <= 0 ? 0 : (configured < kMaxCpus ? static_cast<int>(configured) : kMaxCpus);
    u64 score[kMaxCpus] = {};
    // Prefer the scheduler's capacity; fall back to the maximum frequency when any CPU lacks it.
    const char* sources[2] = {"cpu_capacity", "cpufreq/cpuinfo_max_freq"};
    for (const char* source : sources) {
        bool complete = n > 0;
        for (int c = 0; c < n; ++c) {
            score[c] = cpuScore(c, source);
            complete = complete && score[c] != 0;
        }
        if (complete) break;
    }
    u64 lo = ~0ull;
    u64 hi = 0;
    for (int c = 0; c < n; ++c) {
        if (!score[c]) continue;
        lo = score[c] < lo ? score[c] : lo;
        hi = score[c] > hi ? score[c] : hi;
    }
    if (hi == 0 || lo == hi) return 0;  // unknown topology or symmetric CPUs

    cpu_set_t set;
    CPU_ZERO(&set);
    u32 count = 0;
    size_t used = 0;
    for (int c = 0; c < n; ++c) {
        if (score[c] <= lo) continue;
        CPU_SET(c, &set);
        ++count;
        if (used + 4 < descCapacity)
            used += static_cast<size_t>(std::snprintf(desc + used, descCapacity - used, "%s%d", used ? "," : "", c));
    }
    if (sched_setaffinity(0, sizeof set, &set) != 0) {  // e.g. those cores are offline right now
        if (descCapacity) desc[0] = '\0';
        return 0;
    }
    return count;
#else
    (void)descCapacity;
    return 0;
#endif
}

}  // namespace rf
