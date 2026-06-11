// SPDX-License-Identifier: Apache-2.0

#include <chaistl/concepts.hpp>
#include <chaistl/containers/flat_set.hpp>

#include <concepts>
#include <iterator>
#include <ranges>
#include <type_traits>

static_assert(std::ranges::range<chaistl::flat_set<int>>);
static_assert(std::ranges::sized_range<chaistl::flat_set<int>>);
static_assert(std::ranges::random_access_range<chaistl::flat_set<int>>);
static_assert(std::same_as<chaistl::flat_set<int>::value_type, int>);
static_assert(std::same_as<chaistl::flat_set<int>::key_type, int>);
static_assert(std::same_as<chaistl::flat_set<int>::container_type, chaistl::vector<int>>);

static_assert(std::copy_constructible<chaistl::flat_set<int>>);
static_assert(std::move_constructible<chaistl::flat_set<int>>);
static_assert(chaistl::concepts::reversible_container<chaistl::flat_set<int>>);

static_assert(std::same_as<chaistl::flat_set<int>::iterator, chaistl::flat_set<int>::const_iterator>);
static_assert(std::is_const_v<std::remove_reference_t<std::iter_reference_t<chaistl::flat_set<int>::iterator>>>);
static_assert(!std::output_iterator<chaistl::flat_set<int>::iterator, int>);
static_assert(std::random_access_iterator<chaistl::flat_set<int>::iterator>);
