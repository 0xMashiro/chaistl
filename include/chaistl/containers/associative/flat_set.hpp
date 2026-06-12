// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// flat_set - Sorted unique-key associative adaptor over contiguous storage
// ============================================================================
//
// Architecture:
//   - Stores keys in one sorted random-access container, defaulting to
//     chaistl::vector<Key>.
//   - Lookup is binary search; insertion preserves sorted uniqueness by moving
//     elements inside the underlying sequence.
//   - Iterators are constant iterators because keys are the stored values and
//     mutating a key would break the sorted invariant.
//
// Standardization archaeology:
//   - Flat associative containers entered the standard library in C++23 from
//     the long-running P0429/P1222 proposal line.
//   - The design gives up node stability and logarithmic insertion in exchange
//     for compact storage, cache-friendly traversal, and fast lookup on small
//     to medium data sets.
//   - The sorted_unique construction tag exposes a common optimization: if the
//     caller can prove the input is already sorted and unique, construction can
//     skip sorting and deduplication.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/flat.set
//   - cppreference: https://en.cppreference.com/w/cpp/container/flat_set
//   - P0429: https://wg21.link/P0429
//   - P1222: https://wg21.link/P1222
//   - Boost.Container flat_set: https://www.boost.org/doc/libs/release/doc/html/container/non_standard_containers.html

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
#include <type_traits>
#include <utility>

namespace chaistl {

/**
 * @brief Sorted unique-key associative container adaptor over a random-access container.
 *
 * @ingroup containers_associative
 *
 * `flat_set` provides set-like lookup over a sorted underlying sequence
 * container. The default storage is `chaistl::vector<Key>`.
 *
 * References:
 * - C++23 `std::flat_set`: https://en.cppreference.com/w/cpp/container/flat_set
 * - libstdc++ and libc++ `<flat_set>` implementations
 * - Boost.Container `flat_set` and EASTL `vector_set`
 */
template <class Key, class Compare = std::less<Key>, class KeyContainer = chaistl::vector<Key>>
  requires std::same_as<Key, typename KeyContainer::value_type> &&
           std::random_access_iterator<typename KeyContainer::iterator> &&
           std::random_access_iterator<typename KeyContainer::const_iterator>
class flat_set {
 public:
  using key_type = Key;
  using value_type = Key;
  using key_compare = Compare;
  using value_compare = Compare;
  using reference = value_type&;
  using const_reference = const value_type&;
  using size_type = typename KeyContainer::size_type;
  using difference_type = typename KeyContainer::difference_type;
  using iterator = typename KeyContainer::const_iterator;
  using const_iterator = iterator;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;
  using container_type = KeyContainer;

  constexpr flat_set() = default;

  explicit constexpr flat_set(const key_compare& comp) : compare_(comp) {}

  explicit constexpr flat_set(container_type cont, const key_compare& comp = key_compare{})
      : keys_(std::move(cont)), compare_(comp) {
    sort_and_unique();
  }

  constexpr flat_set(sorted_unique_t, container_type cont, const key_compare& comp = key_compare{})
      : keys_(std::move(cont)), compare_(comp) {
    CHAI_HARDENED(is_sorted_unique(), "chaistl::flat_set: sorted_unique input is invalid");
  }

  template <std::input_iterator InputIt>
  constexpr flat_set(InputIt first, InputIt last, const key_compare& comp = key_compare{}) : compare_(comp) {
    insert(first, last);
  }

