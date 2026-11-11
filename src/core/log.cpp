#include "core/log.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "core/types.h"

#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace rf {

namespace {

#if defined(__ANDROID__)
int androidPriority(LogLevel level) {
    switch (level) {
        case LogLevel::Error: return ANDROID_LOG_ERROR;
        case LogLevel::Warn: return ANDROID_LOG_WARN;
        default: return ANDROID_LOG_INFO;
    }
}
#else
const char* desktopPrefix(LogLevel level) {
    switch (level) {
        case LogLevel::Error: return "[error] ";
        case LogLevel::Warn: return "[warn] ";
        default: return "";
    }
}
#endif

void vlog(LogLevel level, const char* tag, const char* fmt, va_list ap) {
#if defined(__ANDROID__)
    __android_log_vprint(androidPriority(level), tag ? tag : "RAVEFORM", fmt, ap);
#else
    if (tag) {
        std::fputs(tag, stdout);
        std::fputc(' ', stdout);
    } else {
        std::fputs(desktopPrefix(level), stdout);
    }
    std::vfprintf(stdout, fmt, ap);
    std::fputc('\n', stdout);
    std::fflush(stdout);
#endif
}

}  // namespace

void logMessage(LogLevel level, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog(level, nullptr, fmt, ap);
    va_end(ap);
}

void logTagged(const char* tag, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog(LogLevel::Info, tag, fmt, ap);
    va_end(ap);
}

void fatal(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog(LogLevel::Error, nullptr, fmt, ap);
    va_end(ap);
    std::abort();
}

}  // namespace rf
