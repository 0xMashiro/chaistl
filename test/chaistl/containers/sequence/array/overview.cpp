// SPDX-License-Identifier: Apache-2.0

// References:
// - std::array overview and member types:
//   https://en.cppreference.com/w/cpp/container/array
//   https://eel.is/c++draft/array.overview
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <chaistl/concepts.hpp>
#include <chaistl/containers/array.hpp>

#include <concepts>
#include <ranges>
#include <string>
#include <tuple>
#include <type_traits>

namespace {

struct non_assignable {
  non_assignable() = default;
  non_assignable& operator=(const non_assignable&) = delete;
};

struct non_swappable {
  non_swappable() = default;
  non_swappable(const non_swappable&) = delete;
  non_swappable& operator=(const non_swappable&) = delete;
};

struct no_default {
  no_default() = delete;
  explicit no_default(int value) : value(value) {}

  int value;
};

template <class A>
concept can_fill = requires(A values, const typename A::value_type& value) { values.fill(value); };

template <class A>
concept can_member_swap = requires(A lhs, A rhs) { lhs.swap(rhs); };

template <class A>
concept can_adl_swap = requires(A lhs, A rhs) { swap(lhs, rhs); };

}  // namespace

static_assert(chaistl::concepts::container<chaistl::array<int, 3>>);
// std::array is listed with sequence containers, and it partially satisfies the
// SequenceContainer named requirement, but it is fixed-size and has no
// insert/erase/assign operations.
static_assert(!chaistl::concepts::sequence_container<chaistl::array<int, 3>>);
static_assert(!chaistl::concepts::front_sequence_container<chaistl::array<int, 3>>);
static_assert(!chaistl::concepts::back_sequence_container<chaistl::array<int, 3>>);
static_assert(!chaistl::concepts::random_access_sequence_container<chaistl::array<int, 3>>);
static_assert(chaistl::concepts::reversible_container<chaistl::array<int, 3>>);
static_assert(chaistl::concepts::contiguous_container<chaistl::array<int, 3>>);
static_assert(std::ranges::contiguous_range<chaistl::array<int, 3>>);
static_assert(std::contiguous_iterator<chaistl::array<int, 3>::iterator>);
static_assert(std::same_as<chaistl::array<int, 3>::value_type, int>);
static_assert(std::tuple_size_v<chaistl::array<int, 3>> == 3);
static_assert(std::same_as<std::tuple_element_t<0, chaistl::array<int, 3>>, int>);
static_assert(std::same_as<chaistl::array<bool, 3>::reference, bool&>);
static_assert(std::ranges::contiguous_range<chaistl::array<bool, 3>>);
static_assert(std::ranges::contiguous_range<chaistl::array<int, 0>>);
static_assert(chaistl::array<int, 0>{}.empty());
static_assert(chaistl::array<int, 0>{}.size() == 0);
static_assert(std::default_initializable<chaistl::array<no_default, 0>>);
static_assert(!std::default_initializable<chaistl::array<no_default, 1>>);

static_assert(can_fill<chaistl::array<int, 3>>);
static_assert(can_fill<chaistl::array<std::string, 2>>);
static_assert(can_fill<chaistl::array<int, 0>>);
static_assert(!can_fill<chaistl::array<const int, 0>>);
static_assert(!can_fill<chaistl::array<non_assignable, 2>>);
static_assert(can_member_swap<chaistl::array<int, 3>>);
static_assert(can_adl_swap<chaistl::array<int, 3>>);
static_assert(can_member_swap<chaistl::array<int, 0>>);
static_assert(can_adl_swap<chaistl::array<int, 0>>);
static_assert(!can_member_swap<chaistl::array<const int, 0>>);
static_assert(!can_adl_swap<chaistl::array<const int, 0>>);
static_assert(!can_member_swap<chaistl::array<non_swappable, 2>>);
static_assert(!can_adl_swap<chaistl::array<non_swappable, 2>>);
