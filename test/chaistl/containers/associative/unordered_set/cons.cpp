// SPDX-License-Identifier: Apache-2.0

// References:
// - std::unordered_set constructors:
//   https://en.cppreference.com/w/cpp/container/unordered_set/unordered_set
//   https://eel.is/c++draft/unord.set.cnstr

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/functional/hash.hpp>

#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using ::testing::UnorderedElementsAre;

TEST(UnorderedSetCons, DefaultConstructedIsEmptyWithZeroBuckets) {
  chaistl::unordered_set<int> set;

  EXPECT_TRUE(set.empty());
  EXPECT_EQ(set.size(), 0u);
  // Implementation-defined choice, documented in the ADR: the default
  // constructor allocates nothing, so the bucket count starts at zero.
  EXPECT_EQ(set.bucket_count(), 0u);
  EXPECT_EQ(set.begin(), set.end());
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetCons, BucketCountConstructor) {
  chaistl::unordered_set<int> set(50);

  EXPECT_TRUE(set.empty());
  EXPECT_GE(set.bucket_count(), 50u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetCons, AllocatorConstructor) {
  chaistl::unordered_set<int> set(chaistl::allocator<int>{});

  EXPECT_TRUE(set.empty());
  EXPECT_EQ(set.bucket_count(), 0u);
}

TEST(UnorderedSetCons, IteratorRangeDeduplicates) {
  const std::vector<int> values{1, 2, 3, 2, 1};
  chaistl::unordered_set<int> set(values.begin(), values.end());

  EXPECT_THAT(set, UnorderedElementsAre(1, 2, 3));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetCons, InitializerListDeduplicates) {
  chaistl::unordered_set<int> set{4, 4, 5, 6, 5};

  EXPECT_THAT(set, UnorderedElementsAre(4, 5, 6));
}

TEST(UnorderedSetCons, IteratorRangeWithBucketCount) {
  const std::vector<int> values{1, 2, 3};
  chaistl::unordered_set<int> set(values.begin(), values.end(), 100);

  EXPECT_GE(set.bucket_count(), 100u);
  EXPECT_THAT(set, UnorderedElementsAre(1, 2, 3));
}

TEST(UnorderedSetCons, CopyConstructor) {
  chaistl::unordered_set<int> original{1, 2, 3};
  chaistl::unordered_set<int> copy(original);

  EXPECT_EQ(copy, original);
  // Copies are independent.
  copy.insert(4);
  EXPECT_TRUE(copy.contains(4));
  EXPECT_FALSE(original.contains(4));
  EXPECT_TRUE(copy.validate());
  EXPECT_TRUE(original.validate());
}

TEST(UnorderedSetCons, CopyConstructorWithAllocator) {
  chaistl::unordered_set<int> original{1, 2, 3};
  chaistl::unordered_set<int> copy(original, chaistl::allocator<int>{});

  EXPECT_EQ(copy, original);
}

TEST(UnorderedSetCons, MoveConstructorStealsStorage) {
  chaistl::unordered_set<int> source{1, 2, 3};
  const int* stable = &*source.find(2);

  chaistl::unordered_set<int> target(std::move(source));

  EXPECT_THAT(target, UnorderedElementsAre(1, 2, 3));
  // Node-based moves steal the nodes: element addresses survive.
  EXPECT_EQ(stable, &*target.find(2));
  // This implementation leaves the source empty (valid, unspecified per the
  // standard; pinned here as a regression guard).
  EXPECT_TRUE(source.empty());
  EXPECT_TRUE(source.validate());
  EXPECT_TRUE(target.validate());
}

TEST(UnorderedSetCons, MoveConstructorWithEqualAllocatorSteals) {
  chaistl::unordered_set<int> source{1, 2, 3};
  const int* stable = &*source.find(2);

  chaistl::unordered_set<int> target(std::move(source), chaistl::allocator<int>{});

  EXPECT_THAT(target, UnorderedElementsAre(1, 2, 3));
  EXPECT_EQ(stable, &*target.find(2));
}

TEST(UnorderedSetCons, CopyAssignment) {
  chaistl::unordered_set<int> source{1, 2, 3};
  chaistl::unordered_set<int> target{9};

  target = source;

  EXPECT_EQ(target, source);
  EXPECT_FALSE(target.contains(9));
  EXPECT_TRUE(target.validate());
}

TEST(UnorderedSetCons, MoveAssignment) {
  chaistl::unordered_set<int> source{1, 2, 3};
  chaistl::unordered_set<int> target{9};

  target = std::move(source);

  EXPECT_THAT(target, UnorderedElementsAre(1, 2, 3));
  EXPECT_TRUE(target.validate());
}

TEST(UnorderedSetCons, InitializerListAssignment) {
  chaistl::unordered_set<int> set{9, 10};

  set = {1, 2, 2, 3};

  EXPECT_THAT(set, UnorderedElementsAre(1, 2, 3));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetCons, SelfCopyAssignmentIsHarmless) {
  chaistl::unordered_set<int> set{1, 2, 3};
  const auto& alias = set;

  set = alias;

  EXPECT_THAT(set, UnorderedElementsAre(1, 2, 3));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetCons, FromRangeConstructors) {
  const std::vector<int> values{1, 2, 3, 2};

  chaistl::unordered_set<int> defaulted(std::from_range, values);
  EXPECT_THAT(defaulted, UnorderedElementsAre(1, 2, 3));
  EXPECT_TRUE(defaulted.validate());

  chaistl::unordered_set<int> with_buckets(std::from_range, values, 100);
  EXPECT_GE(with_buckets.bucket_count(), 100u);
  EXPECT_THAT(with_buckets, UnorderedElementsAre(1, 2, 3));

  // A transformed view produces convertible-but-not-value_type elements.
  chaistl::unordered_set<int> from_view(std::from_range, values | std::views::transform([](int v) {
                                                           return v * 10;
                                                         }));
  EXPECT_THAT(from_view, UnorderedElementsAre(10, 20, 30));
}

TEST(UnorderedSetCons, DeductionGuides) {
  const std::vector<int> values{1, 2, 3};

  chaistl::unordered_set s1(values.begin(), values.end());
  static_assert(std::is_same_v<decltype(s1), chaistl::unordered_set<int>>);

  chaistl::unordered_set s2{1, 2, 3};
  static_assert(std::is_same_v<decltype(s2), chaistl::unordered_set<int>>);

  // The integral third argument is a bucket count and must never deduce as
  // the Hash — the is_integral guide gate at work.
  chaistl::unordered_set s3(values.begin(), values.end(), 64);
  static_assert(std::is_same_v<decltype(s3), chaistl::unordered_set<int>>);
  EXPECT_GE(s3.bucket_count(), 64u);

  chaistl::unordered_set s4(values.begin(), values.end(), 0, chaistl::hash<int>{});
  static_assert(std::is_same_v<decltype(s4), chaistl::unordered_set<int, chaistl::hash<int>>>);

  chaistl::unordered_set s5({1, 2, 3}, 0, chaistl::hash<int>{}, chaistl::allocator<int>{});
  static_assert(
      std::is_same_v<decltype(s5),
                     chaistl::unordered_set<int, chaistl::hash<int>, std::equal_to<int>, chaistl::allocator<int>>>);

  chaistl::unordered_set s6(std::from_range, values);
  static_assert(std::is_same_v<decltype(s6), chaistl::unordered_set<int>>);

  chaistl::unordered_set s7(std::from_range, values, 8, chaistl::hash<int>{});
  static_assert(std::is_same_v<decltype(s7), chaistl::unordered_set<int, chaistl::hash<int>>>);

  EXPECT_THAT(s6, UnorderedElementsAre(1, 2, 3));
}

}  // namespace
