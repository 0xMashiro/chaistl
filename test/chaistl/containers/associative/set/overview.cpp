// SPDX-License-Identifier: Apache-2.0

// References:
// - std::set member types and container requirements:
//   https://en.cppreference.com/w/cpp/container/set
//   https://eel.is/c++draft/set

#include <chaistl/concepts.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/memory/allocator.hpp>

#include <concepts>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>

namespace {

template <class Set>
concept can_copy_insert = requires(Set& s, const typename Set::value_type& value) { s.insert(value); };

template <class Set>
concept can_move_insert = requires(Set& s, typename Set::value_type&& value) { s.insert(std::move(value)); };

}  // namespace

static_assert(std::ranges::range<chaistl::set<int>>);
static_assert(std::ranges::sized_range<chaistl::set<int>>);
static_assert(!std::ranges::contiguous_range<chaistl::set<int>>);
static_assert(std::same_as<chaistl::set<int>::value_type, int>);
static_assert(std::same_as<chaistl::set<int>::key_type, int>);
static_assert(std::same_as<chaistl::set<int>::allocator_type, chaistl::allocator<int>>);

static_assert(std::copy_constructible<chaistl::set<int>>);
static_assert(std::move_constructible<chaistl::set<int>>);
static_assert(can_copy_insert<chaistl::set<int>>);
static_assert(can_move_insert<chaistl::set<int>>);

// set is a reversible container
static_assert(chaistl::concepts::reversible_container<chaistl::set<int>>);

// set is NOT a sequence container (no push_back, no operator[])
static_assert(!chaistl::concepts::sequence_container<chaistl::set<int>>);

// Move-only types work
static_assert(std::move_constructible<chaistl::set<std::unique_ptr<int>>>);
static_assert(can_move_insert<chaistl::set<std::unique_ptr<int>>>);

// [set.overview]: set's iterator and const_iterator are both constant
// iterators — keys must not be mutable in place, or the ordering invariant
// could be silently broken.
static_assert(std::same_as<chaistl::set<int>::iterator, chaistl::set<int>::const_iterator>);
static_assert(std::is_const_v<std::remove_reference_t<std::iter_reference_t<chaistl::set<int>::iterator>>>);
static_assert(!std::output_iterator<chaistl::set<int>::iterator, int>);
static_assert(std::bidirectional_iterator<chaistl::set<int>::iterator>);
