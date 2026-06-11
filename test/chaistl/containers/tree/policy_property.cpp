// SPDX-License-Identifier: Apache-2.0

// References:
// - Balance policy contract: include/chaistl/containers/tree/binary_search_tree.hpp
// - Policy-based tree design: docs/develop/decisions/tree-policy-design.md
//
// Property tests shared by every balance policy: under randomized and
// adversarial workloads the containers must behave exactly like std::set /
// std::multiset (the oracles) while keeping their structural invariants
// (validate()) intact after every single mutation. New policies join the
// Policies list and inherit the whole suite.

#include <gtest/gtest.h>

#include <chaistl/containers/avl_map.hpp>
#include <chaistl/containers/avl_set.hpp>
#include <chaistl/containers/map.hpp>
#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/containers/splay_map.hpp>
#include <chaistl/containers/splay_set.hpp>
#include <chaistl/containers/tree/policy/avl_tree.hpp>
#include <chaistl/containers/tree/policy/order_statistic_tree.hpp>
#include <chaistl/containers/tree/policy/rb_tree.hpp>
#include <chaistl/containers/tree/policy/size_balanced_tree.hpp>
#include <chaistl/containers/tree/policy/splay_tree.hpp>
#include <chaistl/containers/tree/policy/treap_tree.hpp>
#include <chaistl/containers/tree/policy/weight_balanced_tree.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <random>
#include <set>
#include <utility>

namespace {

template <class Policy>
class TreePolicyPropertyTest : public ::testing::Test {
 public:
  using set_type = chaistl::set<int, std::less<int>, chaistl::allocator<int>, Policy>;
  using multiset_type = chaistl::multiset<int, std::less<int>, chaistl::allocator<int>, Policy>;
};

using Policies = ::testing::Types<chaistl::detail::tree::rb_tree_policy,
                                  chaistl::detail::tree::avl_tree_policy,
                                  chaistl::detail::tree::splay_tree_policy,
                                  chaistl::detail::tree::treap_tree_policy,
                                  chaistl::detail::tree::size_balanced_tree_policy,
                                  chaistl::detail::tree::weight_balanced_tree_policy,
                                  chaistl::detail::tree::order_statistic_tree_policy>;
TYPED_TEST_SUITE(TreePolicyPropertyTest, Policies);

struct recording_access_policy : chaistl::detail::tree::rb_tree_policy {
  static inline int accesses = 0;
  static inline int last_value = -1;

  static void reset() noexcept {
    accesses = 0;
    last_value = -1;
  }

