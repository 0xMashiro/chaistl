// SPDX-License-Identifier: Apache-2.0

// References:
// - [vector.overview] requires Allocator::value_type to be the same as T:
//   https://eel.is/c++draft/vector.overview
// - This is a chaistl compile-fail probe, not copied from upstream tests.

#include <chaistl/containers/vector.hpp>

#include <cstddef>

struct allocator_for_long {
  using value_type = long;

  [[nodiscard]] long* allocate(std::size_t);

  void deallocate(long*, std::size_t) noexcept;
};

int main() {
  chaistl::vector<int, allocator_for_long> values;
  return static_cast<int>(values.size());
}
