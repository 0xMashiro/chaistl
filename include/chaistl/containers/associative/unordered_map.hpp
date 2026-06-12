// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/basic.hpp>
#include <chaistl/concepts/container_compatible_range.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/containers/associative/detail/map_deduction.hpp>
#include <chaistl/containers/hash/hash_table.hpp>
#include <chaistl/containers/hash/key_extract.hpp>
#include <chaistl/memory/allocator.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace chaistl {

template <class Key,
          class T,
          class Hash,
          class KeyEqual,
          class Allocator,
          class RehashPolicy = detail::hash::prime_rehash_policy>
  requires concepts::container_element<std::pair<const Key, T>> &&
           concepts::allocator_for<Allocator, std::pair<const Key, T>> && detail::hash::hasher_for<Hash, Key> &&
           std::predicate<const KeyEqual&, const Key&, const Key&>
class unordered_multimap;

/**
 * @brief Unordered unique-key map.
 *
 * @ingroup containers_associative
 *
 * Architecture:
 *   - Thin wrapper over detail::hash::hash_table (shared with unordered_set)
 *   - value_type is pair<const Key, T>: the const key makes the mutable
 *     iterator safe — only the mapped value can be written through it
 *   - operator[] / insert_or_assign compose try_emplace; try_emplace is the
 *     core's key-first path with piecewise construction, so a hit constructs
 *     nothing and never consumes the mapped arguments
 *
 * Non-standard extensions:
 *   - constexpr end to end (C++26 / P3372 direction; use a constexpr hasher
 *     such as chaistl::hash — std::hash is not required to be constexpr)
 *   - validate() cross-checks the internal structures (project convention)
 *
 * References:
 *   - C++ Draft: https://eel.is/c++draft/unord.map
 *   - cppreference: https://en.cppreference.com/w/cpp/container/unordered_map
 *   - ADR: Hash Table Design (docs/develop/decisions/hash-table-design.md)
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = allocator<std::pair<const Key, T>>,
          class RehashPolicy = detail::hash::prime_rehash_policy>
  requires concepts::container_element<std::pair<const Key, T>> &&
           concepts::allocator_for<Allocator, std::pair<const Key, T>> && detail::hash::hasher_for<Hash, Key> &&
           std::predicate<const KeyEqual&, const Key&, const Key&>
class unordered_map {
  using table_type = detail::hash::hash_table<Key,
                                              std::pair<const Key, T>,
                                              detail::hash::select1st<std::pair<const Key, T>>,
                                              Hash,
                                              KeyEqual,
                                              Allocator,
                                              RehashPolicy>;

 public:
  // ========================================================================
  // Member types
  // ========================================================================

  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<const Key, T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using allocator_type = Allocator;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

  // The const key inside value_type is what protects the bucket placement;
  // the iterator itself may be mutable.
  using iterator = detail::hash::hash_iterator<value_type, false>;
  using const_iterator = detail::hash::hash_iterator<value_type, true>;
  using local_iterator = detail::hash::hash_local_iterator<value_type, false>;
  using const_local_iterator = detail::hash::hash_local_iterator<value_type, true>;

  // The handle exposes key() as a MUTABLE reference (the point of extract);
  // see hash_node_handle for the const_cast stance shared with the tree.
  using node_type = detail::hash::hash_node_handle<value_type, Allocator>;

  // [container.insert.return]: the node-handle insert result. Nested so it is
  // `unordered_map<K, T>::insert_return_type` (not a template).
  struct insert_return_type {
    iterator position;
    bool inserted;
    node_type node;
  };

  // ========================================================================
  // Construction
  // ========================================================================

  constexpr unordered_map() = default;

  explicit constexpr unordered_map(size_type bucket_count,
                                   const hasher& hash = hasher{},
                                   const key_equal& equal = key_equal{},
                                   const allocator_type& alloc = allocator_type{})
      : table_(bucket_count, hash, equal, alloc) {}

  constexpr unordered_map(size_type bucket_count, const allocator_type& alloc)
      : unordered_map(bucket_count, hasher{}, key_equal{}, alloc) {}

  constexpr unordered_map(size_type bucket_count, const hasher& hash, const allocator_type& alloc)
      : unordered_map(bucket_count, hash, key_equal{}, alloc) {}

  explicit constexpr unordered_map(const allocator_type& alloc) : table_(alloc) {}

  template <std::input_iterator InputIt>
  constexpr unordered_map(InputIt first,
                          InputIt last,
                          size_type bucket_count = 0,
                          const hasher& hash = hasher{},
                          const key_equal& equal = key_equal{},
                          const allocator_type& alloc = allocator_type{})
      : table_(bucket_count, hash, equal, alloc) {
    insert(first, last);
  }

  template <std::input_iterator InputIt>
  constexpr unordered_map(InputIt first, InputIt last, size_type bucket_count, const allocator_type& alloc)
      : unordered_map(first, last, bucket_count, hasher{}, key_equal{}, alloc) {}

  template <std::input_iterator InputIt>
  constexpr unordered_map(
      InputIt first, InputIt last, size_type bucket_count, const hasher& hash, const allocator_type& alloc)
      : unordered_map(first, last, bucket_count, hash, key_equal{}, alloc) {}

  constexpr unordered_map(std::initializer_list<value_type> init,
                          size_type bucket_count = 0,
                          const hasher& hash = hasher{},
                          const key_equal& equal = key_equal{},
                          const allocator_type& alloc = allocator_type{})
      : unordered_map(init.begin(), init.end(), bucket_count, hash, equal, alloc) {}

  constexpr unordered_map(std::initializer_list<value_type> init, size_type bucket_count, const allocator_type& alloc)
      : unordered_map(init, bucket_count, hasher{}, key_equal{}, alloc) {}

  constexpr unordered_map(std::initializer_list<value_type> init,
                          size_type bucket_count,
                          const hasher& hash,
                          const allocator_type& alloc)
      : unordered_map(init, bucket_count, hash, key_equal{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R>
  constexpr unordered_map(std::from_range_t,
                          R&& range,
                          size_type bucket_count = 0,
                          const hasher& hash = hasher{},
                          const key_equal& equal = key_equal{},
                          const allocator_type& alloc = allocator_type{})
      : table_(bucket_count, hash, equal, alloc) {
    insert_range(std::forward<R>(range));
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr unordered_map(std::from_range_t, R&& range, size_type bucket_count, const allocator_type& alloc)
      : unordered_map(std::from_range, std::forward<R>(range), bucket_count, hasher{}, key_equal{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R>
  constexpr unordered_map(
      std::from_range_t, R&& range, size_type bucket_count, const hasher& hash, const allocator_type& alloc)
      : unordered_map(std::from_range, std::forward<R>(range), bucket_count, hash, key_equal{}, alloc) {}

  constexpr unordered_map(const unordered_map& other) = default;

  constexpr unordered_map(const unordered_map& other, const std::type_identity_t<Allocator>& alloc)
      : table_(other.table_, alloc) {}

  constexpr unordered_map(unordered_map&& other) = default;

  constexpr unordered_map(unordered_map&& other, const std::type_identity_t<Allocator>& alloc)
      : table_(std::move(other.table_), alloc) {}

  constexpr ~unordered_map() = default;

  // ========================================================================
  // Assignment
  // ========================================================================

  constexpr unordered_map& operator=(const unordered_map& other) = default;
  constexpr unordered_map& operator=(unordered_map&& other) = default;

  constexpr unordered_map& operator=(std::initializer_list<value_type> init) {
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
  // Element access
  // ========================================================================

  constexpr T& operator[](const key_type& key) { return try_emplace(key).first->second; }

  constexpr T& operator[](key_type&& key) { return try_emplace(std::move(key)).first->second; }

  /**
   * @brief Heterogeneous operator[] (P2363, C++26-aligned extension).
   *
   * Behaves as try_emplace(std::forward<K>(key)).first->second: the Key is
   * only constructed from @p key when the entry does not exist yet.
   */
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  constexpr T& operator[](K&& key) {
    return table_.try_emplace_unique(std::forward<K>(key)).first->second;
  }

  constexpr T& at(const key_type& key) {
    iterator it = find(key);
    if (it == end()) {
      throw std::out_of_range("chaistl::unordered_map::at");
    }
    return it->second;
  }

  constexpr const T& at(const key_type& key) const {
    const_iterator it = find(key);
    if (it == end()) {
      throw std::out_of_range("chaistl::unordered_map::at");
    }
    return it->second;
  }

  /// Heterogeneous at (P2363, C++26-aligned extension).
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  constexpr T& at(const K& key) {
    iterator it = find(key);
    if (it == end()) {
      throw std::out_of_range("chaistl::unordered_map::at");
    }
    return it->second;
  }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  constexpr const T& at(const K& key) const {
    const_iterator it = find(key);
    if (it == end()) {
      throw std::out_of_range("chaistl::unordered_map::at");
    }
    return it->second;
  }

  // ========================================================================
  // Modifiers
  // ========================================================================

  constexpr void clear() noexcept { table_.clear(); }

  // --- insert ---

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
   * @brief Inserts every element of @p range (C++23 P1206).
   *
   * Convertible-but-not-value_type elements (e.g. pair<Key, T> without the
   * const) take the construct-first path inside the core.
   */
  template <concepts::container_compatible_range<value_type> R>
  constexpr void insert_range(R&& range) {
    for (auto&& element : range) {
      table_.insert_unique(std::forward<decltype(element)>(element));
    }
  }

  // --- emplace ---

  template <class... Args>
  constexpr std::pair<iterator, bool> emplace(Args&&... args) {
    return table_.emplace_unique(std::forward<Args>(args)...);
  }

  /// The hint is accepted for interface compatibility and ignored.
  template <class... Args>
  constexpr iterator emplace_hint(const_iterator /*hint*/, Args&&... args) {
    return emplace(std::forward<Args>(args)...).first;
  }

  // --- try_emplace ---

  template <class... Args>
  constexpr std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
    return table_.try_emplace_unique(key, std::forward<Args>(args)...);
  }

  template <class... Args>
  constexpr std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
    return table_.try_emplace_unique(std::move(key), std::forward<Args>(args)...);
  }

  template <class... Args>
  constexpr iterator try_emplace(const_iterator /*hint*/, const key_type& key, Args&&... args) {
    return try_emplace(key, std::forward<Args>(args)...).first;
  }

  template <class... Args>
  constexpr iterator try_emplace(const_iterator /*hint*/, key_type&& key, Args&&... args) {
    return try_emplace(std::move(key), std::forward<Args>(args)...).first;
  }

  // --- insert_or_assign ---

  template <class M>
  constexpr std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj) {
    auto [it, inserted] = try_emplace(key, std::forward<M>(obj));
    if (!inserted) {
      it->second = std::forward<M>(obj);
    }
    return {it, inserted};
  }

  template <class M>
  constexpr std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) {
    auto [it, inserted] = try_emplace(std::move(key), std::forward<M>(obj));
    if (!inserted) {
      it->second = std::forward<M>(obj);
    }
    return {it, inserted};
  }

  template <class M>
  constexpr iterator insert_or_assign(const_iterator /*hint*/, const key_type& key, M&& obj) {
    return insert_or_assign(key, std::forward<M>(obj)).first;
  }

  // --- P2363 heterogeneous try_emplace / insert_or_assign (C++26 alignment) ---

  template <class K, class... Args>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr std::pair<iterator, bool> try_emplace(K&& key, Args&&... args) {
    return table_.try_emplace_unique(std::forward<K>(key), std::forward<Args>(args)...);
  }

  template <class K, class... Args>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr iterator try_emplace(const_iterator /*hint*/, K&& key, Args&&... args) {
    return try_emplace(std::forward<K>(key), std::forward<Args>(args)...).first;
  }

  template <class K, class M>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr std::pair<iterator, bool> insert_or_assign(K&& key, M&& obj) {
    auto [it, inserted] = table_.try_emplace_unique(std::forward<K>(key), std::forward<M>(obj));
    if (!inserted) {
      it->second = std::forward<M>(obj);
    }
    return {it, inserted};
  }

  template <class K, class M>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr iterator insert_or_assign(const_iterator /*hint*/, K&& key, M&& obj) {
    return insert_or_assign(std::forward<K>(key), std::forward<M>(obj)).first;
  }

  template <class M>
  constexpr iterator insert_or_assign(const_iterator /*hint*/, key_type&& key, M&& obj) {
    return insert_or_assign(std::move(key), std::forward<M>(obj)).first;
  }

  // --- erase / swap ---

  constexpr iterator erase(const_iterator pos) { return table_.erase(pos); }

  constexpr iterator erase(const_iterator first, const_iterator last) {
    while (first != last) {
      first = table_.erase(first);
    }
    return iterator(first.base());
  }

  constexpr size_type erase(const key_type& key) { return table_.erase_key(key); }

  /**
   * @brief Heterogeneous erase (P2077, C++23).
   *
   * The convertibility gates keep a K that converts to an iterator from
   * hijacking erase(iterator).
   */
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr size_type erase(K&& key) {
    return table_.erase_key(key);
  }

  constexpr void swap(unordered_map& other) noexcept(std::is_nothrow_swappable_v<Hash> &&
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
   * The key is re-hashed with this container's hash function — including a
   * key that was modified through nh.key() while outside any container. On a
   * duplicate, ret.node keeps the handle's node.
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
   * Only the hash and predicate types may differ; spliced nodes keep their
   * original allocation, so element addresses are stable across the merge.
   */
  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_map<Key, T, Hash2, KeyEqual2, Allocator, RehashPolicy2>& source) {
    table_.merge_unique(source.table_);
  }

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_map<Key, T, Hash2, KeyEqual2, Allocator, RehashPolicy2>&& source) {
    merge(source);
  }

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_multimap<Key, T, Hash2, KeyEqual2, Allocator, RehashPolicy2>& source);

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_multimap<Key, T, Hash2, KeyEqual2, Allocator, RehashPolicy2>&& source) {
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
  // Heterogeneous lookup (C++20, P0919R3 + P1690R1): both Hash and KeyEqual
  // must publish is_transparent.
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
   * @brief Set-semantics equality over pairs (key and mapped both compared).
   *
   * No operator<, no operator<=> ([unord.req.general]).
   */
  [[nodiscard]] friend constexpr bool operator==(const unordered_map& lhs, const unordered_map& rhs)
    requires std::equality_comparable<value_type>
  {
    return lhs.table_.equals(rhs.table_);
  }

 private:
  // merge() splices nodes between maps that may differ in hash/predicate
  // type, which requires access to the other instantiation's table_.
  template <class K2, class T2, class H2, class E2, class A2, class R2>
    requires concepts::container_element<std::pair<const K2, T2>> &&
             concepts::allocator_for<A2, std::pair<const K2, T2>> && detail::hash::hasher_for<H2, K2> &&
             std::predicate<const E2&, const K2&, const K2&>
  friend class unordered_map;

  template <class K2, class T2, class H2, class E2, class A2, class R2>
    requires concepts::container_element<std::pair<const K2, T2>> &&
             concepts::allocator_for<A2, std::pair<const K2, T2>> && detail::hash::hasher_for<H2, K2> &&
             std::predicate<const E2&, const K2&, const K2&>
  friend class unordered_multimap;

  table_type table_;
};

