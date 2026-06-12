// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/basic.hpp>
#include <chaistl/concepts/container_compatible_range.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/containers/hash/hash_table.hpp>
#include <chaistl/containers/hash/key_extract.hpp>
#include <chaistl/memory/allocator.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

namespace chaistl {

template <class Key, class Hash, class KeyEqual, class Allocator, class RehashPolicy = detail::hash::prime_rehash_policy>
  requires concepts::container_element<Key> && concepts::allocator_for<Allocator, Key> &&
           detail::hash::hasher_for<Hash, Key> && std::predicate<const KeyEqual&, const Key&, const Key&>
class unordered_multiset;

/**
 * @brief Unordered unique-key associative container.
 *
 * @ingroup containers_associative
 *
 * Architecture:
 *   - Thin wrapper over detail::hash::hash_table
 *   - Direct bucket chains for lookup plus an intrusive global iteration
 *     list, so begin()/++/erase are O(1) without scanning empty buckets
 *     (ADR: Hash Table Design)
 *   - Hash codes are cached per node: rehashing and erase(iterator) call no
 *     user code
 *
 * Non-standard extensions:
 *   - constexpr end to end (C++26 / P3372 direction; use a constexpr hasher
 *     such as chaistl::hash — std::hash is not required to be constexpr)
 *   - validate() cross-checks the internal structures (project convention)
 *
 * Iteration order happens to be insertion order. That is an implementation
 * detail, not an API promise — the standard leaves the order unspecified.
 *
 * References:
 *   - C++ Draft: https://eel.is/c++draft/unord.set
 *   - cppreference: https://en.cppreference.com/w/cpp/container/unordered_set
 *   - ADR: Hash Table Design (docs/develop/decisions/hash-table-design.md)
 */
template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = allocator<Key>,
          class RehashPolicy = detail::hash::prime_rehash_policy>
  requires concepts::container_element<Key> && concepts::allocator_for<Allocator, Key> &&
           detail::hash::hasher_for<Hash, Key> && std::predicate<const KeyEqual&, const Key&, const Key&>
class unordered_set {
  using table_type =
      detail::hash::hash_table<Key, Key, detail::hash::identity<Key>, Hash, KeyEqual, Allocator, RehashPolicy>;

 public:
  // ========================================================================
  // Member types
  // ========================================================================

  using key_type = Key;
  using value_type = Key;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using allocator_type = Allocator;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

  // Set keys are immutable: iterator and const_iterator are the same type.
  using iterator = detail::hash::hash_iterator<value_type, true>;
  using const_iterator = iterator;
  using local_iterator = detail::hash::hash_local_iterator<value_type, true>;
  using const_local_iterator = local_iterator;

  using node_type = detail::hash::hash_node_handle<value_type, Allocator>;

  // [container.insert.return]: the node-handle insert result. Nested so it is
  // `unordered_set<K>::insert_return_type` (not a template).
  struct insert_return_type {
    iterator position;
    bool inserted;
    node_type node;
  };

  // ========================================================================
  // Construction
  // ========================================================================

  constexpr unordered_set() = default;

  explicit constexpr unordered_set(size_type bucket_count,
                                   const hasher& hash = hasher{},
                                   const key_equal& equal = key_equal{},
                                   const allocator_type& alloc = allocator_type{})
      : table_(bucket_count, hash, equal, alloc) {}

  constexpr unordered_set(size_type bucket_count, const allocator_type& alloc)
      : unordered_set(bucket_count, hasher{}, key_equal{}, alloc) {}

  constexpr unordered_set(size_type bucket_count, const hasher& hash, const allocator_type& alloc)
      : unordered_set(bucket_count, hash, key_equal{}, alloc) {}

  explicit constexpr unordered_set(const allocator_type& alloc) : table_(alloc) {}

  template <std::input_iterator InputIt>
  constexpr unordered_set(InputIt first,
                          InputIt last,
                          size_type bucket_count = 0,
                          const hasher& hash = hasher{},
                          const key_equal& equal = key_equal{},
                          const allocator_type& alloc = allocator_type{})
      : table_(bucket_count, hash, equal, alloc) {
    insert(first, last);
  }

  template <std::input_iterator InputIt>
  constexpr unordered_set(InputIt first, InputIt last, size_type bucket_count, const allocator_type& alloc)
      : unordered_set(first, last, bucket_count, hasher{}, key_equal{}, alloc) {}

  template <std::input_iterator InputIt>
  constexpr unordered_set(
      InputIt first, InputIt last, size_type bucket_count, const hasher& hash, const allocator_type& alloc)
      : unordered_set(first, last, bucket_count, hash, key_equal{}, alloc) {}

