#include "core/stats.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace rf {

namespace {

float nearestRank(const float* sorted, u32 count, double p) {
    if (count == 0) return 0.0f;
    i64 rank = static_cast<i64>(std::ceil(p * static_cast<double>(count)));
    if (rank < 1) rank = 1;
    if (rank > static_cast<i64>(count)) rank = count;
    return sorted[rank - 1];
}

}  // namespace

Summary summarize(float* values, u32 count) {
    Summary s;
    s.count = count;
    if (count == 0) return s;
    std::sort(values, values + count);
    double sum = 0.0;
    for (u32 i = 0; i < count; ++i) sum += values[i];
    s.mean = static_cast<float>(sum / count);
    s.p50 = nearestRank(values, count, 0.50);
    s.p95 = nearestRank(values, count, 0.95);
    s.p99 = nearestRank(values, count, 0.99);
    s.p999 = nearestRank(values, count, 0.999);
    s.max = values[count - 1];
    return s;
}

SampleRecorder::~SampleRecorder() { std::free(data_); }

bool SampleRecorder::init(u32 capacity) {
    std::free(data_);
    data_ = static_cast<float*>(std::malloc(sizeof(float) * capacity));
    capacity_ = data_ ? capacity : 0;
    clear();
    return data_ != nullptr;
}

u32 SampleRecorder::countAbove(float threshold) const {
    u32 n = 0;
    for (u32 i = 0; i < count_; ++i) n += data_[i] > threshold ? 1u : 0u;
    return n;
}

}  // namespace rf
