// SPDX-License-Identifier: Apache-2.0

// References:
// - std::map node handle / extract / merge:
//   https://en.cppreference.com/w/cpp/container/map/extract
//   https://en.cppreference.com/w/cpp/container/map/merge

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/map.hpp>

#include <concepts>
#include <functional>
#include <string>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(MapNodeHandle, ExtractByIterator) {
  chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}, {3, "three"}};

  auto it = m.find(2);
  auto node = m.extract(it);

  EXPECT_TRUE(node);
  EXPECT_EQ(node.key(), 2);
  EXPECT_EQ(node.mapped(), "two");
  EXPECT_THAT(m, ElementsAre(Pair(1, "one"), Pair(3, "three")));
  EXPECT_TRUE(m.validate());
}

TEST(MapNodeHandle, ExtractByKey) {
  chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}};

  auto node = m.extract(2);
  EXPECT_TRUE(node);
  EXPECT_EQ(node.key(), 2);
  EXPECT_EQ(node.mapped(), "two");

  auto missing = m.extract(99);
  EXPECT_FALSE(missing);

  EXPECT_THAT(m, ElementsAre(Pair(1, "one")));
  EXPECT_TRUE(m.validate());
}

TEST(MapNodeHandle, InsertNodeHandle) {
  chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}};

  auto node = m.extract(2);
  node.mapped() = "forty-two";

  auto ret = m.insert(std::move(node));
  EXPECT_TRUE(ret.inserted);
  EXPECT_EQ(ret.position->first, 2);
  EXPECT_EQ(ret.position->second, "forty-two");
  EXPECT_FALSE(ret.node);
  EXPECT_THAT(m, ElementsAre(Pair(1, "one"), Pair(2, "forty-two")));
  EXPECT_TRUE(m.validate());
}

TEST(MapNodeHandle, InsertDuplicateNodeHandle) {
  chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}};

  // Extract an existing key and try to reinsert it: should succeed.
  auto node = m.extract(2);
  auto ret = m.insert(std::move(node));
  EXPECT_TRUE(ret.inserted);
  EXPECT_FALSE(ret.node);

  // Extract a key and attempt to insert into a container that already has it.
  chaistl::map<int, std::string> other{{1, "other_one"}, {2, "other_two"}};
  auto dup = other.extract(2);
  auto ret2 = m.insert(std::move(dup));
  EXPECT_FALSE(ret2.inserted);
  EXPECT_EQ(ret2.position->first, 2);
  EXPECT_EQ(ret2.position->second, "two");
  EXPECT_TRUE(ret2.node);
  EXPECT_EQ(ret2.node.key(), 2);
  EXPECT_EQ(ret2.node.mapped(), "other_two");
  EXPECT_TRUE(m.validate());
}

TEST(MapNodeHandle, KeyIsMutableWhileExtracted) {
  // [container.node.overview]: the point of extract() is that the key can be
  // updated outside the container and the node reinserted without reallocation.
  chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}};

  auto nh = m.extract(1);
  static_assert(std::same_as<decltype(nh.key()), int&>);
  nh.key() = 42;
  m.insert(std::move(nh));

  EXPECT_THAT(m, ElementsAre(Pair(2, "two"), Pair(42, "one")));
  EXPECT_TRUE(m.validate());
}

TEST(MapNodeHandle, InsertReturnTypeIsPlainMemberType) {
  chaistl::map<int, std::string> m{{1, "one"}};

  chaistl::map<int, std::string>::insert_return_type ret = m.insert(m.extract(1));

  EXPECT_TRUE(ret.inserted);
  EXPECT_FALSE(ret.node);
  EXPECT_EQ(ret.position->first, 1);
}

TEST(MapNodeHandle, MergeAcceptsDifferentComparator) {
  chaistl::map<int, std::string> a{{1, "a1"}, {3, "a3"}};
  chaistl::map<int, std::string, std::greater<int>> b{{2, "b2"}, {3, "b3"}};

  a.merge(b);

  EXPECT_THAT(a, ElementsAre(Pair(1, "a1"), Pair(2, "b2"), Pair(3, "a3")));
  EXPECT_THAT(b, ElementsAre(Pair(3, "b3")));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(MapNodeHandle, MergeTransfersNonDuplicates) {
  chaistl::map<int, std::string> a{{1, "a1"}, {3, "a3"}};
  chaistl::map<int, std::string> b{{2, "b2"}, {3, "b3"}};

  a.merge(b);

  EXPECT_THAT(a, ElementsAre(Pair(1, "a1"), Pair(2, "b2"), Pair(3, "a3")));
  EXPECT_THAT(b, ElementsAre(Pair(3, "b3")));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(MapNodeHandle, MergeEmptySource) {
  chaistl::map<int, std::string> a{{1, "one"}};
  chaistl::map<int, std::string> b;

  a.merge(b);

  EXPECT_THAT(a, ElementsAre(Pair(1, "one")));
  EXPECT_THAT(b, IsEmpty());
}

TEST(MapNodeHandle, MergeIntoEmpty) {
  chaistl::map<int, std::string> a;
  chaistl::map<int, std::string> b{{1, "one"}, {2, "two"}};

  a.merge(b);

  EXPECT_THAT(a, ElementsAre(Pair(1, "one"), Pair(2, "two")));
  EXPECT_THAT(b, IsEmpty());
}

}  // namespace
