// SPDX-License-Identifier: Apache-2.0

// Benchmarks for chaistl::allocator vs std::allocator
// Covers: allocate, deallocate, allocate_at_least, zero-size edge cases

#include <chaistl/containers/list.hpp>
#include <chaistl/containers/map.hpp>
#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/memory/allocator.hpp>
#include <chaistl/memory_resource.hpp>

#include <benchmark/benchmark.h>
#include <list>
#include <map>
#include <memory>
#include <memory_resource>
#include <unordered_map>
#include <vector>

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
BENCHMARK(BM_AllocDealloc<int, std::pmr::polymorphic_allocator<int>>)->Arg(0)->Arg(1)->Arg(32)->Arg(1024)->Arg(8192);
BENCHMARK(BM_AllocDealloc<int, chaistl::pmr::polymorphic_allocator<int>>)
    ->Arg(0)
    ->Arg(1)
    ->Arg(32)
    ->Arg(1024)
    ->Arg(8192);

BENCHMARK(BM_AllocDealloc<double, std::allocator<double>>)->Arg(0)->Arg(1)->Arg(32)->Arg(1024)->Arg(8192);
BENCHMARK(BM_AllocDealloc<double, chaistl::allocator<double>>)->Arg(0)->Arg(1)->Arg(32)->Arg(1024)->Arg(8192);
BENCHMARK(BM_AllocDealloc<double, std::pmr::polymorphic_allocator<double>>)
    ->Arg(0)
    ->Arg(1)
    ->Arg(32)
    ->Arg(1024)
    ->Arg(8192);
BENCHMARK(BM_AllocDealloc<double, chaistl::pmr::polymorphic_allocator<double>>)
    ->Arg(0)
    ->Arg(1)
    ->Arg(32)
    ->Arg(1024)
    ->Arg(8192);

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
BENCHMARK(BM_RepeatedSmallAlloc<int, std::pmr::polymorphic_allocator<int>>)->Arg(1)->Arg(4)->Arg(16);
BENCHMARK(BM_RepeatedSmallAlloc<int, chaistl::pmr::polymorphic_allocator<int>>)->Arg(1)->Arg(4)->Arg(16);

// =========================================================================
// Monotonic resources: many allocations, one release
// =========================================================================

template <class Resource, class Alloc>
static void BM_MonotonicBatchAlloc(benchmark::State& state) {
  const auto allocation_count = static_cast<std::size_t>(state.range(0));
  const auto object_count = static_cast<std::size_t>(state.range(1));
  std::vector<int*> ptrs;
  ptrs.reserve(allocation_count);

  for (auto _ : state) {
    Resource resource;
    Alloc alloc(&resource);

    ptrs.clear();
    for (std::size_t i = 0; i != allocation_count; ++i) {
      int* pointer = alloc.allocate(object_count);
      benchmark::DoNotOptimize(pointer);
      ptrs.push_back(pointer);
    }

    for (int* pointer : ptrs) {
      alloc.deallocate(pointer, object_count);
    }
    resource.release();
  }

  state.SetItemsProcessed(state.iterations() * allocation_count * object_count);
}

BENCHMARK(BM_MonotonicBatchAlloc<std::pmr::monotonic_buffer_resource, std::pmr::polymorphic_allocator<int>>)
    ->Args({256, 1})
    ->Args({256, 8})
    ->Args({1024, 1})
    ->Args({1024, 8});
BENCHMARK(BM_MonotonicBatchAlloc<chaistl::pmr::monotonic_buffer_resource, chaistl::pmr::polymorphic_allocator<int>>)
    ->Args({256, 1})
    ->Args({256, 8})
    ->Args({1024, 1})
    ->Args({1024, 8});

// =========================================================================
// Pool resources: many small allocations with reuse
// =========================================================================

template <class Resource, class Alloc>
static void BM_PoolBatchAllocDealloc(benchmark::State& state) {
  const auto allocation_count = static_cast<std::size_t>(state.range(0));
  const auto object_count = static_cast<std::size_t>(state.range(1));
  std::vector<int*> ptrs;
  ptrs.reserve(allocation_count);

  for (auto _ : state) {
    Resource resource;
    Alloc alloc(&resource);

    ptrs.clear();
    for (std::size_t i = 0; i != allocation_count; ++i) {
      int* pointer = alloc.allocate(object_count);
      benchmark::DoNotOptimize(pointer);
      ptrs.push_back(pointer);
    }

    for (int* pointer : ptrs) {
      alloc.deallocate(pointer, object_count);
    }
  }

  state.SetItemsProcessed(state.iterations() * allocation_count * object_count);
}

