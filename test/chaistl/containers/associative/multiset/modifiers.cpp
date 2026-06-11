// SPDX-License-Identifier: Apache-2.0

// References:
// - std::multiset modifiers:
//   https://en.cppreference.com/w/cpp/container/multiset
//   https://eel.is/c++draft/multiset.modifiers
// - LWG 233 / N1780: insertion position for equivalent keys
//   https://cplusplus.github.io/LWG/issue233

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multiset.hpp>

#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// Equivalent under TaggedLess while the tag stays observable — the only
// way to see where inside a run an element landed.
struct Tagged {
  int key;
  int tag;
};

struct TaggedLess {
  bool operator()(const Tagged& a, const Tagged& b) const { return a.key < b.key; }
};

using TaggedMultiset = chaistl::multiset<Tagged, TaggedLess>;

std::vector<int> tags_of(const TaggedMultiset& ms, int key) {
  std::vector<int> tags;
  auto [first, last] = ms.equal_range(Tagged{key, 0});
  for (; first != last; ++first) {
    tags.push_back(first->tag);
  }
  return tags;
}

TEST(MultisetModifiers, InsertReturnsIteratorAndGrows) {
  chaistl::multiset<int> ms;

  auto it1 = ms.insert(1);
  EXPECT_EQ(*it1, 1);
  auto it2 = ms.insert(1);
  EXPECT_EQ(*it2, 1);
  auto it3 = ms.insert(1);
  EXPECT_EQ(*it3, 1);

  EXPECT_EQ(ms.size(), 3uz);
  EXPECT_THAT(ms, ElementsAre(1, 1, 1));
  EXPECT_TRUE(ms.validate());
}

// LWG 233: insert(value) places the element at the upper bound of its run,
// so a run reads in insertion order.
TEST(MultisetModifiers, PlainInsertAppendsToRun) {
  TaggedMultiset ms;
  ms.insert(Tagged{1, 1});
  ms.insert(Tagged{2, 9});
  ms.insert(Tagged{1, 2});
  ms.insert(Tagged{1, 3});

  EXPECT_THAT(tags_of(ms, 1), ElementsAre(1, 2, 3));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetModifiers, EmplaceAppendsToRun) {
  TaggedMultiset ms;
  ms.emplace(Tagged{1, 1});
  ms.emplace(Tagged{1, 2});
  EXPECT_THAT(tags_of(ms, 1), ElementsAre(1, 2));
  EXPECT_TRUE(ms.validate());
}

// [associative.reqmts]: with a hint the element lands as close as possible
// to (= directly before) the hint position.
TEST(MultisetModifiers, HintInsertLandsBeforeHint) {
  TaggedMultiset ms;
  ms.insert(Tagged{1, 1});
  ms.insert(Tagged{1, 2});
  ms.insert(Tagged{1, 3});

  // Hint at the middle element of the run {1,2}: insert lands before it.
  auto hint = std::next(ms.begin());
  ASSERT_EQ(hint->tag, 2);
  auto it = ms.insert(hint, Tagged{1, 99});
  EXPECT_EQ(it->tag, 99);

  EXPECT_THAT(tags_of(ms, 1), ElementsAre(1, 99, 2, 3));
  EXPECT_TRUE(ms.validate());

  // Hint at begin().
  ms.insert(ms.begin(), Tagged{1, 100});
  EXPECT_THAT(tags_of(ms, 1), ElementsAre(100, 1, 99, 2, 3));

  // Hint at end().
  ms.insert(ms.end(), Tagged{1, 101});
  EXPECT_THAT(tags_of(ms, 1), ElementsAre(100, 1, 99, 2, 3, 101));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetModifiers, HintInsertFallbackPaths) {
  // Exercise the fallback paths in get_insert_hint_equal_pos where the
  // hint is not close to the insert position.
  chaistl::multiset<int> ms{10, 20, 30, 40, 50};

  // Hint at begin() for value 35: hint is wrong, falls back to full search.
  // 35 should land between 30 and 40.
  auto it = ms.insert(ms.begin(), 35);
  EXPECT_EQ(*it, 35);
  EXPECT_TRUE(ms.validate());

  // Hint at a middle element for value 5: hint is wrong, falls back.
  it = ms.insert(ms.find(30), 5);
  EXPECT_EQ(*it, 5);
  EXPECT_TRUE(ms.validate());

  // Hint at end() for value 55: key > max, fast path when root != nullptr.
  it = ms.insert(ms.end(), 55);
  EXPECT_EQ(*it, 55);
  EXPECT_TRUE(ms.validate());

  // Hint at begin() for value 0: key <= min, fast path.
  it = ms.insert(ms.begin(), 0);
  EXPECT_EQ(*it, 0);
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetModifiers, EmplaceHintLandsBeforeHint) {
  TaggedMultiset ms;
  ms.emplace(Tagged{1, 1});
  ms.emplace(Tagged{1, 2});

  auto hint = std::next(ms.begin());
  ASSERT_EQ(hint->tag, 2);
  auto it = ms.emplace_hint(hint, Tagged{1, 99});
  EXPECT_EQ(it->tag, 99);
  EXPECT_THAT(tags_of(ms, 1), ElementsAre(1, 99, 2));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetModifiers, InsertRangeKeepsDuplicates) {
  chaistl::multiset<int> ms{2};
  const std::vector<int> v{1, 2, 1};
  ms.insert(v.begin(), v.end());
  EXPECT_THAT(ms, ElementsAre(1, 1, 2, 2));

  ms.insert({3, 3});
  EXPECT_THAT(ms, ElementsAre(1, 1, 2, 2, 3, 3));

  ms.insert_range(std::vector<int>{0, 0});
  EXPECT_THAT(ms, ElementsAre(0, 0, 1, 1, 2, 2, 3, 3));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetModifiers, ErasePosition) {
  chaistl::multiset<int> ms{1, 1, 2};
  auto next = ms.erase(ms.begin());
  EXPECT_EQ(*next, 1);
  EXPECT_THAT(ms, ElementsAre(1, 2));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetModifiers, EraseKeyRemovesWholeRun) {
  chaistl::multiset<int> ms{1, 2, 2, 2, 3};

  EXPECT_EQ(ms.erase(2), 3uz);
  EXPECT_THAT(ms, ElementsAre(1, 3));
  EXPECT_TRUE(ms.validate());

  EXPECT_EQ(ms.erase(42), 0uz);
  EXPECT_THAT(ms, ElementsAre(1, 3));
}

