// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/iterator_legacy.hpp>

#include <concepts>
#include <iterator>
#include <type_traits>
#include <utility>

namespace chaistl::concepts {

// Reference:
// - https://eel.is/c++draft/swappable.requirements
// - https://en.cppreference.com/w/cpp/named_req/Swappable
// - https://en.cppreference.com/w/cpp/named_req/ValueSwappable
//
// Semantic boundary: concepts can check that unqualified swap is callable with
// std::swap visible, but cannot prove the before/after value exchange relation.
namespace detail {

using std::swap;

template <class T, class U>
concept unqualified_swappable_with = requires(T&& lhs, U&& rhs) { swap(std::forward<T>(lhs), std::forward<U>(rhs)); };

}  // namespace detail

template <class T, class U>
concept swappable_with = detail::unqualified_swappable_with<T, U> && detail::unqualified_swappable_with<U, T>;

template <class T>
concept swappable = swappable_with<T&, T&>;

template <class I>
concept value_swappable =
    legacy::iterator<I> && requires(I lhs, I rhs) { requires swappable_with<decltype(*lhs), decltype(*rhs)>; };

}  // namespace chaistl::concepts