  constexpr unordered_set(std::initializer_list<value_type> init,
                          size_type bucket_count = 0,
                          const hasher& hash = hasher{},
                          const key_equal& equal = key_equal{},
                          const allocator_type& alloc = allocator_type{})
      : unordered_set(init.begin(), init.end(), bucket_count, hash, equal, alloc) {}

  constexpr unordered_set(std::initializer_list<value_type> init, size_type bucket_count, const allocator_type& alloc)
      : unordered_set(init, bucket_count, hasher{}, key_equal{}, alloc) {}

  constexpr unordered_set(std::initializer_list<value_type> init,
                          size_type bucket_count,
                          const hasher& hash,
                          const allocator_type& alloc)
      : unordered_set(init, bucket_count, hash, key_equal{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R>
  constexpr unordered_set(std::from_range_t,
                          R&& range,
                          size_type bucket_count = 0,
                          const hasher& hash = hasher{},
                          const key_equal& equal = key_equal{},
                          const allocator_type& alloc = allocator_type{})
      : table_(bucket_count, hash, equal, alloc) {
    insert_range(std::forward<R>(range));
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr unordered_set(std::from_range_t, R&& range, size_type bucket_count, const allocator_type& alloc)
      : unordered_set(std::from_range, std::forward<R>(range), bucket_count, hasher{}, key_equal{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R>
  constexpr unordered_set(
      std::from_range_t, R&& range, size_type bucket_count, const hasher& hash, const allocator_type& alloc)
      : unordered_set(std::from_range, std::forward<R>(range), bucket_count, hash, key_equal{}, alloc) {}

  constexpr unordered_set(const unordered_set& other) = default;

  constexpr unordered_set(const unordered_set& other, const std::type_identity_t<Allocator>& alloc)
      : table_(other.table_, alloc) {}

  constexpr unordered_set(unordered_set&& other) = default;

  constexpr unordered_set(unordered_set&& other, const std::type_identity_t<Allocator>& alloc)
      : table_(std::move(other.table_), alloc) {}

  constexpr ~unordered_set() = default;

  // ========================================================================
  // Assignment
  // ========================================================================

  constexpr unordered_set& operator=(const unordered_set& other) = default;
  constexpr unordered_set& operator=(unordered_set&& other) = default;

  constexpr unordered_set& operator=(std::initializer_list<value_type> init) {
    clear();
    insert(init);
    return *this;
  }

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return table_.get_allocator(); }

  // ========================================================================
  // Iterators
  // ========================================================================

  [[nodiscard]] constexpr iterator begin() noexcept { return table_.begin(); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return table_.begin(); }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
  [[nodiscard]] constexpr iterator end() noexcept { return table_.end(); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return table_.end(); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  // ========================================================================
  // Capacity
  // ========================================================================

  [[nodiscard]] constexpr bool empty() const noexcept { return table_.empty(); }
  [[nodiscard]] constexpr size_type size() const noexcept { return table_.size(); }
  [[nodiscard]] constexpr size_type max_size() const noexcept { return table_.max_size(); }

  // ========================================================================
  // Modifiers
  // ========================================================================

  constexpr void clear() noexcept { table_.clear(); }

  constexpr std::pair<iterator, bool> insert(const value_type& value) { return table_.insert_unique(value); }

  constexpr std::pair<iterator, bool> insert(value_type&& value) { return table_.insert_unique(std::move(value)); }

  /// The hint is accepted for interface compatibility and ignored.
  constexpr iterator insert(const_iterator /*hint*/, const value_type& value) { return insert(value).first; }

  constexpr iterator insert(const_iterator /*hint*/, value_type&& value) { return insert(std::move(value)).first; }

  template <std::input_iterator InputIt>
  constexpr void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      table_.insert_unique(*first);
    }
  }

  constexpr void insert(std::initializer_list<value_type> init) { insert(init.begin(), init.end()); }

  /**
   * @brief Heterogeneous insert (P2363, C++26-aligned extension).
   *
   * The key is hashed and looked up through the transparent functors; the
   * value_type conversion only runs on a miss — a hit constructs nothing,
   * which is the point of the overload.
   */
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr std::pair<iterator, bool> insert(K&& key) {
    return table_.insert_transparent_unique(std::forward<K>(key));
  }

  /// The hint is accepted for interface compatibility and ignored.
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr iterator insert(const_iterator /*hint*/, K&& key) {
    return insert(std::forward<K>(key)).first;
  }

  /**
   * @brief Inserts every element of @p range (C++23 P1206).
   *
   * Elements that are convertible to value_type but are not value_type take
   * the construct-first path inside the core — the standard's
   * EmplaceConstructible semantics for range insertion.
   */
  template <concepts::container_compatible_range<value_type> R>
  constexpr void insert_range(R&& range) {
    for (auto&& element : range) {
      table_.insert_unique(std::forward<decltype(element)>(element));
    }
  }

  template <class... Args>
  constexpr std::pair<iterator, bool> emplace(Args&&... args) {
    return table_.emplace_unique(std::forward<Args>(args)...);
  }

  /// The hint is accepted for interface compatibility and ignored.
  template <class... Args>
  constexpr iterator emplace_hint(const_iterator /*hint*/, Args&&... args) {
    return emplace(std::forward<Args>(args)...).first;
  }

  constexpr iterator erase(const_iterator pos) { return table_.erase(pos); }

  constexpr iterator erase(const_iterator first, const_iterator last) {
    while (first != last) {
      first = table_.erase(first);
    }
    return first;
  }

  constexpr size_type erase(const key_type& key) { return table_.erase_key(key); }

  /**
   * @brief Heterogeneous erase (P2077, C++23).
   *
   * The convertibility gates keep a K that converts to an iterator from
   * hijacking erase(iterator) — the overload-resolution trap P2077's wording
   * guards against.
   */
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr size_type erase(K&& key) {
    return table_.erase_key(key);
  }

  constexpr void swap(unordered_set& other) noexcept(std::is_nothrow_swappable_v<Hash> &&
                                                     std::is_nothrow_swappable_v<KeyEqual>) {
    table_.swap(other.table_);
  }

  // --- extract / node-handle insert / merge ---

  constexpr node_type extract(const_iterator pos) { return table_.extract(pos); }

  constexpr node_type extract(const key_type& key) { return table_.extract_key(key); }

  /// Heterogeneous extract (P2077, C++23), gated like heterogeneous erase.
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr node_type extract(K&& key) {
    return table_.extract_key(key);
  }

  /**
   * @brief Adopts the node owned by @p nh.
   *
   * The key is re-hashed with this container's hash function — the handle
   * may come from a container with a different stateful hasher. On a
   * duplicate, ret.node keeps the handle's node ([container.insert.return]).
   */
  constexpr insert_return_type insert(node_type&& nh) {
    auto [it, inserted] = table_.insert_node_unique(nh);
    return {iterator(it), inserted, std::move(nh)};
  }

  /// The hint is accepted for interface compatibility and ignored.
  constexpr iterator insert(const_iterator /*hint*/, node_type&& nh) { return insert(std::move(nh)).position; }

  /**
   * @brief Splices every element of @p source whose key is absent here.
   *
   * Only the hash and predicate types may differ — the allocator type is
   * fixed, because spliced nodes keep their original allocation and must be
   * deallocatable by this container's allocator. Element addresses are
   * stable across the merge.
   */
  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_set<Key, Hash2, KeyEqual2, Allocator, RehashPolicy2>& source) {
    table_.merge_unique(source.table_);
  }

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_set<Key, Hash2, KeyEqual2, Allocator, RehashPolicy2>&& source) {
    merge(source);
  }

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_multiset<Key, Hash2, KeyEqual2, Allocator, RehashPolicy2>& source);

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_multiset<Key, Hash2, KeyEqual2, Allocator, RehashPolicy2>&& source) {
    merge(source);
  }

