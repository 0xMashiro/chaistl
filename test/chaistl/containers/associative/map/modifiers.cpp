// SPDX-License-Identifier: Apache-2.0

// References:
// - std::map modifiers:
//   https://en.cppreference.com/w/cpp/container/map
//   https://eel.is/c++draft/map.modifiers

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/map.hpp>

#include <string>
#include <utility>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(MapModifiers, Insert) {
  chaistl::map<int, std::string> m;

  auto [it1, inserted1] = m.insert({1, "one"});
  EXPECT_TRUE(inserted1);
  EXPECT_EQ(it1->first, 1);
  EXPECT_EQ(it1->second, "one");
  EXPECT_TRUE(m.validate());

  // Duplicate key
  auto [it2, inserted2] = m.insert({1, "duplicate"});
  EXPECT_FALSE(inserted2);
  EXPECT_EQ(it2->second, "one");  // Original value preserved
  EXPECT_TRUE(m.validate());
}

TEST(MapModifiers, InsertHint) {
  chaistl::map<int, std::string> m{{1, "one"}, {3, "three"}};

  auto it = m.insert(m.end(), {5, "five"});
  EXPECT_EQ(it->first, 5);
  EXPECT_TRUE(m.validate());
}

TEST(MapModifiers, Emplace) {
  chaistl::map<int, std::string> m;
  auto [it, inserted] = m.emplace(42, "answer");
  EXPECT_TRUE(inserted);
  EXPECT_EQ(it->first, 42);
  EXPECT_EQ(it->second, "answer");
  EXPECT_TRUE(m.validate());
}

TEST(MapModifiers, TryEmplace) {
  chaistl::map<int, std::string> m;

  auto [it1, inserted1] = m.try_emplace(1, "one");
  EXPECT_TRUE(inserted1);
  EXPECT_EQ(it1->second, "one");

  auto [it2, inserted2] = m.try_emplace(1, "duplicate");
  EXPECT_FALSE(inserted2);
  EXPECT_EQ(it2->second, "one");  // Original preserved
  EXPECT_TRUE(m.validate());
}

TEST(MapModifiers, InsertOrAssign) {
  chaistl::map<int, std::string> m;

  auto [it1, inserted1] = m.insert_or_assign(1, "one");
  EXPECT_TRUE(inserted1);
  EXPECT_EQ(it1->second, "one");

  auto [it2, inserted2] = m.insert_or_assign(1, "updated");
  EXPECT_FALSE(inserted2);
  EXPECT_EQ(it2->second, "updated");
  EXPECT_TRUE(m.validate());
}

TEST(MapModifiers, InsertOrAssignHintInsertsAbsentKey) {
  chaistl::map<int, std::string> m{{1, "one"}, {3, "three"}};

  auto it = m.insert_or_assign(m.find(3), 2, "two");

  ASSERT_NE(it, m.end());
  EXPECT_EQ(it->first, 2);
  EXPECT_EQ(it->second, "two");
  EXPECT_EQ(m.size(), 3uz);
  EXPECT_THAT(m, ElementsAre(Pair(1, "one"), Pair(2, "two"), Pair(3, "three")));
  EXPECT_TRUE(m.validate());
}

TEST(MapModifiers, InsertOrAssignHintAssignsExistingKey) {
  chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}};

  auto it = m.insert_or_assign(m.end(), 1, "updated");

  ASSERT_NE(it, m.end());
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, "updated");
  EXPECT_EQ(m.size(), 2uz);
  EXPECT_THAT(m, ElementsAre(Pair(1, "updated"), Pair(2, "two")));
  EXPECT_TRUE(m.validate());
}

TEST(MapModifiers, EraseByKey) {
  chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}, {3, "three"}};

  EXPECT_EQ(m.erase(2), 1uz);
  EXPECT_FALSE(m.contains(2));
  EXPECT_EQ(m.size(), 2uz);
  EXPECT_TRUE(m.validate());

  EXPECT_EQ(m.erase(99), 0uz);
}

TEST(MapModifiers, EraseByIterator) {
  chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}, {3, "three"}};

  auto it = m.find(2);
  auto next = m.erase(it);
  EXPECT_EQ(next->first, 3);
  EXPECT_TRUE(m.validate());
}

TEST(MapModifiers, Clear) {
  chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}};
  m.clear();
  EXPECT_THAT(m, IsEmpty());

  m[42] = "answer";
  EXPECT_EQ(m.size(), 1uz);
  EXPECT_TRUE(m.validate());
}

TEST(MapModifiers, Swap) {
  chaistl::map<int, std::string> a{{1, "one"}};
  chaistl::map<int, std::string> b{{2, "two"}, {3, "three"}};

  a.swap(b);
  EXPECT_EQ(a.size(), 2uz);
  EXPECT_EQ(b.size(), 1uz);
  EXPECT_TRUE(a.contains(2));
  EXPECT_TRUE(b.contains(1));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(MapModifiers, LargeInsertAndErase) {
  chaistl::map<int, int> m;
  for (int i = 0; i < 500; ++i) {
    m.insert({i, i * 2});
    EXPECT_TRUE(m.validate());
  }
  for (int i = 0; i < 500; ++i) {
    m.erase(i);
    EXPECT_TRUE(m.validate());
  }
  EXPECT_THAT(m, IsEmpty());
}

TEST(MapModifiers, InsertRange) {
  chaistl::map<int, std::string> m{{1, "one"}, {3, "three"}};
  const std::vector<std::pair<int, std::string>> extra{{2, "two"}, {3, "duplicate"}, {4, "four"}};

  m.insert_range(extra);
  EXPECT_EQ(m.size(), 4uz);
  EXPECT_EQ(m[1], "one");
  EXPECT_EQ(m[2], "two");
  EXPECT_EQ(m[3], "three");
  EXPECT_EQ(m[4], "four");
  EXPECT_TRUE(m.validate());
}

}  // namespace
