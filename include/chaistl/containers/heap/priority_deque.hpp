// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// priority_deque - Double-ended priority queue
// ============================================================================
//
// A min-max heap stores elements in one array with alternating min and max
// levels. The root is the minimum; the maximum is one of the root's children.
// This gives O(1) min()/max() and O(log n) insertion/removal at either end.
//
// References:
// - Atkinson et al., Min-Max Heaps and Generalized Priority Queues, 1986.
// - dsacpp SMMH/DEPQ, the teaching double-ended priority queue ancestor.

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/containers/sequence/vector.hpp>
#include <chaistl/memory/allocator.hpp>
#include <chaistl/utility/hardening.hpp>

#include <concepts>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

namespace chaistl {

template <class T, class Compare = std::less<T>, class Allocator = allocator<T>>
  requires concepts::container_element<T> && concepts::allocator_for<Allocator, T> &&
           std::strict_weak_order<const Compare&, const T&, const T&>
class priority_deque {
 public:
  using value_type = T;
  using value_compare = Compare;
  using allocator_type = Allocator;
  using container_type = vector<T, Allocator>;
  using size_type = typename container_type::size_type;
  using const_reference = const T&;

  constexpr priority_deque() = default;

  explicit constexpr priority_deque(const Compare& compare, const Allocator& alloc = Allocator())
      : compare_(compare), storage_(alloc) {}

  explicit constexpr priority_deque(const Allocator& alloc) : storage_(alloc) {}

  template <std::input_iterator InputIt>
  constexpr priority_deque(InputIt first,
                           InputIt last,
                           const Compare& compare = Compare(),
                           const Allocator& alloc = Allocator())
      : compare_(compare), storage_(first, last, alloc) {
    make_heap();
  }

  template <std::input_iterator InputIt>
  constexpr priority_deque(InputIt first, InputIt last, const Allocator& alloc)
      : priority_deque(first, last, Compare(), alloc) {}

  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
  constexpr priority_deque(std::from_range_t,
                           R&& range,
                           const Compare& compare = Compare(),
                           const Allocator& alloc = Allocator())
      : compare_(compare), storage_(std::from_range, std::forward<R>(range), alloc) {
    make_heap();
  }

  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
  constexpr priority_deque(std::from_range_t, R&& range, const Allocator& alloc)
      : priority_deque(std::from_range, std::forward<R>(range), Compare(), alloc) {}

  constexpr priority_deque(std::initializer_list<T> init,
                           const Compare& compare = Compare(),
                           const Allocator& alloc = Allocator())
      : priority_deque(init.begin(), init.end(), compare, alloc) {}

  constexpr priority_deque(std::initializer_list<T> init, const Allocator& alloc)
      : priority_deque(init.begin(), init.end(), Compare(), alloc) {}

  [[nodiscard]] constexpr bool empty() const noexcept { return storage_.empty(); }
  [[nodiscard]] constexpr size_type size() const noexcept { return storage_.size(); }
  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return storage_.get_allocator(); }
  [[nodiscard]] constexpr value_compare value_comp() const { return compare_; }

  [[nodiscard]] constexpr const_reference min() const {
    CHAI_HARDENED(!empty(), "chaistl::priority_deque::min: empty container");
    return storage_[0];
  }

  [[nodiscard]] constexpr const_reference max() const {
    CHAI_HARDENED(!empty(), "chaistl::priority_deque::max: empty container");
    return storage_[max_index()];
  }

  // Mutating operations append/replace in the backing vector, then repair the
  // min-max heap by comparing and swapping along one ancestor/descendant path.
  // Allocation/construction failures keep vector's guarantee; a throwing
  // comparator or element swap can leave the valid storage out of heap order,
  // matching the usual heap-adaptor boundary.
  constexpr void push(const T& value) {
    storage_.push_back(value);
    bubble_up(storage_.size() - 1);
  }

  constexpr void push(T&& value) {
    storage_.push_back(std::move(value));
    bubble_up(storage_.size() - 1);
  }

