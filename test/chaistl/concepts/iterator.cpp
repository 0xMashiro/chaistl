// SPDX-License-Identifier: Apache-2.0

// References:
// - C++20 iterator concepts:
//   https://en.cppreference.com/w/cpp/iterator
//   https://eel.is/c++draft/iterator.concepts
// - Upstream test layout consulted, without copying test code:
//   reference/llvm-project/libcxx/test/std/iterators/iterator.requirements/
//     iterator.concepts/
// - The checks are written for chaistl concepts and use standard iterator
//   types as observable examples of each cppreference category.

#include <chaistl/concepts/iterator.hpp>
#include <chaistl/containers/array.hpp>
#include <chaistl/containers/vector.hpp>
#include <chaistl/iterator/adapter/reverse.hpp>

#include <array>
#include <deque>
#include <iterator>
#include <list>
#include <memory>
#include <vector>

using int_pointer = int*;
using const_int_pointer = const int*;
using vector_iterator = std::vector<int>::iterator;
using deque_iterator = std::deque<int>::iterator;
using list_iterator = std::list<int>::iterator;
using back_insert_iterator = std::back_insert_iterator<std::vector<int>>;
using unreachable_sentinel = std::unreachable_sentinel_t;

static_assert(chaistl::concepts::indirectly_readable<int_pointer>);
static_assert(chaistl::concepts::indirectly_readable<const_int_pointer>);
static_assert(!chaistl::concepts::indirectly_readable<void*>);

static_assert(chaistl::concepts::indirectly_writable<int_pointer, int>);
static_assert(!chaistl::concepts::indirectly_writable<const_int_pointer, int>);

static_assert(chaistl::concepts::weakly_incrementable<int_pointer>);
static_assert(chaistl::concepts::incrementable<int_pointer>);
static_assert(!chaistl::concepts::weakly_incrementable<void*>);

static_assert(chaistl::concepts::input_or_output_iterator<int_pointer>);
static_assert(!chaistl::concepts::input_or_output_iterator<void*>);

static_assert(chaistl::concepts::sentinel_for<int_pointer, int_pointer>);
static_assert(chaistl::concepts::sentinel_for<unreachable_sentinel, int_pointer>);
static_assert(chaistl::concepts::sized_sentinel_for<int_pointer, int_pointer>);
static_assert(!chaistl::concepts::sized_sentinel_for<unreachable_sentinel, int_pointer>);

static_assert(chaistl::concepts::input_iterator<int_pointer>);
static_assert(chaistl::concepts::output_iterator<int_pointer, int>);
static_assert(chaistl::concepts::output_iterator<back_insert_iterator, int>);
static_assert(!chaistl::concepts::input_iterator<back_insert_iterator>);

static_assert(chaistl::concepts::forward_iterator<int_pointer>);
static_assert(chaistl::concepts::bidirectional_iterator<int_pointer>);
static_assert(chaistl::concepts::random_access_iterator<int_pointer>);
static_assert(chaistl::concepts::contiguous_iterator<int_pointer>);

static_assert(chaistl::concepts::bidirectional_iterator<list_iterator>);
static_assert(!chaistl::concepts::random_access_iterator<list_iterator>);
static_assert(!chaistl::concepts::contiguous_iterator<list_iterator>);

static_assert(chaistl::concepts::random_access_iterator<deque_iterator>);
static_assert(!chaistl::concepts::contiguous_iterator<deque_iterator>);

static_assert(chaistl::concepts::contiguous_iterator<vector_iterator>);
static_assert(chaistl::concepts::contiguous_iterator<std::array<int, 1>::iterator>);
static_assert(chaistl::concepts::contiguous_iterator<chaistl::array<int, 1>::iterator>);
static_assert(chaistl::concepts::contiguous_iterator<chaistl::vector<int>::iterator>);
static_assert(chaistl::concepts::random_access_iterator<chaistl::reverse_iterator<chaistl::vector<int>::iterator>>);
static_assert(!chaistl::concepts::contiguous_iterator<chaistl::reverse_iterator<chaistl::vector<int>::iterator>>);
