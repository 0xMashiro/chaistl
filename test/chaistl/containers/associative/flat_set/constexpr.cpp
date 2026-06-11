// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/flat_set.hpp>

namespace {

constexpr bool constexpr_smoke() {
  chaistl::flat_set<int> s{3, 1, 2, 3};

  s.insert(5);
  s.emplace(4);
  const int incoming[] = {7, 6, 4};
  s.insert(std::begin(incoming), std::end(incoming));
  s.erase(6);

  bool ok = s.validate() && s.size() == 6 && s.contains(4) && !s.contains(6);
  ok = ok && *s.begin() == 1 && *s.lower_bound(5) == 5;
  return ok;
}

static_assert(constexpr_smoke(), "flat_set core operations must work in constant evaluation");

TEST(FlatSetConstexpr, CoreOperationsAreConstexpr) {
  EXPECT_TRUE(constexpr_smoke());  // same body at run time
}

}  // namespace
