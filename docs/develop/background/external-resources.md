# External Resources

## Specifications

| Source | URL | Role |
|--------|-----|------|
| C++ Working Draft | https://eel.is/c++draft/ | Normative effects, preconditions, complexity, invalidation |
| cppreference | https://en.cppreference.com/ | Public API shape, names, overloads, first-pass behavior |
| C++ Core Guidelines | https://isocpp.github.io/CppCoreGuidelines/ | Template and generic-programming best practices (T.1–T.144) |

**The rule:** write code for `chaistl` against cppreference and the draft.
Reference implementations are for understanding *how* to sequence operations,
not for copying names, structure, or compatibility layers.

For public functions, match cppreference names and overload structure.
Internal names should explain the data structure (`first_offset_`, `block_size`,
`capacity_last_`) rather than mirror standard-library abbreviations.

---

## Standard-Library Implementations to Watch

| Implementation | Where to follow |
|----------------|-----------------|
| libc++ | https://github.com/llvm/llvm-project/tree/main/libcxx |
| libstdc++ | https://gcc.gnu.org/ml/libstdc/ |
| MSVC STL | https://github.com/microsoft/STL |

---

## C++26 Container Papers to Track

These papers define new container facilities that may eventually be added to
chaistl. Track them for interface design and implementation strategy.

| Paper | Feature |
|-------|---------|
| P0843R14 | `std::inplace_vector<T, N>` — fixed-capacity sequence container |
| P0429R9 / P1222 | `std::flat_map` / `std::flat_set` — sorted container adaptors |
| P3091R4 | Better lookups for map-like containers |
| P3331R1 | `front()` / `back()` for associative containers |
| P3372R3 | `constexpr` containers and adapters |

---

## Non-Standard Extension References

Libraries that provide STL-style containers outside the standard. Useful when
designing `chaistl::experimental::*` extensions.

| Library | Notable Containers |
|---------|-------------------|
| Boost.CircularBuffer | `circular_buffer`, `circular_buffer_space_optimized` |
| Boost.Lockfree | `spsc_queue`, `mpmc_queue`, `stack` |
| Boost.Intrusive | `list`, `slist`, `set`, `unordered_set` (intrusive); tree algorithm families (RB/AVL/splay/treap/scapegoat) |
| Boost.Container | `flat_map`, `flat_set`, `stable_vector`; `tree_type_enum` (RB/AVL/splay/scapegoat) |
| GCC PBDS | `tree` (RB/splay/ordered-vector), `cc_hash_table` / `gp_hash_table`, `rope` |
| Abseil | `InlinedVector`, `flat_hash_map`, `node_hash_map`, `btree_map` |
| Folly | `fbvector`, `small_vector`, `concurrent_queue`, `sorted_vector_map` |
| EASTL | `fixed_vector`, `fixed_string`, `intrusive_list`, `vector_map`, `flat_map` |
