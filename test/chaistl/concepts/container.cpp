// SPDX-License-Identifier: Apache-2.0

// References:
// - C++ named requirements for containers:
//   https://en.cppreference.com/w/cpp/named_req/Container
//   https://en.cppreference.com/w/cpp/named_req/SequenceContainer
//   https://eel.is/c++draft/container.requirements
// - The checks are chaistl learning concepts, not copied standard-library
//   implementation tests.

#include <chaistl/concepts.hpp>
#include <chaistl/containers/array.hpp>
#include <chaistl/containers/deque.hpp>
#include <chaistl/containers/map.hpp>
#include <chaistl/containers/multimap.hpp>
#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/containers/vector.hpp>

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <version>

using int_vector = std::vector<int>;
using unique_ptr_vector = std::vector<std::unique_ptr<int>>;
using c_string_array = std::array<const char*, 2>;
using int_set = chaistl::set<int>;
using int_map = chaistl::map<int, std::string>;
using int_multiset = chaistl::multiset<int>;
using int_multimap = chaistl::multimap<int, std::string>;

static_assert(chaistl::concepts::container_compatible_range<int_vector&, int>);
static_assert(chaistl::concepts::container_compatible_range<int_vector&, long>);
static_assert(chaistl::concepts::container_compatible_range<c_string_array&, std::string>);
static_assert(!chaistl::concepts::container_compatible_range<unique_ptr_vector&, std::unique_ptr<int>>);

static_assert(chaistl::concepts::container<chaistl::array<int, 3>>);
static_assert(chaistl::concepts::is_container_v<chaistl::array<int, 3>>);
static_assert(chaistl::concepts::reversible_container<chaistl::array<int, 3>>);
static_assert(chaistl::concepts::is_reversible_container_v<chaistl::array<int, 3>>);
static_assert(chaistl::concepts::contiguous_container<chaistl::array<int, 3>>);
static_assert(chaistl::concepts::is_contiguous_container_v<chaistl::array<int, 3>>);
// std::array partially satisfies SequenceContainer, but not the full named
// requirement because fixed-size arrays do not provide insert/erase/assign.
static_assert(!chaistl::concepts::sequence_container<chaistl::array<int, 3>>);
static_assert(!chaistl::concepts::is_sequence_container_v<chaistl::array<int, 3>>);

static_assert(chaistl::concepts::container<chaistl::vector<int>>);
static_assert(chaistl::concepts::allocator_aware_container<chaistl::vector<int>>);
static_assert(chaistl::concepts::is_allocator_aware_container_v<chaistl::vector<int>>);
static_assert(chaistl::concepts::reversible_container<chaistl::vector<int>>);
static_assert(chaistl::concepts::contiguous_container<chaistl::vector<int>>);
static_assert(chaistl::concepts::sequence_container<chaistl::vector<int>>);
static_assert(chaistl::concepts::is_sequence_container_v<chaistl::vector<int>>);
static_assert(!chaistl::concepts::associative_container<chaistl::vector<int>>);
static_assert(!chaistl::concepts::is_associative_container_v<chaistl::vector<int>>);
static_assert(chaistl::concepts::back_sequence_container<chaistl::vector<int>>);
static_assert(!chaistl::concepts::front_sequence_container<chaistl::vector<int>>);
static_assert(chaistl::concepts::random_access_sequence_container<chaistl::vector<int>>);
static_assert(chaistl::concepts::container<chaistl::vector<std::unique_ptr<int>>>);
static_assert(chaistl::concepts::allocator_aware_container<chaistl::vector<std::unique_ptr<int>>>);
static_assert(chaistl::concepts::reversible_container<chaistl::vector<std::unique_ptr<int>>>);
static_assert(chaistl::concepts::contiguous_container<chaistl::vector<std::unique_ptr<int>>>);

static_assert(chaistl::concepts::container<chaistl::deque<int>>);
static_assert(chaistl::concepts::allocator_aware_container<chaistl::deque<int>>);
static_assert(chaistl::concepts::reversible_container<chaistl::deque<int>>);
static_assert(chaistl::concepts::sequence_container<chaistl::deque<int>>);
static_assert(!chaistl::concepts::associative_container<chaistl::deque<int>>);
static_assert(chaistl::concepts::front_sequence_container<chaistl::deque<int>>);
static_assert(chaistl::concepts::back_sequence_container<chaistl::deque<int>>);
static_assert(chaistl::concepts::random_access_sequence_container<chaistl::deque<int>>);
static_assert(!chaistl::concepts::contiguous_container<chaistl::deque<int>>);

