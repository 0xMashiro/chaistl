// SPDX-License-Identifier: Apache-2.0

// References:
// - std::set node handle / extract / merge:
//   https://en.cppreference.com/w/cpp/container/set/extract
//   https://en.cppreference.com/w/cpp/container/set/merge

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/set.hpp>

#include <functional>
#include <string>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(SetNodeHandle, ExtractByIterator) {
  chaistl::set<int> s{1, 2, 3, 4, 5};

  auto it = s.find(3);
  auto node = s.extract(it);

  EXPECT_TRUE(node);
  EXPECT_EQ(node.value(), 3);
  EXPECT_THAT(s, ElementsAre(1, 2, 4, 5));
  EXPECT_TRUE(s.validate());
}

TEST(SetNodeHandle, ExtractByKey) {
  chaistl::set<int> s{1, 2, 3};

  auto node = s.extract(2);
  EXPECT_TRUE(node);
  EXPECT_EQ(node.value(), 2);

  auto missing = s.extract(99);
  EXPECT_FALSE(missing);

  EXPECT_THAT(s, ElementsAre(1, 3));
  EXPECT_TRUE(s.validate());
}

TEST(SetNodeHandle, InsertNodeHandle) {
  chaistl::set<int> s{1, 2, 3};

  auto node = s.extract(2);
  node.value() = 42;

  auto ret = s.insert(std::move(node));
  EXPECT_TRUE(ret.inserted);
  EXPECT_EQ(*ret.position, 42);
  EXPECT_FALSE(ret.node);
  EXPECT_THAT(s, ElementsAre(1, 3, 42));
  EXPECT_TRUE(s.validate());
}

TEST(SetNodeHandle, InsertDuplicateNodeHandle) {
  chaistl::set<int> s{1, 2, 3};
  chaistl::set<int> other{1, 4};

  // Extract a key from 'other' that already exists in 's'.
  auto node = other.extract(1);
  auto ret = s.insert(std::move(node));
  EXPECT_FALSE(ret.inserted);
  EXPECT_EQ(*ret.position, 1);
  EXPECT_TRUE(ret.node);
  EXPECT_EQ(ret.node.value(), 1);
  EXPECT_TRUE(s.validate());
}

TEST(SetNodeHandle, InsertReturnTypeIsPlainMemberType) {
  chaistl::set<int> s{1};

  chaistl::set<int>::insert_return_type ret = s.insert(s.extract(1));

  EXPECT_TRUE(ret.inserted);
  EXPECT_FALSE(ret.node);
  EXPECT_EQ(*ret.position, 1);
}

TEST(SetNodeHandle, MergeAcceptsDifferentComparator) {
  chaistl::set<int> a{1, 3};
  chaistl::set<int, std::greater<int>> b{2, 3, 4};

  a.merge(b);

  EXPECT_THAT(a, ElementsAre(1, 2, 3, 4));
  EXPECT_THAT(b, ElementsAre(3));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(SetNodeHandle, MergeTransfersNonDuplicates) {
  chaistl::set<int> a{1, 3, 5};
  chaistl::set<int> b{2, 3, 4};

  a.merge(b);

  EXPECT_THAT(a, ElementsAre(1, 2, 3, 4, 5));
  EXPECT_THAT(b, ElementsAre(3));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(SetNodeHandle, MergeLeavesSourceOnDuplicate) {
  chaistl::set<int> a{1, 2, 3};
  chaistl::set<int> b{2, 4};

  a.merge(b);

  EXPECT_THAT(a, ElementsAre(1, 2, 3, 4));
  EXPECT_THAT(b, ElementsAre(2));
}

TEST(SetNodeHandle, MergeEmptySource) {
  chaistl::set<int> a{1, 2};
  chaistl::set<int> b;

  a.merge(b);

  EXPECT_THAT(a, ElementsAre(1, 2));
  EXPECT_THAT(b, IsEmpty());
}

TEST(SetNodeHandle, MergeIntoEmpty) {
  chaistl::set<int> a;
  chaistl::set<int> b{1, 2, 3};

  a.merge(b);

  EXPECT_THAT(a, ElementsAre(1, 2, 3));
  EXPECT_THAT(b, IsEmpty());
}

}  // namespace
