// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list member types and container requirements:
//   https://en.cppreference.com/w/cpp/container/list
//   https://eel.is/c++draft/list.overview

#include <chaistl/concepts.hpp>
#include <chaistl/containers/list.hpp>
#include <chaistl/memory/allocator.hpp>

#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>

namespace {

template <class List>
concept can_copy_push_back =
    requires(List& values, const typename List::value_type& value) { values.push_back(value); };

template <class List>
concept can_move_push_back =
    requires(List& values, typename List::value_type value) { values.push_back(std::move(value)); };

}  // namespace

static_assert(std::ranges::range<chaistl::list<int>>);
static_assert(std::ranges::sized_range<chaistl::list<int>>);
static_assert(!std::ranges::contiguous_range<chaistl::list<int>>);
static_assert(std::same_as<chaistl::list<int>::value_type, int>);
static_assert(std::same_as<chaistl::list<int>::allocator_type, chaistl::allocator<int>>);

static_assert(chaistl::concepts::sequence_container<chaistl::list<int>>);
static_assert(chaistl::concepts::front_sequence_container<chaistl::list<int>>);
static_assert(chaistl::concepts::back_sequence_container<chaistl::list<int>>);
static_assert(!chaistl::concepts::random_access_sequence_container<chaistl::list<int>>);

static_assert(std::copy_constructible<chaistl::list<std::unique_ptr<int>>>);
static_assert(can_move_push_back<chaistl::list<std::unique_ptr<int>>>);
static_assert(can_copy_push_back<chaistl::list<std::unique_ptr<int>>>);
