// SPDX-License-Identifier: Apache-2.0

// References:
// - Public heap aliases let benchmark users compare policy choices without
//   spelling heap_policy::* directly.
// - Heap workloads separate contiguous binary/d-ary heaps, self-adjusting
//   meldable heaps, and degree-consolidating heaps. The benchmark names keep
//   operations explicit because asymptotic wins are workload-dependent.

#include <chaistl/containers/binomial_heap.hpp>
#include <chaistl/containers/d_ary_heap.hpp>
#include <chaistl/containers/fibonacci_heap.hpp>
#include <chaistl/containers/leftist_heap.hpp>
#include <chaistl/containers/pairing_heap.hpp>
#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/containers/skew_heap.hpp>

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "chaistl/registry.hpp"

namespace chaistl_benchmark {

namespace {

using key_type = std::int64_t;

using std_heap = std::priority_queue<key_type>;
using binary_heap = chaistl::priority_queue<key_type>;
using dary4_heap = chaistl::d_ary_heap<key_type, 4>;
using pairing_heap = chaistl::pairing_heap<key_type>;
using binomial_heap = chaistl::binomial_heap<key_type>;
using skew_heap = chaistl::skew_heap<key_type>;
using leftist_heap = chaistl::leftist_heap<key_type>;
using fibonacci_heap = chaistl::fibonacci_heap<key_type>;

std::vector<key_type> shuffled_keys(std::size_t count, unsigned seed) {
  std::vector<key_type> keys(count);
  std::iota(keys.begin(), keys.end(), key_type{0});
  std::mt19937_64 rng(seed);
  std::ranges::shuffle(keys, rng);
  return keys;
}

template <class Heap>
void bench_push_random(benchmark::State& state) {
  const auto keys = shuffled_keys(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    Heap h;
    for (const key_type k : keys) {
      h.push(k);
    }
    benchmark::DoNotOptimize(h);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Heap>
void bench_push_pop_random(benchmark::State& state) {
  const auto keys = shuffled_keys(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    Heap h;
    for (const key_type k : keys) {
      h.push(k);
    }
    while (!h.empty()) {
      auto top = h.top();
      benchmark::DoNotOptimize(top);
      h.pop();
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0) * 2);
}

template <class Heap>
void bench_interleaved_push_pop(benchmark::State& state) {
  const auto keys = shuffled_keys(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    Heap h;
    std::size_t pushed = 0;
    std::size_t popped = 0;
    for (const key_type k : keys) {
      h.push(k);
      ++pushed;
      if (pushed % 4 == 0) {
        auto top = h.top();
        benchmark::DoNotOptimize(top);
        h.pop();
        ++popped;
      }
    }
    while (!h.empty()) {
      auto top = h.top();
      benchmark::DoNotOptimize(top);
      h.pop();
      ++popped;
    }
    benchmark::DoNotOptimize(popped);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0) * 2);
}

template <class Heap>
void bench_join_random(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto left = shuffled_keys(n, 42u);
  auto right = shuffled_keys(n, 123u);
  for (key_type& k : right) {
    k += static_cast<key_type>(n);
  }

  for (auto _ : state) {
    Heap a;
    Heap b;
    for (const key_type k : left) {
      a.push(k);
    }
    for (const key_type k : right) {
      b.push(k);
    }
    a.join(b);
    benchmark::DoNotOptimize(a);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0) * 2);
}

template <class Heap>
void register_basic_heap_suite(std::string_view container_name) {
  const auto register_one = [container_name](std::string_view operation, auto* function) {
    benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), function)
        ->RangeMultiplier(8)
        ->Range(1 << 10, 1 << 16);
  };

  register_one("push_random", &bench_push_random<Heap>);
  register_one("push_pop_random", &bench_push_pop_random<Heap>);
  register_one("interleaved_push_pop", &bench_interleaved_push_pop<Heap>);
}

template <class Heap>
void register_meldable_heap_suite(std::string_view container_name) {
  register_basic_heap_suite<Heap>(container_name);
  benchmark::RegisterBenchmark((std::string(container_name) + "/join_random").c_str(), &bench_join_random<Heap>)
      ->RangeMultiplier(8)
      ->Range(1 << 10, 1 << 16);
}

}  // namespace

void register_heap_policy_benchmarks() {
  register_basic_heap_suite<std_heap>("heap_std");
  register_basic_heap_suite<binary_heap>("heap_binary");
  register_basic_heap_suite<dary4_heap>("heap_dary4");
  register_meldable_heap_suite<pairing_heap>("heap_pairing");
  register_meldable_heap_suite<binomial_heap>("heap_binomial");
  register_meldable_heap_suite<skew_heap>("heap_skew");
  register_meldable_heap_suite<leftist_heap>("heap_leftist");
  register_meldable_heap_suite<fibonacci_heap>("heap_fibonacci");
}

CHAISTL_BENCHMARK_SMOKE_DOMAIN(
    heap_policy,
    "^heap_(std|binary|dary4|pairing|binomial|skew|leftist|fibonacci)/(push_random|push_pop_random)/"
    "1024$|^heap_(pairing|binomial|skew|leftist|fibonacci)/join_random/1024$")
CHAISTL_REGISTER_BENCHMARK_FILE(register_heap_policy_benchmarks)

}  // namespace chaistl_benchmark
