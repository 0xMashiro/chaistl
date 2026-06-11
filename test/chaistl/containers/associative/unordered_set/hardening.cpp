// SPDX-License-Identifier: Apache-2.0

// References:
// - P3471-style hardened preconditions (chaistl/utility/hardening.hpp):
//   violations terminate via CHAI_HARDENED. Death tests are compiled only
//   when hardening is enabled (debug presets).

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>

namespace {

#ifdef CHAI_HARDENING_ENABLED

TEST(UnorderedSetDeathTest, BucketOfKeyOnZeroBucketTableAborts) {
  chaistl::unordered_set<int> set;  // default-constructed: zero buckets
  EXPECT_DEATH((void)set.bucket(1), "");
}

TEST(UnorderedSetDeathTest, BucketSizeOutOfRangeAborts) {
  chaistl::unordered_set<int> set{1};
  EXPECT_DEATH((void)set.bucket_size(set.bucket_count()), "");
}

TEST(UnorderedSetDeathTest, LocalIteratorIndexOutOfRangeAborts) {
  chaistl::unordered_set<int> set{1};
  EXPECT_DEATH((void)set.begin(set.bucket_count()), "");
}

TEST(UnorderedSetDeathTest, EraseEndIteratorAborts) {
  chaistl::unordered_set<int> set{1};
  EXPECT_DEATH(set.erase(set.end()), "");
}

TEST(UnorderedSetDeathTest, NonPositiveMaxLoadFactorAborts) {
  chaistl::unordered_set<int> set;
  EXPECT_DEATH(set.max_load_factor(0.0f), "");
}

#endif  // CHAI_HARDENING_ENABLED

}  // namespace
