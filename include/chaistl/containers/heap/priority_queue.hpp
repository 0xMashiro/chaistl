// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// priority_queue - Policy-based heap container
// ============================================================================
//
// Architecture:
//   - Defaults to an implicit binary heap over chaistl::vector, matching the
//     std::priority_queue adaptor model.
//   - Node-heap policies switch the backend to a left-child/next-sibling
//     forest, enabling join(), stable handles, erase(), and modify().
//   - The policy concept decides which API exists at compile time instead of
//     routing unsupported operations through slow fallbacks.
//
// Standardization archaeology:
//   - std::priority_queue was standardized in C++98 as a container adaptor over
//     a random-access sequence plus the heap algorithms.
//   - The standard adaptor intentionally exposes only push/pop/top; mutable
//     handles and meld/join were left to non-standard libraries such as GCC
//     pb_ds and Boost.Heap.
//   - chaistl keeps the standard binary-heap shape as the default, then makes
//     the non-standard heap families explicit policies so their extra
//     complexity is visible and benchmarkable.
//
// Non-standard extensions:
//   - d_ary_heap_policy<Arity> keeps the contiguous std-like API, but makes
//     the implicit heap branching factor configurable.
//   - skew_heap_policy, leftist_heap_policy, and fibonacci_heap_policy add
//     join(); pairing_heap_policy and binomial_heap_policy also add stable
//     point_iterators, erase(), and modify().
//
// References:
//   - C++ Draft: https://eel.is/c++draft/priqueue
//   - cppreference: https://en.cppreference.com/w/cpp/container/priority_queue
//   - GCC pb_ds priority_queue: https://gcc.gnu.org/onlinedocs/libstdc++/ext/pb_ds/
//   - Boost.Heap: https://www.boost.org/doc/libs/release/doc/html/heap.html
//   - ADR: docs/develop/decisions/heap-policy-design.md

#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/containers/heap/forest.hpp>
#include <chaistl/containers/heap/node.hpp>
#include <chaistl/containers/heap/policy/binary_heap.hpp>
#include <chaistl/containers/heap/policy/concepts.hpp>
#include <chaistl/containers/sequence/vector.hpp>
#include <chaistl/memory/allocator.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>
#include <chaistl/utility/hardening.hpp>

#include <concepts>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

namespace chaistl {

/**
 * @brief Policy-based priority queue.
 *
 * @ingroup containers_heap
 *
 * @tparam T         Element type.
 * @tparam Compare   Strict weak ordering; the top element is the *largest*
 *                   under it (the std::priority_queue convention).
 * @tparam Allocator Allocator for element/node storage.
 * @tparam Policy    Heap structure. The policy's concept category decides
 *                   both the storage backend and the API surface:
 *
 *   - @ref heap_policy::array_heap_policy (default: binary_heap_policy)
 *     stores elements contiguously in a chaistl::vector and offers exactly
 *     the std::priority_queue interface.
 *   - @ref heap_policy::node_heap_policy stores a linked forest of nodes
 *     and additionally offers join().
 *   - @ref heap_policy::mutable_node_heap_policy (pairing_heap_policy and
 *     binomial_heap_policy) additionally returns a stable @ref point_iterator
 *     from push/emplace and offers erase(it) and modify(it, v).
 *
 * Capability is part of the type: members that a policy cannot honor are
 * constrained away, so `priority_queue<int>::erase` is not a slower
 * fallback — it does not exist, and the compiler says which concept was
 * missing. Query the surface programmatically via the public constants
 * `uses_array_storage` / `has_stable_handles`.
 *
 * Unlike std::priority_queue (a container *adaptor*), this is a container:
 * it owns storage through Allocator directly, supports allocator-extended
 * construction, get_allocator(), and per-element node allocation for the
 * node policies — the pb_ds priority_queue design with std semantics.
 *
 * Iterator/handle invalidation (node policies): a point_iterator tracks its
 * element until that element is erased; no other push/pop/modify/join ever
 * invalidates it. Values are never moved between nodes.
 *
 * Exception safety: memory safety is unconditional, including a throwing
 * comparator (see policy/concepts.hpp). pop()/erase() are strong; push()
 * on a node policy leaves the element inserted if the comparator throws;
 * modify() erases the element if assigning the new value throws.
 */
template <class T,
          class Compare = std::less<T>,
          class Allocator = allocator<T>,
          class Policy = heap_policy::binary_heap_policy>
  requires concepts::container_element<T> && concepts::allocator_for<Allocator, T> &&
           std::strict_weak_order<const Compare&, const T&, const T&> &&
           heap_policy::heap_policy_for<Policy, T, Compare>
class priority_queue {
 public:
  /// True when Policy models array_heap_policy: contiguous storage,
  /// std::priority_queue API only.
  static constexpr bool uses_array_storage = heap_policy::array_heap_policy<Policy, T, Compare>;

