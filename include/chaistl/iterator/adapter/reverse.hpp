// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/iterator.hpp>

#include <compare>
#include <concepts>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl {

// Reference:
// - https://en.cppreference.com/w/cpp/iterator/reverse_iterator
// - https://eel.is/c++draft/reverse.iterators
template <concepts::bidirectional_iterator Iterator>
class reverse_iterator {
 public:
  using iterator_type = Iterator;
  using iterator_concept = std::conditional_t<std::random_access_iterator<Iterator>,
                                              std::random_access_iterator_tag,
                                              std::bidirectional_iterator_tag>;
  using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;
  using value_type = std::iter_value_t<Iterator>;
  using difference_type = std::iter_difference_t<Iterator>;
  using pointer = typename std::iterator_traits<Iterator>::pointer;
  using reference = std::iter_reference_t<Iterator>;

  constexpr reverse_iterator() = default;

  explicit constexpr reverse_iterator(Iterator iterator) : current_(iterator) {}

  template <class Other>
    requires(!std::same_as<Other, Iterator> && std::convertible_to<const Other&, Iterator>)
  constexpr reverse_iterator(const reverse_iterator<Other>& other) : current_(other.base()) {}

  [[nodiscard]] constexpr Iterator base() const { return current_; }

  [[nodiscard]] constexpr reference operator*() const {
    Iterator previous = current_;
    return *--previous;
  }

  [[nodiscard]] constexpr auto operator->() const
    requires(std::is_pointer_v<Iterator> || requires(const Iterator& iterator) { iterator.operator->(); })
  {
    Iterator previous = current_;
    --previous;
    if constexpr (std::is_pointer_v<Iterator>) {
      return previous;
    } else {
      return previous.operator->();
    }
  }

  [[nodiscard]] constexpr reference operator[](difference_type offset) const
    requires std::random_access_iterator<Iterator>
  {
    return current_[-offset - 1];
  }

  constexpr reverse_iterator& operator++() {
    --current_;
    return *this;
  }

  constexpr reverse_iterator operator++(int) {
    auto old = *this;
    --current_;
    return old;
  }

  constexpr reverse_iterator& operator--() {
    ++current_;
    return *this;
  }

  constexpr reverse_iterator operator--(int) {
    auto old = *this;
    ++current_;
    return old;
  }

  constexpr reverse_iterator& operator+=(difference_type offset)
    requires std::random_access_iterator<Iterator>
  {
    current_ -= offset;
    return *this;
  }

  constexpr reverse_iterator& operator-=(difference_type offset)
    requires std::random_access_iterator<Iterator>
  {
    current_ += offset;
    return *this;
  }

 private:
  Iterator current_{};
};

template <class Iterator1, class Iterator2>
  requires requires(const Iterator1& lhs, const Iterator2& rhs) { lhs == rhs; }
[[nodiscard]] constexpr bool operator==(const reverse_iterator<Iterator1>& lhs,
                                        const reverse_iterator<Iterator2>& rhs) {
  return lhs.base() == rhs.base();
}

template <class Iterator1, class Iterator2>
  requires std::three_way_comparable_with<Iterator1, Iterator2>
[[nodiscard]] constexpr auto operator<=>(const reverse_iterator<Iterator1>& lhs,
                                         const reverse_iterator<Iterator2>& rhs) {
  return rhs.base() <=> lhs.base();
}

template <class Iterator>
[[nodiscard]] constexpr reverse_iterator<Iterator> operator+(reverse_iterator<Iterator> iterator,
                                                             std::iter_difference_t<Iterator> offset)
  requires std::random_access_iterator<Iterator>
{
  iterator += offset;
  return iterator;
}

template <class Iterator>
[[nodiscard]] constexpr reverse_iterator<Iterator> operator+(std::iter_difference_t<Iterator> offset,
                                                             reverse_iterator<Iterator> iterator)
  requires std::random_access_iterator<Iterator>
{
  iterator += offset;
  return iterator;
}

template <class Iterator>
[[nodiscard]] constexpr reverse_iterator<Iterator> operator-(reverse_iterator<Iterator> iterator,
                                                             std::iter_difference_t<Iterator> offset)
  requires std::random_access_iterator<Iterator>
{
  iterator -= offset;
  return iterator;
}

template <class Iterator1, class Iterator2>
  requires requires(const Iterator1& lhs, const Iterator2& rhs) { rhs - lhs; }
[[nodiscard]] constexpr auto operator-(const reverse_iterator<Iterator1>& lhs, const reverse_iterator<Iterator2>& rhs) {
  return rhs.base() - lhs.base();
}

// C++23 deduction guide for CTAD
// Reference: https://eel.is/c++draft/reverse.iter.cons
template <class Iterator>
reverse_iterator(Iterator) -> reverse_iterator<Iterator>;

template <class Iterator>
[[nodiscard]] constexpr reverse_iterator<Iterator> make_reverse_iterator(Iterator iterator) {
  return reverse_iterator<Iterator>(iterator);
}

template <class Iterator>
[[nodiscard]] constexpr decltype(auto) iter_move(const reverse_iterator<Iterator>& iterator) noexcept(
    noexcept(std::ranges::iter_move(--std::declval<Iterator&>()))) {
  Iterator previous = iterator.base();
  --previous;
  return std::ranges::iter_move(previous);
}

template <class Iterator1, class Iterator2>
constexpr void iter_swap(const reverse_iterator<Iterator1>& lhs, const reverse_iterator<Iterator2>& rhs) noexcept(
    noexcept(std::ranges::iter_swap(--std::declval<Iterator1&>(), --std::declval<Iterator2&>())))
  requires std::indirectly_swappable<Iterator1, Iterator2>
{
  Iterator1 lhs_previous = lhs.base();
  Iterator2 rhs_previous = rhs.base();
  --lhs_previous;
  --rhs_previous;
  std::ranges::iter_swap(lhs_previous, rhs_previous);
}

}  // namespace chaistl