  // ========================================================================
  // Lookup
  // ========================================================================

  [[nodiscard]] constexpr iterator find(const key_type& key) { return table_.find(key); }

  [[nodiscard]] constexpr const_iterator find(const key_type& key) const { return table_.find(key); }

  [[nodiscard]] constexpr size_type count(const key_type& key) const { return table_.count(key); }

  [[nodiscard]] constexpr bool contains(const key_type& key) const { return table_.contains(key); }

  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const key_type& key) {
    return table_.equal_range(key);
  }

  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
    return table_.equal_range(key);
  }

  // ------------------------------------------------------------------------
  // Heterogeneous lookup (C++20, P0919R3 + P1690R1).
  //
  // These overloads participate only when BOTH Hash and KeyEqual publish
  // is_transparent: a transparent predicate alone cannot decide which bucket
  // to probe, so the hasher must be transparent too — the rule P1690 added.
  // ------------------------------------------------------------------------

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr iterator find(const K& key) {
    return table_.find(key);
  }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr const_iterator find(const K& key) const {
    return table_.find(key);
  }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr size_type count(const K& key) const {
    return table_.count(key);
  }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr bool contains(const K& key) const {
    return table_.contains(key);
  }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const K& key) {
    return table_.equal_range(key);
  }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
    return table_.equal_range(key);
  }

  // ========================================================================
  // Bucket interface
  // ========================================================================

  [[nodiscard]] constexpr size_type bucket_count() const noexcept { return table_.bucket_count(); }

  [[nodiscard]] constexpr size_type max_bucket_count() const noexcept { return table_.max_bucket_count(); }

  [[nodiscard]] constexpr size_type bucket_size(size_type index) const noexcept { return table_.bucket_size(index); }

  [[nodiscard]] constexpr size_type bucket(const key_type& key) const { return table_.bucket(key); }

  /// Heterogeneous bucket lookup (P2363, C++26-aligned extension).
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr size_type bucket(const K& key) const {
    return table_.bucket(key);
  }

  [[nodiscard]] constexpr local_iterator begin(size_type index) noexcept { return table_.begin(index); }

  [[nodiscard]] constexpr const_local_iterator begin(size_type index) const noexcept { return table_.begin(index); }

  [[nodiscard]] constexpr const_local_iterator cbegin(size_type index) const noexcept { return begin(index); }

  [[nodiscard]] constexpr local_iterator end(size_type index) noexcept { return table_.end(index); }

  [[nodiscard]] constexpr const_local_iterator end(size_type index) const noexcept { return table_.end(index); }

  [[nodiscard]] constexpr const_local_iterator cend(size_type index) const noexcept { return end(index); }

  // ========================================================================
  // Hash policy
  // ========================================================================

  [[nodiscard]] constexpr float load_factor() const noexcept { return table_.load_factor(); }

  [[nodiscard]] constexpr float max_load_factor() const noexcept { return table_.max_load_factor(); }

  /// Stores the bound as a hint; never rehashes by itself ([unord.req]).
  constexpr void max_load_factor(float factor) noexcept { table_.max_load_factor(factor); }

  constexpr void rehash(size_type bucket_count_request) { table_.rehash(bucket_count_request); }

  constexpr void reserve(size_type element_count) { table_.reserve(element_count); }

  // ========================================================================
  // Observers
  // ========================================================================

  [[nodiscard]] constexpr hasher hash_function() const { return table_.hash_function(); }

  [[nodiscard]] constexpr key_equal key_eq() const { return table_.key_eq(); }

  // ========================================================================
  // Validation (non-standard, project convention)
  // ========================================================================

  [[nodiscard]] constexpr bool validate() const { return table_.validate(); }

  // ========================================================================
  // Comparison
  // ========================================================================

  /**
   * @brief Set-semantics equality.
   *
   * Unordered containers compare as sets and intentionally provide no
   * operator< and no operator<=> ([unord.req.general]). The behavior is
   * undefined unless both containers' predicates agree — the hash functions
   * need not.
   */
  [[nodiscard]] friend constexpr bool operator==(const unordered_set& lhs, const unordered_set& rhs)
    requires std::equality_comparable<value_type>
  {
    return lhs.table_.equals(rhs.table_);
  }

 private:
  // merge() splices nodes between sets that may differ in hash/predicate
  // type, which requires access to the other instantiation's table_.
  template <class K2, class H2, class E2, class A2, class R2>
    requires concepts::container_element<K2> && concepts::allocator_for<A2, K2> && detail::hash::hasher_for<H2, K2> &&
             std::predicate<const E2&, const K2&, const K2&>
  friend class unordered_set;

  template <class K2, class H2, class E2, class A2, class R2>
    requires concepts::container_element<K2> && concepts::allocator_for<A2, K2> && detail::hash::hasher_for<H2, K2> &&
             std::predicate<const E2&, const K2&, const K2&>
  friend class unordered_multiset;

  table_type table_;
};