TEST(MultisetModifiers, EraseIteratorRange) {
  chaistl::multiset<int> ms{1, 2, 2, 3, 4};
  auto [first, last] = ms.equal_range(2);
  auto next = ms.erase(first, last);
  EXPECT_EQ(*next, 3);
  EXPECT_THAT(ms, ElementsAre(1, 3, 4));
  EXPECT_TRUE(ms.validate());

  ms.erase(ms.begin(), ms.end());
  EXPECT_THAT(ms, IsEmpty());
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetModifiers, MergeTakesEverything) {
  chaistl::multiset<int> dst{1, 2};
  chaistl::multiset<int> src{1, 1, 3};

  dst.merge(src);

  EXPECT_THAT(src, IsEmpty());
  EXPECT_THAT(dst, ElementsAre(1, 1, 1, 2, 3));
  EXPECT_TRUE(dst.validate());
  EXPECT_TRUE(src.validate());
}

TEST(MultisetModifiers, MergeDifferentComparator) {
  chaistl::multiset<int> dst{2};
  chaistl::multiset<int, std::greater<int>> src{3, 1, 3};

  dst.merge(src);

  EXPECT_THAT(src, IsEmpty());
  EXPECT_THAT(dst, ElementsAre(1, 2, 3, 3));
  EXPECT_TRUE(dst.validate());
}

TEST(MultisetModifiers, MergePreservesInsertionOrderWithinRun) {
  TaggedMultiset dst;
  dst.insert(Tagged{1, 1});
  TaggedMultiset src;
  src.insert(Tagged{1, 2});
  src.insert(Tagged{1, 3});

  dst.merge(src);

  // Source elements arrive in source order, each appended to the run.
  EXPECT_THAT(tags_of(dst, 1), ElementsAre(1, 2, 3));
  EXPECT_TRUE(dst.validate());
}

TEST(MultisetModifiers, Swap) {
  chaistl::multiset<int> a{1, 1};
  chaistl::multiset<int> b{2, 2, 2};

  a.swap(b);
  EXPECT_THAT(a, ElementsAre(2, 2, 2));
  EXPECT_THAT(b, ElementsAre(1, 1));

  swap(a, b);
  EXPECT_THAT(a, ElementsAre(1, 1));
  EXPECT_THAT(b, ElementsAre(2, 2, 2));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(MultisetModifiers, Clear) {
  chaistl::multiset<std::string> ms{"a", "a", "b"};
  ms.clear();
  EXPECT_THAT(ms, IsEmpty());
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetModifiers, ComparisonOperators) {
  chaistl::multiset<int> a{1, 1, 2};
  chaistl::multiset<int> b{1, 1, 2};
  chaistl::multiset<int> c{1, 2};

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  // Lexicographic: {1,1,2} < {1,2} at the second element.
  EXPECT_LT(a, c);
  EXPECT_EQ(a <=> b, std::weak_ordering::equivalent);
}

}  // namespace
