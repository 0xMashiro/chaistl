// SPDX-License-Identifier: Apache-2.0

// References:
// - Node handles: https://eel.is/c++draft/container.node
// - std::multiset::extract / insert(node_type&&):
//   https://en.cppreference.com/w/cpp/container/multiset/extract

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multiset.hpp>

#include <utility>

namespace {

using ::testing::ElementsAre;

TEST(MultisetNodeHandle, ExtractKeyTakesOneOfRun) {
  chaistl::multiset<int> ms{1, 2, 2, 2};

  auto nh = ms.extract(2);
  ASSERT_FALSE(nh.empty());
  EXPECT_EQ(nh.value(), 2);
  // Only one element of the run leaves the container.
  EXPECT_THAT(ms, ElementsAre(1, 2, 2));
  EXPECT_TRUE(ms.validate());

  auto none = ms.extract(42);
  EXPECT_TRUE(none.empty());
}

TEST(MultisetNodeHandle, InsertNeverRefuses) {
  chaistl::multiset<int> ms{2, 2};

  auto nh = ms.extract(2);
  auto it = ms.insert(std::move(nh));  // re-insert an equivalent key
  ASSERT_NE(it, ms.end());
  EXPECT_EQ(*it, 2);
  EXPECT_THAT(ms, ElementsAre(2, 2));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetNodeHandle, InsertEmptyHandle) {
  chaistl::multiset<int> ms{1};
  chaistl::multiset<int>::node_type nh;
  auto it = ms.insert(std::move(nh));
  EXPECT_EQ(it, ms.end());
  EXPECT_THAT(ms, ElementsAre(1));
}

TEST(MultisetNodeHandle, HintInsert) {
  chaistl::multiset<int> ms{1, 3};
  chaistl::multiset<int> donor{2};

  auto nh = donor.extract(2);
  auto it = ms.insert(ms.find(3), std::move(nh));
  EXPECT_EQ(*it, 2);
  EXPECT_THAT(ms, ElementsAre(1, 2, 3));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetNodeHandle, TransferBetweenContainers) {
  chaistl::multiset<int> a{1, 1};
  chaistl::multiset<int> b;

  b.insert(a.extract(a.begin()));
  b.insert(a.extract(a.begin()));

  EXPECT_TRUE(a.empty());
  EXPECT_THAT(b, ElementsAre(1, 1));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

// [container.node.overview]: the value may be mutated while outside the
// container, then reinserted under its new ordering position.
TEST(MultisetNodeHandle, MutateValueOutsideContainer) {
  chaistl::multiset<int> ms{1, 5, 5};

  auto nh = ms.extract(1);
  nh.value() = 5;
  ms.insert(std::move(nh));

  EXPECT_THAT(ms, ElementsAre(5, 5, 5));
  EXPECT_TRUE(ms.validate());
}

}  // namespace
