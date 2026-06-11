// SPDX-License-Identifier: Apache-2.0

// References:
// - libc++ sequence-container benchmarks live under
//   libcxx/test/benchmarks/containers/sequence, but do not include a dedicated
//   std::array benchmark because array size is part of the type.
// - This file registers chaistl-specific fixed-extent comparisons against
//   std::array; it is not copied from libc++ or libstdc++ benchmark sources.

#include <chaistl/containers/array.hpp>

#include <array>
#include <cstddef>
#include <string>

#include "array_container_benchmarks.hpp"

namespace chaistl_benchmark {

template <class T, std::size_t N>
void register_array_size_benchmarks(std::string_view value_type_name) {
  register_array_container_benchmarks<std::array<T, N>>("std::array<" + std::string(value_type_name) + "," +
                                                        std::to_string(N) + ">");
  register_array_container_benchmarks<chaistl::array<T, N>>("chaistl::array<" + std::string(value_type_name) + "," +
                                                            std::to_string(N) + ">");
}

template <class T>
void register_array_type_benchmarks(std::string_view value_type_name) {
  register_array_size_benchmarks<T, 32>(value_type_name);
  register_array_size_benchmarks<T, 1024>(value_type_name);
  register_array_size_benchmarks<T, 8192>(value_type_name);
}

void register_array_benchmarks() {
  register_array_type_benchmarks<int>("int");
  register_array_type_benchmarks<std::string>("std::string");
  register_array_type_benchmarks<tracked_value>("tracked_value");
  register_array_type_benchmarks<movable_type>("movable_type");
  register_array_type_benchmarks<large_pod>("large_pod");
}

}  // namespace chaistl_benchmark
