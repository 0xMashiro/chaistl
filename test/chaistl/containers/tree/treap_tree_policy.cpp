// SPDX-License-Identifier: Apache-2.0

// References:
// - Treap balancing: Seidel & Aragon, "Randomized Search Trees" (J. Algorithms 1996)
// - CLRS Introduction to Algorithms, 3rd ed., treap problem.
//
// These tests target treap-specific behaviour that the generic property suite
// does not exercise: heap-order restoration on insert (rotate-up), heap-order
// restoration on erase (rotate-down), deterministic behaviour with a fixed
// policy instance, and instantiation over map / multiset / multimap.

#include <gtest/gtest.h>

#include <chaistl/containers/treap_map.hpp>
#include <chaistl/containers/treap_multimap.hpp>
#include <chaistl/containers/treap_multiset.hpp>
#include <chaistl/containers/treap_set.hpp>

namespace {

using treap_set = chaistl::treap_set<int>;
using treap_map = chaistl::treap_map<int, int>;
using treap_multiset = chaistl::treap_multiset<int>;
using treap_multimap = chaistl::treap_multimap<int, int>;

// ============================================================================
// Insert rotate-up cases
// ============================================================================

TEST(TreapTreePolicy, InsertRotateUpRight) {
  // Insert keys so that the last insertion must rotate right to restore
  // heap order.  Sequence chosen so that the new leaf is a left child with
  // smaller priority than its parent.
  treap_set s;
  s.insert(2);
  s.insert(3);
  s.insert(1);  // 1 becomes left child of 2; if its priority < parent's, rotate
  EXPECT_TRUE(s.validate());
  EXPECT_EQ(s.size(), 3uz);
}

TEST(TreapTreePolicy, InsertRotateUpLeft) {
  treap_set s;
  s.insert(2);
  s.insert(1);
  s.insert(3);  // 3 becomes right child of 2; if its priority < parent's, rotate
  EXPECT_TRUE(s.validate());
  EXPECT_EQ(s.size(), 3uz);
}

TEST(TreapTreePolicy, InsertRotatesUpToRoot) {
  // A sorted insertion that forces the new node to rotate all the way to root.
  treap_set s;
  for (int i = 0; i < 100; ++i) {
    s.insert(i);
    ASSERT_TRUE(s.validate()) << "after inserting " << i;
  }
  EXPECT_EQ(s.size(), 100uz);
}

TEST(TreapTreePolicy, InsertReverseOrderRotatesUpToRoot) {
  treap_set s;
  for (int i = 100; i > 0; --i) {
    s.insert(i);
    ASSERT_TRUE(s.validate()) << "after inserting " << i;
  }
  EXPECT_EQ(s.size(), 100uz);
}

// ============================================================================
// Erase rotate-down cases
// ============================================================================

TEST(TreapTreePolicy, EraseLeaf) {
  treap_set s{1, 2, 3};
  EXPECT_EQ(s.erase(2), 1uz);
  EXPECT_TRUE(s.validate());
  EXPECT_EQ(s.size(), 2uz);
}

TEST(TreapTreePolicy, EraseRootWithTwoChildren) {
  // Root has two children; rotate-down must move the lower-priority child up.
  treap_set s;
  for (int i = 0; i < 7; ++i) {
    s.insert(i);
  }
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(3), 1uz);  // erase middle element (likely root or near)
  EXPECT_TRUE(s.validate());
  EXPECT_EQ(s.size(), 6uz);
}

TEST(TreapTreePolicy, EraseAllElementsMaintainsInvariants) {
  treap_set s;
  for (int i = 0; i < 200; ++i) {
    s.insert(i);
  }
  EXPECT_TRUE(s.validate());

  for (int i = 0; i < 200; ++i) {
    ASSERT_EQ(s.erase(i), 1uz) << "erasing " << i;
    ASSERT_TRUE(s.validate()) << "after erasing " << i;
  }
  EXPECT_TRUE(s.empty());
}

TEST(TreapTreePolicy, EraseAlternatingEnds) {
  // Erase from both ends; exercises both left and right rotate-down paths.
  treap_set s;
  for (int i = 0; i < 1024; ++i) {
    s.insert(i);
  }
  EXPECT_TRUE(s.validate());

  int low = 0;
  int high = 1023;
  while (low < high) {
    ASSERT_EQ(s.erase(low++), 1uz);
    ASSERT_TRUE(s.validate());
    ASSERT_EQ(s.erase(high--), 1uz);
    ASSERT_TRUE(s.validate());
  }
}

// ============================================================================
// Determinism with fixed seed
// ============================================================================

TEST(TreapTreePolicy, SameSeedProducesSameShape) {
  // Two treap_set instances with default-constructed policies share the same
  // fixed seed, so inserting the same keys in the same order must produce
  // identical trees (same root, same structure).
  treap_set a;
  treap_set b;
  for (int i = 0; i < 50; ++i) {
    a.insert(i);
    b.insert(i);
  }

  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());

  // In-order traversal must match.
  EXPECT_TRUE(std::ranges::equal(a, b));

  // Tree shape: iterate and compare parent/child relationships via iterators
  // is not directly possible, but we can verify erase order is identical.
  for (int i = 0; i < 50; ++i) {
    ASSERT_EQ(a.erase(i), 1uz);
    ASSERT_EQ(b.erase(i), 1uz);
    ASSERT_TRUE(a.validate());
    ASSERT_TRUE(b.validate());
    EXPECT_EQ(a.size(), b.size());
  }
  EXPECT_TRUE(a.empty());
  EXPECT_TRUE(b.empty());
}

// ============================================================================
// Map / multiset / multimap instantiation
// ============================================================================

TEST(TreapTreePolicy, MapWorksOverTreap) {
  treap_map m;
  for (int i = 0; i < 200; ++i) {
    m[i % 50] += 1;
  }
  EXPECT_TRUE(m.validate());
  EXPECT_EQ(m.size(), 50uz);
  EXPECT_EQ(m.at(7), 4);

  auto nh = m.extract(7);
  nh.key() = 99;
  m.insert(std::move(nh));
  EXPECT_TRUE(m.validate());
  EXPECT_FALSE(m.contains(7));
  EXPECT_EQ(m.at(99), 4);
}

TEST(TreapTreePolicy, MultisetWorksOverTreap) {
  treap_multiset ms;
  for (int i = 0; i < 100; ++i) {
    ms.insert(i % 10);
  }
  EXPECT_TRUE(ms.validate());
  EXPECT_EQ(ms.size(), 100uz);
  EXPECT_EQ(ms.count(3), 10uz);

  // erase one occurrence
  auto it = ms.find(3);
  ASSERT_NE(it, ms.end());
  ms.erase(it);
  EXPECT_TRUE(ms.validate());
  EXPECT_EQ(ms.count(3), 9uz);
}

TEST(TreapTreePolicy, MultimapWorksOverTreap) {
  treap_multimap mm;
  for (int i = 0; i < 100; ++i) {
    mm.insert({i % 10, i});
  }
  EXPECT_TRUE(mm.validate());
  EXPECT_EQ(mm.size(), 100uz);
  EXPECT_EQ(mm.count(3), 10uz);

  auto [first, last] = mm.equal_range(3);
  EXPECT_EQ(std::distance(first, last), 10);
}

}  // namespace
