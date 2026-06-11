// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/iterator_legacy.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <type_traits>
#include <utility>

namespace chaistl {

namespace detail {

template <class T>
concept comparison_category = std::same_as<T, std::strong_ordering> || std::same_as<T, std::weak_ordering> ||
                              std::same_as<T, std::partial_ordering>;

}  // namespace detail

template <concepts::legacy::input_iterator InputIt1, concepts::legacy::input_iterator InputIt2, class Compare>
  requires requires(InputIt1 first1, InputIt2 first2, Compare compare) { compare(*first1, *first2); }
[[nodiscard]] constexpr auto lexicographical_compare_three_way(
    InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, Compare compare) {
  using result_type = decltype(compare(*first1, *first2));
  static_assert(detail::comparison_category<result_type>, "comparison result must be a comparison category type");
  return std::lexicographical_compare_three_way(first1, last1, first2, last2, compare);
}

template <concepts::legacy::input_iterator InputIt1, concepts::legacy::input_iterator InputIt2>
[[nodiscard]] constexpr auto lexicographical_compare_three_way(InputIt1 first1,
                                                               InputIt1 last1,
                                                               InputIt2 first2,
                                                               InputIt2 last2) {
  return chaistl::lexicographical_compare_three_way(first1, last1, first2, last2, std::compare_three_way{});
}

/**
 * @brief Generic three-way comparison for forward ranges.
 *
 * Compares two ranges lexicographically. If the value type supports <=>,
 * uses std::lexicographical_compare_three_way for efficiency. Otherwise
 * falls back to element-wise compare_three_way or operator<.
 *
 * This is the default implementation used by all sequence containers
 * that need operator<=>.
 */
template <std::ranges::forward_range Range>
  requires requires(const std::ranges::range_value_t<Range>& lhs, const std::ranges::range_value_t<Range>& rhs) {
    { lhs < rhs } -> std::convertible_to<bool>;
    { rhs < lhs } -> std::convertible_to<bool>;
  }
[[nodiscard]] constexpr auto default_three_way_compare(const Range& lhs, const Range& rhs) {
  using T = std::ranges::range_value_t<Range>;
  auto first1 = std::ranges::begin(lhs);
  auto last1 = std::ranges::end(lhs);
  auto first2 = std::ranges::begin(rhs);
  auto last2 = std::ranges::end(rhs);

  if constexpr (std::three_way_comparable_with<T, T>) {
    return std::lexicographical_compare_three_way(first1, last1, first2, last2);
  } else {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
      if (*first1 < *first2) {
        return std::strong_ordering::less;
      }
      if (*first2 < *first1) {
        return std::strong_ordering::greater;
      }
    }
    if (first1 == last1 && first2 == last2) {
      return std::strong_ordering::equal;
    }
    return (first1 == last1) ? std::strong_ordering::less : std::strong_ordering::greater;
  }
}

}  // namespace chaistl

namespace chaistl::detail {

template <class T, class U>
concept synth_three_way_comparable =
    requires(const std::remove_reference_t<T>& lhs, const std::remove_reference_t<U>& rhs) {
      lhs < rhs;
      rhs < lhs;
    };

struct synth_three_way {
  template <class T, class U>
    requires requires(const std::remove_reference_t<T>& lhs, const std::remove_reference_t<U>& rhs) { lhs <=> rhs; } ||
             synth_three_way_comparable<T, U>
  [[nodiscard]] constexpr auto operator()(const T& lhs, const U& rhs) const {
    if constexpr (requires { lhs <=> rhs; }) {
      return lhs <=> rhs;
    } else {
      if (lhs < rhs) {
        return std::weak_ordering::less;
      }
      if (rhs < lhs) {
        return std::weak_ordering::greater;
      }
      return std::weak_ordering::equivalent;
    }
  }
};

}  // namespace chaistl::detail
