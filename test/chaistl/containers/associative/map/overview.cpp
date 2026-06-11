// SPDX-License-Identifier: Apache-2.0

// References:
// - std::map member types and container requirements:
//   https://en.cppreference.com/w/cpp/container/map
//   https://eel.is/c++draft/map

#include <chaistl/concepts.hpp>
#include <chaistl/containers/map.hpp>
#include <chaistl/memory/allocator.hpp>

#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>

namespace {

template <class Map>
concept can_copy_insert = requires(Map& m, const typename Map::value_type& value) { m.insert(value); };

template <class Map>
concept can_move_insert = requires(Map& m, typename Map::value_type&& value) { m.insert(std::move(value)); };

}  // namespace

static_assert(std::ranges::range<chaistl::map<int, std::string>>);
static_assert(std::ranges::sized_range<chaistl::map<int, std::string>>);
static_assert(!std::ranges::contiguous_range<chaistl::map<int, std::string>>);
static_assert(std::same_as<chaistl::map<int, std::string>::key_type, int>);
static_assert(std::same_as<chaistl::map<int, std::string>::mapped_type, std::string>);
static_assert(std::same_as<chaistl::map<int, std::string>::value_type, std::pair<const int, std::string>>);

static_assert(std::copy_constructible<chaistl::map<int, std::string>>);
static_assert(std::move_constructible<chaistl::map<int, std::string>>);
static_assert(can_copy_insert<chaistl::map<int, std::string>>);
static_assert(can_move_insert<chaistl::map<int, std::string>>);

static_assert(chaistl::concepts::reversible_container<chaistl::map<int, std::string>>);
static_assert(!chaistl::concepts::sequence_container<chaistl::map<int, std::string>>);
