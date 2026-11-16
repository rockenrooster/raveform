#pragma once
// float -> IEEE 754 binary16, round to nearest. Used for compact instance attributes.

#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#include "core/types.h"

namespace rf {

inline u16 floatToHalf(float f) {
    u32 x;
    std::memcpy(&x, &f, sizeof x);
    const u32 sign = (x >> 16) & 0x8000u;
    const i32 exponent = static_cast<i32>((x >> 23) & 0xFFu) - 127 + 15;
    u32 mantissa = x & 0x7FFFFFu;

    if (((x >> 23) & 0xFFu) == 0xFFu) {  // inf / nan
        return static_cast<u16>(sign | 0x7C00u | (mantissa ? 0x200u : 0u));
    }
    if (exponent >= 31) return static_cast<u16>(sign | 0x7C00u);  // overflow -> inf
    if (exponent <= 0) {                                           // subnormal or zero
        if (exponent < -10) return static_cast<u16>(sign);
        mantissa |= 0x800000u;
        const u32 shift = static_cast<u32>(14 - exponent);
        u32 h = mantissa >> shift;
        if ((mantissa >> (shift - 1)) & 1u) ++h;
        return static_cast<u16>(sign | h);
    }
    u32 h = sign | (static_cast<u32>(exponent) << 10) | (mantissa >> 13);
    if (mantissa & 0x1000u) ++h;  // round; a carry correctly bumps the exponent
    return static_cast<u16>(h);
}

// Four at once (the instance writers' colours and widths). On arm64 one FCVTN does all four in hardware: the scalar path
// above is a few dozen instructions with branches per value, and the frame writes thousands (a 4x Cortex-A53 tablet spent
// a tenth of its frame there). Hardware rounds a tie to even where the scalar path rounds it up: a one-ulp difference on
// exact ties only. Elsewhere (the desktop, whose pictures are the references) the scalar path, unchanged.
inline void floatToHalf4(float a, float b, float c, float d, u16* out) {
#if defined(__aarch64__)
    const float v[4] = {a, b, c, d};
    vst1_u16(out, vreinterpret_u16_f16(vcvt_f16_f32(vld1q_f32(v))));
#else
    out[0] = floatToHalf(a);
    out[1] = floatToHalf(b);
    out[2] = floatToHalf(c);
    out[3] = floatToHalf(d);
#endif
}

}  // namespace rf
