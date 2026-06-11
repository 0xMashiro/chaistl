// SPDX-License-Identifier: Apache-2.0

// References:
// - vector element requirements depend on operations, and chaistl's base
//   vector template requires destructible<T> so storage cleanup is meaningful:
//   https://en.cppreference.com/w/cpp/container/vector
// - This is a chaistl compile-fail probe, not copied from upstream tests.

#include <chaistl/containers/vector.hpp>

struct not_destructible {
  ~not_destructible() = delete;
};

int main() {
  chaistl::vector<not_destructible> values;
  return static_cast<int>(values.size());
}