template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = allocator<std::pair<const Key, T>>>
using power2_unordered_map =
    unordered_map<Key, T, Hash, KeyEqual, Allocator, detail::hash::power2_rehash_policy>;

// ============================================================================
// Deduction guides ([unord.map.overview]).
//
// Same disambiguation rules as unordered_set (allocator gate + integral gate
// on Hash), plus the map-specific exposition helpers: keys deduced from
// pair<Key, T> elements lose their const for the class arguments, while the
// allocator alias re-adds it.
// ============================================================================

template <class InputIt,
          class Hash = std::hash<detail::set_map_deduction::iter_key_t<InputIt>>,
          class Pred = std::equal_to<detail::set_map_deduction::iter_key_t<InputIt>>,
          class Allocator = allocator<detail::set_map_deduction::iter_alloc_t<InputIt>>>
  requires std::input_iterator<InputIt> && (!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
           (!concepts::qualifies_as_allocator<Pred>) && concepts::qualifies_as_allocator<Allocator>
unordered_map(InputIt, InputIt, std::size_t = 0, Hash = Hash(), Pred = Pred(), Allocator = Allocator())
    -> unordered_map<detail::set_map_deduction::iter_key_t<InputIt>,
                     detail::set_map_deduction::iter_mapped_t<InputIt>,
                     Hash,
                     Pred,
                     Allocator>;

template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class Pred = std::equal_to<Key>,
          class Allocator = allocator<std::pair<const Key, T>>>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
          (!concepts::qualifies_as_allocator<Pred>) && concepts::qualifies_as_allocator<Allocator>
unordered_map(
    std::initializer_list<std::pair<Key, T>>, std::size_t = 0, Hash = Hash(), Pred = Pred(), Allocator = Allocator())
    -> unordered_map<Key, T, Hash, Pred, Allocator>;

template <class InputIt, class Allocator>
  requires std::input_iterator<InputIt> && concepts::qualifies_as_allocator<Allocator>
unordered_map(InputIt, InputIt, std::size_t, Allocator)
    -> unordered_map<detail::set_map_deduction::iter_key_t<InputIt>,
                     detail::set_map_deduction::iter_mapped_t<InputIt>,
                     std::hash<detail::set_map_deduction::iter_key_t<InputIt>>,
                     std::equal_to<detail::set_map_deduction::iter_key_t<InputIt>>,
                     Allocator>;

template <class InputIt, class Hash, class Allocator>
  requires std::input_iterator<InputIt> && (!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
           concepts::qualifies_as_allocator<Allocator>
unordered_map(InputIt, InputIt, std::size_t, Hash, Allocator)
    -> unordered_map<detail::set_map_deduction::iter_key_t<InputIt>,
                     detail::set_map_deduction::iter_mapped_t<InputIt>,
                     Hash,
                     std::equal_to<detail::set_map_deduction::iter_key_t<InputIt>>,
                     Allocator>;

template <class Key, class T, class Allocator>
  requires concepts::qualifies_as_allocator<Allocator>
unordered_map(std::initializer_list<std::pair<Key, T>>, std::size_t, Allocator)
    -> unordered_map<Key, T, std::hash<Key>, std::equal_to<Key>, Allocator>;

template <class Key, class T, class Hash, class Allocator>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
          concepts::qualifies_as_allocator<Allocator>
unordered_map(std::initializer_list<std::pair<Key, T>>, std::size_t, Hash, Allocator)
    -> unordered_map<Key, T, Hash, std::equal_to<Key>, Allocator>;

template <std::ranges::input_range R,
          class Hash = std::hash<detail::set_map_deduction::range_key_t<R>>,
          class Pred = std::equal_to<detail::set_map_deduction::range_key_t<R>>,
          class Allocator = allocator<detail::set_map_deduction::range_alloc_t<R>>>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
          (!concepts::qualifies_as_allocator<Pred>) && concepts::qualifies_as_allocator<Allocator>
unordered_map(std::from_range_t, R&&, std::size_t = 0, Hash = Hash(), Pred = Pred(), Allocator = Allocator())
    -> unordered_map<detail::set_map_deduction::range_key_t<R>,
                     detail::set_map_deduction::range_mapped_t<R>,
                     Hash,
                     Pred,
                     Allocator>;

template <std::ranges::input_range R, class Allocator>
  requires concepts::qualifies_as_allocator<Allocator>
unordered_map(std::from_range_t, R&&, std::size_t, Allocator)
    -> unordered_map<detail::set_map_deduction::range_key_t<R>,
                     detail::set_map_deduction::range_mapped_t<R>,
                     std::hash<detail::set_map_deduction::range_key_t<R>>,
                     std::equal_to<detail::set_map_deduction::range_key_t<R>>,
                     Allocator>;

template <std::ranges::input_range R, class Hash, class Allocator>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
          concepts::qualifies_as_allocator<Allocator>
unordered_map(std::from_range_t, R&&, std::size_t, Hash, Allocator)
    -> unordered_map<detail::set_map_deduction::range_key_t<R>,
                     detail::set_map_deduction::range_mapped_t<R>,
                     Hash,
                     std::equal_to<detail::set_map_deduction::range_key_t<R>>,
                     Allocator>;

/**
 * @brief Erases all elements satisfying @p pred; returns the number erased.
 */
template <class Key, class T, class Hash, class KeyEqual, class Allocator, class RehashPolicy, class Predicate>
constexpr typename unordered_map<Key, T, Hash, KeyEqual, Allocator, RehashPolicy>::size_type erase_if(
    unordered_map<Key, T, Hash, KeyEqual, Allocator, RehashPolicy>& map, Predicate pred) {
  typename unordered_map<Key, T, Hash, KeyEqual, Allocator, RehashPolicy>::size_type removed = 0;
  for (auto it = map.begin(); it != map.end();) {
    if (pred(*it)) {
      it = map.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }
  return removed;
}

template <class Key, class T, class Hash, class KeyEqual, class Allocator, class RehashPolicy>
constexpr void swap(unordered_map<Key, T, Hash, KeyEqual, Allocator, RehashPolicy>& lhs,
                    unordered_map<Key, T, Hash, KeyEqual, Allocator, RehashPolicy>& rhs) noexcept(
    noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

}  // namespace chaistl
