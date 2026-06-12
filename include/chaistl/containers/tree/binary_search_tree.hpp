// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// binary_search_tree - Shared ordered-associative tree engine
// ============================================================================
//
// Architecture:
//   - Owns allocation, lookup, iteration, node handles, copy/move/swap, and
//     the unique-key/equivalent-key operation families.
//   - Delegates rotations and balance metadata to a policy. rb_tree_policy,
//     avl_tree_policy, and treap_tree_policy all satisfy the same structural
//     contract.
//   - Uses a header sentinel holding root, leftmost, and rightmost pointers so
//     begin(), end(), and root rotations are simple and cheap.
//
// Standardization archaeology:
//   - The standard specifies ordered associative containers by behavior and
//     complexity, not by tree species. Red-black trees became the dominant
//     implementation strategy, but the wording leaves AVL-like alternatives
//     legal.
//   - C++17 node handles exposed a formerly internal operation: detaching a
//     tree node while preserving its allocation and value object.
//   - The unique/equivalent split mirrors the standard's set/map versus
//     multiset/multimap families while sharing one node and iterator model.
//
// Non-standard extensions:
//   - The public containers expose the balance policy as a final template
//     argument so tests and benchmarks can compare rb, avl, and treap behavior
//     without duplicating container wrappers.
//
// References:
//   - C++ Draft ordered associative containers: https://eel.is/c++draft/associative
//   - libstdc++ _Rb_tree: gcc/libstdc++-v3/include/bits/stl_tree.h
//   - ADR-004: Tree Policy-Based Design

#include <chaistl/algorithm/comparison.hpp>
#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/basic.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/containers/tree/algorithm.hpp>
#include <chaistl/containers/tree/iterator.hpp>
#include <chaistl/containers/tree/key_extract.hpp>
#include <chaistl/containers/tree/node.hpp>
#include <chaistl/containers/tree/node_handle.hpp>
#include <chaistl/iterator/adapter/reverse.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>
#include <chaistl/memory/detail/storage/node_owner.hpp>
#include <chaistl/meta/type_traits.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::detail::tree {

// ============================================================================
// Balance Policy Concept
// ============================================================================
//
// A policy owns the two structural mutations of the tree and the per-node
// state they need. The tree core never inspects balancing state; everything
// it does (position search, allocation, lookup, iteration, copy/move/swap)
// is policy-independent.
//
// Contract:
//   - node_extension: per-node balancing state; [[no_unique_address]]
//     makes an empty extension free. Must be trivially default constructible
//     and trivially copyable (the tree creates nodes without starting their
//     lifetime, so default initializers never run).
//   - insert_and_rebalance(insert_left, node, parent, header): link a
//     detached node under parent, maintain header pointers, restore invariants.
//   - unlink_and_rebalance(node, header): remove node, maintain headers,
//     restore invariants, return the node to destroy.
//   - verify(root): check balance invariants (for tests).
//
// Key design decision: header is passed by reference, not by value.
// Root, leftmost and rightmost live in the header; rotations deep inside
// the policy can retarget the root in place. Protocols that pass a copy
// of the root pointer cannot express a rotation at the root.
//
// Reference: libstdc++ _Rb_tree_insert_and_rebalance / _Rb_tree_rebalance_for_erase
// ADR-004: Tree Policy-Based Design
template <class P, class Node>
concept balance_policy =
    std::semiregular<P> && requires { typename P::node_extension; } &&
    std::is_trivially_default_constructible_v<typename P::node_extension> &&
    std::is_trivially_copyable_v<typename P::node_extension> &&
    requires(P policy, bool insert_left, Node* node, bst_node_base* parent, bst_node_base& header) {
      { policy.insert_and_rebalance(insert_left, node, parent, header) } noexcept;
      { policy.unlink_and_rebalance(node, header) } noexcept -> std::same_as<bst_node_base*>;
    } && requires(const P policy, const Node* node) {
      { policy.verify(node) } noexcept -> std::same_as<bool>;
    };

/**
 * @brief Optional lookup-access capability for self-adjusting trees.
 *
 * A policy that models this concept may restructure the tree after a lookup
 * descent. @p node is the last real node visited by the search, never the
 * header sentinel. RB/AVL/Treap policies intentionally do not model it; Splay
 * uses it without accessing binary_search_tree internals.
 */
template <class P, class Node>
concept access_rebalance_policy = balance_policy<P, Node> && requires(P policy, Node* node, bst_node_base& header) {
  { policy.after_access(node, header) } noexcept -> std::same_as<void>;
};

/**
 * @brief Optional subtree-size metadata capability for augmented trees.
 *
 * Policies model this by maintaining each node's subtree size under the
 * conventional `extension.subtree_size` name. This is useful both for
 * balancing policies (size/weight-balanced trees) and for public rank/select
 * operations.
 */
