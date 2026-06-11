// SPDX-License-Identifier: Apache-2.0

// References:
// - std::set requirements and semantics:
//   https://en.cppreference.com/w/cpp/container/set
//   https://eel.is/c++draft/set
//
// These typed tests intentionally run the same behavior against std::set
// and chaistl::set. They are compatibility probes.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/set.hpp>

#include <algorithm>
#include <set>
#include <type_traits>
#include <utility>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

template <class Set>
class SetInteropTest : public ::testing::Test {};

using CompatibleIntSets = ::testing::Types<std::set<int>, chaistl::set<int>>;
TYPED_TEST_SUITE(SetInteropTest, CompatibleIntSets);

TYPED_TEST(SetInteropTest, DefaultConstruction) {
  TypeParam s;
  EXPECT_THAT(s, IsEmpty());
  EXPECT_EQ(s.size(), 0uz);
}

TYPED_TEST(SetInteropTest, InsertAndIterate) {
  TypeParam s;
  s.insert(3);
  s.insert(1);
  s.insert(4);
  s.insert(1);  // duplicate

  EXPECT_EQ(s.size(), 3uz);
  EXPECT_TRUE(s.contains(1));
  EXPECT_TRUE(s.contains(3));
  EXPECT_TRUE(s.contains(4));
  EXPECT_FALSE(s.contains(2));

  std::vector<int> values;
  for (auto& v : s) {
    values.push_back(v);
  }
  EXPECT_THAT(values, ElementsAre(1, 3, 4));
}

TYPED_TEST(SetInteropTest, EraseAndFind) {
  TypeParam s{1, 2, 3, 4, 5};

  s.erase(3);
  EXPECT_FALSE(s.contains(3));
  EXPECT_EQ(s.size(), 4uz);

  auto it = s.find(4);
  EXPECT_NE(it, s.end());
  s.erase(it);
  EXPECT_FALSE(s.contains(4));

  EXPECT_EQ(s.find(99), s.end());
}

TYPED_TEST(SetInteropTest, LowerUpperBound) {
  TypeParam s{1, 3, 5, 7, 9};

  EXPECT_EQ(*s.lower_bound(3), 3);
  EXPECT_EQ(*s.upper_bound(3), 5);
  EXPECT_EQ(s.lower_bound(10), s.end());
  EXPECT_EQ(s.upper_bound(10), s.end());
}

TYPED_TEST(SetInteropTest, CopyAndMove) {
  TypeParam original{1, 2, 3};

  TypeParam copy(original);
  EXPECT_EQ(copy.size(), 3uz);
  EXPECT_TRUE(copy.contains(2));

  TypeParam moved(std::move(original));
  EXPECT_EQ(moved.size(), 3uz);
  EXPECT_TRUE(moved.contains(2));
}

TYPED_TEST(SetInteropTest, ClearAndReinsert) {
  TypeParam s{1, 2, 3};
  s.clear();
  EXPECT_THAT(s, IsEmpty());

  s.insert(42);
  EXPECT_EQ(s.size(), 1uz);
  EXPECT_TRUE(s.contains(42));
}

struct LessOnly {
  int v;
  bool operator<(const LessOnly& other) const { return v < other.v; }
  bool operator==(const LessOnly& other) const { return v == other.v; }
};

TEST(SetComparisonInterop, OrderingWorksForLessOnlyElementTypes) {
  // Container operator<=> must fall back to synth-three-way when the element
  // type only provides operator< — exactly as std::set does.
  chaistl::set<LessOnly> a{LessOnly{1}, LessOnly{2}};
  chaistl::set<LessOnly> b{LessOnly{1}, LessOnly{3}};
  std::set<LessOnly> sa{LessOnly{1}, LessOnly{2}};
  std::set<LessOnly> sb{LessOnly{1}, LessOnly{3}};

  EXPECT_TRUE(a < b);
  EXPECT_EQ(a < b, sa < sb);
  EXPECT_EQ(b < a, sb < sa);
  EXPECT_EQ(a == b, sa == sb);
}

}  // namespace
