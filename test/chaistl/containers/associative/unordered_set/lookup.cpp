// SPDX-License-Identifier: Apache-2.0

// References:
// - std::unordered_set lookup:
//   https://en.cppreference.com/w/cpp/container/unordered_set
//   https://eel.is/c++draft/unord.req
// - operator== uses set semantics; unordered containers have no operator<
//   and no operator<=> ([unord.req.general]).

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>

#include <cstdint>
#include <iterator>
#include <unordered_set>

namespace {

TEST(UnorderedSetLookup, FindHitAndMiss) {
  chaistl::unordered_set<int> set{1, 2, 3};
  const auto& const_set = set;

  EXPECT_NE(set.find(2), set.end());
  EXPECT_EQ(*set.find(2), 2);
  EXPECT_EQ(set.find(99), set.end());

  EXPECT_NE(const_set.find(3), const_set.end());
  EXPECT_EQ(const_set.find(99), const_set.end());
}

TEST(UnorderedSetLookup, FindOnEmptyDefaultConstructed) {
  chaistl::unordered_set<int> set;  // zero buckets: lookup must not divide by 0

  EXPECT_EQ(set.find(1), set.end());
  EXPECT_FALSE(set.contains(1));
  EXPECT_EQ(set.count(1), 0u);
}

TEST(UnorderedSetLookup, CountAndContains) {
  chaistl::unordered_set<int> set{1, 2};

  EXPECT_EQ(set.count(1), 1u);
  EXPECT_EQ(set.count(9), 0u);
  EXPECT_TRUE(set.contains(2));
  EXPECT_FALSE(set.contains(9));
}

TEST(UnorderedSetLookup, EqualRange) {
  chaistl::unordered_set<int> set{1, 2, 3};

  auto [first, last] = set.equal_range(2);
  ASSERT_NE(first, set.end());
  EXPECT_EQ(*first, 2);
  EXPECT_EQ(std::distance(first, last), 1);

  auto [miss_first, miss_last] = set.equal_range(99);
  EXPECT_EQ(miss_first, set.end());
  EXPECT_EQ(miss_last, set.end());
}

TEST(UnorderedSetLookup, EqualityIsSetSemantics) {
  chaistl::unordered_set<int> a{1, 2, 3};
  chaistl::unordered_set<int> b{3, 2, 1};
  chaistl::unordered_set<int> c{1, 2};

  // Same elements, different insertion order and bucket geometry.
  b.rehash(100);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

// ============================================================================
// Oracle property test against std::unordered_set: a deterministic
// pseudo-random workload of inserts and erases, with membership compared as a
// set (never by iteration order) and the structure audited along the way.
// ============================================================================

TEST(UnorderedSetLookup, OracleAgainstStd) {
  chaistl::unordered_set<int> ours;
  std::unordered_set<int> oracle;

  std::uint64_t state = 0x2545F4914F6CDD1Dull;
  auto next = [&state] {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
  };

  for (int step = 0; step < 2000; ++step) {
    const int key = static_cast<int>(next() % 200);
    if (next() % 3 != 0) {
      const auto [it, inserted] = ours.insert(key);
      const auto [oracle_it, oracle_inserted] = oracle.insert(key);
      EXPECT_EQ(inserted, oracle_inserted);
      EXPECT_EQ(*it, key);
    } else {
      EXPECT_EQ(ours.erase(key), oracle.erase(key));
    }

    EXPECT_EQ(ours.size(), oracle.size());
    if (step % 250 == 0) {
      ASSERT_TRUE(ours.validate());
      for (int candidate = 0; candidate < 200; ++candidate) {
        ASSERT_EQ(ours.contains(candidate), oracle.contains(candidate)) << "key " << candidate;
      }
    }
  }

  ASSERT_TRUE(ours.validate());
  for (int candidate = 0; candidate < 200; ++candidate) {
    ASSERT_EQ(ours.contains(candidate), oracle.contains(candidate));
  }
}

}  // namespace
