// SPDX-License-Identifier: Apache-2.0

// References:
// - std::multimap constructors:
//   https://en.cppreference.com/w/cpp/container/multimap/multimap
//   https://eel.is/c++draft/multimap.cons

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multimap.hpp>

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "../../sequence/support.hpp"

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(MultimapCons, Default) {
  chaistl::multimap<int, std::string> mm;
  EXPECT_TRUE(mm.empty());
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapCons, InitializerListKeepsDuplicates) {
  chaistl::multimap<int, int> mm{{2, 20}, {1, 10}, {2, 21}, {1, 11}};
  EXPECT_EQ(mm.size(), 4uz);
  EXPECT_THAT(mm, ElementsAre(Pair(1, 10), Pair(1, 11), Pair(2, 20), Pair(2, 21)));
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapCons, IteratorRangeKeepsDuplicates) {
  const std::vector<std::pair<const int, int>> v{{3, 1}, {1, 1}, {3, 2}};
  chaistl::multimap<int, int> mm(v.begin(), v.end());
  EXPECT_THAT(mm, ElementsAre(Pair(1, 1), Pair(3, 1), Pair(3, 2)));
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapCons, FromRange) {
  const std::vector<std::pair<int, int>> v{{1, 1}, {1, 2}};
  chaistl::multimap<int, int> mm(std::from_range, v);
  EXPECT_THAT(mm, ElementsAre(Pair(1, 1), Pair(1, 2)));
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapCons, CopyAndMove) {
  chaistl::multimap<int, int> src{{1, 1}, {1, 2}};

  chaistl::multimap<int, int> copy(src);
  EXPECT_THAT(copy, ElementsAre(Pair(1, 1), Pair(1, 2)));

  chaistl::multimap<int, int> moved(std::move(src));
  EXPECT_THAT(moved, ElementsAre(Pair(1, 1), Pair(1, 2)));
  EXPECT_THAT(src, IsEmpty());
  EXPECT_TRUE(moved.validate());
  EXPECT_TRUE(src.validate());
}

TEST(MultimapCons, MoveConstructWithDifferentAllocatorKeepsDuplicates) {
  using Value = std::pair<const int, int>;
  using Alloc = chaistl::test::TaggedAllocator<Value>;
  using Multimap = chaistl::multimap<int, int, std::less<int>, Alloc>;

  Multimap src(Alloc{1});
  src.insert({1, 10});
  src.insert({1, 11});
  src.insert({2, 20});

  Multimap dst(std::move(src), Alloc{2});

  EXPECT_THAT(dst, ElementsAre(Pair(1, 10), Pair(1, 11), Pair(2, 20)));
  EXPECT_EQ(dst.size(), 3uz);
  EXPECT_TRUE(dst.validate());
}

TEST(MultimapCons, MoveAssignWithDifferentAllocatorKeepsDuplicates) {
  using Value = std::pair<const int, int>;
  using Alloc = chaistl::test::TaggedAllocator<Value>;
  using Multimap = chaistl::multimap<int, int, std::less<int>, Alloc>;

  Multimap src(Alloc{1});
  src.insert({1, 10});
  src.insert({1, 11});
  src.insert({2, 20});
  Multimap dst(Alloc{2});
  dst.insert({9, 90});

  dst = std::move(src);

  EXPECT_THAT(dst, ElementsAre(Pair(1, 10), Pair(1, 11), Pair(2, 20)));
  EXPECT_EQ(dst.size(), 3uz);
  EXPECT_TRUE(dst.validate());
}

TEST(MultimapCons, AssignInitializerListUsesEqualFamily) {
  chaistl::multimap<int, int> mm{{9, 9}};
  mm = {{4, 1}, {4, 2}};
  EXPECT_THAT(mm, ElementsAre(Pair(4, 1), Pair(4, 2)));
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapCons, DeductionGuides) {
  const std::vector<std::pair<int, int>> v{{2, 20}, {1, 10}, {2, 21}};

  chaistl::multimap mm1(v.begin(), v.end());
  static_assert(std::same_as<decltype(mm1), chaistl::multimap<int, int>>);
  EXPECT_EQ(mm1.size(), 3uz);

  chaistl::multimap mm2{std::pair{7, 1}, std::pair{7, 2}};
  static_assert(std::same_as<decltype(mm2), chaistl::multimap<int, int>>);
  EXPECT_EQ(mm2.count(7), 2uz);

  chaistl::multimap mm3(std::from_range, v);
  static_assert(std::same_as<decltype(mm3), chaistl::multimap<int, int>>);
  EXPECT_EQ(mm3.size(), 3uz);

  // Comparator in the trailing slot must not be deduced as an allocator.
  chaistl::multimap mm4(v.begin(), v.end(), std::greater<int>{});
  static_assert(std::same_as<decltype(mm4), chaistl::multimap<int, int, std::greater<int>>>);
  EXPECT_EQ(mm4.begin()->first, 2);
}

}  // namespace
