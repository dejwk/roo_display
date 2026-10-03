#pragma once

// Single-translation-unit host harness for requested C++ heap usage. Excludes
// malloc calls, over-aligned allocations, allocator overhead, and thread
// stacks. Generations exclude storage retained from earlier measurement scopes.
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace allocation {

struct Stats {
  size_t count = 0;
  size_t live = 0;
  size_t peak = 0;
};

struct alignas(std::max_align_t) Header {
  size_t size;
  size_t generation;
};

thread_local Stats stats;
thread_local size_t generation = 0;
thread_local bool enabled = false;

// Generations keep objects allocated in earlier phases out of this phase's
// peak, even when their lifetime ends during the measured operation.
void* Allocate(size_t size) {
  if (size > static_cast<size_t>(-1) - sizeof(Header)) return nullptr;
  Header* header = static_cast<Header*>(std::malloc(size + sizeof(Header)));
  if (header == nullptr) return nullptr;
  header->size = size;
  header->generation = enabled ? generation : 0;
  if (enabled) {
    ++stats.count;
    stats.live += size;
    stats.peak = std::max(stats.peak, stats.live);
  }
  return header + 1;
}

void Free(void* ptr) {
  if (ptr == nullptr) return;
  Header* header = static_cast<Header*>(ptr) - 1;
  if (enabled && header->generation == generation) stats.live -= header->size;
  std::free(header);
}

// Measures allocations while fn runs; callers retain results outside the
// callback when they also need the live-byte count at the end of the scope.
template <typename Fn>
Stats Measure(Fn fn) {
  stats = {};
  ++generation;
  enabled = true;
  fn();
  enabled = false;
  return stats;
}

}  // namespace allocation

void* operator new(size_t size) {
  void* ptr = allocation::Allocate(size);
  if (ptr == nullptr) std::abort();
  return ptr;
}

void* operator new[](size_t size) { return ::operator new(size); }

// Sanitizers can intercept these without delegating to the throwing overloads.
// Keep the same header layout for every ordinary allocation entry point.
void* operator new(size_t size, const std::nothrow_t&) noexcept {
  return allocation::Allocate(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
  return allocation::Allocate(size);
}

void operator delete(void* ptr) noexcept { allocation::Free(ptr); }
void operator delete[](void* ptr) noexcept { allocation::Free(ptr); }
void operator delete(void* ptr, size_t) noexcept { allocation::Free(ptr); }
void operator delete[](void* ptr, size_t) noexcept { allocation::Free(ptr); }

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
  allocation::Free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
  allocation::Free(ptr);
}
