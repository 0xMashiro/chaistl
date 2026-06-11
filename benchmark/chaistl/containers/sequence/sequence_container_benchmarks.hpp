// SPDX-License-Identifier: Apache-2.0

// References:
// - libc++ organizes reusable sequence-container benchmarks under
//   libcxx/test/benchmarks/containers/sequence.
// - This file follows that benchmark-suite shape for chaistl, but the benchmark
//   bodies are written for this project and are not copied from libc++.

#pragma once

#include <algorithm>
#include <array>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace chaistl_benchmark {

struct tracked_value {
  int value{};
  std::string payload{};

  tracked_value() = default;

  explicit tracked_value(int number) : value(number), payload(32, static_cast<char>('a' + (number % 26))) {}

  friend bool operator==(const tracked_value&, const tracked_value&) = default;
  friend auto operator<=>(const tracked_value&, const tracked_value&) = default;
};

// MovableType: tests move semantics efficiency.
// Move should be significantly faster than copy due to heap allocation.
struct movable_type {
  static constexpr std::size_t k_data_size = 128;

  std::unique_ptr<std::byte[]> data;

  movable_type() : data(std::make_unique<std::byte[]>(k_data_size)) {
    std::fill(data.get(), data.get() + k_data_size, std::byte{});
  }

  movable_type(const movable_type& other) : data(std::make_unique<std::byte[]>(k_data_size)) {
    std::copy(other.data.get(), other.data.get() + k_data_size, data.get());
  }

  movable_type& operator=(const movable_type& other) {
    if (this != &other) {
      if (!data) {
        data = std::make_unique<std::byte[]>(k_data_size);
      }
      std::copy(other.data.get(), other.data.get() + k_data_size, data.get());
    }
    return *this;
  }

  movable_type(movable_type&& other) noexcept : data(std::move(other.data)) {}

  movable_type& operator=(movable_type&& other) noexcept {
    data = std::move(other.data);
    return *this;
  }

  ~movable_type() = default;

  friend bool operator==(const movable_type&, const movable_type&) = default;
  friend auto operator<=>(const movable_type& lhs, const movable_type& rhs) {
    return lhs.data.get() <=> rhs.data.get();
  }
};

// Large POD: tests large object handling efficiency.
// Containers that use memcpy/memmove for relocation should outperform
// those that do element-wise construct/destroy.
struct large_pod {
  static constexpr std::size_t k_data_size = 2048;

  std::array<std::byte, k_data_size> buffer{};

  large_pod() = default;

  explicit large_pod(int seed) { std::fill(buffer.begin(), buffer.end(), static_cast<std::byte>(seed & 0xFF)); }

  friend bool operator==(const large_pod& lhs, const large_pod& rhs) { return lhs.buffer == rhs.buffer; }

  friend bool operator<(const large_pod& lhs, const large_pod& rhs) { return lhs.buffer < rhs.buffer; }
};

template <class T>
[[nodiscard]] auto make_value(std::size_t index) -> T {
  if constexpr (std::same_as<T, std::string>) {
    return std::string(32, static_cast<char>('a' + (index % 26)));
  } else if constexpr (std::same_as<T, tracked_value>) {
    return tracked_value(static_cast<int>(index));
  } else if constexpr (std::same_as<T, movable_type>) {
    return movable_type{};
  } else if constexpr (std::same_as<T, large_pod>) {
    return large_pod(static_cast<int>(index));
  } else {
    return static_cast<T>(index);
  }
}

template <class T>
[[nodiscard]] auto make_input(std::size_t count) -> std::vector<T> {
  std::vector<T> values;
  values.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    values.push_back(make_value<T>(index));
  }
  return values;
}

template <class Container>
void do_not_optimize_container(Container& container) {
  if constexpr (requires { container.data(); }) {
    benchmark::DoNotOptimize(container.data());
  } else {
    benchmark::DoNotOptimize(&container);
  }
  benchmark::DoNotOptimize(container.size());
}

template <class Container>
void bench_default_construct(benchmark::State& state) {
  for (auto _ : state) {
    Container container;
    do_not_optimize_container(container);
  }
}

