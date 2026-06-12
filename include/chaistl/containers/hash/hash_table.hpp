// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/containers/hash/iterator.hpp>
#include <chaistl/containers/hash/node.hpp>
#include <chaistl/containers/hash/node_handle.hpp>
#include <chaistl/containers/hash/prime_rehash_policy.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>
#include <chaistl/memory/detail/storage/node_owner.hpp>
#include <chaistl/meta/type_traits.hpp>
#include <chaistl/utility/hardening.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::detail::hash {

/**
 * @brief Constraint for hash function objects (Cpp17Hash, concept form).
 *
 * Internal for now; promotion into chaistl::concepts is deferred until the
 * experimental table families show which hash-related concepts are worth a
 * public name (ADR: Hash Table Design, open questions).
 */
template <class Hash, class Key>
concept hasher_for = std::regular_invocable<const Hash&, const Key&> &&
                     std::convertible_to<std::invoke_result_t<const Hash&, const Key&>, std::size_t>;

/**
 * @brief Owning, node-based, separate-chaining hash table.
 *
 * The shared core under chaistl::unordered_set / unordered_map.
 *
 * Physical layout (ADR: Hash Table Design):
 *
 *       buckets_ ──► [ b0 ][ b1 ][ b2 ] ... [ bN-1 ]     direct chain heads
 *                       │           │
 *                       ▼           ▼
 *                     node ──────► node          next_in_bucket (lookup,
 *                       ▲ │          ▲ │                         local iterators)
 *                       │ ▼          │ ▼
 *       order_head_ ◄─ node ◄─────► node ─► ...  prev/next_in_order (begin(),
 *                                                ++iterator, erase — all O(1))
 *
 * The two structures are orthogonal: bucket chains never affect iteration
 * complexity, and the iteration list never affects lookup. Each node caches
 * its full hash code, which makes rehash migration and erase(iterator) free
 * of user code.
 *
 * Exception-safety discipline for insertion (two paths):
 *   - key-first (insert): hash → find → construct node → rehash → link.
 *   - construct-first (emplace): construct node → hash → find → rehash → link.
 *   Node construction always precedes the rehash so a failed insert never
 *   invalidates iterators ([unord.req.except]); a duplicate never rehashes.
 *
 * @tparam Key          The key type.
 * @tparam Value        The element type (Key for sets, pair<const Key, T> for maps).
 * @tparam KeyOfValue   Extracts `const Key&` from a `const Value&`.
 * @tparam Hash         Hash function object for Key.
 * @tparam KeyEqual     Equality predicate for Key.
 * @tparam Allocator    Allocator for Value; rebound for nodes and buckets.
 * @tparam RehashPolicy Bucket sizing/indexing policy (default prime-based).
 */
template <class Key,
          class Value,
          class KeyOfValue,
          class Hash,
          class KeyEqual,
          class Allocator,
          class RehashPolicy = prime_rehash_policy>
  requires concepts::container_element<Value> && concepts::allocator_for<Allocator, Value> && hasher_for<Hash, Key> &&
           std::predicate<const KeyEqual&, const Key&, const Key&>
class hash_table {
 public:
  // ========================================================================
  // Member types
  // ========================================================================

  using key_type = Key;
  using value_type = Value;
  using key_of_value_type = KeyOfValue;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using allocator_type = Allocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using size_type = typename allocator_traits::size_type;
  using difference_type = typename allocator_traits::difference_type;
  using reference = value_type&;
  using const_reference = const value_type&;

  using iterator = hash_iterator<value_type, false>;
  using const_iterator = hash_iterator<value_type, true>;
  using local_iterator = hash_local_iterator<value_type, false>;
  using const_local_iterator = hash_local_iterator<value_type, true>;

  using table_node_type = hash_node<value_type>;
  using node_allocator_type = typename allocator_traits::template rebind_alloc<table_node_type>;
  using node_allocator_traits = std::allocator_traits<node_allocator_type>;
  using bucket_allocator_type = typename allocator_traits::template rebind_alloc<hash_node_base*>;
  using bucket_allocator_traits = std::allocator_traits<bucket_allocator_type>;
  using bucket_pointer = typename bucket_allocator_traits::pointer;
  using node_handle_type = hash_node_handle<value_type, allocator_type>;

  // ========================================================================
  // Construction / destruction
  // ========================================================================

  /// Zero buckets, no allocation (lazy like libc++; heap-layer discipline).
  constexpr hash_table() = default;

  explicit constexpr hash_table(size_type bucket_count_request,
                                const Hash& hash = Hash{},
                                const KeyEqual& equal = KeyEqual{},
                                const Allocator& alloc = Allocator{})
      : hasher_(hash), key_equal_(equal), node_allocator_(alloc) {
    if (bucket_count_request > 0) {
      allocate_initial_buckets(rehash_policy_.next_bucket_count(bucket_count_request));
    }
  }

  explicit constexpr hash_table(const Allocator& alloc) : node_allocator_(alloc) {}

  constexpr hash_table(const hash_table& other)
      : max_load_factor_(other.max_load_factor_),
        key_of_value_(other.key_of_value_),
        hasher_(other.hasher_),
        key_equal_(other.key_equal_),
        rehash_policy_(other.rehash_policy_),
        node_allocator_(node_allocator_traits::select_on_container_copy_construction(other.node_allocator_)) {
    copy_from(other);
  }

  constexpr hash_table(const hash_table& other, const allocator_type& alloc)
      : max_load_factor_(other.max_load_factor_),
        key_of_value_(other.key_of_value_),
        hasher_(other.hasher_),
        key_equal_(other.key_equal_),
        rehash_policy_(other.rehash_policy_),
        node_allocator_(alloc) {
    copy_from(other);
  }