BENCHMARK(BM_PoolBatchAllocDealloc<std::pmr::unsynchronized_pool_resource, std::pmr::polymorphic_allocator<int>>)
    ->Args({256, 1})
    ->Args({256, 8})
    ->Args({1024, 1})
    ->Args({1024, 8});
BENCHMARK(
    BM_PoolBatchAllocDealloc<chaistl::pmr::unsynchronized_pool_resource, chaistl::pmr::polymorphic_allocator<int>>)
    ->Args({256, 1})
    ->Args({256, 8})
    ->Args({1024, 1})
    ->Args({1024, 8});
BENCHMARK(BM_PoolBatchAllocDealloc<std::pmr::synchronized_pool_resource, std::pmr::polymorphic_allocator<int>>)
    ->Args({256, 1})
    ->Args({256, 8})
    ->Args({1024, 1})
    ->Args({1024, 8});
BENCHMARK(BM_PoolBatchAllocDealloc<chaistl::pmr::synchronized_pool_resource, chaistl::pmr::polymorphic_allocator<int>>)
    ->Args({256, 1})
    ->Args({256, 8})
    ->Args({1024, 1})
    ->Args({1024, 8});

// =========================================================================
// PMR-backed containers: allocator cost under real node workloads
// =========================================================================

template <class Container, class Resource, class Alloc>
static void BM_PmrListPushClear(benchmark::State& state) {
  const auto count = static_cast<int>(state.range(0));

  for (auto _ : state) {
    Resource resource;
    Container values{Alloc(&resource)};
    for (int value = 0; value != count; ++value) {
      values.push_back(value);
    }
    benchmark::DoNotOptimize(values.size());
    values.clear();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(BM_PmrListPushClear<std::pmr::list<int>,
                              std::pmr::unsynchronized_pool_resource,
                              std::pmr::polymorphic_allocator<int>>)
    ->Arg(256)
    ->Arg(1024);
BENCHMARK(BM_PmrListPushClear<chaistl::pmr::list<int>,
                              chaistl::pmr::unsynchronized_pool_resource,
                              chaistl::pmr::polymorphic_allocator<int>>)
    ->Arg(256)
    ->Arg(1024);

template <class Container, class Resource, class Alloc>
static void BM_PmrMapEmplaceClear(benchmark::State& state) {
  const auto count = static_cast<int>(state.range(0));

  for (auto _ : state) {
    Resource resource;
    Container values{Alloc(&resource)};
    for (int value = 0; value != count; ++value) {
      values.emplace(value, value);
    }
    benchmark::DoNotOptimize(values.size());
    values.clear();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(BM_PmrMapEmplaceClear<std::pmr::map<int, int>,
                                std::pmr::unsynchronized_pool_resource,
                                std::pmr::polymorphic_allocator<std::pair<const int, int>>>)
    ->Arg(256)
    ->Arg(1024);
BENCHMARK(BM_PmrMapEmplaceClear<chaistl::pmr::map<int, int>,
                                chaistl::pmr::unsynchronized_pool_resource,
                                chaistl::pmr::polymorphic_allocator<std::pair<const int, int>>>)
    ->Arg(256)
    ->Arg(1024);

BENCHMARK(BM_PmrMapEmplaceClear<std::pmr::unordered_map<int, int>,
                                std::pmr::unsynchronized_pool_resource,
                                std::pmr::polymorphic_allocator<std::pair<const int, int>>>)
    ->Arg(256)
    ->Arg(1024);
BENCHMARK(BM_PmrMapEmplaceClear<chaistl::pmr::unordered_map<int, int>,
                                chaistl::pmr::unsynchronized_pool_resource,
                                chaistl::pmr::polymorphic_allocator<std::pair<const int, int>>>)
    ->Arg(256)
    ->Arg(1024);

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
