#pragma once
// Frame-time statistics. Percentiles are exact (nearest rank) over recorded samples; storage is
// allocated once, so recording in the frame loop never touches the heap.

#include "core/types.h"

namespace rf {

struct Summary {
    u32 count = 0;
    float mean = 0.0f;
    float p50 = 0.0f;
    float p95 = 0.0f;
    float p99 = 0.0f;
    float p999 = 0.0f;
    float max = 0.0f;
};

// Sorts `values` in place and returns nearest-rank percentiles.
Summary summarize(float* values, u32 count);

// Fixed-capacity recorder for benchmark runs.
class SampleRecorder {
public:
    SampleRecorder() = default;
    SampleRecorder(const SampleRecorder&) = delete;
    SampleRecorder& operator=(const SampleRecorder&) = delete;
    ~SampleRecorder();

    bool init(u32 capacity);
    void clear() {
        count_ = 0;
        overflowed_ = false;
    }
    void add(float v) {
        if (count_ < capacity_) {
            data_[count_++] = v;
        } else {
            overflowed_ = true;
        }
    }
    u32 count() const { return count_; }
    bool overflowed() const { return overflowed_; }
    u32 countAbove(float threshold) const;
    // Sorts the stored samples; call once at the end of a run.
    Summary finish() { return summarize(data_, count_); }

private:
    float* data_ = nullptr;
    u32 capacity_ = 0;
    u32 count_ = 0;
    bool overflowed_ = false;
};

// Ring of the most recent N samples for live overlay statistics and graphs.
template <u32 N>
class RecentSamples {
public:
    void add(float v) {
        data_[head_] = v;
        head_ = (head_ + 1) % N;
        if (size_ < N) ++size_;
    }
    void clear() { head_ = size_ = 0; }
    u32 size() const { return size_; }
    static constexpr u32 capacity() { return N; }
    // Oldest-to-newest.
    float at(u32 i) const { return data_[(head_ + N - size_ + i) % N]; }
    Summary summarize() const {
        float tmp[N];
        for (u32 i = 0; i < size_; ++i) tmp[i] = at(i);
        return rf::summarize(tmp, size_);
    }

private:
    float data_[N] = {};
    u32 head_ = 0;
    u32 size_ = 0;
};

}  // namespace rf
