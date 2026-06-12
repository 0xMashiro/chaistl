// SPDX-License-Identifier: Apache-2.0

// References:
// - Constructors and deduction guides:
//   https://eel.is/c++draft/unord.map.overview
//   Deduction strips const from pair keys for the class arguments while the
//   allocator default re-adds it (iter-key-type / iter-alloc-type helpers).

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/functional/hash.hpp>

#include <functional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(UnorderedMapCons, DefaultAndInitializerList) {
  chaistl::unordered_map<int, std::string> empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.bucket_count(), 0u);

  chaistl::unordered_map<int, std::string> map{{1, "one"}, {2, "two"}, {1, "ignored"}};
  EXPECT_THAT(map, UnorderedElementsAre(Pair(1, "one"), Pair(2, "two")));
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapCons, CopyAndMove) {
  chaistl::unordered_map<int, std::string> source{{1, "one"}, {2, "two"}};

  chaistl::unordered_map<int, std::string> copy(source);
  EXPECT_EQ(copy, source);
  copy[3] = "three";
  EXPECT_FALSE(source.contains(3));

  const std::string* stable = &source.find(1)->second;
  chaistl::unordered_map<int, std::string> moved(std::move(source));
  EXPECT_EQ(stable, &moved.find(1)->second);  // nodes transferred, addresses stable
  EXPECT_TRUE(moved.validate());
}

TEST(UnorderedMapCons, FromRange) {
  // pair<int, int> elements (no const key) are convertible to value_type.
  const std::vector<std::pair<int, int>> entries{{1, 10}, {2, 20}, {1, 99}};

  chaistl::unordered_map<int, int> map(std::from_range, entries);

  EXPECT_THAT(map, UnorderedElementsAre(Pair(1, 10), Pair(2, 20)));
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapCons, DeductionGuides) {
  const std::vector<std::pair<int, std::string>> entries{{1, "one"}, {2, "two"}};

  // iter-key-type strips the (absent) const; the allocator re-adds it.
  chaistl::unordered_map m1(entries.begin(), entries.end());
  static_assert(std::is_same_v<decltype(m1), chaistl::unordered_map<int, std::string>>);

  chaistl::unordered_map m2{std::pair{1, 2.0}, {3, 4.0}};
  static_assert(std::is_same_v<decltype(m2), chaistl::unordered_map<int, double>>);

  // The integral third argument is a bucket count, never a Hash.
  chaistl::unordered_map m3(entries.begin(), entries.end(), 64);
  static_assert(std::is_same_v<decltype(m3), chaistl::unordered_map<int, std::string>>);
  EXPECT_GE(m3.bucket_count(), 64u);

  chaistl::unordered_map m4(entries.begin(), entries.end(), 0, chaistl::hash<int>{});
  static_assert(std::is_same_v<decltype(m4), chaistl::unordered_map<int, std::string, chaistl::hash<int>>>);

  chaistl::unordered_map m5(std::from_range, entries);
  static_assert(std::is_same_v<decltype(m5), chaistl::unordered_map<int, std::string>>);
  EXPECT_THAT(m5, UnorderedElementsAre(Pair(1, "one"), Pair(2, "two")));
}

}  // namespace
