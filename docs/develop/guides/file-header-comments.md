# File Header Comments

Every `.hpp` file should have a header comment that explains:

1. **What this file is** — one sentence
2. **Architecture** — what it builds on or what builds on it
3. **Key design decisions** — with ADR reference if cross-cutting
4. **Non-standard extensions** — deviations from `std::`
5. **References** — standard draft, cppreference, industrial references

## Template

```cpp
#pragma once

// ============================================================================
// [文件名] — [一句话描述]
// ============================================================================
//
// Architecture:
//   - [基于什么 / 被什么使用 / 独立实现]
//   - [关键设计模式：policy-based / flat storage / ...]
//
// Key design decisions:
//   - [决策1]（ADR-NNN if cross-cutting）
//   - [决策2]（file-local if obvious from code）
//
// Non-standard extensions:
//   - [扩展1]
//   - [扩展2]
//
// References:
//   - C++ Draft: https://eel.is/c++draft/[章节]
//   - cppreference: https://en.cppreference.com/w/cpp/[路径]
//   - [Industrial reference]: [path or URL]
//   - ADR-NNN: [title]
```

## Examples

### Example 1: Core container (set)

```cpp
// ============================================================================
// set — Ordered unique-key associative container
// ============================================================================
//
// Architecture:
//   - Thin wrapper over detail::tree::binary_search_tree
//   - Balancing scheme is a pluggable policy (ADR-004)
//
// Non-standard extension:
//   - Trailing Policy template parameter (default rb_tree_policy)
//   - Lets tests/benchmarks instantiate over AVL, treap, etc.
//   - set<K> with standard arguments is unaffected
//
// References:
//   - C++ Draft: https://eel.is/c++draft/set
//   - cppreference: https://en.cppreference.com/w/cpp/container/set
//   - ADR-004: Tree Policy-Based Design
```

### Example 2: Policy (rb_tree_policy)

```cpp
// ============================================================================
// rb_tree_policy — Red-black tree balance policy
// ============================================================================
//
// Architecture:
//   - Implements the balance_policy concept (binary_search_tree.hpp:72)
//   - Injected into each node via node_extension; color written at link time
//
// Algorithm source:
//   - libstdc++ _Rb_tree_insert_and_rebalance / _Rb_tree_rebalance_for_erase
//   - CLRS Introduction to Algorithms, 3rd ed., Chapter 13
//
// References:
//   - gcc: libstdc++-v3/src/c++98/tree.cc
//   - ADR-004: Tree Policy-Based Design
```

### Example 3: Experimental container (ring_buffer)

```cpp
// ============================================================================
// ring_buffer — Fixed-capacity circular buffer with pluggable overflow policy
// ============================================================================
//
// Architecture:
//   - Circular array with read/write cursors
//   - Overflow behavior determined by Policy template parameter:
//     - overwrite_policy (default): overwrite oldest elements
//     - reject_policy: reject new elements, count drops
//
// Non-standard extension:
//   - Entire container is non-standard (no std::ring_buffer)
//   - Policy-based overflow is unique to this implementation
//
// References:
//   - P0059R4 (abandoned): https://wg21.link/p0059r4
//   - Boost.CircularBuffer: https://www.boost.org/doc/libs/release/doc/html/circular_buffer.html
//   - EASTL ring_buffer
```

## What NOT to put in header comments

| Don't | Do instead |
|-------|-----------|
| "TODO: implement X" | GitHub issue or inline `// TODO(#123)` |
| Full algorithm explanation | Reference + code comments at function level |
| Future roadmap | Roadmap doc or issue |
| "This might change" | Nothing — all code might change |
