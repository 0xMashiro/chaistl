// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/basic.hpp>
#include <chaistl/concepts/container_compatible_range.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/containers/associative/unordered_set.hpp>
#include <chaistl/containers/hash/hash_table.hpp>
#include <chaistl/containers/hash/key_extract.hpp>
#include <chaistl/memory/allocator.hpp>
#include <chaistl/memory_resource.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

namespace chaistl {

template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = allocator<Key>,
          class RehashPolicy>
  requires concepts::container_element<Key> && concepts::allocator_for<Allocator, Key> &&
           detail::hash::hasher_for<Hash, Key> && std::predicate<const KeyEqual&, const Key&, const Key&>
class unordered_multiset {
  using table_type =
      detail::hash::hash_table<Key, Key, detail::hash::identity<Key>, Hash, KeyEqual, Allocator, RehashPolicy>;

 public:
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
  using iterator = detail::hash::hash_iterator<value_type, true>;
  using const_iterator = iterator;
  using local_iterator = detail::hash::hash_local_iterator<value_type, true>;
  using const_local_iterator = local_iterator;
  using node_type = detail::hash::hash_node_handle<value_type, Allocator>;

  constexpr unordered_multiset() = default;

  explicit constexpr unordered_multiset(size_type bucket_count,
                                        const hasher& hash = hasher{},
                                        const key_equal& equal = key_equal{},
                                        const allocator_type& alloc = allocator_type{})
      : table_(bucket_count, hash, equal, alloc) {}

  constexpr unordered_multiset(size_type bucket_count, const allocator_type& alloc)
      : unordered_multiset(bucket_count, hasher{}, key_equal{}, alloc) {}

  constexpr unordered_multiset(size_type bucket_count, const hasher& hash, const allocator_type& alloc)
      : unordered_multiset(bucket_count, hash, key_equal{}, alloc) {}

  explicit constexpr unordered_multiset(const allocator_type& alloc) : table_(alloc) {}

  template <std::input_iterator InputIt>
  constexpr unordered_multiset(InputIt first,
                               InputIt last,
                               size_type bucket_count = 0,
                               const hasher& hash = hasher{},
                               const key_equal& equal = key_equal{},
                               const allocator_type& alloc = allocator_type{})
      : table_(bucket_count, hash, equal, alloc) {
    insert(first, last);
  }

  template <std::input_iterator InputIt>
  constexpr unordered_multiset(InputIt first, InputIt last, size_type bucket_count, const allocator_type& alloc)
      : unordered_multiset(first, last, bucket_count, hasher{}, key_equal{}, alloc) {}

  template <std::input_iterator InputIt>
  constexpr unordered_multiset(
      InputIt first, InputIt last, size_type bucket_count, const hasher& hash, const allocator_type& alloc)
      : unordered_multiset(first, last, bucket_count, hash, key_equal{}, alloc) {}

  constexpr unordered_multiset(std::initializer_list<value_type> init,
                               size_type bucket_count = 0,
                               const hasher& hash = hasher{},
                               const key_equal& equal = key_equal{},
                               const allocator_type& alloc = allocator_type{})
      : unordered_multiset(init.begin(), init.end(), bucket_count, hash, equal, alloc) {}

  constexpr unordered_multiset(std::initializer_list<value_type> init,
                               size_type bucket_count,
                               const allocator_type& alloc)
      : unordered_multiset(init, bucket_count, hasher{}, key_equal{}, alloc) {}

