// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/order_statistic_map.hpp>
#include <chaistl/containers/order_statistic_multimap.hpp>
#include <chaistl/containers/order_statistic_multiset.hpp>
#include <chaistl/containers/order_statistic_set.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/containers/size_balanced_set.hpp>
#include <chaistl/containers/weight_balanced_set.hpp>

#include <algorithm>
#include <random>
#include <set>
#include <type_traits>
#include <vector>

namespace {

template <class Set>
concept has_order_statistics = requires(Set s, const typename Set::key_type& key, typename Set::size_type order) {
  { s.find_by_order(order) };
  { s.order_of_key(key) } -> std::same_as<typename Set::size_type>;
};

static_assert(has_order_statistics<chaistl::order_statistic_set<int>>);
static_assert(has_order_statistics<chaistl::order_statistic_multiset<int>>);
static_assert(has_order_statistics<chaistl::order_statistic_map<int, int>>);
static_assert(has_order_statistics<chaistl::order_statistic_multimap<int, int>>);
static_assert(has_order_statistics<chaistl::size_balanced_set<int>>);
static_assert(has_order_statistics<chaistl::weight_balanced_set<int>>);
static_assert(!has_order_statistics<chaistl::set<int>>);

std::vector<int> as_vector(const std::set<int>& values) {
  return {values.begin(), values.end()};
}

template <has_order_statistics Set>
void expect_matches_oracle(const Set& tree, const std::set<int>& oracle) {
  ASSERT_TRUE(tree.validate());
  ASSERT_EQ(tree.size(), oracle.size());

  const std::vector<int> sorted = as_vector(oracle);
  for (std::size_t i = 0; i < sorted.size(); ++i) {
    auto it = tree.find_by_order(i);
    ASSERT_NE(it, tree.end()) << "missing order " << i;
    EXPECT_EQ(*it, sorted[i]) << "wrong value at order " << i;
    EXPECT_EQ(tree.order_of_key(sorted[i]), i);
  }
  EXPECT_EQ(tree.find_by_order(sorted.size()), tree.end());

  for (int probe = -5; probe <= 105; ++probe) {
    const auto expected = static_cast<std::size_t>(std::ranges::lower_bound(sorted, probe) - sorted.begin());
    EXPECT_EQ(tree.order_of_key(probe), expected) << "probe " << probe;
  }
}

}  // namespace

TEST(OrderStatisticSetTest, RankAndSelectOnFixedValues) {
  chaistl::order_statistic_set<int> tree{5, 1, 9, 3, 7, 3};

  ASSERT_TRUE(tree.validate());
  ASSERT_EQ(tree.size(), 5uz);
  EXPECT_EQ(*tree.find_by_order(0), 1);
  EXPECT_EQ(*tree.find_by_order(2), 5);
  EXPECT_EQ(*tree.find_by_order(4), 9);
  EXPECT_EQ(tree.find_by_order(5), tree.end());

  EXPECT_EQ(tree.order_of_key(0), 0uz);
  EXPECT_EQ(tree.order_of_key(3), 1uz);
  EXPECT_EQ(tree.order_of_key(4), 2uz);
  EXPECT_EQ(tree.order_of_key(10), 5uz);
}

TEST(OrderStatisticSetTest, MutationsMaintainSubtreeSizes) {
  chaistl::order_statistic_set<int> tree;
  std::set<int> oracle;

  std::mt19937 rng(20260612u);
  for (int step = 0; step < 1000; ++step) {
    const int value = static_cast<int>(rng() % 100);
    if (rng() % 100 < 62) {
      const auto [it, inserted] = tree.insert(value);
      const auto [oracle_it, oracle_inserted] = oracle.insert(value);
      EXPECT_EQ(inserted, oracle_inserted);
      if (inserted) {
        EXPECT_EQ(*it, *oracle_it);
      }
    } else {
      EXPECT_EQ(tree.erase(value), oracle.erase(value));
    }
    if (step % 7 == 0) {
      expect_matches_oracle(tree, oracle);
    }
  }
  expect_matches_oracle(tree, oracle);
}

