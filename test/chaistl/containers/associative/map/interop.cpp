// SPDX-License-Identifier: Apache-2.0

// References:
// - std::map requirements and semantics:
//   https://en.cppreference.com/w/cpp/container/map
//   https://eel.is/c++draft/map
//
// These typed tests intentionally run the same behavior against std::map
// and chaistl::map. They are compatibility probes.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/map.hpp>

#include <map>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

template <class Map>
class MapInteropTest : public ::testing::Test {};

using CompatibleIntStrMaps = ::testing::Types<std::map<int, std::string>, chaistl::map<int, std::string>>;
TYPED_TEST_SUITE(MapInteropTest, CompatibleIntStrMaps);

TYPED_TEST(MapInteropTest, DefaultConstruction) {
  TypeParam m;
  EXPECT_THAT(m, IsEmpty());
  EXPECT_EQ(m.size(), 0uz);
}

TYPED_TEST(MapInteropTest, InsertAndIterate) {
  TypeParam m;
  m.insert({1, "one"});
  m.insert({3, "three"});
  m.insert({2, "two"});
  m.insert({1, "duplicate"});  // duplicate key

  EXPECT_EQ(m.size(), 3uz);
  EXPECT_TRUE(m.contains(1));
  EXPECT_TRUE(m.contains(2));
  EXPECT_TRUE(m.contains(3));

  std::vector<int> keys;
  for (auto& [k, v] : m) {
    keys.push_back(k);
  }
  EXPECT_THAT(keys, ElementsAre(1, 2, 3));
}

TYPED_TEST(MapInteropTest, SubscriptAccess) {
  TypeParam m;
  m[1] = "one";
  m[2] = "two";

  EXPECT_EQ(m[1], "one");
  EXPECT_EQ(m[2], "two");
  EXPECT_EQ(m.size(), 2uz);
}

TYPED_TEST(MapInteropTest, EraseAndFind) {
  TypeParam m{{1, "one"}, {2, "two"}, {3, "three"}};

  m.erase(2);
  EXPECT_FALSE(m.contains(2));
  EXPECT_EQ(m.size(), 2uz);

  auto it = m.find(3);
  EXPECT_NE(it, m.end());
  m.erase(it);
  EXPECT_FALSE(m.contains(3));
}

TYPED_TEST(MapInteropTest, LowerUpperBound) {
  TypeParam m{{1, "one"}, {3, "three"}, {5, "five"}};

  EXPECT_EQ(m.lower_bound(3)->first, 3);
  EXPECT_EQ(m.upper_bound(3)->first, 5);
}

TYPED_TEST(MapInteropTest, CopyAndMove) {
  TypeParam original{{1, "one"}, {2, "two"}};

  TypeParam copy(original);
  EXPECT_EQ(copy.size(), 2uz);
  EXPECT_EQ(copy[1], "one");

  TypeParam moved(std::move(original));
  EXPECT_EQ(moved.size(), 2uz);
  EXPECT_EQ(moved[1], "one");
}

TYPED_TEST(MapInteropTest, ClearAndReinsert) {
  TypeParam m{{1, "one"}, {2, "two"}};
  m.clear();
  EXPECT_THAT(m, IsEmpty());

  m[42] = "answer";
  EXPECT_EQ(m.size(), 1uz);
  EXPECT_EQ(m[42], "answer");
}

}  // namespace
