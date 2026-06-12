// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// list - Doubly-linked sequence container
// ============================================================================
//
// Architecture:
//   - Stores each element in an individually allocated node with prev/next
//     links.
//   - A self-referential header sentinel represents both before-begin and
//     past-the-end, making empty and non-empty splice/erase cases uniform.
//   - The container tracks size explicitly; modern std::list also requires
//     constant-time size().
//
// Standardization archaeology:
//   - list was part of C++98's original sequence-container set, occupying the
//     stable-iterator, constant-time splice/insert/erase point of the design
//     space.
//   - LWG 120 clarified that list::splice does not invalidate iterators and
//     references to transferred elements; they keep referring to the same
//     nodes now owned by the destination list.
//   - C++11 move semantics and allocator propagation made node ownership
//     rules more explicit, while C++17 node handles did that work for the
//     associative containers instead of list.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/list
//   - cppreference: https://en.cppreference.com/w/cpp/container/list
//   - LWG defects: https://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html

#include <chaistl/containers/sequence/detail/common.hpp>
#include <chaistl/iterator/interface.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>
#include <chaistl/memory/detail/storage/node_owner.hpp>
#include <chaistl/memory_resource.hpp>
#include <chaistl/utility/hardening.hpp>

#include <array>
#include <compare>
#include <utility>

namespace chaistl {

/**
 * @brief Doubly-linked list container.
 *
 * @ingroup containers_sequence
 *
 * Stores elements in nodes linked by prev/next pointers.
 * Supports O(1) insertion and removal at any position.
 *
 * Storage model (visible for learning):
 * - `header_`        : sentinel node; header_.next == first element,
 *                      header_.prev == last element
 * - `size_`          : number of live elements
 *
 * References:
 *
 * - C++ Draft Standard: https://eel.is/c++draft/list
 * - cppreference: https://en.cppreference.com/w/cpp/container/list
 */
template <concepts::container_element T, concepts::allocator_for<T> Allocator = allocator<T>>
class list {
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
    node_base* prev;
    node_base* next;

    constexpr void init_self_referential() noexcept {
      prev = this;
      next = this;
    }
  };

  struct node : node_base {
    union {
      T value;
    };

    constexpr node() noexcept : node_base{nullptr, nullptr} {}
    constexpr ~node() {}
  };

  // =======================================================================
  // Iterator implementation
  // =======================================================================

  template <bool Const>
  class iterator_impl : public chaistl::iterator_interface {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using iterator_concept = std::bidirectional_iterator_tag;
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

    constexpr iterator_impl& operator--() noexcept {
      node_ = node_->prev;
      return *this;
    }

    using iterator_interface::operator++;
    using iterator_interface::operator--;

    template <bool OtherConst>
    [[nodiscard]] friend constexpr bool operator==(const iterator_impl& lhs,
                                                   const iterator_impl<OtherConst>& rhs) noexcept {
      return lhs.node_ == rhs.node_;
    }

   private:
    template <bool>
    friend class iterator_impl;
    friend class list;

    using stored_node_ptr = std::conditional_t<Const, const node_base*, node_base*>;
    stored_node_ptr node_{};

    constexpr explicit iterator_impl(stored_node_ptr node) noexcept : node_(node) {}
  };

  using iterator = iterator_impl<false>;
  using const_iterator = iterator_impl<true>;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;

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
  node_base header_;
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
  // Link / unlink helpers (operate on closed intervals [first, last])
  // =======================================================================

  // Link nodes [first, last] just before position.
  static constexpr void link_nodes(node_base* position, node_base* first, node_base* last) noexcept {
    first->prev = position->prev;
    last->next = position;
    position->prev->next = first;
    position->prev = last;
  }

  // Unlink nodes [first, last] from their current list.
  static constexpr void unlink_nodes(node_base* first, node_base* last) noexcept {
    first->prev->next = last->next;
    last->next->prev = first->prev;
  }

  // =======================================================================
  // Storage ownership helpers
  // =======================================================================
  constexpr void destroy_and_deallocate_nodes() noexcept {
    node_base* current = header_.next;
    while (current != &header_) {
      node_base* next = current->next;
      destroy_node(current);
      current = next;
    }
    size_ = 0;
    header_.init_self_referential();
  }

  constexpr void take_storage_from(list& other) noexcept {
    if (other.empty()) {
      header_.init_self_referential();
    } else {
      header_.next = other.header_.next;
      header_.prev = other.header_.prev;
      header_.next->prev = &header_;
      header_.prev->next = &header_;
    }
    size_ = std::exchange(other.size_, 0);
    other.header_.init_self_referential();
  }

