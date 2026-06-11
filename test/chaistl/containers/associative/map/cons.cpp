// SPDX-License-Identifier: Apache-2.0

// References:
// - std::map constructors:
//   https://en.cppreference.com/w/cpp/container/map/map
//   https://eel.is/c++draft/map.cons

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/map.hpp>

#include <array>
#include <functional>
#include <string>
#include <utility>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(MapCons, DefaultConstruct) {
  chaistl::map<int, std::string> m;
  EXPECT_THAT(m, IsEmpty());
  EXPECT_EQ(m.size(), 0uz);
}

TEST(MapCons, InitializerList) {
  chaistl::map<int, std::string> m{{1, "one"}, {3, "three"}, {2, "two"}, {1, "duplicate"}};
  EXPECT_EQ(m.size(), 3uz);
  EXPECT_TRUE(m.validate());

  auto it = m.begin();
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, "one");
  ++it;
  EXPECT_EQ(it->first, 2);
  EXPECT_EQ(it->second, "two");
}

TEST(MapCons, IteratorRange) {
  const std::array<std::pair<const int, std::string>, 3> source{{{1, "one"}, {3, "three"}, {2, "two"}}};
  chaistl::map<int, std::string> m(source.begin(), source.end());
  EXPECT_EQ(m.size(), 3uz);
  EXPECT_TRUE(m.validate());
}

TEST(MapCons, CopyConstruct) {
  chaistl::map<int, std::string> original{{1, "one"}, {2, "two"}};
  chaistl::map<int, std::string> copy(original);
  EXPECT_EQ(copy.size(), 2uz);
  EXPECT_EQ(copy[1], "one");
  EXPECT_TRUE(copy.validate());
}

TEST(MapCons, MoveConstruct) {
  chaistl::map<int, std::string> original{{1, "one"}, {2, "two"}};
  chaistl::map<int, std::string> moved(std::move(original));
  EXPECT_EQ(moved.size(), 2uz);
  EXPECT_EQ(moved[1], "one");
  EXPECT_TRUE(moved.validate());
  EXPECT_THAT(original, IsEmpty());
}

TEST(MapCons, CustomComparator) {
  chaistl::map<int, std::string, std::greater<int>> m{{1, "one"}, {2, "two"}, {3, "three"}};

  auto it = m.begin();
  EXPECT_EQ(it->first, 3);
  ++it;
  EXPECT_EQ(it->first, 2);
  ++it;
  EXPECT_EQ(it->first, 1);
  EXPECT_TRUE(m.validate());
}

TEST(MapCons, CompareAndAllocator) {
  std::less<int> comp;
  chaistl::allocator<std::pair<const int, std::string>> alloc;
  chaistl::map<int, std::string> m(comp, alloc);
  m.insert({1, "one"});
  EXPECT_EQ(m.size(), 1uz);
  EXPECT_TRUE(m.validate());
}

TEST(MapCons, AllocatorOnly) {
  chaistl::allocator<std::pair<const int, std::string>> alloc;
  chaistl::map<int, std::string> m(alloc);
  m.insert({1, "one"});
  EXPECT_EQ(m.size(), 1uz);
  EXPECT_TRUE(m.validate());
}

TEST(MapCons, IteratorRangeWithAllocator) {
  const std::array<std::pair<const int, std::string>, 2> source{{{1, "one"}, {2, "two"}}};
  chaistl::allocator<std::pair<const int, std::string>> alloc;
  chaistl::map<int, std::string> m(source.begin(), source.end(), alloc);
  EXPECT_EQ(m.size(), 2uz);
  EXPECT_TRUE(m.validate());
}

TEST(MapCons, InitializerListWithAllocator) {
  chaistl::allocator<std::pair<const int, std::string>> alloc;
  chaistl::map<int, std::string> m({{1, "one"}, {2, "two"}}, alloc);
  EXPECT_EQ(m.size(), 2uz);
  EXPECT_TRUE(m.validate());
}

TEST(MapCons, CopyWithAllocator) {
  chaistl::map<int, std::string> original{{1, "one"}, {2, "two"}};
  chaistl::allocator<std::pair<const int, std::string>> alloc;
  chaistl::map<int, std::string> copy(original, alloc);
  EXPECT_EQ(copy.size(), 2uz);
  EXPECT_EQ(copy[1], "one");
  EXPECT_TRUE(copy.validate());
}

TEST(MapCons, MoveWithAllocator) {
  chaistl::map<int, std::string> original{{1, "one"}, {2, "two"}};
  chaistl::allocator<std::pair<const int, std::string>> alloc;
  chaistl::map<int, std::string> moved(std::move(original), alloc);
  EXPECT_EQ(moved.size(), 2uz);
  EXPECT_EQ(moved[1], "one");
  EXPECT_TRUE(moved.validate());
}

TEST(MapCons, ConstBegin) {
  const chaistl::map<int, std::string> m{{1, "one"}, {2, "two"}};
  auto it = m.begin();
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, "one");
}

TEST(MapCons, FromRange) {
  const std::vector<std::pair<int, std::string>> source{{1, "one"}, {3, "three"}, {2, "two"}, {1, "duplicate"}};
  chaistl::map<int, std::string> m(std::from_range, source);
  EXPECT_EQ(m.size(), 3uz);
  EXPECT_EQ(m[1], "one");
  EXPECT_EQ(m[2], "two");
  EXPECT_EQ(m[3], "three");
  EXPECT_TRUE(m.validate());
}

TEST(MapCons, FromRangeDeductionGuide) {
  const std::vector<std::pair<int, std::string>> source{{1, "one"}, {2, "two"}};
  chaistl::map m(std::from_range, source);
  static_assert(std::same_as<decltype(m), chaistl::map<int, std::string>>);
  EXPECT_EQ(m.size(), 2uz);
}

// Regression: a comparator in the trailing slot used to be deduced into
// the (..., Allocator) guides and hard-error inside allocator_traits.
// [container.reqmts]: those guides must drop out because std::greater
// does not qualify as an allocator.
TEST(MapCons, DeductionGuideComparatorIsNotAnAllocator) {
  const std::array<std::pair<int, int>, 2> source{{{1, 10}, {2, 20}}};

  chaistl::map m1(source.begin(), source.end(), std::greater<int>{});
  static_assert(std::same_as<decltype(m1), chaistl::map<int, int, std::greater<int>>>);
  EXPECT_EQ(m1.begin()->first, 2);

  chaistl::map m2(std::from_range, source, std::greater<int>{});
  static_assert(std::same_as<decltype(m2), chaistl::map<int, int, std::greater<int>>>);
  EXPECT_EQ(m2.begin()->first, 2);

  chaistl::map m3({std::pair{4, 40}, std::pair{5, 50}}, std::greater<int>{});
  static_assert(std::same_as<decltype(m3), chaistl::map<int, int, std::greater<int>>>);
  EXPECT_EQ(m3.begin()->first, 5);
}

}  // namespace
