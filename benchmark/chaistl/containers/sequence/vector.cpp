// SPDX-License-Identifier: Apache-2.0

// References:
// - libc++ keeps std::vector benchmarks under
//   libcxx/test/benchmarks/containers/sequence/vector.bench.cpp.
// - This file registers chaistl-specific comparisons against std::vector; it is
//   not copied from libc++ or libstdc++ benchmark sources.

#include <chaistl/containers/vector.hpp>

#include <string>
#include <vector>

#include "sequence_container_benchmarks.hpp"

namespace chaistl_benchmark {

namespace {

template <class Container>
void register_vector_specific_benchmarks(std::string_view container_name) {
  auto register_benchmark = [container_name](
                                std::string_view operation, auto* function, void (*apply_args)(benchmark::Benchmark*)) {
    auto* benchmark =
        benchmark::RegisterBenchmark((std::string(container_name) + "/" + std::string(operation)).c_str(), function);
    apply_args(benchmark);
  };

  register_benchmark("random_access_sum", &bench_random_access_sum<Container>, apply_args_access);
  register_benchmark("find", &bench_find<Container>, apply_args_access);
  register_benchmark("sort", &bench_sort<Container>, apply_args_modify);
  register_benchmark("move_reallocate", &bench_move_reallocate<Container>, apply_args_growth);
  register_benchmark("move_erase", &bench_move_erase<Container>, apply_args_modify);
}

}  // namespace

void register_vector_benchmarks() {
  namespace bench = chaistl_benchmark;

  // int
  bench::register_sequence_container_benchmarks<std::vector<int>>("std::vector<int>");
  bench::register_sequence_container_benchmarks<chaistl::vector<int>>("chaistl::vector<int>");
  register_vector_specific_benchmarks<std::vector<int>>("std::vector<int>");
  register_vector_specific_benchmarks<chaistl::vector<int>>("chaistl::vector<int>");

  // std::string
  bench::register_sequence_container_benchmarks<std::vector<std::string>>("std::vector<std::string>");
  bench::register_sequence_container_benchmarks<chaistl::vector<std::string>>("chaistl::vector<std::string>");
  register_vector_specific_benchmarks<std::vector<std::string>>("std::vector<std::string>");
  register_vector_specific_benchmarks<chaistl::vector<std::string>>("chaistl::vector<std::string>");

  // tracked_value
  bench::register_sequence_container_benchmarks<std::vector<bench::tracked_value>>("std::vector<tracked_value>");
  bench::register_sequence_container_benchmarks<chaistl::vector<bench::tracked_value>>(
      "chaistl::vector<tracked_value>");
  register_vector_specific_benchmarks<std::vector<bench::tracked_value>>("std::vector<tracked_value>");
  register_vector_specific_benchmarks<chaistl::vector<bench::tracked_value>>("chaistl::vector<tracked_value>");

  // movable_type
  bench::register_sequence_container_benchmarks<std::vector<bench::movable_type>>("std::vector<movable_type>");
  bench::register_sequence_container_benchmarks<chaistl::vector<bench::movable_type>>("chaistl::vector<movable_type>");
  register_vector_specific_benchmarks<std::vector<bench::movable_type>>("std::vector<movable_type>");
  register_vector_specific_benchmarks<chaistl::vector<bench::movable_type>>("chaistl::vector<movable_type>");

  // large_pod
  bench::register_sequence_container_benchmarks<std::vector<bench::large_pod>>("std::vector<large_pod>");
  bench::register_sequence_container_benchmarks<chaistl::vector<bench::large_pod>>("chaistl::vector<large_pod>");
  register_vector_specific_benchmarks<std::vector<bench::large_pod>>("std::vector<large_pod>");
  register_vector_specific_benchmarks<chaistl::vector<bench::large_pod>>("chaistl::vector<large_pod>");
}

}  // namespace chaistl_benchmark
