// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// multiset - Ordered associative container with non-unique keys
// ============================================================================
//
// Architecture:
//   - Thin public wrapper over detail::tree::binary_search_tree storing keys as
//     values.
//   - Uses the tree's equivalent-key insertion family, so multiple equivalent
//     keys may coexist.
//   - Iterators are constant iterators for the same invariant reason as set:
//     in-place key mutation would corrupt ordering.
//
// Standardization archaeology:
//   - multiset was standardized in C++98 as the ordered bag counterpart to set.
//   - Like all ordered associative containers, equivalence is defined by the
//     comparator rather than equality.
//   - LWG 233 clarified that insertion into equivalent-key ranges preserves
//     insertion order by using the upper bound position.
//   - C++17 node handles and merge/extract let individual nodes move between
//     containers without value relocation.
//
// Non-standard extensions:
//   - The Policy template parameter exposes rb/avl/treap experiments while
//     leaving multiset<Key> source-compatible with the standard shape.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/multiset
//   - cppreference: https://en.cppreference.com/w/cpp/container/multiset
//   - LWG defects: https://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html
//   - ADR-004: Tree Policy-Based Design

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/basic.hpp>
#include <chaistl/concepts/container_compatible_range.hpp>
#include <chaistl/containers/tree/binary_search_tree.hpp>
#include <chaistl/containers/tree/key_extract.hpp>
#include <chaistl/containers/tree/policy/rb_tree.hpp>
#include <chaistl/memory/allocator.hpp>
#include <chaistl/utility/hardening.hpp>

#include <compare>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

namespace chaistl {

/**
 * @brief Ordered associative container allowing equivalent keys.
 *
 * @ingroup containers_associative
 *
 * Stores elements sorted by key; multiple equivalent keys may coexist, and
 * within a run of equivalent elements insertion order is preserved
 * (LWG 233: plain insert places the element at the upper bound). Implemented
 * over the same detail::tree::binary_search_tree as set, using its
 * equivalent-keys operation family; the balancing scheme is the same policy
 * template parameter defaulting to the red-black tree (ADR 004).
 *
 * References:
 * - C++ Draft Standard: https://eel.is/c++draft/multiset
 * - cppreference: https://en.cppreference.com/w/cpp/container/multiset
 */
template <class Key,
          class Compare = std::less<Key>,
          class Allocator = allocator<Key>,
          class Policy = detail::tree::rb_tree_policy>
  requires concepts::container_element<Key> && concepts::allocator_for<Allocator, Key> &&
           std::strict_weak_order<Compare, const Key&, const Key&>
class multiset {
 public:
  // ========================================================================
  // Member types
  // ========================================================================

  using key_type = Key;
  using value_type = Key;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using key_compare = Compare;
  using allocator_type = Allocator;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

  // =======================================================================
  // Nested value_compare class
  // =======================================================================

  class value_compare {
   public:
    [[nodiscard]] constexpr bool operator()(const value_type& lhs, const value_type& rhs) const {
      return comp_(lhs, rhs);
    }

   protected:
    friend class multiset;

    explicit constexpr value_compare(Compare comp) : comp_(std::move(comp)) {}

    Compare comp_;
  };

  // [multiset.overview]: like set, both iterator types are constant
  // iterators — keys are immutable once inserted.
  using iterator = detail::tree::bst_iterator<value_type, true>;
  using const_iterator = iterator;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;

  // ========================================================================
  // Node handle (C++17)
  // ========================================================================

  // No insert_return_type: that member belongs to unique-key containers
  // only; multiset::insert(node_type&&) returns a plain iterator.
  using node_type = detail::tree::bst_node_handle<value_type, Policy, Allocator>;

  // ========================================================================
  // Constructors and destructor
  // ========================================================================

  constexpr multiset() = default;

  explicit constexpr multiset(const Compare& comp, const Allocator& alloc = Allocator{}) : tree_(comp, alloc) {}

  explicit constexpr multiset(const Allocator& alloc) : tree_(alloc) {}

  // The tree's iterator-range constructors insert with the unique family,
  // so the multi containers construct empty and bulk-insert instead; if an
  // insertion throws, the fully constructed tree_ member cleans up.
  template <std::input_iterator InputIt>
  constexpr multiset(InputIt first, InputIt last, const Compare& comp = Compare{}, const Allocator& alloc = Allocator{})
      : tree_(comp, alloc) {
    tree_.insert_equal(first, last);
  }

