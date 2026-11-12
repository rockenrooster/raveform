#pragma once
// Basic types and the always-on assert. Engine code is C++20 without exceptions or RTTI.

#include <cstddef>
#include <cstdint>

namespace rf {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

template <typename T, std::size_t N>
constexpr u32 countOf(const T (&)[N]) {
    return static_cast<u32>(N);
}

[[noreturn]] void fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace rf

#define RF_ASSERT(cond)                                                                   \
    do {                                                                                  \
        if (!(cond)) ::rf::fatal("assert failed: %s (%s:%d)", #cond, __FILE__, __LINE__); \
    } while (0)
