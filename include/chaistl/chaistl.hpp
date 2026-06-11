// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/algorithm.hpp>
#include <chaistl/bitset.hpp>
#include <chaistl/concepts.hpp>
#include <chaistl/containers.hpp>
#include <chaistl/dynamic_bitset.hpp>
#include <chaistl/functional/hash.hpp>
#include <chaistl/iterator.hpp>
#include <chaistl/memory.hpp>
#include <chaistl/meta.hpp>

#include <string_view>

namespace chaistl {

inline constexpr std::string_view project_name = "chaistl";

struct version {
  int major;
  int minor;
  int patch;
};

inline constexpr version library_version{0, 1, 0};

}  // namespace chaistl