template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = allocator<Key>>
using power2_unordered_set =
    unordered_set<Key, Hash, KeyEqual, Allocator, detail::hash::power2_rehash_policy>;

// ============================================================================
// Deduction guides ([unord.set.overview]).
//
// The unordered guides need more disambiguation than the ordered ones: the
// bucket-count argument is integral, so a guide must never deduce an integer
// as the Hash — hence the is_integral gate next to qualifies-as-allocator
// (the same gate the standard specifies).
// ============================================================================

template <class InputIt,
          class Hash = std::hash<std::iter_value_t<InputIt>>,
          class Pred = std::equal_to<std::iter_value_t<InputIt>>,
          class Allocator = allocator<std::iter_value_t<InputIt>>>
  requires std::input_iterator<InputIt> && (!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
           (!concepts::qualifies_as_allocator<Pred>) && concepts::qualifies_as_allocator<Allocator>
unordered_set(InputIt, InputIt, std::size_t = 0, Hash = Hash(), Pred = Pred(), Allocator = Allocator())
    -> unordered_set<std::iter_value_t<InputIt>, Hash, Pred, Allocator>;

template <class T, class Hash = std::hash<T>, class Pred = std::equal_to<T>, class Allocator = allocator<T>>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
          (!concepts::qualifies_as_allocator<Pred>) && concepts::qualifies_as_allocator<Allocator>
unordered_set(std::initializer_list<T>, std::size_t = 0, Hash = Hash(), Pred = Pred(), Allocator = Allocator())
    -> unordered_set<T, Hash, Pred, Allocator>;

template <class InputIt, class Allocator>
  requires std::input_iterator<InputIt> && concepts::qualifies_as_allocator<Allocator>
unordered_set(InputIt, InputIt, std::size_t, Allocator) -> unordered_set<std::iter_value_t<InputIt>,
                                                                         std::hash<std::iter_value_t<InputIt>>,
                                                                         std::equal_to<std::iter_value_t<InputIt>>,
                                                                         Allocator>;

template <class InputIt, class Hash, class Allocator>
  requires std::input_iterator<InputIt> && (!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
           concepts::qualifies_as_allocator<Allocator>
unordered_set(InputIt, InputIt, std::size_t, Hash, Allocator)
    -> unordered_set<std::iter_value_t<InputIt>, Hash, std::equal_to<std::iter_value_t<InputIt>>, Allocator>;

template <class T, class Allocator>
  requires concepts::qualifies_as_allocator<Allocator>
unordered_set(std::initializer_list<T>, std::size_t, Allocator)
    -> unordered_set<T, std::hash<T>, std::equal_to<T>, Allocator>;

template <class T, class Hash, class Allocator>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
          concepts::qualifies_as_allocator<Allocator>
unordered_set(std::initializer_list<T>, std::size_t, Hash, Allocator)
    -> unordered_set<T, Hash, std::equal_to<T>, Allocator>;

template <std::ranges::input_range R,
          class Hash = std::hash<std::ranges::range_value_t<R>>,
          class Pred = std::equal_to<std::ranges::range_value_t<R>>,
          class Allocator = allocator<std::ranges::range_value_t<R>>>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
          (!concepts::qualifies_as_allocator<Pred>) && concepts::qualifies_as_allocator<Allocator>
unordered_set(std::from_range_t, R&&, std::size_t = 0, Hash = Hash(), Pred = Pred(), Allocator = Allocator())
    -> unordered_set<std::ranges::range_value_t<R>, Hash, Pred, Allocator>;

template <std::ranges::input_range R, class Allocator>
  requires concepts::qualifies_as_allocator<Allocator>
unordered_set(std::from_range_t, R&&, std::size_t, Allocator)
    -> unordered_set<std::ranges::range_value_t<R>,
                     std::hash<std::ranges::range_value_t<R>>,
                     std::equal_to<std::ranges::range_value_t<R>>,
                     Allocator>;

template <std::ranges::input_range R, class Hash, class Allocator>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
          concepts::qualifies_as_allocator<Allocator>
unordered_set(std::from_range_t, R&&, std::size_t, Hash, Allocator)
    -> unordered_set<std::ranges::range_value_t<R>, Hash, std::equal_to<std::ranges::range_value_t<R>>, Allocator>;

/**
 * @brief Erases all elements satisfying @p pred; returns the number erased.
 */
template <class Key, class Hash, class KeyEqual, class Allocator, class RehashPolicy, class Predicate>
constexpr typename unordered_set<Key, Hash, KeyEqual, Allocator, RehashPolicy>::size_type erase_if(
    unordered_set<Key, Hash, KeyEqual, Allocator, RehashPolicy>& set, Predicate pred) {
  typename unordered_set<Key, Hash, KeyEqual, Allocator, RehashPolicy>::size_type removed = 0;
  for (auto it = set.begin(); it != set.end();) {
    if (pred(*it)) {
      it = set.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }
  return removed;
}

template <class Key, class Hash, class KeyEqual, class Allocator, class RehashPolicy>
constexpr void swap(unordered_set<Key, Hash, KeyEqual, Allocator, RehashPolicy>& lhs,
                    unordered_set<Key, Hash, KeyEqual, Allocator, RehashPolicy>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

}  // namespace chaistl
