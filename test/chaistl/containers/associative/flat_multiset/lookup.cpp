// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_multiset.hpp>

#include <string>

namespace {

using ::testing::ElementsAre;

TEST(FlatMultisetLookup, CountAndEqualRangeCoverAllEquivalentKeys) {
  chaistl::flat_multiset<int> s{3, 1, 2, 3, 3, 4};

  EXPECT_EQ(s.count(3), 3uz);
  auto [first, last] = s.equal_range(3);
  EXPECT_THAT(chaistl::vector<int>(first, last), ElementsAre(3, 3, 3));
}

TEST(FlatMultisetLookup, BoundsSpanDuplicates) {
  chaistl::flat_multiset<int> s{1, 2, 2, 2, 4};

  EXPECT_EQ(s.lower_bound(2) - s.begin(), 1);
  EXPECT_EQ(s.upper_bound(2) - s.begin(), 4);
  EXPECT_EQ(s.find(2) - s.begin(), 1);
  EXPECT_TRUE(s.contains(2));
  EXPECT_FALSE(s.contains(3));
}

TEST(FlatMultisetLookup, TransparentOverloads) {
  chaistl::flat_multiset<std::string, std::less<>> s{"alpha", "beta", "beta"};

  EXPECT_EQ(s.count("beta"), 2uz);
  auto [first, last] = s.equal_range("beta");
  EXPECT_EQ(last - first, 2);
  EXPECT_NE(s.find("alpha"), s.end());
}

}  // namespace