  template <class... Args>
  constexpr void emplace(Args&&... args) {
    storage_.emplace_back(std::forward<Args>(args)...);
    bubble_up(storage_.size() - 1);
  }

  constexpr void pop_min() {
    CHAI_HARDENED(!empty(), "chaistl::priority_deque::pop_min: empty container");
    remove_at_and_trickle(0);
  }

  constexpr void pop_max() {
    CHAI_HARDENED(!empty(), "chaistl::priority_deque::pop_max: empty container");
    remove_at_and_trickle(max_index());
  }

  constexpr void clear() noexcept { storage_.clear(); }

  constexpr void swap(priority_deque& other) noexcept(std::is_nothrow_swappable_v<Compare> &&
                                                      noexcept(storage_.swap(other.storage_))) {
    using std::swap;
    swap(compare_, other.compare_);
    storage_.swap(other.storage_);
  }

  /**
   * @brief Check the min-max heap invariant.
   *
   * Level parity decides the local order: nodes on min levels are no greater
   * than their descendants; nodes on max levels are no less than theirs.
   * O(n), intended for tests and teaching rather than normal container use.
   */
  [[nodiscard]] constexpr bool verify() const {
    for (size_type index = 0; index < storage_.size(); ++index) {
      if (!node_ordered_before_descendants(index)) {
        return false;
      }
    }
    return true;
  }

 private:
  [[no_unique_address]] Compare compare_{};
  container_type storage_{};

  [[nodiscard]] static constexpr size_type parent(size_type index) noexcept { return (index - 1) / 2; }
  [[nodiscard]] static constexpr size_type grandparent(size_type index) noexcept { return parent(parent(index)); }
  [[nodiscard]] static constexpr size_type left_child(size_type index) noexcept { return index * 2 + 1; }
  [[nodiscard]] static constexpr size_type right_child(size_type index) noexcept { return index * 2 + 2; }
  [[nodiscard]] static constexpr size_type first_grandchild(size_type index) noexcept { return index * 4 + 3; }

  [[nodiscard]] static constexpr bool is_grandchild(size_type root, size_type candidate) noexcept {
    return candidate >= first_grandchild(root);
  }

  [[nodiscard]] static constexpr bool is_min_level(size_type index) noexcept {
    size_type level = 0;
    for (++index; index > 1; index >>= 1) {
      ++level;
    }
    return level % 2 == 0;
  }

  [[nodiscard]] constexpr size_type max_index() const noexcept {
    if (storage_.size() < 3) {
      return storage_.size() - 1;
    }
    return compare_(storage_[1], storage_[2]) ? 2 : 1;
  }

  constexpr void make_heap() {
    if (storage_.size() < 2) {
      return;
    }
    for (size_type index = parent(storage_.size() - 1) + 1; index-- > 0;) {
      trickle_down(index);
    }
  }

  constexpr void bubble_up(size_type index) {
    if (index == 0) {
      return;
    }

    const auto parent_index = parent(index);
    if (is_min_level(index)) {
      if (compare_(storage_[parent_index], storage_[index])) {
        swap_elements(parent_index, index);
        bubble_up_max(parent_index);
      } else {
        bubble_up_min(index);
      }
    } else {
      if (compare_(storage_[index], storage_[parent_index])) {
        swap_elements(parent_index, index);
        bubble_up_min(parent_index);
      } else {
        bubble_up_max(index);
      }
    }
  }

  constexpr void bubble_up_min(size_type index) {
    while (index >= 3) {
      const auto gp = grandparent(index);
      if (!compare_(storage_[index], storage_[gp])) {
        break;
      }
      swap_elements(index, gp);
      index = gp;
    }
  }

  constexpr void bubble_up_max(size_type index) {
    while (index >= 3) {
      const auto gp = grandparent(index);
      if (!compare_(storage_[gp], storage_[index])) {
        break;
      }
      swap_elements(index, gp);
      index = gp;
    }
  }

