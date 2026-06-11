// SPDX-License-Identifier: Apache-2.0

// References:
// - std::multiset constructors:
//   https://en.cppreference.com/w/cpp/container/multiset/multiset
//   https://eel.is/c++draft/multiset.cons

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multiset.hpp>

#include <functional>
#include <ranges>
#include <vector>

#include "../../sequence/support.hpp"

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(MultisetCons, Default) {
  chaistl::multiset<int> ms;
  EXPECT_TRUE(ms.empty());
  EXPECT_EQ(ms.size(), 0uz);
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetCons, InitializerListKeepsDuplicates) {
  chaistl::multiset<int> ms{3, 1, 3, 2, 3, 1};
  EXPECT_EQ(ms.size(), 6uz);
  EXPECT_THAT(ms, ElementsAre(1, 1, 2, 3, 3, 3));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetCons, IteratorRangeKeepsDuplicates) {
  const std::vector<int> v{5, 2, 5, 5, 1};
  chaistl::multiset<int> ms(v.begin(), v.end());
  EXPECT_THAT(ms, ElementsAre(1, 2, 5, 5, 5));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetCons, FromRange) {
  chaistl::multiset<int> ms(std::from_range, std::views::iota(0, 3));
  EXPECT_THAT(ms, ElementsAre(0, 1, 2));

  const std::vector<int> dup{1, 1, 1};
  chaistl::multiset<int> ms2(std::from_range, dup);
  EXPECT_THAT(ms2, ElementsAre(1, 1, 1));
  EXPECT_TRUE(ms2.validate());
}

TEST(MultisetCons, CopyPreservesDuplicates) {
  chaistl::multiset<int> src{1, 1, 2};
  chaistl::multiset<int> dst(src);
  EXPECT_THAT(dst, ElementsAre(1, 1, 2));
  EXPECT_THAT(src, ElementsAre(1, 1, 2));
  EXPECT_TRUE(dst.validate());
}

TEST(MultisetCons, MoveTransfersDuplicates) {
  chaistl::multiset<int> src{1, 1, 2};
  chaistl::multiset<int> dst(std::move(src));
  EXPECT_THAT(dst, ElementsAre(1, 1, 2));
  EXPECT_THAT(src, IsEmpty());
  EXPECT_TRUE(dst.validate());
  EXPECT_TRUE(src.validate());
}

TEST(MultisetCons, MoveConstructWithDifferentAllocatorKeepsDuplicates) {
  using Alloc = chaistl::test::TaggedAllocator<int>;
  using Multiset = chaistl::multiset<int, std::less<int>, Alloc>;

  Multiset src(Alloc{1});
  src.insert(1);
  src.insert(1);
  src.insert(2);

  Multiset dst(std::move(src), Alloc{2});

  EXPECT_THAT(dst, ElementsAre(1, 1, 2));
  EXPECT_EQ(dst.size(), 3uz);
  EXPECT_TRUE(dst.validate());
}

TEST(MultisetCons, MoveAssignWithDifferentAllocatorKeepsDuplicates) {
  using Alloc = chaistl::test::TaggedAllocator<int>;
  using Multiset = chaistl::multiset<int, std::less<int>, Alloc>;

  Multiset src(Alloc{1});
  src.insert(1);
  src.insert(1);
  src.insert(2);
  Multiset dst(Alloc{2});
  dst.insert(9);

  dst = std::move(src);

  EXPECT_THAT(dst, ElementsAre(1, 1, 2));
  EXPECT_EQ(dst.size(), 3uz);
  EXPECT_TRUE(dst.validate());
}

TEST(MultisetCons, AssignInitializerListUsesEqualFamily) {
  chaistl::multiset<int> ms{9, 9};
  ms = {4, 4, 4};
  EXPECT_THAT(ms, ElementsAre(4, 4, 4));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetCons, ComparatorConstructor) {
  chaistl::multiset<int, std::greater<int>> ms(std::greater<int>{});
  ms.insert(1);
  ms.insert(3);
  ms.insert(1);
  EXPECT_THAT(ms, ElementsAre(3, 1, 1));
}

TEST(MultisetCons, DeductionGuides) {
  const std::vector<int> v{2, 1, 2};

  chaistl::multiset ms1(v.begin(), v.end());
  static_assert(std::same_as<decltype(ms1), chaistl::multiset<int>>);
  EXPECT_THAT(ms1, ElementsAre(1, 2, 2));

  chaistl::multiset ms2{7, 7, 7};
  static_assert(std::same_as<decltype(ms2), chaistl::multiset<int>>);
  EXPECT_EQ(ms2.count(7), 3uz);

  chaistl::multiset ms3(std::from_range, v);
  static_assert(std::same_as<decltype(ms3), chaistl::multiset<int>>);
  EXPECT_THAT(ms3, ElementsAre(1, 2, 2));

  chaistl::multiset ms4(v.begin(), v.end(), std::greater<int>{});
  static_assert(std::same_as<decltype(ms4), chaistl::multiset<int, std::greater<int>>>);
  EXPECT_THAT(ms4, ElementsAre(2, 2, 1));
}

}  // namespace
