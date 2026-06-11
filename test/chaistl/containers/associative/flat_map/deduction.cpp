// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/flat_map.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {

TEST(FlatMapDeduction, FromContainers) {
  chaistl::vector<int> keys{3, 1, 2};
  chaistl::vector<std::string> values{"three", "one", "two"};

  chaistl::flat_map m(std::move(keys), std::move(values));

  static_assert(std::is_same_v<decltype(m), chaistl::flat_map<int, std::string>>);
  EXPECT_EQ(m.at(2), "two");
}

TEST(FlatMapDeduction, FromSortedUniqueContainersWithComparator) {
  chaistl::vector<int> keys{3, 2, 1};
  chaistl::vector<std::string> values{"three", "two", "one"};

  chaistl::flat_map m(chaistl::sorted_unique, std::move(keys), std::move(values), std::greater<int>{});

  static_assert(
      std::is_same_v<
          decltype(m),
          chaistl::flat_map<int, std::string, std::greater<int>, chaistl::vector<int>, chaistl::vector<std::string>>>);
  EXPECT_EQ(m.begin()->first, 3);
}

TEST(FlatMapDeduction, FromIteratorPair) {
  std::vector<std::pair<int, std::string>> source{{2, "two"}, {1, "one"}};

  chaistl::flat_map m(source.begin(), source.end());

  static_assert(std::is_same_v<decltype(m), chaistl::flat_map<int, std::string>>);
  EXPECT_EQ(m.size(), 2uz);
}

TEST(FlatMapDeduction, FromRange) {
  std::vector<std::pair<int, int>> source{{1, 10}, {2, 20}};

  chaistl::flat_map m(std::from_range, source);

  static_assert(std::is_same_v<decltype(m), chaistl::flat_map<int, int>>);
  EXPECT_EQ(m.at(2), 20);
}

TEST(FlatMapDeduction, FromInitializerList) {
  chaistl::flat_map m{std::pair{1, 1.5}, std::pair{2, 2.5}};

  static_assert(std::is_same_v<decltype(m), chaistl::flat_map<int, double>>);
  EXPECT_EQ(m.at(1), 1.5);
}

}  // namespace
