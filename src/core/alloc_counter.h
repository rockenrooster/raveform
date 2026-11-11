#pragma once
// Counts calls to the global operator new (replaced in alloc_counter.cpp, backed by malloc/free).
// Benchmarks report the count over the measured window to prove the steady-state frame does not
// allocate. C code (SDL, drivers) is not counted.

#include "core/types.h"

namespace rf {

u64 heapAllocCount();

}  // namespace rf
