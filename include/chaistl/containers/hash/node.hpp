// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>

namespace chaistl::detail::hash {

/**
 * @brief Links and metadata shared by all hash table nodes.
 *
 * Every node participates in two orthogonal structures
 * (ADR: Hash Table Design):
 *
 *   - @c next_in_bucket — the singly-linked collision chain of the bucket
 *     selected by @c cached_hash. Lookup and local iterators walk this chain;
 *     it ends at @c nullptr.
 *   - @c prev_in_order / @c next_in_order — the doubly-linked global
 *     iteration list. `begin()`, `++iterator`, and `erase(iterator)` use this
 *     list, which is what keeps them O(1) without scanning empty buckets.
 *
 * The list must be doubly linked: erasing a node needs its global
 * predecessor, which can live in any other bucket.
 *
 * @c cached_hash stores the full hash code. Rehashing relinks bucket chains
 * from this field without calling the user's hash function, and
 * `erase(iterator)` locates its bucket without calling any user code. The
 * cache is only meaningful inside a table: a node re-inserted elsewhere
 * (node handles, merge) must be re-hashed by the destination.
 */
struct hash_node_base {
  hash_node_base* next_in_bucket = nullptr;
  hash_node_base* prev_in_order = nullptr;
  hash_node_base* next_in_order = nullptr;
  std::size_t cached_hash = 0;
};

/**
 * @brief A hash table node holding a value.
 *
 * The value lives in a union so the node object's lifetime and the value's
 * lifetime are separate steps (same pattern as @c bst_value_node): the table
 * first starts the node lifetime, then constructs the value through
 * `allocator_traits`, and on destruction ends the value lifetime explicitly.
 * The empty constructor/destructor keep both usable in constant evaluation.
 *
 * @tparam Value The element type stored in the table.
 */
template <class Value>
struct hash_node : hash_node_base {
  using value_type = Value;

  union {
    Value value;
  };

  constexpr hash_node() noexcept : hash_node_base{} {}
  constexpr ~hash_node() {}
};

}  // namespace chaistl::detail::hash
