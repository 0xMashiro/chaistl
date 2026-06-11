// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>
#include <tuple>
#include <utility>

namespace chaistl::detail::associative {

/**
 * @brief Key extractor for set-like containers where the key is the value itself.
 *
 * Shared by the tree and hash cores; each layer re-exports it into its own
 * namespace (detail::tree / detail::hash) so call sites stay local.
 *
 * @tparam T The element type (same as the key type).
 */
template <class T>
struct identity {
  using key_type = T;

  [[nodiscard]] constexpr const T& operator()(const T& value) const noexcept { return value; }

  [[nodiscard]] constexpr T& operator()(T& value) const noexcept { return value; }
};

/**
 * @brief Key extractor for map-like containers where the value is a pair-like
 *        type such as @c std::pair<const Key, Mapped>.
 *
 * Uses @c std::tuple_element and @c std::get<0> so the extractor also works
 * for @c std::tuple and other pair-like types, not just @c std::pair.
 *
 * @tparam Pair The pair-like type.
 */
template <class Pair>
struct select1st {
  using key_type = std::tuple_element_t<0, Pair>;

  template <class P>
    requires requires(const P& p) {
      { std::get<0>(p) } -> std::convertible_to<const key_type&>;
    }
  [[nodiscard]] constexpr const key_type& operator()(const P& pair) const noexcept {
    return std::get<0>(pair);
  }

  template <class P>
    requires requires(P& p) {
      { std::get<0>(p) } -> std::convertible_to<key_type&>;
    }
  [[nodiscard]] constexpr key_type& operator()(P& pair) const noexcept {
    return std::get<0>(pair);
  }
};

}  // namespace chaistl::detail::associative
