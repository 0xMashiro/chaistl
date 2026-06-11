// SPDX-License-Identifier: Apache-2.0

// References:
// - libc++ does not keep a separate std::array sequence-container benchmark;
//   its sequence-container helper targets runtime-sized containers.
// - array has fixed extent, so these benchmarks vary size through template
//   instantiations instead of runtime benchmark arguments.

#pragma once

#include <benchmark/benchmark.h>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

#include "sequence_container_benchmarks.hpp"

namespace chaistl_benchmark {

template <class Container>
void initialize_array(Container& container) {
  typename Container::size_type index{};
  for (auto& value : container) {
    value = make_value<typename Container::value_type>(index);
    ++index;
  }
}

template <class Container>
void bench_array_default_construct(benchmark::State& state) {
  for (auto _ : state) {
    Container container;
    do_not_optimize_container(container);
  }

  state.SetItemsProcessed(state.iterations() * Container{}.size());
}

template <class Container>
void bench_array_value_initialize(benchmark::State& state) {
  for (auto _ : state) {
    Container container{};
    do_not_optimize_container(container);
  }

  state.SetItemsProcessed(state.iterations() * Container{}.size());
}

template <class Container>
void bench_array_fill(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto value = make_value<value_type>(Container{}.size());

  for (auto _ : state) {
    Container container{};
    container.fill(value);
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * Container{}.size());
}

template <class Container>
void bench_array_copy_construct(benchmark::State& state) {
  Container input{};
  initialize_array(input);

  for (auto _ : state) {
    Container container(input);
    do_not_optimize_container(container);
    benchmark::DoNotOptimize(&input);
  }

  state.SetItemsProcessed(state.iterations() * input.size());
}

template <class Container>
void bench_array_iteration_sum(benchmark::State& state) {
  using value_type = typename Container::value_type;
  Container container{};
  initialize_array(container);

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

  state.SetItemsProcessed(state.iterations() * container.size());
}

template <class Container>
void bench_array_data_access(benchmark::State& state) {
  Container container{};
  initialize_array(container);

  for (auto _ : state) {
    benchmark::DoNotOptimize(container.data());
    benchmark::DoNotOptimize(container.size());
  }

  state.SetItemsProcessed(state.iterations() * container.size());
}

template <class Container>
void bench_array_three_way_compare(benchmark::State& state)
  requires requires(const Container& lhs, const Container& rhs) { lhs <=> rhs; }
{
  Container lhs{};
  Container rhs{};
  initialize_array(lhs);
  initialize_array(rhs);

  for (auto _ : state) {
    benchmark::DoNotOptimize(lhs <=> rhs);
    benchmark::DoNotOptimize(&lhs);
    benchmark::DoNotOptimize(&rhs);
  }

  state.SetItemsProcessed(state.iterations() * lhs.size());
}

inline void apply_array_unit(benchmark::Benchmark* benchmark) {
  benchmark->Unit(benchmark::kNanosecond);
}

template <class Container>
void register_array_container_benchmarks(std::string container_name) {
  auto register_benchmark = [container_name](std::string_view operation, auto* function) {
    auto* benchmark =
        benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), function);
    apply_array_unit(benchmark);
  };

  register_benchmark("default_construct", &bench_array_default_construct<Container>);
  register_benchmark("value_initialize", &bench_array_value_initialize<Container>);
  register_benchmark("fill", &bench_array_fill<Container>);
  register_benchmark("copy_construct", &bench_array_copy_construct<Container>);
  register_benchmark("iteration_sum", &bench_array_iteration_sum<Container>);
  register_benchmark("data_access", &bench_array_data_access<Container>);

  if constexpr (requires(const Container& lhs, const Container& rhs) { lhs <=> rhs; }) {
    register_benchmark("three_way_compare", &bench_array_three_way_compare<Container>);
  }
}

}  // namespace chaistl_benchmark