// Move-only types satisfy sequence_container and its refinements because
// copy-dependent and initializer_list-based operations are checked
// conditionally via if constexpr in the concept definition.
static_assert(chaistl::concepts::sequence_container<chaistl::deque<std::unique_ptr<int>>>);
static_assert(chaistl::concepts::front_sequence_container<chaistl::deque<std::unique_ptr<int>>>);
static_assert(chaistl::concepts::back_sequence_container<chaistl::deque<std::unique_ptr<int>>>);
static_assert(chaistl::concepts::random_access_sequence_container<chaistl::deque<std::unique_ptr<int>>>);

static_assert(chaistl::concepts::container<std::array<int, 3>>);
static_assert(chaistl::concepts::container<std::array<std::unique_ptr<int>, 3>>);
// Same distinction for the standard implementation.
static_assert(!chaistl::concepts::sequence_container<std::array<int, 3>>);
static_assert(chaistl::concepts::container<std::vector<int>>);
static_assert(chaistl::concepts::allocator_aware_container<std::vector<int>>);

// std::vector<int> satisfies sequence_container because the concept now
// omits copy-dependent and initializer_list-based operations that would
// otherwise require C++23 range-aware members on all toolchains.
static_assert(chaistl::concepts::sequence_container<std::vector<int>>);
static_assert(chaistl::concepts::back_sequence_container<std::vector<int>>);
static_assert(!chaistl::concepts::front_sequence_container<std::vector<int>>);
static_assert(chaistl::concepts::random_access_sequence_container<std::vector<int>>);

static_assert(chaistl::concepts::container<std::vector<std::unique_ptr<int>>>);
static_assert(chaistl::concepts::allocator_aware_container<std::vector<std::unique_ptr<int>>>);

static_assert(chaistl::concepts::container<int_set>);
static_assert(chaistl::concepts::reversible_container<int_set>);
static_assert(chaistl::concepts::allocator_aware_container<int_set>);
static_assert(chaistl::concepts::associative_container<int_set>);
static_assert(chaistl::concepts::unique_associative_container<int_set>);
static_assert(!chaistl::concepts::multi_associative_container<int_set>);
static_assert(chaistl::concepts::is_associative_container_v<int_set>);
static_assert(chaistl::concepts::is_unique_associative_container_v<int_set>);

static_assert(chaistl::concepts::container<int_map>);
static_assert(chaistl::concepts::reversible_container<int_map>);
static_assert(chaistl::concepts::allocator_aware_container<int_map>);
static_assert(chaistl::concepts::associative_container<int_map>);
static_assert(chaistl::concepts::unique_associative_container<int_map>);
static_assert(!chaistl::concepts::multi_associative_container<int_map>);

static_assert(chaistl::concepts::container<int_multiset>);
static_assert(chaistl::concepts::reversible_container<int_multiset>);
static_assert(chaistl::concepts::allocator_aware_container<int_multiset>);
static_assert(chaistl::concepts::associative_container<int_multiset>);
static_assert(!chaistl::concepts::unique_associative_container<int_multiset>);
static_assert(chaistl::concepts::multi_associative_container<int_multiset>);
static_assert(chaistl::concepts::is_multi_associative_container_v<int_multiset>);

static_assert(chaistl::concepts::container<int_multimap>);
static_assert(chaistl::concepts::reversible_container<int_multimap>);
static_assert(chaistl::concepts::allocator_aware_container<int_multimap>);
static_assert(chaistl::concepts::associative_container<int_multimap>);
static_assert(!chaistl::concepts::unique_associative_container<int_multimap>);
static_assert(chaistl::concepts::multi_associative_container<int_multimap>);

static_assert(chaistl::concepts::associative_container<std::set<int>>);
static_assert(chaistl::concepts::unique_associative_container<std::set<int>>);
static_assert(!chaistl::concepts::multi_associative_container<std::set<int>>);
static_assert(chaistl::concepts::associative_container<std::map<int, std::string>>);
static_assert(chaistl::concepts::unique_associative_container<std::map<int, std::string>>);
static_assert(!chaistl::concepts::multi_associative_container<std::map<int, std::string>>);
static_assert(chaistl::concepts::associative_container<std::multiset<int>>);
static_assert(!chaistl::concepts::unique_associative_container<std::multiset<int>>);
static_assert(chaistl::concepts::multi_associative_container<std::multiset<int>>);
static_assert(chaistl::concepts::associative_container<std::multimap<int, std::string>>);
static_assert(!chaistl::concepts::unique_associative_container<std::multimap<int, std::string>>);
static_assert(chaistl::concepts::multi_associative_container<std::multimap<int, std::string>>);