  constexpr void replace_storage_from(list& other) noexcept {
    destroy_and_deallocate_nodes();
    take_storage_from(other);
  }

  [[nodiscard]] constexpr bool storage_compatible_with(const list& other) const {
    return allocator_ == other.allocator_;
  }

  constexpr void copy_allocator_from(const list& other) {
    allocator_ = other.allocator_;
    node_allocator_ = node_allocator_type(allocator_);
  }

  constexpr void move_allocator_from(list& other) {
    allocator_ = std::move(other.allocator_);
    node_allocator_ = node_allocator_type(allocator_);
  }

  constexpr void check_size(size_type new_size) const {
    if (new_size > max_size()) {
      throw std::length_error("chaistl::list exceeds max_size");
    }
  }

 public:
  // =======================================================================
  // Constructors and assignment
  // =======================================================================

  constexpr list() noexcept(noexcept(allocator_type{})) { header_.init_self_referential(); }

  explicit constexpr list(const allocator_type& alloc) noexcept : allocator_(alloc), node_allocator_(allocator_) {
    header_.init_self_referential();
  }

  explicit constexpr list(size_type count, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), node_allocator_(allocator_) {
    header_.init_self_referential();
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (size_type i = 0; i < count; ++i) {
      node_base* n = create_node();
      link_nodes(&header_, n, n);
      ++size_;
    }
    guard.complete();
  }

  constexpr list(size_type count, const T& value, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), node_allocator_(allocator_) {
    header_.init_self_referential();
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (size_type i = 0; i < count; ++i) {
      node_base* n = create_node(value);
      link_nodes(&header_, n, n);
      ++size_;
    }
    guard.complete();
  }

  template <std::input_iterator InputIt>
  constexpr list(InputIt first, InputIt last, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), node_allocator_(allocator_) {
    header_.init_self_referential();
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (; first != last; ++first) {
      node_base* n = create_node(*first);
      link_nodes(&header_, n, n);
      ++size_;
    }
    guard.complete();
  }

  constexpr list(std::initializer_list<T> init, const allocator_type& alloc = allocator_type{})
      : list(init.begin(), init.end(), alloc) {}

  constexpr list(const list& other)
      : allocator_(allocator_traits::select_on_container_copy_construction(other.allocator_)),
        node_allocator_(allocator_) {
    header_.init_self_referential();
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (const auto& value : other) {
      node_base* n = create_node(value);
      link_nodes(&header_, n, n);
      ++size_;
    }
    guard.complete();
  }

  constexpr list(list&& other) noexcept
      : allocator_(std::move(other.allocator_)), node_allocator_(std::move(other.node_allocator_)) {
    header_.init_self_referential();
    take_storage_from(other);
  }

  constexpr list(const list& other, const std::type_identity_t<Allocator>& alloc)
      : allocator_(alloc), node_allocator_(allocator_) {
    header_.init_self_referential();
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (const auto& value : other) {
      node_base* n = create_node(value);
      link_nodes(&header_, n, n);
      ++size_;
    }
    guard.complete();
  }

  constexpr list(list&& other, const std::type_identity_t<Allocator>& alloc)
      : allocator_(alloc), node_allocator_(allocator_) {
    header_.init_self_referential();
    if (storage_compatible_with(other)) {
      take_storage_from(other);
    } else {
      auto guard = detail::make_exception_guard([&] {
        destroy_and_deallocate_nodes();
      });
      for (auto& value : other) {
        node_base* n = create_node(std::move(value));
        link_nodes(&header_, n, n);
        ++size_;
      }
      guard.complete();
    }
  }

