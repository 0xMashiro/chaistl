// SPDX-License-Identifier: Apache-2.0

// References:
// - Node handles: https://eel.is/c++draft/container.node
// - std::multimap::extract / insert(node_type&&):
//   https://en.cppreference.com/w/cpp/container/multimap/extract

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multimap.hpp>

#include <string>
#include <utility>

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(MultimapNodeHandle, ExtractExposesKeyAndMapped) {
  chaistl::multimap<int, std::string> mm{{1, "one"}, {1, "uno"}};

  auto nh = mm.extract(1);
  ASSERT_FALSE(nh.empty());
  EXPECT_EQ(nh.key(), 1);
  EXPECT_EQ(nh.mapped(), "one");  // first of the run
  EXPECT_THAT(mm, ElementsAre(Pair(1, "uno")));
}

TEST(MultimapNodeHandle, InsertNeverRefuses) {
  chaistl::multimap<int, int> mm{{1, 1}, {1, 2}};

  auto nh = mm.extract(mm.begin());
  auto it = mm.insert(std::move(nh));
  ASSERT_NE(it, mm.end());
  EXPECT_THAT(mm, ElementsAre(Pair(1, 2), Pair(1, 1)));  // re-inserted at run end
  EXPECT_TRUE(mm.validate());
}

// [container.node.overview]: the key may be mutated while outside the
// container — the whole point of extract for maps.
TEST(MultimapNodeHandle, RekeyOutsideContainer) {
  chaistl::multimap<int, int> mm{{1, 10}, {5, 50}};

  auto nh = mm.extract(1);
  nh.key() = 5;
  mm.insert(std::move(nh));

  EXPECT_THAT(mm, ElementsAre(Pair(5, 50), Pair(5, 10)));
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapNodeHandle, EmptyHandleAndHint) {
  chaistl::multimap<int, int> mm{{1, 1}};

  chaistl::multimap<int, int>::node_type empty_nh;
  EXPECT_EQ(mm.insert(std::move(empty_nh)), mm.end());

  chaistl::multimap<int, int> donor{{2, 2}};
  auto it = mm.insert(mm.end(), donor.extract(2));
  EXPECT_EQ(it->first, 2);
  EXPECT_THAT(mm, ElementsAre(Pair(1, 1), Pair(2, 2)));
  EXPECT_TRUE(mm.validate());
}

}  // namespace
