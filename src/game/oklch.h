#pragma once
// OKLCH -> linear sRGB (Ottosson's OKLab) at full chroma: the most saturated colour of a lightness and hue inside the gamut, as the
// design pages give colours ("OKLCH [L, hue] at full chroma": design/modes/README.md 10).

#include <cmath>

#include "core/math.h"

namespace rf {

inline Vec3 oklabToLinear(float L, float A, float B) {
    const float l_ = L + 0.3963377774f * A + 0.2158037573f * B, m_ = L - 0.1055613458f * A - 0.0638541728f * B,
                s_ = L - 0.0894841775f * A - 1.2914855480f * B;
    const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    return {4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s, -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
            -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s};
}

// `hueDeg` in degrees.
inline Vec3 oklchFull(float L, float hueDeg) {
    const float h = hueDeg * 0.01745329252f;
    float lo = 0.0f, hi = 0.4f;
    for (int k = 0; k < 24; ++k) {
        const float c = 0.5f * (lo + hi);
        const Vec3 v = oklabToLinear(L, c * std::cos(h), c * std::sin(h));
        const bool in = v.x >= 0.0f && v.x <= 1.0f && v.y >= 0.0f && v.y <= 1.0f && v.z >= 0.0f && v.z <= 1.0f;
        (in ? lo : hi) = c;
    }
    const Vec3 v = oklabToLinear(L, lo * std::cos(h), lo * std::sin(h));
    const auto c01 = [](float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); };
    return {c01(v.x), c01(v.y), c01(v.z)};
}

}  // namespace rf