  /// True when Policy models mutable_node_heap_policy: push/emplace return
  /// stable handles and erase()/modify() exist.
  static constexpr bool has_stable_handles = heap_policy::mutable_node_heap_policy<Policy, T, Compare>;

 private:
  using node_type = heap_policy::policy_node_t<Policy, T>;
  using forest_type = detail::heap::node_forest<T, Allocator, heap_policy::node_extension_of_t<Policy>>;

  // Naming a specialization does not instantiate it, so the backend that
  // the policy category rejects is never even completed.
  using storage_type = std::conditional_t<uses_array_storage, chaistl::vector<T, Allocator>, forest_type>;

 public:
  // ==========================================================================
  // Member types
  // ==========================================================================
  using value_type = T;
  using value_compare = Compare;
  using allocator_type = Allocator;
  using policy_type = Policy;
  using size_type = typename std::allocator_traits<Allocator>::size_type;
  using const_reference = const T&;

  /// Stable handle to one element (see class docs). Only the handle-capable
  /// policies ever produce one; the alias exists for all instantiations.
  using point_iterator = detail::heap::heap_point_iterator<node_type>;
  using point_const_iterator = detail::heap::heap_point_iterator<const node_type>;

  // ==========================================================================
  // Constructors / assignment
  //
  // Both storage backends are regular RAII values (chaistl::vector and
  // node_forest), so copy/move/destroy/assignment are the defaults, and
  // constructor bodies below need no rollback guards: a throw mid-fill
  // destroys the half-built storage member normally.
  // ==========================================================================

  constexpr priority_queue() = default;

  explicit constexpr priority_queue(const Compare& compare, const Allocator& alloc = Allocator())
      : compare_(compare), storage_(alloc) {}

  explicit constexpr priority_queue(const Allocator& alloc) : storage_(alloc) {}

  template <std::input_iterator InputIt>
  constexpr priority_queue(InputIt first,
                           InputIt last,
                           const Compare& compare = Compare(),
                           const Allocator& alloc = Allocator())
      : compare_(compare), storage_(alloc) {
    if constexpr (uses_array_storage) {
      storage_ = storage_type(first, last, alloc);  // bulk load, then O(n) make
      Policy::make(mutable_span(), compare_);
    } else {
      for (; first != last; ++first) {
        create_and_insert_node(*first);
      }
    }
  }

  template <std::input_iterator InputIt>
  constexpr priority_queue(InputIt first, InputIt last, const Allocator& alloc)
      : priority_queue(first, last, Compare(), alloc) {}

  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
  constexpr priority_queue(std::from_range_t,
                           R&& range,
                           const Compare& compare = Compare(),
                           const Allocator& alloc = Allocator())
      : compare_(compare), storage_(alloc) {
    if constexpr (uses_array_storage) {
      storage_ = storage_type(std::from_range, std::forward<R>(range), alloc);
      Policy::make(mutable_span(), compare_);
    } else {
      for (auto&& value : range) {
        create_and_insert_node(std::forward<decltype(value)>(value));
      }
    }
  }

  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
  constexpr priority_queue(std::from_range_t, R&& range, const Allocator& alloc)
      : priority_queue(std::from_range, std::forward<R>(range), Compare(), alloc) {}

  constexpr priority_queue(std::initializer_list<T> init,
                           const Compare& compare = Compare(),
                           const Allocator& alloc = Allocator())
      : priority_queue(init.begin(), init.end(), compare, alloc) {}

  constexpr priority_queue(std::initializer_list<T> init, const Allocator& alloc)
      : priority_queue(init.begin(), init.end(), Compare(), alloc) {}

  constexpr priority_queue(const priority_queue&) = default;
  constexpr priority_queue(priority_queue&&) = default;

  constexpr priority_queue(const priority_queue& other, const Allocator& alloc)
      : compare_(other.compare_), storage_(other.storage_, alloc) {}

  constexpr priority_queue(priority_queue&& other, const Allocator& alloc)
      : compare_(std::move(other.compare_)), storage_(std::move(other.storage_), alloc) {}

  constexpr priority_queue& operator=(const priority_queue&) = default;
  constexpr priority_queue& operator=(priority_queue&&) = default;
  constexpr ~priority_queue() = default;

  // ==========================================================================
  // Observers
  // ==========================================================================

  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  [[nodiscard]] constexpr size_type size() const noexcept {
    if constexpr (uses_array_storage) {
      return storage_.size();
    } else {
      return storage_.count;
    }
  }

