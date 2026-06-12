// SPDX-License-Identifier: Apache-2.0

#include <chaistl/algorithm/sort.hpp>
#include <chaistl/containers/deque.hpp>
#include <chaistl/containers/vector.hpp>

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "chaistl/registry.hpp"

namespace chaistl_benchmark {
namespace {

enum class input_pattern {
  reverse,
  randomish,
  few_unique,
  all_equal,
  sorted,
  organ_pipe,
};

std::vector<int> make_sort_values(std::size_t count, input_pattern pattern) {
  std::vector<int> values(count);
  std::iota(values.begin(), values.end(), 0);

  switch (pattern) {
    case input_pattern::reverse:
      std::ranges::reverse(values);
      break;
    case input_pattern::randomish:
      for (std::size_t index = 0; index < count; ++index) {
        values[index] = static_cast<int>((index * 48271u + 17u) % (count == 0 ? 1 : count));
      }
      break;
    case input_pattern::few_unique:
      for (std::size_t index = 0; index < count; ++index) {
        values[index] = static_cast<int>((index * 48271u + 17u) % 8u);
      }
      break;
    case input_pattern::all_equal:
      std::ranges::fill(values, 7);
      break;
    case input_pattern::sorted:
      break;
    case input_pattern::organ_pipe:
      for (std::size_t index = 0; index < count; ++index) {
        const std::size_t mirror = index < count / 2 ? index : count - index;
        values[index] = static_cast<int>(mirror);
      }
      break;
  }

  return values;
}

template <class Container, class Sorter>
void bench_sorter(benchmark::State& state, input_pattern pattern, Sorter sorter) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto input = make_sort_values(count, pattern);

  for (auto _ : state) {
    state.PauseTiming();
    Container values(input.begin(), input.end());
    state.ResumeTiming();

    sorter(values.begin(), values.end());
    benchmark::DoNotOptimize(&values);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_std_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    std::sort(first, last);
  });
}

template <class Container>
void bench_chaistl_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::sort(first, last);
  });
}

template <class Container>
void bench_pdqsort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::pdqsort(first, last);
  });
}

template <class Container>
void bench_quick_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::quick_sort(first, last);
  });
}

template <class Container>
void bench_qsort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::qsort(first, last);
  });
}

template <class Container>
void bench_intro_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::intro_sort(first, last);
  });
}

template <class Container>
void bench_heap_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::heap_sort(first, last);
  });
}

template <class Container>
void bench_merge_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::merge_sort(first, last);
  });
}

template <class Container>
void bench_bottom_up_merge_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::bottom_up_merge_sort(first, last);
  });
}

template <class Container>
void bench_stable_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::stable_sort(first, last);
  });
}

template <class Container>
void bench_shell_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::shell_sort(first, last);
  });
}

template <class Container>
void bench_insertion_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::insertion_sort(first, last);
  });
}

template <class Container>
void bench_binary_insertion_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::binary_insertion_sort(first, last);
  });
}

template <class Container>
void bench_selection_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::selection_sort(first, last);
  });
}

template <class Container>
void bench_bubble_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::bubble_sort(first, last);
  });
}

template <class Container>
void bench_cocktail_sort(benchmark::State& state, input_pattern pattern) {
  bench_sorter<Container>(state, pattern, [](auto first, auto last) {
    chaistl::cocktail_sort(first, last);
  });
}

void small_sort_args(benchmark::Benchmark* benchmark) {
  benchmark->Arg(32)->Arg(128)->Arg(512)->Unit(benchmark::kNanosecond);
}

void large_sort_args(benchmark::Benchmark* benchmark) {
  benchmark->Arg(1024)->Arg(16384)->Arg(65536)->Unit(benchmark::kNanosecond);
}

template <class Container>
void register_sort_benchmark(std::string_view container,
                             std::string_view name,
                             input_pattern pattern,
                             void (*function)(benchmark::State&, input_pattern),
                             void (*args)(benchmark::Benchmark*)) {
  auto* benchmark = benchmark::RegisterBenchmark(
      (std::string(container) + "/" + std::string(name)).c_str(),
      [function, pattern](benchmark::State& state) {
        function(state, pattern);
      });
  args(benchmark);
}

template <class Container>
void register_container_sorts(std::string_view container) {
  for (const auto [suffix, pattern] : {std::pair{"reverse", input_pattern::reverse},
                                      std::pair{"randomish", input_pattern::randomish},
                                      std::pair{"few_unique", input_pattern::few_unique},
                                      std::pair{"all_equal", input_pattern::all_equal},
                                      std::pair{"sorted", input_pattern::sorted},
                                      std::pair{"organ_pipe", input_pattern::organ_pipe}}) {
    register_sort_benchmark<Container>(container, std::string("std_sort/") + suffix, pattern, &bench_std_sort<Container>, large_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("chaistl_sort/") + suffix, pattern, &bench_chaistl_sort<Container>, large_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("pdqsort/") + suffix, pattern, &bench_pdqsort<Container>, large_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("quick_sort/") + suffix, pattern, &bench_quick_sort<Container>, large_sort_args);
    register_sort_benchmark<Container>(container, std::string("qsort/") + suffix, pattern, &bench_qsort<Container>, large_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("intro_sort/") + suffix, pattern, &bench_intro_sort<Container>, large_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("heap_sort/") + suffix, pattern, &bench_heap_sort<Container>, large_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("merge_sort/") + suffix, pattern, &bench_merge_sort<Container>, large_sort_args);
    register_sort_benchmark<Container>(container,
                                       std::string("bottom_up_merge_sort/") + suffix,
                                       pattern,
                                       &bench_bottom_up_merge_sort<Container>,
                                       large_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("stable_sort/") + suffix, pattern, &bench_stable_sort<Container>, large_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("shell_sort/") + suffix, pattern, &bench_shell_sort<Container>, small_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("insertion_sort/") + suffix, pattern, &bench_insertion_sort<Container>, small_sort_args);
    register_sort_benchmark<Container>(container,
                                       std::string("binary_insertion_sort/") + suffix,
                                       pattern,
                                       &bench_binary_insertion_sort<Container>,
                                       small_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("selection_sort/") + suffix, pattern, &bench_selection_sort<Container>, small_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("bubble_sort/") + suffix, pattern, &bench_bubble_sort<Container>, small_sort_args);
    register_sort_benchmark<Container>(
        container, std::string("cocktail_sort/") + suffix, pattern, &bench_cocktail_sort<Container>, small_sort_args);
  }
}

}  // namespace

void register_sorting_benchmarks() {
  register_container_sorts<std::vector<int>>("std::vector<int>");
  register_container_sorts<chaistl::vector<int>>("chaistl::vector<int>");
  register_container_sorts<chaistl::deque<int>>("chaistl::deque<int>");
}

CHAISTL_BENCHMARK_SMOKE_DOMAIN(algorithm_sorting, "^chaistl::vector<int>/chaistl_sort/reverse/1024$")
CHAISTL_REGISTER_BENCHMARK_FILE(register_sorting_benchmarks)

}  // namespace chaistl_benchmark
