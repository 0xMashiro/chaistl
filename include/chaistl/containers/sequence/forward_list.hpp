// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// forward_list - Singly-linked sequence container
// ============================================================================
//
// Architecture:
//   - Stores each element in an individually allocated node linked only by a
//     next pointer.
//   - A before-begin sentinel makes insertion and erasure after a position
//     uniform, matching the standard forward_list API shape.
//   - chaistl also keeps a tail pointer and size counter so push_back(),
//     append_range(), and size() are constant-time educational extensions.
//
// Standardization archaeology:
//   - forward_list was standardized in C++11 as the minimal-overhead singly
//     linked list. Its design deliberately omits size() and back insertion so
//     implementations can be just one head pointer.
//   - The before_begin()/insert_after()/erase_after() vocabulary is the direct
//     consequence of singly-linked structure: without a previous pointer, the
//     container can cheaply modify only the node after a known position.
//   - C++23 range insertion extended the sequence-container surface while
//     preserving the same after-position model.
//
// Non-standard extensions:
//   - Constant-time size(), tail tracking, push_back()/emplace_back(), and
//     append_range() trade one pointer plus one counter for a friendlier
//     teaching API.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/forwardlist
//   - cppreference: https://en.cppreference.com/w/cpp/container/forward_list
//   - ADR-003: forward-list tail pointer

#include <chaistl/containers/sequence/detail/common.hpp>
#include <chaistl/iterator/interface.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>
#include <chaistl/memory/detail/storage/node_owner.hpp>
#include <chaistl/memory_resource.hpp>
#include <chaistl/utility/hardening.hpp>

#include <compare>

namespace chaistl {

/**
 * @brief Singly-linked list container.
 *
 * @ingroup containers_sequence
 *
 * Stores elements in nodes linked by next pointers only.
 * Supports O(1) insertion and removal at the front, and O(1)
 * insertion/removal after any known position.
 *
 * Storage model (visible for learning):
 * - `before_begin_`  : sentinel node; before_begin_.next == first element
 * - `last_`          : pointer to the last element node (or &before_begin_ if empty)
 * - `size_`          : number of live elements
 *
 * @note Non-standard extensions (see ADR 003): this implementation
 * maintains a tail pointer and an element count, enabling O(1)
 * `push_back` / `emplace_back` / `append_range` and an O(1) `size()` —
 * none of which exist on `std::forward_list`, whose entire layout is a
 * single head pointer. Code that must stay portable to `std` should
 * avoid these members.
 *
 * References:
 *
 * - C++ Draft Standard: https://eel.is/c++draft/forwardlist
 * - cppreference: https://en.cppreference.com/w/cpp/container/forward_list
 * - ADR 003: docs/develop/decisions/003-forward-list-tail-pointer.md
 */
template <concepts::container_element T, concepts::allocator_for<T> Allocator = allocator<T>>
class forward_list {
 public:
  // =======================================================================
  // Member types
  // =======================================================================
  using value_type = T;
  using allocator_type = Allocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using size_type = typename allocator_traits::size_type;
  using difference_type = typename allocator_traits::difference_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename allocator_traits::pointer;
  using const_pointer = typename allocator_traits::const_pointer;

  // =======================================================================
  // Node types
  // =======================================================================

  struct node_base {
    node_base* next{nullptr};
  };

  struct node : node_base {
    union {
      T value;
    };

    constexpr node() noexcept : node_base{} {}
    constexpr ~node() {}
  };

  // =======================================================================
  // Iterator implementation
  // =======================================================================

  template <bool Const>
  class iterator_impl : public chaistl::iterator_interface {
   public:
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = typename allocator_traits::difference_type;
    using reference = std::conditional_t<Const, const T&, T&>;
    using pointer =
        std::conditional_t<Const, typename allocator_traits::const_pointer, typename allocator_traits::pointer>;

    constexpr iterator_impl() noexcept = default;
    constexpr iterator_impl(const iterator_impl&) noexcept = default;
    constexpr iterator_impl& operator=(const iterator_impl&) noexcept = default;

    constexpr iterator_impl(const iterator_impl<false>& other) noexcept
      requires Const
        : node_(other.node_) {}

    [[nodiscard]] constexpr reference operator*() const noexcept {
      return static_cast<std::conditional_t<Const, const node*, node*>>(node_)->value;
    }

    constexpr iterator_impl& operator++() noexcept {
      node_ = node_->next;
      return *this;
    }

    using iterator_interface::operator++;

    template <bool OtherConst>
    [[nodiscard]] friend constexpr bool operator==(const iterator_impl& lhs,
                                                   const iterator_impl<OtherConst>& rhs) noexcept {
      return lhs.node_ == rhs.node_;
    }

   private:
    template <bool>
    friend class iterator_impl;
    friend class forward_list;

