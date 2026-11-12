#pragma once
// The uniform spatial grid of TECH_SPEC 7.2: 64 wu cells over the whole arena (150 x 85), rebuilt every
// tick by a counting sort of the items' positions. Items are indices into a pool's dense arrays; a cell
// lists its items in ascending index order, so every query visits them in a stable order. No
// allocation: the arrays are sized for the largest pool.

#include <cmath>

#include "sim/sim_types.h"

namespace rf::sim {

class SpatialGrid {
public:
    static constexpr u32 kCells = kGridCols * kGridRows;
    static constexpr u32 kMaxItems = kMaxEnemies;

    // Cell of a world position (positions outside the arena land in the border cells).
    static u32 cellX(float x) {
        const float c = std::floor((x + kArenaHalfW) / kCellWu);
        return c < 0.0f ? 0u : (c >= static_cast<float>(kGridCols) ? kGridCols - 1 : static_cast<u32>(c));
    }
    static u32 cellY(float y) {
        const float c = std::floor((y + kArenaHalfH) / kCellWu);
        return c < 0.0f ? 0u : (c >= static_cast<float>(kGridRows) ? kGridRows - 1 : static_cast<u32>(c));
    }

    // Sorts items 0..count-1 into their cells; items with include[i] == 0 are left out (include may be
    // null: everything).
    void build(const float* x, const float* y, u32 count, const u8* include);

    // f(item) for every item in the cells that overlap [x0, x1] x [y0, y1].
    template <typename F>
    void query(float x0, float y0, float x1, float y1, F&& f) const {
        const u32 cx0 = cellX(x0), cx1 = cellX(x1), cy0 = cellY(y0), cy1 = cellY(y1);
        for (u32 cy = cy0; cy <= cy1; ++cy) {
            for (u32 cx = cx0; cx <= cx1; ++cx) {
                const u32 c = cy * kGridCols + cx;
                for (u32 k = start_[c]; k < start_[c + 1]; ++k) f(items_[k]);
            }
        }
    }

    u32 itemCount() const { return start_[kCells]; }
    u32 cellCount(u32 cx, u32 cy) const {
        const u32 c = cy * kGridCols + cx;
        return start_[c + 1] - start_[c];
    }

private:
    u32 start_[kCells + 1] = {};
    u32 items_[kMaxItems] = {};
    u32 cellOf_[kMaxItems] = {};
};

}  // namespace rf::sim
