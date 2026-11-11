#pragma once
// Logging: logcat on Android (tag RAVEFORM), stdout on desktop. Never call from the audio callback.

namespace rf {

enum class LogLevel { Info, Warn, Error };

void logMessage(LogLevel level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

// One line under a fixed tag: the logcat tag on Android, a "TAG " prefix on desktop stdout.
// Scripts grep for these (RFBENCH, RFLIFE).
void logTagged(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

}  // namespace rf

#define RF_LOG(...) ::rf::logMessage(::rf::LogLevel::Info, __VA_ARGS__)
#define RF_WARN(...) ::rf::logMessage(::rf::LogLevel::Warn, __VA_ARGS__)
#define RF_ERR(...) ::rf::logMessage(::rf::LogLevel::Error, __VA_ARGS__)
