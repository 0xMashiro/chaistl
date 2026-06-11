// SPDX-License-Identifier: Apache-2.0

// References:
// - C++ named requirements for iterators:
//   https://en.cppreference.com/w/cpp/named_req/Iterator
//   https://en.cppreference.com/w/cpp/named_req/OutputIterator
//   https://en.cppreference.com/w/cpp/named_req/RandomAccessIterator
// - C++20 iterator concepts:
//   https://en.cppreference.com/w/cpp/iterator
//   https://eel.is/c++draft/iterator.concepts
// - The checks are chaistl learning concepts, not copied standard-library
//   implementation tests.

#include <chaistl/concepts.hpp>

#include <array>
#include <concepts>
#include <iterator>
#include <list>

using list_iterator = std::list<int>::iterator;

static_assert(chaistl::concepts::legacy::bidirectional_iterator<list_iterator>);
static_assert(!chaistl::concepts::legacy::random_access_iterator<list_iterator>);
static_assert(!chaistl::concepts::legacy::contiguous_iterator<list_iterator>);

static_assert(chaistl::concepts::legacy::contiguous_iterator<int*>);
static_assert(chaistl::concepts::legacy::output_iterator<int*, int>);
static_assert(!chaistl::concepts::legacy::output_iterator<const int*, int>);
static_assert(std::contiguous_iterator<int*>);
