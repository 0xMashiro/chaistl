// SPDX-License-Identifier: Apache-2.0

// References:
// - std::multimap overview:
//   https://en.cppreference.com/w/cpp/container/multimap
//   https://eel.is/c++draft/multimap.overview
//
// These suites focus on what distinguishes multimap from map (equivalent
// keys, iterator-returning inserts, no unique-key conveniences); the
// shared tree core is exercised by the set/map suites.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/avl_multimap.hpp>
#include <chaistl/containers/multimap.hpp>

#include <concepts>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(MultimapOverview, MemberTypes) {
  using mm = chaistl::multimap<int, std::string>;

  static_assert(std::same_as<mm::key_type, int>);
  static_assert(std::same_as<mm::mapped_type, std::string>);
  static_assert(std::same_as<mm::value_type, std::pair<const int, std::string>>);
  static_assert(std::same_as<mm::key_compare, std::less<int>>);

  // Unlike set/multiset, the mapped part stays mutable through iterator.
  static_assert(!std::same_as<mm::iterator, mm::const_iterator>);
  static_assert(std::bidirectional_iterator<mm::iterator>);
}

// Probed through concepts: in a non-dependent context a failing
// requires-expression is a hard error, not false.
template <class Container>
concept has_insert_return_type = requires { typename Container::insert_return_type; };

template <class Container>
concept has_subscript = requires(Container container) { container[typename Container::key_type{}]; };

template <class Container>
concept has_at = requires(Container container) { container.at(typename Container::key_type{}); };

template <class Container>
concept has_try_emplace = requires(Container container) { container.try_emplace(typename Container::key_type{}); };

TEST(MultimapOverview, NoUniqueKeyConveniences) {
  using mm = chaistl::multimap<int, int>;
  static_assert(!has_insert_return_type<mm>);
  static_assert(!has_subscript<mm>);
  static_assert(!has_at<mm>);
  static_assert(!has_try_emplace<mm>);
}

TEST(MultimapOverview, InsertReturnsPlainIterator) {
  using mm = chaistl::multimap<int, int>;
  static_assert(std::same_as<decltype(std::declval<mm&>().insert(std::declval<mm::value_type>())), mm::iterator>);
}

TEST(MultimapOverview, MappedValueIsMutable) {
  chaistl::multimap<int, int> mm{{1, 10}, {1, 20}};
  for (auto& [key, mapped] : mm) {
    mapped += 1;
  }
  EXPECT_THAT(mm, ElementsAre(Pair(1, 11), Pair(1, 21)));
  EXPECT_TRUE(mm.validate());
}

TEST(MultimapOverview, ValueCompareComparesKeysOnly) {
  chaistl::multimap<int, int> mm;
  auto vc = mm.value_comp();
  EXPECT_TRUE(vc({1, 99}, {2, 0}));
  EXPECT_FALSE(vc({1, 0}, {1, 99}));  // equivalent keys, mapped ignored
}

TEST(MultimapOverview, AvlAliasInstantiation) {
  chaistl::avl_multimap<int, int> mm{{2, 1}, {1, 1}, {2, 2}};
  EXPECT_EQ(mm.size(), 3uz);
  EXPECT_EQ(mm.count(2), 2uz);
  EXPECT_TRUE(mm.validate());
}

}  // namespace
