// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list member types and container requirements:
//   https://en.cppreference.com/w/cpp/container/forward_list
//   https://eel.is/c++draft/forwardlist.overview

#include <chaistl/concepts.hpp>
#include <chaistl/containers/forward_list.hpp>
#include <chaistl/memory/allocator.hpp>

#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>

namespace {

template <class List>
concept can_copy_push_front =
    requires(List& values, const typename List::value_type& value) { values.push_front(value); };

template <class List>
concept can_move_push_front =
    requires(List& values, typename List::value_type value) { values.push_front(std::move(value)); };

}  // namespace

static_assert(std::ranges::range<chaistl::forward_list<int>>);
static_assert(std::ranges::sized_range<chaistl::forward_list<int>>);
static_assert(!std::ranges::contiguous_range<chaistl::forward_list<int>>);
static_assert(!std::ranges::bidirectional_range<chaistl::forward_list<int>>);
static_assert(std::same_as<chaistl::forward_list<int>::value_type, int>);
static_assert(std::same_as<chaistl::forward_list<int>::allocator_type, chaistl::allocator<int>>);

// forward_list satisfies the base container concept.
static_assert(chaistl::concepts::container<chaistl::forward_list<int>>);
static_assert(chaistl::concepts::allocator_aware_container<chaistl::forward_list<int>>);

// forward_list does NOT satisfy sequence_container or the higher-level
// refining concepts because it uses insert_after/emplace_after semantics
// instead of the standard insert/emplace signatures.  This is a known
// limitation of the chaistl concept hierarchy: the standard treats
// forward_list as a SequenceContainer, but the concept checks for exact
// signatures.  A forward_list-specific concept would be needed.
static_assert(!chaistl::concepts::sequence_container<chaistl::forward_list<int>>);
static_assert(!chaistl::concepts::front_sequence_container<chaistl::forward_list<int>>);
static_assert(!chaistl::concepts::back_sequence_container<chaistl::forward_list<int>>);
static_assert(!chaistl::concepts::random_access_sequence_container<chaistl::forward_list<int>>);
static_assert(!chaistl::concepts::reversible_container<chaistl::forward_list<int>>);

static_assert(std::copy_constructible<chaistl::forward_list<std::unique_ptr<int>>>);
static_assert(can_move_push_front<chaistl::forward_list<std::unique_ptr<int>>>);
static_assert(can_copy_push_front<chaistl::forward_list<std::unique_ptr<int>>>);

static_assert(std::is_same_v<chaistl::forward_list<int>::iterator::iterator_category, std::forward_iterator_tag>);
static_assert(std::is_same_v<chaistl::forward_list<int>::const_iterator::iterator_category, std::forward_iterator_tag>);
