// SPDX-License-Identifier: Apache-2.0

// References:
// - std::set constructors:
//   https://en.cppreference.com/w/cpp/container/set/set
//   https://eel.is/c++draft/set.cons

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/set.hpp>

#include <array>
#include <functional>
#include <iterator>
#include <sstream>
#include <string>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(SetCons, DefaultConstruct) {
  chaistl::set<int> s;
  EXPECT_THAT(s, IsEmpty());
  EXPECT_EQ(s.size(), 0uz);
}

TEST(SetCons, InitializerList) {
  chaistl::set<int> s{3, 1, 4, 1, 5};
  // Duplicates removed, sorted order
  EXPECT_THAT(s, ElementsAre(1, 3, 4, 5));
  EXPECT_EQ(s.size(), 4uz);
  EXPECT_TRUE(s.validate());
}

TEST(SetCons, IteratorRange) {
  const std::array source{3, 1, 4, 1, 5};
  chaistl::set<int> s(source.begin(), source.end());
  EXPECT_THAT(s, ElementsAre(1, 3, 4, 5));
  EXPECT_TRUE(s.validate());
}

TEST(SetCons, InputIterator) {
  std::istringstream input("3 1 4 1 5");
  std::istream_iterator<int> first(input);
  std::istream_iterator<int> last;
  chaistl::set<int> s(first, last);
  EXPECT_THAT(s, ElementsAre(1, 3, 4, 5));
  EXPECT_TRUE(s.validate());
}

TEST(SetCons, CopyConstruct) {
  chaistl::set<int> original{1, 2, 3};
  chaistl::set<int> copy(original);
  EXPECT_THAT(copy, ElementsAre(1, 2, 3));
  EXPECT_TRUE(copy.validate());
}

TEST(SetCons, MoveConstruct) {
  chaistl::set<int> original{1, 2, 3};
  chaistl::set<int> moved(std::move(original));
  EXPECT_THAT(moved, ElementsAre(1, 2, 3));
  EXPECT_TRUE(moved.validate());
  EXPECT_THAT(original, IsEmpty());
}

TEST(SetCons, CustomComparator) {
  chaistl::set<int, std::greater<int>> s{1, 2, 3};
  EXPECT_THAT(s, ElementsAre(3, 2, 1));
  EXPECT_TRUE(s.validate());
}

TEST(SetCons, StringElements) {
  chaistl::set<std::string> s{"hello", "world", "abc"};
  EXPECT_THAT(s, ElementsAre("abc", "hello", "world"));
  EXPECT_TRUE(s.validate());
}

TEST(SetCons, FromRange) {
  const std::vector<int> source{3, 1, 4, 1, 5};
  chaistl::set<int> s(std::from_range, source);
  EXPECT_THAT(s, ElementsAre(1, 3, 4, 5));
  EXPECT_EQ(s.size(), 4uz);
  EXPECT_TRUE(s.validate());
}

TEST(SetCons, FromRangeWithComparatorAndAllocator) {
  const std::vector<int> source{1, 2, 3};
  chaistl::set<int, std::greater<int>> s(std::from_range, source);
  EXPECT_THAT(s, ElementsAre(3, 2, 1));
  EXPECT_TRUE(s.validate());
}

TEST(SetCons, FromRangeDeductionGuide) {
  const std::vector<int> source{3, 1, 4};
  chaistl::set s(std::from_range, source);
  static_assert(std::same_as<decltype(s), chaistl::set<int>>);
  EXPECT_THAT(s, ElementsAre(1, 3, 4));
}

// Regression: a comparator in the trailing slot used to be deduced into
// the (..., Allocator) guides and hard-error inside allocator_traits.
// [container.reqmts]: those guides must drop out because std::greater
// does not qualify as an allocator.
TEST(SetCons, DeductionGuideComparatorIsNotAnAllocator) {
  const std::array<int, 2> source{1, 2};

  chaistl::set s1(source.begin(), source.end(), std::greater<int>{});
  static_assert(std::same_as<decltype(s1), chaistl::set<int, std::greater<int>>>);
  EXPECT_THAT(s1, ElementsAre(2, 1));

  chaistl::set s2(std::from_range, source, std::greater<int>{});
  static_assert(std::same_as<decltype(s2), chaistl::set<int, std::greater<int>>>);
  EXPECT_THAT(s2, ElementsAre(2, 1));

  chaistl::set s3({4, 5}, std::greater<int>{});
  static_assert(std::same_as<decltype(s3), chaistl::set<int, std::greater<int>>>);
  EXPECT_THAT(s3, ElementsAre(5, 4));
}

}  // namespace