template <class Container>
void bench_size_construct(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container(count);
    do_not_optimize_container(container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_fill_construct(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  const auto value = make_value<value_type>(count);

  for (auto _ : state) {
    Container container(count, value);
    do_not_optimize_container(container);
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_iterator_construct(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto input = make_input<value_type>(static_cast<std::size_t>(state.range(0)));

  for (auto _ : state) {
    Container container(input.begin(), input.end());
    do_not_optimize_container(container);
    benchmark::DoNotOptimize(input.data());
  }

  state.SetItemsProcessed(state.iterations() * input.size());
}

template <class Container>
void bench_copy_construct(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const Container input = [](std::size_t count) {
    auto values = make_input<value_type>(count);
    return Container(values.begin(), values.end());
  }(static_cast<std::size_t>(state.range(0)));

  for (auto _ : state) {
    Container container(input);
    do_not_optimize_container(container);
    benchmark::DoNotOptimize(&input);
  }

  state.SetItemsProcessed(state.iterations() * input.size());
}

template <class Container>
void bench_push_back_growth(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container;
    for (std::size_t index = 0; index < count; ++index) {
      container.push_back(make_value<value_type>(index));
    }
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_push_back_reserved(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container;
    container.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      container.push_back(make_value<value_type>(index));
    }
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_insert_middle_no_realloc(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container;
    container.reserve(count + 1);
    for (const auto& value : input) {
      container.push_back(value);
    }
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
void bench_insert_middle_realloc(benchmark::State& state) {
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
void bench_erase_middle(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    auto position = container.begin() + static_cast<typename Container::difference_type>(count / 2);
    state.ResumeTiming();

    container.erase(position);
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

template <class Container>
void bench_iteration_sum(benchmark::State& state) {
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

  state.SetItemsProcessed(state.iterations() * container.size());
}

// Random access benchmark: tests operator[] efficiency.
template <class Container>
void bench_random_access_sum(benchmark::State& state) {
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

// Find benchmark: tests iteration + comparison efficiency.
template <class Container>
void bench_find(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const Container container = [](std::size_t count) {
    auto values = make_input<value_type>(count);
    return Container(values.begin(), values.end());
  }(static_cast<std::size_t>(state.range(0)));

  // Search for a value that does not exist, forcing full traversal.
  const auto target = make_value<value_type>(static_cast<std::size_t>(-1));

  for (auto _ : state) {
    auto it = std::find(container.begin(), container.end(), target);
    benchmark::DoNotOptimize(it);
    benchmark::DoNotOptimize(&container);
  }

  state.SetItemsProcessed(state.iterations() * container.size());
}

// Sort benchmark: tests element access + comparison during sort.
template <class Container>
void bench_sort(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    if constexpr (requires(value_type a, value_type b) {
                    { a < b } -> std::convertible_to<bool>;
                  }) {
      std::sort(container.begin(), container.end());
    }
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// Move reallocate benchmark: tests move efficiency during reallocation.
// Only applicable to containers that support reserve/capacity.
template <class Container>
  requires requires(Container c) {
    c.reserve(1);
    { c.capacity() } -> std::convertible_to<typename Container::size_type>;
    c.resize(1);
  }
void bench_move_reallocate(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto target_size = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container;
    while (container.size() < target_size) {
      container.reserve(container.capacity() + 1);
      container.push_back(make_value<value_type>(container.size()));
    }
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * target_size);
}

// Move erase benchmark: tests move efficiency during erase from front.
template <class Container>
void bench_move_erase(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    state.PauseTiming();
    auto input = make_input<value_type>(count);
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    while (!container.empty()) {
      container.erase(container.begin());
    }
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

inline void apply_unit(benchmark::Benchmark* benchmark) {
  benchmark->Unit(benchmark::kNanosecond);
}

// Args for construction operations: moderate sizes.
inline void apply_args_construct(benchmark::Benchmark* benchmark) {
  benchmark->Arg(32)->Arg(1024)->Arg(8192)->Unit(benchmark::kNanosecond);
}

// Args for growth operations: larger sizes to amortize reallocation cost.
inline void apply_args_growth(benchmark::Benchmark* benchmark) {
  benchmark->Arg(128)->Arg(4096)->Arg(65536)->Unit(benchmark::kNanosecond);
}

// Args for modification operations (insert/erase): smaller sizes to avoid timeout.
inline void apply_args_modify(benchmark::Benchmark* benchmark) {
  benchmark->Arg(32)->Arg(512)->Arg(4096)->Unit(benchmark::kNanosecond);
}

// Args for access operations (iteration, random access): larger sizes.
inline void apply_args_access(benchmark::Benchmark* benchmark) {
  benchmark->Arg(128)->Arg(4096)->Arg(65536)->Unit(benchmark::kNanosecond);
}

inline void apply_sequence_args(benchmark::Benchmark* benchmark) {
  benchmark->Arg(32)->Arg(1024)->Arg(8192)->Unit(benchmark::kNanosecond);
}

template <class Container>
void register_sequence_container_benchmarks(std::string_view container_name) {
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
  register_benchmark("push_back_reserved", &bench_push_back_reserved<Container>, apply_args_growth);
  register_benchmark("insert_middle_no_realloc", &bench_insert_middle_no_realloc<Container>, apply_args_modify);
  register_benchmark("insert_middle_realloc", &bench_insert_middle_realloc<Container>, apply_args_modify);
  register_benchmark("erase_middle", &bench_erase_middle<Container>, apply_args_modify);
  register_benchmark("iteration_sum", &bench_iteration_sum<Container>, apply_args_access);
}

}  // namespace chaistl_benchmark
