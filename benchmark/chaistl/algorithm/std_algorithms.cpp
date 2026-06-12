// SPDX-License-Identifier: Apache-2.0

// These benchmarks measure standard algorithms running over container
// iterators. They complement member-function benchmarks by checking the cost of
// the generic STL surface that user code naturally composes with.

#include <chaistl/containers/deque.hpp>
#include <chaistl/containers/forward_list.hpp>
#include <chaistl/containers/list.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/containers/vector.hpp>

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <deque>
#include <forward_list>
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

auto make_values(std::size_t count) -> std::vector<int> {
  std::vector<int> values;
  values.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    values.push_back(static_cast<int>((count - index) * 2));
  }
  return values;
}

template <class Container>
auto make_container(std::size_t count) -> Container {
  const auto values = make_values(count);
  return Container(values.begin(), values.end());
}

template <class Container>
void bench_ranges_find(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_container<Container>(count);

  for (auto _ : state) {
    auto found = std::ranges::find(container, -1);
    benchmark::DoNotOptimize(found);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_count_if(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_container<Container>(count);

  for (auto _ : state) {
    auto matches = std::ranges::count_if(container, [](int value) {
      return (value & 3) == 0;
    });
    benchmark::DoNotOptimize(matches);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_all_of(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_container<Container>(count);

  for (auto _ : state) {
    auto all_positive = std::ranges::all_of(container, [](int value) {
      return value > 0;
    });
    benchmark::DoNotOptimize(all_positive);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_equal(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_container<Container>(count);
  const std::vector<int> oracle(container.begin(), container.end());

  for (auto _ : state) {
    auto equal = std::ranges::equal(container, oracle);
    benchmark::DoNotOptimize(equal);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_std_accumulate(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_container<Container>(count);

  for (auto _ : state) {
    auto sum = std::accumulate(container.begin(), container.end(), 0LL);
    benchmark::DoNotOptimize(sum);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_copy(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_container<Container>(count);
  std::vector<int> output(count);

  for (auto _ : state) {
    auto result = std::ranges::copy(container, output.begin());
    benchmark::DoNotOptimize(result);
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_transform(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto container = make_container<Container>(count);
  std::vector<int> output(count);

  for (auto _ : state) {
    auto result = std::ranges::transform(container, output.begin(), [](int value) {
      return value + 1;
    });
    benchmark::DoNotOptimize(result);
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_lower_bound(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  auto values = make_values(count);
  std::ranges::sort(values);
  const Container container(values.begin(), values.end());
  const int needle = values.empty() ? 0 : values[values.size() / 2];

  for (auto _ : state) {
    auto found = std::ranges::lower_bound(container, needle);
    benchmark::DoNotOptimize(found);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_binary_search(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  auto values = make_values(count);
  std::ranges::sort(values);
  const Container container(values.begin(), values.end());
  const int needle = values.empty() ? 0 : values[values.size() / 2];

  for (auto _ : state) {
    auto found = std::ranges::binary_search(container, needle);
    benchmark::DoNotOptimize(found);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_equal_range(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  auto values = make_values(count);
  std::ranges::sort(values);
  const Container container(values.begin(), values.end());
  const int needle = values.empty() ? 0 : values[values.size() / 2];

  for (auto _ : state) {
    auto range = std::ranges::equal_range(container, needle);
    benchmark::DoNotOptimize(range);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_sort(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto input = make_values(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    std::ranges::sort(container);
    benchmark::DoNotOptimize(&container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_stable_sort(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto input = make_values(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    std::ranges::stable_sort(container);
    benchmark::DoNotOptimize(&container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_nth_element(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto input = make_values(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    auto middle = container.begin() + static_cast<typename Container::difference_type>(count / 2);
    state.ResumeTiming();

    std::ranges::nth_element(container, middle);
    benchmark::DoNotOptimize(&container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_make_heap_sort_heap(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto input = make_values(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    std::ranges::make_heap(container);
    std::ranges::sort_heap(container);
    benchmark::DoNotOptimize(&container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_reverse(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto input = make_values(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    std::ranges::reverse(container);
    benchmark::DoNotOptimize(&container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_rotate(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto input = make_values(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    auto middle = std::next(container.begin(), static_cast<typename Container::difference_type>(count / 2));
    state.ResumeTiming();

    auto result = std::ranges::rotate(container, middle);
    benchmark::DoNotOptimize(result);
    benchmark::DoNotOptimize(&container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_ranges_partition(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto input = make_values(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    auto result = std::ranges::partition(container, [](int value) {
      return (value & 3) == 0;
    });
    benchmark::DoNotOptimize(result);
    benchmark::DoNotOptimize(&container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

void apply_algorithm_args(benchmark::Benchmark* benchmark) {
  benchmark->Arg(128)->Arg(4096)->Arg(65536)->Unit(benchmark::kNanosecond);
}

void apply_mutating_algorithm_args(benchmark::Benchmark* benchmark) {
  benchmark->Arg(128)->Arg(4096)->Arg(16384)->Unit(benchmark::kNanosecond);
}

template <class Container>
void register_algorithm_benchmark(std::string_view container_name,
                                  std::string_view operation,
                                  void (*function)(benchmark::State&),
                                  void (*apply_args)(benchmark::Benchmark*) = apply_algorithm_args) {
  auto* benchmark =
      benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), function);
  apply_args(benchmark);
}

template <class Container>
void register_algorithm_read_benchmarks(std::string_view container_name) {
  register_algorithm_benchmark<Container>(container_name, "ranges_find", &bench_ranges_find<Container>);
  register_algorithm_benchmark<Container>(container_name, "ranges_count_if", &bench_ranges_count_if<Container>);
  register_algorithm_benchmark<Container>(container_name, "ranges_all_of", &bench_ranges_all_of<Container>);
  register_algorithm_benchmark<Container>(container_name, "ranges_equal", &bench_ranges_equal<Container>);
  register_algorithm_benchmark<Container>(container_name, "std_accumulate", &bench_std_accumulate<Container>);
}

template <class Container>
void register_algorithm_output_benchmarks(std::string_view container_name) {
  register_algorithm_benchmark<Container>(container_name, "ranges_copy", &bench_ranges_copy<Container>);
  register_algorithm_benchmark<Container>(container_name, "ranges_transform", &bench_ranges_transform<Container>);
}

template <class Container>
void register_algorithm_sort_benchmark(std::string_view container_name) {
  register_algorithm_benchmark<Container>(container_name, "ranges_sort", &bench_ranges_sort<Container>,
                                          apply_mutating_algorithm_args);
  register_algorithm_benchmark<Container>(container_name, "ranges_stable_sort", &bench_ranges_stable_sort<Container>,
                                          apply_mutating_algorithm_args);
  register_algorithm_benchmark<Container>(container_name, "ranges_nth_element", &bench_ranges_nth_element<Container>,
                                          apply_mutating_algorithm_args);
  register_algorithm_benchmark<Container>(container_name, "ranges_make_heap_sort_heap",
                                          &bench_ranges_make_heap_sort_heap<Container>,
                                          apply_mutating_algorithm_args);
}

template <class Container>
void register_algorithm_reverse_benchmark(std::string_view container_name) {
  register_algorithm_benchmark<Container>(container_name, "ranges_reverse", &bench_ranges_reverse<Container>,
                                          apply_mutating_algorithm_args);
}

template <class Container>
void register_algorithm_forward_permutation_benchmarks(std::string_view container_name) {
  register_algorithm_benchmark<Container>(container_name, "ranges_rotate", &bench_ranges_rotate<Container>,
                                          apply_mutating_algorithm_args);
  register_algorithm_benchmark<Container>(container_name, "ranges_partition", &bench_ranges_partition<Container>,
                                          apply_mutating_algorithm_args);
}

template <class Container>
void register_algorithm_lower_bound_benchmark(std::string_view container_name) {
  register_algorithm_benchmark<Container>(container_name, "ranges_lower_bound", &bench_ranges_lower_bound<Container>);
  register_algorithm_benchmark<Container>(container_name, "ranges_binary_search", &bench_ranges_binary_search<Container>);
  register_algorithm_benchmark<Container>(container_name, "ranges_equal_range", &bench_ranges_equal_range<Container>);
}

}  // namespace

void register_std_algorithm_benchmarks() {
  register_algorithm_read_benchmarks<std::vector<int>>("std::vector<int>");
  register_algorithm_read_benchmarks<chaistl::vector<int>>("chaistl::vector<int>");
  register_algorithm_output_benchmarks<std::vector<int>>("std::vector<int>");
  register_algorithm_output_benchmarks<chaistl::vector<int>>("chaistl::vector<int>");
  register_algorithm_sort_benchmark<std::vector<int>>("std::vector<int>");
  register_algorithm_sort_benchmark<chaistl::vector<int>>("chaistl::vector<int>");
  register_algorithm_reverse_benchmark<std::vector<int>>("std::vector<int>");
  register_algorithm_reverse_benchmark<chaistl::vector<int>>("chaistl::vector<int>");
  register_algorithm_forward_permutation_benchmarks<std::vector<int>>("std::vector<int>");
  register_algorithm_forward_permutation_benchmarks<chaistl::vector<int>>("chaistl::vector<int>");
  register_algorithm_lower_bound_benchmark<std::vector<int>>("std::vector<int>");
  register_algorithm_lower_bound_benchmark<chaistl::vector<int>>("chaistl::vector<int>");

  register_algorithm_read_benchmarks<std::deque<int>>("std::deque<int>");
  register_algorithm_read_benchmarks<chaistl::deque<int>>("chaistl::deque<int>");
  register_algorithm_output_benchmarks<std::deque<int>>("std::deque<int>");
  register_algorithm_output_benchmarks<chaistl::deque<int>>("chaistl::deque<int>");
  register_algorithm_sort_benchmark<std::deque<int>>("std::deque<int>");
  register_algorithm_sort_benchmark<chaistl::deque<int>>("chaistl::deque<int>");
  register_algorithm_reverse_benchmark<std::deque<int>>("std::deque<int>");
  register_algorithm_reverse_benchmark<chaistl::deque<int>>("chaistl::deque<int>");
  register_algorithm_forward_permutation_benchmarks<std::deque<int>>("std::deque<int>");
  register_algorithm_forward_permutation_benchmarks<chaistl::deque<int>>("chaistl::deque<int>");

  register_algorithm_read_benchmarks<std::list<int>>("std::list<int>");
  register_algorithm_read_benchmarks<chaistl::list<int>>("chaistl::list<int>");
  register_algorithm_output_benchmarks<std::list<int>>("std::list<int>");
  register_algorithm_output_benchmarks<chaistl::list<int>>("chaistl::list<int>");
  register_algorithm_reverse_benchmark<std::list<int>>("std::list<int>");
  register_algorithm_reverse_benchmark<chaistl::list<int>>("chaistl::list<int>");
  register_algorithm_forward_permutation_benchmarks<std::list<int>>("std::list<int>");
  register_algorithm_forward_permutation_benchmarks<chaistl::list<int>>("chaistl::list<int>");

  register_algorithm_read_benchmarks<std::forward_list<int>>("std::forward_list<int>");
  register_algorithm_read_benchmarks<chaistl::forward_list<int>>("chaistl::forward_list<int>");
  register_algorithm_output_benchmarks<std::forward_list<int>>("std::forward_list<int>");
  register_algorithm_output_benchmarks<chaistl::forward_list<int>>("chaistl::forward_list<int>");
  register_algorithm_forward_permutation_benchmarks<std::forward_list<int>>("std::forward_list<int>");
  register_algorithm_forward_permutation_benchmarks<chaistl::forward_list<int>>("chaistl::forward_list<int>");

  register_algorithm_read_benchmarks<std::set<int>>("std::set<int>");
  register_algorithm_read_benchmarks<chaistl::set<int>>("chaistl::set<int>");
  register_algorithm_output_benchmarks<std::set<int>>("std::set<int>");
  register_algorithm_output_benchmarks<chaistl::set<int>>("chaistl::set<int>");
  // This is the generic algorithm, not set::lower_bound; node-based sets pay
  // for bidirectional iterator traversal after the logarithmic comparisons.
  register_algorithm_lower_bound_benchmark<std::set<int>>("std::set<int>");
  register_algorithm_lower_bound_benchmark<chaistl::set<int>>("chaistl::set<int>");

  register_algorithm_read_benchmarks<std::unordered_set<int>>("std::unordered_set<int>");
  register_algorithm_read_benchmarks<chaistl::unordered_set<int>>("chaistl::unordered_set<int>");
  register_algorithm_output_benchmarks<std::unordered_set<int>>("std::unordered_set<int>");
  register_algorithm_output_benchmarks<chaistl::unordered_set<int>>("chaistl::unordered_set<int>");
}

CHAISTL_BENCHMARK_SMOKE_DOMAIN(algorithm,
                               "^(std|chaistl)::(vector|deque|list|forward_list|set|unordered_set)<int>/ranges_find/"
                               "128$")
CHAISTL_REGISTER_BENCHMARK_FILE(register_std_algorithm_benchmarks)

}  // namespace chaistl_benchmark
