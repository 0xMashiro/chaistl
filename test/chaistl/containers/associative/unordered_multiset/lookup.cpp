// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multiset.hpp>

namespace {

TEST(UnorderedMultisetLookup, EqualRangeMissIsEmptyEndRange) {
  chaistl::unordered_multiset<int> set{1, 1, 2};

  auto [first, last] = set.equal_range(9);

  EXPECT_EQ(first, set.end());
  EXPECT_EQ(last, set.end());
  EXPECT_EQ(set.count(9), 0u);
}

TEST(UnorderedMultisetLookup, EqualityIgnoresGroupOrderButChecksMultiplicity) {
  chaistl::unordered_multiset<int> a{1, 2, 1, 3};
  chaistl::unordered_multiset<int> b{3, 1, 2, 1};
  chaistl::unordered_multiset<int> c{1, 2, 3};

  EXPECT_EQ(a, b);
  EXPECT_FALSE(a == c);
}

}  // namespace
