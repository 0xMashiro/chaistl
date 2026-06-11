// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/algorithm.hpp>
#include <chaistl/chaistl.hpp>
#include <chaistl/concepts.hpp>
#include <chaistl/containers.hpp>
#include <chaistl/iterator.hpp>
#include <chaistl/memory.hpp>

#include <array>
#include <compare>
#include <string_view>
#include <type_traits>

TEST(SmokeTest, ExportsProjectVersion) {
  static_assert(chaistl::project_name == std::string_view{"chaistl"});
  static_assert(chaistl::library_version.major == 0);
  static_assert(chaistl::library_version.minor == 1);
  static_assert(chaistl::library_version.patch == 0);
  static_assert(chaistl::bitset<4>{0b1010}.to_ullong() == 0b1010);
  static_assert(chaistl::dynamic_bitset<>{4, 0b1010}.to_ullong() == 0b1010);

  EXPECT_EQ(chaistl::project_name, "chaistl");
}

TEST(SmokeTest, ExportsStandardLibraryCategoryHeaders) {
  constexpr std::array lhs{1, 2};
  constexpr std::array rhs{1, 3};
  static_assert(chaistl::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()) ==
                std::strong_ordering::less);
  static_assert(chaistl::concepts::container_element<int>);
  static_assert(std::is_same_v<chaistl::array<int, 1>::value_type, int>);
  static_assert(std::is_same_v<chaistl::deque<int>::value_type, int>);
  static_assert(std::is_same_v<chaistl::vector<int>::value_type, int>);
  static_assert(std::is_same_v<chaistl::priority_queue<int>::policy_type, chaistl::heap_policy::binary_heap_policy>);
  static_assert(std::is_same_v<chaistl::priority_deque<int>::value_type, int>);
  static_assert(std::is_same_v<chaistl::max_heap<int>::value_compare, std::less<int>>);
  static_assert(std::is_same_v<chaistl::min_heap<int>::value_compare, std::greater<int>>);
  static_assert(std::is_same_v<chaistl::d_ary_heap<int, 4>::policy_type, chaistl::heap_policy::d_ary_heap_policy<4>>);
  static_assert(std::is_same_v<chaistl::pairing_heap<int>::policy_type, chaistl::heap_policy::pairing_heap_policy>);
  static_assert(std::is_same_v<chaistl::binomial_heap<int>::policy_type, chaistl::heap_policy::binomial_heap_policy>);
  static_assert(std::is_same_v<chaistl::skew_heap<int>::policy_type, chaistl::heap_policy::skew_heap_policy>);
  static_assert(std::is_same_v<chaistl::leftist_heap<int>::policy_type, chaistl::heap_policy::leftist_heap_policy>);
  static_assert(std::is_same_v<chaistl::fibonacci_heap<int>::policy_type, chaistl::heap_policy::fibonacci_heap_policy>);
  static_assert(std::is_same_v<chaistl::avl_set<int>::key_type, int>);
  static_assert(std::is_same_v<chaistl::treap_set<int>::key_type, int>);
  static_assert(std::is_same_v<chaistl::size_balanced_set<int>::key_type, int>);
  static_assert(std::is_same_v<chaistl::weight_balanced_set<int>::key_type, int>);
  static_assert(std::is_same_v<chaistl::order_statistic_set<int>::key_type, int>);
  static_assert(std::is_same_v<chaistl::order_statistic_multiset<int>::key_type, int>);
  static_assert(std::is_same_v<chaistl::order_statistic_map<int, int>::key_type, int>);
  static_assert(std::is_same_v<chaistl::order_statistic_multimap<int, int>::key_type, int>);
  static_assert(std::is_same_v<chaistl::skip_list_multiset<int>::key_type, int>);
  static_assert(std::is_same_v<chaistl::skip_list_set<int>::key_type, int>);
  static_assert(chaistl::concepts::contiguous_iterator<int*>);
  static_assert(chaistl::concepts::allocator_for<chaistl::allocator<int>, int>);
}