    using stored_node_ptr = std::conditional_t<Const, const node_base*, node_base*>;
    stored_node_ptr node_{nullptr};

    constexpr explicit iterator_impl(stored_node_ptr node) noexcept : node_(node) {}
  };

  using iterator = iterator_impl<false>;
  using const_iterator = iterator_impl<true>;

 private:
  // =======================================================================
  // Allocator rebinding for nodes
  // =======================================================================
  using node_type = node;
  using node_allocator_type = typename allocator_traits::template rebind_alloc<node_type>;
  using node_allocator_traits = std::allocator_traits<node_allocator_type>;

  // =======================================================================
  // Storage
  // =======================================================================
  node_base before_begin_{};
  node_base* last_{&before_begin_};
  size_type size_{};

  [[no_unique_address]] allocator_type allocator_{};
  [[no_unique_address]] node_allocator_type node_allocator_{allocator_};

  // =======================================================================
  // Node lifecycle
  // =======================================================================
  constexpr void deallocate_node(node_type* node) noexcept {
    using node_pointer = typename node_allocator_traits::pointer;
    node_allocator_traits::deallocate(node_allocator_, std::pointer_traits<node_pointer>::pointer_to(*node), 1);
  }

  template <class... Args>
  [[nodiscard]] constexpr node_base* create_node(Args&&... args) {
    detail::node_owner<node_allocator_type> storage(node_allocator_);
    node_type* raw = storage.get();
    std::construct_at(raw);
    auto node_lifetime = detail::make_exception_guard([&] {
      std::destroy_at(raw);
    });
    node_allocator_traits::construct(node_allocator_, std::addressof(raw->value), std::forward<Args>(args)...);
    node_lifetime.complete();
    return storage.release();
  }

  constexpr void destroy_node(node_base* node) noexcept {
    auto* typed = static_cast<node_type*>(node);
    node_allocator_traits::destroy(node_allocator_, std::addressof(typed->value));
    std::destroy_at(typed);
    deallocate_node(typed);
  }

  // =======================================================================
  // Link / unlink helpers
  // =======================================================================

  // Link nodes [first, last] immediately after position.
  // Pre: first and last are in correct next-linked order, last->next may be anything.
  static constexpr void link_after(node_base* position, node_base* first, node_base* last_node) noexcept {
    last_node->next = position->next;
    position->next = first;
  }

  // Unlink nodes from position->next through last_node (inclusive).
  // Returns the node following last_node.
  static constexpr node_base* unlink_after(node_base* position, node_base* last_node) noexcept {
    node_base* result = last_node->next;
    position->next = result;
    return result;
  }

  // =======================================================================
  // Storage ownership helpers
  // =======================================================================
  constexpr void destroy_and_deallocate_nodes() noexcept {
    node_base* current = before_begin_.next;
    while (current != nullptr) {
      node_base* next = current->next;
      destroy_node(current);
      current = next;
    }
    before_begin_.next = nullptr;
    last_ = &before_begin_;
    size_ = 0;
  }

  constexpr void take_storage_from(forward_list& other) noexcept {
    before_begin_.next = other.before_begin_.next;
    last_ = other.empty() ? &before_begin_ : other.last_;
    size_ = std::exchange(other.size_, 0);
    other.before_begin_.next = nullptr;
    other.last_ = &other.before_begin_;
  }

  constexpr void replace_storage_from(forward_list& other) noexcept {
    destroy_and_deallocate_nodes();
    take_storage_from(other);
  }

  [[nodiscard]] constexpr bool storage_compatible_with(const forward_list& other) const {
    return allocator_ == other.allocator_;
  }

  constexpr void copy_allocator_from(const forward_list& other) {
    allocator_ = other.allocator_;
    node_allocator_ = node_allocator_type(allocator_);
  }

  constexpr void move_allocator_from(forward_list& other) {
    allocator_ = std::move(other.allocator_);
    node_allocator_ = node_allocator_type(allocator_);
  }

  constexpr void check_size(size_type new_size) const {
    if (new_size > max_size()) {
      throw std::length_error("chaistl::forward_list exceeds max_size");
    }
  }

  // Link one detached node after the current tail and make it the new tail.
  constexpr void link_back_node(node_base* new_last) noexcept {
    last_->next = new_last;
    last_ = new_last;
  }

 public:
  // =======================================================================
  // Constructors and assignment
  // =======================================================================

  constexpr forward_list() noexcept(noexcept(allocator_type{})) {
    // before_begin_ already zeroed, last_ already points to before_begin_.
  }

  explicit constexpr forward_list(const allocator_type& alloc) noexcept
      : allocator_(alloc), node_allocator_(allocator_) {}

  explicit constexpr forward_list(size_type count, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), node_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (size_type i = 0; i < count; ++i) {
      node_base* n = create_node();
      link_back_node(n);
      ++size_;
    }
    guard.complete();
  }

