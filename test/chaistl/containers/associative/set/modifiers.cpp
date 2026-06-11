// SPDX-License-Identifier: Apache-2.0

// References:
// - std::set modifiers:
//   https://en.cppreference.com/w/cpp/container/set
//   https://eel.is/c++draft/set.modifiers

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/set.hpp>

#include <string>
#include <utility>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(SetModifiers, InsertUnique) {
  chaistl::set<int> s;

  auto [it1, inserted1] = s.insert(1);
  EXPECT_TRUE(inserted1);
  EXPECT_EQ(*it1, 1);
  EXPECT_TRUE(s.validate());

  auto [it2, inserted2] = s.insert(2);
  EXPECT_TRUE(inserted2);
  EXPECT_EQ(*it2, 2);
  EXPECT_TRUE(s.validate());

  // Duplicate
  auto [it3, inserted3] = s.insert(1);
  EXPECT_FALSE(inserted3);
  EXPECT_EQ(*it3, 1);
  EXPECT_EQ(s.size(), 2uz);
  EXPECT_TRUE(s.validate());
}

TEST(SetModifiers, InsertRvalue) {
  chaistl::set<std::string> s;
  std::string value = "hello";
  auto [it, inserted] = s.insert(std::move(value));
  EXPECT_TRUE(inserted);
  EXPECT_EQ(*it, "hello");
  EXPECT_TRUE(s.validate());
}

TEST(SetModifiers, InsertHintBasic) {
  chaistl::set<int> s{1, 3, 5};

  // Hint at end for value greater than all
  auto it = s.insert(s.end(), 6);
  EXPECT_EQ(*it, 6);
  EXPECT_TRUE(s.validate());

  // Hint at begin for value smaller than all
  it = s.insert(s.begin(), 0);
  EXPECT_EQ(*it, 0);
  EXPECT_TRUE(s.validate());

  EXPECT_THAT(s, ElementsAre(0, 1, 3, 5, 6));
}

TEST(SetModifiers, Emplace) {
  chaistl::set<std::pair<int, int>> s;
  auto [it, inserted] = s.emplace(1, 2);
  EXPECT_TRUE(inserted);
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, 2);
  EXPECT_TRUE(s.validate());
}

// Regression: emplacing an existing key must drop the speculatively
// constructed node exactly once (this used to double-deallocate).
TEST(SetModifiers, EmplaceDuplicate) {
  chaistl::set<std::string> s;

  auto [it1, inserted1] = s.emplace("hello");
  EXPECT_TRUE(inserted1);

  auto [it2, inserted2] = s.emplace("hello");
  EXPECT_FALSE(inserted2);
  EXPECT_EQ(it2, it1);
  EXPECT_EQ(s.size(), 1uz);
  EXPECT_TRUE(s.validate());

  auto it3 = s.emplace_hint(s.end(), "hello");
  EXPECT_EQ(it3, it1);
  EXPECT_EQ(s.size(), 1uz);
  EXPECT_TRUE(s.validate());
}

TEST(SetModifiers, EraseByIterator) {
  chaistl::set<int> s{1, 2, 3, 4, 5};

  auto it = s.find(3);
  auto next = s.erase(it);
  EXPECT_EQ(*next, 4);
  EXPECT_THAT(s, ElementsAre(1, 2, 4, 5));
  EXPECT_TRUE(s.validate());
}

TEST(SetModifiers, EraseByKey) {
  chaistl::set<int> s{1, 2, 3, 4, 5};

  EXPECT_EQ(s.erase(3), 1uz);
  EXPECT_THAT(s, ElementsAre(1, 2, 4, 5));
  EXPECT_TRUE(s.validate());

  // Erase non-existent
  EXPECT_EQ(s.erase(99), 0uz);
  EXPECT_TRUE(s.validate());
}