  /**
   * @brief The largest element. O(1); O(log n) for binomial_heap_policy,
   * where the maximum must be found among the tree roots.
   */
  [[nodiscard]] constexpr const_reference top() const {
    CHAI_HARDENED(!empty(), "chaistl::priority_queue::top: empty container");
    if constexpr (uses_array_storage) {
      return storage_.front();
    } else {
      return Policy::top(const_root(), compare_)->value;
    }
  }

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return storage_.get_allocator(); }

  [[nodiscard]] constexpr value_compare value_comp() const { return compare_; }

  // ==========================================================================
  // Modifiers
  //
  // For handle-capable policies, push/emplace return the element's stable
  // point_iterator (the Boost.Heap convention); otherwise they return void.
  // The pairs below overload on the constraint — only one of each pair
  // exists in any instantiation.
  // ==========================================================================

  constexpr point_iterator push(const T& value)
    requires has_stable_handles
  {
    return point_iterator(create_and_insert_node(value));
  }

  constexpr point_iterator push(T&& value)
    requires has_stable_handles
  {
    return point_iterator(create_and_insert_node(std::move(value)));
  }

  constexpr void push(const T& value)
    requires(!has_stable_handles)
  {
    push_value(value);
  }

  constexpr void push(T&& value)
    requires(!has_stable_handles)
  {
    push_value(std::move(value));
  }

  template <class... Args>
    requires std::constructible_from<T, Args...> && has_stable_handles
  constexpr point_iterator emplace(Args&&... args) {
    return point_iterator(create_and_insert_node(std::forward<Args>(args)...));
  }

  template <class... Args>
    requires std::constructible_from<T, Args...> && (!has_stable_handles)
  constexpr void emplace(Args&&... args) {
    push_value(std::forward<Args>(args)...);
  }

  /// Insert every element of @p range; the C++23 std::priority_queue
  /// extension. Array storage appends in bulk and re-heapifies in O(n+m),
  /// the libstdc++ strategy.
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
  constexpr void push_range(R&& range) {
    if constexpr (uses_array_storage) {
      storage_.append_range(std::forward<R>(range));
      Policy::make(mutable_span(), compare_);
    } else {
      for (auto&& value : range) {
        create_and_insert_node(std::forward<decltype(value)>(value));
      }
    }
  }

  constexpr void pop() {
    CHAI_HARDENED(!empty(), "chaistl::priority_queue::pop: empty container");
    if constexpr (uses_array_storage) {
      Policy::pop(mutable_span(), compare_);
      storage_.pop_back();
    } else {
      node_type* top_node = Policy::extract_top(storage_.root, compare_);
      storage_.destroy(top_node);
      --storage_.count;
    }
  }

  /**
   * @brief Remove the element @p it refers to. Strong guarantee.
   *
   * @pre @p it was obtained from this container and not yet erased.
   */
  constexpr void erase(point_iterator it)
    requires has_stable_handles
  {
    CHAI_HARDENED(it.node() != nullptr && !empty(), "chaistl::priority_queue::erase: invalid iterator");
    node_type* node = Policy::detach(it.node(), storage_.root, compare_);
    storage_.destroy(node);
    --storage_.count;
  }

  /**
   * @brief Replace the element @p it refers to with @p value and restore
   * heap order. The handle remains valid and refers to the new value.
   *
   * Implemented as detach → assign → reinsert, so it handles increases and
   * decreases alike. If assigning the value throws, the element is erased
   * (the half-written value must not re-enter the order); the handle is
   * then invalid.
   */
  constexpr void modify(point_iterator it, const T& value)
    requires has_stable_handles
  {
    replace_value_and_reinsert(it, value);
  }

  constexpr void modify(point_iterator it, T&& value)
    requires has_stable_handles
  {
    replace_value_and_reinsert(it, std::move(value));
  }

  /**
   * @brief Move every element of @p other into this queue. @p other is
   * left empty. O(1) for pairing, O(log n) for binomial.
   *
   * @pre get_allocator() == other.get_allocator(), and both comparators
   * induce the same ordering (the standard merge()/splice precondition).
   */
  constexpr void join(priority_queue& other)
    requires(!uses_array_storage)
  {
    CHAI_HARDENED(this != &other, "chaistl::priority_queue::join: self-join");
    CHAI_HARDENED(storage_.node_allocator() == other.storage_.node_allocator(),
                  "chaistl::priority_queue::join: allocators must compare equal");
    node_type* transferred_root = std::exchange(other.storage_.root, nullptr);
    storage_.count += std::exchange(other.storage_.count, 0);
    Policy::join(storage_.root, transferred_root, compare_);  // absorbs even on throw
  }

  constexpr void swap(priority_queue& other) noexcept(std::is_nothrow_swappable_v<Compare>) {
    using std::swap;
    swap(compare_, other.compare_);
    storage_.swap(other.storage_);
  }

