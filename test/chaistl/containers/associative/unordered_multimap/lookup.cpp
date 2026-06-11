// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multimap.hpp>

namespace {

TEST(UnorderedMultimapLookup, EqualRangeMissIsEmptyEndRange) {
  chaistl::unordered_multimap<int, int> map{{1, 10}, {1, 11}, {2, 20}};

  auto [first, last] = map.equal_range(9);

  EXPECT_EQ(first, map.end());
  EXPECT_EQ(last, map.end());
  EXPECT_EQ(map.count(9), 0u);
}

TEST(UnorderedMultimapLookup, EqualityIgnoresGroupOrderButChecksMappedMultiplicity) {
  chaistl::unordered_multimap<int, int> a{{1, 10}, {2, 20}, {1, 11}};
  chaistl::unordered_multimap<int, int> b{{2, 20}, {1, 11}, {1, 10}};
  chaistl::unordered_multimap<int, int> c{{1, 10}, {2, 20}, {1, 10}};

  EXPECT_EQ(a, b);
  EXPECT_FALSE(a == c);
}

}  // namespace