  template <class Node>
  constexpr void after_access(Node* node, chaistl::detail::tree::bst_node_base&) noexcept {
    ++accesses;
    last_value = node->value;
  }
};

using recording_access_set = chaistl::set<int, std::less<int>, chaistl::allocator<int>, recording_access_policy>;

TYPED_TEST(TreePolicyPropertyTest, RandomizedOpsMatchStdSetOracle) {
  using set_type = typename TestFixture::set_type;

  for (const unsigned seed : {1u, 2u, 3u}) {
    std::mt19937 rng(seed);
    // Bounded key space forces duplicate inserts and hits on erase.
    std::uniform_int_distribution<int> key(0, 255);
    std::uniform_int_distribution<int> op(0, 99);

    set_type tree;
    std::set<int> oracle;

    for (int step = 0; step < 4000; ++step) {
      const int k = key(rng);
      const int action = op(rng);
      if (action < 50) {
        auto [it, inserted] = tree.insert(k);
        auto [oracle_it, oracle_inserted] = oracle.insert(k);
        ASSERT_EQ(inserted, oracle_inserted) << "seed " << seed << " step " << step;
        ASSERT_EQ(*it, *oracle_it);
      } else if (action < 80) {
        ASSERT_EQ(tree.erase(k), oracle.erase(k)) << "seed " << seed << " step " << step;
      } else if (action < 90) {
        // extract + reinsert exercises unlink and node-handle insertion.
        auto nh = tree.extract(k);
        ASSERT_EQ(static_cast<bool>(nh), oracle.contains(k));
        if (nh) {
          tree.insert(std::move(nh));
        }
      } else {
        ASSERT_EQ(tree.contains(k), oracle.contains(k));
      }
      ASSERT_TRUE(tree.validate()) << "seed " << seed << " step " << step;
      ASSERT_EQ(tree.size(), oracle.size()) << "seed " << seed << " step " << step;
    }
    ASSERT_TRUE(std::ranges::equal(tree, oracle));
  }
}

TEST(TreePolicyAccessHook, LookupNotifiesPolicyWhenCapabilityIsPresent) {
  recording_access_set tree{1, 2, 3};
  ASSERT_TRUE(tree.validate());

  recording_access_policy::reset();
  auto found = tree.find(2);
  ASSERT_NE(found, tree.end());
  EXPECT_EQ(*found, 2);
  EXPECT_EQ(recording_access_policy::accesses, 1);
  EXPECT_EQ(recording_access_policy::last_value, 2);

  auto lower = tree.lower_bound(3);
  ASSERT_NE(lower, tree.end());
  EXPECT_EQ(*lower, 3);
  EXPECT_EQ(recording_access_policy::accesses, 2);
  EXPECT_EQ(recording_access_policy::last_value, 3);

  auto upper = tree.upper_bound(3);
  EXPECT_EQ(upper, tree.end());
  EXPECT_EQ(recording_access_policy::accesses, 3);
  EXPECT_EQ(recording_access_policy::last_value, 3);
  EXPECT_TRUE(tree.validate());

  auto missing = tree.find(99);
  EXPECT_EQ(missing, tree.end());
  EXPECT_EQ(recording_access_policy::accesses, 4);
  EXPECT_EQ(recording_access_policy::last_value, 3);
}

TEST(TreePolicyAccessHook, ConstLookupDoesNotNotifyAccessPolicy) {
  recording_access_set tree{1, 2, 3};
  const recording_access_set& ctree = tree;

  recording_access_policy::reset();
  EXPECT_NE(ctree.find(2), ctree.end());
  EXPECT_NE(ctree.lower_bound(2), ctree.end());
  EXPECT_NE(ctree.upper_bound(2), ctree.end());
  EXPECT_TRUE(ctree.contains(2));
  EXPECT_EQ(ctree.count(2), 1uz);
  EXPECT_EQ(recording_access_policy::accesses, 0);
  EXPECT_TRUE(ctree.validate());
}

TYPED_TEST(TreePolicyPropertyTest, RandomizedMultisetOpsMatchStdMultisetOracle) {
  using multiset_type = typename TestFixture::multiset_type;

  for (const unsigned seed : {11u, 12u, 13u}) {
    std::mt19937 rng(seed);
    // Tight key space: long equivalent runs are the norm, not the edge
    // case, so the equal-family position searches and the whole-run erase
    // are exercised constantly.
    std::uniform_int_distribution<int> key(0, 63);
    std::uniform_int_distribution<int> op(0, 99);

    multiset_type tree;
    std::multiset<int> oracle;

    for (int step = 0; step < 4000; ++step) {
      const int k = key(rng);
      const int action = op(rng);
      if (action < 45) {
        auto it = tree.insert(k);
        oracle.insert(k);
        ASSERT_EQ(*it, k) << "seed " << seed << " step " << step;
      } else if (action < 55) {
        // Hint at lower_bound: the element lands at the front of its run.
        auto it = tree.insert(tree.lower_bound(k), k);
        oracle.insert(oracle.lower_bound(k), k);
        ASSERT_EQ(*it, k) << "seed " << seed << " step " << step;
      } else if (action < 75) {
        // erase(key) removes the whole run in both containers.
        ASSERT_EQ(tree.erase(k), oracle.erase(k)) << "seed " << seed << " step " << step;
      } else if (action < 85) {
        // extract + reinsert: unlink and node-handle insertion under
        // equivalent keys; the multiset is unchanged as a multiset.
        auto nh = tree.extract(k);
        ASSERT_EQ(static_cast<bool>(nh), oracle.contains(k)) << "seed " << seed << " step " << step;
        if (nh) {
          tree.insert(std::move(nh));
        }
      } else if (action < 95) {
        ASSERT_EQ(tree.count(k), oracle.count(k)) << "seed " << seed << " step " << step;
      } else {
        auto [first, last] = tree.equal_range(k);
        ASSERT_EQ(std::distance(first, last), static_cast<std::ptrdiff_t>(oracle.count(k)))
            << "seed " << seed << " step " << step;
      }
      ASSERT_TRUE(tree.validate()) << "seed " << seed << " step " << step;
      ASSERT_EQ(tree.size(), oracle.size()) << "seed " << seed << " step " << step;
    }
    ASSERT_TRUE(std::ranges::equal(tree, oracle));
  }
}

TYPED_TEST(TreePolicyPropertyTest, SortedInsertThenSweepErase) {
  // Sorted insertion is the classic degenerate workload for an unbalanced
  // BST; a balance policy must keep the structure valid throughout.
  using set_type = typename TestFixture::set_type;
  set_type tree;

  for (int i = 0; i < 2000; ++i) {
    tree.insert(i);
  }
  ASSERT_TRUE(tree.validate());
  ASSERT_EQ(tree.size(), 2000uz);

  for (int i = 0; i < 2000; i += 2) {
    ASSERT_EQ(tree.erase(i), 1uz);
  }
  ASSERT_TRUE(tree.validate());
  ASSERT_EQ(tree.size(), 1000uz);

  for (int i = 1999; i > 0; i -= 2) {
    ASSERT_EQ(tree.erase(i), 1uz);
  }
  ASSERT_TRUE(tree.validate());
  ASSERT_TRUE(tree.empty());
}

TYPED_TEST(TreePolicyPropertyTest, CopyMoveSwapMergePreserveInvariants) {
  using set_type = typename TestFixture::set_type;
  std::mt19937 rng(7);
  std::uniform_int_distribution<int> key(0, 999);

  set_type a;
  set_type b;
  for (int i = 0; i < 500; ++i) {
    a.insert(key(rng));
    b.insert(key(rng));
  }
  ASSERT_TRUE(a.validate());
  ASSERT_TRUE(b.validate());

  set_type copy = a;
  ASSERT_TRUE(copy.validate());
  ASSERT_TRUE(std::ranges::equal(copy, a));

  set_type moved = std::move(copy);
  ASSERT_TRUE(moved.validate());
  ASSERT_TRUE(std::ranges::equal(moved, a));

  moved.swap(b);
  ASSERT_TRUE(moved.validate());
  ASSERT_TRUE(b.validate());

  // moved now holds b's original elements; merging a in must keep both
  // trees valid and produce the set union.
  std::set<int> expected(moved.begin(), moved.end());
  expected.insert(a.begin(), a.end());
  moved.merge(a);
  ASSERT_TRUE(moved.validate());
  ASSERT_TRUE(a.validate());
  ASSERT_TRUE(std::ranges::equal(moved, expected));
}

// ============================================================================
// AVL-specific cases
// ============================================================================

using avl_set = chaistl::avl_set<int>;
using avl_map = chaistl::avl_map<int, int>;
using splay_set = chaistl::splay_set<int>;
using splay_map = chaistl::splay_map<int, int>;

TEST(AvlTreePolicy, InsertRotationCases) {
  {
    avl_set s{1, 2, 3};  // right-right: single left rotation
    EXPECT_TRUE(s.validate());
  }
  {
    avl_set s{3, 2, 1};  // left-left: single right rotation
    EXPECT_TRUE(s.validate());
  }
  {
    avl_set s{1, 3, 2};  // right-left: double rotation
    EXPECT_TRUE(s.validate());
  }
  {
    avl_set s{3, 1, 2};  // left-right: double rotation
    EXPECT_TRUE(s.validate());
  }
}

TEST(AvlTreePolicy, EraseCascadesAreValid) {
  // A Fibonacci-shaped (minimal) AVL tree is the worst case for deletion:
  // removing one node can cascade rotations along the whole search path.
  // Build minimal-ish trees by sorted insert, then delete from both ends.
  avl_set s;
  for (int i = 0; i < 1024; ++i) {
    s.insert(i);
  }
  int low = 0;
  int high = 1023;
  while (low < high) {
    ASSERT_EQ(s.erase(low++), 1uz);
    ASSERT_TRUE(s.validate());
    ASSERT_EQ(s.erase(high--), 1uz);
    ASSERT_TRUE(s.validate());
  }
}

TEST(AvlTreePolicy, MapWorksOverAvl) {
  avl_map m;
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

// ============================================================================
// Splay-specific cases
// ============================================================================

TEST(SplayTreePolicy, LookupSplaysAccessedNodeToRootWithoutBreakingOrder) {
  splay_set s{1, 2, 3, 4, 5};

  auto it = s.find(4);
  ASSERT_NE(it, s.end());
  EXPECT_EQ(*it, 4);
  EXPECT_TRUE(s.validate());
  EXPECT_EQ(*s.begin(), 1);

  auto lower = s.lower_bound(5);
  ASSERT_NE(lower, s.end());
  EXPECT_EQ(*lower, 5);
  EXPECT_TRUE(s.validate());

  auto upper = s.upper_bound(5);
  EXPECT_EQ(upper, s.end());
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.find(99), s.end());
  EXPECT_TRUE(s.validate());
  EXPECT_TRUE(std::ranges::equal(s, std::set<int>{1, 2, 3, 4, 5}));
}

TEST(SplayTreePolicy, MapNodeHandlesRemainUsable) {
  splay_map m;
  for (int i = 0; i < 100; ++i) {
    m.emplace(i, i * 10);
  }

  EXPECT_EQ(m.at(42), 420);
  auto nh = m.extract(42);
  ASSERT_TRUE(nh);
  nh.key() = 142;
  auto result = m.insert(std::move(nh));

  EXPECT_TRUE(result.inserted);
  EXPECT_EQ(result.position->first, 142);
  EXPECT_EQ(result.position->second, 420);
  EXPECT_TRUE(m.validate());
  EXPECT_FALSE(m.contains(42));
}

}  // namespace
