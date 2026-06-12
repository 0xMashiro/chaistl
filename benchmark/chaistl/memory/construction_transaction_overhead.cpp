// SPDX-License-Identifier: Apache-2.0

// Microbenchmarks for construction_transaction optimizations.
// Uses BENCHMARK macros for auto-registration — no manual register_* needed.

#include <chaistl/memory/detail/lifetime/allocator_uninitialized.hpp>
#include <chaistl/memory/detail/lifetime/construction_transaction.hpp>
#include <chaistl/memory/detail/storage/uninitialized_storage_builder.hpp>

#include <benchmark/benchmark.h>
#include <memory>
#include <string>
#include <vector>

namespace {

// Tier 2: non-trivially copyable, trivially destructible
// (optimisation target: skip exception_guard + transaction tracking)
struct Tier2 {
  int x;
  Tier2() : x(0) {}
  explicit Tier2(int v) : x(v) {}
  Tier2(const Tier2& o) : x(o.x) {}
  Tier2& operator=(const Tier2& o) {
    x = o.x;
    return *this;
  }
};
static_assert(!std::is_trivially_copyable_v<Tier2>);
static_assert(std::is_trivially_destructible_v<Tier2>);

// Tier 3: needs_rollback — full transaction tracking
struct Tier3 {
  int x;
  Tier3() : x(0) {}
  explicit Tier3(int v) : x(v) {}
  Tier3(const Tier3& o) : x(o.x) {}
  ~Tier3() { x = -1; }
};
static_assert(!std::is_trivially_copyable_v<Tier3>);
static_assert(!std::is_trivially_destructible_v<Tier3>);

// =========================================================================
// B1: Constructor overhead (dead-init removal)
// =========================================================================

template <class T>
static void BM_ConstructTx(benchmark::State& state) {
  std::allocator<T> alloc;
  for (auto _ : state) {
    chaistl::detail::construction_transaction<std::allocator<T>, T*, T> tx(alloc);
    benchmark::DoNotOptimize(tx);
  }
}
BENCHMARK(BM_ConstructTx<int>);
BENCHMARK(BM_ConstructTx<Tier2>);
BENCHMARK(BM_ConstructTx<Tier3>);
BENCHMARK(BM_ConstructTx<std::string>);

// =========================================================================
// B2: construct_at + complete round-trip
// =========================================================================

template <class T>
static void BM_ConstructAtComplete(benchmark::State& state) {
  std::allocator<T> alloc;
  T storage{};
  for (auto _ : state) {
    chaistl::detail::construction_transaction<std::allocator<T>, T*, T> tx(alloc);
    tx.construct_at(&storage, T{});
    tx.complete();
    benchmark::DoNotOptimize(tx);
  }
}
BENCHMARK(BM_ConstructAtComplete<int>);
BENCHMARK(BM_ConstructAtComplete<Tier2>);
BENCHMARK(BM_ConstructAtComplete<Tier3>);

// =========================================================================
// B3: allocator_uninitialized_copy (trivially_destructible fast path)
// =========================================================================

template <class T>
static void BM_UAlgoCopy(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  std::allocator<T> alloc;
  std::vector<T> src(n);
  std::vector<T> dst(n);

  for (auto _ : state) {
    auto* r = chaistl::detail::allocator_uninitialized_copy(alloc, src.data(), src.data() + n, dst.data());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_UAlgoCopy<int>)->Arg(64)->Arg(1024)->Arg(8192);
BENCHMARK(BM_UAlgoCopy<Tier2>)->Arg(64)->Arg(1024)->Arg(8192);
BENCHMARK(BM_UAlgoCopy<Tier3>)->Arg(64)->Arg(1024)->Arg(8192);

// =========================================================================
// B4: storage_builder full path (allocate → fill → release)
//     Vector growth hot path.
// =========================================================================

template <class T>
static void BM_StorageBuilderFill(benchmark::State& state) {
  using A = std::allocator<T>;
  using AT = std::allocator_traits<A>;
  const auto n = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    A alloc;
    chaistl::detail::uninitialized_storage_builder<T, A> storage(alloc, n);
    auto* last = storage.uninitialized_default_construct_n(storage.data(), n);
    benchmark::DoNotOptimize(last);
    auto* ptr = storage.release();
    AT::deallocate(alloc, ptr, n);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_StorageBuilderFill<int>)->Arg(64)->Arg(1024)->Arg(8192);
BENCHMARK(BM_StorageBuilderFill<Tier2>)->Arg(64)->Arg(1024)->Arg(8192);
BENCHMARK(BM_StorageBuilderFill<Tier3>)->Arg(64)->Arg(1024)->Arg(8192);

// =========================================================================
// B5: Multi-op transaction (3 construction calls → 3 ranges)
// =========================================================================

template <class T>
static void BM_MultiOp(benchmark::State& state) {
  using A = std::allocator<T>;
  using AT = std::allocator_traits<A>;
  const auto n = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    A alloc;
    chaistl::detail::uninitialized_storage_builder<T, A> storage(alloc, n);
    auto* p = storage.data();
    p = storage.uninitialized_default_construct_n(p, n / 3);
    p = storage.uninitialized_default_construct_n(p, n / 3);
    p = storage.uninitialized_default_construct_n(p, n - 2 * (n / 3));
    benchmark::DoNotOptimize(p);
    auto* ptr = storage.release();
    AT::deallocate(alloc, ptr, n);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_MultiOp<int>)->Arg(64)->Arg(1024)->Arg(8192);
BENCHMARK(BM_MultiOp<Tier2>)->Arg(64)->Arg(1024)->Arg(8192);
BENCHMARK(BM_MultiOp<Tier3>)->Arg(64)->Arg(1024)->Arg(8192);

// =========================================================================
// B6: Raw alloc/dealloc calibration baseline
// =========================================================================

template <class T>
static void BM_RawAlloc(benchmark::State& state) {
  using A = std::allocator<T>;
  using AT = std::allocator_traits<A>;
  const auto n = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    A alloc;
    auto* ptr = AT::allocate(alloc, n);
    benchmark::DoNotOptimize(ptr);
    AT::deallocate(alloc, ptr, n);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_RawAlloc<int>)->Arg(64)->Arg(1024)->Arg(8192);
BENCHMARK(BM_RawAlloc<Tier2>)->Arg(64)->Arg(1024)->Arg(8192);
BENCHMARK(BM_RawAlloc<Tier3>)->Arg(64)->Arg(1024)->Arg(8192);

}  // namespace

// BENCHMARK_MAIN() is not used; we link against benchmark_main.
