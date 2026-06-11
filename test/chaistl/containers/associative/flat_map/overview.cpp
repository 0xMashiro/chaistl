// SPDX-License-Identifier: Apache-2.0

#include <chaistl/concepts.hpp>
#include <chaistl/containers/flat_map.hpp>

#include <concepts>
#include <iterator>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>

static_assert(std::ranges::range<chaistl::flat_map<int, std::string>>);
static_assert(std::ranges::sized_range<chaistl::flat_map<int, std::string>>);
static_assert(std::ranges::random_access_range<chaistl::flat_map<int, std::string>>);
static_assert(std::same_as<chaistl::flat_map<int, std::string>::key_type, int>);
static_assert(std::same_as<chaistl::flat_map<int, std::string>::mapped_type, std::string>);
static_assert(std::same_as<chaistl::flat_map<int, std::string>::value_type, std::pair<int, std::string>>);
static_assert(std::same_as<chaistl::flat_map<int, std::string>::reference, std::pair<const int&, std::string&>>);
static_assert(
    std::same_as<chaistl::flat_map<int, std::string>::const_reference, std::pair<const int&, const std::string&>>);

static_assert(std::copy_constructible<chaistl::flat_map<int, std::string>>);
static_assert(std::move_constructible<chaistl::flat_map<int, std::string>>);

static_assert(!std::output_iterator<chaistl::flat_map<int, std::string>::iterator, std::pair<int, std::string>>);
static_assert(std::random_access_iterator<chaistl::flat_map<int, std::string>::iterator>);
