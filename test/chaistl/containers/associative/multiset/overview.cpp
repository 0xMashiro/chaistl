// SPDX-License-Identifier: Apache-2.0

// References:
// - std::multiset overview:
//   https://en.cppreference.com/w/cpp/container/multiset
//   https://eel.is/c++draft/multiset.overview
//
// These suites focus on what distinguishes multiset from set (equivalent
// keys, iterator-returning inserts, run semantics); the shared tree core
// is exercised by the set/map suites.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/avl_multiset.hpp>
#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/set.hpp>

#include <concepts>
#include <functional>
#include <type_traits>

namespace {

using ::testing::ElementsAre;

TEST(MultisetOverview, MemberTypes) {
  using ms = chaistl::multiset<int>;

  static_assert(std::same_as<ms::key_type, int>);
  static_assert(std::same_as<ms::value_type, int>);
  static_assert(std::same_as<ms::size_type, std::size_t>);
  static_assert(std::same_as<ms::difference_type, std::ptrdiff_t>);
  static_assert(std::same_as<ms::key_compare, std::less<int>>);
  static_assert(std::same_as<ms::allocator_type, chaistl::allocator<int>>);
  static_assert(std::same_as<ms::reference, int&>);
  static_assert(std::same_as<ms::const_reference, const int&>);

  // [multiset.overview]: iterator and const_iterator are both constant
  // iterators (and may be the same type).
  static_assert(std::same_as<ms::iterator, ms::const_iterator>);
  static_assert(std::same_as<std::iter_reference_t<ms::iterator>, const int&>);
  static_assert(std::bidirectional_iterator<ms::iterator>);
}

// Probed through a concept: in a non-dependent context a failing
// requires-expression is a hard error, not false.
template <class Container>
concept has_insert_return_type = requires { typename Container::insert_return_type; };

TEST(MultisetOverview, NoInsertReturnType) {
  // insert_return_type belongs to unique-key containers only.
  static_assert(!has_insert_return_type<chaistl::multiset<int>>);
  static_assert(has_insert_return_type<chaistl::set<int>>);
}

TEST(MultisetOverview, InsertReturnsPlainIterator) {
  using ms = chaistl::multiset<int>;
  static_assert(std::same_as<decltype(std::declval<ms&>().insert(1)), ms::iterator>);
  static_assert(std::same_as<decltype(std::declval<ms&>().emplace(1)), ms::iterator>);
}

TEST(MultisetOverview, ValueCompare) {
  chaistl::multiset<int> ms;
  auto vc = ms.value_comp();
  EXPECT_TRUE(vc(1, 2));
  EXPECT_FALSE(vc(2, 1));
  EXPECT_FALSE(vc(1, 1));
}

TEST(MultisetOverview, CustomComparator) {
  chaistl::multiset<int, std::greater<int>> ms{3, 1, 3, 2};
  EXPECT_THAT(ms, ElementsAre(3, 3, 2, 1));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetOverview, AvlAliasInstantiation) {
  chaistl::avl_multiset<int> ms{2, 1, 2, 3, 2};
  EXPECT_EQ(ms.size(), 5uz);
  EXPECT_EQ(ms.count(2), 3uz);
  EXPECT_THAT(ms, ElementsAre(1, 2, 2, 2, 3));
  EXPECT_TRUE(ms.validate());
}

}  // namespace
