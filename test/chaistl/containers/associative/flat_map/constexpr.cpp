// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/flat_map.hpp>

#include <utility>

namespace {

constexpr bool constexpr_smoke() {
  chaistl::flat_map<int, int> m{{3, 30}, {1, 10}, {3, 33}};

  m.try_emplace(2, 20);
  m.insert_or_assign(1, 11);
  const std::pair<int, int> incoming[] = {{5, 50}, {4, 40}, {1, 99}};
  m.insert(std::begin(incoming), std::end(incoming));
  m.erase(4);

  bool ok = m.validate() && m.size() == 4;
  ok = ok && m.at(1) == 11 && m.at(3) == 30 && m.contains(5) && !m.contains(4);
  ok = ok && m.begin()->first == 1;
  return ok;
}

static_assert(constexpr_smoke(), "flat_map core operations must work in constant evaluation");

TEST(FlatMapConstexpr, CoreOperationsAreConstexpr) {
  EXPECT_TRUE(constexpr_smoke());  // same body at run time
}

}  // namespace
