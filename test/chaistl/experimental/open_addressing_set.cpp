// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/experimental/containers/open_addressing_set.hpp>

#include <cstddef>
#include <memory>
#include <type_traits>

namespace {

using ::testing::UnorderedElementsAre;

struct ConstantHash {
  std::size_t operator()(int) const noexcept { return 0; }
};

struct ShiftedHash {
  std::size_t operator()(int value) const noexcept { return static_cast<std::size_t>(value) << 8u; }
};

template <class Set>
void exercise_collision_heavy_set() {
  Set set;
  set.reserve(64);

  for (int value = 0; value < 32; ++value) {
    auto [it, inserted] = set.insert(value);
    ASSERT_TRUE(inserted);
    EXPECT_EQ(*it, value);
  }

  for (int value = 0; value < 32; ++value) {
    EXPECT_TRUE(set.contains(value));
  }
  EXPECT_TRUE(set.validate());
}

TEST(OpenAddressingSet, InsertFindAndEraseUniqueKeys) {
  chaistl::experimental::open_addressing_set<int> set;

  EXPECT_TRUE(set.empty());
  auto [first, inserted_first] = set.insert(10);
  EXPECT_TRUE(inserted_first);
  EXPECT_EQ(*first, 10);

  auto [duplicate, inserted_duplicate] = set.insert(10);
  EXPECT_FALSE(inserted_duplicate);
  EXPECT_EQ(*duplicate, 10);

  set.emplace(20);
  EXPECT_EQ(set.size(), 2u);
  EXPECT_TRUE(set.contains(10));
  EXPECT_EQ(set.count(20), 1u);

  EXPECT_EQ(set.erase(10), 1u);
  EXPECT_EQ(set.erase(10), 0u);
  EXPECT_FALSE(set.contains(10));
  EXPECT_TRUE(set.contains(20));
  EXPECT_TRUE(set.validate());
}

TEST(OpenAddressingSet, SupportsClassicProbingPolicies) {
  exercise_collision_heavy_set<chaistl::experimental::linear_open_addressing_set<int, ConstantHash>>();
  exercise_collision_heavy_set<chaistl::experimental::quadratic_open_addressing_set<int, ConstantHash>>();
  exercise_collision_heavy_set<chaistl::experimental::double_hashing_open_addressing_set<int, ConstantHash>>();
}

TEST(OpenAddressingSet, TombstonesKeepProbeChainsSearchable) {
  using Set = chaistl::experimental::open_addressing_set<int, ConstantHash>;
  Set set;

  ASSERT_TRUE(set.insert(1).second);
  ASSERT_TRUE(set.insert(9).second);
  ASSERT_TRUE(set.insert(17).second);
  EXPECT_TRUE(set.validate());

  EXPECT_EQ(set.erase(9), 1u);
  EXPECT_TRUE(set.contains(17));
  EXPECT_FALSE(set.contains(9));

  ASSERT_TRUE(set.insert(25).second);
  EXPECT_TRUE(set.contains(1));
  EXPECT_TRUE(set.contains(17));
  EXPECT_TRUE(set.contains(25));
  EXPECT_TRUE(set.validate());
}

TEST(OpenAddressingSet, ReserveAndRehashKeepPowerOfTwoBuckets) {
  chaistl::experimental::open_addressing_set<int> set;

  set.reserve(100);
  EXPECT_TRUE(std::has_single_bit(set.bucket_count()));
  EXPECT_GE(set.bucket_count(), 100u);

  for (int value = 0; value < 1000; ++value) {
    set.insert(value);
    ASSERT_TRUE(std::has_single_bit(set.bucket_count()));
    ASSERT_LE(set.load_factor(), set.max_load_factor());
  }

  const auto bucket_count = set.bucket_count();
  set.rehash(32);
  EXPECT_GE(set.bucket_count(), bucket_count);
  EXPECT_TRUE(set.validate());
}

TEST(OpenAddressingSet, IterationVisitsOccupiedElementsOnly) {
  chaistl::experimental::open_addressing_set<int> set{1, 2, 3, 4};

  set.erase(2);
  set.insert(5);

  EXPECT_THAT(set, UnorderedElementsAre(1, 3, 4, 5));
  EXPECT_TRUE(set.validate());
}

TEST(OpenAddressingSet, DoubleHashingUsesHighBitsForClusteredLowBits) {
  using Set = chaistl::experimental::
      open_addressing_set<int, ShiftedHash, std::equal_to<int>, chaistl::experimental::double_hashing>;
  Set set;

  for (int value = 0; value < 200; ++value) {
    ASSERT_TRUE(set.insert(value).second);
  }

  for (int value = 0; value < 200; ++value) {
    EXPECT_TRUE(set.contains(value));
  }
  EXPECT_TRUE(set.validate());
}

TEST(OpenAddressingSet, SupportsMoveOnlyValues) {
  using Set = chaistl::experimental::open_addressing_set<std::unique_ptr<int>>;
  static_assert(!std::is_copy_constructible_v<Set::value_type>);

  Set set;
  auto [it, inserted] = set.insert(std::make_unique<int>(42));

  ASSERT_TRUE(inserted);
  EXPECT_EQ(**it, 42);
  EXPECT_TRUE(set.validate());
}

}  // namespace
