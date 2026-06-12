// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// skip_list_multiset - Probabilistically balanced ordered bag
// ============================================================================
//
// Multi-key companion to skip_list_set. Equivalent keys are inserted after the
// existing equivalent run, matching the usual multiset insertion-order rule for
// equivalent elements while retaining expected logarithmic search/update.

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/memory/allocator.hpp>
#include <chaistl/meta/type_traits.hpp>

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
  requires concepts::container_element<Key> && concepts::allocator_for<Allocator, Key> &&
           std::strict_weak_order<Compare, const Key&, const Key&>
class skip_list_multiset {
  static constexpr std::size_t max_level = 32;

  struct node {
    Key value;
    node** next;
    std::size_t height;

    template <class... Args>
    explicit node(node** next_links, std::size_t node_height, Args&&... args)
        : value(std::forward<Args>(args)...), next(next_links), height(node_height) {}
  };

  using node_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<node>;
  using node_traits = std::allocator_traits<node_allocator>;
  using link_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<node*>;
  using link_traits = std::allocator_traits<link_allocator>;

 public:
  using key_type = Key;
  using value_type = Key;
  using key_compare = Compare;
  using value_compare = Compare;
  using allocator_type = Allocator;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = const value_type&;
  using const_reference = const value_type&;
  using pointer = const value_type*;
  using const_pointer = const value_type*;

  class const_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    using value_type = Key;
    using difference_type = std::ptrdiff_t;
    using reference = const value_type&;
    using pointer = const value_type*;

    constexpr const_iterator() = default;

    [[nodiscard]] constexpr reference operator*() const noexcept { return current_->value; }
    [[nodiscard]] constexpr pointer operator->() const noexcept { return std::addressof(current_->value); }

    constexpr const_iterator& operator++() noexcept {
      current_ = current_->next[0];
      return *this;
    }

    constexpr const_iterator operator++(int) noexcept {
      auto copy = *this;
      ++*this;
      return copy;
    }

    [[nodiscard]] friend constexpr bool operator==(const const_iterator&, const const_iterator&) = default;

   private:
    friend class skip_list_multiset;

    explicit constexpr const_iterator(node* current) noexcept : current_(current) {}

