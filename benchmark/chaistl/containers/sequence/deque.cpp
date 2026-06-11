// SPDX-License-Identifier: Apache-2.0

// References:
// - libc++ keeps deque benchmarks under
//   libcxx/test/benchmarks/containers/sequence/deque.bench.cpp.
// - This file registers chaistl-specific comparisons against std::deque; it is
//   not copied from libc++ or libstdc++ benchmark sources.

#include <chaistl/containers/deque.hpp>

#include <deque>
#include <string>

#include "sequence_container_benchmarks.hpp"

namespace chaistl_benchmark {

template <class Container>
void bench_deque_push_front_growth(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container;
    for (std::size_t index = 0; index < count; ++index) {
      container.push_front(make_value<value_type>(index));
    }
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_deque_mixed_front_back_growth(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container;
    for (std::size_t index = 0; index < count; ++index) {
      if (index % 2 == 0) {
        container.push_back(make_value<value_type>(index));
      } else {
        container.push_front(make_value<value_type>(index));
      }
    }
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_deque_random_access_sum(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const Container container = [](std::size_t count) {
    auto values = make_input<value_type>(count);
    return Container(values.begin(), values.end());
  }(static_cast<std::size_t>(state.range(0)));

  for (auto _ : state) {
    typename Container::size_type total{};
    for (typename Container::size_type index = 0; index < container.size(); ++index) {
      const auto& value = container[index];
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

  state.SetItemsProcessed(state.iterations() * container.size());
}

template <class Container>
void bench_deque_insert_middle(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    auto position = container.begin() + static_cast<typename Container::difference_type>(count / 2);
    auto value = make_value<value_type>(count);
    state.ResumeTiming();

    container.insert(position, value);
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void register_deque_container_benchmarks(std::string_view container_name) {
  auto register_benchmark = [container_name](
                                std::string_view operation, auto* function, void (*apply_args)(benchmark::Benchmark*)) {
    auto* benchmark =
        benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), function);
    apply_args(benchmark);
  };

  auto* default_construct = benchmark::RegisterBenchmark((std::string(container_name) + "/default_construct").c_str(),
                                                         &bench_default_construct<Container>);
  apply_unit(default_construct);

  register_benchmark("size_construct", &bench_size_construct<Container>, apply_args_construct);
  register_benchmark("fill_construct", &bench_fill_construct<Container>, apply_args_construct);
  register_benchmark("iterator_construct", &bench_iterator_construct<Container>, apply_args_construct);
  register_benchmark("copy_construct", &bench_copy_construct<Container>, apply_args_construct);
  register_benchmark("push_back_growth", &bench_push_back_growth<Container>, apply_args_growth);
  register_benchmark("push_front_growth", &bench_deque_push_front_growth<Container>, apply_args_growth);
  register_benchmark("mixed_front_back_growth", &bench_deque_mixed_front_back_growth<Container>, apply_args_growth);
  register_benchmark("insert_middle", &bench_deque_insert_middle<Container>, apply_args_modify);
  register_benchmark("erase_middle", &bench_erase_middle<Container>, apply_args_modify);
  register_benchmark("iteration_sum", &bench_iteration_sum<Container>, apply_args_access);
  register_benchmark("random_access_sum", &bench_deque_random_access_sum<Container>, apply_args_access);
}

void register_deque_benchmarks() {
  register_deque_container_benchmarks<std::deque<int>>("std::deque<int>");
  register_deque_container_benchmarks<chaistl::deque<int>>("chaistl::deque<int>");

  register_deque_container_benchmarks<std::deque<std::string>>("std::deque<std::string>");
  register_deque_container_benchmarks<chaistl::deque<std::string>>("chaistl::deque<std::string>");

  register_deque_container_benchmarks<std::deque<tracked_value>>("std::deque<tracked_value>");
  register_deque_container_benchmarks<chaistl::deque<tracked_value>>("chaistl::deque<tracked_value>");

  register_deque_container_benchmarks<std::deque<movable_type>>("std::deque<movable_type>");
  register_deque_container_benchmarks<chaistl::deque<movable_type>>("chaistl::deque<movable_type>");

  register_deque_container_benchmarks<std::deque<large_pod>>("std::deque<large_pod>");
  register_deque_container_benchmarks<chaistl::deque<large_pod>>("chaistl::deque<large_pod>");
}

}  // namespace chaistl_benchmark
