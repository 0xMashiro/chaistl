// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_map.hpp>

#include <array>
#include <ranges>
#include <string>
#include <utility>

namespace {

using ::testing::ElementsAre;

TEST(FlatMapCons, InitializerListSortsAndDeduplicates) {
  chaistl::flat_map<int, std::string> m{{5, "five"}, {1, "one"}, {3, "three"}, {1, "duplicate"}};

  EXPECT_EQ(m.size(), 3uz);
  EXPECT_THAT(m.keys(), ElementsAre(1, 3, 5));
  EXPECT_EQ(m.at(1), "one");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapCons, ContainersConstructorSortsAndDeduplicates) {
  chaistl::flat_map<int, std::string>::containers storage;
  storage.keys = {3, 1, 3, 2};
  storage.values = {"three", "one", "duplicate", "two"};

  chaistl::flat_map<int, std::string> m(std::move(storage));

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 3));
  EXPECT_EQ(m.at(3), "three");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapCons, KeyAndMappedContainersConstructorSortsAndDeduplicates) {
  chaistl::vector<int> keys{3, 1, 3, 2};
  chaistl::vector<std::string> values{"three", "one", "duplicate", "two"};

  chaistl::flat_map<int, std::string> m(std::move(keys), std::move(values));

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 3));
  EXPECT_EQ(m.at(3), "three");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapCons, SortedUniqueConstructorAdoptsContainers) {
  chaistl::flat_map<int, std::string>::containers storage;
  storage.keys = {1, 2, 3};
  storage.values = {"one", "two", "three"};

  chaistl::flat_map<int, std::string> m(chaistl::sorted_unique, std::move(storage));

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 3));
  EXPECT_THAT(m.values(), ElementsAre("one", "two", "three"));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapCons, SortedUniqueRangeConstructor) {
  const std::array values{std::pair{1, std::string("one")}, std::pair{2, std::string("two")}};

  chaistl::flat_map<int, std::string> m(chaistl::sorted_unique, values.begin(), values.end());

  EXPECT_THAT(m.keys(), ElementsAre(1, 2));
  EXPECT_EQ(m.at(2), "two");
}

TEST(FlatMapCons, RangeConstructor) {
  const std::array values{std::pair{2, std::string("two")}, std::pair{1, std::string("one")}};

  chaistl::flat_map<int, std::string> m(values.begin(), values.end());

  EXPECT_THAT(m.keys(), ElementsAre(1, 2));
  EXPECT_EQ(m.at(2), "two");
}

TEST(FlatMapCons, FromRangeConstructor) {
  const std::array values{std::pair{2, std::string("two")}, std::pair{1, std::string("one")}};

  chaistl::flat_map<int, std::string> m(std::from_range, values);

  EXPECT_THAT(m.keys(), ElementsAre(1, 2));
}

TEST(FlatMapCons, AssignmentFromInitializerList) {
  chaistl::flat_map<int, std::string> m{{1, "one"}};

  m = {{2, "two"}, {1, "one"}};

  EXPECT_THAT(m.keys(), ElementsAre(1, 2));
  EXPECT_EQ(m.at(2), "two");
}

TEST(FlatMapCons, SortedUniqueInitializerListConstructor) {
  chaistl::flat_map<int, std::string> m(chaistl::sorted_unique, {{1, "one"}, {2, "two"}});

  EXPECT_THAT(m.keys(), ElementsAre(1, 2));
  EXPECT_EQ(m.at(1), "one");
}

}  // namespace