  template <std::input_iterator InputIt>
  constexpr flat_set(sorted_unique_t, InputIt first, InputIt last, const key_compare& comp = key_compare{})
      : keys_(first, last), compare_(comp) {
    CHAI_HARDENED(is_sorted_unique(), "chaistl::flat_set: sorted_unique input is invalid");
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr flat_set(std::from_range_t, R&& range, const key_compare& comp = key_compare{}) : compare_(comp) {
    insert_range(std::forward<R>(range));
  }

  constexpr flat_set(std::initializer_list<value_type> init, const key_compare& comp = key_compare{})
      : flat_set(init.begin(), init.end(), comp) {}

  constexpr flat_set(sorted_unique_t tag,
                     std::initializer_list<value_type> init,
                     const key_compare& comp = key_compare{})
      : flat_set(tag, init.begin(), init.end(), comp) {}

  // Allocator-extended constructors. The allocator is forwarded to the
  // underlying container through uses-allocator construction.

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  explicit constexpr flat_set(const Alloc& alloc) : flat_set(key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(const key_compare& comp, const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<container_type>(alloc)), compare_(comp) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(const container_type& cont, const key_compare& comp, const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<container_type>(alloc, cont)), compare_(comp) {
    sort_and_unique();
  }

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(const container_type& cont, const Alloc& alloc) : flat_set(cont, key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(sorted_unique_t, const container_type& cont, const key_compare& comp, const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<container_type>(alloc, cont)), compare_(comp) {
    CHAI_HARDENED(is_sorted_unique(), "chaistl::flat_set: sorted_unique input is invalid");
  }

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(sorted_unique_t tag, const container_type& cont, const Alloc& alloc)
      : flat_set(tag, cont, key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(const flat_set& other, const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<container_type>(alloc, other.keys_)), compare_(other.compare_) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(flat_set&& other, const Alloc& alloc)
      : keys_(std::make_obj_using_allocator<container_type>(alloc, std::move(other.keys_))),
        compare_(std::move(other.compare_)) {
    other.clear();
  }

  template <std::input_iterator InputIt, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(InputIt first, InputIt last, const key_compare& comp, const Alloc& alloc) : flat_set(comp, alloc) {
    insert(first, last);
  }

  template <std::input_iterator InputIt, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(InputIt first, InputIt last, const Alloc& alloc) : flat_set(first, last, key_compare{}, alloc) {}

  template <std::input_iterator InputIt, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(sorted_unique_t tag, InputIt first, InputIt last, const key_compare& comp, const Alloc& alloc)
      : flat_set(comp, alloc) {
    insert(tag, first, last);
  }

  template <std::input_iterator InputIt, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(sorted_unique_t tag, InputIt first, InputIt last, const Alloc& alloc)
      : flat_set(tag, first, last, key_compare{}, alloc) {}

  template <concepts::container_compatible_range<value_type> R, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(std::from_range_t, R&& range, const key_compare& comp, const Alloc& alloc)
      : flat_set(comp, alloc) {
    insert_range(std::forward<R>(range));
  }

  template <concepts::container_compatible_range<value_type> R, class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(std::from_range_t tag, R&& range, const Alloc& alloc)
      : flat_set(tag, std::forward<R>(range), key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(std::initializer_list<value_type> init, const key_compare& comp, const Alloc& alloc)
      : flat_set(init.begin(), init.end(), comp, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(std::initializer_list<value_type> init, const Alloc& alloc)
      : flat_set(init, key_compare{}, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(sorted_unique_t tag,
                     std::initializer_list<value_type> init,
                     const key_compare& comp,
                     const Alloc& alloc)
      : flat_set(tag, init.begin(), init.end(), comp, alloc) {}

  template <class Alloc>
    requires detail::flat_uses_allocator<Alloc, KeyContainer>
  constexpr flat_set(sorted_unique_t tag, std::initializer_list<value_type> init, const Alloc& alloc)
      : flat_set(tag, init, key_compare{}, alloc) {}

  constexpr flat_set& operator=(std::initializer_list<value_type> init) {
    clear();
    insert(init.begin(), init.end());
    return *this;
  }

  [[nodiscard]] constexpr iterator begin() noexcept { return keys_.begin(); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return keys_.begin(); }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
  [[nodiscard]] constexpr iterator end() noexcept { return keys_.end(); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return keys_.end(); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

  [[nodiscard]] constexpr bool empty() const noexcept { return keys_.empty(); }
  [[nodiscard]] constexpr size_type size() const noexcept { return keys_.size(); }
  [[nodiscard]] constexpr size_type max_size() const noexcept { return keys_.max_size(); }

  constexpr void clear() noexcept { keys_.clear(); }

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
    auto pos = lower_bound(value);
    if (pos != end() && equivalent(value, *pos)) {
      return {pos, false};
    }
    return {keys_.insert(pos, value), true};
  }

  constexpr std::pair<iterator, bool> insert(value_type&& value) {
    auto pos = lower_bound(value);
    if (pos != end() && equivalent(value, *pos)) {
      return {pos, false};
    }
    return {keys_.insert(pos, std::move(value)), true};
  }

  template <class K>
    requires concepts::transparent_compare<Compare> && std::constructible_from<value_type, K&&> &&
             (!std::same_as<std::remove_cvref_t<K>, value_type>)
  constexpr std::pair<iterator, bool> insert(K&& value) {
    auto pos = lower_bound(value);
    if (pos != end() && equivalent(value, *pos)) {
      return {pos, false};
    }
    return {keys_.insert(pos, value_type(std::forward<K>(value))), true};
  }

  constexpr iterator insert(const_iterator hint, const value_type& value) {
    if (is_valid_insert_hint(hint, value)) {
      return keys_.insert(hint, value);
    }
    return insert(value).first;
  }

  constexpr iterator insert(const_iterator hint, value_type&& value) {
    if (is_valid_insert_hint(hint, value)) {
      return keys_.insert(hint, std::move(value));
    }
    return insert(std::move(value)).first;
  }

  template <class K>
    requires concepts::transparent_compare<Compare> && std::constructible_from<value_type, K&&> &&
             (!std::same_as<std::remove_cvref_t<K>, value_type>)
  constexpr iterator insert(const_iterator hint, K&& value) {
    if (is_valid_insert_hint(hint, value)) {
      return keys_.insert(hint, value_type(std::forward<K>(value)));
    }
    return insert(std::forward<K>(value)).first;
  }

  template <std::input_iterator InputIt>
  constexpr void insert(InputIt first, InputIt last) {
    const auto old_size = keys_.size();
    keys_.insert(keys_.end(), first, last);
    normalize_appended_unique_tail(old_size, false);
  }

  template <std::input_iterator InputIt>
  constexpr void insert(sorted_unique_t, InputIt first, InputIt last) {
    const auto old_size = keys_.size();
    keys_.insert(keys_.end(), first, last);
    normalize_appended_unique_tail(old_size, true);
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr void insert_range(R&& range) {
    const auto old_size = keys_.size();
    keys_.insert_range(keys_.end(), std::forward<R>(range));
    normalize_appended_unique_tail(old_size, false);
  }

  template <concepts::container_compatible_range<value_type> R>
  constexpr void insert_range(sorted_unique_t, R&& range) {
    const auto old_size = keys_.size();
    keys_.insert_range(keys_.end(), std::forward<R>(range));
    normalize_appended_unique_tail(old_size, true);
  }

  constexpr void insert(std::initializer_list<value_type> init) { insert(init.begin(), init.end()); }

  constexpr void insert(sorted_unique_t tag, std::initializer_list<value_type> init) {
    insert(tag, init.begin(), init.end());
  }

  [[nodiscard]] constexpr container_type extract() && {
    struct clear_guard {
      flat_set* self;
      constexpr ~clear_guard() {
        if (self != nullptr) self->clear();
      }
    } guard{this};
    return std::move(keys_);
  }

  constexpr void replace(container_type&& cont) {
    CHAI_HARDENED(is_sorted_unique(cont), "chaistl::flat_set::replace: replacement is invalid");
    struct clear_guard {
      flat_set* self;
      constexpr ~clear_guard() {
        if (self != nullptr) self->clear();
      }
      constexpr void complete() noexcept { self = nullptr; }
    } guard{this};
    keys_ = std::move(cont);
    guard.complete();
  }

  constexpr iterator erase(const_iterator pos) { return keys_.erase(pos); }

  constexpr iterator erase(const_iterator first, const_iterator last) { return keys_.erase(first, last); }

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
    erase(pos);
    return 1;
  }

  constexpr void swap(flat_set& other) noexcept(noexcept(keys_.swap(other.keys_)) &&
                                                noexcept(std::swap(compare_, other.compare_))) {
    keys_.swap(other.keys_);
    using std::swap;
    swap(compare_, other.compare_);
  }

  [[nodiscard]] constexpr iterator find(const key_type& key) {
    auto pos = lower_bound(key);
    return pos != end() && equivalent(key, *pos) ? pos : end();
  }

  [[nodiscard]] constexpr const_iterator find(const key_type& key) const {
    auto pos = lower_bound(key);
    return pos != end() && equivalent(key, *pos) ? pos : end();
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator find(const K& key) {
    auto pos = lower_bound(key);
    return pos != end() && equivalent(key, *pos) ? pos : end();
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator find(const K& key) const {
    auto pos = lower_bound(key);
    return pos != end() && equivalent(key, *pos) ? pos : end();
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

  [[nodiscard]] constexpr iterator lower_bound(const key_type& key) {
    return std::lower_bound(begin(), end(), key, compare_);
  }

  [[nodiscard]] constexpr const_iterator lower_bound(const key_type& key) const {
    return std::lower_bound(begin(), end(), key, compare_);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator lower_bound(const K& key) {
    return std::lower_bound(begin(), end(), key, compare_);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator lower_bound(const K& key) const {
    return std::lower_bound(begin(), end(), key, compare_);
  }

  [[nodiscard]] constexpr iterator upper_bound(const key_type& key) {
    return std::upper_bound(begin(), end(), key, compare_);
  }

  [[nodiscard]] constexpr const_iterator upper_bound(const key_type& key) const {
    return std::upper_bound(begin(), end(), key, compare_);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr iterator upper_bound(const K& key) {
    return std::upper_bound(begin(), end(), key, compare_);
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr const_iterator upper_bound(const K& key) const {
    return std::upper_bound(begin(), end(), key, compare_);
  }

  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const key_type& key) {
    auto first = lower_bound(key);
    if (first == end() || !equivalent(key, *first)) return {first, first};
    auto last = first;
    ++last;
    return {first, last};
  }

  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
    auto first = lower_bound(key);
    if (first == end() || !equivalent(key, *first)) return {first, first};
    auto last = first;
    ++last;
    return {first, last};
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const K& key) {
    auto first = lower_bound(key);
    if (first == end() || !equivalent(key, *first)) return {first, first};
    auto last = first;
    ++last;
    return {first, last};
  }

  template <class K>
    requires concepts::transparent_compare<Compare>
  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
    auto first = lower_bound(key);
    if (first == end() || !equivalent(key, *first)) return {first, first};
    auto last = first;
    ++last;
    return {first, last};
  }

  [[nodiscard]] constexpr key_compare key_comp() const { return compare_; }
  [[nodiscard]] constexpr value_compare value_comp() const { return compare_; }

  [[nodiscard]] constexpr const container_type& keys() const noexcept { return keys_; }

  [[nodiscard]] constexpr bool validate() const { return is_sorted_unique(); }

 private:
  template <class L, class R>
  [[nodiscard]] constexpr bool equivalent(const L& lhs, const R& rhs) const {
    return !compare_(lhs, rhs) && !compare_(rhs, lhs);
  }

  template <class K>
  [[nodiscard]] constexpr bool is_valid_insert_hint(const_iterator hint, const K& value) const {
    if (hint != end() && !compare_(value, *hint)) return false;
    if (hint != begin()) {
      auto prev = hint;
      --prev;
      if (!compare_(*prev, value)) return false;
    }
    return true;
  }

  // Standard flat containers restore the invariant on exception, possibly by
  // clearing the adaptor. This guard implements that policy for multi-step
  // mutations whose intermediate states violate the sorted-unique invariant.
  struct clear_on_failure_guard {
    flat_set* self;
    constexpr ~clear_on_failure_guard() {
      if (self != nullptr) self->clear();
    }
    constexpr void complete() noexcept { self = nullptr; }
  };

  // Rolls a failed bulk insertion back to the pre-insert state by erasing the
  // appended tail. Falls back to clearing if the container is no longer in a
  // recognizable shape (for example after an inner clear).
  struct tail_rollback_guard {
    flat_set* self;
    size_type old_size;
    constexpr ~tail_rollback_guard() {
      if (self == nullptr) return;
      if (self->keys_.size() >= old_size) {
        self->keys_.erase(self->keys_.begin() + static_cast<difference_type>(old_size), self->keys_.end());
      } else {
        self->clear();
      }
    }
    constexpr void complete() noexcept { self = nullptr; }
  };

  constexpr void sort_and_unique() {
    std::ranges::sort(keys_, compare_);
    unique_erase();
  }

  constexpr void normalize_appended_unique_tail(size_type old_size, bool tail_is_sorted_unique) {
    if (old_size == keys_.size()) return;

    if (!tail_is_sorted_unique) {
      // Only tail elements are touched here, so a throw rolls back to the
      // pre-insert state by erasing the tail.
      tail_rollback_guard guard{this, old_size};
      const auto middle = keys_.begin() + static_cast<difference_type>(old_size);
      std::ranges::sort(middle, keys_.end(), compare_);
      unique_erase(middle, keys_.end());
      guard.complete();
    } else {
      CHAI_HARDENED(is_sorted_unique(keys_.begin() + static_cast<difference_type>(old_size), keys_.end()),
                    "chaistl::flat_set: sorted_unique insertion is invalid");
    }

    // Gather-merge the sorted prefix and the normalized tail into a fresh
    // container and commit by move; each element is moved exactly once and,
    // unlike std::inplace_merge, the loop is constexpr-friendly before C++26.
    // For duplicate keys the pre-existing element wins. On a throw the
    // invariant is restored by clearing, the standard flat container policy.
    clear_on_failure_guard guard{this};
    auto merged = make_empty_with_allocator_of(keys_);
    if constexpr (requires { merged.reserve(size_type{}); }) {
      merged.reserve(keys_.size());
    }
    const size_type total = keys_.size();
    size_type prefix = 0;
    size_type tail = old_size;
    while (prefix < old_size && tail < total) {
      if (compare_(keys_[prefix], keys_[tail])) {
        merged.insert(merged.end(), std::move(keys_[prefix]));
        ++prefix;
      } else if (compare_(keys_[tail], keys_[prefix])) {
        merged.insert(merged.end(), std::move(keys_[tail]));
        ++tail;
      } else {
        merged.insert(merged.end(), std::move(keys_[prefix]));  // existing element wins
        ++prefix;
        ++tail;
      }
    }
    for (; prefix < old_size; ++prefix) {
      merged.insert(merged.end(), std::move(keys_[prefix]));
    }
    for (; tail < total; ++tail) {
      merged.insert(merged.end(), std::move(keys_[tail]));
    }
    keys_ = std::move(merged);
    guard.complete();
  }

  template <class Container>
  [[nodiscard]] static constexpr Container make_empty_with_allocator_of(const Container& source) {
    if constexpr (requires { Container(source.get_allocator()); }) {
      return Container(source.get_allocator());
    } else {
      return Container();
    }
  }

  constexpr void unique_erase() { unique_erase(keys_.begin(), keys_.end()); }

  constexpr void unique_erase(typename container_type::iterator first, typename container_type::iterator last) {
    const auto new_last = std::unique(first, last, [this](const auto& lhs, const auto& rhs) {
      return this->equivalent(lhs, rhs);
    });
    keys_.erase(new_last, last);
  }

  [[nodiscard]] constexpr bool is_sorted_unique() const { return is_sorted_unique(keys_); }

  [[nodiscard]] constexpr bool is_sorted_unique(const container_type& cont) const {
    return is_sorted_unique(cont.begin(), cont.end());
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

  container_type keys_{};
  [[no_unique_address]] key_compare compare_{};
};

template <class KeyContainer, class Compare = std::less<typename KeyContainer::value_type>>
flat_set(KeyContainer, Compare = Compare()) -> flat_set<typename KeyContainer::value_type, Compare, KeyContainer>;

template <class KeyContainer, class Compare = std::less<typename KeyContainer::value_type>>
flat_set(sorted_unique_t, KeyContainer, Compare = Compare())
    -> flat_set<typename KeyContainer::value_type, Compare, KeyContainer>;

template <std::input_iterator InputIt, class Compare = std::less<std::iter_value_t<InputIt>>>
flat_set(InputIt, InputIt, Compare = Compare()) -> flat_set<std::iter_value_t<InputIt>, Compare>;

template <std::input_iterator InputIt, class Compare = std::less<std::iter_value_t<InputIt>>>
flat_set(sorted_unique_t, InputIt, InputIt, Compare = Compare()) -> flat_set<std::iter_value_t<InputIt>, Compare>;

template <std::ranges::input_range R, class Compare = std::less<std::ranges::range_value_t<R>>>
flat_set(std::from_range_t, R&&, Compare = Compare()) -> flat_set<std::ranges::range_value_t<R>, Compare>;

template <class Key, class Compare = std::less<Key>>
flat_set(std::initializer_list<Key>, Compare = Compare()) -> flat_set<Key, Compare>;

template <class Key, class Compare = std::less<Key>>
flat_set(sorted_unique_t, std::initializer_list<Key>, Compare = Compare()) -> flat_set<Key, Compare>;

template <class Key, class Compare, class KeyContainer>
[[nodiscard]] constexpr bool operator==(const flat_set<Key, Compare, KeyContainer>& lhs,
                                        const flat_set<Key, Compare, KeyContainer>& rhs) {
  return std::ranges::equal(lhs, rhs);
}

template <class Key, class Compare, class KeyContainer>
[[nodiscard]] constexpr auto operator<=>(const flat_set<Key, Compare, KeyContainer>& lhs,
                                         const flat_set<Key, Compare, KeyContainer>& rhs) {
  return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <class Key, class Compare, class KeyContainer>
constexpr void swap(flat_set<Key, Compare, KeyContainer>& lhs,
                    flat_set<Key, Compare, KeyContainer>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

template <class Key, class Compare, class KeyContainer, class Predicate>
constexpr typename flat_set<Key, Compare, KeyContainer>::size_type erase_if(flat_set<Key, Compare, KeyContainer>& set,
                                                                            Predicate pred) {
  typename flat_set<Key, Compare, KeyContainer>::size_type removed = 0;
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

}  // namespace chaistl