    node* current_ = nullptr;
  };

  using iterator = const_iterator;

  constexpr skip_list_multiset() : skip_list_multiset(Compare{}, Allocator{}) {}

  explicit constexpr skip_list_multiset(const Compare& comp, const Allocator& alloc = Allocator{})
      : compare_(comp), alloc_(alloc) {}

  explicit constexpr skip_list_multiset(const Allocator& alloc) : skip_list_multiset(Compare{}, alloc) {}

  template <std::input_iterator InputIt>
  constexpr skip_list_multiset(InputIt first,
                               InputIt last,
                               const Compare& comp = Compare{},
                               const Allocator& alloc = Allocator{})
      : skip_list_multiset(comp, alloc) {
    insert(first, last);
  }

  template <std::input_iterator InputIt>
  constexpr skip_list_multiset(InputIt first, InputIt last, const Allocator& alloc)
      : skip_list_multiset(first, last, Compare{}, alloc) {}

  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, value_type>
  constexpr skip_list_multiset(std::from_range_t,
                               R&& range,
                               const Compare& comp = Compare{},
                               const Allocator& alloc = Allocator{})
      : skip_list_multiset(comp, alloc) {
    insert_range(std::forward<R>(range));
  }

  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, value_type>
  constexpr skip_list_multiset(std::from_range_t, R&& range, const Allocator& alloc)
      : skip_list_multiset(std::from_range, std::forward<R>(range), Compare{}, alloc) {}

  constexpr skip_list_multiset(std::initializer_list<value_type> init,
                               const Compare& comp = Compare{},
                               const Allocator& alloc = Allocator{})
      : skip_list_multiset(init.begin(), init.end(), comp, alloc) {}

  constexpr skip_list_multiset(std::initializer_list<value_type> init, const Allocator& alloc)
      : skip_list_multiset(init.begin(), init.end(), Compare{}, alloc) {}

  constexpr skip_list_multiset(const skip_list_multiset& other)
      : skip_list_multiset(other.compare_, other.get_allocator()) {
    insert(other.begin(), other.end());
  }

  constexpr skip_list_multiset(const skip_list_multiset& other, const std::type_identity_t<Allocator>& alloc)
      : skip_list_multiset(other.compare_, alloc) {
    insert(other.begin(), other.end());
  }

  constexpr skip_list_multiset(skip_list_multiset&& other) noexcept(std::is_nothrow_move_constructible_v<Compare> &&
                                                                    std::is_nothrow_move_constructible_v<node_allocator>)
      : compare_(std::move(other.compare_)),
        alloc_(std::move(other.alloc_)),
        head_(std::move(other.head_)),
        size_(std::exchange(other.size_, 0)),
        level_count_(std::exchange(other.level_count_, 1)),
        rng_state_(std::exchange(other.rng_state_, default_seed)) {
    other.head_.fill(nullptr);
  }

  constexpr skip_list_multiset(skip_list_multiset&& other, const std::type_identity_t<Allocator>& alloc)
      : skip_list_multiset(other.compare_, alloc) {
    if (storage_compatible_with(other)) {
      take_storage_from(other);
    } else {
      insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
      other.clear();
    }
  }

  constexpr ~skip_list_multiset() { clear(); }

  constexpr skip_list_multiset& operator=(const skip_list_multiset& other) {
    if (this == std::addressof(other)) {
      return *this;
    }
    if constexpr (meta::propagate_on_container_copy_assignment_v<node_allocator>) {
      skip_list_multiset copy(other, other.get_allocator());
      clear();
      compare_ = other.compare_;
      alloc_ = other.alloc_;
      take_storage_from(copy);
    } else {
      skip_list_multiset copy(other, get_allocator());
      swap(copy);
    }
    return *this;
  }

  constexpr skip_list_multiset& operator=(skip_list_multiset&& other) noexcept(
      (meta::propagate_on_container_move_assignment_v<node_allocator> ||
       meta::allocator_is_always_equal_v<node_allocator>) &&
      std::is_nothrow_move_assignable_v<Compare>) {
    if (this == std::addressof(other)) {
      return *this;
    }
    clear();
    compare_ = std::move(other.compare_);
    if constexpr (meta::propagate_on_container_move_assignment_v<node_allocator>) {
      alloc_ = std::move(other.alloc_);
      take_storage_from(other);
    } else if constexpr (meta::allocator_is_always_equal_v<node_allocator>) {
      take_storage_from(other);
    } else if (storage_compatible_with(other)) {
      take_storage_from(other);
    } else {
      insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    }
    return *this;
  }

  constexpr skip_list_multiset& operator=(std::initializer_list<value_type> init) {
    clear();
    insert(init.begin(), init.end());
    return *this;
  }

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return allocator_type(alloc_); }

  [[nodiscard]] constexpr iterator begin() noexcept { return iterator(head_[0]); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(head_[0]); }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
  [[nodiscard]] constexpr iterator end() noexcept { return iterator(nullptr); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(nullptr); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
  [[nodiscard]] constexpr size_type max_size() const noexcept { return node_traits::max_size(alloc_); }

  constexpr void clear() noexcept {
    node* current = head_[0];
    while (current != nullptr) {
      node* next = current->next[0];
      destroy_node(current);
      current = next;
    }
    head_.fill(nullptr);
    size_ = 0;
    level_count_ = 1;
  }

  constexpr iterator insert(const value_type& value) { return insert_impl(value); }
  constexpr iterator insert(value_type&& value) { return insert_impl(std::move(value)); }

  constexpr iterator insert(const_iterator, const value_type& value) { return insert(value); }
  constexpr iterator insert(const_iterator, value_type&& value) { return insert(std::move(value)); }

  template <std::input_iterator InputIt>
  constexpr void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, value_type>
  constexpr void insert_range(R&& range) {
    for (auto&& value : range) {
      insert(std::forward<decltype(value)>(value));
    }
  }

  constexpr void insert(std::initializer_list<value_type> init) { insert(init.begin(), init.end()); }

  template <class... Args>
  constexpr iterator emplace(Args&&... args) {
    value_type value(std::forward<Args>(args)...);
    return insert(std::move(value));
  }

  template <class... Args>
  constexpr iterator emplace_hint(const_iterator hint, Args&&... args) {
    (void)hint;
    return emplace(std::forward<Args>(args)...);
  }

  constexpr iterator erase(const_iterator position) {
    if (position == end()) {
      return end();
    }
    node* current = position.current_;
    auto next = position;
    ++next;
    erase_node(current);
    return next;
  }

  constexpr size_type erase(const key_type& key) {
    auto first = lower_bound(key);
    size_type removed = 0;
    while (first != end() && equivalent(*first, key)) {
      first = erase(first);
      ++removed;
    }
    return removed;
  }

  constexpr iterator erase(const_iterator first, const_iterator last) {
    while (first != last) {
      first = erase(first);
    }
    return iterator(last.current_);
  }

  [[nodiscard]] constexpr size_type count(const key_type& key) const {
    size_type total = 0;
    for (auto it = lower_bound(key); it != end() && equivalent(*it, key); ++it) {
      ++total;
    }
    return total;
  }

  [[nodiscard]] constexpr iterator find(const key_type& key) noexcept { return iterator(find_node(key)); }
  [[nodiscard]] constexpr const_iterator find(const key_type& key) const noexcept {
    return const_iterator(find_node(key));
  }

  [[nodiscard]] constexpr bool contains(const key_type& key) const noexcept { return find_node(key) != nullptr; }

  [[nodiscard]] constexpr iterator lower_bound(const key_type& key) noexcept {
    auto update = search_predecessors_lower(key);
    return iterator(next_after(update[0], 0));
  }

  [[nodiscard]] constexpr const_iterator lower_bound(const key_type& key) const noexcept {
    auto update = search_predecessors_lower(key);
    return const_iterator(next_after(update[0], 0));
  }

  [[nodiscard]] constexpr iterator upper_bound(const key_type& key) noexcept {
    auto update = search_predecessors_upper(key);
    return iterator(next_after(update[0], 0));
  }

  [[nodiscard]] constexpr const_iterator upper_bound(const key_type& key) const noexcept {
    auto update = search_predecessors_upper(key);
    return const_iterator(next_after(update[0], 0));
  }

  [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(const key_type& key) noexcept {
    return {lower_bound(key), upper_bound(key)};
  }

  [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept {
    return {lower_bound(key), upper_bound(key)};
  }

  [[nodiscard]] constexpr key_compare key_comp() const { return compare_; }
  [[nodiscard]] constexpr value_compare value_comp() const { return compare_; }

  constexpr void swap(skip_list_multiset& other) noexcept(meta::is_nothrow_container_swappable_v<node_allocator> &&
                                                          std::is_nothrow_swappable_v<Compare>) {
    using std::swap;
    swap(compare_, other.compare_);
    if constexpr (meta::propagate_on_container_swap_v<node_allocator>) {
      swap(alloc_, other.alloc_);
    }
    swap(head_, other.head_);
    swap(size_, other.size_);
    swap(level_count_, other.level_count_);
    swap(rng_state_, other.rng_state_);
  }

 private:
  static constexpr std::uint64_t default_seed = 0x9e3779b97f4a7c15ull;

  Compare compare_{};
  node_allocator alloc_{};
  std::array<node*, max_level> head_{};
  size_type size_ = 0;
  std::size_t level_count_ = 1;
  std::uint64_t rng_state_ = default_seed;

  [[nodiscard]] constexpr bool equivalent(const key_type& lhs, const key_type& rhs) const {
    return !compare_(lhs, rhs) && !compare_(rhs, lhs);
  }

  [[nodiscard]] constexpr bool storage_compatible_with(const skip_list_multiset& other) const {
    return alloc_ == other.alloc_;
  }

  constexpr void take_storage_from(skip_list_multiset& other) noexcept {
    head_ = other.head_;
    size_ = std::exchange(other.size_, 0);
    level_count_ = std::exchange(other.level_count_, 1);
    rng_state_ = std::exchange(other.rng_state_, default_seed);
    other.head_.fill(nullptr);
  }

  template <class... Args>
  [[nodiscard]] constexpr node* create_node(std::size_t height, Args&&... args) {
    link_allocator link_alloc(alloc_);
    node** links = link_traits::allocate(link_alloc, height);
    std::size_t constructed_links = 0;
    try {
      for (; constructed_links < height; ++constructed_links) {
        link_traits::construct(link_alloc, links + constructed_links, nullptr);
      }
    } catch (...) {
      for (std::size_t i = 0; i < constructed_links; ++i) {
        link_traits::destroy(link_alloc, links + i);
      }
      link_traits::deallocate(link_alloc, links, height);
      throw;
    }

    node* allocated = node_traits::allocate(alloc_, 1);
    try {
      node_traits::construct(alloc_, allocated, links, height, std::forward<Args>(args)...);
    } catch (...) {
      node_traits::deallocate(alloc_, allocated, 1);
      for (std::size_t i = 0; i < height; ++i) {
        link_traits::destroy(link_alloc, links + i);
      }
      link_traits::deallocate(link_alloc, links, height);
      throw;
    }
    return allocated;
  }

  constexpr void destroy_node(node* current) noexcept {
    link_allocator link_alloc(alloc_);
    node** links = current->next;
    const std::size_t height = current->height;
    node_traits::destroy(alloc_, current);
    node_traits::deallocate(alloc_, current, 1);
    for (std::size_t i = 0; i < height; ++i) {
      link_traits::destroy(link_alloc, links + i);
    }
    link_traits::deallocate(link_alloc, links, height);
  }

  [[nodiscard]] constexpr node* next_after(node* current, std::size_t level) const noexcept {
    return current == nullptr ? head_[level] : current->next[level];
  }

  constexpr void set_next_after(node* current, std::size_t level, node* next) noexcept {
    if (current == nullptr) {
      head_[level] = next;
    } else {
      current->next[level] = next;
    }
  }

  [[nodiscard]] constexpr node* predecessor_lower_at_level(node* start, std::size_t level, const key_type& key) const {
    node* current = start;
    while (next_after(current, level) != nullptr && compare_(next_after(current, level)->value, key)) {
      current = next_after(current, level);
    }
    return current;
  }

  [[nodiscard]] constexpr node* predecessor_upper_at_level(node* start, std::size_t level, const key_type& key) const {
    node* current = start;
    while (next_after(current, level) != nullptr && !compare_(key, next_after(current, level)->value)) {
      current = next_after(current, level);
    }
    return current;
  }

  [[nodiscard]] constexpr std::array<node*, max_level> search_predecessors_lower(const key_type& key) const {
    std::array<node*, max_level> update{};
    node* current = nullptr;
    for (std::size_t level = level_count_; level-- > 0;) {
      current = predecessor_lower_at_level(current, level, key);
      update[level] = current;
    }
    return update;
  }

  [[nodiscard]] constexpr std::array<node*, max_level> search_predecessors_upper(const key_type& key) const {
    std::array<node*, max_level> update{};
    node* current = nullptr;
    for (std::size_t level = level_count_; level-- > 0;) {
      current = predecessor_upper_at_level(current, level, key);
      update[level] = current;
    }
    return update;
  }

  [[nodiscard]] constexpr node* find_node(const key_type& key) const noexcept {
    node* current = nullptr;
    for (std::size_t level = level_count_; level-- > 0;) {
      while (next_after(current, level) != nullptr && compare_(next_after(current, level)->value, key)) {
        current = next_after(current, level);
      }
    }
    node* candidate = next_after(current, 0);
    return candidate != nullptr && equivalent(candidate->value, key) ? candidate : nullptr;
  }

  [[nodiscard]] constexpr std::size_t random_height() {
    std::size_t height = 1;
    while (height < max_level && (next_random_bit() != 0)) {
      ++height;
    }
    return height;
  }

  [[nodiscard]] constexpr unsigned next_random_bit() noexcept {
    rng_state_ ^= rng_state_ << 7;
    rng_state_ ^= rng_state_ >> 9;
    return static_cast<unsigned>(rng_state_ & 1u);
  }

  constexpr void ensure_levels(std::size_t height) {
    if (height > level_count_) {
      level_count_ = height;
    }
  }

  constexpr void trim_empty_levels() {
    while (level_count_ > 1 && head_[level_count_ - 1] == nullptr) {
      --level_count_;
    }
  }

  constexpr void erase_node(node* target) {
    for (std::size_t level = 0; level < level_count_; ++level) {
      node* current = nullptr;
      while (next_after(current, level) != nullptr && next_after(current, level) != target &&
             !compare_(target->value, next_after(current, level)->value)) {
        current = next_after(current, level);
      }
      if (next_after(current, level) == target) {
        set_next_after(current, level, target->next[level]);
      }
    }
    destroy_node(target);
    --size_;
    trim_empty_levels();
  }

  template <class Value>
  constexpr iterator insert_impl(Value&& value) {
    const auto height = random_height();
    ensure_levels(height);
    const auto update = search_predecessors_upper(value);

    node* inserted = create_node(height, std::forward<Value>(value));
    for (std::size_t level = 0; level < height; ++level) {
      inserted->next[level] = next_after(update[level], level);
      set_next_after(update[level], level, inserted);
    }
    ++size_;
    return iterator(inserted);
  }
};

template <class Key, class Compare, class Allocator>
constexpr void swap(skip_list_multiset<Key, Compare, Allocator>& lhs,
                    skip_list_multiset<Key, Compare, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

}  // namespace chaistl
