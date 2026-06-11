// SPDX-License-Identifier: Apache-2.0

// References:
// - Public policy aliases let benchmark users compare tree choices without
//   spelling detail::tree policy types directly.
// - Knuth TAOCP 6.2.3 and the classic folklore: AVL trees are more rigidly
//   balanced (height <= ~1.44 log2 n vs ~2 log2 n for red-black), trading
//   more rotation work on mutation for shorter search paths. These
//   benchmarks measure where that trade-off actually lands.

#include <chaistl/containers/avl_set.hpp>
#include <chaistl/containers/order_statistic_set.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/containers/size_balanced_set.hpp>
#include <chaistl/containers/skip_list_multiset.hpp>
#include <chaistl/containers/skip_list_set.hpp>
#include <chaistl/containers/splay_set.hpp>
#include <chaistl/containers/treap_set.hpp>
#include <chaistl/containers/weight_balanced_set.hpp>

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace chaistl_benchmark {

namespace {

using key_type = std::int64_t;

using rb_set = chaistl::set<key_type>;  // default policy: red-black
using avl_set = chaistl::avl_set<key_type>;
using size_balanced_set = chaistl::size_balanced_set<key_type>;
using splay_set = chaistl::splay_set<key_type>;
using treap_set = chaistl::treap_set<key_type>;
using weight_balanced_set = chaistl::weight_balanced_set<key_type>;
using order_statistic_set = chaistl::order_statistic_set<key_type>;
using skip_list_multiset = chaistl::skip_list_multiset<key_type>;
using skip_list_set = chaistl::skip_list_set<key_type>;
using std_set = std::set<key_type>;

std::vector<key_type> shuffled_keys(std::size_t count, unsigned seed) {
  std::vector<key_type> keys(count);
  std::iota(keys.begin(), keys.end(), key_type{0});
  std::mt19937_64 rng(seed);
  std::ranges::shuffle(keys, rng);
  return keys;
}

// Build the whole container from uniformly random keys. Rotation-heavy for
// both policies; AVL pays for its tighter invariant here.
template <class Set>
void bench_insert_random(benchmark::State& state) {
  const auto keys = shuffled_keys(static_cast<std::size_t>(state.range(0)), 42u);
  for (auto _ : state) {
    Set s;
    for (const key_type k : keys) {
      s.insert(k);
    }
    benchmark::DoNotOptimize(s);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// Monotone insertion (no hint): every insert lands at the rightmost edge
// and immediately violates balance — the rebalancing fast paths dominate.
template <class Set>
void bench_insert_sorted(benchmark::State& state) {
  const key_type n = state.range(0);
  for (auto _ : state) {
    Set s;
    for (key_type k = 0; k < n; ++k) {
      s.insert(k);
    }
    benchmark::DoNotOptimize(s);
  }
  state.SetItemsProcessed(state.iterations() * n);
}

// Successful searches in a randomly built tree, probed in an order
// uncorrelated with the build. For RB/AVL/Treap/std this is a path-length
// comparison; for Splay it also measures the cost/benefit of self-adjustment.
template <class Set>
void bench_lookup_hit(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = shuffled_keys(n, 42u);
  const Set s(keys.begin(), keys.end());
  const auto probes = shuffled_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type k : probes) {
      benchmark::DoNotOptimize(s.find(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// Sequential-access workload: build from random keys, then repeatedly scan
// adjacent keys in sorted order. This is the textbook locality pattern where a
// splay tree's self-adjustment is expected to matter (sequential access
// theorem), and contrasts with the random lookup benchmark above.
template <class Set>
void bench_lookup_sequential_scan(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = shuffled_keys(n, 42u);
  Set s(keys.begin(), keys.end());

  for (auto _ : state) {
    for (key_type k = 0; k < static_cast<key_type>(n); ++k) {
      benchmark::DoNotOptimize(s.find(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// Unsuccessful searches always run to a null link: the full-height walk.
template <class Set>
void bench_lookup_miss(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = shuffled_keys(n, 42u);
  const Set s(keys.begin(), keys.end());
  auto probes = shuffled_keys(n, 1234u);
  for (key_type& k : probes) {
    k += static_cast<key_type>(n);  // disjoint key range: every find misses
  }

  for (auto _ : state) {
    for (const key_type k : probes) {
      benchmark::DoNotOptimize(s.find(k));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// Steady-state mutation: erase one key, reinsert it. Size is constant, so
// this isolates per-mutation rebalancing cost at a given tree height.
template <class Set>
void bench_churn(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = shuffled_keys(n, 42u);
  Set s(keys.begin(), keys.end());
  const auto order = shuffled_keys(n, 99u);

  std::size_t i = 0;
  for (auto _ : state) {
    const key_type k = order[i];
    i = (i + 1) % n;
    s.erase(k);
    s.insert(k);
  }
  state.SetItemsProcessed(state.iterations() * 2);
}

// Read-heavy steady state (90% find / 10% erase+insert): the workload the
// AVL trade-off is supposed to win.
template <class Set>
void bench_mixed_read_heavy(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = shuffled_keys(n, 42u);
  Set s(keys.begin(), keys.end());
  const auto order = shuffled_keys(n, 99u);

  std::size_t i = 0;
  for (auto _ : state) {
    const key_type k = order[i];
    i = (i + 1) % n;
    if (i % 10 == 0) {
      s.erase(k);
      s.insert(k);
    } else {
      benchmark::DoNotOptimize(s.find(k));
    }
  }
  state.SetItemsProcessed(state.iterations());
}

template <class Set>
void bench_find_by_order(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = shuffled_keys(n, 42u);
  const Set s(keys.begin(), keys.end());
  const auto probes = shuffled_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type probe : probes) {
      benchmark::DoNotOptimize(s.find_by_order(static_cast<std::size_t>(probe)));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void bench_std_find_by_order_linear(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = shuffled_keys(n, 42u);
  const std_set s(keys.begin(), keys.end());
  const auto probes = shuffled_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type probe : probes) {
      benchmark::DoNotOptimize(std::next(s.begin(), static_cast<std::ptrdiff_t>(probe)));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Set>
void bench_order_of_key(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = shuffled_keys(n, 42u);
  const Set s(keys.begin(), keys.end());
  const auto probes = shuffled_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type probe : probes) {
      benchmark::DoNotOptimize(s.order_of_key(probe));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void bench_std_order_of_key_linear(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto keys = shuffled_keys(n, 42u);
  const std_set s(keys.begin(), keys.end());
  const auto probes = shuffled_keys(n, 1234u);

  for (auto _ : state) {
    for (const key_type probe : probes) {
      benchmark::DoNotOptimize(std::distance(s.begin(), s.lower_bound(probe)));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Set>
void register_policy_suite(std::string_view container_name) {
  const auto register_one = [container_name](std::string_view operation, auto* function) {
    benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), function)
        ->RangeMultiplier(8)
        ->Range(1 << 10, 1 << 16);
  };

  register_one("insert_random", &bench_insert_random<Set>);
  register_one("insert_sorted", &bench_insert_sorted<Set>);
  register_one("lookup_hit", &bench_lookup_hit<Set>);
  register_one("lookup_sequential_scan", &bench_lookup_sequential_scan<Set>);
  register_one("lookup_miss", &bench_lookup_miss<Set>);
  register_one("churn_erase_insert", &bench_churn<Set>);
  register_one("mixed_read_heavy", &bench_mixed_read_heavy<Set>);
}

void register_order_statistic_suite() {
  const auto register_augmented = [](std::string_view container_name, auto find_by_order_fn, auto order_of_key_fn) {
    benchmark::RegisterBenchmark((std::string(container_name) + "/find_by_order").c_str(), find_by_order_fn)
        ->RangeMultiplier(4)
        ->Range(1 << 10, 1 << 14);
    benchmark::RegisterBenchmark((std::string(container_name) + "/order_of_key").c_str(), order_of_key_fn)
        ->RangeMultiplier(4)
        ->Range(1 << 10, 1 << 14);
  };

  register_augmented(
      "order_statistic_set", &bench_find_by_order<order_statistic_set>, &bench_order_of_key<order_statistic_set>);
  register_augmented(
      "set_size_balanced", &bench_find_by_order<size_balanced_set>, &bench_order_of_key<size_balanced_set>);
  register_augmented(
      "set_weight_balanced", &bench_find_by_order<weight_balanced_set>, &bench_order_of_key<weight_balanced_set>);

  benchmark::RegisterBenchmark("set_std/find_by_order_linear", &bench_std_find_by_order_linear)
      ->RangeMultiplier(4)
      ->Range(1 << 10, 1 << 14);
  benchmark::RegisterBenchmark("set_std/order_of_key_linear", &bench_std_order_of_key_linear)
      ->RangeMultiplier(4)
      ->Range(1 << 10, 1 << 14);
}

}  // namespace

void register_tree_policy_benchmarks() {
  register_policy_suite<rb_set>("set_rb");
  register_policy_suite<avl_set>("set_avl");
  register_policy_suite<splay_set>("set_splay");
  register_policy_suite<treap_set>("set_treap");
  register_policy_suite<size_balanced_set>("set_size_balanced");
  register_policy_suite<weight_balanced_set>("set_weight_balanced");
  register_policy_suite<skip_list_multiset>("multiset_skip_list");
  register_policy_suite<skip_list_set>("set_skip_list");
  register_policy_suite<std_set>("set_std");
  register_order_statistic_suite();
}

}  // namespace chaistl_benchmark
