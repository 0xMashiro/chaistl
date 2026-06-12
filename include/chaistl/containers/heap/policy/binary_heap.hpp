// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <span>
#include <utility>

namespace chaistl::heap_policy {

// ============================================================================
// binary_heap_policy — the implicit d=2 array heap
//
// The classic structure behind std::priority_queue: a complete binary tree
// stored breadth-first in contiguous memory, so the shape needs no pointers
// at all — arithmetic on indices is the tree:
//
//   parent(i) = (i - 1) / 2        children(i) = 2i + 1, 2i + 2
//
// All algorithms use the *hole* technique rather than std::swap chains:
// the element being repositioned is moved out into a local once, the gap
// ("hole") is slid along the sift path with one move per level, and the
// element is dropped into its final slot — about half the moves of a
// swap-based sift (libstdc++'s __push_heap/__adjust_heap do the same).
//
// Complexity: push O(log n), pop O(log n), make O(n), top O(1).
//
// Exception note: the comparator may throw mid-sift. The container's vector
// still owns n fully-constructed elements (the hole holds a moved-from
// duplicate), so the heap stays a valid container in unspecified order —
// the same basic guarantee the standard heap algorithms give.
// ============================================================================

struct binary_heap_policy {
  /**
   * @brief Restore the heap after the container appended a new last element.
   */
  template <class T, class Compare>
  static constexpr void push(std::span<T> heap, const Compare& cmp) {
    sift_up(heap, heap.size() - 1, cmp);
  }

  /**
   * @brief Move the top element to the back; re-heap the rest.
   *
   * The displaced last element re-enters through a top-down hole sift.
   * For a one-element heap the value is already where the container's
   * pop_back() wants it.
   */
  template <class T, class Compare>
  static constexpr void pop(std::span<T> heap, const Compare& cmp) {
    if (heap.size() <= 1) {
      return;
    }
    T displaced = std::move(heap.back());
    heap.back() = std::move(heap.front());
    sift_down(heap.first(heap.size() - 1), 0, std::move(displaced), cmp);
  }

  /**
   * @brief Heapify an arbitrary range: Floyd's bottom-up construction.
   *
   * Sifting subtrees from the last parent down to the root costs
   * Σ (nodes at height h) · h = O(n) — the reason "build a heap then pop n
   * times" beats "push n times" as a heapsort front end.
   */
  template <class T, class Compare>
  static constexpr void make(std::span<T> heap, const Compare& cmp) {
    if (heap.size() <= 1) {
      return;
    }
    std::size_t i = parent(heap.size() - 1);
    while (true) {
      sift_down(heap, i, std::move(heap[i]), cmp);
      if (i == 0) {
        return;
      }
      --i;
    }
  }

  /// The heap property, checked directly from its definition. Tests only.
  template <class T, class Compare>
  [[nodiscard]] static constexpr bool verify(std::span<const T> heap, const Compare& cmp) {
    for (std::size_t i = 1; i < heap.size(); ++i) {
      if (cmp(heap[parent(i)], heap[i])) {
        return false;
      }
    }
    return true;
  }

 private:
  [[nodiscard]] static constexpr std::size_t parent(std::size_t i) noexcept { return (i - 1) / 2; }

  // Slide the hole at `index` toward the root while the element above it is
  // smaller than the element in flight, then drop the element in.
  template <class T, class Compare>
  static constexpr void sift_up(std::span<T> heap, std::size_t index, const Compare& cmp) {
    T value = std::move(heap[index]);
    drop_in_from(heap, index, 0, std::move(value), cmp);
  }

  // The ascending half of a hole sift: starting at the hole `index`, move
  // ancestors down while they are smaller than `value` (never above `top`),
  // then drop `value` into the hole where the walk stopped.
  template <class T, class Compare>
  static constexpr void drop_in_from(std::span<T> heap, std::size_t index, std::size_t top, T value,
                                     const Compare& cmp) {
    while (index > top && cmp(heap[parent(index)], value)) {
      heap[index] = std::move(heap[parent(index)]);
      index = parent(index);
    }
    heap[index] = std::move(value);
  }

  // Floyd's bounce for pop/make_heap. The displaced value came from a leaf, so
  // during pop it usually belongs near the bottom; testing it at every level
  // mostly burns one predictable-false comparison per level. Move the hole to
  // a leaf first, then use the ordinary sift-up half to place the value.
  template <class T, class Compare>
  static constexpr void sift_down(std::span<T> heap, std::size_t index, T value, const Compare& cmp) {
    const std::size_t size = heap.size();
    const std::size_t top = index;
    std::size_t child = 2 * index + 1;
    while (child < size) {
      if (child + 1 < size && cmp(heap[child], heap[child + 1])) {
        ++child;
      }
      heap[index] = std::move(heap[child]);
      index = child;
      child = 2 * index + 1;
    }
    drop_in_from(heap, index, top, std::move(value), cmp);
  }
};

}  // namespace chaistl::heap_policy
