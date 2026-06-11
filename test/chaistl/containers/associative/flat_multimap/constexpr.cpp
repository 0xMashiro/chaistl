// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/flat_multimap.hpp>

namespace {

constexpr bool constexpr_flat_multimap_works() {
  chaistl::flat_multimap<int, int> m{{2, 10}, {1, 1}, {2, 20}};
  auto it = m.insert({2, 30});
  if (it->second != 30 || m.count(2) != 3) return false;
  auto [first, last] = m.equal_range(2);
  if (last - first != 3) return false;
  if (m.erase(2) != 3) return false;
  m.insert(chaistl::sorted_equivalent, {{3, 30}, {3, 31}, {4, 40}});
  return m.validate() && m.size() == 4 && m.begin()->first == 1;
}

static_assert(constexpr_flat_multimap_works());

TEST(FlatMultimapConstexpr, BasicOperationsAreConstexpr) {
  EXPECT_TRUE(constexpr_flat_multimap_works());
}

}  // namespace