  constexpr unordered_multiset(std::initializer_list<value_type> init,
                               size_type bucket_count,
                               const hasher& hash,
                               const allocator_type& alloc)
      : unordered_multiset(init, bucket_count, hash, key_equal{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R>
  constexpr unordered_multiset(std::from_range_t,
                               R&& range,
                               size_type bucket_count = 0,
                               const hasher& hash = hasher{},
                               const key_equal& equal = key_equal{},
                               const allocator_type& alloc = allocator_type{})
      : table_(bucket_count, hash, equal, alloc) {
    insert_range(std::forward<R>(range));
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr unordered_multiset(std::from_range_t, R&& range, size_type bucket_count, const allocator_type& alloc)
      : unordered_multiset(std::from_range, std::forward<R>(range), bucket_count, hasher{}, key_equal{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R>
  constexpr unordered_multiset(
      std::from_range_t, R&& range, size_type bucket_count, const hasher& hash, const allocator_type& alloc)
      : unordered_multiset(std::from_range, std::forward<R>(range), bucket_count, hash, key_equal{}, alloc) {}

  constexpr unordered_multiset(const unordered_multiset&) = default;
  constexpr unordered_multiset(const unordered_multiset& other, const std::type_identity_t<Allocator>& alloc)
      : table_(other.table_, alloc) {}
  constexpr unordered_multiset(unordered_multiset&&) = default;
  constexpr unordered_multiset(unordered_multiset&& other, const std::type_identity_t<Allocator>& alloc)
      : table_(std::move(other.table_), alloc) {}
  constexpr ~unordered_multiset() = default;

  constexpr unordered_multiset& operator=(const unordered_multiset&) = default;
  constexpr unordered_multiset& operator=(unordered_multiset&&) = default;

  constexpr unordered_multiset& operator=(std::initializer_list<value_type> init) {
    clear();
    insert(init);
    return *this;
  }

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return table_.get_allocator(); }
  [[nodiscard]] constexpr iterator begin() noexcept { return table_.begin(); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return table_.begin(); }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
  [[nodiscard]] constexpr iterator end() noexcept { return table_.end(); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return table_.end(); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }
  [[nodiscard]] constexpr bool empty() const noexcept { return table_.empty(); }
  [[nodiscard]] constexpr size_type size() const noexcept { return table_.size(); }
  [[nodiscard]] constexpr size_type max_size() const noexcept { return table_.max_size(); }

  constexpr void clear() noexcept { table_.clear(); }
  constexpr iterator insert(const value_type& value) { return table_.insert_multi(value); }
  constexpr iterator insert(value_type&& value) { return table_.insert_multi(std::move(value)); }
  constexpr iterator insert(const_iterator, const value_type& value) { return insert(value); }
  constexpr iterator insert(const_iterator, value_type&& value) { return insert(std::move(value)); }

  template <std::input_iterator InputIt>
  constexpr void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      table_.insert_multi(*first);
    }
  }

  constexpr void insert(std::initializer_list<value_type> init) { insert(init.begin(), init.end()); }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr iterator insert(K&& key) {
    return table_.insert_transparent_multi(std::forward<K>(key));
  }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr iterator insert(const_iterator, K&& key) {
    return insert(std::forward<K>(key));
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr void insert_range(R&& range) {
    for (auto&& element : range) {
      table_.insert_multi(std::forward<decltype(element)>(element));
    }
  }

  template <class... Args>
  constexpr iterator emplace(Args&&... args) {
    return table_.emplace_multi(std::forward<Args>(args)...);
  }

  template <class... Args>
  constexpr iterator emplace_hint(const_iterator, Args&&... args) {
    return emplace(std::forward<Args>(args)...);
  }

  constexpr iterator erase(const_iterator pos) { return table_.erase(pos); }

  constexpr iterator erase(const_iterator first, const_iterator last) {
    while (first != last) {
      first = table_.erase(first);
    }
    return first;
  }

  constexpr size_type erase(const key_type& key) { return table_.erase_multi(key); }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr size_type erase(K&& key) {
    return table_.erase_multi(key);
  }

  constexpr void swap(unordered_multiset& other) noexcept(std::is_nothrow_swappable_v<Hash> &&
                                                          std::is_nothrow_swappable_v<KeyEqual>) {
    table_.swap(other.table_);
  }

  constexpr node_type extract(const_iterator pos) { return table_.extract(pos); }
  constexpr node_type extract(const key_type& key) { return table_.extract_key(key); }

  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual> &&
             (!std::is_convertible_v<K &&, iterator>) && (!std::is_convertible_v<K &&, const_iterator>)
  constexpr node_type extract(K&& key) {
    return table_.extract_key(key);
  }

  constexpr iterator insert(node_type&& nh) { return table_.insert_node_multi(nh); }
  constexpr iterator insert(const_iterator, node_type&& nh) { return insert(std::move(nh)); }

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_set<Key, Hash2, KeyEqual2, Allocator, RehashPolicy2>& source) {
    table_.merge_multi(source.table_);
  }

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_set<Key, Hash2, KeyEqual2, Allocator, RehashPolicy2>&& source) {
    merge(source);
  }

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_multiset<Key, Hash2, KeyEqual2, Allocator, RehashPolicy2>& source) {
    table_.merge_multi(source.table_);
  }

