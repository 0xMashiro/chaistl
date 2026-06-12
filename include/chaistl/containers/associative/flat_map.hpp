// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// flat_map - Sorted unique-key associative adaptor over parallel arrays
// ============================================================================
//
// Architecture:
//   - Stores sorted keys and mapped values in two random-access containers,
//     defaulting to chaistl::vector<Key> plus chaistl::vector<T>.
//   - The iterator zips the two arrays into pair-like references, keeping keys
//     immutable and mapped values mutable.
//   - Lookup is binary search over keys; insertion and erase move elements in
//     both arrays to preserve positional correspondence.
//
// Standardization archaeology:
//   - C++23 standardized flat_map from the P0429/P1222 proposal line, bringing
//     sorted-vector associative containers into the standard family.
//   - Unlike the common Boost.Container representation of vector<pair<K, V>>,
//     the standard flat_map is specified as two containers. That permits dense
//     key scans and independent storage choices for keys and mapped values.
//   - The sorted_unique construction tag acknowledges a practical performance
//     path: bulk construction from already-normalized data can avoid sorting
//     and duplicate removal.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/flat.map
//   - cppreference: https://en.cppreference.com/w/cpp/container/flat_map
//   - P0429: https://wg21.link/P0429
//   - P1222: https://wg21.link/P1222
//   - Boost.Container flat_map: https://www.boost.org/doc/libs/release/doc/html/container/non_standard_containers.html

#include <chaistl/concepts/basic.hpp>
#include <chaistl/concepts/container_compatible_range.hpp>
#include <chaistl/containers/associative/detail/flat_tags.hpp>
#include <chaistl/containers/vector.hpp>
#include <chaistl/utility/hardening.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace chaistl {

namespace detail {

template <class InputIt>
using iter_key_t = std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>;

template <class InputIt>
using iter_mapped_t = typename std::iterator_traits<InputIt>::value_type::second_type;

template <class R>
using range_key_t = std::remove_const_t<typename std::ranges::range_value_t<R>::first_type>;

template <class R>
using range_mapped_t = typename std::ranges::range_value_t<R>::second_type;

}  // namespace detail

/**
 * @brief Sorted unique-key associative container adaptor over two random-access containers.
 *
 * @ingroup containers_associative
 *
 * `flat_map` stores keys and mapped values in separate underlying containers.
 * The default storage is `chaistl::vector<Key>` plus `chaistl::vector<T>`.
 *
 * References:
 * - C++23 `std::flat_map`: https://en.cppreference.com/w/cpp/container/flat_map
 * - libstdc++ and libc++ `<flat_map>` implementations
 * - Boost.Container `flat_map` and EASTL `vector_map`
 */
template <class Key,
          class T,
          class Compare = std::less<Key>,
          class KeyContainer = chaistl::vector<Key>,
          class MappedContainer = chaistl::vector<T>>
  requires std::same_as<Key, typename KeyContainer::value_type> &&
           std::same_as<T, typename MappedContainer::value_type> &&
           std::random_access_iterator<typename KeyContainer::iterator> &&
           std::random_access_iterator<typename KeyContainer::const_iterator> &&
           std::random_access_iterator<typename MappedContainer::iterator> &&
           std::random_access_iterator<typename MappedContainer::const_iterator>
class flat_map {
  template <bool IsConst>
  class basic_iterator;

 public:
  using key_container_type = KeyContainer;
  using mapped_container_type = MappedContainer;
  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<key_type, mapped_type>;
  using key_compare = Compare;
  using reference = std::pair<const key_type&, mapped_type&>;
  using const_reference = std::pair<const key_type&, const mapped_type&>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using iterator = basic_iterator<false>;
  using const_iterator = basic_iterator<true>;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;

  struct containers {
    key_container_type keys;
    mapped_container_type values;
  };

  class value_compare {
   public:
    template <class L, class R>
    [[nodiscard]] constexpr bool operator()(const L& lhs, const R& rhs) const {
      return comp_(lhs.first, rhs.first);
    }

   protected:
    friend class flat_map;

    explicit constexpr value_compare(key_compare comp) : comp_(std::move(comp)) {}

    key_compare comp_;
  };

  constexpr flat_map() = default;

  explicit constexpr flat_map(const key_compare& comp) : compare_(comp) {}

  constexpr flat_map(key_container_type key_cont,
                     mapped_container_type mapped_cont,
                     const key_compare& comp = key_compare{})
      : flat_map(containers{std::move(key_cont), std::move(mapped_cont)}, comp) {}

  explicit constexpr flat_map(containers cont, const key_compare& comp = key_compare{})
      : keys_(std::move(cont.keys)), values_(std::move(cont.values)), compare_(comp) {
    CHAI_HARDENED(keys_.size() == values_.size(), "chaistl::flat_map: container sizes differ");
    sort_and_unique();
  }

