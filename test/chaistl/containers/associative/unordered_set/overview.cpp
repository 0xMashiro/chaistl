// SPDX-License-Identifier: Apache-2.0

// References:
// - std::unordered_set:
//   https://en.cppreference.com/w/cpp/container/unordered_set
//   https://eel.is/c++draft/unord.set
// - P3372 constexpr containers (the constexpr smoke test below exercises the
//   C++26 direction; chaistl::hash exists because std::hash need not be
//   constexpr-callable).

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/functional/hash.hpp>

#include <concepts>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using int_set = chaistl::unordered_set<int>;

// ============================================================================
// Member types and concepts
// ============================================================================

static_assert(std::is_same_v<int_set::key_type, int>);
static_assert(std::is_same_v<int_set::value_type, int>);
static_assert(std::is_same_v<int_set::hasher, std::hash<int>>);
static_assert(std::is_same_v<int_set::key_equal, std::equal_to<int>>);

// Set keys are immutable: both iterator types are the same const iterator.
static_assert(std::is_same_v<int_set::iterator, int_set::const_iterator>);
static_assert(std::is_same_v<int_set::local_iterator, int_set::const_local_iterator>);

static_assert(std::forward_iterator<int_set::iterator>);
static_assert(std::forward_iterator<int_set::local_iterator>);
static_assert(std::ranges::forward_range<int_set>);
static_assert(!std::bidirectional_iterator<int_set::iterator>);

static_assert(std::is_same_v<std::iter_reference_t<int_set::iterator>, const int&>);
static_assert(std::is_same_v<std::iter_reference_t<int_set::local_iterator>, const int&>);

// Unordered containers have no ordering comparisons.
static_assert(std::equality_comparable<int_set>);

// Default construction allocates nothing and cannot throw.
static_assert(std::is_nothrow_default_constructible_v<int_set>);

// ============================================================================
// chaistl::hash sanity
// ============================================================================

static_assert(chaistl::hash<int>{}(42) == chaistl::hash<int>{}(42));
static_assert(chaistl::hash<int>{}(42) != chaistl::hash<int>{}(43));
// String content hashing is consistent between string and string_view.
static_assert(chaistl::hash<std::string_view>{}("chai") == chaistl::hash<std::string>{}(std::string("chai")));

// ============================================================================
// constexpr smoke test: the whole container in constant evaluation.
// std::hash is not constexpr-callable, so the constexpr path uses
// chaistl::hash — exactly the boundary P3372 documents.
// ============================================================================

constexpr bool constexpr_smoke() {
  chaistl::unordered_set<int, chaistl::hash<int>> set;
  for (int value = 0; value < 50; ++value) {
    set.insert(value);
  }
  bool ok = set.size() == 50 && set.contains(25) && !set.contains(99);
  ok = ok && set.validate();
  ok = ok && set.load_factor() <= set.max_load_factor();

  set.erase(25);
  ok = ok && !set.contains(25) && set.size() == 49;
  for (int value : set) {
    if (value == 25) {
      ok = false;
    }
  }

  chaistl::unordered_set<int, chaistl::hash<int>> copy = set;
  ok = ok && copy.size() == set.size() && copy.contains(7) && copy.validate();
  copy.clear();
  ok = ok && copy.empty() && !set.empty();

  return ok && set.validate();
}

static_assert(constexpr_smoke());

// ============================================================================
// Runtime sanity
// ============================================================================

TEST(UnorderedSetOverview, BasicUsage) {
  int_set set{1, 2, 3};

  EXPECT_EQ(set.size(), 3u);
  EXPECT_TRUE(set.contains(2));
  EXPECT_FALSE(set.contains(4));

  auto [it, inserted] = set.insert(4);
  EXPECT_TRUE(inserted);
  EXPECT_EQ(*it, 4);
  EXPECT_TRUE(set.validate());
}

}  // namespace
