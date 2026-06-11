// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <iterator>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace chaistl::detail {
namespace set_map_deduction {

// Helper aliases for map/multimap deduction guides, matching the
// exposition-only helpers used by the C++23 standard (iter-key-type,
// range-key-t, ...). The allocator aliases re-add const to the key:
// deducing from pair<int, int> elements must still produce the
// container's allocator<pair<const int, int>>, or the guide's result
// type fails the class constraints.
template <class InputIt>
using iter_key_t = std::remove_const_t<std::tuple_element_t<0, typename std::iterator_traits<InputIt>::value_type>>;

template <class InputIt>
using iter_mapped_t = std::tuple_element_t<1, typename std::iterator_traits<InputIt>::value_type>;

template <class InputIt>
using iter_alloc_t = std::pair<std::add_const_t<iter_key_t<InputIt>>, iter_mapped_t<InputIt>>;

template <class R>
using range_key_t = std::tuple_element_t<0, std::ranges::range_value_t<R>>;

template <class R>
using range_mapped_t = std::tuple_element_t<1, std::ranges::range_value_t<R>>;

template <class R>
using range_alloc_t = std::pair<std::add_const_t<range_key_t<R>>, range_mapped_t<R>>;

}  // namespace set_map_deduction
}  // namespace chaistl::detail