TEST(OrderStatisticSetTest, HintsAndNodeHandlesMaintainRanks) {
  chaistl::order_statistic_set<int> tree;
  std::set<int> oracle;

  for (int value : {8, 1, 6, 3, 10, 4}) {
    tree.insert(tree.end(), value);
    oracle.insert(value);
  }
  expect_matches_oracle(tree, oracle);

  auto nh = tree.extract(6);
  oracle.erase(6);
  expect_matches_oracle(tree, oracle);

  nh.value() = 7;
  auto [it, inserted, leftover] = tree.insert(std::move(nh));
  EXPECT_TRUE(inserted);
  EXPECT_TRUE(leftover.empty());
  EXPECT_EQ(*it, 7);
  oracle.insert(7);
  expect_matches_oracle(tree, oracle);
}

TEST(WeightBalancedSetTest, SubtreeSizeCapabilityProvidesRankAndSelect) {
  chaistl::weight_balanced_set<int> tree;
  std::set<int> oracle;

  for (int value : {12, 2, 8, 20, 4, 16, 10, 6}) {
    tree.insert(value);
    oracle.insert(value);
  }

  expect_matches_oracle(tree, oracle);
  EXPECT_EQ(*tree.find_by_order(3), 8);
  EXPECT_EQ(tree.order_of_key(15), 6uz);
}

TEST(SizeBalancedSetTest, SubtreeSizeCapabilityProvidesRankAndSelect) {
  chaistl::size_balanced_set<int> tree;
  std::set<int> oracle;

  for (int value : {12, 2, 8, 20, 4, 16, 10, 6}) {
    tree.insert(value);
    oracle.insert(value);
  }

  expect_matches_oracle(tree, oracle);
  EXPECT_EQ(*tree.find_by_order(4), 10);
  EXPECT_EQ(tree.order_of_key(15), 6uz);
}

TEST(OrderStatisticMultisetTest, DuplicatesParticipateInRankAndSelect) {
  chaistl::order_statistic_multiset<int> tree{3, 1, 3, 2};

  ASSERT_TRUE(tree.validate());
  ASSERT_EQ(tree.size(), 4uz);
  EXPECT_EQ(*tree.find_by_order(0), 1);
  EXPECT_EQ(*tree.find_by_order(1), 2);
  EXPECT_EQ(*tree.find_by_order(2), 3);
  EXPECT_EQ(*tree.find_by_order(3), 3);
  EXPECT_EQ(tree.find_by_order(4), tree.end());
  EXPECT_EQ(tree.order_of_key(3), 2uz);
  EXPECT_EQ(tree.order_of_key(4), 4uz);
}

TEST(OrderStatisticMapTest, RankAndSelectUseKeys) {
  chaistl::order_statistic_map<int, char> tree{{5, 'e'}, {1, 'a'}, {3, 'c'}};

  ASSERT_TRUE(tree.validate());
  ASSERT_EQ(tree.size(), 3uz);
  EXPECT_EQ(tree.find_by_order(0)->first, 1);
  EXPECT_EQ(tree.find_by_order(1)->second, 'c');
  EXPECT_EQ(tree.find_by_order(3), tree.end());
  EXPECT_EQ(tree.order_of_key(4), 2uz);
}

TEST(OrderStatisticMultimapTest, DuplicateKeysParticipateInRankAndSelect) {
  chaistl::order_statistic_multimap<int, char> tree{{2, 'b'}, {1, 'a'}, {2, 'c'}, {4, 'd'}};

  ASSERT_TRUE(tree.validate());
  ASSERT_EQ(tree.size(), 4uz);
  EXPECT_EQ(tree.find_by_order(0)->first, 1);
  EXPECT_EQ(tree.find_by_order(1)->first, 2);
  EXPECT_EQ(tree.find_by_order(2)->first, 2);
  EXPECT_EQ(tree.order_of_key(2), 1uz);
  EXPECT_EQ(tree.order_of_key(3), 3uz);
}