  constexpr hash_table(hash_table&& other) noexcept(std::is_nothrow_move_constructible_v<Hash> &&
                                                    std::is_nothrow_move_constructible_v<KeyEqual>)
      : buckets_(std::exchange(other.buckets_, bucket_pointer{})),
        bucket_count_(std::exchange(other.bucket_count_, 0)),
        order_head_(std::exchange(other.order_head_, nullptr)),
        order_tail_(std::exchange(other.order_tail_, nullptr)),
        size_(std::exchange(other.size_, 0)),
        max_load_factor_(other.max_load_factor_),
        key_of_value_(std::move(other.key_of_value_)),
        hasher_(std::move(other.hasher_)),
        key_equal_(std::move(other.key_equal_)),
        rehash_policy_(std::move(other.rehash_policy_)),
        node_allocator_(std::move(other.node_allocator_)) {}

  constexpr hash_table(hash_table&& other, const allocator_type& alloc)
      : max_load_factor_(other.max_load_factor_),
        key_of_value_(std::move(other.key_of_value_)),
        hasher_(std::move(other.hasher_)),
        key_equal_(std::move(other.key_equal_)),
        rehash_policy_(std::move(other.rehash_policy_)),
        node_allocator_(alloc) {
    if (node_allocator_ == other.node_allocator_) {
      take_storage_from(other);
    } else {
      // Unequal allocators: nodes cannot change owners, move element-wise.
      move_values_from(other);
    }
  }

  constexpr ~hash_table() { destroy_and_deallocate_storage(); }

  // ========================================================================
  // Assignment / swap
  // ========================================================================

  constexpr hash_table& operator=(const hash_table& other) {
    if (this == std::addressof(other)) {
      return *this;
    }
    destroy_and_deallocate_storage();  // deallocate with the pre-assignment allocator
    if constexpr (meta::propagate_on_container_copy_assignment_v<node_allocator_type>) {
      node_allocator_ = other.node_allocator_;
    }
    // [unord.req]: assignment copies the hash function, predicate, and
    // maximum load factor along with the elements.
    key_of_value_ = other.key_of_value_;
    hasher_ = other.hasher_;
    key_equal_ = other.key_equal_;
    rehash_policy_ = other.rehash_policy_;
    max_load_factor_ = other.max_load_factor_;
    copy_from(other);
    return *this;
  }

  constexpr hash_table& operator=(hash_table&& other) noexcept(
      meta::is_nothrow_container_move_assignable_v<node_allocator_type, value_type> &&
      std::is_nothrow_move_assignable_v<Hash> && std::is_nothrow_move_assignable_v<KeyEqual>) {
    if (this == std::addressof(other)) {
      return *this;
    }
    destroy_and_deallocate_storage();
    key_of_value_ = std::move(other.key_of_value_);
    hasher_ = std::move(other.hasher_);
    key_equal_ = std::move(other.key_equal_);
    rehash_policy_ = std::move(other.rehash_policy_);
    max_load_factor_ = other.max_load_factor_;
    if constexpr (meta::propagate_on_container_move_assignment_v<node_allocator_type>) {
      node_allocator_ = std::move(other.node_allocator_);
      take_storage_from(other);
    } else {
      if (node_allocator_ == other.node_allocator_) {
        take_storage_from(other);
      } else {
        move_values_from(other);
      }
    }
    return *this;
  }

  constexpr void swap(hash_table& other) noexcept(std::is_nothrow_swappable_v<Hash> &&
                                                  std::is_nothrow_swappable_v<KeyEqual>) {
    using std::swap;
    swap(buckets_, other.buckets_);
    swap(bucket_count_, other.bucket_count_);
    swap(order_head_, other.order_head_);
    swap(order_tail_, other.order_tail_);
    swap(size_, other.size_);
    swap(max_load_factor_, other.max_load_factor_);
    swap(key_of_value_, other.key_of_value_);
    swap(hasher_, other.hasher_);
    swap(key_equal_, other.key_equal_);
    swap(rehash_policy_, other.rehash_policy_);
    if constexpr (meta::propagate_on_container_swap_v<node_allocator_type>) {
      swap(node_allocator_, other.node_allocator_);
    }
  }

