// SPDX-License-Identifier: Apache-2.0

// The power-of-two policy is an opt-in ChaiSTL extension: bucket indexing is
// mask-based, so it is fast when the hasher has useful low bits and fragile
// when it does not.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/power2_unordered_map.hpp>
#include <chaistl/containers/power2_unordered_multimap.hpp>
#include <chaistl/containers/power2_unordered_multiset.hpp>
#include <chaistl/containers/power2_unordered_set.hpp>
#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/containers/unordered_set.hpp>

#include <cstddef>
#include <utility>

namespace {

using ::testing::UnorderedElementsAre;

constexpr bool is_power_of_two(std::size_t value) noexcept {
  return value != 0 && (value & (value - 1)) == 0;
}

struct ShiftedHash {
  std::size_t operator()(int value) const noexcept { return static_cast<std::size_t>(value) << 8u; }
};

TEST(UnorderedSetPower2Policy, RehashAndReserveChoosePowerOfTwoBucketCounts) {
  chaistl::power2_unordered_set<int> set;

  set.reserve(100);
  EXPECT_TRUE(is_power_of_two(set.bucket_count()));
  EXPECT_GE(set.bucket_count(), 100u);

  set.rehash(300);
  EXPECT_TRUE(is_power_of_two(set.bucket_count()));
  EXPECT_GE(set.bucket_count(), 300u);

  for (int value = 0; value < 1000; ++value) {
    set.insert(value);
    ASSERT_TRUE(is_power_of_two(set.bucket_count()));
    ASSERT_LE(set.load_factor(), set.max_load_factor());
  }
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetPower2Policy, BucketUsesMaskOfHashCode) {
  chaistl::power2_unordered_set<int, ShiftedHash> set;
  set.rehash(1024);

  const std::size_t buckets = set.bucket_count();
  ASSERT_TRUE(is_power_of_two(buckets));

  for (int value : {1, 2, 17, 255, 1024}) {
    EXPECT_EQ(set.bucket(value), ShiftedHash{}(value) & (buckets - 1));
  }
}

TEST(UnorderedSetPower2Policy, MergeWorksAcrossPrimeAndPower2Policies) {
  chaistl::unordered_set<int> prime{1, 2, 3};
  chaistl::power2_unordered_set<int> power2{3, 4, 5};

  prime.merge(power2);

  EXPECT_THAT(prime, UnorderedElementsAre(1, 2, 3, 4, 5));
  EXPECT_THAT(power2, UnorderedElementsAre(3));
  EXPECT_TRUE(prime.validate());
  EXPECT_TRUE(power2.validate());

  power2.merge(prime);

  EXPECT_THAT(power2, UnorderedElementsAre(1, 2, 3, 4, 5));
  EXPECT_THAT(prime, UnorderedElementsAre(3));
  EXPECT_TRUE(prime.validate());
  EXPECT_TRUE(power2.validate());
}

TEST(UnorderedSetPower2Policy, MapAndMultiAliasesUseTheSamePowerOfTwoPolicy) {
  chaistl::power2_unordered_map<int, int> map;
  chaistl::power2_unordered_multiset<int> multiset;
  chaistl::power2_unordered_multimap<int, int> multimap;

  map.reserve(64);
  multiset.reserve(64);
  multimap.reserve(64);

  EXPECT_TRUE(is_power_of_two(map.bucket_count()));
  EXPECT_TRUE(is_power_of_two(multiset.bucket_count()));
  EXPECT_TRUE(is_power_of_two(multimap.bucket_count()));

  map.insert({1, 10});
  map.insert({2, 20});
  multiset.insert(1);
  multiset.insert(1);
  multimap.insert({1, 10});
  multimap.insert({1, 20});

  EXPECT_EQ(map.at(2), 20);
  EXPECT_EQ(multiset.count(1), 2u);
  EXPECT_EQ(multimap.count(1), 2u);
  EXPECT_TRUE(map.validate());
  EXPECT_TRUE(multiset.validate());
  EXPECT_TRUE(multimap.validate());
}

TEST(UnorderedSetPower2Policy, MapMergeWorksAcrossPrimeAndPower2Policies) {
  chaistl::unordered_map<int, int> prime{{1, 10}, {2, 20}};
  chaistl::power2_unordered_map<int, int> power2{{2, 200}, {3, 30}};

  prime.merge(power2);

  EXPECT_THAT(prime, UnorderedElementsAre(std::pair<const int, int>{1, 10},
                                          std::pair<const int, int>{2, 20},
                                          std::pair<const int, int>{3, 30}));
  EXPECT_THAT(power2, UnorderedElementsAre(std::pair<const int, int>{2, 200}));
  EXPECT_TRUE(prime.validate());
  EXPECT_TRUE(power2.validate());
}

}  // namespace
