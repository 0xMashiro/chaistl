// SPDX-License-Identifier: Apache-2.0

// Diagnostics for unordered-container iteration locality. The main unordered
// benchmarks compare operations; these vary construction policy so iteration
// differences can be attributed to insertion order, reserve behavior, or load
// factor instead of being treated as generic algorithm noise.

#include <chaistl/containers/unordered_set.hpp>

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "chaistl/registry.hpp"

namespace chaistl_benchmark {
namespace {

using key_type = int;

enum class key_order {
  sequential,
  random,
};

enum class reserve_policy {
  natural_growth,
  reserve_first,
};

enum class miss_probe {
  near_range,
  far_range,
};

enum class miss_bucket {
  empty,
  occupied,
};

auto make_keys(std::size_t count, key_order order) -> std::vector<key_type> {
  std::vector<key_type> keys(count);
  std::iota(keys.begin(), keys.end(), key_type{0});
  if (order == key_order::random) {
    std::mt19937 rng(42u);
    std::ranges::shuffle(keys, rng);
  }
  return keys;
}

auto make_miss_keys(std::size_t count, miss_probe probe) -> std::vector<key_type> {
  const key_type first = probe == miss_probe::near_range ? static_cast<key_type>(count)
                                                         : static_cast<key_type>(count * 16);
  std::vector<key_type> keys(count);
  std::iota(keys.begin(), keys.end(), first);
  std::mt19937 rng(1234u);
  std::ranges::shuffle(keys, rng);
  return keys;
}

template <class Set>
auto make_bucket_classified_miss_keys(const Set& set, std::size_t count, miss_bucket bucket_kind)
    -> std::vector<key_type> {
  std::vector<key_type> keys;
  keys.reserve(count);

  for (key_type candidate = static_cast<key_type>(count); keys.size() < count; ++candidate) {
    const auto bucket = set.bucket(candidate);
    const bool bucket_is_empty = set.bucket_size(bucket) == 0;
    if ((bucket_kind == miss_bucket::empty) == bucket_is_empty && !set.contains(candidate)) {
      keys.push_back(candidate);
    }
    if (candidate == static_cast<key_type>(count * 32) && keys.empty()) {
      break;
    }
  }

  if (keys.empty()) {
    keys.push_back(static_cast<key_type>(count));
  }
  for (std::size_t index = keys.size(); index < count; ++index) {
    keys.push_back(keys[index % keys.size()]);
  }

  std::mt19937 rng(1234u);
  std::ranges::shuffle(keys, rng);
  return keys;
}

template <class Set>
void record_probe_bucket_counters(benchmark::State& state, const Set& set, const std::vector<key_type>& probes) {
  std::size_t empty_count = 0;
  std::size_t bucket_size_sum = 0;
  for (const key_type key : probes) {
    const auto size = set.bucket_size(set.bucket(key));
    bucket_size_sum += size;
    if (size == 0) {
      ++empty_count;
    }
  }
  state.counters["empty_ratio"] = static_cast<double>(empty_count) / static_cast<double>(probes.size());
  state.counters["avg_bucket"] = static_cast<double>(bucket_size_sum) / static_cast<double>(probes.size());
}

template <class Set>
auto make_set(std::size_t count, key_order order, reserve_policy reserve, float max_load_factor) -> Set {
  const auto keys = make_keys(count, order);
  Set set;
  set.max_load_factor(max_load_factor);
  if (reserve == reserve_policy::reserve_first) {
    set.reserve(count);
  }
  for (const key_type key : keys) {
    set.insert(key);
  }
  return set;
}

template <class Set, reserve_policy Reserve, miss_probe Probe>
void bench_lookup_miss_with_policy(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const float max_load_factor = static_cast<float>(state.range(1)) / 100.0F;
  const Set set = make_set<Set>(count, key_order::random, Reserve, max_load_factor);
  const auto probes = make_miss_keys(count, Probe);

  state.counters["load_factor"] = set.load_factor();
  state.counters["bucket_count"] = static_cast<double>(set.bucket_count());
  record_probe_bucket_counters(state, set, probes);

  for (auto _ : state) {
    for (const key_type key : probes) {
      benchmark::DoNotOptimize(set.find(key));
    }
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Set, reserve_policy Reserve, miss_bucket Bucket>
void bench_lookup_miss_bucket_class(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const float max_load_factor = static_cast<float>(state.range(1)) / 100.0F;
  const Set set = make_set<Set>(count, key_order::random, Reserve, max_load_factor);
  const auto probes = make_bucket_classified_miss_keys(set, count, Bucket);

  state.counters["load_factor"] = set.load_factor();
  state.counters["bucket_count"] = static_cast<double>(set.bucket_count());
  record_probe_bucket_counters(state, set, probes);

  for (auto _ : state) {
    for (const key_type key : probes) {
      benchmark::DoNotOptimize(set.find(key));
    }
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Set, key_order Order, reserve_policy Reserve>
void bench_iterate_sum_with_policy(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const float max_load_factor = static_cast<float>(state.range(1)) / 100.0F;
  const Set set = make_set<Set>(count, Order, Reserve, max_load_factor);

  state.counters["load_factor"] = set.load_factor();
  state.counters["bucket_count"] = static_cast<double>(set.bucket_count());

  for (auto _ : state) {
    long long sum = 0;
    for (const key_type key : set) {
      sum += key;
    }
    benchmark::DoNotOptimize(sum);
    benchmark::DoNotOptimize(&set);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

void apply_iteration_diagnostic_args(benchmark::Benchmark* benchmark) {
  benchmark->Args({4096, 50})
      ->Args({4096, 100})
      ->Args({4096, 200})
      ->Args({65536, 50})
      ->Args({65536, 100})
      ->Args({65536, 200})
      ->Unit(benchmark::kNanosecond);
}

template <class Set, key_order Order, reserve_policy Reserve>
void register_iteration_diagnostic(std::string_view container_name, std::string_view operation) {
  auto* benchmark = benchmark::RegisterBenchmark(
      (std::string(container_name) + "/" + std::string(operation)).c_str(),
      &bench_iterate_sum_with_policy<Set, Order, Reserve>);
  apply_iteration_diagnostic_args(benchmark);
}

template <class Set, reserve_policy Reserve, miss_probe Probe>
void register_lookup_miss_diagnostic(std::string_view container_name, std::string_view operation) {
  auto* benchmark = benchmark::RegisterBenchmark(
      (std::string(container_name) + "/" + std::string(operation)).c_str(),
      &bench_lookup_miss_with_policy<Set, Reserve, Probe>);
  apply_iteration_diagnostic_args(benchmark);
}

template <class Set, reserve_policy Reserve, miss_bucket Bucket>
void register_lookup_miss_bucket_diagnostic(std::string_view container_name, std::string_view operation) {
  auto* benchmark = benchmark::RegisterBenchmark(
      (std::string(container_name) + "/" + std::string(operation)).c_str(),
      &bench_lookup_miss_bucket_class<Set, Reserve, Bucket>);
  apply_iteration_diagnostic_args(benchmark);
}

template <class Set>
void register_iteration_diagnostics_for(std::string_view container_name) {
  register_iteration_diagnostic<Set, key_order::sequential, reserve_policy::natural_growth>(
      container_name, "iterate_sum_seq_natural");
  register_iteration_diagnostic<Set, key_order::sequential, reserve_policy::reserve_first>(
      container_name, "iterate_sum_seq_reserved");
  register_iteration_diagnostic<Set, key_order::random, reserve_policy::natural_growth>(
      container_name, "iterate_sum_random_natural");
  register_iteration_diagnostic<Set, key_order::random, reserve_policy::reserve_first>(
      container_name, "iterate_sum_random_reserved");
  register_lookup_miss_diagnostic<Set, reserve_policy::natural_growth, miss_probe::near_range>(
      container_name, "lookup_miss_near_natural");
  register_lookup_miss_diagnostic<Set, reserve_policy::reserve_first, miss_probe::near_range>(
      container_name, "lookup_miss_near_reserved");
  register_lookup_miss_diagnostic<Set, reserve_policy::natural_growth, miss_probe::far_range>(
      container_name, "lookup_miss_far_natural");
  register_lookup_miss_diagnostic<Set, reserve_policy::reserve_first, miss_probe::far_range>(
      container_name, "lookup_miss_far_reserved");
  register_lookup_miss_bucket_diagnostic<Set, reserve_policy::reserve_first, miss_bucket::empty>(
      container_name, "lookup_miss_empty_reserved");
  register_lookup_miss_bucket_diagnostic<Set, reserve_policy::reserve_first, miss_bucket::occupied>(
      container_name, "lookup_miss_occupied_reserved");
}

}  // namespace

void register_unordered_iteration_diagnostics() {
  register_iteration_diagnostics_for<std::unordered_set<int>>("std::unordered_set<int>");
  register_iteration_diagnostics_for<chaistl::unordered_set<int>>("chaistl::unordered_set<int>");
}

CHAISTL_BENCHMARK_SMOKE_DOMAIN(
    hash,
    "^(std|chaistl)::unordered_set<int>/iterate_sum_(seq|random)_(natural|reserved)/4096/100$")
CHAISTL_REGISTER_BENCHMARK_FILE(register_unordered_iteration_diagnostics)

}  // namespace chaistl_benchmark