  // ========================================================================
  // Observers
  // ========================================================================

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return allocator_type(node_allocator_); }

  [[nodiscard]] constexpr hasher hash_function() const { return hasher_; }

  [[nodiscard]] constexpr key_equal key_eq() const { return key_equal_; }

  // ========================================================================
  // Iterators
  // ========================================================================

  [[nodiscard]] constexpr iterator begin() noexcept { return iterator(order_head_); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(order_head_); }
  // end() is the default-constructed (null) iterator; constructing from a
  // bare nullptr would be ambiguous between the two pointer constructors.
  [[nodiscard]] constexpr iterator end() noexcept { return iterator{}; }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator{}; }

  // ========================================================================
  // Capacity
  // ========================================================================

  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

  [[nodiscard]] constexpr size_type size() const noexcept { return size_; }

  [[nodiscard]] constexpr size_type max_size() const noexcept {
    return node_allocator_traits::max_size(node_allocator_);
  }

  // ========================================================================
  // Modifiers
  // ========================================================================

  /**
   * @brief Unique-key insertion, key-first path.
   *
   * The key is extracted from @p value before any construction, so a
   * duplicate is detected without allocating and an exception from Hash or
   * KeyEqual leaves the table untouched.
   *
   * The key-first path is only sound when @p value IS the value type: for a
   * merely convertible argument, KeyOfValue would bind its `const Key&`
   * parameter to a conversion temporary that dies at the end of the
   * declaration statement, leaving `key` dangling. Convertible arguments
   * (range insertion) take the construct-first path instead, which is the
   * standard's EmplaceConstructible semantics for those overloads anyway.
   */
  template <class Arg>
  constexpr std::pair<iterator, bool> insert_unique(Arg&& value) {
    if constexpr (!std::same_as<std::remove_cvref_t<Arg>, value_type>) {
      return emplace_unique(std::forward<Arg>(value));
    } else {
      const key_type& key = key_of_value_(value);
      const std::size_t code = hasher_(key);
      const bool must_rehash = will_rehash_for_one_more();
      const unique_insert_position position = find_unique_insert_position(code, key);
      if (position.existing != nullptr) {
        return {iterator(position.existing), false};
      }
      table_node_type* node = create_node(std::forward<Arg>(value));
      auto guard = detail::make_exception_guard([&] {
        destroy_node(node);
      });
      if (must_rehash) {
        reserve_for_one_more();
        link_node(node, code);
      } else {
        link_node_at_bucket_tail(node, code, position.tail_link);
      }
      guard.complete();
      return {iterator(node), true};
    }
  }

  /**
   * @brief Equivalent-key insertion, key-first path.
   *
   * Equal keys are kept contiguous in the global iteration list. When an
   * equivalent node already exists, the new node is linked directly after
   * that key's existing segment in both the order list and bucket chain.
   */
  template <class Arg>
  constexpr iterator insert_multi(Arg&& value) {
    if constexpr (!std::same_as<std::remove_cvref_t<Arg>, value_type>) {
      return emplace_multi(std::forward<Arg>(value));
    } else {
      const key_type& key = key_of_value_(value);
      const std::size_t code = hasher_(key);
      hash_node_base* existing = find_node(code, key);
      table_node_type* node = create_node(std::forward<Arg>(value));
      auto guard = detail::make_exception_guard([&] {
        destroy_node(node);
      });
      reserve_for_one_more();
      link_node_multi(node, code, existing);
      guard.complete();
      return iterator(node);
    }
  }

  /**
   * @brief Unique-key insertion, construct-first path.
   *
   * emplace cannot know the key until the value exists, so the node is
   * constructed first. Every later failure point (Hash, KeyEqual, the
   * bucket-array allocation of a rehash) destroys the node via the guard and
   * leaves the table untouched; a discovered duplicate likewise never
   * triggers the pending rehash.
   */
  template <class... Args>
  constexpr std::pair<iterator, bool> emplace_unique(Args&&... args) {
    table_node_type* node = create_node(std::forward<Args>(args)...);
    auto guard = detail::make_exception_guard([&] {
      destroy_node(node);
    });
    const key_type& key = key_of_value_(node->value);
    const std::size_t code = hasher_(key);
    const bool must_rehash = will_rehash_for_one_more();
    const unique_insert_position position = find_unique_insert_position(code, key);
    if (position.existing != nullptr) {
      return {iterator(position.existing), false};
    }
    if (must_rehash) {
      reserve_for_one_more();
      link_node(node, code);
    } else {
      link_node_at_bucket_tail(node, code, position.tail_link);
    }
    guard.complete();
    return {iterator(node), true};
  }

  /// Equivalent-key insertion, construct-first path.
  template <class... Args>
  constexpr iterator emplace_multi(Args&&... args) {
    table_node_type* node = create_node(std::forward<Args>(args)...);
    auto guard = detail::make_exception_guard([&] {
      destroy_node(node);
    });
    const key_type& key = key_of_value_(node->value);
    const std::size_t code = hasher_(key);
    hash_node_base* existing = find_node(code, key);
    reserve_for_one_more();
    link_node_multi(node, code, existing);
    guard.complete();
    return iterator(node);
  }

  /**
   * @brief Map-style try_emplace: key-first, piecewise construction on miss.
   *
   * The key is available up front, so a hit constructs nothing and leaves the
   * mapped-construction arguments untouched ([map.modifiers]). On a miss the
   * value is built as value_type(piecewise_construct, (key), (args...)); the
   * key is only moved from after hashing and lookup are done with it.
   */
  template <class K, class... Args>
  constexpr std::pair<iterator, bool> try_emplace_unique(K&& key, Args&&... args) {
    const std::size_t code = hasher_(key);
    const bool must_rehash = will_rehash_for_one_more();
    const unique_insert_position position = find_unique_insert_position(code, key);
    if (position.existing != nullptr) {
      return {iterator(position.existing), false};
    }
    table_node_type* node = create_node(std::piecewise_construct,
                                        std::forward_as_tuple(std::forward<K>(key)),
                                        std::forward_as_tuple(std::forward<Args>(args)...));
    auto guard = detail::make_exception_guard([&] {
      destroy_node(node);
    });
    if (must_rehash) {
      reserve_for_one_more();
      link_node(node, code);
    } else {
      link_node_at_bucket_tail(node, code, position.tail_link);
    }
    guard.complete();
    return {iterator(node), true};
  }

  /**
   * @brief Heterogeneous unique insert (P2363): key-first with a foreign key.
   *
   * Hashes and looks up @p key directly (transparent functors required at
   * the public surface), and only on a miss constructs value_type from it —
   * the expensive conversion P2363 exists to avoid never runs on a hit.
   */
  template <class K>
  constexpr std::pair<iterator, bool> insert_transparent_unique(K&& key) {
    const std::size_t code = hasher_(key);
    const bool must_rehash = will_rehash_for_one_more();
    const unique_insert_position position = find_unique_insert_position(code, key);
    if (position.existing != nullptr) {
      return {iterator(position.existing), false};
    }
    table_node_type* node = create_node(std::forward<K>(key));
    auto guard = detail::make_exception_guard([&] {
      destroy_node(node);
    });
    if (must_rehash) {
      reserve_for_one_more();
      link_node(node, code);
    } else {
      link_node_at_bucket_tail(node, code, position.tail_link);
    }
    guard.complete();
    return {iterator(node), true};
  }

  /// Heterogeneous equivalent-key insert (P2363-style extension for sets).
  template <class K>
  constexpr iterator insert_transparent_multi(K&& key) {
    const std::size_t code = hasher_(key);
    hash_node_base* existing = find_node(code, key);
    table_node_type* node = create_node(std::forward<K>(key));
    auto guard = detail::make_exception_guard([&] {
      destroy_node(node);
    });
    reserve_for_one_more();
    link_node_multi(node, code, existing);
    guard.complete();
    return iterator(node);
  }

  /**
   * @brief Erases the element at @p pos; returns the iterator past it.
   *
   * Calls no user code: the cached hash locates the bucket and the iteration
   * list provides the successor, both in O(1) ([unord.req] erase guarantees).
   */
  constexpr iterator erase(const_iterator pos) {
    hash_node_base* node = pos.base();
    CHAI_HARDENED(node != nullptr, "hash_table::erase: cannot erase end()");
    hash_node_base* next = node->next_in_order;
    unlink_node(node);
    destroy_node(static_cast<table_node_type*>(node));
    return iterator(next);
  }

  /// Erases the element with key equivalent to @p key, if any. Returns 0 or 1.
  template <class K>
  constexpr size_type erase_key(const K& key) {
    hash_node_base* node = find_node(hasher_(key), key);
    if (node == nullptr) {
      return 0;
    }
    unlink_node(node);
    destroy_node(static_cast<table_node_type*>(node));
    return 1;
  }

  /// Erases all elements with key equivalent to @p key.
  template <class K>
  constexpr size_type erase_multi(const K& key) {
    auto [first, last] = equal_range_multi(key);
    size_type removed = 0;
    while (first != last) {
      first = erase(first);
      ++removed;
    }
    return removed;
  }

  // ========================================================================
  // Node handles / merge
  // ========================================================================

  /// Detaches the element at @p pos into an allocator-preserving handle.
  constexpr node_handle_type extract(const_iterator pos) {
    hash_node_base* node = pos.base();
    CHAI_HARDENED(node != nullptr, "hash_table::extract: cannot extract end()");
    unlink_node(node);
    return node_handle_type(static_cast<table_node_type*>(node), node_allocator_);
  }

  /// Detaches the element with key equivalent to @p key, if any.
  template <class K>
  constexpr node_handle_type extract_key(const K& key) {
    hash_node_base* node = find_node(hasher_(key), key);
    if (node == nullptr) {
      return node_handle_type{};
    }
    return extract(const_iterator(node));
  }

  /**
   * @brief Adopts the node owned by @p handle (unique keys).
   *
   * The hash is recomputed with this table's hash function — the handle's
   * cached value may come from a different stateful hasher and is never
   * trusted across containers (ADR: Hash Table Design). On a duplicate or a
   * rehash-allocation failure the handle keeps ownership of the node.
   *
   * @pre The handle's allocator equals this container's allocator.
   */
  constexpr std::pair<iterator, bool> insert_node_unique(node_handle_type& handle) {
    if (handle.empty()) {
      return {end(), false};
    }
    CHAI_HARDENED(allocator_type(node_allocator_) == handle.get_allocator(),
                  "hash_table::insert_node_unique: allocator mismatch");
    table_node_type* node = handle.ptr();
    const key_type& key = key_of_value_(node->value);
    const std::size_t code = hasher_(key);
    const bool must_rehash = will_rehash_for_one_more();
    const unique_insert_position position = find_unique_insert_position(code, key);
    if (position.existing != nullptr) {
      return {iterator(position.existing), false};
    }
    if (must_rehash) {
      reserve_for_one_more();  // throws: the handle still owns the node
      link_node(node, code);
    } else {
      link_node_at_bucket_tail(node, code, position.tail_link);
    }
    (void)handle.release();
    return {iterator(node), true};
  }

  /// Adopts the node owned by @p handle (equivalent keys).
  constexpr iterator insert_node_multi(node_handle_type& handle) {
    if (handle.empty()) {
      return end();
    }
    CHAI_HARDENED(allocator_type(node_allocator_) == handle.get_allocator(),
                  "hash_table::insert_node_multi: allocator mismatch");
    table_node_type* node = handle.ptr();
    const key_type& key = key_of_value_(node->value);
    const std::size_t code = hasher_(key);
    hash_node_base* existing = find_node(code, key);
    reserve_for_one_more();  // throws: the handle still owns the node
    link_node_multi(node, code, existing);
    (void)handle.release();
    return iterator(node);
  }

  /**
   * @brief Splices every element of @p source whose key is absent here.
   *
   * Nodes change owners without copying or moving values, so element
   * addresses are stable across the merge. Keys are re-hashed with this
   * table's hash function (the sources' hasher may differ). An exception —
   * Hash, KeyEqual, or a rehash allocation — leaves the current element
   * still in @p source.
   *
   * @pre Equal allocators (the standard's splicing precondition).
   */
  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge_unique(hash_table<Key, Value, KeyOfValue, Hash2, KeyEqual2, Allocator, RehashPolicy2>& source) {
    CHAI_HARDENED(node_allocator_ == source.node_allocator_, "hash_table::merge_unique: allocator mismatch");
    hash_node_base* node = source.order_head_;
    while (node != nullptr) {
      hash_node_base* next = node->next_in_order;
      table_node_type* typed = static_cast<table_node_type*>(node);
      const key_type& key = key_of_value_(typed->value);
      const std::size_t code = hasher_(key);
      const bool must_rehash = will_rehash_for_one_more();
      const unique_insert_position position = find_unique_insert_position(code, key);
      if (position.existing == nullptr) {
        if (must_rehash) {
          reserve_for_one_more();  // throws: the node is still in source
          source.unlink_node(node);
          link_node(typed, code);
        } else {
          source.unlink_node(node);
          link_node_at_bucket_tail(typed, code, position.tail_link);
        }
      }
      node = next;
    }
  }

  /// Splices every element of @p source, preserving equivalent-key groups.
  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge_multi(hash_table<Key, Value, KeyOfValue, Hash2, KeyEqual2, Allocator, RehashPolicy2>& source) {
    CHAI_HARDENED(node_allocator_ == source.node_allocator_, "hash_table::merge_multi: allocator mismatch");
    hash_node_base* node = source.order_head_;
    while (node != nullptr) {
      hash_node_base* next = node->next_in_order;
      table_node_type* typed = static_cast<table_node_type*>(node);
      const key_type& key = key_of_value_(typed->value);
      const std::size_t code = hasher_(key);
      hash_node_base* existing = find_node(code, key);
      reserve_for_one_more();  // throws: the node is still in source
      source.unlink_node(node);
      link_node_multi(typed, code, existing);
      node = next;
    }
  }

  /// Destroys all elements; keeps the bucket array (capacity is retained).
  constexpr void clear() noexcept {
    clear_nodes();
    hash_node_base** bucket_heads = buckets();
    for (size_type i = 0; i < bucket_count_; ++i) {
      bucket_heads[i] = nullptr;
    }
  }

  // ========================================================================
  // Lookup
  // ========================================================================

  template <class K>
  [[nodiscard]] constexpr iterator find(const K& key) {
    return iterator(find_node(hasher_(key), key));
  }

  template <class K>
  [[nodiscard]] constexpr const_iterator find(const K& key) const {
    return const_iterator(find_node(hasher_(key), key));
  }

  template <class K>
  [[nodiscard]] constexpr size_type count(const K& key) const {
    return find_node(hasher_(key), key) != nullptr ? 1 : 0;
  }

  template <class K>
  [[nodiscard]] constexpr size_type count_multi(const K& key) const {
    auto [first, last] = equal_range_multi(key);
    size_type result = 0;
    for (; first != last; ++first) {
      ++result;
    }
    return result;
  }

  template <class K>
  [[nodiscard]] constexpr bool contains(const K& key) const {
    return find_node(hasher_(key), key) != nullptr;
  }

  template <class K>
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const K& key) {
    if (hash_node_base* node = find_node(hasher_(key), key)) {
      return {iterator(node), iterator(node->next_in_order)};
    }
    return {end(), end()};
  }

  template <class K>
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
    if (hash_node_base* node = find_node(hasher_(key), key)) {
      return {const_iterator(node), const_iterator(node->next_in_order)};
    }
    return {end(), end()};
  }

  template <class K>
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range_multi(const K& key) {
    hash_node_base* first = find_node(hasher_(key), key);
    if (first == nullptr) {
      return {end(), end()};
    }
    hash_node_base* last = first->next_in_order;
    while (last != nullptr && key_equal_(key_of_value_(static_cast<table_node_type*>(last)->value), key)) {
      last = last->next_in_order;
    }
    return {iterator(first), iterator(last)};
  }

  template <class K>
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range_multi(const K& key) const {
    hash_node_base* first = find_node(hasher_(key), key);
    if (first == nullptr) {
      return {end(), end()};
    }
    hash_node_base* last = first->next_in_order;
    while (last != nullptr && key_equal_(key_of_value_(static_cast<table_node_type*>(last)->value), key)) {
      last = last->next_in_order;
    }
    return {const_iterator(first), const_iterator(last)};
  }

  /**
   * @brief Set-semantics equality ([unord.req.general]).
   *
   * Looks every element up in @p other with @e other's hash function — the
   * standard only requires the predicates to agree, not the hashers.
   */
  [[nodiscard]] constexpr bool equals(const hash_table& other) const
    requires std::equality_comparable<value_type>
  {
    if (size_ != other.size_) {
      return false;
    }
    for (const hash_node_base* node = order_head_; node != nullptr; node = node->next_in_order) {
      const value_type& value = static_cast<const table_node_type*>(node)->value;
      const_iterator match = other.find(key_of_value_(value));
      if (match == other.end() || !(*match == value)) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Equivalent-key container equality.
   *
   * For each key group, the other table must have an equal-size group with
   * the same value multiset. Group order and within-group order are ignored.
   */
  [[nodiscard]] constexpr bool equals_multi(const hash_table& other) const
    requires std::equality_comparable<value_type>
  {
    if (size_ != other.size_) {
      return false;
    }
    for (const hash_node_base* group = order_head_; group != nullptr;) {
      const value_type& first_value = static_cast<const table_node_type*>(group)->value;
      const key_type& key = key_of_value_(first_value);
      auto [other_first_it, other_last_it] = other.equal_range_multi(key);
      const hash_node_base* other_first = other_first_it.base();
      const hash_node_base* other_last = other_last_it.base();
      if (other_first == nullptr) {
        return false;
      }

      const hash_node_base* group_last = group->next_in_order;
      size_type group_size = 1;
      while (group_last != nullptr &&
             key_equal_(key_of_value_(static_cast<const table_node_type*>(group_last)->value), key)) {
        group_last = group_last->next_in_order;
        ++group_size;
      }

      size_type other_group_size = 0;
      for (const hash_node_base* node = other_first; node != other_last; node = node->next_in_order) {
        ++other_group_size;
      }
      if (group_size != other_group_size) {
        return false;
      }

      for (const hash_node_base* node = group; node != group_last; node = node->next_in_order) {
        const value_type& value = static_cast<const table_node_type*>(node)->value;
        bool seen = false;
        for (const hash_node_base* prev = group; prev != node; prev = prev->next_in_order) {
          if (static_cast<const table_node_type*>(prev)->value == value) {
            seen = true;
            break;
          }
        }
        if (seen) {
          continue;
        }
        size_type lhs_count = 0;
        for (const hash_node_base* scan = node; scan != group_last; scan = scan->next_in_order) {
          if (static_cast<const table_node_type*>(scan)->value == value) {
            ++lhs_count;
          }
        }
        size_type rhs_count = 0;
        for (const hash_node_base* scan = other_first; scan != other_last; scan = scan->next_in_order) {
          if (static_cast<const table_node_type*>(scan)->value == value) {
            ++rhs_count;
          }
        }
        if (lhs_count != rhs_count) {
          return false;
        }
      }

      group = group_last;
    }
    return true;
  }

  // ========================================================================
  // Bucket interface
  // ========================================================================

  [[nodiscard]] constexpr size_type bucket_count() const noexcept { return bucket_count_; }

  [[nodiscard]] constexpr size_type max_bucket_count() const noexcept { return rehash_policy_.max_bucket_count(); }

  [[nodiscard]] constexpr size_type bucket_size(size_type index) const noexcept {
    CHAI_HARDENED(index < bucket_count_, "hash_table::bucket_size: index out of range");
    size_type count = 0;
    for (const hash_node_base* node = buckets()[index]; node != nullptr; node = node->next_in_bucket) {
      ++count;
    }
    return count;
  }

  template <class K>
  [[nodiscard]] constexpr size_type bucket(const K& key) const {
    CHAI_HARDENED(bucket_count_ > 0, "hash_table::bucket: table has no buckets");
    return rehash_policy_.bucket_for_hash(hasher_(key), bucket_count_);
  }

  [[nodiscard]] constexpr local_iterator begin(size_type index) noexcept {
    CHAI_HARDENED(index < bucket_count_, "hash_table::begin(n): index out of range");
    return local_iterator(buckets()[index]);
  }

  [[nodiscard]] constexpr const_local_iterator begin(size_type index) const noexcept {
    CHAI_HARDENED(index < bucket_count_, "hash_table::begin(n): index out of range");
    return const_local_iterator(buckets()[index]);
  }

  [[nodiscard]] constexpr local_iterator end(size_type index) noexcept {
    CHAI_HARDENED(index < bucket_count_, "hash_table::end(n): index out of range");
    (void)index;
    return local_iterator{};
  }

  [[nodiscard]] constexpr const_local_iterator end(size_type index) const noexcept {
    CHAI_HARDENED(index < bucket_count_, "hash_table::end(n): index out of range");
    (void)index;
    return const_local_iterator{};
  }

  // ========================================================================
  // Hash policy
  // ========================================================================

  [[nodiscard]] constexpr float load_factor() const noexcept {
    return bucket_count_ == 0 ? 0.0f : static_cast<float>(size_) / static_cast<float>(bucket_count_);
  }

  [[nodiscard]] constexpr float max_load_factor() const noexcept { return max_load_factor_; }

  /// Stores the bound. Never rehashes by itself ([unord.req]: a hint).
  constexpr void max_load_factor(float factor) noexcept {
    CHAI_HARDENED(factor > 0.0f, "hash_table::max_load_factor: bound must be positive");
    max_load_factor_ = factor;
  }

  /**
   * @brief Ensures `bucket_count() >= max(request, size() / max_load_factor())`.
   *
   * Migration relinks bucket chains from cached hashes and never touches the
   * iteration list: the only failure point is the new bucket array's
   * allocation, before which the table is untouched — the strong guarantee
   * required by [unord.req.except] falls out of the layout.
   */
  constexpr void rehash(size_type bucket_count_request) {
    const size_type required = std::max(bucket_count_request, minimum_bucket_count(size_));
    if (required == 0) {
      return;  // any table, including the zero-bucket one, satisfies this
    }
    const size_type target = rehash_policy_.next_bucket_count(required);
    if (target != bucket_count_) {
      rebuild_bucket_array(target);
    }
  }

  /// Prepares for @p element_count elements without intervening rehashes.
  constexpr void reserve(size_type element_count) { rehash(minimum_bucket_count(element_count)); }

  // ========================================================================
  // Validation
  // ========================================================================

  /**
   * @brief Cross-checks the two link structures against each other.
   *
   * Verifies that the iteration list is well-formed (back links, head/tail,
   * size), that every listed node sits in the chain of the bucket its cached
   * hash selects, that the cached hash matches a fresh evaluation of the
   * hash function, and that the chains contain no nodes outside the list.
   * The orthogonal-structures layout is what makes this mutual audit cheap
   * (ADR: Hash Table Design).
   */
  [[nodiscard]] constexpr bool validate() const {
    if (bucket_count_ == 0) {
      return buckets_ == bucket_pointer{} && size_ == 0 && order_head_ == nullptr && order_tail_ == nullptr;
    }
    size_type order_count = 0;
    const hash_node_base* previous = nullptr;
    for (const hash_node_base* node = order_head_; node != nullptr; node = node->next_in_order) {
      if (node->prev_in_order != previous) {
        return false;
      }
      const value_type& value = static_cast<const table_node_type*>(node)->value;
      if (hasher_(key_of_value_(value)) != node->cached_hash) {
        return false;  // stale hash cache
      }
      const size_type index = rehash_policy_.bucket_for_hash(node->cached_hash, bucket_count_);
      bool in_chain = false;
      for (const hash_node_base* chain = buckets()[index]; chain != nullptr; chain = chain->next_in_bucket) {
        if (chain == node) {
          in_chain = true;
          break;
        }
      }
      if (!in_chain) {
        return false;
      }
      previous = node;
      ++order_count;
    }
    if (order_tail_ != previous || order_count != size_) {
      return false;
    }
    size_type chain_count = 0;
    for (size_type i = 0; i < bucket_count_; ++i) {
      for (const hash_node_base* node = buckets()[i]; node != nullptr; node = node->next_in_bucket) {
        ++chain_count;
      }
    }
    return chain_count == size_;
  }

 private:
  // merge_unique splices from tables that differ only in Hash/KeyEqual.
  template <class K2, class V2, class X2, class H2, class E2, class A2, class R2>
    requires concepts::container_element<V2> && concepts::allocator_for<A2, V2> && hasher_for<H2, K2> &&
             std::predicate<const E2&, const K2&, const K2&>
  friend class hash_table;

  struct unique_insert_position {
    hash_node_base* existing = nullptr;
    hash_node_base** tail_link = nullptr;
  };

  // ========================================================================
  // Node lifetime
  // ========================================================================

  template <class... Args>
  [[nodiscard]] constexpr table_node_type* create_node(Args&&... args) {
    detail::node_owner<node_allocator_type> storage(node_allocator_);
    table_node_type* node = storage.get();
    std::construct_at(node);  // start the node lifetime; value stays inactive
    auto node_lifetime = detail::make_exception_guard([&] {
      std::destroy_at(node);
    });
    node_allocator_traits::construct(node_allocator_, std::addressof(node->value), std::forward<Args>(args)...);
    node_lifetime.complete();
    return storage.release();
  }

  constexpr void destroy_node(table_node_type* node) noexcept {
    node_allocator_traits::destroy(node_allocator_, std::addressof(node->value));
    std::destroy_at(node);
    node_allocator_traits::deallocate(
        node_allocator_, std::pointer_traits<typename node_allocator_traits::pointer>::pointer_to(*node), 1);
  }

  // ========================================================================
  // Bucket array lifetime
  // ========================================================================

  [[nodiscard]] constexpr hash_node_base** buckets() const noexcept { return std::to_address(buckets_); }

  [[nodiscard]] constexpr bucket_pointer allocate_bucket_array(size_type count) {
    bucket_allocator_type bucket_allocator(node_allocator_);
    bucket_pointer storage = bucket_allocator_traits::allocate(bucket_allocator, count);
    hash_node_base** raw = std::to_address(storage);
    for (size_type i = 0; i < count; ++i) {
      std::construct_at(raw + i);  // value-initialize each head to nullptr
    }
    return storage;
  }

  constexpr void deallocate_bucket_array(bucket_pointer storage, size_type count) noexcept {
    bucket_allocator_type bucket_allocator(node_allocator_);
    bucket_allocator_traits::deallocate(bucket_allocator, storage, count);
  }

  constexpr void deallocate_buckets() noexcept {
    if (buckets_ != bucket_pointer{}) {
      deallocate_bucket_array(buckets_, bucket_count_);
      buckets_ = bucket_pointer{};
      bucket_count_ = 0;
    }
  }

  /// @pre the table owns no bucket array.
  constexpr void allocate_initial_buckets(size_type count) {
    buckets_ = allocate_bucket_array(count);
    bucket_count_ = count;
  }

  // ========================================================================
  // Linking
  // ========================================================================

  /// Links a detached unique-key node into both structures. No user code; cannot fail.
  constexpr void link_node(table_node_type* node, std::size_t code) noexcept {
    node->cached_hash = code;
    link_bucket_tail(bucket_head_for(code), node);
    link_order_tail(node);
    ++size_;
  }

  constexpr void link_node_at_bucket_tail(table_node_type* node,
                                          std::size_t code,
                                          hash_node_base** tail_link) noexcept {
    CHAI_HARDENED(tail_link != nullptr, "hash_table::link_node_at_bucket_tail: missing tail link");
    CHAI_HARDENED(*tail_link == nullptr, "hash_table::link_node_at_bucket_tail: tail link must be empty");
    node->cached_hash = code;
    node->next_in_bucket = nullptr;
    *tail_link = node;
    link_order_tail(node);
    ++size_;
  }

  /// Links a detached equivalent-key node after the existing key segment.
  constexpr void link_node_multi(table_node_type* node, std::size_t code, hash_node_base* existing) noexcept {
    node->cached_hash = code;
    if (existing == nullptr) {
      link_node(node, code);
      return;
    }

    const key_type& key = key_of_value_(node->value);
    link_order_after(last_equivalent_in_order(existing, key), node);
    link_bucket_after(last_equivalent_in_bucket(existing, code, key), node);
    ++size_;
  }

  /// Unlinks @p node from both structures. No user code; cannot fail.
  constexpr void unlink_node(hash_node_base* node) noexcept {
    // Bucket chain: the cached hash locates the bucket without calling Hash.
    hash_node_base** link = std::addressof(bucket_head_for(node->cached_hash));
    while (*link != node) {
      link = &(*link)->next_in_bucket;
    }
    *link = node->next_in_bucket;
    // Iteration list.
    if (node->prev_in_order != nullptr) {
      node->prev_in_order->next_in_order = node->next_in_order;
    } else {
      order_head_ = node->next_in_order;
    }
    if (node->next_in_order != nullptr) {
      node->next_in_order->prev_in_order = node->prev_in_order;
    } else {
      order_tail_ = node->prev_in_order;
    }
    --size_;
  }

  [[nodiscard]] constexpr size_type bucket_index_for(std::size_t code) const noexcept {
    return rehash_policy_.bucket_for_hash(code, bucket_count_);
  }

  [[nodiscard]] constexpr hash_node_base*& bucket_head_for(std::size_t code) noexcept {
    return buckets()[bucket_index_for(code)];
  }

  constexpr void link_order_tail(hash_node_base* node) noexcept {
    node->prev_in_order = order_tail_;
    node->next_in_order = nullptr;
    if (order_tail_ != nullptr) {
      order_tail_->next_in_order = node;
    } else {
      order_head_ = node;
    }
    order_tail_ = node;
  }

  constexpr void link_order_after(hash_node_base* position, hash_node_base* node) noexcept {
    node->prev_in_order = position;
    node->next_in_order = position->next_in_order;
    if (position->next_in_order != nullptr) {
      position->next_in_order->prev_in_order = node;
    } else {
      order_tail_ = node;
    }
    position->next_in_order = node;
  }

  static constexpr void link_bucket_tail(hash_node_base*& head, hash_node_base* node) noexcept {
    node->next_in_bucket = nullptr;
    if (head == nullptr) {
      head = node;
      return;
    }
    hash_node_base* tail = head;
    while (tail->next_in_bucket != nullptr) {
      tail = tail->next_in_bucket;
    }
    tail->next_in_bucket = node;
  }

  static constexpr void link_bucket_after(hash_node_base* position, hash_node_base* node) noexcept {
    node->next_in_bucket = position->next_in_bucket;
    position->next_in_bucket = node;
  }

  [[nodiscard]] constexpr hash_node_base* last_equivalent_in_order(hash_node_base* first,
                                                                   const key_type& key) const noexcept {
    hash_node_base* last = first;
    while (last->next_in_order != nullptr &&
           key_equal_(key_of_value_(static_cast<table_node_type*>(last->next_in_order)->value), key)) {
      last = last->next_in_order;
    }
    return last;
  }

  [[nodiscard]] constexpr hash_node_base* last_equivalent_in_bucket(hash_node_base* first,
                                                                    std::size_t code,
                                                                    const key_type& key) const noexcept {
    hash_node_base* last = first;
    while (last->next_in_bucket != nullptr && last->next_in_bucket->cached_hash == code &&
           key_equal_(key_of_value_(static_cast<table_node_type*>(last->next_in_bucket)->value), key)) {
      last = last->next_in_bucket;
    }
    return last;
  }

  // ========================================================================
  // Lookup / rehash internals
  // ========================================================================

  template <class K>
  [[nodiscard]] constexpr hash_node_base* find_node(std::size_t code, const K& key) const {
    if (bucket_count_ == 0) {
      return nullptr;
    }
    const size_type index = bucket_index_for(code);
    for (hash_node_base* node = buckets()[index]; node != nullptr; node = node->next_in_bucket) {
      // Full-hash prefilter: KeyEqual only runs on hash-equal candidates.
      if (node->cached_hash == code && key_equal_(key_of_value_(static_cast<table_node_type*>(node)->value), key)) {
        return node;
      }
    }
    return nullptr;
  }

  // A unique insert miss can reuse the null link reached by lookup as the
  // bucket insertion point. That link is only stable when the insert will not
  // rehash; callers deliberately fall back to link_node() after growth.
  template <class K>
  [[nodiscard]] constexpr unique_insert_position find_unique_insert_position(std::size_t code, const K& key) {
    if (bucket_count_ == 0) {
      return {};
    }
    hash_node_base** link = std::addressof(buckets()[bucket_index_for(code)]);
    while (*link != nullptr) {
      hash_node_base* node = *link;
      if (node->cached_hash == code && key_equal_(key_of_value_(static_cast<table_node_type*>(node)->value), key)) {
        return {.existing = node};
      }
      link = std::addressof(node->next_in_bucket);
    }
    return {.tail_link = link};
  }

  /// Smallest bucket count b with `element_count <= max_load_factor() * b`.
  [[nodiscard]] constexpr size_type minimum_bucket_count(size_type element_count) const noexcept {
    size_type required = static_cast<size_type>(static_cast<float>(element_count) / max_load_factor_);
    while (static_cast<float>(element_count) > max_load_factor_ * static_cast<float>(required)) {
      ++required;  // repair float rounding; at most a few steps
    }
    return required;
  }

  /// Grows the bucket array if one more element would exceed the load bound.
  constexpr void reserve_for_one_more() {
    if (will_rehash_for_one_more()) {
      rebuild_bucket_array(rehash_policy_.next_bucket_count(minimum_bucket_count(size_ + 1)));
    }
  }

  [[nodiscard]] constexpr bool will_rehash_for_one_more() const noexcept {
    return rehash_policy_.need_rehash(size_, 1, bucket_count_, max_load_factor_);
  }

  constexpr void rebuild_bucket_array(size_type new_bucket_count) {
    bucket_pointer new_buckets = allocate_bucket_array(new_bucket_count);
    auto new_buckets_guard = detail::make_exception_guard([&] {
      deallocate_bucket_array(new_buckets, new_bucket_count);
    });

    deallocate_buckets();
    buckets_ = new_buckets;
    bucket_count_ = new_bucket_count;
    new_buckets_guard.complete();

    // Relink every chain from the iteration list, which itself never changes:
    // iteration order is stable across rehash. Only cached hashes are read,
    // so no user code runs and migration cannot fail.
    //
    // Walk backwards and push each node at its new bucket head. This preserves
    // the same per-bucket order as a forward walk with tail pointers, but
    // avoids allocating a parallel tail array.
    for (hash_node_base* node = order_tail_; node != nullptr; node = node->prev_in_order) {
      size_type bucket_index = rehash_policy_.bucket_for_hash(node->cached_hash, bucket_count_);
      node->next_in_bucket = buckets()[bucket_index];
      buckets()[bucket_index] = node;
    }
  }

  // ========================================================================
  // Whole-table helpers
  // ========================================================================

  constexpr void clear_nodes() noexcept {
    hash_node_base* node = order_head_;
    while (node != nullptr) {
      hash_node_base* next = node->next_in_order;
      destroy_node(static_cast<table_node_type*>(node));
      node = next;
    }
    order_head_ = nullptr;
    order_tail_ = nullptr;
    size_ = 0;
  }

  constexpr void destroy_and_deallocate_storage() noexcept {
    clear_nodes();
    deallocate_buckets();
  }

  /// @pre *this owns no storage (freshly constructed or already destroyed).
  constexpr void take_storage_from(hash_table& other) noexcept {
    buckets_ = std::exchange(other.buckets_, bucket_pointer{});
    bucket_count_ = std::exchange(other.bucket_count_, 0);
    order_head_ = std::exchange(other.order_head_, nullptr);
    order_tail_ = std::exchange(other.order_tail_, nullptr);
    size_ = std::exchange(other.size_, 0);
  }

  /**
   * @brief Rebuilds this (empty) table as a copy of @p other.
   *
   * Reuses the source's cached hash codes: this table's hash function was
   * just copied from the source, and equal hashers hash equal keys equally —
   * so a copy never calls the user's Hash. The bucket count is preserved,
   * which keeps the copy's observable bucket geometry predictable.
   */
  constexpr void copy_from(const hash_table& other) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_storage();
    });
    if (other.bucket_count_ > 0) {
      allocate_initial_buckets(other.bucket_count_);
    }
    for (const hash_node_base* source = other.order_head_; source != nullptr; source = source->next_in_order) {
      table_node_type* node = create_node(static_cast<const table_node_type*>(source)->value);
      link_node(node, source->cached_hash);
    }
    guard.complete();
  }

  /// Rebuilds this (empty) table by moving values out of @p other's nodes.
  constexpr void move_values_from(hash_table& other) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_storage();
    });
    if (other.bucket_count_ > 0) {
      allocate_initial_buckets(other.bucket_count_);
    }
    for (hash_node_base* source = other.order_head_; source != nullptr; source = source->next_in_order) {
      table_node_type* node = create_node(std::move(static_cast<table_node_type*>(source)->value));
      link_node(node, source->cached_hash);
    }
    guard.complete();
  }

  // ========================================================================
  // Data members
  // ========================================================================

  bucket_pointer buckets_{};
  size_type bucket_count_ = 0;
  hash_node_base* order_head_ = nullptr;
  hash_node_base* order_tail_ = nullptr;
  size_type size_ = 0;
  float max_load_factor_ = 1.0f;
  [[no_unique_address]] KeyOfValue key_of_value_{};
  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual key_equal_{};
  [[no_unique_address]] RehashPolicy rehash_policy_{};
  [[no_unique_address]] node_allocator_type node_allocator_{};
};

}  // namespace chaistl::detail::hash
