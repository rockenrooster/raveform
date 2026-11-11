#include "gfx/capsule.h"

#include <cstring>

#include "core/half.h"

namespace rf {

void CapsuleBatch::line(Vec2 a, Vec2 b, const Stroke& s) {
    if (count_ >= cap_) {
        ++dropped_;
        return;
    }
    float skirtQuarters = s.skirt * 4.0f;
    if (skirtQuarters > 255.0f) skirtQuarters = 255.0f;
    if (skirtQuarters < 0.0f) skirtQuarters = 0.0f;
    CapsuleInstance inst;
    inst.p0x = a.x;
    inst.p0y = a.y;
    inst.p1x = b.x;
    inst.p1y = b.y;
    inst.radius = floatToHalf(s.radius);
    inst.intensity = floatToHalf(s.intensity);
    inst.color = s.color;
    inst.flags = static_cast<u32>(skirtQuarters + 0.5f);
    inst.reserved = 0;
    // One sequential 32-byte write: the destination is write-combined GPU memory.
    std::memcpy(mem_ + count_, &inst, sizeof inst);
    ++count_;
}

void CapsuleBatch::polygon(Vec2 c, float radius, u32 sides, float rotation, const Stroke& s) {
    if (sides < 2) return;
    Vec2 prev = c + polar(radius, rotation);
    for (u32 i = 1; i <= sides; ++i) {
        const Vec2 next = c + polar(radius, rotation + kTau * static_cast<float>(i) / static_cast<float>(sides));
        line(prev, next, s);
        prev = next;
    }
}

}  // namespace rf
