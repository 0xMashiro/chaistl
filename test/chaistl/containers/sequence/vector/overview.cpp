// SPDX-License-Identifier: Apache-2.0

// References:
// - std::vector member types and container requirements:
//   https://en.cppreference.com/w/cpp/container/vector
//   https://eel.is/c++draft/vector.overview
// - Test organization is inspired by libc++'s standard-conformance tests and
//   libstdc++'s chapter-based testsuite layout. The test cases here are written
//   for chaistl and are not copied from either implementation.

#include <chaistl/concepts.hpp>
#include <chaistl/containers/vector.hpp>
#include <chaistl/memory/allocator.hpp>

#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>

namespace {

template <class Vector>
concept can_copy_push_back =
    requires(Vector& values, const typename Vector::value_type& value) { values.push_back(value); };

template <class Vector>
concept can_move_push_back =
    requires(Vector& values, typename Vector::value_type value) { values.push_back(std::move(value)); };

}  // namespace

static_assert(std::ranges::range<chaistl::vector<int>>);
static_assert(std::ranges::sized_range<chaistl::vector<int>>);
static_assert(std::ranges::contiguous_range<chaistl::vector<int>>);
static_assert(std::contiguous_iterator<chaistl::vector<int>::iterator>);
static_assert(std::same_as<chaistl::vector<int>::value_type, int>);
static_assert(std::same_as<chaistl::vector<int>::allocator_type, chaistl::allocator<int>>);
static_assert(std::same_as<decltype(std::declval<chaistl::vector<int>&>().data()), int*>);
static_assert(std::same_as<decltype(std::declval<const chaistl::vector<int>&>().data()), const int*>);

static_assert(chaistl::concepts::sequence_container<chaistl::vector<int>>);
static_assert(chaistl::concepts::back_sequence_container<chaistl::vector<int>>);
static_assert(!chaistl::concepts::front_sequence_container<chaistl::vector<int>>);
static_assert(chaistl::concepts::random_access_sequence_container<chaistl::vector<int>>);

// Note: std::vector<std::unique_ptr<int>> is copy_constructible in the type
// system (the copy constructor always exists), but copying it would be UB
// because std::unique_ptr is not CopyInsertable.  The standard describes this
// as a precondition, not as a constraint that removes the overload.
static_assert(std::copy_constructible<chaistl::vector<std::unique_ptr<int>>>);
static_assert(can_move_push_back<chaistl::vector<std::unique_ptr<int>>>);
// push_back(const T&) always exists; whether it compiles depends on whether
// T is copyable.  The standard does not remove this overload via constraint.
static_assert(can_copy_push_back<chaistl::vector<std::unique_ptr<int>>>);
