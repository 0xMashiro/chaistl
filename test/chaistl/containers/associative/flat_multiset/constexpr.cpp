// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/flat_multiset.hpp>

namespace {

constexpr bool constexpr_flat_multiset_works() {
  chaistl::flat_multiset<int> s{2, 1, 2};
  auto it = s.insert(2);
  if (*it != 2 || s.count(2) != 3) return false;
  if (s.erase(2) != 3) return false;
  s.insert(chaistl::sorted_equivalent, {3, 3, 4});
  return s.validate() && s.size() == 4 && *s.begin() == 1;
}

static_assert(constexpr_flat_multiset_works());

TEST(FlatMultisetConstexpr, BasicOperationsAreConstexpr) {
  EXPECT_TRUE(constexpr_flat_multiset_works());
}

}  // namespace
