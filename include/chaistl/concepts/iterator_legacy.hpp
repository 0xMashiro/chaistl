// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/basic.hpp>

#include <concepts>
#include <iterator>
#include <memory>
#include <type_traits>

namespace chaistl::concepts::legacy {

// Reference:
// - https://en.cppreference.com/w/cpp/named_req/Iterator
// - https://en.cppreference.com/w/cpp/named_req/InputIterator
// - https://en.cppreference.com/w/cpp/named_req/OutputIterator
// - https://en.cppreference.com/w/cpp/named_req/ForwardIterator
// - https://en.cppreference.com/w/cpp/named_req/BidirectionalIterator
// - https://en.cppreference.com/w/cpp/named_req/RandomAccessIterator
// - https://en.cppreference.com/w/cpp/iterator/contiguous_iterator
//
// The legacy_* names correspond to cppreference's LegacyIterator family. They
// are useful for studying the pre-C++20 named requirements and for checking
// whether a handwritten iterator exposes the operations those pages describe.
namespace detail {

template <class I, class Tag>
concept iter_concept_derived_from =
    (requires { typename I::iterator_concept; } && std::derived_from<typename I::iterator_concept, Tag>) || (requires {
      typename std::iterator_traits<I>::iterator_concept;
    } && std::derived_from<typename std::iterator_traits<I>::iterator_concept, Tag>) || (requires {
      typename std::iterator_traits<I>::iterator_category;
    } && std::derived_from<typename std::iterator_traits<I>::iterator_category, Tag>);

}  // namespace detail

template <class I>
concept iterator = requires(I iterator) {
  { *iterator } -> referenceable;
  { ++iterator } -> std::same_as<I&>;
  { *iterator++ } -> referenceable;
} && std::copyable<I>;

template <class I, class T>
concept output_iterator = iterator<I> && requires(I iterator, T value) {
  *iterator = value;
  ++iterator;
  iterator++;
  *iterator++ = value;
};

template <class I>
concept input_iterator = iterator<I> && std::equality_comparable<I> && requires(I iterator) {
  typename std::incrementable_traits<I>::difference_type;
  typename std::indirectly_readable_traits<I>::value_type;
  typename std::common_reference_t<std::iter_reference_t<I>&&,
                                   typename std::indirectly_readable_traits<I>::value_type&>;
  *iterator++;
  typename std::common_reference_t<decltype(*iterator++)&&, typename std::indirectly_readable_traits<I>::value_type&>;
  requires std::signed_integral<typename std::incrementable_traits<I>::difference_type>;
};

template <class I>
concept forward_iterator =
    input_iterator<I> && std::default_initializable<I> && std::is_reference_v<std::iter_reference_t<I>> &&
    std::same_as<std::remove_cvref_t<std::iter_reference_t<I>>,
                 typename std::indirectly_readable_traits<I>::value_type> &&
    requires(I iterator) {
      { iterator++ } -> std::convertible_to<const I&>;
      { *iterator++ } -> std::same_as<std::iter_reference_t<I>>;
    };

template <class I>
concept bidirectional_iterator = forward_iterator<I> && requires(I iterator) {
  { --iterator } -> std::same_as<I&>;
  { iterator-- } -> std::convertible_to<const I&>;
  { *iterator-- } -> std::same_as<std::iter_reference_t<I>>;
};

template <class I>
concept random_access_iterator = bidirectional_iterator<I> && std::totally_ordered<I> &&
                                 requires(I iterator, typename std::incrementable_traits<I>::difference_type offset) {
                                   { iterator += offset } -> std::same_as<I&>;
                                   { iterator -= offset } -> std::same_as<I&>;
                                   { iterator + offset } -> std::same_as<I>;
                                   { offset + iterator } -> std::same_as<I>;
                                   { iterator - offset } -> std::same_as<I>;
                                   { iterator - iterator } -> std::same_as<decltype(offset)>;
                                   { iterator[offset] } -> std::convertible_to<std::iter_reference_t<I>>;
                                 };

template <class I>
concept contiguous_iterator =
    random_access_iterator<I> && detail::iter_concept_derived_from<I, std::contiguous_iterator_tag> &&
    std::is_lvalue_reference_v<std::iter_reference_t<I>> &&
    std::same_as<std::iter_value_t<I>, std::remove_cvref_t<std::iter_reference_t<I>>> && requires(const I& iterator) {
      { std::to_address(iterator) } -> std::same_as<std::add_pointer_t<std::iter_reference_t<I>>>;
    };

}  // namespace chaistl::concepts::legacy