  template <concepts::container_compatible_range<T> R>
  constexpr list(std::from_range_t, R&& range, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), node_allocator_(allocator_) {
    header_.init_self_referential();
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_nodes();
    });
    for (auto&& value : range) {
      node_base* n = create_node(std::forward<decltype(value)>(value));
      link_nodes(&header_, n, n);
      ++size_;
    }
    guard.complete();
  }

  constexpr ~list() { destroy_and_deallocate_nodes(); }

  constexpr list& operator=(const list& other);

  constexpr list& operator=(list&& other) noexcept(meta::is_nothrow_container_move_assignable_v<allocator_type, T>);

  constexpr list& operator=(std::initializer_list<T> init);

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
    CHAI_HARDENED(!empty(), "chaistl::list::front: empty container");
    return *begin();
  }
  [[nodiscard]] constexpr const_reference front() const noexcept {
    CHAI_HARDENED(!empty(), "chaistl::list::front: empty container");
    return *begin();
  }
  [[nodiscard]] constexpr reference back() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::list::back: empty container");
    return *std::prev(end());
  }
  [[nodiscard]] constexpr const_reference back() const noexcept {
    CHAI_HARDENED(!empty(), "chaistl::list::back: empty container");
    return *std::prev(end());
  }

  // =======================================================================
  // Iterators
  // =======================================================================

  [[nodiscard]] constexpr iterator begin() noexcept { return iterator(header_.next); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(header_.next); }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

  [[nodiscard]] constexpr iterator end() noexcept { return iterator(&header_); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(&header_); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

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
    link_nodes(header_.next, n, n);
    ++size_;
    return front();
  }

  constexpr void push_front(const T& value) { emplace_front(value); }
  constexpr void push_front(T&& value) { emplace_front(std::move(value)); }

  template <concepts::container_compatible_range<T> R>
  constexpr void prepend_range(R&& range);

  constexpr void pop_front() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::list::pop_front: empty container");
    node_base* n = header_.next;
    unlink_nodes(n, n);
    --size_;
    destroy_node(n);
  }

  template <class... Args>
  constexpr reference emplace_back(Args&&... args) {
    check_size(size_ + 1);
    node_base* n = create_node(std::forward<Args>(args)...);
    link_nodes(&header_, n, n);
    ++size_;
    return back();
  }

  constexpr void push_back(const T& value) { emplace_back(value); }
  constexpr void push_back(T&& value) { emplace_back(std::move(value)); }

  template <concepts::container_compatible_range<T> R>
  constexpr void append_range(R&& range);

  constexpr void pop_back() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::list::pop_back: empty container");
    node_base* n = header_.prev;
    unlink_nodes(n, n);
    --size_;
    destroy_node(n);
  }

  template <class... Args>
  constexpr iterator emplace(const_iterator position, Args&&... args) {
    check_size(size_ + 1);
    node_base* n = create_node(std::forward<Args>(args)...);
    link_nodes(const_cast<node_base*>(position.node_), n, n);
    ++size_;
    return iterator(n);
  }

  constexpr iterator insert(const_iterator position, const T& value) { return emplace(position, value); }
  constexpr iterator insert(const_iterator position, T&& value) { return emplace(position, std::move(value)); }
  constexpr iterator insert(const_iterator position, size_type count, const T& value);

  template <std::input_iterator InputIt>
  constexpr iterator insert(const_iterator position, InputIt first, InputIt last);

  template <concepts::container_compatible_range<T> R>
  constexpr iterator insert_range(const_iterator position, R&& range);

  constexpr iterator insert(const_iterator position, std::initializer_list<T> init);

  constexpr iterator erase(const_iterator position) noexcept;
  constexpr iterator erase(const_iterator first, const_iterator last) noexcept;

  constexpr void swap(list& other) noexcept(meta::is_nothrow_container_swappable_v<allocator_type>) {
    using std::swap;
    if constexpr (meta::propagate_on_container_swap_v<allocator_type>) {
      swap(allocator_, other.allocator_);
      swap(node_allocator_, other.node_allocator_);
    }
    // Swap the circular links by redirecting neighbors.
    if (empty()) {
      if (!other.empty()) {
        header_.next = other.header_.next;
        header_.prev = other.header_.prev;
        header_.next->prev = &header_;
        header_.prev->next = &header_;
        other.header_.init_self_referential();
      }
    } else if (other.empty()) {
      other.header_.next = header_.next;
      other.header_.prev = header_.prev;
      other.header_.next->prev = &other.header_;
      other.header_.prev->next = &other.header_;
      header_.init_self_referential();
    } else {
      node_base* this_first = header_.next;
      node_base* this_last = header_.prev;
      node_base* other_first = other.header_.next;
      node_base* other_last = other.header_.prev;

      header_.next = other_first;
      header_.prev = other_last;
      other.header_.next = this_first;
      other.header_.prev = this_last;

      header_.next->prev = &header_;
      header_.prev->next = &header_;
      other.header_.next->prev = &other.header_;
      other.header_.prev->next = &other.header_;
    }
    swap(size_, other.size_);
  }

  constexpr void clear() noexcept { destroy_and_deallocate_nodes(); }

  // =======================================================================
  // List operations
  // =======================================================================

  constexpr void splice(const_iterator position, list& other);
  constexpr void splice(const_iterator position, list&& other);
  constexpr void splice(const_iterator position, list& other, const_iterator it);
  constexpr void splice(const_iterator position, list&& other, const_iterator it);
  constexpr void splice(const_iterator position, list& other, const_iterator first, const_iterator last);
  constexpr void splice(const_iterator position, list&& other, const_iterator first, const_iterator last);

  constexpr size_type remove(const T& value);

  template <class UnaryPredicate>
    requires std::predicate<UnaryPredicate, T>
  constexpr size_type remove_if(UnaryPredicate pred);

  constexpr size_type unique();

  template <class BinaryPredicate>
    requires std::predicate<BinaryPredicate, T, T>
  constexpr size_type unique(BinaryPredicate binary_pred);

  constexpr void merge(list& other);
  constexpr void merge(list&& other);

  template <class Compare>
    requires std::strict_weak_order<Compare, T, T>
  constexpr void merge(list& other, Compare comp);

  template <class Compare>
    requires std::strict_weak_order<Compare, T, T>
  constexpr void merge(list&& other, Compare comp);

  constexpr void sort();

  template <class Compare>
    requires std::strict_weak_order<Compare, T, T>
  constexpr void sort(Compare comp);

  constexpr void reverse() noexcept;
};

