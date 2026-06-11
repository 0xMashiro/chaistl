// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_multimap.hpp>

#include <string>

namespace {

using ::testing::ElementsAre;

TEST(FlatMultimapLookup, CountAndEqualRangeCoverAllEquivalentKeys) {
  chaistl::flat_multimap<int, std::string> m{{3, "a"}, {1, "one"}, {3, "b"}, {2, "two"}, {3, "c"}};

  EXPECT_EQ(m.count(3), 3uz);
  auto [first, last] = m.equal_range(3);

  chaistl::vector<std::string> values;
  for (; first != last; ++first) {
    values.insert(values.end(), first->second);
  }
  EXPECT_THAT(values, ElementsAre("a", "b", "c"));
}

TEST(FlatMultimapLookup, BoundsSpanDuplicates) {
  chaistl::flat_multimap<int, std::string> m{{1, "one"}, {2, "a"}, {2, "b"}, {2, "c"}, {4, "four"}};

  EXPECT_EQ(m.lower_bound(2) - m.begin(), 1);
  EXPECT_EQ(m.upper_bound(2) - m.begin(), 4);
  EXPECT_EQ(m.find(2) - m.begin(), 1);
  EXPECT_TRUE(m.contains(2));
  EXPECT_FALSE(m.contains(3));
}

TEST(FlatMultimapLookup, TransparentOverloads) {
  chaistl::flat_multimap<std::string, int, std::less<>> m{{"alpha", 1}, {"beta", 2}, {"beta", 3}};

  EXPECT_EQ(m.count("beta"), 2uz);
  auto [first, last] = m.equal_range("beta");
  EXPECT_EQ(last - first, 2);
  EXPECT_NE(m.find("alpha"), m.end());
}

}  // namespace