template <class P, class Node>
concept subtree_size_policy = balance_policy<P, Node> && requires(const Node* node) {
  { node->extension.subtree_size } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Optional rank/select capability for augmented trees.
 *
 * Today every subtree-size policy can answer find_by_order(k) and
 * order_of_key(key). Keeping this named concept documents the public
 * operation family separately from the metadata it relies on.
 */
template <class P, class Node>
concept order_statistic_policy = subtree_size_policy<P, Node>;

// ============================================================================
// Generic Binary Search Tree
// ============================================================================

/**
 * @brief Generic binary search tree with pluggable balance policy.
 *
 * This is the core implementation shared by set, map, multiset and
 * multimap. It handles the policy-independent logic — position search,
 * node lifetime, lookup, iteration, copy/move/swap — and delegates every
 * structural mutation to the @p Policy (see @ref balance_policy).
 *
 * Mutating and counting operations come in two families sharing one node
 * layout and policy contract: the @c *_unique family refuses equivalent
 * keys (set/map), the @c *_equal / @c *_multi family accepts them
 * (multiset/multimap). The unique-key wrappers must never call the equal
 * family and vice versa — lookup helpers shared by both (find,
 * lower/upper_bound) are written to be correct under either key regime.
 * The iterator-range constructors insert with the unique family; the
 * multi containers construct empty and bulk-insert via insert_equal.
 *
 * @tparam Key        The key type used for ordering.
 * @tparam Value      The stored value type (same as Key for set, pair for map).
 * @tparam KeyOfValue Extracts a key from a value.
 * @tparam Compare    Strict weak ordering on keys.
 * @tparam Allocator  Allocator for node storage.
 * @tparam Policy     Balance policy (e.g. rb_tree_policy).
 */
template <class Key, class Value, class KeyOfValue, class Compare, class Allocator, class Policy>
  requires concepts::container_element<Value> && concepts::allocator_for<Allocator, Value> &&
           balance_policy<Policy, bst_node<Value, Policy>>
class binary_search_tree {
 public:
  // ========================================================================
  // Member types
  // ========================================================================

  using key_type = Key;
  using value_type = Value;
  using key_of_value_type = KeyOfValue;
  using key_compare = Compare;
  using value_compare = Compare;  // For set, same as key_compare
  using allocator_type = Allocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using size_type = typename allocator_traits::size_type;
  using difference_type = typename allocator_traits::difference_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename allocator_traits::pointer;
  using const_pointer = typename allocator_traits::const_pointer;

  using tree_node_type = bst_node<value_type, Policy>;
  using node_allocator_type = typename allocator_traits::template rebind_alloc<tree_node_type>;
  using node_allocator_traits = std::allocator_traits<node_allocator_type>;

  using node_handle_type = bst_node_handle<value_type, Policy, allocator_type>;

  using iterator = bst_iterator<value_type, false>;
  using const_iterator = bst_iterator<value_type, true>;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;

  // ========================================================================
  // Constructors and destructor
  // ========================================================================

  constexpr binary_search_tree() noexcept(std::is_nothrow_default_constructible_v<Compare> &&
                                          std::is_nothrow_default_constructible_v<node_allocator_type>)
      : key_compare_(), node_allocator_() {
    init_header();
  }

  explicit constexpr binary_search_tree(const Compare& comp, const Allocator& alloc = Allocator{})
      : key_compare_(comp), node_allocator_(alloc) {
    init_header();
  }

  explicit constexpr binary_search_tree(const Allocator& alloc) : key_compare_(), node_allocator_(alloc) {
    init_header();
  }

  template <std::input_iterator InputIt>
  constexpr binary_search_tree(InputIt first,
                               InputIt last,
                               const Compare& comp = Compare{},
                               const Allocator& alloc = Allocator{})
      : key_compare_(comp), node_allocator_(alloc) {
    init_header();
    auto guard = detail::make_exception_guard([this] {
      clear();
    });
    insert_unique(first, last);
    guard.complete();
  }

  template <std::input_iterator InputIt>
  constexpr binary_search_tree(InputIt first, InputIt last, const Allocator& alloc)
      : binary_search_tree(first, last, Compare{}, alloc) {}

  constexpr binary_search_tree(std::initializer_list<value_type> init,
                               const Compare& comp = Compare{},
                               const Allocator& alloc = Allocator{})
      : binary_search_tree(init.begin(), init.end(), comp, alloc) {}

  constexpr binary_search_tree(std::initializer_list<value_type> init, const Allocator& alloc)
      : binary_search_tree(init.begin(), init.end(), Compare{}, alloc) {}

  constexpr binary_search_tree(const binary_search_tree& other)
      : key_compare_(other.key_compare_),
        policy_(other.policy_),
        node_allocator_(node_allocator_traits::select_on_container_copy_construction(other.node_allocator_)) {
    init_header();
    copy_from(other);
  }

  constexpr binary_search_tree(const binary_search_tree& other, const Allocator& alloc)
      : key_compare_(other.key_compare_), policy_(other.policy_), node_allocator_(alloc) {
    init_header();
    copy_from(other);
  }

  constexpr binary_search_tree(binary_search_tree&& other) noexcept(std::is_nothrow_move_constructible_v<Compare> &&
                                                                    std::is_nothrow_move_constructible_v<Policy>)
      : key_compare_(std::move(other.key_compare_)),
        policy_(std::move(other.policy_)),
        node_allocator_(std::move(other.node_allocator_)),
        size_(std::exchange(other.size_, 0)) {
    init_header();
    steal_header_from(other);
  }

  constexpr binary_search_tree(binary_search_tree&& other, const Allocator& alloc)
      : key_compare_(std::move(other.key_compare_)), policy_(std::move(other.policy_)), node_allocator_(alloc) {
    init_header();
    if (node_allocator_ == other.node_allocator_) {
      size_ = std::exchange(other.size_, 0);
      steal_header_from(other);
    } else {
      // Different allocators: nodes must be reallocated, move the elements.
      auto guard = detail::make_exception_guard([this] {
        clear();
      });
      for (auto& value : other) {
        insert_hint_equal(cend(), std::move(value));
      }
      guard.complete();
    }
  }

  constexpr ~binary_search_tree() { clear(); }

  // ========================================================================
  // Assignment
  // ========================================================================

  constexpr binary_search_tree& operator=(const binary_search_tree& other) {
    if (this != &other) {
      // Destroy with the old allocator before a propagating assignment
      // replaces it.
      clear();
      key_compare_ = other.key_compare_;
      policy_ = other.policy_;
      if constexpr (meta::propagate_on_container_copy_assignment_v<node_allocator_type>) {
        node_allocator_ = other.node_allocator_;
      }
      copy_from(other);
    }
    return *this;
  }

  constexpr binary_search_tree& operator=(binary_search_tree&& other) noexcept(
      (meta::propagate_on_container_move_assignment_v<node_allocator_type> ||
       meta::allocator_is_always_equal_v<node_allocator_type>) &&
      std::is_nothrow_move_assignable_v<Compare> && std::is_nothrow_move_assignable_v<Policy>) {
    if (this != &other) {
      clear();
      key_compare_ = std::move(other.key_compare_);
      policy_ = std::move(other.policy_);
      if constexpr (meta::propagate_on_container_move_assignment_v<node_allocator_type>) {
        node_allocator_ = std::move(other.node_allocator_);
      }
      if constexpr (meta::propagate_on_container_move_assignment_v<node_allocator_type> ||
                    meta::allocator_is_always_equal_v<node_allocator_type>) {
        size_ = std::exchange(other.size_, 0);
        steal_header_from(other);
      } else if (node_allocator_ == other.node_allocator_) {
        size_ = std::exchange(other.size_, 0);
        steal_header_from(other);
      } else {
        // Different allocators: nodes must be reallocated, move the elements.
        for (auto& value : other) {
          insert_hint_equal(cend(), std::move(value));
        }
      }
    }
    return *this;
  }

  constexpr binary_search_tree& operator=(std::initializer_list<value_type> init) {
    clear();
    insert_unique(init.begin(), init.end());
    return *this;
  }

  // ========================================================================
  // Allocator
  // ========================================================================

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return allocator_type(node_allocator_); }

  // ========================================================================
  // Iterators
  // ========================================================================

  [[nodiscard]] constexpr iterator begin() noexcept { return iterator(header_.left); }

  [[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(header_.left); }

  [[nodiscard]] constexpr iterator end() noexcept { return iterator(&header_); }

  [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(&header_); }

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

  // ========================================================================
  // Capacity
  // ========================================================================

  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

  [[nodiscard]] constexpr size_type size() const noexcept { return size_; }

  [[nodiscard]] constexpr size_type max_size() const noexcept {
    return node_allocator_traits::max_size(node_allocator_);
  }

  // ========================================================================
  // Modifiers: clear
  // ========================================================================

  constexpr void clear() noexcept {
    if (root() != nullptr) {
      destroy_tree(root());
      init_header();
      size_ = 0;
    }
  }

  // ========================================================================
  // Modifiers: insert (unique keys)
  // ========================================================================

  template <class Arg>
  constexpr std::pair<iterator, bool> insert_unique(Arg&& value) {
    const insert_position pos =
        get_insert_unique_pos(root(), header_, key_of_value_(value), key_of_value_, key_compare_);
    if (pos.existing != nullptr) {
      return {iterator(pos.existing), false};
    }

    tree_node_type* node = create_node(std::forward<Arg>(value));
    policy_.insert_and_rebalance(pos.as_left_child, node, pos.parent, header_);
    ++size_;
    return {iterator(node), true};
  }

  /**
   * The position search runs while @p nh still owns the node: if the
   * comparator throws, or the key turns out to be a duplicate, ownership
   * never moved. release() happens only on the no-fail insert path.
   */
  constexpr std::pair<iterator, bool> insert_node_unique(node_handle_type&& nh) {
    if (nh.empty()) {
      return {end(), false};
    }
    tree_node_type* const node = nh.ptr();
    const insert_position pos =
        get_insert_unique_pos(root(), header_, key_of_value_(node->value), key_of_value_, key_compare_);
    if (pos.existing != nullptr) {
      return {iterator(pos.existing), false};  // nh keeps the node
    }
    policy_.insert_and_rebalance(pos.as_left_child, nh.release(), pos.parent, header_);
    ++size_;
    return {iterator(node), true};
  }

  template <class Arg>
  constexpr iterator insert_hint_unique(const_iterator hint, Arg&& value) {
    const insert_position pos =
        get_insert_hint_unique_pos(root(), header_, hint.base(), key_of_value_(value), key_of_value_, key_compare_);
    if (pos.existing != nullptr) {
      return iterator(pos.existing);
    }

    tree_node_type* node = create_node(std::forward<Arg>(value));
    policy_.insert_and_rebalance(pos.as_left_child, node, pos.parent, header_);
    ++size_;
    return iterator(node);
  }

  constexpr iterator insert_hint_node_unique(const_iterator hint, node_handle_type&& nh) {
    if (nh.empty()) {
      return end();
    }
    tree_node_type* const node = nh.ptr();
    const insert_position pos = get_insert_hint_unique_pos(
        root(), header_, hint.base(), key_of_value_(node->value), key_of_value_, key_compare_);
    if (pos.existing != nullptr) {
      return iterator(pos.existing);  // nh keeps the node
    }
    policy_.insert_and_rebalance(pos.as_left_child, nh.release(), pos.parent, header_);
    ++size_;
    return iterator(node);
  }

  template <std::input_iterator InputIt>
  constexpr void insert_unique(InputIt first, InputIt last) {
    // end() as hint makes inserting an already-sorted range linear: each
    // element lands one comparison away from the current maximum.
    for (; first != last; ++first) {
      insert_hint_unique(cend(), *first);
    }
  }

  // ========================================================================
  // Modifiers: insert (equivalent keys)
  // ========================================================================

  template <class Arg>
  constexpr iterator insert_equal(Arg&& value) {
    const insert_position pos =
        get_insert_equal_pos(root(), header_, key_of_value_(value), key_of_value_, key_compare_);
    tree_node_type* node = create_node(std::forward<Arg>(value));
    policy_.insert_and_rebalance(pos.as_left_child, node, pos.parent, header_);
    ++size_;
    return iterator(node);
  }

  constexpr iterator insert_node_equal(node_handle_type&& nh) {
    if (nh.empty()) {
      return end();
    }
    // Search before release: a throwing comparator must leave nh owning.
    tree_node_type* const node = nh.ptr();
    const insert_position pos =
        get_insert_equal_pos(root(), header_, key_of_value_(node->value), key_of_value_, key_compare_);
    policy_.insert_and_rebalance(pos.as_left_child, nh.release(), pos.parent, header_);
    ++size_;
    return iterator(node);
  }

  template <class Arg>
  constexpr iterator insert_hint_equal(const_iterator hint, Arg&& value) {
    const insert_position pos =
        get_insert_hint_equal_pos(root(), header_, hint.base(), key_of_value_(value), key_of_value_, key_compare_);
    tree_node_type* node = create_node(std::forward<Arg>(value));
    policy_.insert_and_rebalance(pos.as_left_child, node, pos.parent, header_);
    ++size_;
    return iterator(node);
  }

  constexpr iterator insert_hint_node_equal(const_iterator hint, node_handle_type&& nh) {
    if (nh.empty()) {
      return end();
    }
    tree_node_type* const node = nh.ptr();
    const insert_position pos = get_insert_hint_equal_pos(
        root(), header_, hint.base(), key_of_value_(node->value), key_of_value_, key_compare_);
    policy_.insert_and_rebalance(pos.as_left_child, nh.release(), pos.parent, header_);
    ++size_;
    return iterator(node);
  }

  template <std::input_iterator InputIt>
  constexpr void insert_equal(InputIt first, InputIt last) {
    // Same linear-for-sorted-input property as the unique overload.
    for (; first != last; ++first) {
      insert_hint_equal(cend(), *first);
    }
  }

  // ========================================================================
  // Modifiers: emplace (unique keys)
  // ========================================================================

  /**
   * The key is only known once the value exists, so the node is built
   * first and dropped again on a duplicate (libstdc++'s _Auto_node
   * pattern). The guard also covers a comparator that throws mid-search.
   */
  template <class... Args>
  constexpr std::pair<iterator, bool> emplace_unique(Args&&... args) {
    tree_node_type* node = create_node(std::forward<Args>(args)...);
    auto guard = detail::make_exception_guard([&] {
      destroy_node(node);
    });

    const insert_position pos =
        get_insert_unique_pos(root(), header_, key_of_value_(node->value), key_of_value_, key_compare_);
    if (pos.existing != nullptr) {
      return {iterator(pos.existing), false};  // guard drops the new node
    }

    policy_.insert_and_rebalance(pos.as_left_child, node, pos.parent, header_);
    ++size_;
    guard.complete();
    return {iterator(node), true};
  }

  template <class... Args>
  constexpr iterator emplace_hint_unique(const_iterator hint, Args&&... args) {
    tree_node_type* node = create_node(std::forward<Args>(args)...);
    auto guard = detail::make_exception_guard([&] {
      destroy_node(node);
    });

    const insert_position pos = get_insert_hint_unique_pos(
        root(), header_, hint.base(), key_of_value_(node->value), key_of_value_, key_compare_);
    if (pos.existing != nullptr) {
      return iterator(pos.existing);  // guard drops the new node
    }

    policy_.insert_and_rebalance(pos.as_left_child, node, pos.parent, header_);
    ++size_;
    guard.complete();
    return iterator(node);
  }

  // ========================================================================
  // Modifiers: emplace (equivalent keys)
  // ========================================================================

  /**
   * Same _Auto_node pattern as the unique family; with no duplicate exit
   * the guard exists purely for a comparator that throws mid-search.
   */
  template <class... Args>
  constexpr iterator emplace_equal(Args&&... args) {
    tree_node_type* node = create_node(std::forward<Args>(args)...);
    auto guard = detail::make_exception_guard([&] {
      destroy_node(node);
    });

    const insert_position pos =
        get_insert_equal_pos(root(), header_, key_of_value_(node->value), key_of_value_, key_compare_);
    policy_.insert_and_rebalance(pos.as_left_child, node, pos.parent, header_);
    ++size_;
    guard.complete();
    return iterator(node);
  }

  template <class... Args>
  constexpr iterator emplace_hint_equal(const_iterator hint, Args&&... args) {
    tree_node_type* node = create_node(std::forward<Args>(args)...);
    auto guard = detail::make_exception_guard([&] {
      destroy_node(node);
    });

    const insert_position pos = get_insert_hint_equal_pos(
        root(), header_, hint.base(), key_of_value_(node->value), key_of_value_, key_compare_);
    policy_.insert_and_rebalance(pos.as_left_child, node, pos.parent, header_);
    ++size_;
    guard.complete();
    return iterator(node);
  }

  // ========================================================================
  // Modifiers: erase
  // ========================================================================

  constexpr iterator erase(const_iterator pos) noexcept {
    const_iterator next = pos;
    ++next;
    erase_node(pos.base());
    return iterator(next.base());
  }

  constexpr iterator erase(const_iterator first, const_iterator last) noexcept {
    if (first == cbegin() && last == cend()) {
      clear();
    } else {
      while (first != last) {
        first = erase(first);
      }
    }
    return iterator(last.base());
  }

  constexpr size_type erase(const key_type& key) {
    iterator it = find(key);
    if (it == end()) {
      return 0;
    }
    erase(const_iterator(it));
    return 1;
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  constexpr size_type erase(const K& key) {
    iterator it = find(key);
    if (it == end()) {
      return 0;
    }
    erase(const_iterator(it));
    return 1;
  }

  /// Erase every element equivalent to @p key; returns how many.
  constexpr size_type erase_equal(const key_type& key) {
    auto [first, last] = equal_range_multi(key);
    size_type n = 0;
    for (const_iterator it = first; it != last; ++n) {
      it = erase(it);
    }
    return n;
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  constexpr size_type erase_equal(const K& key) {
    auto [first, last] = equal_range_multi(key);
    size_type n = 0;
    for (const_iterator it = first; it != last; ++n) {
      it = erase(it);
    }
    return n;
  }

  // ========================================================================
  // Modifiers: extract / merge
  // ========================================================================

  /**
   * @brief Extract the node at @p pos into a node handle.
   */
  constexpr node_handle_type extract(const_iterator pos) noexcept {
    bst_node_base* base = policy_.unlink_and_rebalance(static_cast<tree_node_type*>(pos.base()), header_);
    --size_;
    return node_handle_type(static_cast<tree_node_type*>(base), node_allocator_);
  }

  /**
   * @brief Extract the node matching @p key into a node handle (or empty).
   */
  constexpr node_handle_type extract(const key_type& key) {
    iterator it = find(key);
    if (it == end()) {
      return node_handle_type();
    }
    return extract(const_iterator(it));
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  constexpr node_handle_type extract(const K& key) {
    iterator it = find(key);
    if (it == end()) {
      return node_handle_type();
    }
    return extract(const_iterator(it));
  }

  /**
   * @brief Transfer every element from @p source whose key is not already
   *        present in this tree.
   *
   * Elements whose key already exists remain in @p source. The source tree
   * must use the same balance policy and a compatible node layout; the
   * standard only requires merge between containers of the same kind.
   */
  template <class Compare2>
  constexpr void merge(binary_search_tree<Key, Value, KeyOfValue, Compare2, Allocator, Policy>& source) {
    // Iterate and try to splice each node. Because unlink invalidates only
    // the unlinked node, we advance before extracting.
    auto it = source.begin();
    while (it != source.end()) {
      auto current = it++;
      const insert_position pos =
          get_insert_unique_pos(root(), header_, key_of_value_(*current), key_of_value_, key_compare_);
      if (pos.existing == nullptr) {
        auto nh = source.extract(current);
        tree_node_type* node = static_cast<tree_node_type*>(nh.release());
        policy_.insert_and_rebalance(pos.as_left_child, node, pos.parent, header_);
        ++size_;
      }
    }
  }

  template <class Compare2>
  constexpr void merge(binary_search_tree<Key, Value, KeyOfValue, Compare2, Allocator, Policy>&& source) {
    merge(source);
  }

  /**
   * @brief Transfer every element from @p source (equivalent keys allowed,
   *        so nothing is ever refused).
   *
   * The position search runs before the node leaves @p source: if the
   * comparator throws, the current element and everything after it remain
   * in @p source, elements already transferred stay transferred — the
   * standard's merge guarantee.
   */
  template <class Compare2>
  constexpr void merge_equal(binary_search_tree<Key, Value, KeyOfValue, Compare2, Allocator, Policy>& source) {
    auto it = source.begin();
    while (it != source.end()) {
      auto current = it++;
      const insert_position pos =
          get_insert_equal_pos(root(), header_, key_of_value_(*current), key_of_value_, key_compare_);
      auto nh = source.extract(current);
      policy_.insert_and_rebalance(pos.as_left_child, static_cast<tree_node_type*>(nh.release()), pos.parent, header_);
      ++size_;
    }
  }

  template <class Compare2>
  constexpr void merge_equal(binary_search_tree<Key, Value, KeyOfValue, Compare2, Allocator, Policy>&& source) {
    merge_equal(source);
  }

  // ========================================================================
  // Modifiers: swap
  // ========================================================================

  constexpr void swap(binary_search_tree& other) noexcept(meta::is_nothrow_container_swappable_v<node_allocator_type> &&
                                                          std::is_nothrow_swappable_v<Compare> &&
                                                          std::is_nothrow_swappable_v<Policy>) {
    using std::swap;

    if constexpr (meta::propagate_on_container_swap_v<node_allocator_type>) {
      swap(node_allocator_, other.node_allocator_);
    }
    swap(key_compare_, other.key_compare_);
    swap(policy_, other.policy_);

    swap(header_.parent, other.header_.parent);
    swap(header_.left, other.header_.left);
    swap(header_.right, other.header_.right);
    swap(size_, other.size_);

    reanchor_header();
    other.reanchor_header();
  }

  // ========================================================================
  // Lookup
  // ========================================================================

  [[nodiscard]] constexpr iterator find(const key_type& key) { return find_mutating(key); }

  [[nodiscard]] constexpr const_iterator find(const key_type& key) const { return find_const(key); }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator find(const K& key) {
    return find_mutating(key);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator find(const K& key) const {
    return find_const(key);
  }

  [[nodiscard]] constexpr bool contains(const key_type& key) const { return find(key) != end(); }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr bool contains(const K& key) const {
    return find(key) != end();
  }

  [[nodiscard]] constexpr size_type count(const key_type& key) const { return contains(key) ? 1 : 0; }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr size_type count(const K& key) const {
    return contains(key) ? 1 : 0;
  }

  /// Count under the equivalent-keys regime: O(log n + matches).
  [[nodiscard]] constexpr size_type count_multi(const key_type& key) const {
    auto [first, last] = equal_range_multi(key);
    return static_cast<size_type>(std::distance(first, last));
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr size_type count_multi(const K& key) const {
    auto [first, last] = equal_range_multi(key);
    return static_cast<size_type>(std::distance(first, last));
  }

  [[nodiscard]] constexpr iterator lower_bound(const key_type& key) {
    const lookup_result search = lower_bound_search(root(), &header_, key, key_of_value_, key_compare_);
    notify_access(bound_access_node(search));
    return iterator(search.result);
  }

  [[nodiscard]] constexpr const_iterator lower_bound(const key_type& key) const {
    return const_iterator(
        lower_bound_node(root(), const_cast<bst_node_base*>(&header_), key, key_of_value_, key_compare_));
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator lower_bound(const K& key) {
    const lookup_result search = lower_bound_search(root(), &header_, key, key_of_value_, key_compare_);
    notify_access(bound_access_node(search));
    return iterator(search.result);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator lower_bound(const K& key) const {
    return const_iterator(
        lower_bound_node(root(), const_cast<bst_node_base*>(&header_), key, key_of_value_, key_compare_));
  }

  [[nodiscard]] constexpr iterator upper_bound(const key_type& key) {
    const lookup_result search = upper_bound_search(root(), &header_, key, key_of_value_, key_compare_);
    notify_access(bound_access_node(search));
    return iterator(search.result);
  }

  [[nodiscard]] constexpr const_iterator upper_bound(const key_type& key) const {
    return const_iterator(
        upper_bound_node(root(), const_cast<bst_node_base*>(&header_), key, key_of_value_, key_compare_));
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator upper_bound(const K& key) {
    const lookup_result search = upper_bound_search(root(), &header_, key, key_of_value_, key_compare_);
    notify_access(bound_access_node(search));
    return iterator(search.result);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator upper_bound(const K& key) const {
    return const_iterator(
        upper_bound_node(root(), const_cast<bst_node_base*>(&header_), key, key_of_value_, key_compare_));
  }

  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const key_type& key) {
    auto [lower, upper] = equal_range_unique_node(root(), &header_, key, key_of_value_, key_compare_);
    return {iterator(lower), iterator(upper)};
  }

  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
    auto [lower, upper] = const_cast<binary_search_tree*>(this)->equal_range(key);
    return {const_iterator(lower), const_iterator(upper)};
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const K& key) {
    auto [lower, upper] = equal_range_unique_node(root(), &header_, key, key_of_value_, key_compare_);
    return {iterator(lower), iterator(upper)};
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
    auto [lower, upper] = const_cast<binary_search_tree*>(this)->equal_range(key);
    return {const_iterator(lower), const_iterator(upper)};
  }

  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range_multi(const key_type& key) {
    auto [lower, upper] = equal_range_multi_node(root(), &header_, key, key_of_value_, key_compare_);
    return {iterator(lower), iterator(upper)};
  }

  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range_multi(const key_type& key) const {
    auto [lower, upper] = const_cast<binary_search_tree*>(this)->equal_range_multi(key);
    return {const_iterator(lower), const_iterator(upper)};
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range_multi(const K& key) {
    auto [lower, upper] = equal_range_multi_node(root(), &header_, key, key_of_value_, key_compare_);
    return {iterator(lower), iterator(upper)};
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range_multi(const K& key) const {
    auto [lower, upper] = const_cast<binary_search_tree*>(this)->equal_range_multi(key);
    return {const_iterator(lower), const_iterator(upper)};
  }

  // ========================================================================
  // Observers
  // ========================================================================

  [[nodiscard]] constexpr key_compare key_comp() const { return key_compare_; }

  [[nodiscard]] constexpr value_compare value_comp() const { return key_compare_; }

  // ========================================================================
  // Order statistics (policy extension)
  // ========================================================================

  [[nodiscard]] constexpr iterator find_by_order(size_type order)
    requires order_statistic_policy<Policy, tree_node_type>
  {
    return iterator(select_by_order(order));
  }

  [[nodiscard]] constexpr const_iterator find_by_order(size_type order) const
    requires order_statistic_policy<Policy, tree_node_type>
  {
    return const_iterator(select_by_order(order));
  }

  template <class K>
  [[nodiscard]] constexpr size_type order_of_key(const K& key) const
    requires order_statistic_policy<Policy, tree_node_type>
  {
    size_type order = 0;
    const tree_node_type* node = root();
    while (node != nullptr) {
      if (key_compare_(key_of_value_(node->value), key)) {
        order += node_size(static_cast<const tree_node_type*>(node->left)) + 1;
        node = static_cast<const tree_node_type*>(node->right);
      } else {
        node = static_cast<const tree_node_type*>(node->left);
      }
    }
    return order;
  }

  // ========================================================================
  // Validation (for testing)
  // ========================================================================

  /**
   * @brief Full structural self-check, for tests and debugging.
   *
   * Verifies, in order: empty-state consistency, the header/root parent
   * cycle, the policy's balance invariants, child->parent back-links,
   * in-order key ordering, that exactly size() nodes are reachable,
   * and the leftmost/rightmost caches. The reachable-node count doubles as
   * a bail-out so a corrupted (cyclic) structure fails instead of hanging.
   *
   * Ordering is checked strictly (each key less than its successor) unless
   * @p allow_equivalent — the multi containers' regime, where in-order
   * traversal must merely never descend.
   */
  [[nodiscard]] constexpr bool validate(const bool allow_equivalent = false) const {
    if (header_.parent == nullptr) {
      return size_ == 0 && header_.left == &header_ && header_.right == &header_;
    }
    if (size_ == 0 || header_.parent->parent != &header_) {
      return false;
    }
    if (!policy_.verify(root())) {
      return false;
    }

    size_type count = 0;
    const tree_node_type* prev = nullptr;
    for (const_iterator it = cbegin(); it != cend(); ++it) {
      if (++count > size_) {
        return false;
      }
      const auto* node = static_cast<const tree_node_type*>(it.base());
      if (node->left != nullptr && node->left->parent != node) {
        return false;
      }
      if (node->right != nullptr && node->right->parent != node) {
        return false;
      }
      if (prev != nullptr) {
        const auto& prev_key = key_of_value_(prev->value);
        const auto& node_key = key_of_value_(node->value);
        if (allow_equivalent ? key_compare_(node_key, prev_key) : !key_compare_(prev_key, node_key)) {
          return false;
        }
      }
      prev = node;
    }
    if (count != size_) {
      return false;
    }
    return header_.left == node_minimum(header_.parent) && header_.right == node_maximum(header_.parent);
  }

 private:
  // ========================================================================
  // Header node management
  // ========================================================================

  /**
   * @brief Initialize the header for an empty tree.
   *
   * header_.parent = nullptr (no root)
   * header_.left = &header_ (begin() == end())
   * header_.right = &header_ (rbegin() == rend())
   */
  constexpr void init_header() noexcept {
    header_.parent = nullptr;
    header_.left = &header_;
    header_.right = &header_;
  }

  /**
   * @brief Adopt another tree's nodes by relinking its header into ours.
   *
   * Precondition: this tree is empty with an initialized header; sizes
   * are handled by the caller. @p other is left empty.
   */
  constexpr void steal_header_from(binary_search_tree& other) noexcept {
    if (other.header_.parent != nullptr) {
      header_.parent = other.header_.parent;
      header_.left = other.header_.left;
      header_.right = other.header_.right;
      header_.parent->parent = &header_;
      other.init_header();
    }
  }

  /**
   * @brief Re-establish the header invariants after swapping raw pointers.
   *
   * An adopted root must point back at this header; an adopted empty
   * state must collapse to the self-referencing header.
   */
  constexpr void reanchor_header() noexcept {
    if (header_.parent == nullptr) {
      init_header();
    } else {
      header_.parent->parent = &header_;
    }
  }

  // ========================================================================
  // Root access
  // ========================================================================

  [[nodiscard]] constexpr tree_node_type* root() const noexcept { return static_cast<tree_node_type*>(header_.parent); }

  [[nodiscard]] static constexpr size_type node_size(const tree_node_type* node) noexcept
    requires order_statistic_policy<Policy, tree_node_type>
  {
    return node == nullptr ? 0 : node->extension.subtree_size;
  }

  [[nodiscard]] constexpr bst_node_base* select_by_order(size_type order) const noexcept
    requires order_statistic_policy<Policy, tree_node_type>
  {
    tree_node_type* node = root();
    while (node != nullptr) {
      const size_type left_size = node_size(static_cast<const tree_node_type*>(node->left));
      if (order < left_size) {
        node = static_cast<tree_node_type*>(node->left);
      } else if (order == left_size) {
        return node;
      } else {
        order -= left_size + 1;
        node = static_cast<tree_node_type*>(node->right);
      }
    }
    return const_cast<bst_node_base*>(&header_);
  }

  template <class K>
  [[nodiscard]] constexpr iterator find_mutating(const K& key) {
    const lookup_result search = find_search(root(), &header_, key, key_of_value_, key_compare_);
    // On a hit, splay the found node; on a miss, the descent frontier.
    // (find_search descends past the match, so last_visited is the leaf
    // where the descent ended, not the matching node.)
    notify_access(bound_access_node(search));
    return iterator(search.result);
  }

  template <class K>
  [[nodiscard]] constexpr const_iterator find_const(const K& key) const {
    return const_iterator(
        find_search(root(), const_cast<bst_node_base*>(&header_), key, key_of_value_, key_compare_).result);
  }

  // Bounds follow the STL convention and return end() when no bound exists.
  // A self-adjusting tree still needs a real node to splay on that miss, so
  // access notification falls back to the final node visited by the descent.
  [[nodiscard]] constexpr bst_node_base* bound_access_node(const lookup_result& search) noexcept {
    return search.result != &header_ ? search.result : search.last_visited;
  }

  constexpr void notify_access(bst_node_base* node) noexcept {
    if constexpr (access_rebalance_policy<Policy, tree_node_type>) {
      if (node != nullptr && node != &header_) {
        policy_.after_access(static_cast<tree_node_type*>(node), header_);
      }
    }
  }

  // ========================================================================
  // Node allocation and construction
  // ========================================================================

  template <class... Args>
  constexpr tree_node_type* create_node(Args&&... args) {
    detail::node_owner<node_allocator_type> storage(node_allocator_);
    tree_node_type* node = storage.get();
    std::construct_at(node);
    auto node_lifetime = detail::make_exception_guard([&] {
      std::destroy_at(node);
    });
    node->left = nullptr;
    node->right = nullptr;
    node->parent = nullptr;
    node_allocator_traits::construct(node_allocator_, std::addressof(node->value), std::forward<Args>(args)...);
    node_lifetime.complete();
    return storage.release();
  }

  // ========================================================================
  // Node destruction and deallocation
  // ========================================================================

  constexpr void destroy_node(tree_node_type* node) noexcept {
    node_allocator_traits::destroy(node_allocator_, std::addressof(node->value));
    std::destroy_at(node);
    using node_pointer = typename node_allocator_traits::pointer;
    node_allocator_traits::deallocate(node_allocator_, std::pointer_traits<node_pointer>::pointer_to(*node), 1);
  }

  /**
   * @brief Destroy the subtree rooted at @p root.
   *
   * Iterative post-order walk over the parent pointers — no recursion and
   * no auxiliary stack, so even a fully degenerate tree (a policy is free
   * to balance poorly) cannot overflow the call stack. Child pointers are
   * nulled while ascending; the walk never touches @p root's parent, so
   * partially built subtrees (copy_tree rollback) destroy safely.
   */
  constexpr void destroy_tree(tree_node_type* root) noexcept {
    tree_node_type* node = root;
    while (node != nullptr) {
      if (node->left != nullptr) {
        node = static_cast<tree_node_type*>(node->left);
      } else if (node->right != nullptr) {
        node = static_cast<tree_node_type*>(node->right);
      } else {
        tree_node_type* parent = node == root ? nullptr : static_cast<tree_node_type*>(node->parent);
        if (parent != nullptr) {
          if (parent->left == node) {
            parent->left = nullptr;
          } else {
            parent->right = nullptr;
          }
        }
        destroy_node(node);
        node = parent;
      }
    }
  }

  // ========================================================================
  // Tree copy
  // ========================================================================

  /**
   * @brief Recursively copy the subtree at @p src, including policy state.
   *
   * Exception-safe: if copying a child subtree throws, the partially
   * built subtree is destroyed before the exception escapes.
   *
   * Recursion depth equals the height of @p src, which a balance-
   * maintaining policy bounds at O(log n); the source always satisfies
   * its policy's invariants, so no iterative rewrite is needed here
   * (destroy_tree, which must also handle partially built shapes, is
   * iterative).
   */
  constexpr tree_node_type* copy_tree(const tree_node_type* src, bst_node_base* parent) {
    if (src == nullptr) {
      return nullptr;
    }

    tree_node_type* dst = create_node(src->value);
    dst->parent = parent;
    dst->extension = src->extension;
    auto guard = detail::make_exception_guard([&] {
      destroy_tree(dst);
    });

    dst->left = copy_tree(static_cast<const tree_node_type*>(src->left), dst);
    dst->right = copy_tree(static_cast<const tree_node_type*>(src->right), dst);

    guard.complete();
    return dst;
  }

  /**
   * @brief Copy another tree's contents. Precondition: this tree is empty.
   */
  constexpr void copy_from(const binary_search_tree& other) {
    if (other.root() != nullptr) {
      header_.parent = copy_tree(other.root(), &header_);
      header_.left = node_minimum(header_.parent);
      header_.right = node_maximum(header_.parent);
      size_ = other.size_;
    }
  }

  // ========================================================================
  // Erase a single node
  // ========================================================================

  constexpr void erase_node(bst_node_base* node) noexcept {
    bst_node_base* removed = policy_.unlink_and_rebalance(static_cast<tree_node_type*>(node), header_);
    destroy_node(static_cast<tree_node_type*>(removed));
    --size_;
  }

  // ========================================================================
  // Data members
  // ========================================================================

  bst_node_base header_;  // Sentinel: parent=root, left=leftmost, right=rightmost
  [[no_unique_address]] KeyOfValue key_of_value_;
  [[no_unique_address]] Compare key_compare_;
  [[no_unique_address]] Policy policy_;
  [[no_unique_address]] node_allocator_type node_allocator_;
  size_type size_ = 0;
};

// ============================================================================
// Non-member comparison operators
// ============================================================================

template <class K, class V, class KOV, class C, class A, class P>
[[nodiscard]] constexpr bool operator==(const binary_search_tree<K, V, KOV, C, A, P>& lhs,
                                        const binary_search_tree<K, V, KOV, C, A, P>& rhs) {
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

// [container.opt.reqmts]: ordered containers compare via synth-three-way,
// so value types that only provide operator< still get an ordering.
template <class K, class V, class KOV, class C, class A, class P>
[[nodiscard]] constexpr auto operator<=>(const binary_search_tree<K, V, KOV, C, A, P>& lhs,
                                         const binary_search_tree<K, V, KOV, C, A, P>& rhs) {
  return std::lexicographical_compare_three_way(
      lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), detail::synth_three_way{});
}

// ============================================================================
// Non-member swap
// ============================================================================

template <class K, class V, class KOV, class C, class A, class P>
  requires std::swappable<binary_search_tree<K, V, KOV, C, A, P>>
constexpr void swap(binary_search_tree<K, V, KOV, C, A, P>& lhs,
                    binary_search_tree<K, V, KOV, C, A, P>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

}  // namespace chaistl::detail::tree