// =============================================================================
// Non-member comparison
// =============================================================================

// =============================================================================
// Deduction guides
// =============================================================================

template <std::input_iterator InputIt, class Allocator = allocator<std::iter_value_t<InputIt>>>
list(InputIt, InputIt, Allocator = Allocator()) -> list<std::iter_value_t<InputIt>, Allocator>;

template <class R, class Allocator = allocator<std::ranges::range_value_t<R>>>
  requires concepts::container_compatible_range<R, std::ranges::range_value_t<R>>
list(std::from_range_t, R&&, Allocator = Allocator()) -> list<std::ranges::range_value_t<R>, Allocator>;

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
[[nodiscard]] constexpr bool operator==(const list<T, Allocator>& lhs, const list<T, Allocator>& rhs) {
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
[[nodiscard]] constexpr auto operator<=>(const list<T, Allocator>& lhs, const list<T, Allocator>& rhs) {
  return chaistl::default_three_way_compare(lhs, rhs);
}

// =============================================================================
// Non-member swap
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void swap(list<T, Allocator>& lhs, list<T, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

// =============================================================================
// C++20 erase / erase_if
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator, class U>
constexpr typename list<T, Allocator>::size_type erase(list<T, Allocator>& c, const U& value) {
  return c.remove(value);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator, class Pred>
  requires std::predicate<Pred, T>
constexpr typename list<T, Allocator>::size_type erase_if(list<T, Allocator>& c, Pred pred) {
  return c.remove_if(pred);
}

// =============================================================================
// Assignment / assign implementations
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr list<T, Allocator>& list<T, Allocator>::operator=(const list& other) {
  if (this == &other) {
    return *this;
  }
  if constexpr (meta::propagate_on_container_copy_assignment_v<allocator_type>) {
    list copy(other, other.get_allocator());
    destroy_and_deallocate_nodes();
    copy_allocator_from(other);
    take_storage_from(copy);
  } else {
    list copy(other, get_allocator());
    swap(copy);
  }
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr list<T, Allocator>& list<T, Allocator>::operator=(list&& other) noexcept(
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
constexpr list<T, Allocator>& list<T, Allocator>::operator=(std::initializer_list<T> init) {
  assign(init.begin(), init.end());
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr void list<T, Allocator>::assign(InputIt first, InputIt last) {
  iterator current = begin();
  iterator finish = end();

  // Reuse existing nodes while both ranges have elements.
  for (; current != finish && first != last; ++current, ++first) {
    *current = *first;
  }

  if (first == last) {
    // Input exhausted: erase remaining elements [current, finish).
    erase(current, finish);
  } else {
    // Existing nodes exhausted: insert remaining elements [first, last) at end.
    insert(finish, first, last);
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void list<T, Allocator>::assign_range(R&& range) {
  assign(std::ranges::begin(range), std::ranges::end(range));
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::assign(size_type count, const T& value) {
  iterator current = begin();
  iterator finish = end();

  // Reuse existing nodes while both ranges have elements.
  for (; current != finish && count > 0; ++current, --count) {
    *current = value;
  }

  if (count == 0) {
    // Input exhausted: erase remaining elements [current, finish).
    erase(current, finish);
  } else {
    // Existing nodes exhausted: insert remaining elements at end.
    insert(finish, count, value);
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::assign(std::initializer_list<T> init) {
  assign(init.begin(), init.end());
}

// =============================================================================
// Capacity implementations
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::resize(size_type count) {
  if (count > size_) {
    check_size(count);
    auto guard = detail::make_exception_guard([&] {
      while (size_ > count) {
        pop_back();
      }
    });
    while (size_ < count) {
      node_base* n = create_node();
      link_nodes(&header_, n, n);
      ++size_;
    }
    guard.complete();
  } else {
    while (size_ > count) {
      pop_back();
    }
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::resize(size_type count, const T& value) {
  if (count > size_) {
    check_size(count);
    auto guard = detail::make_exception_guard([&] {
      while (size_ > count) {
        pop_back();
      }
    });
    while (size_ < count) {
      node_base* n = create_node(value);
      link_nodes(&header_, n, n);
      ++size_;
    }
    guard.complete();
  } else {
    while (size_ > count) {
      pop_back();
    }
  }
}

// =============================================================================
// Modifier implementations
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename list<T, Allocator>::iterator list<T, Allocator>::insert(const_iterator position,
                                                                           size_type count,
                                                                           const T& value) {
  if (count == 0) {
    return iterator(const_cast<node_base*>(position.node_));
  }

  check_size(size_ + count);

  // Create first node.
  node_base* first = create_node(value);
  node_base* last = first;

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
    last->next = new_node;
    last = new_node;
  }
  last->next = nullptr;

  guard.complete();

  // Link into list: [first, last] before position.
  link_nodes(const_cast<node_base*>(position.node_), first, last);
  size_ += count;
  return iterator(first);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr typename list<T, Allocator>::iterator list<T, Allocator>::insert(const_iterator position,
                                                                           InputIt first,
                                                                           InputIt last) {
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

  link_nodes(const_cast<node_base*>(position.node_), chain_first, chain_last);
  size_ += count;
  return iterator(chain_first);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr typename list<T, Allocator>::iterator list<T, Allocator>::insert_range(const_iterator position, R&& range) {
  auto first = std::ranges::begin(range);
  auto last = std::ranges::end(range);
  return insert(position, first, last);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename list<T, Allocator>::iterator list<T, Allocator>::insert(const_iterator position,
                                                                           std::initializer_list<T> init) {
  return insert(position, init.begin(), init.end());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void list<T, Allocator>::prepend_range(R&& range) {
  for (auto&& value : range) {
    emplace_front(std::forward<decltype(value)>(value));
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void list<T, Allocator>::append_range(R&& range) {
  for (auto&& value : range) {
    emplace_back(std::forward<decltype(value)>(value));
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename list<T, Allocator>::iterator list<T, Allocator>::erase(const_iterator position) noexcept {
  node_base* node = const_cast<node_base*>(position.node_);
  node_base* next = node->next;
  unlink_nodes(node, node);
  --size_;
  destroy_node(node);
  return iterator(next);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename list<T, Allocator>::iterator list<T, Allocator>::erase(const_iterator first,
                                                                          const_iterator last) noexcept {
  if (first == last) {
    return iterator(const_cast<node_base*>(first.node_));
  }
  node_base* f = const_cast<node_base*>(first.node_);
  node_base* l = const_cast<node_base*>(last.node_);
  node_base* l_prev = l->prev;

  // Unlink the closed interval [f, l_prev].
  unlink_nodes(f, l_prev);

  // Destroy and deallocate.
  node_base* current = f;
  while (current != l) {
    node_base* next = current->next;
    destroy_node(current);
    current = next;
    --size_;
  }

  return iterator(l);
}

// =============================================================================
// List operation implementations
// =============================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::splice(const_iterator position, list& other) {
  if (!other.empty()) {
    node_base* f = other.header_.next;
    node_base* l = other.header_.prev;
    unlink_nodes(f, l);
    link_nodes(const_cast<node_base*>(position.node_), f, l);
    size_ += other.size_;
    other.size_ = 0;
    other.header_.init_self_referential();
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::splice(const_iterator position, list&& other) {
  splice(position, other);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::splice(const_iterator position, list& other, const_iterator it) {
  if (position.node_ != it.node_ && position.node_ != it.node_->next) {
    node_base* node = const_cast<node_base*>(it.node_);
    unlink_nodes(node, node);
    --other.size_;
    link_nodes(const_cast<node_base*>(position.node_), node, node);
    ++size_;
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::splice(const_iterator position, list&& other, const_iterator it) {
  splice(position, other, it);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::splice(const_iterator position,
                                          list& other,
                                          const_iterator first,
                                          const_iterator last) {
  if (first == last) {
    return;
  }
  node_base* f = const_cast<node_base*>(first.node_);
  node_base* l = const_cast<node_base*>(last.node_);
  node_base* l_prev = l->prev;

  if (this != &other) {
    size_type count = static_cast<size_type>(std::distance(first, last));
    other.size_ -= count;
    size_ += count;
  }

  unlink_nodes(f, l_prev);
  link_nodes(const_cast<node_base*>(position.node_), f, l_prev);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::splice(const_iterator position,
                                          list&& other,
                                          const_iterator first,
                                          const_iterator last) {
  splice(position, other, first, last);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename list<T, Allocator>::size_type list<T, Allocator>::remove(const T& value) {
  return remove_if([&value](const T& v) {
    return v == value;
  });
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class UnaryPredicate>
  requires std::predicate<UnaryPredicate, T>
constexpr typename list<T, Allocator>::size_type list<T, Allocator>::remove_if(UnaryPredicate pred) {
  size_type count = 0;
  iterator it = begin();
  while (it != end()) {
    if (pred(*it)) {
      it = erase(it);
      ++count;
    } else {
      ++it;
    }
  }
  return count;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename list<T, Allocator>::size_type list<T, Allocator>::unique() {
  return unique(std::equal_to<T>{});
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class BinaryPredicate>
  requires std::predicate<BinaryPredicate, T, T>
constexpr typename list<T, Allocator>::size_type list<T, Allocator>::unique(BinaryPredicate binary_pred) {
  if (size_ < 2) {
    return 0;
  }
  size_type count = 0;
  iterator it = begin();
  iterator next = std::next(it);
  while (next != end()) {
    if (binary_pred(*it, *next)) {
      next = erase(next);
      ++count;
    } else {
      ++it;
      ++next;
    }
  }
  return count;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::merge(list& other) {
  merge(other, std::less<T>{});
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::merge(list&& other) {
  merge(other);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class Compare>
  requires std::strict_weak_order<Compare, T, T>
constexpr void list<T, Allocator>::merge(list& other, Compare comp) {
  if (this == &other) {
    return;
  }
  iterator it1 = begin();
  iterator it2 = other.begin();
  while (it1 != end() && it2 != other.end()) {
    if (comp(*it2, *it1)) {
      iterator next2 = std::next(it2);
      // Find the run of elements in other that belong before *it1.
      while (next2 != other.end() && comp(*next2, *it1)) {
        ++next2;
      }
      // Splice [it2, next2) before it1.
      node_base* f = it2.node_;
      node_base* l = next2.node_;
      node_base* l_prev = l->prev;
      size_type count = static_cast<size_type>(std::distance(it2, next2));
      other.size_ -= count;
      size_ += count;
      unlink_nodes(f, l_prev);
      link_nodes(it1.node_, f, l_prev);
      it2 = next2;
    } else {
      ++it1;
    }
  }
  // Splice any remaining elements from other.
  if (!other.empty()) {
    splice(end(), other);
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class Compare>
  requires std::strict_weak_order<Compare, T, T>
constexpr void list<T, Allocator>::merge(list&& other, Compare comp) {
  merge(other, comp);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void list<T, Allocator>::sort() {
  sort(std::less<T>{});
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class Compare>
  requires std::strict_weak_order<Compare, T, T>
constexpr void list<T, Allocator>::sort(Compare comp) {
  if (size_ < 2) {
    return;
  }

  // Bottom-up merge sort using a counter array.
  // counter[i] always stores a sorted list of size 2^i.
  auto make_counter = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    return std::array<list, 64>{((void)Is, list(get_allocator()))...};
  };
  list carry(get_allocator());
  auto counter = make_counter(std::make_index_sequence<64>{});
  int fill = 0;

  while (!empty()) {
    // Move first element to carry.
    carry.splice(carry.begin(), *this, begin());

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
constexpr void list<T, Allocator>::reverse() noexcept {
  if (size_ < 2) {
    return;
  }
  node_base* current = header_.next;
  while (current != &header_) {
    std::swap(current->prev, current->next);
    current = current->prev;  // prev was original next
  }
  std::swap(header_.next, header_.prev);
}

namespace pmr {

template <concepts::container_element T>
using list = chaistl::list<T, chaistl::pmr::polymorphic_allocator<T>>;

}  // namespace pmr

}  // namespace chaistl