  constexpr forward_list(size_type count, const T& value, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), node_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (size_type i = 0; i < count; ++i) {
      node_base* n = create_node(value);
      link_back_node(n);
      ++size_;
    }
    guard.complete();
  }

  template <std::input_iterator InputIt>
  constexpr forward_list(InputIt first, InputIt last, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), node_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (; first != last; ++first) {
      node_base* n = create_node(*first);
      link_back_node(n);
      ++size_;
    }
    guard.complete();
  }

  constexpr forward_list(std::initializer_list<T> init, const allocator_type& alloc = allocator_type{})
      : forward_list(init.begin(), init.end(), alloc) {}

  constexpr forward_list(const forward_list& other)
      : allocator_(allocator_traits::select_on_container_copy_construction(other.allocator_)),
        node_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (const auto& value : other) {
      node_base* n = create_node(value);
      link_back_node(n);
      ++size_;
    }
    guard.complete();
  }

  constexpr forward_list(forward_list&& other) noexcept
      : allocator_(std::move(other.allocator_)), node_allocator_(std::move(other.node_allocator_)) {
    take_storage_from(other);
  }

  constexpr forward_list(const forward_list& other, const std::type_identity_t<Allocator>& alloc)
      : allocator_(alloc), node_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (const auto& value : other) {
      node_base* n = create_node(value);
      link_back_node(n);
      ++size_;
    }
    guard.complete();
  }

  constexpr forward_list(forward_list&& other, const std::type_identity_t<Allocator>& alloc)
      : allocator_(alloc), node_allocator_(allocator_) {
    if (storage_compatible_with(other)) {
      take_storage_from(other);
    } else {
      auto guard = detail::make_exception_guard([&] {
        destroy_and_deallocate_nodes();
      });
      for (auto& value : other) {
        node_base* n = create_node(std::move(value));
        link_back_node(n);
        ++size_;
      }
      guard.complete();
    }
  }

  template <concepts::container_compatible_range<T> R>
  constexpr forward_list(std::from_range_t, R&& range, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), node_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (auto&& value : range) {
      node_base* n = create_node(std::forward<decltype(value)>(value));
      link_back_node(n);
      ++size_;
    }
    guard.complete();
  }

  constexpr ~forward_list() { destroy_and_deallocate_nodes(); }

  constexpr forward_list& operator=(const forward_list& other);

  constexpr forward_list& operator=(forward_list&& other) noexcept(
      meta::is_nothrow_container_move_assignable_v<allocator_type, T>);

  constexpr forward_list& operator=(std::initializer_list<T> init);

  template <std::input_iterator InputIt>
  constexpr void assign(InputIt first, InputIt last);

  template <concepts::container_compatible_range<T> R>
  constexpr void assign_range(R&& range);

  constexpr void assign(size_type count, const T& value);
  constexpr void assign(std::initializer_list<T> init);

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return allocator_; }

  // =======================================================================
  // Element access
  // =======================================================================

  [[nodiscard]] constexpr reference front() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::forward_list::front: empty container");
    return *begin();
  }
  [[nodiscard]] constexpr const_reference front() const noexcept {
    CHAI_HARDENED(!empty(), "chaistl::forward_list::front: empty container");
    return *begin();
  }

  // =======================================================================
  // Iterators
  // =======================================================================

  [[nodiscard]] constexpr iterator before_begin() noexcept { return iterator(&before_begin_); }
  [[nodiscard]] constexpr const_iterator before_begin() const noexcept { return const_iterator(&before_begin_); }
  [[nodiscard]] constexpr const_iterator cbefore_begin() const noexcept { return before_begin(); }

  [[nodiscard]] constexpr iterator begin() noexcept { return iterator(before_begin_.next); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(before_begin_.next); }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

  [[nodiscard]] constexpr iterator end() noexcept { return iterator(nullptr); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(nullptr); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  // =======================================================================
  // Capacity
  // =======================================================================

  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
  [[nodiscard]] constexpr size_type max_size() const noexcept {
    return node_allocator_traits::max_size(node_allocator_);
  }

  constexpr void resize(size_type count);
  constexpr void resize(size_type count, const T& value);

  // =======================================================================
  // Modifiers
  // =======================================================================

  template <class... Args>
  constexpr reference emplace_front(Args&&... args) {
    check_size(size_ + 1);
    node_base* n = create_node(std::forward<Args>(args)...);
    if (empty()) {
      link_back_node(n);
    } else {
      link_after(&before_begin_, n, n);
    }
    ++size_;
    return front();
  }

  constexpr void push_front(const T& value) { emplace_front(value); }
  constexpr void push_front(T&& value) { emplace_front(std::move(value)); }

  template <concepts::container_compatible_range<T> R>
  constexpr void prepend_range(R&& range);

  constexpr void pop_front() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::forward_list::pop_front: empty container");
    node_base* n = before_begin_.next;
    unlink_after(&before_begin_, n);
    --size_;
    if (empty()) {
      last_ = &before_begin_;
    }
    destroy_node(n);
  }

  template <class... Args>
  constexpr iterator emplace_after(const_iterator position, Args&&... args) {
    check_size(size_ + 1);
    node_base* pos = const_cast<node_base*>(position.node_);
    node_base* n = create_node(std::forward<Args>(args)...);
    const bool inserted_at_end = pos == last_ || pos->next == nullptr;
    link_after(pos, n, n);
    if (inserted_at_end) {
      last_ = n;
    }
    ++size_;
    return iterator(n);
  }

  constexpr iterator insert_after(const_iterator position, const T& value) { return emplace_after(position, value); }
  constexpr iterator insert_after(const_iterator position, T&& value) {
    return emplace_after(position, std::move(value));
  }
  constexpr iterator insert_after(const_iterator position, size_type count, const T& value);

  template <std::input_iterator InputIt>
  constexpr iterator insert_after(const_iterator position, InputIt first, InputIt last);

  template <concepts::container_compatible_range<T> R>
  constexpr iterator insert_range_after(const_iterator position, R&& range);

  constexpr iterator insert_after(const_iterator position, std::initializer_list<T> init);

  template <class... Args>
  constexpr reference emplace_back(Args&&... args) {
    check_size(size_ + 1);
    node_base* n = create_node(std::forward<Args>(args)...);
    link_back_node(n);
    ++size_;
    return static_cast<node*>(last_)->value;
  }

  constexpr void push_back(const T& value) { emplace_back(value); }
  constexpr void push_back(T&& value) { emplace_back(std::move(value)); }

  template <concepts::container_compatible_range<T> R>
  constexpr void append_range(R&& range);

  constexpr iterator erase_after(const_iterator position) noexcept;
  constexpr iterator erase_after(const_iterator first, const_iterator last) noexcept;

  constexpr void swap(forward_list& other) noexcept(meta::is_nothrow_container_swappable_v<allocator_type>) {
    using std::swap;
    if constexpr (meta::propagate_on_container_swap_v<allocator_type>) {
      swap(allocator_, other.allocator_);
      swap(node_allocator_, other.node_allocator_);
    }
    swap(before_begin_.next, other.before_begin_.next);
    swap(last_, other.last_);
    swap(size_, other.size_);

    // Fix up null last_ pointers: if a list is now empty, its last_ must
    // point to its own before_begin_.
    if (empty()) {
      last_ = &before_begin_;
    }
    if (other.empty()) {
      other.last_ = &other.before_begin_;
    }
  }

  constexpr void clear() noexcept { destroy_and_deallocate_nodes(); }

  // =======================================================================
  // Forward list operations
  // =======================================================================

  constexpr void splice_after(const_iterator position, forward_list& other);
  constexpr void splice_after(const_iterator position, forward_list&& other);
  constexpr void splice_after(const_iterator position, forward_list& other, const_iterator it);
  constexpr void splice_after(const_iterator position, forward_list&& other, const_iterator it);
  constexpr void splice_after(const_iterator position, forward_list& other, const_iterator first, const_iterator last);
  constexpr void splice_after(const_iterator position, forward_list&& other, const_iterator first, const_iterator last);

  constexpr size_type remove(const T& value);

  template <class UnaryPredicate>
    requires std::predicate<UnaryPredicate, T>
  constexpr size_type remove_if(UnaryPredicate pred);

  constexpr size_type unique();

  template <class BinaryPredicate>
    requires std::predicate<BinaryPredicate, T, T>
  constexpr size_type unique(BinaryPredicate binary_pred);

  constexpr void merge(forward_list& other);
  constexpr void merge(forward_list&& other);

  template <class Compare>
    requires std::strict_weak_order<Compare, T, T>
  constexpr void merge(forward_list& other, Compare comp);

  template <class Compare>
    requires std::strict_weak_order<Compare, T, T>
  constexpr void merge(forward_list&& other, Compare comp);

  constexpr void sort();

  template <class Compare>
    requires std::strict_weak_order<Compare, T, T>
  constexpr void sort(Compare comp);

  constexpr void reverse() noexcept;
};

