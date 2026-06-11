// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/meta/type_traits.hpp>

#include <type_traits>
#include <utility>

namespace chaistl::detail {

// ----------------------------------------------------------------------
// Workaround for Clang-22 + libstdc++-14: std::forward_like has a deduced
// return type that cannot be used before it is defined when called inside
// a deducing-this member function with auto&& return type.
//
// This helper re-implements std::forward_like semantics using an explicit
// return type so the compiler never needs to see the deduced return type
// of std::forward_like before its definition.
// ----------------------------------------------------------------------

/**
 * @brief Re-implementation of std::forward_like with an explicit return type.
 *
 * Semantically identical to std::forward_like<Like>(value), but avoids a
 * compiler bug where the deduced return type of std::forward_like cannot be
 * used before it is defined in certain deducing-this contexts.
 */
template <class Like, class T>
[[nodiscard]] constexpr meta::copy_cvref_t<Like, T> forward_like(T&& value) noexcept {
  return static_cast<meta::copy_cvref_t<Like, T>>(std::forward<T>(value));
}

}  // namespace chaistl::detail
