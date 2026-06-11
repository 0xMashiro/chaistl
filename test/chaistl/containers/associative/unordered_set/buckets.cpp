// SPDX-License-Identifier: Apache-2.0

// References:
// - Bucket interface and hash policy:
//   https://en.cppreference.com/w/cpp/container/unordered_set
//   https://eel.is/c++draft/unord.req
// - max_load_factor(z) is a hint and does not rehash by itself; insertion
//   keeps (N + n) <= z * B or rehashes ([unord.req.general]).

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>

#include <cstddef>

namespace {

TEST(UnorderedSetBuckets, GrowthKeepsLoadFactorBounded) {
  chaistl::unordered_set<int> set;

  for (int value = 0; value < 1000; ++value) {
    set.insert(value);
    ASSERT_LE(set.load_factor(), set.max_load_factor());
  }

  EXPECT_GT(set.bucket_count(), 0u);
  EXPECT_LE(set.bucket_count(), set.max_bucket_count());
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetBuckets, BucketConsistency) {
  chaistl::unordered_set<int> set;
  for (int value = 0; value < 100; ++value) {
    set.insert(value);
  }

  // Every element lives in exactly the bucket bucket(x) names, local
  // iteration visits only that bucket, and the bucket sizes add up.
  std::size_t total = 0;
  for (std::size_t index = 0; index < set.bucket_count(); ++index) {
    std::size_t chain_length = 0;
    for (auto it = set.begin(index); it != set.end(index); ++it) {
      EXPECT_EQ(set.bucket(*it), index);
      ++chain_length;
    }
    EXPECT_EQ(chain_length, set.bucket_size(index));
    total += chain_length;
  }
  EXPECT_EQ(total, set.size());

  for (int value = 0; value < 100; ++value) {
    const std::size_t index = set.bucket(value);
    bool found = false;
    for (auto it = set.begin(index); it != set.end(index); ++it) {
      if (*it == value) {
        found = true;
      }
    }
    EXPECT_TRUE(found) << "element " << value << " not in its own bucket";
  }
}

TEST(UnorderedSetBuckets, RehashGrowsToRequest) {
  chaistl::unordered_set<int> set{1, 2, 3};

  set.rehash(500);

  EXPECT_GE(set.bucket_count(), 500u);
  EXPECT_TRUE(set.contains(1));
  EXPECT_TRUE(set.contains(2));
  EXPECT_TRUE(set.contains(3));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetBuckets, RehashCanShrink) {
  chaistl::unordered_set<int> set;
  for (int value = 0; value < 100; ++value) {
    set.insert(value);
  }
  for (int value = 5; value < 100; ++value) {
    set.erase(value);
  }
  const auto before = set.bucket_count();

  set.rehash(0);  // bucket_count() only needs to satisfy the load factor

  EXPECT_LT(set.bucket_count(), before);
  EXPECT_LE(set.load_factor(), set.max_load_factor());
  EXPECT_EQ(set.size(), 5u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetBuckets, ReserveAvoidsLaterRehashes) {
  chaistl::unordered_set<int> set;
  set.reserve(100);
  const auto buckets_after_reserve = set.bucket_count();

  for (int value = 0; value < 100; ++value) {
    set.insert(value);
  }

  // reserve(n) prepares for n elements: no rehash happened on the way.
  EXPECT_EQ(set.bucket_count(), buckets_after_reserve);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetBuckets, MaxLoadFactorIsAHintAndAppliesOnNextInsert) {
  chaistl::unordered_set<int> set;
  for (int value = 0; value < 10; ++value) {
    set.insert(value);
  }
  const auto before = set.bucket_count();

  set.max_load_factor(0.1f);
  // Setting the bound never rehashes by itself.
  EXPECT_EQ(set.bucket_count(), before);

  set.insert(10);  // the next insert enforces the new bound
  EXPECT_LE(set.load_factor(), set.max_load_factor());
  EXPECT_GT(set.bucket_count(), before);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetBuckets, LoadFactorOfEmptyDefaultConstructed) {
  chaistl::unordered_set<int> set;

  EXPECT_EQ(set.bucket_count(), 0u);
  EXPECT_EQ(set.load_factor(), 0.0f);
  EXPECT_EQ(set.max_load_factor(), 1.0f);
}

}  // namespace