  /**
   * @brief Full invariant check — structure, order, and size bookkeeping.
   *
   * O(n). For tests and teaching; never called by the container itself.
   */
  [[nodiscard]] constexpr bool verify() const {
    if constexpr (uses_array_storage) {
      return Policy::verify(std::span<const T>(storage_.data(), storage_.size()), compare_);
    } else {
      return detail::heap::count_nodes(const_root()) == storage_.count && Policy::verify(const_root(), compare_);
    }
  }

 private:
  [[no_unique_address]] Compare compare_{};
  storage_type storage_{};

  constexpr std::span<T> mutable_span() noexcept
    requires uses_array_storage
  {
    return std::span<T>(storage_.data(), storage_.size());
  }

  constexpr const node_type* const_root() const noexcept
    requires(!uses_array_storage)
  {
    return storage_.root;
  }

  // Node-policy insertion. Count first, then link: per the insert contract
  // the forest owns the node even if the comparator throws mid-link, so the
  // bookkeeping must already include it. Only node creation itself may fail
  // without ownership having transferred.
  template <class... Args>
  constexpr node_type* create_and_insert_node(Args&&... args)
    requires(!uses_array_storage)
  {
    node_type* node = storage_.create(std::forward<Args>(args)...);
    ++storage_.count;
    Policy::insert(node, storage_.root, compare_);
    return node;
  }

  template <class... Args>
  constexpr void push_value(Args&&... args) {
    if constexpr (uses_array_storage) {
      storage_.emplace_back(std::forward<Args>(args)...);
      Policy::push(mutable_span(), compare_);
    } else {
      create_and_insert_node(std::forward<Args>(args)...);
    }
  }

  template <class Arg>
  constexpr void replace_value_and_reinsert(point_iterator it, Arg&& value) {
    CHAI_HARDENED(it.node() != nullptr && !empty(), "chaistl::priority_queue::modify: invalid iterator");
    node_type* node = Policy::detach(it.node(), storage_.root, compare_);  // strong
    --storage_.count;
    auto guard = detail::make_exception_guard([&] {
      storage_.destroy(node);
    });
    node->value = std::forward<Arg>(value);
    guard.complete();
    ++storage_.count;
    Policy::insert(node, storage_.root, compare_);  // absorbs even on throw
  }
};

// ============================================================================
// Non-member swap
// ============================================================================

template <class T, class Compare, class Allocator, class Policy>
constexpr void swap(priority_queue<T, Compare, Allocator, Policy>& lhs,
                    priority_queue<T, Compare, Allocator, Policy>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

// ============================================================================
// Deduction guides
//
// Same shape as the set/map guides: per [container.reqmts], a guide drops
// out of overload resolution when an allocator lands in a Compare slot or a
// non-allocator in an Allocator slot — qualifies_as_allocator is the gate.
// The Policy parameter is never deduced; CTAD yields the default (binary)
// queue, exactly as std::priority_queue CTAD yields the vector-backed one.
// ============================================================================

template <class InputIt,
          class Compare = std::less<std::iter_value_t<InputIt>>,
          class Allocator = allocator<std::iter_value_t<InputIt>>>
  requires std::input_iterator<InputIt> && (!concepts::qualifies_as_allocator<Compare>) &&
           concepts::qualifies_as_allocator<Allocator>
priority_queue(InputIt, InputIt, Compare = Compare(), Allocator = Allocator())
    -> priority_queue<std::iter_value_t<InputIt>, Compare, Allocator>;

template <class InputIt, class Allocator>
  requires std::input_iterator<InputIt> && concepts::qualifies_as_allocator<Allocator>
priority_queue(InputIt, InputIt, Allocator)
    -> priority_queue<std::iter_value_t<InputIt>, std::less<std::iter_value_t<InputIt>>, Allocator>;

template <class R,
          class Compare = std::less<std::ranges::range_value_t<R>>,
          class Allocator = allocator<std::ranges::range_value_t<R>>>
  requires std::ranges::input_range<R> && (!concepts::qualifies_as_allocator<Compare>) &&
           concepts::qualifies_as_allocator<Allocator>
priority_queue(std::from_range_t, R&&, Compare = Compare(), Allocator = Allocator())
    -> priority_queue<std::ranges::range_value_t<R>, Compare, Allocator>;

template <class R, class Allocator>
  requires std::ranges::input_range<R> && concepts::qualifies_as_allocator<Allocator>
priority_queue(std::from_range_t, R&&, Allocator)
    -> priority_queue<std::ranges::range_value_t<R>, std::less<std::ranges::range_value_t<R>>, Allocator>;

}  // namespace chaistl
