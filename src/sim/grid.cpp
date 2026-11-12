#include "sim/grid.h"

#include <cstring>

namespace rf::sim {

void SpatialGrid::build(const float* x, const float* y, u32 count, const u8* include) {
    if (count > kMaxItems) count = kMaxItems;
    // Counting sort: counts per cell, prefix sums, then the items in ascending order.
    std::memset(start_, 0, sizeof start_);
    for (u32 i = 0; i < count; ++i) {
        if (include && !include[i]) {
            cellOf_[i] = kCells;
            continue;
        }
        const u32 c = cellY(y[i]) * kGridCols + cellX(x[i]);
        cellOf_[i] = c;
        ++start_[c + 1];
    }
    for (u32 c = 0; c < kCells; ++c) start_[c + 1] += start_[c];
    // Fill with a running cursor per cell: reuse the counts by walking items in order and writing at
    // start_[c] (then shifting back).
    for (u32 i = 0; i < count; ++i) {
        const u32 c = cellOf_[i];
        if (c == kCells) continue;
        items_[start_[c]++] = i;
    }
    // start_[c] now holds the end of cell c; shift right by one cell to restore the starts.
    for (u32 c = kCells; c > 0; --c) start_[c] = start_[c - 1];
    start_[0] = 0;
}

}  // namespace rf::sim
