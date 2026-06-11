// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// multimap - Ordered associative container with non-unique mapped keys
// ============================================================================
//
// Architecture:
//   - Thin public wrapper over detail::tree::binary_search_tree storing
//     pair<const Key, T> nodes.
//   - Uses the tree's equivalent-key insertion family, so multiple elements
//     whose keys compare equivalent may coexist.
//   - The balancing scheme is a trailing policy parameter, defaulting to the
//     same red-black policy as map.
//
// Standardization archaeology:
//   - multimap was standardized in C++98 for ordered one-to-many key/value
//     relationships.
//   - LWG 233 clarified insertion order within an equivalent-key run: plain
//     insert places the new element at the upper bound, preserving stable
//     ordering among equivalent keys.
//   - It intentionally lacks operator[], at(), try_emplace(), and
//     insert_or_assign() because "the" mapped value for a key is ambiguous.
//   - C++17 node handles and merge/extract apply here too, but insertion of a
//     node returns only an iterator because duplicates are allowed.
//
// Non-standard extensions:
//   - The Policy template parameter exposes rb/avl/treap experiments while
//     leaving multimap<Key, T> source-compatible with the standard shape.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/multimap
//   - cppreference: https://en.cppreference.com/w/cpp/container/multimap
//   - LWG defects: https://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html
//   - ADR-004: Tree Policy-Based Design

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/basic.hpp>
#include <chaistl/concepts/container_compatible_range.hpp>
#include <chaistl/containers/associative/detail/map_deduction.hpp>
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
#include <tuple>
#include <type_traits>
#include <utility>

namespace chaistl {

/**
 * @brief Ordered associative container with mapped values, allowing
 *        equivalent keys.
 *
 * @ingroup containers_associative
 *
 * Stores key-value pairs sorted by key; multiple pairs with equivalent keys
 * may coexist, and within a run of equivalent elements insertion order is
 * preserved (LWG 233: plain insert places the element at the upper bound).
 * Implemented over the same detail::tree::binary_search_tree as map, using
 * its equivalent-keys operation family; the balancing scheme is the same
 * policy template parameter defaulting to the red-black tree (ADR 004).
 *
 * Unlike map there is no operator[], at(), try_emplace() or
 * insert_or_assign() — those require "the unique element with this key" to
 * be meaningful.
 *
 * References:
 * - C++ Draft Standard: https://eel.is/c++draft/multimap
 * - cppreference: https://en.cppreference.com/w/cpp/container/multimap
 */
template <class Key,
          class T,
          class Compare = std::less<Key>,
          class Allocator = allocator<std::pair<const Key, T>>,
          class Policy = detail::tree::rb_tree_policy>
  requires concepts::container_element<std::pair<const Key, T>> &&
           concepts::allocator_for<Allocator, std::pair<const Key, T>> &&
           std::strict_weak_order<Compare, const Key&, const Key&>
class multimap {
 public:
  // ========================================================================
  // Member types
  // ========================================================================

  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<const Key, T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using key_compare = Compare;
  using allocator_type = Allocator;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

  using iterator = detail::tree::bst_iterator<value_type, false>;
  using const_iterator = detail::tree::bst_iterator<value_type, true>;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;

  // ========================================================================
  // Node handle (C++17)
  // ========================================================================

  // No insert_return_type: that member belongs to unique-key containers
  // only; multimap::insert(node_type&&) returns a plain iterator.
  using node_type = detail::tree::bst_node_handle<value_type, Policy, Allocator>;

  // ========================================================================
  // Nested value_compare class
  // ========================================================================

  class value_compare {
   public:
    [[nodiscard]] constexpr bool operator()(const value_type& lhs, const value_type& rhs) const {
      return comp_(std::get<0>(lhs), std::get<0>(rhs));
    }

   protected:
    friend class multimap;

    explicit constexpr value_compare(Compare comp) : comp_(std::move(comp)) {}

    Compare comp_;
  };

  // ========================================================================
  // Constructors and destructor
  // ========================================================================

  constexpr multimap() = default;

  explicit constexpr multimap(const Compare& comp, const Allocator& alloc = Allocator{}) : tree_(comp, alloc) {}

  explicit constexpr multimap(const Allocator& alloc) : tree_(alloc) {}

  // The tree's iterator-range constructors insert with the unique family,
  // so the multi containers construct empty and bulk-insert instead; if an
  // insertion throws, the fully constructed tree_ member cleans up.
  template <std::input_iterator InputIt>
  constexpr multimap(InputIt first, InputIt last, const Compare& comp = Compare{}, const Allocator& alloc = Allocator{})
      : tree_(comp, alloc) {
    tree_.insert_equal(first, last);
  }

