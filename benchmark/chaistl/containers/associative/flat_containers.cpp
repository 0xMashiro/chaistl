// SPDX-License-Identifier: Apache-2.0

// References:
// - Flat containers store elements in a sorted contiguous buffer rather than a
//   node-based tree. Insert is O(n) due to shifting but lookup and iteration
//   are cache-friendly, often winning over node-based trees on small-to-medium
//   N due to memory locality (Boost.Container docs, P0429R9 rationale).
// - These benchmarks compare chaistl::flat_set against chaistl::set (RB-tree)
//   and std::set, and measure bulk vs. single-element insert for flat_map.
// - Multi-container suites use keys with a compressed value range (N/4) to
//   guarantee many duplicate keys, exercising equal_range paths.

#include <chaistl/containers/flat_map.hpp>
#include <chaistl/containers/flat_multimap.hpp>
#include <chaistl/containers/flat_multiset.hpp>
#include <chaistl/containers/flat_set.hpp>
#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/set.hpp>

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "chaistl/registry.hpp"

namespace chaistl_benchmark {

namespace {

using key_type = int;

std::vector<key_type> random_keys(std::size_t count, unsigned seed) {
  std::vector<key_type> keys(count);
  std::iota(keys.begin(), keys.end(), key_type{0});
  std::mt19937 rng(seed);
  std::ranges::shuffle(keys, rng);
  return keys;
}

// ── insert_random ────────────────────────────────────────────────────────────
// Build the whole container by inserting N randomly-ordered keys one at a
// time. For flat_set this exercises O(n) shifting on every insert.
template <class Set>
void bench_insert_random(benchmark::State& state) {
  const auto keys = random_keys(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    Set s;
    for (const key_type k : keys) {
      s.insert(k);
    }
    benchmark::DoNotOptimize(s);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// ── lookup_hit ───────────────────────────────────────────────────────────────
// Successful searches in a pre-built container probed in an order uncorrelated
// with the build order: flat_set uses binary search, tree uses pointer chasing.
template <class Set>
void bench_lookup_hit(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = random_keys(n, 42u);
  const Set s(keys.begin(), keys.end());
  const auto probes = random_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type k : probes) {
      benchmark::DoNotOptimize(s.find(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// ── iterate ──────────────────────────────────────────────────────────────────
// Full traversal summing all elements. flat_set walks a contiguous array;
// tree-based sets walk a linked structure — cache behaviour dominates here.
template <class Set>
void bench_iterate(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = random_keys(n, 42u);
  const Set s(keys.begin(), keys.end());

  for (auto _ : state) {
    key_type sum = 0;
    for (const key_type k : s) {
      sum += k;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// ── flat_map bulk vs. single-element insert ──────────────────────────────────
// Bulk insert (range overload) lets flat_map sort-and-merge in one pass;
// single-element insert pays O(n) shift on every call.

std::vector<std::pair<int, int>> random_pairs(std::size_t count, unsigned seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> dist(0, static_cast<int>(count) * 4);
  std::vector<std::pair<int, int>> pairs;
  pairs.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    pairs.emplace_back(dist(rng), dist(rng));
  }
  return pairs;
}

void bench_flat_map_bulk_insert(benchmark::State& state) {
  const auto pairs = random_pairs(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    chaistl::flat_map<int, int> m;
    m.insert(pairs.begin(), pairs.end());
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void bench_flat_map_single_insert(benchmark::State& state) {
  const auto pairs = random_pairs(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    chaistl::flat_map<int, int> m;
    for (const auto& p : pairs) {
      m.insert(p);
    }
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// ── duplicate-key helper ─────────────────────────────────────────────────────
// Generates N keys drawn from [0, N/4), guaranteeing substantial repetition.
std::vector<key_type> random_duplicate_keys(std::size_t count, unsigned seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<key_type> dist(0, static_cast<key_type>(count / 4));
  std::vector<key_type> keys(count);
  for (auto& k : keys) {
    k = dist(rng);
  }
  return keys;
}

// ── multiset benchmarks ───────────────────────────────────────────────────────
// Same structure as the set suite but uses duplicate keys so that equal_range
// paths are exercised. The key generator is passed as a template parameter to
// allow register_set_suite to remain unmodified.

template <class MultiSet>
void bench_multiset_insert_random(benchmark::State& state) {
  const auto keys = random_duplicate_keys(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    MultiSet s;
    for (const key_type k : keys) {
      s.insert(k);
    }
    benchmark::DoNotOptimize(s);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class MultiSet>
void bench_multiset_lookup_hit(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = random_duplicate_keys(n, 42u);
  const MultiSet s(keys.begin(), keys.end());
  const auto probes = random_duplicate_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type k : probes) {
      benchmark::DoNotOptimize(s.find(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class MultiSet>
void bench_multiset_iterate(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = random_duplicate_keys(n, 42u);
  const MultiSet s(keys.begin(), keys.end());

  for (auto _ : state) {
    key_type sum = 0;
    for (const key_type k : s) {
      sum += k;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class MultiSet>
void register_multiset_suite(std::string_view container_name) {
  const auto reg = [container_name](std::string_view operation, auto* fn) {
    benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), fn)
        ->RangeMultiplier(4)
        ->Range(1 << 10, 1 << 14);  // 1024 .. 16384
  };

  reg("insert_random", &bench_multiset_insert_random<MultiSet>);
  reg("lookup_hit", &bench_multiset_lookup_hit<MultiSet>);
  reg("iterate", &bench_multiset_iterate<MultiSet>);
}

// ── flat_multimap bulk vs. single-element insert ─────────────────────────────
// Key range is compressed to N/4 to produce many duplicate keys.

std::vector<std::pair<int, int>> random_duplicate_pairs(std::size_t count, unsigned seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> dist(0, static_cast<int>(count / 4));
  std::vector<std::pair<int, int>> pairs;
  pairs.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    pairs.emplace_back(dist(rng), dist(rng));
  }
  return pairs;
}

void bench_flat_multimap_bulk_insert(benchmark::State& state) {
  const auto pairs = random_duplicate_pairs(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    chaistl::flat_multimap<int, int> m;
    m.insert(pairs.begin(), pairs.end());
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void bench_flat_multimap_single_insert(benchmark::State& state) {
  const auto pairs = random_duplicate_pairs(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    chaistl::flat_multimap<int, int> m;
    for (const auto& p : pairs) {
      m.insert(p);
    }
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// ── registration helpers ─────────────────────────────────────────────────────

template <class Set>
void register_set_suite(std::string_view container_name) {
  const auto reg = [container_name](std::string_view operation, auto* fn) {
    benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), fn)
        ->RangeMultiplier(4)
        ->Range(1 << 10, 1 << 14);  // 1024 .. 16384
  };

  reg("insert_random", &bench_insert_random<Set>);
  reg("lookup_hit", &bench_lookup_hit<Set>);
  reg("iterate", &bench_iterate<Set>);
}

}  // namespace

void register_flat_container_benchmarks() {
  register_set_suite<chaistl::flat_set<key_type>>("flat_set");
  register_set_suite<chaistl::set<key_type>>("set_rb");
  register_set_suite<std::set<key_type>>("set_std");

  // flat_map bulk vs. single-element insert
  const auto reg_map = [](std::string_view operation, auto* fn) {
    benchmark::RegisterBenchmark(("flat_map/" + std::string(operation)).c_str(), fn)
        ->RangeMultiplier(4)
        ->Range(1 << 10, 1 << 14);
  };
  reg_map("bulk_insert", &bench_flat_map_bulk_insert);
  reg_map("single_insert", &bench_flat_map_single_insert);

  // multi-container suites (duplicate keys, value range = N/4)
  register_multiset_suite<chaistl::flat_multiset<key_type>>("flat_multiset");
  register_multiset_suite<chaistl::multiset<key_type>>("multiset_rb");
  register_multiset_suite<std::multiset<key_type>>("multiset_std");

  // flat_multimap bulk vs. single-element insert
  const auto reg_mmap = [](std::string_view operation, auto* fn) {
    benchmark::RegisterBenchmark(("flat_multimap/" + std::string(operation)).c_str(), fn)
        ->RangeMultiplier(4)
        ->Range(1 << 10, 1 << 14);
  };
  reg_mmap("bulk_insert", &bench_flat_multimap_bulk_insert);
  reg_mmap("single_insert", &bench_flat_multimap_single_insert);
}

CHAISTL_BENCHMARK_SMOKE_DOMAIN(flat,
                               "^(flat_set|set_rb|set_std)/(insert_random|lookup_hit)/1024$|^flat_map/"
                               "(bulk_insert|single_insert)/1024$|^(flat_multiset|multiset_rb|multiset_std)/"
                               "(insert_random|lookup_hit)/1024$|^flat_multimap/(bulk_insert|single_insert)/1024$")
CHAISTL_REGISTER_BENCHMARK_FILE(register_flat_container_benchmarks)

}  // namespace chaistl_benchmark