TEST(SetModifiers, EraseRange) {
  chaistl::set<int> s{1, 2, 3, 4, 5};

  auto first = s.find(2);
  auto last = s.find(5);
  auto next = s.erase(first, last);
  EXPECT_EQ(*next, 5);
  EXPECT_THAT(s, ElementsAre(1, 5));
  EXPECT_TRUE(s.validate());
}

TEST(SetModifiers, Clear) {
  chaistl::set<int> s{1, 2, 3};
  s.clear();
  EXPECT_THAT(s, IsEmpty());
  EXPECT_EQ(s.size(), 0uz);

  // Can insert after clear
  s.insert(42);
  EXPECT_THAT(s, ElementsAre(42));
  EXPECT_TRUE(s.validate());
}

TEST(SetModifiers, Swap) {
  chaistl::set<int> a{1, 2, 3};
  chaistl::set<int> b{4, 5};

  a.swap(b);
  EXPECT_THAT(a, ElementsAre(4, 5));
  EXPECT_THAT(b, ElementsAre(1, 2, 3));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(SetModifiers, LargeInsert) {
  chaistl::set<int> s;
  for (int i = 0; i < 1000; ++i) {
    s.insert(i);
  }
  EXPECT_EQ(s.size(), 1000uz);
  EXPECT_TRUE(s.validate());

  // Insert duplicates
  for (int i = 0; i < 1000; ++i) {
    auto [it, inserted] = s.insert(i);
    EXPECT_FALSE(inserted);
  }
  EXPECT_EQ(s.size(), 1000uz);
  EXPECT_TRUE(s.validate());
}

TEST(SetModifiers, InsertAndEraseAlternating) {
  chaistl::set<int> s;
  for (int i = 0; i < 100; ++i) {
    s.insert(i);
    EXPECT_TRUE(s.validate());
  }
  for (int i = 0; i < 100; ++i) {
    s.erase(i);
    EXPECT_TRUE(s.validate());
  }
  EXPECT_THAT(s, IsEmpty());
}

TEST(SetModifiers, InsertHint) {
  chaistl::set<int> s{1, 3, 5};

  // Hint at end for value greater than all
  auto it = s.insert(s.end(), 6);
  EXPECT_EQ(*it, 6);
  EXPECT_TRUE(s.validate());

  // Hint at begin for value smaller than all
  it = s.insert(s.begin(), 0);
  EXPECT_EQ(*it, 0);
  EXPECT_TRUE(s.validate());

  // Hint at lower_bound(3) for value 2: 2 < 3, belongs before hint.
  // Predecessor of hint is 1, and 1 < 2, so the fast path applies.
  it = s.insert(s.lower_bound(3), 2);
  EXPECT_EQ(*it, 2);
  EXPECT_TRUE(s.validate());

  // Hint at lower_bound(3) for value 4: 4 > 3, belongs after hint.
  // Successor of hint is 5, and 4 < 5, so the fast path applies.
  it = s.insert(s.lower_bound(3), 4);
  EXPECT_EQ(*it, 4);
  EXPECT_TRUE(s.validate());

  // Hint at begin() for value 7 (greater than max): hint is wrong,
  // falls back to full position search.
  it = s.insert(s.begin(), 7);
  EXPECT_EQ(*it, 7);
  EXPECT_TRUE(s.validate());

  // Duplicate with hint: should return existing element.
  it = s.insert(s.begin(), 3);
  EXPECT_EQ(*it, 3);
  EXPECT_EQ(s.size(), 8uz);
  EXPECT_TRUE(s.validate());

  EXPECT_THAT(s, ElementsAre(0, 1, 2, 3, 4, 5, 6, 7));
}

TEST(SetModifiers, InsertRange) {
  chaistl::set<int> s{1, 3, 5};
  const std::vector<int> extra{2, 4, 5, 6};

  s.insert_range(extra);
  EXPECT_THAT(s, ElementsAre(1, 2, 3, 4, 5, 6));
  EXPECT_TRUE(s.validate());
}

}  // namespace