  template <class Hash2, class KeyEqual2, class RehashPolicy2>
  constexpr void merge(unordered_multiset<Key, Hash2, KeyEqual2, Allocator, RehashPolicy2>&& source) {
    merge(source);
  }

  [[nodiscard]] constexpr iterator find(const key_type& key) { return table_.find(key); }
  [[nodiscard]] constexpr const_iterator find(const key_type& key) const { return table_.find(key); }
  [[nodiscard]] constexpr size_type count(const key_type& key) const { return table_.count_multi(key); }
  [[nodiscard]] constexpr bool contains(const key_type& key) const { return table_.contains(key); }
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const key_type& key) {
    return table_.equal_range_multi(key);
  }
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
    return table_.equal_range_multi(key);
  }

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
    return table_.count_multi(key);
  }
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr bool contains(const K& key) const {
    return table_.contains(key);
  }
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const K& key) {
    return table_.equal_range_multi(key);
  }
  template <class K>
    requires concepts::transparent_compare<Hash> && concepts::transparent_compare<KeyEqual>
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
    return table_.equal_range_multi(key);
  }

  [[nodiscard]] constexpr size_type bucket_count() const noexcept { return table_.bucket_count(); }
  [[nodiscard]] constexpr size_type max_bucket_count() const noexcept { return table_.max_bucket_count(); }
  [[nodiscard]] constexpr size_type bucket_size(size_type index) const noexcept { return table_.bucket_size(index); }
  [[nodiscard]] constexpr size_type bucket(const key_type& key) const { return table_.bucket(key); }
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
  [[nodiscard]] constexpr float load_factor() const noexcept { return table_.load_factor(); }
  [[nodiscard]] constexpr float max_load_factor() const noexcept { return table_.max_load_factor(); }
  constexpr void max_load_factor(float factor) noexcept { table_.max_load_factor(factor); }
  constexpr void rehash(size_type bucket_count_request) { table_.rehash(bucket_count_request); }
  constexpr void reserve(size_type element_count) { table_.reserve(element_count); }
  [[nodiscard]] constexpr hasher hash_function() const { return table_.hash_function(); }
  [[nodiscard]] constexpr key_equal key_eq() const { return table_.key_eq(); }
  [[nodiscard]] constexpr bool validate() const { return table_.validate(); }

  [[nodiscard]] friend constexpr bool operator==(const unordered_multiset& lhs, const unordered_multiset& rhs)
    requires std::equality_comparable<value_type>
  {
    return lhs.table_.equals_multi(rhs.table_);
  }

 private:
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

template <class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>, class Allocator = allocator<Key>>
using power2_unordered_multiset =
    unordered_multiset<Key, Hash, KeyEqual, Allocator, detail::hash::power2_rehash_policy>;

template <class Key, class Hash, class KeyEqual, class Allocator, class RehashPolicy>
  requires concepts::container_element<Key> && concepts::allocator_for<Allocator, Key> &&
           detail::hash::hasher_for<Hash, Key> && std::predicate<const KeyEqual&, const Key&, const Key&>
template <class Hash2, class KeyEqual2, class RehashPolicy2>
constexpr void unordered_set<Key, Hash, KeyEqual, Allocator, RehashPolicy>::merge(
    unordered_multiset<Key, Hash2, KeyEqual2, Allocator, RehashPolicy2>& source) {
  table_.merge_unique(source.table_);
}