  constexpr void remove_at_and_trickle(size_type index) {
    if (storage_.size() == 1) {
      storage_.pop_back();
      return;
    }
    storage_[index] = std::move(storage_.back());
    storage_.pop_back();
    if (index < storage_.size()) {
      trickle_down(index);
    }
  }

  constexpr void trickle_down(size_type index) {
    if (is_min_level(index)) {
      trickle_down_min(index);
    } else {
      trickle_down_max(index);
    }
  }

  constexpr void trickle_down_min(size_type index) {
    while (left_child(index) < storage_.size()) {
      const auto best = smallest_descendant(index);
      if (is_grandchild(index, best)) {
        if (!compare_(storage_[best], storage_[index])) {
          break;
        }
        swap_elements(best, index);
        const auto parent_of_best = parent(best);
        if (compare_(storage_[parent_of_best], storage_[best])) {
          swap_elements(parent_of_best, best);
        }
        index = best;
      } else {
        if (compare_(storage_[best], storage_[index])) {
          swap_elements(best, index);
        }
        break;
      }
    }
  }

  constexpr void trickle_down_max(size_type index) {
    while (left_child(index) < storage_.size()) {
      const auto best = largest_descendant(index);
      if (is_grandchild(index, best)) {
        if (!compare_(storage_[index], storage_[best])) {
          break;
        }
        swap_elements(best, index);
        const auto parent_of_best = parent(best);
        if (compare_(storage_[best], storage_[parent_of_best])) {
          swap_elements(parent_of_best, best);
        }
        index = best;
      } else {
        if (compare_(storage_[index], storage_[best])) {
          swap_elements(best, index);
        }
        break;
      }
    }
  }

  [[nodiscard]] constexpr size_type smallest_descendant(size_type index) const {
    auto best = left_child(index);
    consider_smaller(best, right_child(index));
    for (size_type child = first_grandchild(index); child < first_grandchild(index) + 4; ++child) {
      consider_smaller(best, child);
    }
    return best;
  }

  [[nodiscard]] constexpr size_type largest_descendant(size_type index) const {
    auto best = left_child(index);
    consider_larger(best, right_child(index));
    for (size_type child = first_grandchild(index); child < first_grandchild(index) + 4; ++child) {
      consider_larger(best, child);
    }
    return best;
  }

  constexpr void consider_smaller(size_type& best, size_type candidate) const {
    if (candidate < storage_.size() && compare_(storage_[candidate], storage_[best])) {
      best = candidate;
    }
  }

  constexpr void consider_larger(size_type& best, size_type candidate) const {
    if (candidate < storage_.size() && compare_(storage_[best], storage_[candidate])) {
      best = candidate;
    }
  }

  [[nodiscard]] constexpr bool node_ordered_before_descendants(size_type index) const {
    const bool min_level = is_min_level(index);
    for (size_type child = left_child(index); child <= right_child(index); ++child) {
      if (child < storage_.size() && violates_level_order(index, child, min_level)) {
        return false;
      }
    }
    for (size_type child = first_grandchild(index); child < first_grandchild(index) + 4; ++child) {
      if (child < storage_.size() && violates_level_order(index, child, min_level)) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] constexpr bool violates_level_order(size_type ancestor,
                                                    size_type descendant,
                                                    bool ancestor_min_level) const {
    if (ancestor_min_level) {
      return compare_(storage_[descendant], storage_[ancestor]);
    }
    return compare_(storage_[ancestor], storage_[descendant]);
  }

  constexpr void swap_elements(size_type lhs,
                               size_type rhs) noexcept(noexcept(std::swap(std::declval<T&>(), std::declval<T&>()))) {
    using std::swap;
    swap(storage_[lhs], storage_[rhs]);
  }
};

template <class T, class Compare, class Allocator>
constexpr void swap(priority_deque<T, Compare, Allocator>& lhs,
                    priority_deque<T, Compare, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

}  // namespace chaistl
