// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <compare>
#include <concepts>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl {

// ============================================================================
// iterator_interface — P2727-style deducing-this iterator facade.
//
// Derived classes provide only the core primitives:
//   - operator*()          (all iterators)
//   - operator++()         (all iterators)
//   - operator--()         (bidirectional / random_access)
//   - operator+=()         (random_access)
//   - operator-=(other)    (random_access, optional, default derived from +=)
//   - operator-(other)     (random_access, distance between two iterators)
//   - operator<=>()        (random_access)
//
// The base class deduces the rest via deducing this:
//   - operator++(int)      derived from ++
//   - operator--(int)      derived from --
//   - operator->()         derived from *
//   - operator+()          derived from +=
//   - operator-()          derived from -=
//   - operator[]()         derived from + and *
//   - operator!=()         C++20 default from == (no explicit code needed)
//
// Note: non-member operator+(n, it) cannot be deduced from the member
// operator+(it, n) because deducing-this members are not found by ADL.
// Random-access iterators that need symmetric operator+ should provide
// their own hidden friend or non-member overload.
// ============================================================================

struct iterator_interface {
 private:
  template <class Self>
  using self_t = std::remove_cvref_t<Self>;

  template <class Self>
  using difference_t = typename self_t<Self>::difference_type;

 public:
  // Deduced from operator*().
  //
  // Note: This only works when operator* returns a real reference (T& or
  // const T&). Proxy-reference iterators (e.g. vector<bool>) are not
  // supported by this facade.
  [[nodiscard]] constexpr auto operator->(this const auto& self) noexcept(noexcept(std::addressof(*self)))
    requires requires { std::addressof(*self); }
  {
    return std::addressof(*self);
  }

  // Deduced from operator++().
  //
  // Note: P3668 (Defaulting Postfix Increment/Decrement, target C++29)
  // proposes making operator++(int) defaultable at the language level:
  //   MyIter operator++(int) = default;
  // When compiler support arrives, derived classes can use = default
  // directly and this member may be retired.
  // Not [[nodiscard]]: `it++;` as a discarded-value statement is idiomatic,
  // and the standard library does not mark postfix increment nodiscard.
  constexpr auto operator++(this auto& self, int) noexcept(noexcept(++self) &&
                                                           std::is_nothrow_copy_constructible_v<self_t<decltype(self)>>)
    requires(std::copy_constructible<self_t<decltype(self)>> && requires { ++self; })
  {
    auto tmp = self;
    ++self;
    return tmp;
  }

  // Deduced from operator--().
  //
  // Note: See operator++(int) above for P3668 (C++29) future direction.
  constexpr auto operator--(this auto& self, int) noexcept(noexcept(--self) &&
                                                           std::is_nothrow_copy_constructible_v<self_t<decltype(self)>>)
    requires(std::copy_constructible<self_t<decltype(self)>> && requires { --self; })
  {
    auto tmp = self;
    --self;
    return tmp;
  }

  // Deduced from operator+=().
  //
  // We use the derived class's nested difference_type instead of
  // std::iter_difference_t to avoid constraint recursion in some
  // standard-library implementations (iter_difference_t may inspect
  // operator+ to determine the difference type).
  [[nodiscard]] constexpr auto operator+(this auto self, difference_t<decltype(self)> n) noexcept(noexcept(self += n))
    requires requires { self += n; }
  {
    self += n;
    return self;
  }

  // Deduced from operator-=().
  [[nodiscard]] constexpr auto operator-(this auto self, difference_t<decltype(self)> n) noexcept(noexcept(self -= n))
    requires requires { self -= n; }
  {
    self -= n;
    return self;
  }

  // Deduced from operator+() and operator*().
  [[nodiscard]] constexpr decltype(auto) operator[](this const auto& self,
                                                    difference_t<decltype(self)> n) noexcept(noexcept(*(self + n)))
    requires requires { *(self + n); }
  {
    return *(self + n);
  }
};

}  // namespace chaistl