template <class InputIt,
          class Hash = std::hash<std::iter_value_t<InputIt>>,
          class Pred = std::equal_to<std::iter_value_t<InputIt>>,
          class Allocator = allocator<std::iter_value_t<InputIt>>>
  requires std::input_iterator<InputIt> && (!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
               (!concepts::qualifies_as_allocator<Pred>) && concepts::qualifies_as_allocator<Allocator>
unordered_multiset(InputIt, InputIt, std::size_t = 0, Hash = Hash(), Pred = Pred(), Allocator = Allocator())
    -> unordered_multiset<std::iter_value_t<InputIt>, Hash, Pred, Allocator>;

template <class T, class Hash = std::hash<T>, class Pred = std::equal_to<T>, class Allocator = allocator<T>>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
              (!concepts::qualifies_as_allocator<Pred>) && concepts::qualifies_as_allocator<Allocator>
unordered_multiset(std::initializer_list<T>, std::size_t = 0, Hash = Hash(), Pred = Pred(), Allocator = Allocator())
    -> unordered_multiset<T, Hash, Pred, Allocator>;

template <class InputIt, class Allocator>
  requires std::input_iterator<InputIt> && concepts::qualifies_as_allocator<Allocator>
unordered_multiset(InputIt, InputIt, std::size_t, Allocator)
    -> unordered_multiset<std::iter_value_t<InputIt>,
                          std::hash<std::iter_value_t<InputIt>>,
                          std::equal_to<std::iter_value_t<InputIt>>,
                          Allocator>;

template <class InputIt, class Hash, class Allocator>
  requires std::input_iterator<InputIt> && (!concepts::qualifies_as_allocator<Hash>) &&
               (!std::is_integral_v<Hash>) && concepts::qualifies_as_allocator<Allocator>
unordered_multiset(InputIt, InputIt, std::size_t, Hash, Allocator)
    -> unordered_multiset<std::iter_value_t<InputIt>, Hash, std::equal_to<std::iter_value_t<InputIt>>, Allocator>;

template <class T, class Allocator>
  requires concepts::qualifies_as_allocator<Allocator>
unordered_multiset(std::initializer_list<T>,
                   std::size_t,
                   Allocator) -> unordered_multiset<T, std::hash<T>, std::equal_to<T>, Allocator>;

template <class T, class Hash, class Allocator>
  requires(!concepts::qualifies_as_allocator<Hash>) &&
              (!std::is_integral_v<Hash>) && concepts::qualifies_as_allocator<Allocator>
unordered_multiset(std::initializer_list<T>, std::size_t, Hash, Allocator)
    -> unordered_multiset<T, Hash, std::equal_to<T>, Allocator>;

template <std::ranges::input_range R,
          class Hash = std::hash<std::ranges::range_value_t<R>>,
          class Pred = std::equal_to<std::ranges::range_value_t<R>>,
          class Allocator = allocator<std::ranges::range_value_t<R>>>
  requires(!concepts::qualifies_as_allocator<Hash>) && (!std::is_integral_v<Hash>) &&
              (!concepts::qualifies_as_allocator<Pred>) && concepts::qualifies_as_allocator<Allocator>
unordered_multiset(std::from_range_t, R&&, std::size_t = 0, Hash = Hash(), Pred = Pred(), Allocator = Allocator())
    -> unordered_multiset<std::ranges::range_value_t<R>, Hash, Pred, Allocator>;

template <std::ranges::input_range R, class Allocator>
  requires concepts::qualifies_as_allocator<Allocator>
unordered_multiset(std::from_range_t, R&&, std::size_t, Allocator)
    -> unordered_multiset<std::ranges::range_value_t<R>,
                          std::hash<std::ranges::range_value_t<R>>,
                          std::equal_to<std::ranges::range_value_t<R>>,
                          Allocator>;

template <std::ranges::input_range R, class Hash, class Allocator>
  requires(!concepts::qualifies_as_allocator<Hash>) &&
              (!std::is_integral_v<Hash>) && concepts::qualifies_as_allocator<Allocator>
unordered_multiset(std::from_range_t, R&&, std::size_t, Hash, Allocator)
    -> unordered_multiset<std::ranges::range_value_t<R>, Hash, std::equal_to<std::ranges::range_value_t<R>>, Allocator>;

template <class Key, class Hash, class KeyEqual, class Allocator, class RehashPolicy, class Predicate>
constexpr typename unordered_multiset<Key, Hash, KeyEqual, Allocator, RehashPolicy>::size_type erase_if(
    unordered_multiset<Key, Hash, KeyEqual, Allocator, RehashPolicy>& set, Predicate pred) {
  typename unordered_multiset<Key, Hash, KeyEqual, Allocator, RehashPolicy>::size_type removed = 0;
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
constexpr void swap(
    unordered_multiset<Key, Hash, KeyEqual, Allocator, RehashPolicy>& lhs,
    unordered_multiset<Key, Hash, KeyEqual, Allocator, RehashPolicy>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

namespace pmr {

template <class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using unordered_multiset = chaistl::unordered_multiset<Key, Hash, KeyEqual, chaistl::pmr::polymorphic_allocator<Key>>;

}  // namespace pmr

}  // namespace chaistl
