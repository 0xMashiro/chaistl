// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/basic.hpp>

#include <concepts>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::concepts {

// Reference:
// - https://en.cppreference.com/w/cpp/iterator/input_or_output_iterator
// - https://en.cppreference.com/w/cpp/iterator/input_iterator
// - https://en.cppreference.com/w/cpp/iterator/forward_iterator
// - https://en.cppreference.com/w/cpp/iterator/bidirectional_iterator
// - https://en.cppreference.com/w/cpp/iterator/random_access_iterator
// - https://en.cppreference.com/w/cpp/iterator/contiguous_iterator
// - https://eel.is/c++draft/iterator.concepts
//
// These concepts mirror the C++20 iterator concept refinement chain. They are
// learning-facing concepts: each stronger iterator category is expressed in
// terms of the previous one plus the additional operations documented by the
// standard.
namespace iterator_detail {

template <class T>
concept boolean_testable = std::convertible_to<T, bool>;

template <class I>
concept has_member_iterator_concept = requires { typename I::iterator_concept; };

template <class I>
concept has_traits_iterator_concept = requires { typename std::iterator_traits<I>::iterator_concept; };

template <class I>
concept has_traits_iterator_category = requires { typename std::iterator_traits<I>::iterator_category; };

template <class I>
concept has_any_iter_concept_source =
    has_member_iterator_concept<I> || has_traits_iterator_concept<I> || has_traits_iterator_category<I>;

template <class I, class Tag>
concept iter_concept_derived_from =
    (has_member_iterator_concept<I> && std::derived_from<typename I::iterator_concept, Tag>) ||
    (!has_member_iterator_concept<I> && has_traits_iterator_concept<I> &&
     std::derived_from<typename std::iterator_traits<I>::iterator_concept, Tag>) ||
    (!has_member_iterator_concept<I> && !has_traits_iterator_concept<I> && has_traits_iterator_category<I> &&
     std::derived_from<typename std::iterator_traits<I>::iterator_category, Tag>) ||
    (!has_any_iter_concept_source<I> && std::derived_from<std::random_access_iterator_tag, Tag>);

// weakly-equality-comparable-with is exposition-only in the standard. This is
// the syntax-level form needed by sentinel_for; equality-preserving semantics
// are documented requirements that concepts cannot fully verify.
template <class T, class U>
concept weakly_equality_comparable_with =
    requires(const std::remove_reference_t<T>& lhs, const std::remove_reference_t<U>& rhs) {
      { lhs == rhs } -> boolean_testable;
      { lhs != rhs } -> boolean_testable;
      { rhs == lhs } -> boolean_testable;
      { rhs != lhs } -> boolean_testable;
    };

template <class I>
concept indirectly_readable_impl =
    requires(const I iterator) {
      typename std::iter_value_t<I>;
      typename std::iter_reference_t<I>;
      typename std::iter_rvalue_reference_t<I>;
      { *iterator } -> std::same_as<std::iter_reference_t<I>>;
      { std::ranges::iter_move(iterator) } -> std::same_as<std::iter_rvalue_reference_t<I>>;
    } && std::common_reference_with<std::iter_reference_t<I>&&, std::iter_value_t<I>&> &&
    std::common_reference_with<std::iter_reference_t<I>&&, std::iter_rvalue_reference_t<I>&&> &&
    std::common_reference_with<std::iter_rvalue_reference_t<I>&&, const std::iter_value_t<I>&>;

template <class T>
concept signed_integer_like = std::signed_integral<T>;

}  // namespace iterator_detail

template <class I>
concept indirectly_readable = iterator_detail::indirectly_readable_impl<std::remove_cvref_t<I>>;

template <class O, class T>
concept indirectly_writable = requires(O&& output, T&& value) {
  *output = std::forward<T>(value);
  *std::forward<O>(output) = std::forward<T>(value);
  const_cast<const std::iter_reference_t<O>&&>(*output) = std::forward<T>(value);
  const_cast<const std::iter_reference_t<O>&&>(*std::forward<O>(output)) = std::forward<T>(value);
};

template <class I>
concept weakly_incrementable = std::movable<I> && requires(I iterator) {
  typename std::iter_difference_t<I>;
  requires iterator_detail::signed_integer_like<std::iter_difference_t<I>>;
  { ++iterator } -> std::same_as<I&>;
  iterator++;
};

template <class I>
concept incrementable = std::regular<I> && weakly_incrementable<I> && requires(I iterator) {
  { iterator++ } -> std::same_as<I>;
};

template <class I>
concept input_or_output_iterator = requires(I iterator) {
  { *iterator } -> referenceable;
} && weakly_incrementable<I>;

template <class S, class I>
concept sentinel_for =
    std::semiregular<S> && input_or_output_iterator<I> && iterator_detail::weakly_equality_comparable_with<S, I>;

template <class S, class I>
inline constexpr bool disable_sized_sentinel_for = false;

template <class S, class I>
concept sized_sentinel_for =
    sentinel_for<S, I> && !disable_sized_sentinel_for<std::remove_cv_t<S>, std::remove_cv_t<I>> &&
    requires(const I& iterator, const S& sentinel) {
      { sentinel - iterator } -> std::same_as<std::iter_difference_t<I>>;
      { iterator - sentinel } -> std::same_as<std::iter_difference_t<I>>;
    };

template <class I>
concept input_iterator = input_or_output_iterator<I> && indirectly_readable<I> &&
                         iterator_detail::iter_concept_derived_from<I, std::input_iterator_tag>;

template <class I, class T>
concept output_iterator = input_or_output_iterator<I> && indirectly_writable<I, T> &&
                          requires(I iterator, T&& value) { *iterator++ = std::forward<T>(value); };

template <class I>
concept forward_iterator =
    input_iterator<I> && iterator_detail::iter_concept_derived_from<I, std::forward_iterator_tag> && incrementable<I> &&
    sentinel_for<I, I>;

template <class I>
concept bidirectional_iterator =
    forward_iterator<I> && iterator_detail::iter_concept_derived_from<I, std::bidirectional_iterator_tag> &&
    requires(I iterator) {
      { --iterator } -> std::same_as<I&>;
      { iterator-- } -> std::same_as<I>;
    };

template <class I>
concept random_access_iterator =
    bidirectional_iterator<I> && iterator_detail::iter_concept_derived_from<I, std::random_access_iterator_tag> &&
    std::totally_ordered<I> && sized_sentinel_for<I, I> &&
    requires(I iterator, const I const_iterator, const std::iter_difference_t<I> offset) {
      { iterator += offset } -> std::same_as<I&>;
      { const_iterator + offset } -> std::same_as<I>;
      { offset + const_iterator } -> std::same_as<I>;
      { iterator -= offset } -> std::same_as<I&>;
      { const_iterator - offset } -> std::same_as<I>;
      { const_iterator[offset] } -> std::same_as<std::iter_reference_t<I>>;
    };

template <class I>
concept contiguous_iterator =
    random_access_iterator<I> && iterator_detail::iter_concept_derived_from<I, std::contiguous_iterator_tag> &&
    std::is_lvalue_reference_v<std::iter_reference_t<I>> &&
    std::same_as<std::iter_value_t<I>, std::remove_cvref_t<std::iter_reference_t<I>>> && requires(const I& iterator) {
      { std::to_address(iterator) } -> std::same_as<std::add_pointer_t<std::iter_reference_t<I>>>;
    };

}  // namespace chaistl::concepts
