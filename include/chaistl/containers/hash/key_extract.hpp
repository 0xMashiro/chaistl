// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/associative/detail/key_extract.hpp>

namespace chaistl::detail::hash {

// The key extractors are shared associative machinery (the tree core uses
// the same two); the definitions live in associative/detail/key_extract.hpp
// and are re-exported here so hash call sites stay local to this namespace.
using chaistl::detail::associative::identity;
using chaistl::detail::associative::select1st;

}  // namespace chaistl::detail::hash
