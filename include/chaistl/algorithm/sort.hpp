// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

namespace chaistl {

template <std::random_access_iterator RandomIt, class Compare>
constexpr void insertion_sort(RandomIt first, RandomIt last, Compare comp);

namespace detail {

template <class Compare, class T, class U>
constexpr bool sort_less(Compare& comp, T&& lhs, U&& rhs) {
  return std::invoke(comp, std::forward<T>(lhs), std::forward<U>(rhs));
}

template <std::random_access_iterator RandomIt>
constexpr auto sort_size(RandomIt first, RandomIt last) {
  return last - first;
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void median_to_middle(RandomIt first, RandomIt middle, RandomIt last_minus_one, Compare& comp) {
  if (sort_less(comp, *middle, *first)) {
    std::iter_swap(middle, first);
  }
  if (sort_less(comp, *last_minus_one, *middle)) {
    std::iter_swap(last_minus_one, middle);
    if (sort_less(comp, *middle, *first)) {
      std::iter_swap(middle, first);
    }
  }
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void median_to_first(RandomIt first, RandomIt middle, RandomIt last_minus_one, Compare& comp) {
  median_to_middle(first, middle, last_minus_one, comp);
  std::iter_swap(first, middle);
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void ninther_to_first(RandomIt first, RandomIt last, Compare& comp) {
  const auto step = (last - first) / 8;
  median_to_first(first, first + step, first + step * 2, comp);
  median_to_first(first + step * 3, first + step * 4, first + step * 5, comp);
  median_to_first(first + step * 6, first + step * 7, last - 1, comp);
  median_to_first(first, first + step * 4, first + step * 7, comp);
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr RandomIt partition_around_first(RandomIt first, RandomIt last, Compare& comp) {
  RandomIt left = first + 1;
  RandomIt right = last - 1;

  while (true) {
    while (left <= right && sort_less(comp, *left, *first)) {
      ++left;
    }
    while (left <= right && sort_less(comp, *first, *right)) {
      --right;
    }
    if (left >= right) {
      break;
    }
    std::iter_swap(left, right);
    ++left;
    --right;
  }

  std::iter_swap(first, right);
  return right;
}

template <class Compare, class T, class U>
constexpr bool sort_equivalent(Compare& comp, const T& lhs, const U& rhs) {
  return !sort_less(comp, lhs, rhs) && !sort_less(comp, rhs, lhs);
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr std::pair<RandomIt, RandomIt> partition_around_pivot(RandomIt first,
                                                               RandomIt pivot,
                                                               RandomIt last,
                                                               Compare& comp) {
  RandomIt equal_first = pivot;
  RandomIt equal_last = pivot + 1;

  while (first < equal_first && sort_equivalent(comp, *(equal_first - 1), *equal_first)) {
    --equal_first;
  }
  while (equal_last < last && sort_equivalent(comp, *equal_last, *equal_first)) {
    ++equal_last;
  }

  RandomIt greater_first = equal_last;
  RandomIt greater_last = equal_first;

  while (true) {
    while (greater_first < last) {
      if (sort_less(comp, *equal_first, *greater_first)) {
        ++greater_first;
      } else if (sort_less(comp, *greater_first, *equal_first)) {
        break;
      } else {
        if (equal_last != greater_first) {
          std::iter_swap(equal_last, greater_first);
        }
        ++equal_last;
        ++greater_first;
      }
    }

    while (first < greater_last) {
      RandomIt before_greater_last = greater_last - 1;
      if (sort_less(comp, *before_greater_last, *equal_first)) {
        --greater_last;
      } else if (sort_less(comp, *equal_first, *before_greater_last)) {
        break;
      } else {
        --equal_first;
        if (equal_first != before_greater_last) {
          std::iter_swap(equal_first, before_greater_last);
        }
        --greater_last;
      }
    }

    if (greater_last == first && greater_first == last) {
      return {equal_first, equal_last};
    }

    if (greater_last == first) {
      if (equal_last != greater_first) {
        std::iter_swap(equal_first, equal_last);
      }
      ++equal_last;
      std::iter_swap(equal_first, greater_first);
      ++equal_first;
      ++greater_first;
    } else if (greater_first == last) {
      --greater_last;
      --equal_first;
      if (greater_last != equal_first) {
        std::iter_swap(greater_last, equal_first);
      }
      --equal_last;
      std::iter_swap(equal_first, equal_last);
    } else {
      --greater_last;
      std::iter_swap(greater_first, greater_last);
      ++greater_first;
    }
  }
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr std::pair<RandomIt, bool> partition_right(RandomIt first, RandomIt last, Compare& comp) {
  auto pivot_value = std::iter_value_t<RandomIt>(std::move(*first));
  RandomIt left = first;
  RandomIt right = last;

  while (sort_less(comp, *++left, pivot_value)) {
  }

  if (left - 1 == first) {
    while (left < right && !sort_less(comp, *--right, pivot_value)) {
    }
  } else {
    while (!sort_less(comp, *--right, pivot_value)) {
    }
  }

  const bool already_partitioned = left >= right;
  while (left < right) {
    std::iter_swap(left, right);
    while (sort_less(comp, *++left, pivot_value)) {
    }
    while (!sort_less(comp, *--right, pivot_value)) {
    }
  }

  RandomIt pivot = left - 1;
  if (pivot != first) {
    *first = std::move(*pivot);
  }
  *pivot = std::move(pivot_value);
  return {pivot, already_partitioned};
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr RandomIt partition_left(RandomIt first, RandomIt last, Compare& comp) {
  auto pivot_value = std::iter_value_t<RandomIt>(std::move(*first));
  RandomIt left = first;
  RandomIt right = last;

  while (sort_less(comp, pivot_value, *--right)) {
  }

  if (right + 1 == last) {
    while (left < right && !sort_less(comp, pivot_value, *++left)) {
    }
  } else {
    while (!sort_less(comp, pivot_value, *++left)) {
    }
  }

  while (left < right) {
    std::iter_swap(left, right);
    while (sort_less(comp, pivot_value, *--right)) {
    }
    while (!sort_less(comp, pivot_value, *++left)) {
    }
  }

  if (right != first) {
    *first = std::move(*right);
  }
  *right = std::move(pivot_value);
  return right;
}

constexpr int log2_floor(std::size_t value) noexcept {
  int result = 0;
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void intro_sort_loop(RandomIt first, RandomIt last, Compare& comp, int depth_limit);

template <std::random_access_iterator RandomIt, class Compare>
constexpr bool partial_insertion_sort(RandomIt first, RandomIt last, Compare& comp) {
  constexpr int move_limit = 8;
  int moves = 0;

  for (RandomIt current = first + 1; current != last; ++current) {
    if (!sort_less(comp, *current, *(current - 1))) {
      continue;
    }
    auto value = std::iter_value_t<RandomIt>(std::move(*current));
    RandomIt hole = current;
    do {
      *hole = std::move(*(hole - 1));
      --hole;
      ++moves;
      if (moves > move_limit) {
        return false;
      }
    } while (hole != first && sort_less(comp, value, *(hole - 1)));
    *hole = std::move(value);
  }

  return true;
}

template <std::random_access_iterator RandomIt>
constexpr void break_pattern(RandomIt first, RandomIt last) {
  const auto n = last - first;
  if (n < 8) {
    return;
  }

  RandomIt middle = first + n / 2;
  std::iter_swap(first + n / 4, middle);
  std::iter_swap(middle + n / 4, last - 1);
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void pdq_sort_loop(RandomIt first, RandomIt last, Compare& comp, int bad_allowed, bool leftmost = true) {
  constexpr auto insertion_cutoff = typename std::iterator_traits<RandomIt>::difference_type{24};

  while (true) {
    const auto n = last - first;
    if (n < insertion_cutoff) {
      chaistl::insertion_sort(first, last, comp);
      return;
    }
    if (bad_allowed == 0) {
      std::make_heap(first, last, comp);
      std::sort_heap(first, last, comp);
      return;
    }

    if (n > 128) {
      ninther_to_first(first, last, comp);
    } else {
      median_to_first(first, first + n / 2, last - 1, comp);
    }

    if (!leftmost && !sort_less(comp, *(first - 1), *first)) {
      first = partition_left(first, last, comp) + 1;
      continue;
    }

    auto [pivot, already_partitioned] = partition_right(first, last, comp);
    const auto left_size = pivot - first;
    const auto right_size = last - (pivot + 1);
    const bool highly_unbalanced = left_size < n / 8 || right_size < n / 8;

    if (highly_unbalanced) {
      --bad_allowed;
      if (left_size >= insertion_cutoff) {
        std::iter_swap(first, first + left_size / 4);
        std::iter_swap(pivot - 1, pivot - left_size / 4);
        if (left_size > 128) {
          std::iter_swap(first + 1, first + left_size / 4 + 1);
          std::iter_swap(first + 2, first + left_size / 4 + 2);
          std::iter_swap(pivot - 2, pivot - left_size / 4 - 1);
          std::iter_swap(pivot - 3, pivot - left_size / 4 - 2);
        }
      }
      if (right_size >= insertion_cutoff) {
        std::iter_swap(pivot + 1, pivot + 1 + right_size / 4);
        std::iter_swap(last - 1, last - right_size / 4);
        if (right_size > 128) {
          std::iter_swap(pivot + 2, pivot + 2 + right_size / 4);
          std::iter_swap(pivot + 3, pivot + 3 + right_size / 4);
          std::iter_swap(last - 2, last - right_size / 4 - 1);
          std::iter_swap(last - 3, last - right_size / 4 - 2);
        }
      }
    } else if (already_partitioned && partial_insertion_sort(first, pivot, comp) &&
               partial_insertion_sort(pivot + 1, last, comp)) {
      return;
    }

    pdq_sort_loop(first, pivot, comp, bad_allowed, leftmost);
    first = pivot + 1;
    leftmost = false;
  }
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void intro_sort_loop(RandomIt first, RandomIt last, Compare& comp, int depth_limit) {
  constexpr auto insertion_cutoff = typename std::iterator_traits<RandomIt>::difference_type{24};

  while (last - first > insertion_cutoff) {
    if (depth_limit == 0) {
      std::make_heap(first, last, comp);
      std::sort_heap(first, last, comp);
      return;
    }
    --depth_limit;

    RandomIt middle = first + (last - first) / 2;
    median_to_middle(first, middle, last - 1, comp);
    auto equal_range = partition_around_pivot(first, middle, last, comp);

    if (equal_range.first - first < last - equal_range.second) {
      intro_sort_loop(first, equal_range.first, comp, depth_limit);
      first = equal_range.second;
    } else {
      intro_sort_loop(equal_range.second, last, comp, depth_limit);
      last = equal_range.first;
    }
  }

  chaistl::insertion_sort(first, last, comp);
}

}  // namespace detail

template <std::random_access_iterator RandomIt, class Compare>
constexpr void insertion_sort(RandomIt first, RandomIt last, Compare comp) {
  if (first == last) {
    return;
  }

  for (RandomIt current = first + 1; current != last; ++current) {
    if (!detail::sort_less(comp, *current, *(current - 1))) {
      continue;
    }
    auto value = std::iter_value_t<RandomIt>(std::move(*current));
    RandomIt hole = current;
    do {
      *hole = std::move(*(hole - 1));
      --hole;
    } while (hole != first && detail::sort_less(comp, value, *(hole - 1)));
    *hole = std::move(value);
  }
}

template <std::random_access_iterator RandomIt>
constexpr void insertion_sort(RandomIt first, RandomIt last) {
  chaistl::insertion_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void binary_insertion_sort(RandomIt first, RandomIt last, Compare comp) {
  for (RandomIt current = first + 1; current != last; ++current) {
    auto value = std::iter_value_t<RandomIt>(std::move(*current));
    RandomIt insert_at = std::upper_bound(first, current, value, comp);
    for (RandomIt hole = current; hole != insert_at; --hole) {
      *hole = std::move(*(hole - 1));
    }
    *insert_at = std::move(value);
  }
}

template <std::random_access_iterator RandomIt>
constexpr void binary_insertion_sort(RandomIt first, RandomIt last) {
  chaistl::binary_insertion_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void selection_sort(RandomIt first, RandomIt last, Compare comp) {
  for (RandomIt current = first; current != last; ++current) {
    RandomIt selected = current;
    for (RandomIt candidate = current + 1; candidate != last; ++candidate) {
      if (detail::sort_less(comp, *candidate, *selected)) {
        selected = candidate;
      }
    }
    if (selected != current) {
      std::iter_swap(current, selected);
    }
  }
}

template <std::random_access_iterator RandomIt>
constexpr void selection_sort(RandomIt first, RandomIt last) {
  chaistl::selection_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void bubble_sort(RandomIt first, RandomIt last, Compare comp) {
  for (auto unsorted = last; unsorted - first > 1; --unsorted) {
    bool swapped = false;
    for (RandomIt current = first + 1; current != unsorted; ++current) {
      if (detail::sort_less(comp, *current, *(current - 1))) {
        std::iter_swap(current, current - 1);
        swapped = true;
      }
    }
    if (!swapped) {
      return;
    }
  }
}

template <std::random_access_iterator RandomIt>
constexpr void bubble_sort(RandomIt first, RandomIt last) {
  chaistl::bubble_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void cocktail_sort(RandomIt first, RandomIt last, Compare comp) {
  if (last - first < 2) {
    return;
  }

  RandomIt left = first;
  RandomIt right = last - 1;

  while (left < right) {
    bool swapped = false;
    for (RandomIt current = left; current < right; ++current) {
      if (detail::sort_less(comp, *(current + 1), *current)) {
        std::iter_swap(current, current + 1);
        swapped = true;
      }
    }
    if (!swapped) {
      return;
    }
    --right;

    swapped = false;
    for (RandomIt current = right; current > left; --current) {
      if (detail::sort_less(comp, *current, *(current - 1))) {
        std::iter_swap(current, current - 1);
        swapped = true;
      }
    }
    if (!swapped) {
      return;
    }
    ++left;
  }
}

template <std::random_access_iterator RandomIt>
constexpr void cocktail_sort(RandomIt first, RandomIt last) {
  chaistl::cocktail_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void shell_sort(RandomIt first, RandomIt last, Compare comp) {
  const auto n = last - first;
  for (auto gap = n / 2; gap > 0; gap /= 2) {
    for (auto index = gap; index < n; ++index) {
      auto value = std::iter_value_t<RandomIt>(std::move(*(first + index)));
      auto hole = index;
      while (hole >= gap && detail::sort_less(comp, value, *(first + hole - gap))) {
        *(first + hole) = std::move(*(first + hole - gap));
        hole -= gap;
      }
      *(first + hole) = std::move(value);
    }
  }
}

template <std::random_access_iterator RandomIt>
constexpr void shell_sort(RandomIt first, RandomIt last) {
  chaistl::shell_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void heap_sort(RandomIt first, RandomIt last, Compare comp) {
  std::make_heap(first, last, comp);
  std::sort_heap(first, last, comp);
}

template <std::random_access_iterator RandomIt>
constexpr void heap_sort(RandomIt first, RandomIt last) {
  chaistl::heap_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void quick_sort(RandomIt first, RandomIt last, Compare comp) {
  constexpr auto insertion_cutoff = typename std::iterator_traits<RandomIt>::difference_type{16};

  while (last - first > insertion_cutoff) {
    detail::median_to_first(first, first + (last - first) / 2, last - 1, comp);
    RandomIt pivot = detail::partition_around_first(first, last, comp);
    if (pivot - first < last - (pivot + 1)) {
      chaistl::quick_sort(first, pivot, comp);
      first = pivot + 1;
    } else {
      chaistl::quick_sort(pivot + 1, last, comp);
      last = pivot;
    }
  }

  chaistl::insertion_sort(first, last, comp);
}

template <std::random_access_iterator RandomIt>
constexpr void quick_sort(RandomIt first, RandomIt last) {
  chaistl::quick_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void qsort(RandomIt first, RandomIt last, Compare comp) {
  chaistl::quick_sort(first, last, comp);
}

template <std::random_access_iterator RandomIt>
constexpr void qsort(RandomIt first, RandomIt last) {
  chaistl::qsort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
void merge_sort(RandomIt first, RandomIt last, Compare comp) {
  using value_type = std::iter_value_t<RandomIt>;
  const auto n = last - first;
  if (n < 2) {
    return;
  }

  RandomIt middle = first + n / 2;
  chaistl::merge_sort(first, middle, comp);
  chaistl::merge_sort(middle, last, comp);

  std::vector<value_type> buffer;
  buffer.reserve(static_cast<std::size_t>(n));

  RandomIt left = first;
  RandomIt right = middle;
  while (left != middle && right != last) {
    if (detail::sort_less(comp, *right, *left)) {
      buffer.push_back(std::move(*right));
      ++right;
    } else {
      buffer.push_back(std::move(*left));
      ++left;
    }
  }
  while (left != middle) {
    buffer.push_back(std::move(*left));
    ++left;
  }
  while (right != last) {
    buffer.push_back(std::move(*right));
    ++right;
  }

  std::move(buffer.begin(), buffer.end(), first);
}

template <std::random_access_iterator RandomIt>
void merge_sort(RandomIt first, RandomIt last) {
  chaistl::merge_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
void bottom_up_merge_sort(RandomIt first, RandomIt last, Compare comp) {
  using value_type = std::iter_value_t<RandomIt>;
  using difference_type = typename std::iterator_traits<RandomIt>::difference_type;
  const auto n = last - first;
  if (n < 2) {
    return;
  }

  std::vector<value_type> buffer;
  buffer.reserve(static_cast<std::size_t>(n));

  for (difference_type width = 1; width < n; width *= 2) {
    for (difference_type left_index = 0; left_index < n; left_index += width * 2) {
      const auto middle_index = std::min(left_index + width, n);
      const auto right_index = std::min(left_index + width * 2, n);
      if (middle_index == right_index) {
        continue;
      }

      buffer.clear();
      RandomIt left = first + left_index;
      RandomIt middle = first + middle_index;
      RandomIt right = first + right_index;
      RandomIt lhs = left;
      RandomIt rhs = middle;

      while (lhs != middle && rhs != right) {
        if (detail::sort_less(comp, *rhs, *lhs)) {
          buffer.push_back(std::move(*rhs));
          ++rhs;
        } else {
          buffer.push_back(std::move(*lhs));
          ++lhs;
        }
      }
      while (lhs != middle) {
        buffer.push_back(std::move(*lhs));
        ++lhs;
      }
      while (rhs != right) {
        buffer.push_back(std::move(*rhs));
        ++rhs;
      }
      std::move(buffer.begin(), buffer.end(), left);
    }
  }
}

template <std::random_access_iterator RandomIt>
void bottom_up_merge_sort(RandomIt first, RandomIt last) {
  chaistl::bottom_up_merge_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
void stable_sort(RandomIt first, RandomIt last, Compare comp) {
  chaistl::merge_sort(first, last, comp);
}

template <std::random_access_iterator RandomIt>
void stable_sort(RandomIt first, RandomIt last) {
  chaistl::stable_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void intro_sort(RandomIt first, RandomIt last, Compare comp) {
  const auto n = detail::sort_size(first, last);
  if (n < 2) {
    return;
  }
  detail::intro_sort_loop(first, last, comp, detail::log2_floor(static_cast<std::size_t>(n)) * 2);
}

template <std::random_access_iterator RandomIt>
constexpr void intro_sort(RandomIt first, RandomIt last) {
  chaistl::intro_sort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void pdqsort(RandomIt first, RandomIt last, Compare comp) {
  const auto n = detail::sort_size(first, last);
  if (n < 2) {
    return;
  }
  detail::pdq_sort_loop(first, last, comp, detail::log2_floor(static_cast<std::size_t>(n)) * 2);
}

template <std::random_access_iterator RandomIt>
constexpr void pdqsort(RandomIt first, RandomIt last) {
  chaistl::pdqsort(first, last, std::less<>{});
}

template <std::random_access_iterator RandomIt, class Compare>
constexpr void sort(RandomIt first, RandomIt last, Compare comp) {
  chaistl::pdqsort(first, last, comp);
}

template <std::random_access_iterator RandomIt>
constexpr void sort(RandomIt first, RandomIt last) {
  chaistl::sort(first, last, std::less<>{});
}

}  // namespace chaistl
