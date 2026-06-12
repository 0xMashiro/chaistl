// SPDX-License-Identifier: Apache-2.0

// Focused iterator microbenchmarks. The broader std_algorithms.cpp benchmarks
// show end-to-end algorithm behavior; this file separates basic traversal costs
// so regressions can be attributed to iterator increment/dereference paths.

#include <chaistl/containers/deque.hpp>
#include <chaistl/containers/forward_list.hpp>
#include <chaistl/containers/list.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/containers/vector.hpp>

#include <benchmark/benchmark.h>
#include <cstddef>
#include <deque>
#include <forward_list>
#include <iterator>
#include <list>
#include <numeric>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "chaistl/registry.hpp"

namespace chaistl_benchmark {
namespace {

auto make_iterator_values(std::size_t count) -> std::vector<int> {
  std::vector<int> values;
  values.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    values.push_back(static_cast<int>(index + 1));
  }
  return values;
}

template <class Container>
auto make_iterator_container(std::size_t count) -> Container {
  const auto values = make_iterator_values(count);
  return Container(values.begin(), values.end());
}

template <class Container>
void bench_iterator_advance_full(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_iterator_container<Container>(count);

  for (auto _ : state) {
    auto it = container.begin();
    std::ranges::advance(it, static_cast<typename Container::difference_type>(count), container.end());
    benchmark::DoNotOptimize(it);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_iterator_manual_sum(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_iterator_container<Container>(count);

  for (auto _ : state) {
    long long sum = 0;
    for (auto it = container.begin(), last = container.end(); it != last; ++it) {
      sum += *it;
    }
    benchmark::DoNotOptimize(sum);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_iterator_range_for_sum(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_iterator_container<Container>(count);

  for (auto _ : state) {
    long long sum = 0;
    for (int value : container) {
      sum += value;
    }
    benchmark::DoNotOptimize(sum);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

void apply_iterator_args(benchmark::Benchmark* benchmark) {
  benchmark->Arg(128)->Arg(4096)->Arg(65536)->Unit(benchmark::kNanosecond);
}

template <class Container>
void register_iterator_micro_benchmarks_for(std::string_view container_name) {
  auto register_benchmark = [container_name](std::string_view operation, auto* function) {
    auto* benchmark =
        benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), function);
    apply_iterator_args(benchmark);
  };

  register_benchmark("iterator_advance_full", &bench_iterator_advance_full<Container>);
  register_benchmark("iterator_manual_sum", &bench_iterator_manual_sum<Container>);
  register_benchmark("iterator_range_for_sum", &bench_iterator_range_for_sum<Container>);
}

}  // namespace

void register_iterator_micro_benchmarks() {
  register_iterator_micro_benchmarks_for<std::vector<int>>("std::vector<int>");
  register_iterator_micro_benchmarks_for<chaistl::vector<int>>("chaistl::vector<int>");

  register_iterator_micro_benchmarks_for<std::deque<int>>("std::deque<int>");
  register_iterator_micro_benchmarks_for<chaistl::deque<int>>("chaistl::deque<int>");

  register_iterator_micro_benchmarks_for<std::list<int>>("std::list<int>");
  register_iterator_micro_benchmarks_for<chaistl::list<int>>("chaistl::list<int>");

  register_iterator_micro_benchmarks_for<std::forward_list<int>>("std::forward_list<int>");
  register_iterator_micro_benchmarks_for<chaistl::forward_list<int>>("chaistl::forward_list<int>");

  register_iterator_micro_benchmarks_for<std::set<int>>("std::set<int>");
  register_iterator_micro_benchmarks_for<chaistl::set<int>>("chaistl::set<int>");

  register_iterator_micro_benchmarks_for<std::unordered_set<int>>("std::unordered_set<int>");
  register_iterator_micro_benchmarks_for<chaistl::unordered_set<int>>("chaistl::unordered_set<int>");
}

CHAISTL_BENCHMARK_SMOKE_DOMAIN(algorithm,
                               "^(std|chaistl)::(vector|deque|list|forward_list|set|unordered_set)<int>/"
                               "iterator_manual_sum/128$")
CHAISTL_REGISTER_BENCHMARK_FILE(register_iterator_micro_benchmarks)

}  // namespace chaistl_benchmark