  template <std::input_iterator InputIt>
  constexpr multimap(InputIt first, InputIt last, const Allocator& alloc) : multimap(first, last, Compare{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R>
  constexpr multimap(std::from_range_t,
                     R&& range,
                     const Compare& comp = Compare{},
                     const Allocator& alloc = Allocator{})
      : tree_(comp, alloc) {
    insert_range(std::forward<R>(range));
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr multimap(std::from_range_t, R&& range, const Allocator& alloc)
      : multimap(std::from_range, std::forward<R>(range), Compare{}, alloc) {}

  constexpr multimap(std::initializer_list<value_type> init,
                     const Compare& comp = Compare{},
                     const Allocator& alloc = Allocator{})
      : multimap(init.begin(), init.end(), comp, alloc) {}

  constexpr multimap(std::initializer_list<value_type> init, const Allocator& alloc)
      : multimap(init.begin(), init.end(), Compare{}, alloc) {}

  constexpr multimap(const multimap& other) = default;
  constexpr multimap(const multimap& other, const std::type_identity_t<Allocator>& alloc) : tree_(other.tree_, alloc) {}

  constexpr multimap(multimap&& other) = default;
  constexpr multimap(multimap&& other, const std::type_identity_t<Allocator>& alloc)
      : tree_(std::move(other.tree_), alloc) {}

  constexpr ~multimap() = default;

  // ========================================================================
  // Assignment
  // ========================================================================

  constexpr multimap& operator=(const multimap& other) = default;
  constexpr multimap& operator=(multimap&& other) = default;
  constexpr multimap& operator=(std::initializer_list<value_type> init) {
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
    CHAI_HARDENED(pos != end(), "chaistl::multimap::erase: invalid iterator");
    return tree_.erase(pos);
  }

  constexpr iterator erase(const_iterator pos) noexcept {
    CHAI_HARDENED(pos != end(), "chaistl::multimap::erase: invalid iterator");
    return tree_.erase(pos);
  }
  constexpr iterator erase(const_iterator first, const_iterator last) noexcept { return tree_.erase(first, last); }

  /// Erases every element with key equivalent to @p key; returns how many.
  constexpr size_type erase(const key_type& key) { return tree_.erase_equal(key); }

  template <class K>
    requires concepts::transparent_compare<Compare>
  constexpr size_type erase(const K& key) {
    return tree_.erase_equal(key);
  }

  // --- extract ---

  constexpr node_type extract(const_iterator pos) { return tree_.extract(pos); }

  /// Extracts the first element with key equivalent to @p key (or empty).
  constexpr node_type extract(const key_type& key) { return tree_.extract(key); }

  // Heterogeneous extract: same forward-looking P2077 companion as map.
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

  // [multimap.modifiers]: only the comparator may differ — the allocator
  // type is fixed, because spliced nodes keep their original allocation.
  // The map<->multimap cross-family overloads the standard also specifies
  // are not provided yet (they need mutual friendship across headers).
  template <class Compare2>
  constexpr void merge(multimap<Key, T, Compare2, Allocator, Policy>& source) {
    tree_.merge_equal(source.tree_);
  }

  template <class Compare2>
  constexpr void merge(multimap<Key, T, Compare2, Allocator, Policy>&& source) {
    tree_.merge_equal(std::move(source.tree_));
  }

  // --- swap ---

  constexpr void swap(multimap& other) noexcept(noexcept(tree_.swap(other.tree_))) { tree_.swap(other.tree_); }

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
  using tree_type = detail::tree::
      binary_search_tree<Key, value_type, detail::tree::select1st<value_type>, Compare, Allocator, Policy>;

  tree_type tree_;

  // merge() splices nodes between multimaps that may differ in comparator
  // type, which are distinct class types — grant them mutual access to
  // tree_. (A friend redeclaration of a constrained template must repeat
  // its requires clause verbatim.)
  template <class K2, class T2, class C2, class A2, class P2>
    requires concepts::container_element<std::pair<const K2, T2>> &&
             concepts::allocator_for<A2, std::pair<const K2, T2>> && std::strict_weak_order<C2, const K2&, const K2&>
  friend class multimap;

  // Grant non-member operators access to tree_
  template <class K2, class T2, class C2, class A2, class P2>
  friend constexpr bool operator==(const multimap<K2, T2, C2, A2, P2>& lhs, const multimap<K2, T2, C2, A2, P2>& rhs);

  template <class K2, class T2, class C2, class A2, class P2>
  friend constexpr auto operator<=>(const multimap<K2, T2, C2, A2, P2>& lhs, const multimap<K2, T2, C2, A2, P2>& rhs);
};

// ============================================================================
// Deduction guides
// ============================================================================

// Standard-style deduction guides. Per [container.reqmts], a guide drops
// out of overload resolution when an allocator lands in a Compare slot or
// a non-allocator in an Allocator slot — the qualifies_as_allocator gate
// is what disambiguates (first, last, comp) from (first, last, alloc).
template <class InputIt,
          class Compare = std::less<detail::set_map_deduction::iter_key_t<InputIt>>,
          class Allocator = allocator<detail::set_map_deduction::iter_alloc_t<InputIt>>>
  requires std::input_iterator<InputIt> && (!concepts::qualifies_as_allocator<Compare>) &&
           concepts::qualifies_as_allocator<Allocator>
multimap(InputIt, InputIt, Compare = Compare(), Allocator = Allocator())
    -> multimap<detail::set_map_deduction::iter_key_t<InputIt>,
                detail::set_map_deduction::iter_mapped_t<InputIt>,
                Compare,
                Allocator>;

template <class InputIt, class Allocator>
  requires std::input_iterator<InputIt> && concepts::qualifies_as_allocator<Allocator>
multimap(InputIt, InputIt, Allocator) -> multimap<detail::set_map_deduction::iter_key_t<InputIt>,
                                                  detail::set_map_deduction::iter_mapped_t<InputIt>,
                                                  std::less<detail::set_map_deduction::iter_key_t<InputIt>>,
                                                  Allocator>;

// Standard-style from_range deduction guides.
template <class R,
          class Compare = std::less<detail::set_map_deduction::range_key_t<R>>,
          class Allocator = allocator<detail::set_map_deduction::range_alloc_t<R>>>
  requires std::ranges::input_range<R> && (!concepts::qualifies_as_allocator<Compare>) &&
           concepts::qualifies_as_allocator<Allocator>
multimap(std::from_range_t, R&&, Compare = Compare(), Allocator = Allocator())
    -> multimap<detail::set_map_deduction::range_key_t<R>,
                detail::set_map_deduction::range_mapped_t<R>,
                Compare,
                Allocator>;

template <class R, class Allocator>
  requires std::ranges::input_range<R> && concepts::qualifies_as_allocator<Allocator>
multimap(std::from_range_t, R&&, Allocator) -> multimap<detail::set_map_deduction::range_key_t<R>,
                                                        detail::set_map_deduction::range_mapped_t<R>,
                                                        std::less<detail::set_map_deduction::range_key_t<R>>,
                                                        Allocator>;

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
  requires(!concepts::qualifies_as_allocator<Compare>) && concepts::qualifies_as_allocator<Allocator>
multimap(std::initializer_list<std::pair<Key, T>>, Compare = Compare(), Allocator = Allocator())
    -> multimap<Key, T, Compare, Allocator>;

template <class Key, class T, class Allocator>
  requires concepts::qualifies_as_allocator<Allocator>
multimap(std::initializer_list<std::pair<Key, T>>, Allocator) -> multimap<Key, T, std::less<Key>, Allocator>;

// ============================================================================
// Non-member comparison operators
// ============================================================================

template <class Key, class T, class Compare, class Allocator, class Policy>
[[nodiscard]] constexpr bool operator==(const multimap<Key, T, Compare, Allocator, Policy>& lhs,
                                        const multimap<Key, T, Compare, Allocator, Policy>& rhs) {
  return lhs.tree_ == rhs.tree_;
}

template <class Key, class T, class Compare, class Allocator, class Policy>
[[nodiscard]] constexpr auto operator<=>(const multimap<Key, T, Compare, Allocator, Policy>& lhs,
                                         const multimap<Key, T, Compare, Allocator, Policy>& rhs) {
  return lhs.tree_ <=> rhs.tree_;
}

// ============================================================================
// Non-member swap
// ============================================================================

template <class Key, class T, class Compare, class Allocator, class Policy>
constexpr void swap(multimap<Key, T, Compare, Allocator, Policy>& lhs,
                    multimap<Key, T, Compare, Allocator, Policy>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

// ============================================================================
// Convenience alias — included here because rb_tree.hpp is the default policy
// and is already transitively included. For treap / avl variants, include
// the corresponding opt-in header (e.g. <chaistl/containers/treap_multimap.hpp>).
// ============================================================================

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using rb_multimap = multimap<Key, T, Compare, Allocator, detail::tree::rb_tree_policy>;

}  // namespace chaistl
