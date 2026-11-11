#include "core/alloc_counter.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {
std::atomic<rf::u64> g_allocations{0};

void* countedAlloc(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size ? size : 1);
}
}  // namespace

namespace rf {
u64 heapAllocCount() { return g_allocations.load(std::memory_order_relaxed); }
}  // namespace rf

// Replacements for the unaligned forms. Built without exceptions, so failure aborts.
void* operator new(std::size_t size) {
    if (void* p = countedAlloc(size)) return p;
    std::abort();
}
void* operator new[](std::size_t size) {
    if (void* p = countedAlloc(size)) return p;
    std::abort();
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return countedAlloc(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return countedAlloc(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
