// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list benchmarks should focus on before_begin/insert_after,
//   splice_after, and single-pass traversal rather than size() or random access.
// - This file registers chaistl-specific comparisons against std::forward_list.

#include <chaistl/containers/forward_list.hpp>

#include <forward_list>
#include <string>

#include "chaistl/registry.hpp"
#include "sequence_container_benchmarks.hpp"

namespace chaistl_benchmark {

namespace {

template <class Container>
void do_not_optimize_forward_list(Container& container) {
  benchmark::DoNotOptimize(&container);
  if (!container.empty()) {
    benchmark::DoNotOptimize(&container.front());
  }
}

template <class Container>
void bench_forward_list_default_construct(benchmark::State& state) {
  for (auto _ : state) {
    Container container;
    do_not_optimize_forward_list(container);
  }
}

template <class Container>
void bench_forward_list_iterator_construct(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    Container container(input.begin(), input.end());
    do_not_optimize_forward_list(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_forward_list_push_front_growth(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container;
    for (std::size_t index = 0; index < count; ++index) {
      container.push_front(make_value<value_type>(index));
    }
    do_not_optimize_forward_list(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_forward_list_insert_after_front(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container;
    auto position = container.before_begin();
    for (std::size_t index = 0; index < count; ++index) {
      position = container.insert_after(position, make_value<value_type>(index));
    }
    do_not_optimize_forward_list(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_forward_list_erase_after_front(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    while (!container.empty()) {
      container.erase_after(container.before_begin());
    }
    do_not_optimize_forward_list(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_forward_list_iteration_sum(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const Container container = [](std::size_t count) {
    auto values = make_input<value_type>(count);
    return Container(values.begin(), values.end());
  }(static_cast<std::size_t>(state.range(0)));

  for (auto _ : state) {
    typename Container::size_type total{};
    for (const auto& value : container) {
      if constexpr (std::same_as<value_type, std::string>) {
        total += value.size();
      } else if constexpr (std::same_as<value_type, tracked_value>) {
        total += static_cast<typename Container::size_type>(value.payload.size());
      } else if constexpr (std::same_as<value_type, movable_type>) {
        total += static_cast<typename Container::size_type>(reinterpret_cast<std::uintptr_t>(value.data.get()));
      } else if constexpr (std::same_as<value_type, large_pod>) {
        total += static_cast<typename Container::size_type>(static_cast<int>(value.buffer[0]));
      } else {
        total += static_cast<typename Container::size_type>(value);
      }
    }
    benchmark::DoNotOptimize(total);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <class Container>
void bench_forward_list_splice_after_all(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input1 = make_input<value_type>(count / 2);
  auto input2 = make_input<value_type>(count - count / 2);

  for (auto _ : state) {
    state.PauseTiming();
    Container container1(input1.begin(), input1.end());
    Container container2(input2.begin(), input2.end());
    state.ResumeTiming();

    container1.splice_after(container1.before_begin(), container2);
    do_not_optimize_forward_list(container1);
    benchmark::DoNotOptimize(&container2);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_forward_list_reverse(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    container.reverse();
    do_not_optimize_forward_list(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_forward_list_sort(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    container.sort();
    do_not_optimize_forward_list(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_forward_list_merge(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    state.PauseTiming();
    auto input1 = make_input<value_type>(count / 2);
    auto input2 = make_input<value_type>(count - count / 2);
    Container container1(input1.begin(), input1.end());
    Container container2(input2.begin(), input2.end());
    container1.sort();
    container2.sort();
    state.ResumeTiming();

    container1.merge(container2);
    do_not_optimize_forward_list(container1);
    benchmark::DoNotOptimize(&container2);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_forward_list_remove(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    container.remove(make_value<value_type>(count / 2));
    do_not_optimize_forward_list(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_forward_list_unique(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);
  for (std::size_t index = 0; index + 1 < input.size(); index += 2) {
    input[index + 1] = input[index];
  }

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    container.unique();
    do_not_optimize_forward_list(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void register_forward_list_container_benchmarks(std::string_view container_name) {
  auto register_benchmark = [container_name](
                                std::string_view operation, auto* function, void (*apply_args)(benchmark::Benchmark*)) {
    auto* benchmark =
        benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), function);
    apply_args(benchmark);
  };

  auto* default_construct = benchmark::RegisterBenchmark((std::string(container_name) + "/default_construct").c_str(),
                                                         &bench_forward_list_default_construct<Container>);
  apply_unit(default_construct);

  register_benchmark("iterator_construct", &bench_forward_list_iterator_construct<Container>, apply_args_construct);
  register_benchmark("push_front_growth", &bench_forward_list_push_front_growth<Container>, apply_args_growth);
  register_benchmark("insert_after_front", &bench_forward_list_insert_after_front<Container>, apply_args_growth);
  register_benchmark("erase_after_front", &bench_forward_list_erase_after_front<Container>, apply_args_modify);
  register_benchmark("iteration_sum", &bench_forward_list_iteration_sum<Container>, apply_args_access);
  register_benchmark("splice_after_all", &bench_forward_list_splice_after_all<Container>, apply_args_modify);
  register_benchmark("merge", &bench_forward_list_merge<Container>, apply_args_modify);
  register_benchmark("reverse", &bench_forward_list_reverse<Container>, apply_args_modify);
  register_benchmark("sort", &bench_forward_list_sort<Container>, apply_args_modify);
  register_benchmark("remove", &bench_forward_list_remove<Container>, apply_args_modify);
  register_benchmark("unique", &bench_forward_list_unique<Container>, apply_args_modify);
}

}  // namespace

void register_forward_list_benchmarks() {
  register_forward_list_container_benchmarks<std::forward_list<int>>("std::forward_list<int>");
  register_forward_list_container_benchmarks<chaistl::forward_list<int>>("chaistl::forward_list<int>");

  register_forward_list_container_benchmarks<std::forward_list<std::string>>("std::forward_list<std::string>");
  register_forward_list_container_benchmarks<chaistl::forward_list<std::string>>("chaistl::forward_list<std::string>");

  register_forward_list_container_benchmarks<std::forward_list<tracked_value>>("std::forward_list<tracked_value>");
  register_forward_list_container_benchmarks<chaistl::forward_list<tracked_value>>(
      "chaistl::forward_list<tracked_value>");

  register_forward_list_container_benchmarks<std::forward_list<movable_type>>("std::forward_list<movable_type>");
  register_forward_list_container_benchmarks<chaistl::forward_list<movable_type>>(
      "chaistl::forward_list<movable_type>");

  register_forward_list_container_benchmarks<std::forward_list<large_pod>>("std::forward_list<large_pod>");
  register_forward_list_container_benchmarks<chaistl::forward_list<large_pod>>("chaistl::forward_list<large_pod>");
}

CHAISTL_REGISTER_BENCHMARK_FILE(register_forward_list_benchmarks)

}  // namespace chaistl_benchmark
