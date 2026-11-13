#include "game/lattice.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace rf {

namespace {
constexpr float kAnchor = 30.0f;    // pull back to rest, 1/s^2
constexpr float kCouple = 140.0f;   // neighbour coupling, 1/s^2
constexpr float kDamping = 5.0f;    // 1/s
constexpr float kSubstep = 1.0f / 120.0f;
constexpr float kWellPeakWu = 60.0f;
constexpr float kWellGoneWu = 200.0f;

float smooth(float a, float b, float x) {
    const float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}  // namespace

Lattice::~Lattice() {
    std::free(d_);
    std::free(v_);
    std::free(acc_);
    std::free(px_);
    std::free(col_);
    std::free(hw_);
    std::free(flat_);
}

bool Lattice::init(float spacingWu, float viewW, float viewH) {
    spacing_ = spacingWu;
    cols_ = static_cast<int>(std::ceil(viewW / spacingWu)) + 5;
    rows_ = static_cast<int>(std::ceil(viewH / spacingWu)) + 5;
    const size_t n = static_cast<size_t>(cols_) * static_cast<size_t>(rows_);
    std::free(d_);
    std::free(v_);
    std::free(acc_);
    std::free(px_);
    std::free(col_);
    std::free(hw_);
    std::free(flat_);
    d_ = static_cast<Vec2*>(std::calloc(n, sizeof(Vec2)));
    v_ = static_cast<Vec2*>(std::calloc(n, sizeof(Vec2)));
    acc_ = static_cast<Vec2*>(std::calloc(n, sizeof(Vec2)));
    px_ = static_cast<Vec2*>(std::calloc(n, sizeof(Vec2)));
    col_ = static_cast<Vec3*>(std::calloc(n, sizeof(Vec3)));
    hw_ = static_cast<float*>(std::calloc(n, sizeof(float)));
    flat_ = static_cast<u8*>(std::calloc(n, 1));
    placed_ = false;
    return d_ && v_ && acc_ && px_ && col_ && hw_ && flat_;
}

void Lattice::recentre(Vec2 camera) {
    const int ni = static_cast<int>(std::floor(camera.x / spacing_)) - cols_ / 2;
    const int nj = static_cast<int>(std::floor(camera.y / spacing_)) - rows_ / 2;
    if (!placed_) {
        i0_ = ni;
        j0_ = nj;
        placed_ = true;
        return;
    }
    // Columns and rows that enter the window start at rest.
    while (i0_ < ni) {
        const int i = i0_ + cols_;  // entering on the right, reusing the leftmost column's slots
        for (int j = j0_; j < j0_ + rows_; ++j) d_[slot(i, j)] = v_[slot(i, j)] = Vec2{};
        ++i0_;
    }
    while (i0_ > ni) {
        --i0_;
        for (int j = j0_; j < j0_ + rows_; ++j) d_[slot(i0_, j)] = v_[slot(i0_, j)] = Vec2{};
    }
    while (j0_ < nj) {
        const int j = j0_ + rows_;
        for (int i = i0_; i < i0_ + cols_; ++i) d_[slot(i, j)] = v_[slot(i, j)] = Vec2{};
        ++j0_;
    }
    while (j0_ > nj) {
        --j0_;
        for (int i = i0_; i < i0_ + cols_; ++i) d_[slot(i, j0_)] = v_[slot(i, j0_)] = Vec2{};
    }
}

void Lattice::impulse(Vec2 at, float strength, float radiusWu) {
    if (!placed_ || strength == 0.0f) return;
    const float reach = 3.0f * radiusWu;
    const int ia = static_cast<int>(std::floor((at.x - reach) / spacing_));
    const int ib = static_cast<int>(std::ceil((at.x + reach) / spacing_));
    const int ja = static_cast<int>(std::floor((at.y - reach) / spacing_));
    const int jb = static_cast<int>(std::ceil((at.y + reach) / spacing_));
    const float inv = 1.0f / (radiusWu * radiusWu);
    for (int j = ja > j0_ ? ja : j0_; j <= jb && j < j0_ + rows_; ++j) {
        for (int i = ia > i0_ ? ia : i0_; i <= ib && i < i0_ + cols_; ++i) {
            const u32 s = slot(i, j);
            const Vec2 p = Vec2{static_cast<float>(i) * spacing_, static_cast<float>(j) * spacing_} + d_[s];
            const Vec2 e = p - at;
            const float r2 = dot(e, e);
            if (r2 < 1e-4f) continue;
            const float r = std::sqrt(r2);
            v_[s] = v_[s] + e * (strength * std::exp(-r2 * inv) / r);
        }
    }
}

void Lattice::step(float dt) {
    if (!placed_) return;
    stepRemainder_ += dt;
    int substeps = 0;
    while (stepRemainder_ >= kSubstep && substeps < 4) {
        stepRemainder_ -= kSubstep;
        ++substeps;
        for (int j = j0_; j < j0_ + rows_; ++j) {
            for (int i = i0_; i < i0_ + cols_; ++i) {
                const u32 s = slot(i, j);
                const Vec2 d = d_[s];
                Vec2 nb{};
                if (i > i0_) nb = nb + d_[slot(i - 1, j)];
                if (i + 1 < i0_ + cols_) nb = nb + d_[slot(i + 1, j)];
                if (j > j0_) nb = nb + d_[slot(i, j - 1)];
                if (j + 1 < j0_ + rows_) nb = nb + d_[slot(i, j + 1)];
                Vec2 a = d * (-kAnchor - 4.0f * kCouple) + nb * kCouple - v_[s] * kDamping;
                for (u32 w = 0; w < wellCount_; ++w) {
                    const Vec2 e = Vec2{static_cast<float>(i) * spacing_, static_cast<float>(j) * spacing_} + d - wells_[w];
                    const float r = length(e);
                    if (r < 1e-3f || r >= kWellGoneWu) continue;
                    float f = r <= kWellPeakWu ? r / kWellPeakWu : (kWellGoneWu - r) / (kWellGoneWu - kWellPeakWu);
                    f = f * f * (3.0f - 2.0f * f);
                    a = a - e * (kAnchor * wellPull_ * f / r);
                }
                acc_[s] = a;
            }
        }
        const u32 n = static_cast<u32>(cols_ * rows_);
        for (u32 s = 0; s < n; ++s) {
            v_[s] = v_[s] + acc_[s] * kSubstep;
            d_[s] = d_[s] + v_[s] * kSubstep;
        }
    }
    if (substeps == 4) stepRemainder_ = 0.0f;  // a long frame: do not try to catch up
}

u32 Lattice::draw(CapsuleBatch& out, const LatticeView& view, const LatticeStyle& st) {
    if (!placed_) return 0;
    const Vec3 near = st.grid * (1.0f - st.tintNearPlayerMix) + st.player * st.tintNearPlayerMix;
    const Vec3 far = st.grid * (1.0f - st.tintFarThirdHueMix) + st.thirdHue * st.tintFarThirdHueMix;
    const Vec3 warm = mix(st.grid, st.gridHot, st.warmTowardHot) * st.warmIntensity;
    const float cos20 = std::cos(20.0f * kPi / 180.0f);

    // Pass 1: every node's drawn position, colour and width.
    heat_ = HeatStats{};
    for (int j = j0_; j < j0_ + rows_; ++j) {
        for (int i = i0_; i < i0_ + cols_; ++i) {
            const u32 s = slot(i, j);
            Vec2 p = Vec2{static_cast<float>(i) * spacing_, static_cast<float>(j) * spacing_} + d_[s];
            float heat = length(d_[s]);
            const float springs = heat;
            const Vec2 fromPlayer = p - view.player;
            const float dist = length(fromPlayer);
            if (dist > 1e-3f) {
                for (u32 r = 0; r < view.rippleCount; ++r) {
                    const LatticeView::Ripple& rp = view.ripples[r];
                    const float k = (dist - rp.radiusWu) / rp.widthWu;
                    if (k * k > 9.0f) continue;
                    const float push = rp.amplitudeWu * std::exp(-k * k);
                    p = p + fromPlayer * (push / dist);
                    heat += push;
                }
            }
            px_[s] = (p - view.camera) * view.pxPerWu + view.screenCentre;
            if (px_[s].x >= view.widthPx / 3.0f && px_[s].x <= 2.0f * view.widthPx / 3.0f && px_[s].y >= 0.0f && px_[s].y <= view.heightPx) {
                ++heat_.nodes;
                heat_.fromSprings += springs;
                heat_.fromRipples += heat - springs;
                if (heat >= st.warmFromWu) ++heat_.warm;
                if (heat >= st.hotFromWu) ++heat_.hot;
            }
            Vec3 rest = st.grid;
            if (st.tint) {
                const float t = clampf(dist / st.tintRadiusWu, 0.0f, 1.0f) * 2.0f;
                rest = t < 1.0f ? mix(near, st.grid, t) : mix(st.grid, far, t - 1.0f);
            }
            const float f1 = smooth(st.warmFromWu - 1.5f, st.warmFromWu, heat);
            const float f2 = smooth(st.hotFromWu - 2.0f, st.hotFromWu, heat);
            const float peak = clampf((heat - st.hotFromWu) / st.hotFromWu, 0.0f, 1.0f);
            const Vec3 hot = st.gridHot * (st.hotIntensity + (st.hotPeakIntensity - st.hotIntensity) * peak);
            const Vec3 atRest = rest * st.restIntensity;
            Vec3 c = mix(mix(atRest, warm, f1), hot, f2);
            c = atRest + (c - atRest) * st.heatGain;  // only the heat above the rest level dims
            const float w = st.restWidth + (st.warmWidth - st.restWidth) * f1 + (st.hotWidth - st.warmWidth) * f2;
            const float shimmer = 1.0f + view.hatShimmer * (((i + j) & 1) ? 1.0f : -1.0f);
            col_[s] = c * shimmer;
            hw_[s] = 0.5f * w * view.refPx;
        }
    }
    // Pass 2: where the lines run straight through a node (look.md §4 joints), both ends are flat.
    for (int j = j0_; j < j0_ + rows_; ++j) {
        for (int i = i0_; i < i0_ + cols_; ++i) {
            const u32 s = slot(i, j);
            u8 f = 0;
            if (i > i0_ && i + 1 < i0_ + cols_) {
                const Vec2 a = px_[s] - px_[slot(i - 1, j)], b = px_[slot(i + 1, j)] - px_[s];
                const float la = length(a), lb = length(b);
                if (la > 1e-3f && lb > 1e-3f && dot(a, b) >= cos20 * la * lb) f |= 1u;
            } else {
                f |= 1u;
            }
            if (j > j0_ && j + 1 < j0_ + rows_) {
                const Vec2 a = px_[s] - px_[slot(i, j - 1)], b = px_[slot(i, j + 1)] - px_[s];
                const float la = length(a), lb = length(b);
                if (la > 1e-3f && lb > 1e-3f && dot(a, b) >= cos20 * la * lb) f |= 2u;
            } else {
                f |= 2u;
            }
            flat_[s] = f;
        }
    }
    // Pass 3: visible segments.
    const float pad = 8.0f;
    const auto outside = [&](Vec2 p) {
        return p.x < -pad || p.y < -pad || p.x > view.widthPx + pad || p.y > view.heightPx + pad;
    };
    // M3: inside the arena only (rest positions), when bounded.
    const auto inArena = [&](int i, int j) {
        if (!view.bounded) return true;
        const float x = static_cast<float>(i) * spacing_, y = static_cast<float>(j) * spacing_;
        return x >= view.boundsMin.x && x <= view.boundsMax.x && y >= view.boundsMin.y && y <= view.boundsMax.y;
    };
    u32 n = 0;
    for (int j = j0_; j < j0_ + rows_; ++j) {
        for (int i = i0_; i < i0_ + cols_; ++i) {
            const u32 s = slot(i, j);
            const Vec2 a = px_[s];
            const bool aOut = outside(a);
            if (!inArena(i, j)) continue;
            if (i + 1 < i0_ + cols_ && inArena(i + 1, j)) {
                const u32 t = slot(i + 1, j);
                if (!(aOut && outside(px_[t]))) {
                    const u32 flags = ((flat_[s] & 1u) ? kFlatStart : 0u) | ((flat_[t] & 1u) ? kFlatEnd : 0u);
                    out.line(a, px_[t], hw_[s] > hw_[t] ? hw_[s] : hw_[t], col_[s], col_[t], flags);
                    ++n;
                }
            }
            if (j + 1 < j0_ + rows_ && inArena(i, j + 1)) {
                const u32 t = slot(i, j + 1);
                if (!(aOut && outside(px_[t]))) {
                    const u32 flags = ((flat_[s] & 2u) ? kFlatStart : 0u) | ((flat_[t] & 2u) ? kFlatEnd : 0u);
                    out.line(a, px_[t], hw_[s] > hw_[t] ? hw_[s] : hw_[t], col_[s], col_[t], flags);
                    ++n;
                }
            }
        }
    }
    return n;
}

}  // namespace rf
