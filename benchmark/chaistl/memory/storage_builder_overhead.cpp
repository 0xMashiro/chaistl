// SPDX-License-Identifier: Apache-2.0

// Benchmark to measure the overhead of uninitialized_storage_builder
// compared to direct allocator usage.

#include <chaistl/memory/detail/lifetime/allocator_uninitialized.hpp>
#include <chaistl/memory/detail/storage/uninitialized_storage_builder.hpp>

#include <benchmark/benchmark.h>
#include <memory>

#include "chaistl/registry.hpp"

namespace chaistl_benchmark {

// Baseline: direct allocator allocate + deallocate (no construction)
template <class T>
void bench_direct_allocate(benchmark::State& state) {
  using allocator_type = std::allocator<T>;
  using allocator_traits = std::allocator_traits<allocator_type>;

  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    allocator_type alloc;
    auto* ptr = allocator_traits::allocate(alloc, count);
    benchmark::DoNotOptimize(ptr);
    allocator_traits::deallocate(alloc, ptr, count);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// uninitialized_storage_builder: construct + release + deallocate
template <class T>
void bench_storage_builder(benchmark::State& state) {
  using allocator_type = std::allocator<T>;
  using allocator_traits = std::allocator_traits<allocator_type>;

  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    allocator_type alloc;
    chaistl::detail::uninitialized_storage_builder<T, allocator_type> storage(alloc, count);
    auto* ptr = storage.release();
    benchmark::DoNotOptimize(ptr);
    allocator_traits::deallocate(alloc, ptr, count);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// uninitialized_storage_builder with default construct
template <class T>
void bench_storage_builder_default_construct(benchmark::State& state) {
  using allocator_type = std::allocator<T>;
  using allocator_traits = std::allocator_traits<allocator_type>;

  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    allocator_type alloc;
    chaistl::detail::uninitialized_storage_builder<T, allocator_type> storage(alloc, count);
    auto* first = storage.data();
    auto* last = storage.uninitialized_default_construct_n(first, count);
    benchmark::DoNotOptimize(first);
    benchmark::DoNotOptimize(last);
    auto* ptr = storage.release();
    benchmark::DoNotOptimize(ptr);
    allocator_traits::deallocate(alloc, ptr, count);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// Direct allocator + allocator_uninitialized_default_construct_n
template <class T>
void bench_direct_default_construct(benchmark::State& state) {
  using allocator_type = std::allocator<T>;
  using allocator_traits = std::allocator_traits<allocator_type>;

  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    allocator_type alloc;
    auto* first = allocator_traits::allocate(alloc, count);
    auto* last = chaistl::detail::allocator_uninitialized_default_construct_n(alloc, first, count);
    benchmark::DoNotOptimize(first);
    benchmark::DoNotOptimize(last);
    allocator_traits::deallocate(alloc, first, count);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

void register_storage_builder_benchmarks() {
  auto register_benchmark = [](const char* name, auto* func) {
    benchmark::RegisterBenchmark(name, func)->Arg(32)->Arg(1024)->Arg(8192)->Unit(benchmark::kNanosecond);
  };

  register_benchmark("direct_allocate<int>", bench_direct_allocate<int>);
  register_benchmark("storage_builder<int>", bench_storage_builder<int>);
  register_benchmark("storage_builder_default_construct<int>", bench_storage_builder_default_construct<int>);
  register_benchmark("direct_default_construct<int>", bench_direct_default_construct<int>);
}

CHAISTL_REGISTER_BENCHMARK_FILE(register_storage_builder_benchmarks)

}  // namespace chaistl_benchmark
