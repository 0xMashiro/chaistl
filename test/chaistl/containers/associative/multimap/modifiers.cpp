// SPDX-License-Identifier: Apache-2.0

// References:
// - std::multimap modifiers:
//   https://en.cppreference.com/w/cpp/container/multimap
//   https://eel.is/c++draft/multimap.modifiers
// - LWG 233 / N1780: insertion position for equivalent keys

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multimap.hpp>

#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

// The mapped value doubles as the insertion tag: the only way to observe
// where inside an equivalent-key run an element landed.
std::vector<int> mapped_of(const chaistl::multimap<int, int>& mm, int key) {
  std::vector<int> mapped;
  auto [first, last] = mm.equal_range(key);
  for (; first != last; ++first) {
    mapped.push_back(first->second);
  }
  return mapped;
}

// LWG 233: insert(value) places the element at the upper bound of its run.
TEST(MultimapModifiers, PlainInsertAppendsToRun) {
  chaistl::multimap<int, int> mm;
  mm.insert({1, 1});
  mm.insert({2, 9});
  mm.insert({1, 2});
  mm.insert({1, 3});

  EXPECT_THAT(mapped_of(mm, 1), ElementsAre(1, 2, 3));
  EXPECT_EQ(mm.size(), 4uz);
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapModifiers, EmplaceAppendsToRun) {
  chaistl::multimap<int, int> mm;
  mm.emplace(1, 1);
  mm.emplace(1, 2);
  auto it = mm.emplace(std::piecewise_construct, std::forward_as_tuple(1), std::forward_as_tuple(3));
  EXPECT_EQ(it->second, 3);
  EXPECT_THAT(mapped_of(mm, 1), ElementsAre(1, 2, 3));
  EXPECT_TRUE(mm.validate());
}

// [associative.reqmts]: with a hint the element lands as close as possible
// to (= directly before) the hint position.
TEST(MultimapModifiers, HintInsertLandsBeforeHint) {
  chaistl::multimap<int, int> mm;
  mm.insert({1, 1});
  mm.insert({1, 2});

  auto hint = std::next(mm.begin());
  ASSERT_EQ(hint->second, 2);
  auto it = mm.insert(hint, {1, 99});
  EXPECT_EQ(it->second, 99);
  EXPECT_THAT(mapped_of(mm, 1), ElementsAre(1, 99, 2));

  auto it2 = mm.emplace_hint(mm.begin(), 1, 100);
  EXPECT_EQ(it2->second, 100);
  EXPECT_THAT(mapped_of(mm, 1), ElementsAre(100, 1, 99, 2));
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapModifiers, InsertRangeKeepsDuplicates) {
  chaistl::multimap<int, int> mm;
  const std::vector<std::pair<const int, int>> v{{1, 1}, {1, 2}};
  mm.insert(v.begin(), v.end());
  mm.insert({{2, 1}, {2, 2}});
  mm.insert_range(std::vector<std::pair<int, int>>{{0, 1}, {0, 2}});

  EXPECT_THAT(mm, ElementsAre(Pair(0, 1), Pair(0, 2), Pair(1, 1), Pair(1, 2), Pair(2, 1), Pair(2, 2)));
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapModifiers, EraseKeyRemovesWholeRun) {
  chaistl::multimap<int, int> mm{{1, 1}, {2, 1}, {2, 2}, {2, 3}, {3, 1}};

  EXPECT_EQ(mm.erase(2), 3uz);
  EXPECT_THAT(mm, ElementsAre(Pair(1, 1), Pair(3, 1)));
  EXPECT_EQ(mm.erase(42), 0uz);
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapModifiers, ErasePositionAndRange) {
  chaistl::multimap<int, int> mm{{1, 1}, {1, 2}, {2, 1}};

  auto next = mm.erase(mm.begin());
  EXPECT_EQ(next->second, 2);

  auto [first, last] = mm.equal_range(1);
  next = mm.erase(first, last);
  EXPECT_EQ(next->first, 2);
  EXPECT_THAT(mm, ElementsAre(Pair(2, 1)));
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapModifiers, MergeTakesEverything) {
  chaistl::multimap<int, int> dst{{1, 1}};
  chaistl::multimap<int, int> src{{1, 2}, {2, 1}};

  dst.merge(src);

  EXPECT_THAT(src, IsEmpty());
  EXPECT_THAT(dst, ElementsAre(Pair(1, 1), Pair(1, 2), Pair(2, 1)));
  EXPECT_TRUE(dst.validate());
  EXPECT_TRUE(src.validate());
}

TEST(MultimapModifiers, MergeDifferentComparator) {
  chaistl::multimap<int, int> dst{{2, 1}};
  chaistl::multimap<int, int, std::greater<int>> src{{3, 1}, {1, 1}};

  dst.merge(src);

  EXPECT_THAT(src, IsEmpty());
  EXPECT_THAT(dst, ElementsAre(Pair(1, 1), Pair(2, 1), Pair(3, 1)));
  EXPECT_TRUE(dst.validate());
}

TEST(MultimapModifiers, SwapAndClear) {
  chaistl::multimap<int, int> a{{1, 1}, {1, 2}};
  chaistl::multimap<int, int> b{{2, 1}};

  swap(a, b);
  EXPECT_THAT(a, ElementsAre(Pair(2, 1)));
  EXPECT_THAT(b, ElementsAre(Pair(1, 1), Pair(1, 2)));

  b.clear();
  EXPECT_THAT(b, IsEmpty());
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(MultimapModifiers, ComparisonOperators) {
  chaistl::multimap<int, int> a{{1, 1}, {1, 2}};
  chaistl::multimap<int, int> b{{1, 1}, {1, 2}};
  chaistl::multimap<int, int> c{{1, 1}};

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_GT(a, c);  // {<1,1>,<1,2>} > {<1,1>} lexicographically (prefix)
}

}  // namespace