  template <std::input_iterator InputIt>
  constexpr multiset(InputIt first, InputIt last, const Allocator& alloc) : multiset(first, last, Compare{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R>
  constexpr multiset(std::from_range_t,
                     R&& range,
                     const Compare& comp = Compare{},
                     const Allocator& alloc = Allocator{})
      : tree_(comp, alloc) {
    insert_range(std::forward<R>(range));
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr multiset(std::from_range_t, R&& range, const Allocator& alloc)
      : multiset(std::from_range, std::forward<R>(range), Compare{}, alloc) {}

  constexpr multiset(std::initializer_list<value_type> init,
                     const Compare& comp = Compare{},
                     const Allocator& alloc = Allocator{})
      : multiset(init.begin(), init.end(), comp, alloc) {}

  constexpr multiset(std::initializer_list<value_type> init, const Allocator& alloc)
      : multiset(init.begin(), init.end(), Compare{}, alloc) {}

  constexpr multiset(const multiset& other) = default;
  constexpr multiset(const multiset& other, const std::type_identity_t<Allocator>& alloc) : tree_(other.tree_, alloc) {}

  constexpr multiset(multiset&& other) = default;
  constexpr multiset(multiset&& other, const std::type_identity_t<Allocator>& alloc)
      : tree_(std::move(other.tree_), alloc) {}

  constexpr ~multiset() = default;

  // ========================================================================
  // Assignment
  // ========================================================================

  constexpr multiset& operator=(const multiset& other) = default;
  constexpr multiset& operator=(multiset&& other) = default;
  constexpr multiset& operator=(std::initializer_list<value_type> init) {
    // The tree's initializer_list assignment inserts with the unique family.
    clear();
    tree_.insert_equal(init.begin(), init.end());
    return *this;
  }

  // ========================================================================
  // Allocator
  // ========================================================================

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return tree_.get_allocator(); }

  // ========================================================================
  // Iterators
  // ========================================================================

  [[nodiscard]] constexpr iterator begin() noexcept { return tree_.begin(); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return tree_.begin(); }
  [[nodiscard]] constexpr iterator end() noexcept { return tree_.end(); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return tree_.end(); }

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return tree_.rbegin(); }
  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return tree_.rbegin(); }
  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return tree_.rend(); }
  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return tree_.rend(); }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return tree_.cbegin(); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return tree_.cend(); }
  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return tree_.crbegin(); }
  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return tree_.crend(); }

  // ========================================================================
  // Capacity
  // ========================================================================

  [[nodiscard]] constexpr bool empty() const noexcept { return tree_.empty(); }
  [[nodiscard]] constexpr size_type size() const noexcept { return tree_.size(); }
  [[nodiscard]] constexpr size_type max_size() const noexcept { return tree_.max_size(); }

  // ========================================================================
  // Modifiers
  // ========================================================================

  constexpr void clear() noexcept { tree_.clear(); }

  // --- insert ---

  constexpr iterator insert(const value_type& value) { return tree_.insert_equal(value); }

  constexpr iterator insert(value_type&& value) { return tree_.insert_equal(std::move(value)); }

  constexpr iterator insert(const_iterator hint, const value_type& value) {
    return tree_.insert_hint_equal(hint, value);
  }

  constexpr iterator insert(const_iterator hint, value_type&& value) {
    return tree_.insert_hint_equal(hint, std::move(value));
  }

  template <std::input_iterator InputIt>
  constexpr void insert(InputIt first, InputIt last) {
    tree_.insert_equal(first, last);
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr void insert_range(R&& range) {
    tree_.insert_equal(std::ranges::begin(range), std::ranges::end(range));
  }

  constexpr void insert(std::initializer_list<value_type> init) { tree_.insert_equal(init.begin(), init.end()); }

  // --- emplace ---

  template <class... Args>
  constexpr iterator emplace(Args&&... args) {
    return tree_.emplace_equal(std::forward<Args>(args)...);
  }

  template <class... Args>
  constexpr iterator emplace_hint(const_iterator hint, Args&&... args) {
    return tree_.emplace_hint_equal(hint, std::forward<Args>(args)...);
  }

  // --- erase ---

  constexpr iterator erase(iterator pos) noexcept
    requires(!std::same_as<iterator, const_iterator>)
  {
    CHAI_HARDENED(pos != end(), "chaistl::multiset::erase: invalid iterator");
    return tree_.erase(pos);
  }

  constexpr iterator erase(const_iterator pos) noexcept {
    CHAI_HARDENED(pos != end(), "chaistl::multiset::erase: invalid iterator");
    return tree_.erase(pos);
  }
  constexpr iterator erase(const_iterator first, const_iterator last) noexcept { return tree_.erase(first, last); }

  /// Erases every element equivalent to @p key; returns how many.
  constexpr size_type erase(const key_type& key) { return tree_.erase_equal(key); }

  template <class K>
    requires concepts::transparent_compare<Compare>
  constexpr size_type erase(const K& key) {
    return tree_.erase_equal(key);
  }

  // --- extract ---

  constexpr node_type extract(const_iterator pos) { return tree_.extract(pos); }

  /// Extracts the first element equivalent to @p key (or returns empty).
  constexpr node_type extract(const key_type& key) { return tree_.extract(key); }

  // Heterogeneous extract: same forward-looking P2077 companion as set.
  template <class K>
    requires concepts::transparent_compare<Compare>
  constexpr node_type extract(const K& key) {
    return tree_.extract(key);
  }

  // --- insert (node handle) ---

  constexpr iterator insert(node_type&& nh) { return tree_.insert_node_equal(std::move(nh)); }

  constexpr iterator insert(const_iterator hint, node_type&& nh) {
    return tree_.insert_hint_node_equal(hint, std::move(nh));
  }

  // --- merge ---

  // [multiset.modifiers]: only the comparator may differ — the allocator
  // type is fixed, because spliced nodes keep their original allocation.
  // The set<->multiset cross-family overloads the standard also specifies
  // are not provided yet (they need mutual friendship across headers).
  template <class Compare2>
  constexpr void merge(multiset<Key, Compare2, Allocator, Policy>& source) {
    tree_.merge_equal(source.tree_);
  }

  template <class Compare2>
  constexpr void merge(multiset<Key, Compare2, Allocator, Policy>&& source) {
    tree_.merge_equal(std::move(source.tree_));
  }

  // --- swap ---

  constexpr void swap(multiset& other) noexcept(noexcept(tree_.swap(other.tree_))) { tree_.swap(other.tree_); }

  // ========================================================================
  // Lookup
  // ========================================================================

  [[nodiscard]] constexpr size_type count(const key_type& key) const { return tree_.count_multi(key); }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr size_type count(const K& key) const {
    return tree_.count_multi(key);
  }

  /// Returns an iterator to one element of the equivalent run (the first).
  [[nodiscard]] constexpr iterator find(const key_type& key) { return tree_.find(key); }
  [[nodiscard]] constexpr const_iterator find(const key_type& key) const { return tree_.find(key); }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator find(const K& key) {
    return tree_.find(key);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator find(const K& key) const {
    return tree_.find(key);
  }

  [[nodiscard]] constexpr bool contains(const key_type& key) const { return tree_.contains(key); }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr bool contains(const K& key) const {
    return tree_.contains(key);
  }

  [[nodiscard]] constexpr iterator lower_bound(const key_type& key) { return tree_.lower_bound(key); }
  [[nodiscard]] constexpr const_iterator lower_bound(const key_type& key) const { return tree_.lower_bound(key); }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator lower_bound(const K& key) {
    return tree_.lower_bound(key);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator lower_bound(const K& key) const {
    return tree_.lower_bound(key);
  }

  [[nodiscard]] constexpr iterator upper_bound(const key_type& key) { return tree_.upper_bound(key); }
  [[nodiscard]] constexpr const_iterator upper_bound(const key_type& key) const { return tree_.upper_bound(key); }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator upper_bound(const K& key) {
    return tree_.upper_bound(key);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator upper_bound(const K& key) const {
    return tree_.upper_bound(key);
  }

  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const key_type& key) {
    return tree_.equal_range_multi(key);
  }
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
    return tree_.equal_range_multi(key);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const K& key) {
    return tree_.equal_range_multi(key);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
    return tree_.equal_range_multi(key);
  }

  // ========================================================================
  // Observers
  // ========================================================================

  [[nodiscard]] constexpr key_compare key_comp() const { return tree_.key_comp(); }
  [[nodiscard]] constexpr value_compare value_comp() const { return value_compare(tree_.key_comp()); }

  // ========================================================================
  // Order statistics (policy extension)
  // ========================================================================

  [[nodiscard]] constexpr iterator find_by_order(size_type order)
    requires detail::tree::order_statistic_policy<Policy, detail::tree::bst_node<value_type, Policy>>
  {
    return tree_.find_by_order(order);
  }

  [[nodiscard]] constexpr const_iterator find_by_order(size_type order) const
    requires detail::tree::order_statistic_policy<Policy, detail::tree::bst_node<value_type, Policy>>
  {
    return tree_.find_by_order(order);
  }

  template <class K>
  [[nodiscard]] constexpr size_type order_of_key(const K& key) const
    requires detail::tree::order_statistic_policy<Policy, detail::tree::bst_node<value_type, Policy>>
  {
    return tree_.order_of_key(key);
  }

  // ========================================================================
  // Validation (for testing)
  // ========================================================================

  [[nodiscard]] constexpr bool validate() const { return tree_.validate(/*allow_equivalent=*/true); }

 private:
  using tree_type = detail::tree::binary_search_tree<Key, Key, detail::tree::identity<Key>, Compare, Allocator, Policy>;

  tree_type tree_;

  // merge() splices nodes between multisets that may differ in comparator
  // type, which are distinct class types — grant them mutual access to
  // tree_. (A friend redeclaration of a constrained template must repeat
  // its requires clause verbatim.)
  template <class K2, class C2, class A2, class P2>
    requires concepts::container_element<K2> && concepts::allocator_for<A2, K2> &&
             std::strict_weak_order<C2, const K2&, const K2&>
  friend class multiset;

  // Grant non-member operators access to tree_
  template <class K2, class C2, class A2, class P2>
  friend constexpr bool operator==(const multiset<K2, C2, A2, P2>& lhs, const multiset<K2, C2, A2, P2>& rhs);

  template <class K2, class C2, class A2, class P2>
  friend constexpr auto operator<=>(const multiset<K2, C2, A2, P2>& lhs, const multiset<K2, C2, A2, P2>& rhs);
};

// ============================================================================
// Deduction guides
// ============================================================================

// Standard-style deduction guides. Per [container.reqmts], a guide drops
// out of overload resolution when an allocator lands in a Compare slot or
// a non-allocator in an Allocator slot — the qualifies_as_allocator gate
// is what disambiguates (first, last, comp) from (first, last, alloc).
template <class InputIt,
          class Compare = std::less<std::iter_value_t<InputIt>>,
          class Allocator = allocator<std::iter_value_t<InputIt>>>
  requires std::input_iterator<InputIt> && (!concepts::qualifies_as_allocator<Compare>) &&
           concepts::qualifies_as_allocator<Allocator>
multiset(InputIt, InputIt, Compare = Compare(), Allocator = Allocator())
    -> multiset<std::iter_value_t<InputIt>, Compare, Allocator>;

template <class InputIt, class Allocator>
  requires std::input_iterator<InputIt> && concepts::qualifies_as_allocator<Allocator>
multiset(InputIt, InputIt, Allocator)
    -> multiset<std::iter_value_t<InputIt>, std::less<std::iter_value_t<InputIt>>, Allocator>;

// Standard-style from_range deduction guides.
template <class R,
          class Compare = std::less<std::ranges::range_value_t<R>>,
          class Allocator = allocator<std::ranges::range_value_t<R>>>
  requires std::ranges::input_range<R> && (!concepts::qualifies_as_allocator<Compare>) &&
           concepts::qualifies_as_allocator<Allocator>
multiset(std::from_range_t, R&&, Compare = Compare(), Allocator = Allocator())
    -> multiset<std::ranges::range_value_t<R>, Compare, Allocator>;

template <class R, class Allocator>
  requires std::ranges::input_range<R> && concepts::qualifies_as_allocator<Allocator>
multiset(std::from_range_t, R&&, Allocator)
    -> multiset<std::ranges::range_value_t<R>, std::less<std::ranges::range_value_t<R>>, Allocator>;

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
  requires(!concepts::qualifies_as_allocator<Compare>) && concepts::qualifies_as_allocator<Allocator>
multiset(std::initializer_list<Key>, Compare = Compare(), Allocator = Allocator()) -> multiset<Key, Compare, Allocator>;

template <class Key, class Allocator>
  requires concepts::qualifies_as_allocator<Allocator>
multiset(std::initializer_list<Key>, Allocator) -> multiset<Key, std::less<Key>, Allocator>;

// ============================================================================
// Non-member comparison operators
// ============================================================================

template <class Key, class Compare, class Allocator, class Policy>
[[nodiscard]] constexpr bool operator==(const multiset<Key, Compare, Allocator, Policy>& lhs,
                                        const multiset<Key, Compare, Allocator, Policy>& rhs) {
  return lhs.tree_ == rhs.tree_;
}

template <class Key, class Compare, class Allocator, class Policy>
[[nodiscard]] constexpr auto operator<=>(const multiset<Key, Compare, Allocator, Policy>& lhs,
                                         const multiset<Key, Compare, Allocator, Policy>& rhs) {
  return lhs.tree_ <=> rhs.tree_;
}

// ============================================================================
// Non-member swap
// ============================================================================

template <class Key, class Compare, class Allocator, class Policy>
constexpr void swap(multiset<Key, Compare, Allocator, Policy>& lhs,
                    multiset<Key, Compare, Allocator, Policy>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

// ============================================================================
// Convenience alias — included here because rb_tree.hpp is the default policy
// and is already transitively included. For treap / avl variants, include
// the corresponding opt-in header (e.g. <chaistl/containers/treap_multiset.hpp>).
// ============================================================================

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using rb_multiset = multiset<Key, Compare, Allocator, detail::tree::rb_tree_policy>;

}  // namespace chaistl
