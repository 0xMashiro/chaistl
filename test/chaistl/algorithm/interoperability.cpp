// SPDX-License-Identifier: Apache-2.0

// Standard algorithms are a useful compatibility test for STL-like containers:
// they exercise iterator concepts, projections, sentinel equality, and mutable
// element access without relying on container-specific member functions.

#include <gtest/gtest.h>

#include <chaistl/containers/deque.hpp>
#include <chaistl/containers/flat_set.hpp>
#include <chaistl/containers/forward_list.hpp>
#include <chaistl/containers/list.hpp>
#include <chaistl/containers/map.hpp>
#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/containers/stack.hpp>
#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/containers/vector.hpp>
#include <chaistl/experimental/containers/ring_buffer.hpp>

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

namespace {

static_assert(std::ranges::contiguous_range<chaistl::vector<int>>);
static_assert(std::ranges::random_access_range<chaistl::deque<int>>);
static_assert(std::ranges::bidirectional_range<chaistl::list<int>>);
static_assert(std::ranges::forward_range<chaistl::forward_list<int>>);
static_assert(std::ranges::bidirectional_range<chaistl::set<int>>);
static_assert(std::ranges::bidirectional_range<chaistl::map<int, std::string>>);
static_assert(std::ranges::forward_range<chaistl::unordered_set<int>>);
static_assert(std::ranges::random_access_range<chaistl::flat_set<int>>);
static_assert(std::ranges::random_access_range<chaistl::experimental::ring_buffer<int>>);

static_assert(!std::ranges::range<chaistl::stack<int>>);
static_assert(!std::ranges::range<chaistl::priority_queue<int>>);

TEST(StandardAlgorithmInteropTest, RandomAccessSequenceContainersSupportPermutationAlgorithms) {
  chaistl::vector<int> vector_values{5, 1, 4, 2, 3};
  std::ranges::sort(vector_values);
  EXPECT_TRUE(std::ranges::equal(vector_values, std::array{1, 2, 3, 4, 5}));
  EXPECT_TRUE(std::ranges::binary_search(vector_values, 4));

  chaistl::deque<int> deque_values{6, 2, 5, 1, 4, 3};
  std::ranges::sort(deque_values);
  EXPECT_TRUE(std::ranges::equal(deque_values, std::array{1, 2, 3, 4, 5, 6}));

  std::ranges::reverse(deque_values);
  EXPECT_TRUE(std::ranges::equal(deque_values, std::array{6, 5, 4, 3, 2, 1}));
}

TEST(StandardAlgorithmInteropTest, NodeSequenceContainersSupportIteratorCategoryAppropriateAlgorithms) {
  chaistl::forward_list<int> forward_values{1, 2, 3, 4};
  std::ranges::replace(forward_values, 2, 20);

  std::vector<int> copied;
  std::ranges::copy(forward_values, std::back_inserter(copied));
  EXPECT_TRUE(std::ranges::equal(copied, std::array{1, 20, 3, 4}));
  EXPECT_EQ(std::ranges::find(forward_values, 3), std::next(forward_values.begin(), 2));

  chaistl::list<int> list_values{1, 2, 3, 4};
  std::ranges::reverse(list_values);
  EXPECT_TRUE(std::ranges::equal(list_values, std::array{4, 3, 2, 1}));
  EXPECT_EQ(std::ranges::distance(list_values), 4);
}

TEST(StandardAlgorithmInteropTest, AssociativeContainersSupportReadOnlyAlgorithmsAndProjections) {
  chaistl::set<int> set_values{4, 1, 3, 2};
  EXPECT_TRUE(std::ranges::equal(set_values, std::array{1, 2, 3, 4}));
  EXPECT_EQ(*std::ranges::lower_bound(set_values, 3), 3);

  chaistl::map<int, std::string> map_values{{1, "one"}, {3, "three"}, {2, "two"}};
  const auto found = std::ranges::find_if(map_values, [](const auto& entry) {
    return entry.second == "two";
  });
  ASSERT_NE(found, map_values.end());
  EXPECT_EQ(found->first, 2);

  const auto bound = std::ranges::lower_bound(map_values, 3, {}, [](const auto& entry) {
    return entry.first;
  });
  ASSERT_NE(bound, map_values.end());
  EXPECT_EQ(bound->second, "three");
}

TEST(StandardAlgorithmInteropTest, HashContainersSupportForwardTraversalAlgorithms) {
  chaistl::unordered_set<int> values{1, 2, 3, 4};

  EXPECT_NE(std::ranges::find(values, 3), values.end());
  EXPECT_EQ(std::ranges::count_if(values, [](int value) {
              return value % 2 == 0;
            }),
            2);
}

TEST(StandardAlgorithmInteropTest, FlatAndRingContainersExposeTheirStrongerIteratorCapabilities) {
  chaistl::flat_set<int> flat_values{4, 1, 3, 2};
  EXPECT_TRUE(std::ranges::binary_search(flat_values, 3));
  EXPECT_TRUE(std::ranges::equal(flat_values, std::array{1, 2, 3, 4}));

  chaistl::experimental::ring_buffer<int> ring_values{1, 2, 3, 4};
  std::ranges::rotate(ring_values, std::next(ring_values.begin(), 1));
  EXPECT_TRUE(std::ranges::equal(ring_values, std::array{2, 3, 4, 1}));
}

}  // namespace
