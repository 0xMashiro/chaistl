// SPDX-License-Identifier: Apache-2.0

// References:
// - std::bitset is the fixed-size baseline for chaistl::bitset.
// - dynamic_bitset has no standard-library baseline; workloads focus on
//   bit density and next-set-bit scans.
// - priority_deque is a double-ended priority queue, benchmarked against
//   std::multiset for the same min/max removal surface.

#include <chaistl/bitset.hpp>
#include <chaistl/containers/priority_deque.hpp>
#include <chaistl/dynamic_bitset.hpp>

#include <algorithm>
#include <benchmark/benchmark.h>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "chaistl/registry.hpp"

namespace chaistl_benchmark {

namespace {

using key_type = std::int64_t;

std::vector<key_type> shuffled_keys(std::size_t count, unsigned seed) {
  std::vector<key_type> keys(count);
  std::iota(keys.begin(), keys.end(), key_type{0});
  std::mt19937_64 rng(seed);
  std::ranges::shuffle(keys, rng);
  return keys;
}

template <class Bitset>
void set_periodic_bits(Bitset& bits, std::size_t size, std::size_t period) {
  for (std::size_t i = 0; i < size; i += period) {
    bits.set(i);
  }
}

template <class Bitset>
void bench_fixed_count_dense(benchmark::State& state) {
  constexpr std::size_t size = 4096;
  Bitset bits;
  for (std::size_t i = 0; i < size; i += 3) {
    bits.set(i);
  }

  for (auto _ : state) {
    benchmark::DoNotOptimize(bits.count());
  }
  state.SetItemsProcessed(state.iterations() * size);
}

template <class Bitset>
void bench_fixed_bitwise_xor(benchmark::State& state) {
  constexpr std::size_t size = 4096;
  Bitset lhs;
  Bitset rhs;
  set_periodic_bits(lhs, size, 3);
  set_periodic_bits(rhs, size, 5);

  for (auto _ : state) {
    auto result = lhs ^ rhs;
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}

void bench_dynamic_count_dense(benchmark::State& state) {
  const auto size = static_cast<std::size_t>(state.range(0));
  chaistl::dynamic_bitset<> bits(size);
  set_periodic_bits(bits, size, 3);

  for (auto _ : state) {
    benchmark::DoNotOptimize(bits.count());
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void bench_dynamic_find_next_sparse(benchmark::State& state) {
  const auto size = static_cast<std::size_t>(state.range(0));
  chaistl::dynamic_bitset<> bits(size);
  set_periodic_bits(bits, size, 97);

  for (auto _ : state) {
    std::size_t count = 0;
    for (auto pos = bits.find_first(); pos != chaistl::dynamic_bitset<>::npos; pos = bits.find_next(pos)) {
      ++count;
      benchmark::DoNotOptimize(pos);
    }
    benchmark::DoNotOptimize(count);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void bench_dynamic_bitwise_xor(benchmark::State& state) {
  const auto size = static_cast<std::size_t>(state.range(0));
  chaistl::dynamic_bitset<> lhs(size);
  chaistl::dynamic_bitset<> rhs(size);
  set_periodic_bits(lhs, size, 3);
  set_periodic_bits(rhs, size, 5);

  for (auto _ : state) {
    auto result = lhs ^ rhs;
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Queue>
void bench_priority_push_random(benchmark::State& state) {
  const auto keys = shuffled_keys(static_cast<std::size_t>(state.range(0)), 20260612u);

  for (auto _ : state) {
    Queue queue;
    for (const key_type key : keys) {
      queue.push(key);
    }
    benchmark::DoNotOptimize(queue);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void bench_priority_deque_pop_minmax_random(benchmark::State& state) {
  const auto keys = shuffled_keys(static_cast<std::size_t>(state.range(0)), 20260612u);

  for (auto _ : state) {
    chaistl::priority_deque<key_type> queue(keys.begin(), keys.end());
    bool pop_min = true;
    while (!queue.empty()) {
      if (pop_min) {
        auto value = queue.min();
        benchmark::DoNotOptimize(value);
        queue.pop_min();
      } else {
        auto value = queue.max();
        benchmark::DoNotOptimize(value);
        queue.pop_max();
      }
      pop_min = !pop_min;
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void bench_multiset_pop_minmax_random(benchmark::State& state) {
  const auto keys = shuffled_keys(static_cast<std::size_t>(state.range(0)), 20260612u);

  for (auto _ : state) {
    std::multiset<key_type> values(keys.begin(), keys.end());
    bool pop_min = true;
    while (!values.empty()) {
      if (pop_min) {
        auto value = *values.begin();
        benchmark::DoNotOptimize(value);
        values.erase(values.begin());
      } else {
        auto last = std::prev(values.end());
        auto value = *last;
        benchmark::DoNotOptimize(value);
        values.erase(last);
      }
      pop_min = !pop_min;
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void register_range_benchmark(std::string_view name, auto* function) {
  benchmark::RegisterBenchmark(std::string(name).c_str(), function)->RangeMultiplier(8)->Range(1 << 10, 1 << 16);
}

}  // namespace

void register_bit_structure_benchmarks() {
  benchmark::RegisterBenchmark("std::bitset<4096>/count_dense", &bench_fixed_count_dense<std::bitset<4096>>);
  benchmark::RegisterBenchmark("chaistl::bitset<4096>/count_dense", &bench_fixed_count_dense<chaistl::bitset<4096>>);
  benchmark::RegisterBenchmark("std::bitset<4096>/bitwise_xor", &bench_fixed_bitwise_xor<std::bitset<4096>>);
  benchmark::RegisterBenchmark("chaistl::bitset<4096>/bitwise_xor", &bench_fixed_bitwise_xor<chaistl::bitset<4096>>);

  register_range_benchmark("chaistl::dynamic_bitset/count_dense", &bench_dynamic_count_dense);
  register_range_benchmark("chaistl::dynamic_bitset/find_next_sparse", &bench_dynamic_find_next_sparse);
  register_range_benchmark("chaistl::dynamic_bitset/bitwise_xor", &bench_dynamic_bitwise_xor);

  register_range_benchmark("chaistl::priority_deque/push_random",
                           &bench_priority_push_random<chaistl::priority_deque<key_type>>);
  register_range_benchmark("chaistl::priority_deque/pop_minmax_random", &bench_priority_deque_pop_minmax_random);
  register_range_benchmark("std::multiset/pop_minmax_random", &bench_multiset_pop_minmax_random);
}

CHAISTL_BENCHMARK_SMOKE_DOMAIN(
    bit_structure,
    "^(std::bitset<4096>|chaistl::bitset<4096>)/(count_dense|bitwise_xor)$|^chaistl::dynamic_bitset/"
    "(count_dense|find_next_sparse|bitwise_xor)/1024$|^(chaistl::priority_deque|std::multiset)/"
    "(push_random|pop_minmax_random)/1024$")
CHAISTL_REGISTER_BENCHMARK_FILE(register_bit_structure_benchmarks)

}  // namespace chaistl_benchmark
