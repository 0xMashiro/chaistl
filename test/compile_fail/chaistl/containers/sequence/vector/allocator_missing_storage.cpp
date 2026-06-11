// SPDX-License-Identifier: Apache-2.0

// References:
// - Allocator named requirement:
//   https://en.cppreference.com/w/cpp/named_req/Allocator
// - std::vector requires its Allocator template argument to meet Allocator:
//   https://en.cppreference.com/w/cpp/container/vector
// - This is a chaistl compile-fail probe, not copied from upstream tests.

#include <chaistl/containers/vector.hpp>

struct allocator_without_storage {
  using value_type = int;
};

int main() {
  chaistl::vector<int, allocator_without_storage> values;
  return static_cast<int>(values.size());
}
