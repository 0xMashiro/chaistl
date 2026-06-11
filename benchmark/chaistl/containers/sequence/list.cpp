// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list benchmarks should focus on operations where list excels:
//   O(1) insert/erase at any position, splice, merge, and member sort.
// - This file registers chaistl-specific comparisons against std::list.

#include <chaistl/containers/list.hpp>

#include <list>
#include <string>

#include "sequence_container_benchmarks.hpp"

namespace chaistl_benchmark {

// ---------------------------------------------------------------------------
// List-specific benchmarks
// ---------------------------------------------------------------------------

// push_front: list's unique advantage over vector.
template <class Container>
void bench_list_push_front_growth(benchmark::State& state) {
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

// insert at front: tests list's O(1) insert at arbitrary position.
template <class Container>
void bench_list_insert_front(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container;
    for (std::size_t index = 0; index < count; ++index) {
      container.insert(container.begin(), make_value<value_type>(index));
    }
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// erase from front: tests list's O(1) erase at arbitrary position.
template <class Container>
void bench_list_erase_front(benchmark::State& state) {
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

// splice all: tests list's O(1) splice (whole list).
template <class Container>
void bench_list_splice_all(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input1 = make_input<value_type>(count / 2);
  auto input2 = make_input<value_type>(count - count / 2);

  for (auto _ : state) {
    state.PauseTiming();
    Container container1(input1.begin(), input1.end());
    Container container2(input2.begin(), input2.end());
    state.ResumeTiming();

    container1.splice(container1.end(), container2);
    do_not_optimize_container(container1);
    benchmark::DoNotOptimize(&container2);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// merge: tests list's merge of two sorted lists.
template <class Container>
void bench_list_merge(benchmark::State& state) {
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
    do_not_optimize_container(container1);
    benchmark::DoNotOptimize(&container2);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// reverse: tests list's O(n) reverse.
template <class Container>
void bench_list_reverse(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    container.reverse();
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// sort: tests list's member sort (merge sort).
template <class Container>
void bench_list_sort(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    container.sort();
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// remove: tests list's remove (linear scan + erase).
template <class Container>
void bench_list_remove(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    container.remove(make_value<value_type>(count / 2));
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// swap: tests list's O(1) swap (pointer exchange).
template <class Container>
void bench_list_swap(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input1 = make_input<value_type>(count);
  auto input2 = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container1(input1.begin(), input1.end());
    Container container2(input2.begin(), input2.end());
    state.ResumeTiming();

    container1.swap(container2);
    do_not_optimize_container(container1);
    do_not_optimize_container(container2);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// unique: tests list's unique (remove consecutive duplicates).
template <class Container>
void bench_list_unique(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_input<value_type>(count);
  // Create consecutive duplicates so unique has actual work to do.
  for (std::size_t i = 0; i + 1 < input.size(); i += 2) {
    input[i + 1] = input[i];
  }

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input.begin(), input.end());
    state.ResumeTiming();

    container.unique();
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// assign_reuse: tests whether assign reuses existing nodes.
template <class Container>
void bench_list_assign_reuse(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input1 = make_input<value_type>(count);
  auto input2 = make_input<value_type>(count);

  for (auto _ : state) {
    state.PauseTiming();
    Container container(input1.begin(), input1.end());
    state.ResumeTiming();

    container.assign(input2.begin(), input2.end());
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// splice_single: tests repeated single-element splice (realistic usage pattern).
template <class Container>
void bench_list_splice_single(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));
  auto input1 = make_input<value_type>(count / 2);
  auto input2 = make_input<value_type>(count - count / 2);

  for (auto _ : state) {
    state.PauseTiming();
    Container container1(input1.begin(), input1.end());
    Container container2(input2.begin(), input2.end());
    state.ResumeTiming();

    auto it = container1.begin();
    while (!container2.empty()) {
      container1.splice(it, container2, container2.begin());
    }
    do_not_optimize_container(container1);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// emplace_back: tests in-place construction efficiency.
template <class Container>
void bench_list_emplace_back(benchmark::State& state) {
  using value_type = typename Container::value_type;
  const auto count = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    Container container;
    for (std::size_t index = 0; index < count; ++index) {
      container.emplace_back(make_value<value_type>(index));
    }
    do_not_optimize_container(container);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * count);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

template <class Container>
void register_list_container_benchmarks(std::string_view container_name) {
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
  register_benchmark("push_front_growth", &bench_list_push_front_growth<Container>, apply_args_growth);
  register_benchmark("insert_front", &bench_list_insert_front<Container>, apply_args_growth);
  register_benchmark("erase_front", &bench_list_erase_front<Container>, apply_args_modify);
  register_benchmark("iteration_sum", &bench_iteration_sum<Container>, apply_args_access);
  register_benchmark("splice_all", &bench_list_splice_all<Container>, apply_args_modify);
  register_benchmark("splice_single", &bench_list_splice_single<Container>, apply_args_modify);
  register_benchmark("merge", &bench_list_merge<Container>, apply_args_modify);
  register_benchmark("reverse", &bench_list_reverse<Container>, apply_args_modify);
  register_benchmark("sort", &bench_list_sort<Container>, apply_args_modify);
  register_benchmark("remove", &bench_list_remove<Container>, apply_args_modify);
  register_benchmark("unique", &bench_list_unique<Container>, apply_args_modify);
  register_benchmark("swap", &bench_list_swap<Container>, apply_args_modify);
  register_benchmark("assign_reuse", &bench_list_assign_reuse<Container>, apply_args_construct);
  register_benchmark("emplace_back", &bench_list_emplace_back<Container>, apply_args_growth);
}

void register_list_benchmarks() {
  register_list_container_benchmarks<std::list<int>>("std::list<int>");
  register_list_container_benchmarks<chaistl::list<int>>("chaistl::list<int>");

  register_list_container_benchmarks<std::list<std::string>>("std::list<std::string>");
  register_list_container_benchmarks<chaistl::list<std::string>>("chaistl::list<std::string>");

  register_list_container_benchmarks<std::list<tracked_value>>("std::list<tracked_value>");
  register_list_container_benchmarks<chaistl::list<tracked_value>>("chaistl::list<tracked_value>");

  register_list_container_benchmarks<std::list<movable_type>>("std::list<movable_type>");
  register_list_container_benchmarks<chaistl::list<movable_type>>("chaistl::list<movable_type>");

  register_list_container_benchmarks<std::list<large_pod>>("std::list<large_pod>");
  register_list_container_benchmarks<chaistl::list<large_pod>>("chaistl::list<large_pod>");
}

}  // namespace chaistl_benchmark
