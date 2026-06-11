// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/basic.hpp>
#include <chaistl/concepts/container_compatible_range.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/concepts/iterator.hpp>
#include <chaistl/concepts/iterator_legacy.hpp>
#include <chaistl/concepts/library_wide.hpp>

#include <concepts>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

namespace chaistl::concepts {

namespace detail {

template <class C>
concept has_allocator_type = requires { typename C::allocator_type; };

template <class C>
concept copy_insertable_into_container =
    requires { typename C::value_type; } &&
    ((has_allocator_type<C> && copy_insertable<typename C::value_type, typename C::allocator_type>) ||
     (!has_allocator_type<C> && copy_constructible<typename C::value_type>));

template <class C>
concept container_copy_operations = (!copy_insertable_into_container<C>) || requires(C value, const C const_value) {
  { C(const_value) } -> std::same_as<C>;
  { value = const_value } -> std::same_as<C&>;
};

template <class C>
concept allocator_aware_copy_operations =
    (!copy_insertable_into_container<C>) || requires(const C const_value, const typename C::allocator_type allocator) {
      { C(const_value, allocator) } -> std::same_as<C>;
    };

}  // namespace detail

// ============================================================================
// Named Requirement: Container
// https://en.cppreference.com/w/cpp/named_req/Container
//
// Expressed as a C++20 concept. No direct std:: equivalent exists.
//
// These concepts are intentionally lightweight learning checks. The standard's
// named requirements include semantic guarantees that concepts cannot fully
// express, so tests still cover behavior such as invalidation, capacity growth,
// and exception safety.
//
// Also covers:
//   - ReversibleContainer: https://en.cppreference.com/w/cpp/named_req/ReversibleContainer
//   - ContiguousContainer: https://en.cppreference.com/w/cpp/named_req/ContiguousContainer
// ============================================================================
template <class C>
concept container =
    std::ranges::range<C> && std::default_initializable<C> && std::move_constructible<C> &&
    std::assignable_from<C&, C> && detail::container_copy_operations<C> && std::equality_comparable<C> &&
    swappable<C> && requires(C value, C other, const C const_value, C&& rvalue) {
      typename C::value_type;
      typename C::reference;
      typename C::const_reference;
      typename C::iterator;
      typename C::const_iterator;
      typename C::difference_type;
      typename C::size_type;

      requires std::same_as<typename C::reference, typename C::value_type&>;
      requires std::same_as<typename C::const_reference, const typename C::value_type&>;
      requires legacy::forward_iterator<typename C::iterator>;
      requires legacy::forward_iterator<typename C::const_iterator>;
      requires std::convertible_to<typename C::iterator, typename C::const_iterator>;
      requires std::same_as<typename C::difference_type, std::iter_difference_t<typename C::iterator>>;
      requires std::same_as<typename C::difference_type, std::iter_difference_t<typename C::const_iterator>>;
      requires std::unsigned_integral<typename C::size_type>;

      { C() } -> std::same_as<C>;
      { C(std::move(rvalue)) } -> std::same_as<C>;
      { value = std::move(rvalue) } -> std::same_as<C&>;
      { value.~C() } -> std::same_as<void>;
      { value.begin() } -> std::same_as<typename C::iterator>;
      { value.end() } -> std::same_as<typename C::iterator>;
      { const_value.begin() } -> std::same_as<typename C::const_iterator>;
      { const_value.end() } -> std::same_as<typename C::const_iterator>;
      { value.cbegin() } -> std::same_as<typename C::const_iterator>;
      { value.cend() } -> std::same_as<typename C::const_iterator>;
      { const_value == const_value } -> std::same_as<bool>;
      { const_value != const_value } -> std::same_as<bool>;
      { value.swap(other) } -> std::same_as<void>;
      { swap(value, other) } -> std::same_as<void>;
      { const_value.size() } -> std::same_as<typename C::size_type>;
      { const_value.max_size() } -> std::same_as<typename C::size_type>;
      { const_value.empty() } -> std::same_as<bool>;
    };

// ============================================================================
// Named Requirement: ReversibleContainer
// https://en.cppreference.com/w/cpp/named_req/ReversibleContainer
//
// Expressed as a C++20 concept. No std:: equivalent exists.
// ============================================================================
template <class C>
concept reversible_container = container<C> && requires(C value, const C const_value) {
  typename C::reverse_iterator;
  typename C::const_reverse_iterator;
  { value.rbegin() } -> std::same_as<typename C::reverse_iterator>;
  { value.rend() } -> std::same_as<typename C::reverse_iterator>;
  { const_value.rbegin() } -> std::same_as<typename C::const_reverse_iterator>;
  { const_value.rend() } -> std::same_as<typename C::const_reverse_iterator>;
};

// ============================================================================
// Named Requirement: ContiguousContainer
// https://en.cppreference.com/w/cpp/named_req/ContiguousContainer
//
// Expressed as a C++20 concept. No std:: equivalent exists.
// ============================================================================
template <class C>
concept contiguous_container =
    container<C> && random_access_iterator<typename C::iterator> &&
    random_access_iterator<typename C::const_iterator> && contiguous_iterator<typename C::iterator> &&
    contiguous_iterator<typename C::const_iterator>;

// ============================================================================
// Named Requirement: AllocatorAwareContainer
// https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer
//
// Expressed as a C++20 concept. No std:: equivalent exists.
// ============================================================================
template <class C>
concept allocator_aware_container =
    container<C> && detail::has_allocator_type<C> &&
    allocator_for<typename C::allocator_type, typename C::value_type> && detail::allocator_aware_copy_operations<C> &&
    requires(const C const_value, const typename C::allocator_type allocator) {
      { C(allocator) } -> std::same_as<C>;
      { const_value.get_allocator() } -> std::same_as<typename C::allocator_type>;
    };

namespace detail {

// std::initializer_list<T> stores copies of its elements, so it is usable
// only when T is copy-constructible.  The type name itself is well-formed
// for move-only T, but brace-initialization and any operation that creates
// an initializer_list are ill-formed.
template <class T>
concept initializer_list_compatible = std::copy_constructible<T>;

// Copy-dependent sequence-container operations are required only when
// the element type is copy-insertable.  This matches the standard's
// model: the members always exist, but calling them with a non-copyable
// T is a runtime precondition violation.  We use if constexpr to keep
// the concept satisfied for move-only types while still checking the
// copy-dependent forms for copyable types.
template <class C>
concept sequence_container_copy_operations = [] {
  if constexpr (copy_insertable_into_container<C>) {
    return requires(C value,
                    const C const_value,
                    typename C::const_iterator position,
                    typename C::size_type count,
                    typename C::value_type element) {
      { C(count, element) } -> std::same_as<C>;
      { value.assign(count, element) } -> std::same_as<void>;
      { value.insert(position, element) } -> std::same_as<typename C::iterator>;
      { value.insert(position, count, element) } -> std::same_as<typename C::iterator>;
    };
  } else {
    return true;
  }
}();

// initializer_list-based operations are required only when
// std::initializer_list<value_type> is actually usable (i.e. the element
// type is copy-constructible).
template <class C>
concept sequence_container_initializer_list_operations = [] {
  if constexpr (initializer_list_compatible<typename C::value_type>) {
    return requires(
        C value, typename C::const_iterator position, std::initializer_list<typename C::value_type> initializer_list) {
      { C(initializer_list) } -> std::same_as<C>;
      { value = initializer_list } -> std::same_as<C&>;
      { value.assign(initializer_list) } -> std::same_as<void>;
      { value.insert(position, initializer_list) } -> std::same_as<typename C::iterator>;
    };
  } else {
    return true;
  }
}();

}  // namespace detail

// ============================================================================
// Named Requirement: SequenceContainer
// https://en.cppreference.com/w/cpp/named_req/SequenceContainer
//
// Expressed as a C++20 concept. No std:: equivalent exists.
//
// sequence_container models the basic operations of the SequenceContainer
// named requirement. These are the operations required by all sequence
// containers except std::array (which only partially satisfies it).
//
// Extra operations such as front/back, push_front/back, pop_front/back,
// emplace_front/back, prepend_range/append_range, and operator[]/at are
// modeled by separate refining concepts below.
//
// Copy-dependent operations and initializer_list-based operations are checked
// conditionally. This lets the concept accept move-only element types while
// still validating the full interface for copyable types.
// ============================================================================
template <class C>
concept sequence_container = container<C> && detail::sequence_container_copy_operations<C> &&
                             detail::sequence_container_initializer_list_operations<C> &&
                             requires(C value,
                                      const C const_value,
                                      typename C::const_iterator position,
                                      typename C::const_iterator first,
                                      typename C::const_iterator last,
                                      typename C::value_type&& rvalue) {
                               { C(first, last) } -> std::same_as<C>;
                               { value.assign(first, last) } -> std::same_as<void>;
                               { value.emplace(position, std::move(rvalue)) } -> std::same_as<typename C::iterator>;
                               { value.insert(position, std::move(rvalue)) } -> std::same_as<typename C::iterator>;
                               { value.insert(position, first, last) } -> std::same_as<typename C::iterator>;
                               { value.erase(position) } -> std::same_as<typename C::iterator>;
                               { value.erase(first, last) } -> std::same_as<typename C::iterator>;
                               { value.clear() } -> std::same_as<void>;
                             };

// ============================================================================
// SequenceContainer refinement: front operations
// https://en.cppreference.com/w/cpp/named_req/SequenceContainer
//
// front_sequence_container refines sequence_container with operations that
// insert or remove elements at the front. Satisfied by deque, list, and
// forward_list (and basic_string in the standard, though not in chaistl).
// ============================================================================
template <class C>
concept front_sequence_container =
    sequence_container<C> &&
    requires(C value, const C const_value, typename C::value_type element, typename C::value_type&& rvalue) {
      { const_value.front() } -> std::same_as<typename C::const_reference>;
      { value.front() } -> std::same_as<typename C::reference>;
      { value.emplace_front(element) } -> std::same_as<typename C::reference>;
      { value.push_front(element) } -> std::same_as<void>;
      { value.push_front(std::move(rvalue)) } -> std::same_as<void>;
      { value.pop_front() } -> std::same_as<void>;
    };

// ============================================================================
// SequenceContainer refinement: back operations
// https://en.cppreference.com/w/cpp/named_req/SequenceContainer
//
// back_sequence_container refines sequence_container with operations that
// insert or remove elements at the back. Satisfied by vector, deque, and list
// (and basic_string and inplace_vector in the standard).
// ============================================================================
template <class C>
concept back_sequence_container =
    sequence_container<C> &&
    requires(C value, const C const_value, typename C::value_type element, typename C::value_type&& rvalue) {
      { const_value.back() } -> std::same_as<typename C::const_reference>;
      { value.back() } -> std::same_as<typename C::reference>;
      { value.emplace_back(element) } -> std::same_as<typename C::reference>;
      { value.push_back(element) } -> std::same_as<void>;
      { value.push_back(std::move(rvalue)) } -> std::same_as<void>;
      { value.pop_back() } -> std::same_as<void>;
    };

// ============================================================================
// SequenceContainer refinement: random access
// https://en.cppreference.com/w/cpp/named_req/SequenceContainer
//
// random_access_sequence_container refines sequence_container with subscript
// and at() operations. Satisfied by vector and deque (and array, basic_string,
// and inplace_vector in the standard).
// ============================================================================
template <class C>
concept random_access_sequence_container =
    sequence_container<C> && requires(C value, const C const_value, typename C::size_type pos) {
      { value[pos] } -> std::same_as<typename C::reference>;
      { const_value[pos] } -> std::same_as<typename C::const_reference>;
      { value.at(pos) } -> std::same_as<typename C::reference>;
      { const_value.at(pos) } -> std::same_as<typename C::const_reference>;
    };

// ============================================================================
// Named Requirement: AssociativeContainer
// https://en.cppreference.com/w/cpp/named_req/AssociativeContainer
//
// Expressed as a C++20 concept. No std:: equivalent exists.
//
// associative_container models the common ordered associative surface shared by
// set, map, multiset, and multimap. Unique-key and equivalent-key insertion
// return types differ, so they are modeled by refinements below instead of
// being forced into this base concept.
// ============================================================================
template <class C>
concept associative_container = reversible_container<C> && allocator_aware_container<C> &&
                                requires(C value,
                                         const C const_value,
                                         typename C::key_type key,
                                         typename C::value_type element,
                                         typename C::const_iterator position,
                                         typename C::const_iterator first,
                                         typename C::const_iterator last,
                                         typename C::node_type node) {
                                  typename C::key_type;
                                  typename C::key_compare;
                                  typename C::value_compare;
                                  typename C::node_type;

                                  requires std::strict_weak_order<typename C::key_compare&,
                                                                  const typename C::key_type&,
                                                                  const typename C::key_type&>;

                                  { const_value.key_comp() } -> std::same_as<typename C::key_compare>;
                                  { const_value.value_comp() } -> std::same_as<typename C::value_compare>;

                                  { value.insert(position, element) } -> std::same_as<typename C::iterator>;
                                  { value.insert(position, std::move(element)) } -> std::same_as<typename C::iterator>;
                                  { value.insert(first, last) } -> std::same_as<void>;
                                  { value.insert({}) } -> std::same_as<void>;

                                  { value.erase(position) } -> std::same_as<typename C::iterator>;
                                  { value.erase(first, last) } -> std::same_as<typename C::iterator>;
                                  { value.erase(key) } -> std::same_as<typename C::size_type>;
                                  { value.clear() } -> std::same_as<void>;

                                  { value.find(key) } -> std::same_as<typename C::iterator>;
                                  { const_value.find(key) } -> std::same_as<typename C::const_iterator>;
                                  { const_value.count(key) } -> std::same_as<typename C::size_type>;
                                  { const_value.contains(key) } -> std::same_as<bool>;
                                  { value.lower_bound(key) } -> std::same_as<typename C::iterator>;
                                  { const_value.lower_bound(key) } -> std::same_as<typename C::const_iterator>;
                                  { value.upper_bound(key) } -> std::same_as<typename C::iterator>;
                                  { const_value.upper_bound(key) } -> std::same_as<typename C::const_iterator>;
                                  {
                                    value.equal_range(key)
                                  } -> std::same_as<std::pair<typename C::iterator, typename C::iterator>>;
                                  {
                                    const_value.equal_range(key)
                                  } -> std::same_as<std::pair<typename C::const_iterator, typename C::const_iterator>>;

                                  { value.extract(position) } -> std::same_as<typename C::node_type>;
                                  { value.extract(key) } -> std::same_as<typename C::node_type>;
                                  { value.insert(position, std::move(node)) } -> std::same_as<typename C::iterator>;
                                };

// ============================================================================
// AssociativeContainer refinement: unique-key containers
//
// Satisfied by set and map.
// ============================================================================
template <class C>
concept unique_associative_container =
    associative_container<C> && requires(C value, typename C::value_type element, typename C::node_type node) {
      typename C::insert_return_type;
      { value.insert(element) } -> std::same_as<std::pair<typename C::iterator, bool>>;
      { value.insert(std::move(element)) } -> std::same_as<std::pair<typename C::iterator, bool>>;
      { value.insert(std::move(node)) } -> std::same_as<typename C::insert_return_type>;
    };

// ============================================================================
// AssociativeContainer refinement: equivalent-key containers
//
// Satisfied by multiset and multimap.
// ============================================================================
template <class C>
concept multi_associative_container =
    associative_container<C> && requires(C value, typename C::value_type element, typename C::node_type node) {
      { value.insert(element) } -> std::same_as<typename C::iterator>;
      { value.insert(std::move(element)) } -> std::same_as<typename C::iterator>;
      { value.insert(std::move(node)) } -> std::same_as<typename C::iterator>;
    };

// ============================================================================
// Concept traits
//
// These wrappers are useful in non-concept contexts while keeping the source of
// truth in the concept definitions above.
// ============================================================================
template <class C>
struct is_container : std::bool_constant<container<C>> {};

template <class C>
inline constexpr bool is_container_v = is_container<C>::value;

template <class C>
struct is_reversible_container : std::bool_constant<reversible_container<C>> {};

template <class C>
inline constexpr bool is_reversible_container_v = is_reversible_container<C>::value;

template <class C>
struct is_contiguous_container : std::bool_constant<contiguous_container<C>> {};

template <class C>
inline constexpr bool is_contiguous_container_v = is_contiguous_container<C>::value;

template <class C>
struct is_allocator_aware_container : std::bool_constant<allocator_aware_container<C>> {};

template <class C>
inline constexpr bool is_allocator_aware_container_v = is_allocator_aware_container<C>::value;

template <class C>
struct is_sequence_container : std::bool_constant<sequence_container<C>> {};

template <class C>
inline constexpr bool is_sequence_container_v = is_sequence_container<C>::value;

template <class C>
struct is_associative_container : std::bool_constant<associative_container<C>> {};

template <class C>
inline constexpr bool is_associative_container_v = is_associative_container<C>::value;

template <class C>
struct is_unique_associative_container : std::bool_constant<unique_associative_container<C>> {};

template <class C>
inline constexpr bool is_unique_associative_container_v = is_unique_associative_container<C>::value;

template <class C>
struct is_multi_associative_container : std::bool_constant<multi_associative_container<C>> {};

template <class C>
inline constexpr bool is_multi_associative_container_v = is_multi_associative_container<C>::value;

}  // namespace chaistl::concepts