  constexpr flat_map(sorted_unique_t,
                     key_container_type key_cont,
                     mapped_container_type mapped_cont,
                     const key_compare& comp = key_compare{})
      : flat_map(sorted_unique, containers{std::move(key_cont), std::move(mapped_cont)}, comp) {}

  constexpr flat_map(sorted_unique_t, containers cont, const key_compare& comp = key_compare{})
      : keys_(std::move(cont.keys)), values_(std::move(cont.values)), compare_(comp) {
    CHAI_HARDENED(validate(), "chaistl::flat_map: sorted_unique input is invalid");
  }

  template <std::input_iterator InputIt>
  constexpr flat_map(InputIt first, InputIt last, const key_compare& comp = key_compare{}) : compare_(comp) {
    insert(first, last);
  }

  template <std::input_iterator InputIt>
  constexpr flat_map(sorted_unique_t, InputIt first, InputIt last, const key_compare& comp = key_compare{})
      : compare_(comp) {
    insert(sorted_unique, first, last);
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr flat_map(std::from_range_t, R&& range, const key_compare& comp = key_compare{}) : compare_(comp) {
    insert_range(std::forward<R>(range));
  }

  constexpr flat_map(std::initializer_list<value_type> init, const key_compare& comp = key_compare{})
      : flat_map(init.begin(), init.end(), comp) {}

  constexpr flat_map(sorted_unique_t tag,
                     std::initializer_list<value_type> init,
                     const key_compare& comp = key_compare{})
      : flat_map(tag, init.begin(), init.end(), comp) {}

  // Allocator-extended constructors. The allocator is forwarded to both
  // underlying containers through uses-allocator construction.

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  explicit constexpr flat_map(const Alloc& alloc) : flat_map(key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(const key_compare& comp, const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<key_container_type>(alloc)),
        values_(std::make_obj_using_allocator<mapped_container_type>(alloc)),
        compare_(comp) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(const key_container_type& key_cont,
                     const mapped_container_type& mapped_cont,
                     const key_compare& comp,
                     const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<key_container_type>(alloc, key_cont)),
        values_(std::make_obj_using_allocator<mapped_container_type>(alloc, mapped_cont)),
        compare_(comp) {
    CHAI_HARDENED(keys_.size() == values_.size(), "chaistl::flat_map: container sizes differ");
    sort_and_unique();
  }

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(const key_container_type& key_cont, const mapped_container_type& mapped_cont, const Alloc& alloc)
      : flat_map(key_cont, mapped_cont, key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(sorted_unique_t,
                     const key_container_type& key_cont,
                     const mapped_container_type& mapped_cont,
                     const key_compare& comp,
                     const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<key_container_type>(alloc, key_cont)),
        values_(std::make_obj_using_allocator<mapped_container_type>(alloc, mapped_cont)),
        compare_(comp) {
    CHAI_HARDENED(validate(), "chaistl::flat_map: sorted_unique input is invalid");
  }

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(sorted_unique_t tag,
                     const key_container_type& key_cont,
                     const mapped_container_type& mapped_cont,
                     const Alloc& alloc)
      : flat_map(tag, key_cont, mapped_cont, key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(const flat_map& other, const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<key_container_type>(alloc, other.keys_)),
        values_(std::make_obj_using_allocator<mapped_container_type>(alloc, other.values_)),
        compare_(other.compare_) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(flat_map&& other, const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<key_container_type>(alloc, std::move(other.keys_))),
        values_(std::make_obj_using_allocator<mapped_container_type>(alloc, std::move(other.values_))),
        compare_(std::move(other.compare_)) {
    other.clear();
  }

  template <std::input_iterator InputIt, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(InputIt first, InputIt last, const key_compare& comp, const Alloc& alloc) : flat_map(comp, alloc) {
    insert(first, last);
  }

  template <std::input_iterator InputIt, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(InputIt first, InputIt last, const Alloc& alloc) : flat_map(first, last, key_compare{}, alloc) {}

  template <std::input_iterator InputIt, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(sorted_unique_t tag, InputIt first, InputIt last, const key_compare& comp, const Alloc& alloc)
      : flat_map(comp, alloc) {
    insert(tag, first, last);
  }

  template <std::input_iterator InputIt, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(sorted_unique_t tag, InputIt first, InputIt last, const Alloc& alloc)
      : flat_map(tag, first, last, key_compare{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(std::from_range_t, R&& range, const key_compare& comp, const Alloc& alloc)
      : flat_map(comp, alloc) {
    insert_range(std::forward<R>(range));
  }

  template <concepts::container_compatible_range<value_type> R, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(std::from_range_t tag, R&& range, const Alloc& alloc)
      : flat_map(tag, std::forward<R>(range), key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(std::initializer_list<value_type> init, const key_compare& comp, const Alloc& alloc)
      : flat_map(init.begin(), init.end(), comp, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(std::initializer_list<value_type> init, const Alloc& alloc)
      : flat_map(init, key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(sorted_unique_t tag,
                     std::initializer_list<value_type> init,
                     const key_compare& comp,
                     const Alloc& alloc)
      : flat_map(tag, init.begin(), init.end(), comp, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer, MappedContainer>
  constexpr flat_map(sorted_unique_t tag, std::initializer_list<value_type> init, const Alloc& alloc)
      : flat_map(tag, init, key_compare{}, alloc) {}

  constexpr flat_map& operator=(std::initializer_list<value_type> init) {
    clear();
    insert(init.begin(), init.end());
    return *this;
  }

  [[nodiscard]] constexpr mapped_type& at(const key_type& key) {
    const auto pos = lower_bound_index(key);
    if (pos == size() || !equivalent(key, keys_[pos])) {
      throw std::out_of_range("chaistl::flat_map::at");
    }
    return values_[pos];
  }

  [[nodiscard]] constexpr const mapped_type& at(const key_type& key) const {
    const auto pos = lower_bound_index(key);
    if (pos == size() || !equivalent(key, keys_[pos])) {
      throw std::out_of_range("chaistl::flat_map::at");
    }
    return values_[pos];
  }

  constexpr mapped_type& operator[](const key_type& key) { return try_emplace(key).first->second; }

  constexpr mapped_type& operator[](key_type&& key) { return try_emplace(std::move(key)).first->second; }

  [[nodiscard]] constexpr iterator begin() noexcept { return iterator(keys_.begin(), values_.begin()); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept {
    return const_iterator(keys_.begin(), values_.begin());
  }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
  [[nodiscard]] constexpr iterator end() noexcept { return iterator(keys_.end(), values_.end()); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(keys_.end(), values_.end()); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

  [[nodiscard]] constexpr bool empty() const noexcept { return keys_.empty(); }
  [[nodiscard]] constexpr size_type size() const noexcept { return keys_.size(); }
  [[nodiscard]] constexpr size_type max_size() const noexcept { return std::min(keys_.max_size(), values_.max_size()); }

  constexpr void clear() noexcept {
    keys_.clear();
    values_.clear();
  }

  template <class... Args>
  constexpr std::pair<iterator, bool> emplace(Args&&... args) {
    value_type value(std::forward<Args>(args)...);
    return insert(std::move(value));
  }

  template <class... Args>
  constexpr iterator emplace_hint(const_iterator hint, Args&&... args) {
    value_type value(std::forward<Args>(args)...);
    return insert(hint, std::move(value));
  }

  constexpr std::pair<iterator, bool> insert(const value_type& value) {
    return insert_value(value.first, value.second);
  }

  constexpr std::pair<iterator, bool> insert(value_type&& value) {
    return insert_value(std::move(value.first), std::move(value.second));
  }

  template <class P>
    requires std::constructible_from<value_type, P&&> && (!std::same_as<std::remove_cvref_t<P>, value_type>)
  constexpr std::pair<iterator, bool> insert(P&& value) {
    return insert(value_type(std::forward<P>(value)));
  }

  constexpr iterator insert(const_iterator hint, const value_type& value) {
    if (is_valid_insert_hint(hint, value.first)) {
      return insert_at(index_of(hint), value.first, value.second);
    }
    return insert(value).first;
  }

  constexpr iterator insert(const_iterator hint, value_type&& value) {
    if (is_valid_insert_hint(hint, value.first)) {
      return insert_at(index_of(hint), std::move(value.first), std::move(value.second));
    }
    return insert(std::move(value)).first;
  }

  template <class P>
    requires std::constructible_from<value_type, P&&> && (!std::same_as<std::remove_cvref_t<P>, value_type>)
  constexpr iterator insert(const_iterator hint, P&& value) {
    return insert(hint, value_type(std::forward<P>(value)));
  }

  template <std::input_iterator InputIt>
  constexpr void insert(InputIt first, InputIt last) {
    append_iterator_range_and_normalize(first, last, false);
  }

  template <std::input_iterator InputIt>
  constexpr void insert(sorted_unique_t, InputIt first, InputIt last) {
    append_iterator_range_and_normalize(first, last, true);
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr void insert_range(R&& range) {
    append_range_and_normalize(std::forward<R>(range), false);
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr void insert_range(sorted_unique_t, R&& range) {
    append_range_and_normalize(std::forward<R>(range), true);
  }

  constexpr void insert(std::initializer_list<value_type> init) { insert(init.begin(), init.end()); }

  constexpr void insert(sorted_unique_t tag, std::initializer_list<value_type> init) {
    insert(tag, init.begin(), init.end());
  }

  template <class... Args>
  constexpr std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
    return try_emplace_impl(key, std::forward<Args>(args)...);
  }

  template <class... Args>
  constexpr std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
    return try_emplace_impl(std::move(key), std::forward<Args>(args)...);
  }

  template <class M>
  constexpr std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj) {
    return insert_or_assign_impl(key, std::forward<M>(obj));
  }

  template <class M>
  constexpr std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) {
    return insert_or_assign_impl(std::move(key), std::forward<M>(obj));
  }

  template <class... Args>
  constexpr iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args) {
    return try_emplace_hint_impl(hint, key, std::forward<Args>(args)...);
  }

  template <class... Args>
  constexpr iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args) {
    return try_emplace_hint_impl(hint, std::move(key), std::forward<Args>(args)...);
  }

  template <class M>
  constexpr iterator insert_or_assign(const_iterator hint, const key_type& key, M&& obj) {
    return insert_or_assign_hint_impl(hint, key, std::forward<M>(obj));
  }

  template <class M>
  constexpr iterator insert_or_assign(const_iterator hint, key_type&& key, M&& obj) {
    return insert_or_assign_hint_impl(hint, std::move(key), std::forward<M>(obj));
  }

  [[nodiscard]] constexpr containers extract() && {
    struct clear_guard {
      flat_map* self;
      constexpr ~clear_guard() {
        if (self != nullptr) self->clear();
      }
    } guard{this};
    return containers{std::move(keys_), std::move(values_)};
  }

  constexpr void replace(key_container_type&& key_cont, mapped_container_type&& mapped_cont) {
    replace(containers{std::move(key_cont), std::move(mapped_cont)});
  }

  constexpr void replace(containers&& cont) {
    CHAI_HARDENED(cont.keys.size() == cont.values.size(), "chaistl::flat_map::replace: container sizes differ");
    CHAI_HARDENED(is_sorted_unique(cont.keys), "chaistl::flat_map::replace: replacement keys are invalid");
    struct clear_guard {
      flat_map* self;
      constexpr ~clear_guard() {
        if (self != nullptr) self->clear();
      }
      constexpr void complete() noexcept { self = nullptr; }
    } guard{this};
    keys_ = std::move(cont.keys);
    values_ = std::move(cont.values);
    guard.complete();
  }

  constexpr iterator erase(const_iterator pos) {
    const auto offset = index_of(pos);
    keys_.erase(keys_.begin() + static_cast<difference_type>(offset));
    const auto value_next = values_.erase(values_.begin() + static_cast<difference_type>(offset));
    return iterator(keys_.begin() + static_cast<difference_type>(offset), value_next);
  }

  constexpr iterator erase(const_iterator first, const_iterator last) {
    const auto first_offset = index_of(first);
    const auto last_offset = index_of(last);
    keys_.erase(keys_.begin() + static_cast<difference_type>(first_offset),
                keys_.begin() + static_cast<difference_type>(last_offset));
    const auto value_next = values_.erase(values_.begin() + static_cast<difference_type>(first_offset),
                                          values_.begin() + static_cast<difference_type>(last_offset));
    return iterator(keys_.begin() + static_cast<difference_type>(first_offset), value_next);
  }

  constexpr size_type erase(const key_type& key) {
    auto pos = find(key);
    if (pos == end()) return 0;
    erase(pos);
    return 1;
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  constexpr size_type erase(const K& key) {
    auto pos = find(key);
    if (pos == end()) return 0;
    erase(const_iterator(pos));
    return 1;
  }

  constexpr void swap(flat_map& other) noexcept(noexcept(keys_.swap(other.keys_)) &&
                                                noexcept(values_.swap(other.values_)) &&
                                                noexcept(std::swap(compare_, other.compare_))) {
    keys_.swap(other.keys_);
    values_.swap(other.values_);
    using std::swap;
    swap(compare_, other.compare_);
  }

  [[nodiscard]] constexpr iterator find(const key_type& key) {
    const auto pos = lower_bound_index(key);
    return pos != size() && equivalent(key, keys_[pos]) ? iterator_at(pos) : end();
  }

  [[nodiscard]] constexpr const_iterator find(const key_type& key) const {
    const auto pos = lower_bound_index(key);
    return pos != size() && equivalent(key, keys_[pos]) ? const_iterator_at(pos) : end();
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator find(const K& key) {
    const auto pos = lower_bound_index(key);
    return pos != size() && equivalent(key, keys_[pos]) ? iterator_at(pos) : end();
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator find(const K& key) const {
    const auto pos = lower_bound_index(key);
    return pos != size() && equivalent(key, keys_[pos]) ? const_iterator_at(pos) : end();
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

  [[nodiscard]] constexpr iterator lower_bound(const key_type& key) { return iterator_at(lower_bound_index(key)); }

  [[nodiscard]] constexpr const_iterator lower_bound(const key_type& key) const {
    return const_iterator_at(lower_bound_index(key));
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator lower_bound(const K& key) {
    return iterator_at(lower_bound_index(key));
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator lower_bound(const K& key) const {
    return const_iterator_at(lower_bound_index(key));
  }

  [[nodiscard]] constexpr iterator upper_bound(const key_type& key) { return iterator_at(upper_bound_index(key)); }

  [[nodiscard]] constexpr const_iterator upper_bound(const key_type& key) const {
    return const_iterator_at(upper_bound_index(key));
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator upper_bound(const K& key) {
    return iterator_at(upper_bound_index(key));
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator upper_bound(const K& key) const {
    return const_iterator_at(upper_bound_index(key));
  }

  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const key_type& key) {
    auto first = lower_bound(key);
    if (first == end() || !equivalent(key, first->first)) return {first, first};
    auto last = first;
    ++last;
    return {first, last};
  }

  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
    auto first = lower_bound(key);
    if (first == end() || !equivalent(key, first->first)) return {first, first};
    auto last = first;
    ++last;
    return {first, last};
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const K& key) {
    auto first = lower_bound(key);
    if (first == end() || !equivalent(key, first->first)) return {first, first};
    auto last = first;
    ++last;
    return {first, last};
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
    auto first = lower_bound(key);
    if (first == end() || !equivalent(key, first->first)) return {first, first};
    auto last = first;
    ++last;
    return {first, last};
  }

  [[nodiscard]] constexpr key_compare key_comp() const { return compare_; }
  [[nodiscard]] constexpr value_compare value_comp() const { return value_compare(compare_); }

  [[nodiscard]] constexpr const key_container_type& keys() const noexcept { return keys_; }
  [[nodiscard]] constexpr mapped_container_type& values() noexcept { return values_; }
  [[nodiscard]] constexpr const mapped_container_type& values() const noexcept { return values_; }

  [[nodiscard]] constexpr bool validate() const { return keys_.size() == values_.size() && is_sorted_unique(keys_); }

 private:
  template <bool IsConst>
  class basic_iterator {
    using key_iterator =
        std::conditional_t<IsConst, typename key_container_type::const_iterator, typename key_container_type::iterator>;
    using mapped_iterator = std::conditional_t<IsConst,
                                               typename mapped_container_type::const_iterator,
                                               typename mapped_container_type::iterator>;

   public:
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;
    using value_type = flat_map::value_type;
    using difference_type = flat_map::difference_type;
    using reference = std::conditional_t<IsConst, flat_map::const_reference, flat_map::reference>;
    using pointer = void;

    constexpr basic_iterator() = default;

    constexpr basic_iterator(key_iterator key_it, mapped_iterator mapped_it) : key_it_(key_it), mapped_it_(mapped_it) {}

    template <bool OtherConst>
      requires IsConst && (!OtherConst)
    constexpr basic_iterator(const basic_iterator<OtherConst>& other)
        : key_it_(other.key_it_), mapped_it_(other.mapped_it_) {}

    [[nodiscard]] constexpr reference operator*() const { return reference{*key_it_, *mapped_it_}; }

    struct arrow_proxy {
      reference value;
      [[nodiscard]] constexpr const reference* operator->() const noexcept { return &value; }
      [[nodiscard]] constexpr reference* operator->() noexcept { return &value; }
    };

    [[nodiscard]] constexpr arrow_proxy operator->() const { return arrow_proxy{operator*()}; }

    constexpr basic_iterator& operator++() {
      ++key_it_;
      ++mapped_it_;
      return *this;
    }

    constexpr basic_iterator operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr basic_iterator& operator--() {
      --key_it_;
      --mapped_it_;
      return *this;
    }

    constexpr basic_iterator operator--(int) {
      auto tmp = *this;
      --*this;
      return tmp;
    }

    constexpr basic_iterator& operator+=(difference_type n) {
      key_it_ += n;
      mapped_it_ += n;
      return *this;
    }

    constexpr basic_iterator& operator-=(difference_type n) {
      key_it_ -= n;
      mapped_it_ -= n;
      return *this;
    }

    [[nodiscard]] constexpr reference operator[](difference_type n) const { return *(*this + n); }

    friend constexpr basic_iterator operator+(basic_iterator it, difference_type n) {
      it += n;
      return it;
    }

    friend constexpr basic_iterator operator+(difference_type n, basic_iterator it) {
      it += n;
      return it;
    }

    friend constexpr basic_iterator operator-(basic_iterator it, difference_type n) {
      it -= n;
      return it;
    }

    friend constexpr difference_type operator-(const basic_iterator& lhs, const basic_iterator& rhs) {
      return lhs.key_it_ - rhs.key_it_;
    }

    friend constexpr bool operator==(const basic_iterator& lhs, const basic_iterator& rhs) {
      return lhs.key_it_ == rhs.key_it_;
    }

    friend constexpr auto operator<=>(const basic_iterator& lhs, const basic_iterator& rhs) {
      return lhs.key_it_ <=> rhs.key_it_;
    }

   private:
    friend class flat_map;
    template <bool>
    friend class basic_iterator;

    key_iterator key_it_{};
    mapped_iterator mapped_it_{};
  };

  template <class L, class R>
  [[nodiscard]] constexpr bool equivalent(const L& lhs, const R& rhs) const {
    return !compare_(lhs, rhs) && !compare_(rhs, lhs);
  }

  template <class K>
  [[nodiscard]] constexpr size_type lower_bound_index(const K& key) const {
    const auto it = std::lower_bound(keys_.begin(), keys_.end(), key, compare_);
    return static_cast<size_type>(it - keys_.begin());
  }

  template <class K>
  [[nodiscard]] constexpr size_type upper_bound_index(const K& key) const {
    const auto it = std::upper_bound(keys_.begin(), keys_.end(), key, compare_);
    return static_cast<size_type>(it - keys_.begin());
  }

  [[nodiscard]] constexpr iterator iterator_at(size_type offset) {
    return iterator(keys_.begin() + static_cast<difference_type>(offset),
                    values_.begin() + static_cast<difference_type>(offset));
  }

  [[nodiscard]] constexpr const_iterator const_iterator_at(size_type offset) const {
    return const_iterator(keys_.begin() + static_cast<difference_type>(offset),
                          values_.begin() + static_cast<difference_type>(offset));
  }

  [[nodiscard]] constexpr size_type index_of(const_iterator it) const {
    return static_cast<size_type>(it.key_it_ - keys_.begin());
  }

  template <class K>
  [[nodiscard]] constexpr bool is_valid_insert_hint(const_iterator hint, const K& key) const {
    if (hint != end() && !compare_(key, hint->first)) return false;
    if (hint != begin()) {
      auto prev = hint;
      --prev;
      if (!compare_(prev->first, key)) return false;
    }
    return true;
  }

  template <class K, class M>
  constexpr std::pair<iterator, bool> insert_value(K&& key, M&& mapped) {
    const auto pos = lower_bound_index(key);
    if (pos != size() && equivalent(key, keys_[pos])) {
      return {iterator_at(pos), false};
    }
    return {insert_at(pos, std::forward<K>(key), std::forward<M>(mapped)), true};
  }

  template <class K, class... Args>
  constexpr std::pair<iterator, bool> try_emplace_impl(K&& key, Args&&... args) {
    const auto pos = lower_bound_index(key);
    if (pos != size() && equivalent(key, keys_[pos])) {
      return {iterator_at(pos), false};
    }
    return {insert_at(pos, std::forward<K>(key), mapped_type(std::forward<Args>(args)...)), true};
  }

  template <class K, class M>
  constexpr std::pair<iterator, bool> insert_or_assign_impl(K&& key, M&& obj) {
    const auto pos = lower_bound_index(key);
    if (pos != size() && equivalent(key, keys_[pos])) {
      values_[pos] = std::forward<M>(obj);
      return {iterator_at(pos), false};
    }
    return {insert_at(pos, std::forward<K>(key), std::forward<M>(obj)), true};
  }

  template <class K, class M>
  constexpr iterator insert_at(size_type offset, K&& key, M&& mapped) {
    const auto key_pos = keys_.insert(keys_.begin() + static_cast<difference_type>(offset), std::forward<K>(key));
    try {
      const auto value_pos =
          values_.insert(values_.begin() + static_cast<difference_type>(offset), std::forward<M>(mapped));
      return iterator(key_pos, value_pos);
    } catch (...) {
      keys_.erase(key_pos);
      throw;
    }
  }

  // Standard flat containers restore the invariant on exception, possibly by
  // clearing the adaptor. This guard implements that policy for multi-step
  // mutations whose intermediate states violate the sorted-unique invariant.
  struct clear_on_failure_guard {
    flat_map* self;
    constexpr ~clear_on_failure_guard() {
      if (self != nullptr) self->clear();
    }
    constexpr void complete() noexcept { self = nullptr; }
  };

  // Rolls a failed bulk insertion back to the pre-insert state by erasing the
  // appended tail. Falls back to clearing if the containers are no longer in a
  // recognizable shape (for example after an inner clear).
  struct tail_rollback_guard {
    flat_map* self;
    size_type old_size;
    constexpr ~tail_rollback_guard() {
      if (self == nullptr) return;
      if (self->keys_.size() >= old_size && self->keys_.size() == self->values_.size()) {
        self->erase_suffix(old_size);
      } else {
        self->clear();
      }
    }
    constexpr void complete() noexcept { self = nullptr; }
  };

  template <class Container>
  [[nodiscard]] static constexpr Container make_empty_with_allocator_of(const Container& source) {
    if constexpr (requires { Container(source.get_allocator()); }) {
      return Container(source.get_allocator());
    } else {
      return Container();
    }
  }

  template <class K, class... Args>
  constexpr iterator try_emplace_hint_impl(const_iterator hint, K&& key, Args&&... args) {
    if (is_valid_insert_hint(hint, key)) {
      return insert_at(index_of(hint), std::forward<K>(key), mapped_type(std::forward<Args>(args)...));
    }
    return try_emplace_impl(std::forward<K>(key), std::forward<Args>(args)...).first;
  }

  template <class K, class M>
  constexpr iterator insert_or_assign_hint_impl(const_iterator hint, K&& key, M&& obj) {
    if (is_valid_insert_hint(hint, key)) {
      return insert_at(index_of(hint), std::forward<K>(key), std::forward<M>(obj));
    }
    return insert_or_assign_impl(std::forward<K>(key), std::forward<M>(obj)).first;
  }

  template <class InputIt>
  constexpr void append_iterator_range_and_normalize(InputIt first, InputIt last, bool tail_is_sorted_unique) {
    const auto old_size = size();
    tail_rollback_guard guard{this, old_size};
    for (; first != last; ++first) {
      value_type value(*first);
      append(std::move(value.first), std::move(value.second));
    }
    normalize_appended_tail(old_size, tail_is_sorted_unique);
    guard.complete();
  }

  template <class R>
  constexpr void append_range_and_normalize(R&& range, bool tail_is_sorted_unique) {
    const auto old_size = size();
    tail_rollback_guard guard{this, old_size};
    for (auto&& element : range) {
      value_type value(std::forward<decltype(element)>(element));
      append(std::move(value.first), std::move(value.second));
    }
    normalize_appended_tail(old_size, tail_is_sorted_unique);
    guard.complete();
  }

  constexpr void erase_suffix(size_type new_size) {
    keys_.erase(keys_.begin() + static_cast<difference_type>(new_size), keys_.end());
    values_.erase(values_.begin() + static_cast<difference_type>(new_size), values_.end());
  }

  // Normalizes the appended tail [old_size, size()) against the sorted prefix.
  //
  // Phase 1 establishes a stable sorted-unique order over tail *indices*
  // without touching any element, so a comparator throw here lets the caller
  // roll back to the pre-insert state. Phase 2 gather-merges prefix and tail
  // into fresh containers (each element moved exactly once) and commits by
  // move; a throw there restores the invariant by clearing, which is the
  // standard flat container policy. For duplicate keys the pre-existing
  // mapped value wins; within the tail the first occurrence wins.
  //
  // An inplace_merge over a zip view of both containers would be the
  // libstdc++/libc++ form, but libstdc++ 14 still dispatches stable_sort and
  // inplace_merge on iterator_category, which rejects proxy iterators. The
  // index-permutation gather is the portable equivalent.
  constexpr void normalize_appended_tail(size_type old_size, bool tail_is_sorted_unique) {
    if (old_size == keys_.size()) return;
    const size_type tail_size = keys_.size() - old_size;

    chaistl::vector<size_type> order(tail_size);
    for (size_type i = 0; i < tail_size; ++i) {
      order[i] = old_size + i;
    }
    size_type unique_count = tail_size;
    if (!tail_is_sorted_unique) {
      std::sort(order.begin(), order.end(), [this](size_type lhs, size_type rhs) {
        if (compare_(keys_[lhs], keys_[rhs])) return true;
        if (compare_(keys_[rhs], keys_[lhs])) return false;
        return lhs < rhs;  // stable tie-break: the first occurrence of a key wins
      });
      unique_count = 0;
      for (size_type i = 0; i < tail_size; ++i) {
        if (unique_count == 0 || compare_(keys_[order[unique_count - 1]], keys_[order[i]])) {
          order[unique_count++] = order[i];
        }
      }
    } else {
      CHAI_HARDENED(is_sorted_unique(keys_.begin() + static_cast<difference_type>(old_size), keys_.end()),
                    "chaistl::flat_map: sorted_unique insertion is invalid");
    }

    auto merged_keys = make_empty_with_allocator_of(keys_);
    auto merged_values = make_empty_with_allocator_of(values_);
    if constexpr (requires { merged_keys.reserve(size_type{}); }) {
      merged_keys.reserve(old_size + unique_count);
    }
    if constexpr (requires { merged_values.reserve(size_type{}); }) {
      merged_values.reserve(old_size + unique_count);
    }

    clear_on_failure_guard guard{this};
    const auto move_entry_at = [&](size_type index) {
      merged_keys.insert(merged_keys.end(), std::move(keys_[index]));
      merged_values.insert(merged_values.end(), std::move(values_[index]));
    };
    size_type prefix = 0;
    size_type tail = 0;
    while (prefix < old_size && tail < unique_count) {
      if (compare_(keys_[prefix], keys_[order[tail]])) {
        move_entry_at(prefix);
        ++prefix;
      } else if (compare_(keys_[order[tail]], keys_[prefix])) {
        move_entry_at(order[tail]);
        ++tail;
      } else {
        move_entry_at(prefix);  // existing mapped value wins over the incoming duplicate
        ++prefix;
        ++tail;
      }
    }
    for (; prefix < old_size; ++prefix) {
      move_entry_at(prefix);
    }
    for (; tail < unique_count; ++tail) {
      move_entry_at(order[tail]);
    }

    keys_ = std::move(merged_keys);
    values_ = std::move(merged_values);
    guard.complete();
  }

  constexpr void sort_and_unique() { normalize_appended_tail(0, false); }

  template <class K, class M>
  constexpr void append(K&& key, M&& mapped) {
    keys_.insert(keys_.end(), std::forward<K>(key));
    try {
      values_.insert(values_.end(), std::forward<M>(mapped));
    } catch (...) {
      keys_.erase(std::prev(keys_.end()));
      throw;
    }
  }

  [[nodiscard]] constexpr bool is_sorted_unique(const key_container_type& keys) const {
    return is_sorted_unique(keys.begin(), keys.end());
  }

  template <class Iter>
  [[nodiscard]] constexpr bool is_sorted_unique(Iter first, Iter last) const {
    if (first == last) return true;
    auto prev = first;
    for (auto it = std::next(first); it != last; ++it, ++prev) {
      if (!compare_(*prev, *it)) return false;
    }
    return true;
  }

  key_container_type keys_{};
  mapped_container_type values_{};
  [[no_unique_address]] key_compare compare_{};
};

template <class KeyContainer, class MappedContainer, class Compare = std::less<typename KeyContainer::value_type>>
flat_map(KeyContainer, MappedContainer, Compare = Compare()) -> flat_map<typename KeyContainer::value_type,
                                                                         typename MappedContainer::value_type,
                                                                         Compare,
                                                                         KeyContainer,
                                                                         MappedContainer>;

template <class KeyContainer, class MappedContainer, class Compare = std::less<typename KeyContainer::value_type>>
flat_map(sorted_unique_t, KeyContainer, MappedContainer, Compare = Compare())
    -> flat_map<typename KeyContainer::value_type,
                typename MappedContainer::value_type,
                Compare,
                KeyContainer,
                MappedContainer>;

template <std::input_iterator InputIt, class Compare = std::less<detail::iter_key_t<InputIt>>>
flat_map(InputIt, InputIt, Compare = Compare())
    -> flat_map<detail::iter_key_t<InputIt>, detail::iter_mapped_t<InputIt>, Compare>;

template <std::input_iterator InputIt, class Compare = std::less<detail::iter_key_t<InputIt>>>
flat_map(sorted_unique_t, InputIt, InputIt, Compare = Compare())
    -> flat_map<detail::iter_key_t<InputIt>, detail::iter_mapped_t<InputIt>, Compare>;

template <std::ranges::input_range R, class Compare = std::less<detail::range_key_t<R>>>
flat_map(std::from_range_t, R&&, Compare = Compare())
    -> flat_map<detail::range_key_t<R>, detail::range_mapped_t<R>, Compare>;

template <class Key, class T, class Compare = std::less<Key>>
flat_map(std::initializer_list<std::pair<Key, T>>, Compare = Compare()) -> flat_map<Key, T, Compare>;

template <class Key, class T, class Compare = std::less<Key>>
flat_map(sorted_unique_t, std::initializer_list<std::pair<Key, T>>, Compare = Compare()) -> flat_map<Key, T, Compare>;

template <class Key, class T, class Compare, class KeyContainer, class MappedContainer>
[[nodiscard]] constexpr bool operator==(const flat_map<Key, T, Compare, KeyContainer, MappedContainer>& lhs,
                                        const flat_map<Key, T, Compare, KeyContainer, MappedContainer>& rhs) {
  return std::ranges::equal(lhs, rhs, [](const auto& l, const auto& r) {
    return l.first == r.first && l.second == r.second;
  });
}

template <class Key, class T, class Compare, class KeyContainer, class MappedContainer>
[[nodiscard]] constexpr auto operator<=>(const flat_map<Key, T, Compare, KeyContainer, MappedContainer>& lhs,
                                         const flat_map<Key, T, Compare, KeyContainer, MappedContainer>& rhs) {
  return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <class Key, class T, class Compare, class KeyContainer, class MappedContainer>
constexpr void swap(flat_map<Key, T, Compare, KeyContainer, MappedContainer>& lhs,
                    flat_map<Key, T, Compare, KeyContainer, MappedContainer>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

template <class Key, class T, class Compare, class KeyContainer, class MappedContainer, class Predicate>
constexpr typename flat_map<Key, T, Compare, KeyContainer, MappedContainer>::size_type erase_if(
    flat_map<Key, T, Compare, KeyContainer, MappedContainer>& map, Predicate pred) {
  typename flat_map<Key, T, Compare, KeyContainer, MappedContainer>::size_type removed = 0;
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

namespace pmr {

template <class Key, class T, class Compare = std::less<Key>>
using flat_map = chaistl::flat_map<Key, T, Compare, chaistl::pmr::vector<Key>, chaistl::pmr::vector<T>>;

}  // namespace pmr

}  // namespace chaistl
