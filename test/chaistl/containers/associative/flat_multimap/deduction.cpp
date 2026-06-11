// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/flat_multimap.hpp>

#include <functional>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

TEST(FlatMultimapDeduction, FromContainers) {
  chaistl::vector<int> keys{3, 1, 2, 2};
  chaistl::vector<std::string> values{"three", "one", "two-a", "two-b"};

  chaistl::flat_multimap m(std::move(keys), std::move(values));

  static_assert(std::is_same_v<decltype(m), chaistl::flat_multimap<int, std::string>>);
  EXPECT_EQ(m.size(), 4uz);
  EXPECT_EQ(m.count(2), 2uz);
  auto [first, last] = m.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 2);
  EXPECT_EQ(first->second, "two-a");
}

TEST(FlatMultimapDeduction, FromSortedEquivalentContainersWithComparator) {
  chaistl::vector<int> keys{3, 2, 2, 1};
  chaistl::vector<std::string> values{"three", "two-a", "two-b", "one"};

  chaistl::flat_multimap m(chaistl::sorted_equivalent, std::move(keys), std::move(values), std::greater<int>{});

  static_assert(
      std::is_same_v<
          decltype(m),
          chaistl::
              flat_multimap<int, std::string, std::greater<int>, chaistl::vector<int>, chaistl::vector<std::string>>>);
  EXPECT_EQ(m.begin()->first, 3);
  EXPECT_EQ(m.size(), 4uz);
  EXPECT_EQ(m.count(2), 2uz);
}

TEST(FlatMultimapDeduction, FromIteratorPair) {
  std::vector<std::pair<int, std::string>> source{{2, "two-a"}, {1, "one"}, {2, "two-b"}};

  chaistl::flat_multimap m(source.begin(), source.end());

  static_assert(std::is_same_v<decltype(m), chaistl::flat_multimap<int, std::string>>);
  EXPECT_EQ(m.size(), 3uz);
  EXPECT_EQ(m.count(2), 2uz);
}

TEST(FlatMultimapDeduction, FromRange) {
  std::vector<std::pair<int, int>> source{{1, 10}, {2, 20}, {2, 22}};

  chaistl::flat_multimap m(std::from_range, source);

  static_assert(std::is_same_v<decltype(m), chaistl::flat_multimap<int, int>>);
  EXPECT_EQ(m.count(2), 2uz);
  auto [first, last] = m.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 2);
}

TEST(FlatMultimapDeduction, FromInitializerList) {
  chaistl::flat_multimap m{std::pair{1, 1.5}, std::pair{2, 2.5}, std::pair{2, 2.75}};

  static_assert(std::is_same_v<decltype(m), chaistl::flat_multimap<int, double>>);
  EXPECT_EQ(m.size(), 3uz);
  EXPECT_EQ(m.count(2), 2uz);
}

}  // namespace