// =============================================================================
// Deduction guides
// =============================================================================

template <std::input_iterator InputIt, class Allocator = allocator<std::iter_value_t<InputIt>>>
forward_list(InputIt, InputIt, Allocator = Allocator()) -> forward_list<std::iter_value_t<InputIt>, Allocator>;

template <class R, class Allocator = allocator<std::ranges::range_value_t<R>>>
  requires concepts::container_compatible_range<R, std::ranges::range_value_t<R>>
forward_list(std::from_range_t, R&&, Allocator = Allocator()) -> forward_list<std::ranges::range_value_t<R>, Allocator>;

// =============================================================================
// Non-member comparison
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
[[nodiscard]] constexpr bool operator==(const forward_list<T, Allocator>& lhs, const forward_list<T, Allocator>& rhs) {
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
[[nodiscard]] constexpr auto operator<=>(const forward_list<T, Allocator>& lhs, const forward_list<T, Allocator>& rhs) {
  return chaistl::default_three_way_compare(lhs, rhs);
}

// =============================================================================
// Non-member swap
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void swap(forward_list<T, Allocator>& lhs,
                    forward_list<T, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

// =============================================================================
// C++20 erase / erase_if
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator, class U>
constexpr typename forward_list<T, Allocator>::size_type erase(forward_list<T, Allocator>& c, const U& value) {
  return c.remove(value);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator, class Pred>
  requires std::predicate<Pred, T>
constexpr typename forward_list<T, Allocator>::size_type erase_if(forward_list<T, Allocator>& c, Pred pred) {
  return c.remove_if(pred);
}

// =============================================================================
// Assignment / assign implementations
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr forward_list<T, Allocator>& forward_list<T, Allocator>::operator=(const forward_list& other) {
  if (this == &other) {
    return *this;
  }
  if constexpr (meta::propagate_on_container_copy_assignment_v<allocator_type>) {
    forward_list copy(other, other.get_allocator());
    destroy_and_deallocate_nodes();
    copy_allocator_from(other);
    take_storage_from(copy);
  } else {
    forward_list copy(other, get_allocator());
    swap(copy);
  }
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr forward_list<T, Allocator>& forward_list<T, Allocator>::operator=(forward_list&& other) noexcept(
    meta::is_nothrow_container_move_assignable_v<allocator_type, T>) {
  if (this == &other) {
    return *this;
  }

  if constexpr (meta::propagate_on_container_move_assignment_v<allocator_type>) {
    destroy_and_deallocate_nodes();
    move_allocator_from(other);
    take_storage_from(other);
  } else if constexpr (meta::allocator_is_always_equal_v<allocator_type>) {
    replace_storage_from(other);
  } else if (storage_compatible_with(other)) {
    replace_storage_from(other);
  } else {
    assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
  }
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr forward_list<T, Allocator>& forward_list<T, Allocator>::operator=(std::initializer_list<T> init) {
  assign(init.begin(), init.end());
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr void forward_list<T, Allocator>::assign(InputIt first, InputIt last) {
  iterator current = begin();
  iterator finish = end();

  // Reuse existing nodes while both ranges have elements.
  for (; current != finish && first != last; ++current, ++first) {
    *current = *first;
  }

  if (first == last) {
    // Input exhausted: erase remaining elements after the last reused node.
    if (current == begin()) {
      clear();
    } else {
      // current it points to the first node we want to erase.
      // We need the node BEFORE current to call erase_after.
      auto prev = before_begin();
      while (std::next(prev) != current && std::next(prev) != finish) {
        ++prev;
      }
      erase_after(prev, finish);
    }
  } else {
    // Existing nodes exhausted: insert remaining at end.
    for (; first != last; ++first) {
      emplace_back(*first);
    }
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void forward_list<T, Allocator>::assign_range(R&& range) {
  assign(std::ranges::begin(range), std::ranges::end(range));
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::assign(size_type count, const T& value) {
  iterator current = begin();
  iterator finish = end();

  // Reuse existing nodes while both ranges have elements.
  for (; current != finish && count > 0; ++current, --count) {
    *current = value;
  }

  if (count == 0) {
    if (current == begin()) {
      clear();
    } else {
      auto prev = before_begin();
      while (std::next(prev) != current && std::next(prev) != finish) {
        ++prev;
      }
      erase_after(prev, finish);
    }
  } else {
    // Existing nodes exhausted: insert remaining at end.
    for (size_type i = 0; i < count; ++i) {
      emplace_back(value);
    }
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::assign(std::initializer_list<T> init) {
  assign(init.begin(), init.end());
}

// =============================================================================
// Capacity implementations
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::resize(size_type count) {
  if (count > size_) {
    check_size(count);
    const size_type old_size = size_;
    auto guard = detail::make_exception_guard([&] {
      while (size_ > old_size) {
        node_base* prev = &before_begin_;
        while (prev->next != last_ && prev->next != nullptr) {
          prev = prev->next;
        }
        erase_after(const_iterator(prev));
      }
    });
    while (size_ < count) {
      emplace_back();
    }
    guard.complete();
  } else {
    while (size_ > count) {
      // Erase the last element efficiently: iterate to find the node before last.
      // For forward_list, we need to erase_after the previous-to-last node.
      if (size_ == 1) {
        pop_front();
      } else {
        // Find node before last_.
        node_base* prev = &before_begin_;
        while (prev->next != last_ && prev->next != nullptr) {
          prev = prev->next;
        }
        // erase_after(prev) removes prev->next (which is last_).
        erase_after(const_iterator(prev));
      }
    }
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::resize(size_type count, const T& value) {
  if (count > size_) {
    check_size(count);
    const size_type old_size = size_;
    auto guard = detail::make_exception_guard([&] {
      while (size_ > old_size) {
        node_base* prev = &before_begin_;
        while (prev->next != last_ && prev->next != nullptr) {
          prev = prev->next;
        }
        erase_after(const_iterator(prev));
      }
    });
    while (size_ < count) {
      emplace_back(value);
    }
    guard.complete();
  } else {
    while (size_ > count) {
      if (size_ == 1) {
        pop_front();
      } else {
        node_base* prev = &before_begin_;
        while (prev->next != last_ && prev->next != nullptr) {
          prev = prev->next;
        }
        erase_after(const_iterator(prev));
      }
    }
  }
}

// =============================================================================
// Modifier implementations
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::insert_after(
    const_iterator position, size_type count, const T& value) {
  if (count == 0) {
    return iterator(const_cast<node_base*>(position.node_));
  }

  check_size(size_ + count);

  // Create first node.
  node_base* first = create_node(value);
  node_base* chain_last = first;

  auto guard = detail::make_exception_guard([&] {
    node_base* current = first;
    while (current != nullptr) {
      node_base* next = current->next;
      destroy_node(current);
      current = next;
    }
  });

  // Temporarily chain remaining nodes via next pointer.
  for (size_type i = 1; i < count; ++i) {
    node_base* new_node = create_node(value);
    chain_last->next = new_node;
    chain_last = new_node;
  }
  chain_last->next = nullptr;

  guard.complete();

  node_base* pos = const_cast<node_base*>(position.node_);
  const bool inserted_at_end = pos == last_ || pos->next == nullptr;
  link_after(pos, first, chain_last);
  if (inserted_at_end) {
    last_ = chain_last;
  }
  size_ += count;
  return iterator(first);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::insert_after(
    const_iterator position, InputIt first, InputIt last) {
  if (first == last) {
    return iterator(const_cast<node_base*>(position.node_));
  }

  // Create first node.
  node_base* chain_first = create_node(*first);
  node_base* chain_last = chain_first;
  size_type count = 1;
  ++first;

  auto guard = detail::make_exception_guard([&] {
    node_base* current = chain_first;
    while (current != nullptr) {
      node_base* next = current->next;
      destroy_node(current);
      current = next;
    }
  });

  for (; first != last; ++first, ++count) {
    check_size(size_ + count + 1);
    node_base* new_node = create_node(*first);
    chain_last->next = new_node;
    chain_last = new_node;
  }
  chain_last->next = nullptr;

  guard.complete();

  node_base* pos = const_cast<node_base*>(position.node_);
  const bool inserted_at_end = pos == last_ || pos->next == nullptr;
  link_after(pos, chain_first, chain_last);
  if (inserted_at_end) {
    last_ = chain_last;
  }
  size_ += count;
  return iterator(chain_first);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::insert_range_after(
    const_iterator position, R&& range) {
  auto first = std::ranges::begin(range);
  auto last = std::ranges::end(range);
  return insert_after(position, first, last);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::insert_after(
    const_iterator position, std::initializer_list<T> init) {
  return insert_after(position, init.begin(), init.end());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void forward_list<T, Allocator>::prepend_range(R&& range) {
  // Prepend in reverse order to maintain range order at the front.
  // Collect elements first and insert in reverse.
  auto first = std::ranges::begin(range);
  auto last = std::ranges::end(range);
  if (first == last) {
    return;
  }
  // Build with this allocator; splice_after transfers nodes without rebinding.
  forward_list temp(get_allocator());
  for (auto&& value : range) {
    temp.emplace_back(std::forward<decltype(value)>(value));
  }
  splice_after(cbefore_begin(), std::move(temp));
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void forward_list<T, Allocator>::append_range(R&& range) {
  for (auto&& value : range) {
    emplace_back(std::forward<decltype(value)>(value));
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::erase_after(
    const_iterator position) noexcept {
  node_base* pos = const_cast<node_base*>(position.node_);
  node_base* target = pos->next;
  if (target == nullptr) {
    return end();
  }
  node_base* next = unlink_after(pos, target);
  if (target == last_) {
    last_ = pos;
  }
  --size_;
  destroy_node(target);
  return iterator(next);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::erase_after(
    const_iterator first, const_iterator last) noexcept {
  if (first == last) {
    return iterator(const_cast<node_base*>(last.node_));
  }

  node_base* pos = const_cast<node_base*>(first.node_);
  node_base* last_node = const_cast<node_base*>(last.node_);

  // The range to erase is [pos->next, last_node] (inclusive? No,
  // erase_after(first, last) erases (first, last) — i.e., from the element
  // after first up to but NOT including the element pointed to by last.
  // So the last element to erase is the one BEFORE last_node.
  //
  // We need to find the node before last_node in the chain [pos->next, last_node).
  node_base* erase_last = pos;
  while (erase_last->next != last_node && erase_last->next != nullptr) {
    erase_last = erase_last->next;
  }

  if (erase_last == pos) {
    // Nothing to erase.
    return iterator(last_node);
  }

  // Unlink [pos->next, erase_last] and connect pos->next = last_node.
  node_base* range_first = pos->next;
  pos->next = last_node;

  // Destroy and deallocate.
  node_base* current = range_first;
  while (current != last_node) {
    node_base* next = current->next;
    destroy_node(current);
    current = next;
    --size_;
  }

  // Update last_ if we erased the tail.
  if (last_node == nullptr || pos->next == nullptr) {
    last_ = pos;
  }

  return iterator(last_node);
}

// =============================================================================
// Forward list operation implementations
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::splice_after(const_iterator position, forward_list& other) {
  if (!other.empty()) {
    node_base* pos = const_cast<node_base*>(position.node_);
    node_base* f = other.before_begin_.next;
    node_base* l = other.last_;

    // Unlink from other.
    other.before_begin_.next = nullptr;
    other.last_ = &other.before_begin_;

    // Link into this after pos.
    if (pos == last_ || pos->next == nullptr) {
      last_ = l;
    }
    link_after(pos, f, l);
    size_ += std::exchange(other.size_, 0);
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::splice_after(const_iterator position, forward_list&& other) {
  splice_after(position, other);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::splice_after(const_iterator position,
                                                        forward_list& other,
                                                        const_iterator it) {
  // Move the element after `it` from other to after `position`.
  node_base* src = const_cast<node_base*>(it.node_);
  node_base* target = src->next;
  if (target == nullptr) {
    return;  // nothing after it
  }

  node_base* pos = const_cast<node_base*>(position.node_);

  // Unlink target from other.
  unlink_after(src, target);
  --other.size_;

  // If target was other's last, update other.last_.
  if (target == other.last_) {
    other.last_ = src;
  }

  // Link target into this after pos.
  target->next = pos->next;
  pos->next = target;
  if (pos == last_ || pos->next == target) {
    // If pos was the last node, or target is now at the end (pos->next == target
    // and target->next was null), update last_.
    if (target->next == nullptr) {
      last_ = target;
    }
  }
  ++size_;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::splice_after(const_iterator position,
                                                        forward_list&& other,
                                                        const_iterator it) {
  splice_after(position, other, it);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::splice_after(const_iterator position,
                                                        forward_list& other,
                                                        const_iterator first,
                                                        const_iterator last) {
  if (first == last) {
    return;
  }

  node_base* src_first = const_cast<node_base*>(first.node_);
  node_base* src_last = const_cast<node_base*>(last.node_);

  // The range to splice is (first, last) — elements AFTER first, up to but
  // NOT including last.  We need the last node of the range.
  node_base* range_first = src_first->next;
  if (range_first == src_last) {
    return;  // empty range
  }

  // Find the node before src_last.
  node_base* range_last = src_first;
  while (range_last->next != src_last && range_last->next != nullptr) {
    range_last = range_last->next;
  }

  // Count elements being moved.
  size_type count = 0;
  if (this != &other) {
    node_base* c = range_first;
    while (c != src_last) {
      ++count;
      c = c->next;
    }
    other.size_ -= count;
    size_ += count;
    // Update other.last_ if we've removed its tail.
    if (src_last == nullptr || range_last->next == nullptr) {
      // We removed everything up to the end of other.
      other.last_ = src_first;
    }
  }

  // Unlink.
  src_first->next = src_last;

  // Link into this after pos.
  node_base* pos = const_cast<node_base*>(position.node_);
  if (src_last == nullptr || pos == last_ || pos->next == nullptr) {
    // We're inserting at the end of this list.
    if (pos == last_) {
      last_ = range_last;
    }
    range_last->next = pos->next;
    pos->next = range_first;
    if (range_last->next == nullptr) {
      last_ = range_last;
    }
  } else {
    link_after(pos, range_first, range_last);
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::splice_after(const_iterator position,
                                                        forward_list&& other,
                                                        const_iterator first,
                                                        const_iterator last) {
  splice_after(position, other, first, last);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename forward_list<T, Allocator>::size_type forward_list<T, Allocator>::remove(const T& value) {
  return remove_if([&value](const T& v) {
    return v == value;
  });
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class UnaryPredicate>
  requires std::predicate<UnaryPredicate, T>
constexpr typename forward_list<T, Allocator>::size_type forward_list<T, Allocator>::remove_if(UnaryPredicate pred) {
  size_type count = 0;
  node_base* prev = &before_begin_;
  node_base* current = before_begin_.next;
  while (current != nullptr) {
    if (pred(static_cast<node*>(current)->value)) {
      node_base* next = current->next;
      unlink_after(prev, current);
      destroy_node(current);
      --size_;
      ++count;
      current = next;
    } else {
      prev = current;
      current = current->next;
    }
  }
  last_ = prev;
  return count;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename forward_list<T, Allocator>::size_type forward_list<T, Allocator>::unique() {
  return unique(std::equal_to<T>{});
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class BinaryPredicate>
  requires std::predicate<BinaryPredicate, T, T>
constexpr typename forward_list<T, Allocator>::size_type forward_list<T, Allocator>::unique(
    BinaryPredicate binary_pred) {
  if (size_ < 2) {
    return 0;
  }
  size_type count = 0;
  node_base* prev = before_begin_.next;
  while (prev != nullptr && prev->next != nullptr) {
    node_base* current = prev->next;
    if (binary_pred(static_cast<node*>(prev)->value, static_cast<node*>(current)->value)) {
      // Remove current.
      unlink_after(prev, current);
      destroy_node(current);
      --size_;
      ++count;
    } else {
      prev = prev->next;
    }
  }
  last_ = prev != nullptr ? prev : &before_begin_;
  return count;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::merge(forward_list& other) {
  merge(other, std::less<T>{});
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::merge(forward_list&& other) {
  merge(other);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class Compare>
  requires std::strict_weak_order<Compare, T, T>
constexpr void forward_list<T, Allocator>::merge(forward_list& other, Compare comp) {
  if (this == &other) {
    return;
  }
  if (other.empty()) {
    return;
  }
  if (empty()) {
    take_storage_from(other);
    return;
  }

  node_base* p1 = &before_begin_;
  node_base* p2 = &other.before_begin_;

  while (p1->next != nullptr && p2->next != nullptr) {
    if (comp(static_cast<node*>(p2->next)->value, static_cast<node*>(p1->next)->value)) {
      // Move p2->next to after p1.
      node_base* moved = p2->next;
      p2->next = moved->next;
      moved->next = p1->next;
      p1->next = moved;
      if (moved->next == nullptr) {
        last_ = moved;
      }
      if (p2->next == nullptr) {
        other.last_ = p2;
      }
      --other.size_;
      ++size_;
    }
    p1 = p1->next;
  }

  // Append remaining elements from other.
  if (p2->next != nullptr) {
    node_base* rest = p2->next;
    p2->next = nullptr;
    last_->next = rest;
    last_ = other.last_;
    size_ += other.size_;
    other.size_ = 0;
    other.last_ = &other.before_begin_;
  }
  other.size_ = 0;
  other.last_ = &other.before_begin_;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class Compare>
  requires std::strict_weak_order<Compare, T, T>
constexpr void forward_list<T, Allocator>::merge(forward_list&& other, Compare comp) {
  merge(other, comp);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::sort() {
  sort(std::less<T>{});
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class Compare>
  requires std::strict_weak_order<Compare, T, T>
constexpr void forward_list<T, Allocator>::sort(Compare comp) {
  if (size_ < 2) {
    return;
  }

  // Bottom-up merge sort using a counter array.
  // counter[i] stores a sorted list of size 2^i.
  forward_list carry;
  std::array<forward_list, 64> counter{};
  int fill = 0;

  while (!empty()) {
    // Move first element to carry.
    carry.splice_after(carry.cbefore_begin(), *this, cbefore_begin());

    int i = 0;
    while (i < fill && !counter[i].empty()) {
      counter[i].merge(carry, comp);
      carry.swap(counter[i]);
      ++i;
    }
    carry.swap(counter[i]);
    if (i == fill) {
      ++fill;
    }
  }

  for (int i = 1; i < fill; ++i) {
    counter[i].merge(counter[i - 1], comp);
  }
  swap(counter[fill - 1]);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void forward_list<T, Allocator>::reverse() noexcept {
  if (size_ < 2) {
    return;
  }
  node_base* prev = nullptr;
  node_base* current = before_begin_.next;
  last_ = current;  // old first becomes new last
  while (current != nullptr) {
    node_base* next = current->next;
    current->next = prev;
    prev = current;
    current = next;
  }
  before_begin_.next = prev;
}

namespace pmr {

template <concepts::container_element T>
using forward_list = chaistl::forward_list<T, chaistl::pmr::polymorphic_allocator<T>>;

}  // namespace pmr

}  // namespace chaistl
