// SPDX-License-Identifier: Apache-2.0

// References:
// - C++ named requirements:
//   https://en.cppreference.com/w/cpp/named_req
// - CopyConstructible:
//   https://en.cppreference.com/w/cpp/named_req/CopyConstructible
// - Swappable requirements:
//   https://eel.is/c++draft/swappable.requirements
// - The checks are chaistl learning concepts, not copied standard-library
//   implementation tests.

#include <chaistl/concepts.hpp>

#include <array>
#include <iterator>
#include <memory>
#include <string>

struct move_only {
  move_only() = default;
  move_only(const move_only&) = delete;
  move_only(move_only&&) = default;
};

struct no_default {
  no_default() = delete;
};

struct no_destructor {
  ~no_destructor() = delete;
};

struct explicit_copy {
  explicit_copy() = default;
  explicit explicit_copy(const explicit_copy&) {}
  explicit_copy(explicit_copy&&) = default;
};

struct no_const_rvalue_assignment {
  no_const_rvalue_assignment() = default;
  no_const_rvalue_assignment& operator=(no_const_rvalue_assignment&&) = default;
  no_const_rvalue_assignment& operator=(no_const_rvalue_assignment&) { return *this; }
  no_const_rvalue_assignment& operator=(const no_const_rvalue_assignment&) { return *this; }
  no_const_rvalue_assignment& operator=(const no_const_rvalue_assignment&&) = delete;
};

struct adl_swappable {
  friend void swap(adl_swappable&, adl_swappable&) noexcept {}
};

struct nonassignable_adl_swappable {
  nonassignable_adl_swappable& operator=(nonassignable_adl_swappable) = delete;

  friend void swap(nonassignable_adl_swappable&, nonassignable_adl_swappable&) noexcept {}
};

namespace adl {

struct value {
  int member{};
};

struct proxy {
  value* target{};
};

void swap(value& lhs, proxy rhs) {
  std::swap(lhs.member, rhs.target->member);
}

void swap(proxy lhs, value& rhs) {
  swap(rhs, lhs);
}

}  // namespace adl

static_assert(chaistl::concepts::referenceable<int>);
static_assert(chaistl::concepts::referenceable<int&>);
static_assert(!chaistl::concepts::referenceable<void>);

static_assert(chaistl::concepts::default_constructible<int>);
static_assert(chaistl::concepts::default_constructible<std::string>);
static_assert(!chaistl::concepts::default_constructible<no_default>);
static_assert(!chaistl::concepts::default_constructible<no_destructor>);
static_assert(!chaistl::concepts::default_constructible<move_only&>);

static_assert(chaistl::concepts::move_constructible<int>);
static_assert(chaistl::concepts::move_constructible<std::unique_ptr<int>>);
static_assert(!chaistl::concepts::move_constructible<no_destructor>);
static_assert(chaistl::concepts::copy_constructible<int>);
static_assert(chaistl::concepts::copy_constructible<std::string>);
static_assert(!chaistl::concepts::copy_constructible<std::unique_ptr<int>>);
static_assert(!chaistl::concepts::copy_constructible<move_only>);
static_assert(!chaistl::concepts::copy_constructible<explicit_copy>);

static_assert(chaistl::concepts::move_assignable<int>);
static_assert(chaistl::concepts::copy_assignable<int>);
static_assert(chaistl::concepts::copy_assignable<std::string>);
static_assert(!chaistl::concepts::copy_assignable<std::unique_ptr<int>>);
static_assert(!chaistl::concepts::copy_assignable<move_only>);
static_assert(!chaistl::concepts::copy_assignable<no_const_rvalue_assignment>);

static_assert(std::destructible<int>);
static_assert(!std::destructible<no_destructor>);
static_assert(chaistl::concepts::swappable<int>);
static_assert(chaistl::concepts::swappable<std::string>);
static_assert(chaistl::concepts::swappable<adl_swappable>);
static_assert(chaistl::concepts::swappable<nonassignable_adl_swappable>);
static_assert(chaistl::concepts::swappable_with<int&, int&>);
static_assert(chaistl::concepts::swappable_with<adl::value&, adl::proxy>);
static_assert(chaistl::concepts::swappable_with<adl::proxy, adl::value&>);
static_assert(!chaistl::concepts::swappable_with<adl::value&, std::string&>);
static_assert(chaistl::concepts::value_swappable<int*>);
static_assert(chaistl::concepts::value_swappable<std::array<int, 2>::iterator>);

static_assert(chaistl::concepts::default_constructible<int> == std::default_initializable<int>);
static_assert(chaistl::concepts::move_constructible<int> == std::move_constructible<int>);
static_assert(chaistl::concepts::move_constructible<std::unique_ptr<int>> ==
              std::move_constructible<std::unique_ptr<int>>);
static_assert(chaistl::concepts::copy_constructible<int> == std::copy_constructible<int>);
static_assert(chaistl::concepts::copy_constructible<std::string> == std::copy_constructible<std::string>);
static_assert(chaistl::concepts::copy_constructible<std::unique_ptr<int>> ==
              std::copy_constructible<std::unique_ptr<int>>);
