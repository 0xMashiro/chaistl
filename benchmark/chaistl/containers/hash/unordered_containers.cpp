// SPDX-License-Identifier: Apache-2.0

// References:
// - Unordered containers store elements in hash buckets rather than an ordered
//   tree or sorted contiguous buffer. Average-case insert, lookup, and erase
//   are O(1), but constants depend heavily on hashing, load factor, bucket
//   growth, and node/link locality.
// - These benchmarks compare the default prime policy against std and the
//   opt-in power-of-two policy. The shifted/mixed hash cases make the policy
//   trade-off visible: mask indexing is fast only when low hash bits are good.
// - Rehash benchmarks compare reserve-then-insert against normal growth so the
//   cost of avoiding incremental rehashes is visible at the same key scale.

#include <chaistl/containers/power2_unordered_map.hpp>
#include <chaistl/containers/power2_unordered_set.hpp>
#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/experimental/containers/open_addressing_set.hpp>

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "chaistl/registry.hpp"

namespace chaistl_benchmark {

namespace {

using key_type = int;
using mapped_type = int;
using chaistl_unordered_set = chaistl::unordered_set<key_type>;
using chaistl_power2_unordered_set = chaistl::power2_unordered_set<key_type>;
using chaistl_linear_open_addressing_set = chaistl::experimental::linear_open_addressing_set<key_type>;
using chaistl_quadratic_open_addressing_set = chaistl::experimental::quadratic_open_addressing_set<key_type>;
using chaistl_double_hashing_open_addressing_set = chaistl::experimental::double_hashing_open_addressing_set<key_type>;
using std_unordered_set = std::unordered_set<key_type>;
using chaistl_unordered_map = chaistl::unordered_map<key_type, mapped_type>;
using chaistl_power2_unordered_map = chaistl::power2_unordered_map<key_type, mapped_type>;
using std_unordered_map = std::unordered_map<key_type, mapped_type>;

struct shifted_hash {
  std::size_t operator()(key_type value) const noexcept { return static_cast<std::size_t>(value) << 8u; }
};

struct mixed_hash {
  std::size_t operator()(key_type value) const noexcept {
    std::size_t x = static_cast<std::size_t>(value);
    x ^= x >> 16u;
    x *= 0x7feb352dU;
    x ^= x >> 15u;
    x *= 0x846ca68bU;
    x ^= x >> 16u;
    return x;
  }
};

using chaistl_shifted_unordered_set = chaistl::unordered_set<key_type, shifted_hash>;
using chaistl_power2_shifted_unordered_set = chaistl::power2_unordered_set<key_type, shifted_hash>;
using chaistl_linear_shifted_open_addressing_set =
    chaistl::experimental::linear_open_addressing_set<key_type, shifted_hash>;
using chaistl_quadratic_shifted_open_addressing_set =
    chaistl::experimental::quadratic_open_addressing_set<key_type, shifted_hash>;
using chaistl_double_hashing_shifted_open_addressing_set =
    chaistl::experimental::double_hashing_open_addressing_set<key_type, shifted_hash>;
using chaistl_linear_shifted_mixed_open_addressing_set =
    chaistl::experimental::open_addressing_set<key_type,
                                               shifted_hash,
                                               std::equal_to<key_type>,
                                               chaistl::experimental::linear_probing,
                                               chaistl::allocator<key_type>,
                                               chaistl::experimental::avalanche_hash_mix>;
using chaistl_quadratic_shifted_mixed_open_addressing_set =
    chaistl::experimental::open_addressing_set<key_type,
                                               shifted_hash,
                                               std::equal_to<key_type>,
                                               chaistl::experimental::quadratic_probing,
                                               chaistl::allocator<key_type>,
                                               chaistl::experimental::avalanche_hash_mix>;
using chaistl_double_hashing_shifted_mixed_open_addressing_set =
    chaistl::experimental::open_addressing_set<key_type,
                                               shifted_hash,
                                               std::equal_to<key_type>,
                                               chaistl::experimental::double_hashing,
                                               chaistl::allocator<key_type>,
                                               chaistl::experimental::avalanche_hash_mix>;
using chaistl_mixed_unordered_set = chaistl::unordered_set<key_type, mixed_hash>;
using chaistl_power2_mixed_unordered_set = chaistl::power2_unordered_set<key_type, mixed_hash>;
using chaistl_linear_mixed_open_addressing_set =
    chaistl::experimental::linear_open_addressing_set<key_type, mixed_hash>;
using chaistl_quadratic_mixed_open_addressing_set =
    chaistl::experimental::quadratic_open_addressing_set<key_type, mixed_hash>;
using chaistl_double_hashing_mixed_open_addressing_set =
    chaistl::experimental::double_hashing_open_addressing_set<key_type, mixed_hash>;

std::vector<key_type> random_keys(std::size_t count, unsigned seed) {
  std::vector<key_type> keys(count);
  std::iota(keys.begin(), keys.end(), key_type{0});
  std::mt19937 rng(seed);
  std::ranges::shuffle(keys, rng);
  return keys;
}

std::vector<key_type> random_miss_keys(std::size_t count, unsigned seed) {
  std::vector<key_type> keys(count);
  std::iota(keys.begin(), keys.end(), static_cast<key_type>(count));
  std::mt19937 rng(seed);
  std::ranges::shuffle(keys, rng);
  return keys;
}

std::vector<std::pair<key_type, mapped_type>> random_pairs(std::size_t count, unsigned seed) {
  const auto keys = random_keys(count, seed);
  std::vector<std::pair<key_type, mapped_type>> pairs;
  pairs.reserve(count);
  for (const key_type k : keys) {
    pairs.emplace_back(k, k);
  }
  return pairs;
}

// -- unordered_set benchmarks ------------------------------------------------

template <class Set>
void BM_UnorderedSet_InsertRandom(benchmark::State& state) {
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

template <class Set>
void BM_UnorderedSet_LookupHit(benchmark::State& state) {
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

template <class Set>
void BM_UnorderedSet_LookupMiss(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = random_keys(n, 42u);
  const Set s(keys.begin(), keys.end());
  const auto probes = random_miss_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type k : probes) {
      benchmark::DoNotOptimize(s.find(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Set>
void BM_UnorderedSet_EraseRandom(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = random_keys(n, 42u);
  const auto erase_order = random_keys(n, 1234u);

  for (auto _ : state) {
    state.PauseTiming();
    Set s(keys.begin(), keys.end());
    state.ResumeTiming();
    for (const key_type k : erase_order) {
      benchmark::DoNotOptimize(s.erase(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Set>
void BM_UnorderedSet_IterateSum(benchmark::State& state) {
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

template <class Set, bool ReserveFirst>
void BM_UnorderedSet_Rehash(benchmark::State& state) {
  const auto keys = random_keys(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    Set s;
    if constexpr (ReserveFirst) {
      s.reserve(keys.size());
    }
    for (const key_type k : keys) {
      s.insert(k);
    }
    benchmark::DoNotOptimize(s);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// -- unordered_map benchmarks ------------------------------------------------

template <class Map>
void BM_UnorderedMap_InsertRandom(benchmark::State& state) {
  const auto pairs = random_pairs(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    Map m;
    for (const auto& p : pairs) {
      m.insert(p);
    }
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Map>
void BM_UnorderedMap_LookupHit(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto pairs = random_pairs(n, 42u);
  const Map m(pairs.begin(), pairs.end());
  const auto probes = random_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type k : probes) {
      benchmark::DoNotOptimize(m.find(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Map>
void BM_UnorderedMap_LookupMiss(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto pairs = random_pairs(n, 42u);
  const Map m(pairs.begin(), pairs.end());
  const auto probes = random_miss_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type k : probes) {
      benchmark::DoNotOptimize(m.find(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Map>
void BM_UnorderedMap_EraseRandom(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto pairs = random_pairs(n, 42u);
  const auto erase_order = random_keys(n, 1234u);

  for (auto _ : state) {
    state.PauseTiming();
    Map m(pairs.begin(), pairs.end());
    state.ResumeTiming();
    for (const key_type k : erase_order) {
      benchmark::DoNotOptimize(m.erase(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Map>
void BM_UnorderedMap_IterateSum(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto pairs = random_pairs(n, 42u);
  const Map m(pairs.begin(), pairs.end());

  for (auto _ : state) {
    key_type sum = 0;
    for (const auto& p : m) {
      sum += p.first + p.second;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Map, bool ReserveFirst>
void BM_UnorderedMap_Rehash(benchmark::State& state) {
  const auto pairs = random_pairs(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    Map m;
    if constexpr (ReserveFirst) {
      m.reserve(pairs.size());
    }
    for (const auto& p : pairs) {
      m.insert(p);
    }
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class BenchmarkFn>
void register_unordered_benchmark(const char* container_name, const char* operation, BenchmarkFn* function) {
  benchmark::RegisterBenchmark((std::string(container_name) + "/" + operation).c_str(), function)
      ->Arg(1024)
      ->Arg(65536);
}

template <class Set>
void register_unordered_set_benchmarks(const char* container_name) {
  register_unordered_benchmark(container_name, "insert_random", &BM_UnorderedSet_InsertRandom<Set>);
  register_unordered_benchmark(container_name, "lookup_hit", &BM_UnorderedSet_LookupHit<Set>);
  register_unordered_benchmark(container_name, "lookup_miss", &BM_UnorderedSet_LookupMiss<Set>);
  register_unordered_benchmark(container_name, "erase_random", &BM_UnorderedSet_EraseRandom<Set>);
  register_unordered_benchmark(container_name, "iterate_sum", &BM_UnorderedSet_IterateSum<Set>);
  register_unordered_benchmark(container_name, "insert_reserved", &BM_UnorderedSet_Rehash<Set, true>);
  register_unordered_benchmark(container_name, "insert_no_reserve", &BM_UnorderedSet_Rehash<Set, false>);
}

template <class Set>
void register_hash_quality_set_benchmarks(const char* container_name) {
  register_unordered_benchmark(container_name, "lookup_hit", &BM_UnorderedSet_LookupHit<Set>);
  register_unordered_benchmark(container_name, "lookup_miss", &BM_UnorderedSet_LookupMiss<Set>);
  register_unordered_benchmark(container_name, "insert_reserved", &BM_UnorderedSet_Rehash<Set, true>);
  register_unordered_benchmark(container_name, "insert_no_reserve", &BM_UnorderedSet_Rehash<Set, false>);
}

template <class Map>
void register_unordered_map_benchmarks(const char* container_name) {
  register_unordered_benchmark(container_name, "insert_random", &BM_UnorderedMap_InsertRandom<Map>);
  register_unordered_benchmark(container_name, "lookup_hit", &BM_UnorderedMap_LookupHit<Map>);
  register_unordered_benchmark(container_name, "lookup_miss", &BM_UnorderedMap_LookupMiss<Map>);
  register_unordered_benchmark(container_name, "erase_random", &BM_UnorderedMap_EraseRandom<Map>);
  register_unordered_benchmark(container_name, "iterate_sum", &BM_UnorderedMap_IterateSum<Map>);
  register_unordered_benchmark(container_name, "insert_reserved", &BM_UnorderedMap_Rehash<Map, true>);
  register_unordered_benchmark(container_name, "insert_no_reserve", &BM_UnorderedMap_Rehash<Map, false>);
}

const int registered_unordered_benchmarks = [] {
  register_unordered_set_benchmarks<chaistl_unordered_set>("chaistl::unordered_set<int>");
  register_unordered_set_benchmarks<chaistl_power2_unordered_set>("chaistl::power2_unordered_set<int>");
  register_unordered_set_benchmarks<chaistl_linear_open_addressing_set>(
      "chaistl::experimental::linear_open_addressing_set<int>");
  register_unordered_set_benchmarks<chaistl_quadratic_open_addressing_set>(
      "chaistl::experimental::quadratic_open_addressing_set<int>");
  register_unordered_set_benchmarks<chaistl_double_hashing_open_addressing_set>(
      "chaistl::experimental::double_hashing_open_addressing_set<int>");
  register_unordered_set_benchmarks<std_unordered_set>("std::unordered_set<int>");
  register_unordered_map_benchmarks<chaistl_unordered_map>("chaistl::unordered_map<int,int>");
  register_unordered_map_benchmarks<chaistl_power2_unordered_map>("chaistl::power2_unordered_map<int,int>");
  register_unordered_map_benchmarks<std_unordered_map>("std::unordered_map<int,int>");

  register_hash_quality_set_benchmarks<chaistl_shifted_unordered_set>("chaistl::unordered_set<int,shifted_hash>");
  register_hash_quality_set_benchmarks<chaistl_power2_shifted_unordered_set>(
      "chaistl::power2_unordered_set<int,shifted_hash>");
  register_hash_quality_set_benchmarks<chaistl_linear_shifted_open_addressing_set>(
      "chaistl::experimental::linear_open_addressing_set<int,shifted_hash>");
  register_hash_quality_set_benchmarks<chaistl_quadratic_shifted_open_addressing_set>(
      "chaistl::experimental::quadratic_open_addressing_set<int,shifted_hash>");
  register_hash_quality_set_benchmarks<chaistl_double_hashing_shifted_open_addressing_set>(
      "chaistl::experimental::double_hashing_open_addressing_set<int,shifted_hash>");
  register_hash_quality_set_benchmarks<chaistl_linear_shifted_mixed_open_addressing_set>(
      "chaistl::experimental::linear_open_addressing_set<int,shifted_hash,avalanche_hash_mix>");
  register_hash_quality_set_benchmarks<chaistl_quadratic_shifted_mixed_open_addressing_set>(
      "chaistl::experimental::quadratic_open_addressing_set<int,shifted_hash,avalanche_hash_mix>");
  register_hash_quality_set_benchmarks<chaistl_double_hashing_shifted_mixed_open_addressing_set>(
      "chaistl::experimental::double_hashing_open_addressing_set<int,shifted_hash,avalanche_hash_mix>");
  register_hash_quality_set_benchmarks<chaistl_mixed_unordered_set>("chaistl::unordered_set<int,mixed_hash>");
  register_hash_quality_set_benchmarks<chaistl_power2_mixed_unordered_set>(
      "chaistl::power2_unordered_set<int,mixed_hash>");
  register_hash_quality_set_benchmarks<chaistl_linear_mixed_open_addressing_set>(
      "chaistl::experimental::linear_open_addressing_set<int,mixed_hash>");
  register_hash_quality_set_benchmarks<chaistl_quadratic_mixed_open_addressing_set>(
      "chaistl::experimental::quadratic_open_addressing_set<int,mixed_hash>");
  register_hash_quality_set_benchmarks<chaistl_double_hashing_mixed_open_addressing_set>(
      "chaistl::experimental::double_hashing_open_addressing_set<int,mixed_hash>");
  return 0;
}();

}  // namespace

CHAISTL_BENCHMARK_SMOKE_DOMAIN(
    hash,
    "^((std|chaistl)::unordered_(set<int>|map<int,int>)|chaistl::power2_unordered_set<int>|"
    "chaistl::experimental::(linear|quadratic|double_hashing)_open_addressing_set<int>)/"
    "(insert_random|lookup_hit|lookup_miss|erase_random|iterate_sum|insert_reserved|insert_no_reserve)/1024$")

}  // namespace chaistl_benchmark
