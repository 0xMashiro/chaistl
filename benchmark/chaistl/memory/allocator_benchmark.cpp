// SPDX-License-Identifier: Apache-2.0

// Benchmarks for chaistl::allocator vs std::allocator
// Covers: allocate, deallocate, allocate_at_least, zero-size edge cases

#include <chaistl/memory/allocator.hpp>

#include <benchmark/benchmark.h>
#include <memory>

namespace {

// =========================================================================
// Baseline: allocate + deallocate round-trip
// =========================================================================

template <class T, class Alloc>
static void BM_AllocDealloc(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  Alloc alloc;
  for (auto _ : state) {
    auto* p = alloc.allocate(n);
    benchmark::DoNotOptimize(p);
    alloc.deallocate(p, n);
  }
  state.SetItemsProcessed(state.iterations() * n);
}

BENCHMARK(BM_AllocDealloc<int, std::allocator<int>>)->Arg(0)->Arg(1)->Arg(32)->Arg(1024)->Arg(8192);
BENCHMARK(BM_AllocDealloc<int, chaistl::allocator<int>>)->Arg(0)->Arg(1)->Arg(32)->Arg(1024)->Arg(8192);

BENCHMARK(BM_AllocDealloc<double, std::allocator<double>>)->Arg(0)->Arg(1)->Arg(32)->Arg(1024)->Arg(8192);
BENCHMARK(BM_AllocDealloc<double, chaistl::allocator<double>>)->Arg(0)->Arg(1)->Arg(32)->Arg(1024)->Arg(8192);

// =========================================================================
// allocate_at_least
// =========================================================================

template <class T, class Alloc>
static void BM_AllocAtLeast(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  Alloc alloc;
  for (auto _ : state) {
    auto r = alloc.allocate_at_least(n);
    benchmark::DoNotOptimize(r.ptr);
    benchmark::DoNotOptimize(r.count);
    alloc.deallocate(r.ptr, r.count);
  }
  state.SetItemsProcessed(state.iterations() * n);
}

BENCHMARK(BM_AllocAtLeast<int, chaistl::allocator<int>>)->Arg(0)->Arg(1)->Arg(32)->Arg(1024)->Arg(8192);

// =========================================================================
// Repeated small allocations (stress allocator fast path)
// =========================================================================

template <class T, class Alloc>
static void BM_RepeatedSmallAlloc(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  Alloc alloc;
  std::vector<T*> ptrs;
  ptrs.reserve(256);

  for (auto _ : state) {
    ptrs.clear();
    for (int i = 0; i < 256; ++i) {
      auto* p = alloc.allocate(n);
      benchmark::DoNotOptimize(p);
      ptrs.push_back(p);
    }
    for (auto* p : ptrs) {
      alloc.deallocate(p, n);
    }
  }
  state.SetItemsProcessed(state.iterations() * 256 * n);
}

BENCHMARK(BM_RepeatedSmallAlloc<int, std::allocator<int>>)->Arg(1)->Arg(4)->Arg(16);
BENCHMARK(BM_RepeatedSmallAlloc<int, chaistl::allocator<int>>)->Arg(1)->Arg(4)->Arg(16);

// =========================================================================
// Large allocation (page-boundary stress)
// =========================================================================

template <class T, class Alloc>
static void BM_LargeAlloc(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  Alloc alloc;
  for (auto _ : state) {
    auto* p = alloc.allocate(n);
    benchmark::DoNotOptimize(p);
    alloc.deallocate(p, n);
  }
  state.SetItemsProcessed(state.iterations() * n);
}

BENCHMARK(BM_LargeAlloc<char, std::allocator<char>>)->Arg(1 << 20)->Arg(1 << 24);
BENCHMARK(BM_LargeAlloc<char, chaistl::allocator<char>>)->Arg(1 << 20)->Arg(1 << 24);

}  // namespace
