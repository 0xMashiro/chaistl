// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_multimap.hpp>

#include <array>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using ::testing::ElementsAre;

TEST(FlatMultimapCons, InitializerListSortsAndKeepsDuplicates) {
  chaistl::flat_multimap<int, std::string> m{{5, "five"}, {1, "one"}, {3, "three"}, {1, "duplicate"}};

  EXPECT_THAT(m.keys(), ElementsAre(1, 1, 3, 5));
  EXPECT_THAT(m.values(), ElementsAre("one", "duplicate", "three", "five"));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMultimapCons, ContainersConstructorSortsAndKeepsDuplicatesStably) {
  chaistl::flat_multimap<int, std::string>::containers storage;
  storage.keys = {3, 1, 3, 2};
  storage.values = {"three", "one", "duplicate", "two"};

  chaistl::flat_multimap<int, std::string> m(std::move(storage));

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 3, 3));
  EXPECT_THAT(m.values(), ElementsAre("one", "two", "three", "duplicate"));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMultimapCons, SortedEquivalentConstructorAdoptsContainers) {
  chaistl::flat_multimap<int, std::string>::containers storage;
  storage.keys = {1, 2, 2, 3};
  storage.values = {"one", "two-a", "two-b", "three"};

  chaistl::flat_multimap<int, std::string> m(chaistl::sorted_equivalent, std::move(storage));

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 2, 3));
  EXPECT_THAT(m.values(), ElementsAre("one", "two-a", "two-b", "three"));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMultimapCons, RangeAndFromRangeConstructors) {
  const std::array values{std::pair{2, std::string("two")}, std::pair{1, std::string("one")}};

  chaistl::flat_multimap<int, std::string> by_iter(values.begin(), values.end());
  chaistl::flat_multimap<int, std::string> by_range(std::from_range, values);

  EXPECT_THAT(by_iter.keys(), ElementsAre(1, 2));
  EXPECT_THAT(by_range.keys(), ElementsAre(1, 2));
}

TEST(FlatMultimapCons, AssignmentFromInitializerList) {
  chaistl::flat_multimap<int, std::string> m{{1, "one"}};

  m = {{2, "two-a"}, {1, "one"}, {2, "two-b"}};

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 2));
  EXPECT_THAT(m.values(), ElementsAre("one", "two-a", "two-b"));
}

TEST(FlatMultimapCons, DeductionGuides) {
  chaistl::vector<int> keys{1, 1};
  chaistl::vector<std::string> values{"a", "b"};
  chaistl::flat_multimap from_containers(keys, values);
  static_assert(
      std::is_same_v<
          decltype(from_containers),
          chaistl::
              flat_multimap<int, std::string, std::less<int>, chaistl::vector<int>, chaistl::vector<std::string>>>);

  const std::array pairs{std::pair{1, std::string("one")}, std::pair{1, std::string("again")}};
  chaistl::flat_multimap from_iters(pairs.begin(), pairs.end());
  static_assert(std::is_same_v<decltype(from_iters), chaistl::flat_multimap<int, std::string>>);

  chaistl::flat_multimap from_init{std::pair{1, std::string("one")}, std::pair{1, std::string("again")}};
  static_assert(std::is_same_v<decltype(from_init), chaistl::flat_multimap<int, std::string>>);
}

}  // namespace
