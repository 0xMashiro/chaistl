# Scope: Standard Components, Stable Extensions, Experimental Extensions

## References & Sources

| Priority | Source | Use for |
|----------|--------|---------|
| 1 | [cppreference](https://en.cppreference.com/) | Public API shape, names, overloads, first-pass behavior |
| 2 | [C++ working draft](https://eel.is/c++draft/) | Normative effects, preconditions, complexity, invalidation |
| 3 | [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/) | Template and generic-programming best practices (T.1–T.144) |
| 4 | libc++ / libstdc++ / MSVC STL | Implementation strategy only — study the algorithm, ignore the scaffolding |

For public functions, match cppreference names and overload structure.
Internal names should explain the data structure (`first_offset_`, `block_size`,
`capacity_last_`) rather than mirror standard-library abbreviations.

## Standard Components

Standard components must track a C++ standard clause or cppreference page.
Every public function must match standard names, overload structure, and
complexity claims. Any deviation must be documented with `@note Non-standard
extension` or `@bug`.

## Stable chaistl Extensions

Stable extensions live in `chaistl::*`, are covered by semantic versioning,
and must be documented as chaistl extensions instead of standard-library
components. These names are for facilities whose behavior is implemented,
tested, and intended for users, but whose interface is intentionally beyond the
standard STL surface.

Examples:

- `max_heap` and `min_heap`: teaching aliases over `priority_queue` that make
  the comparator direction explicit.
- `d_ary_heap`, `pairing_heap`, `binomial_heap`, `skew_heap`,
  `leftist_heap`, and `fibonacci_heap`: aliases over `priority_queue` with
  non-default heap policies.
- `avl_set`, `avl_map`, `treap_set`, `treap_map`, `splay_set`, and
  `splay_map`: aliases over the ordered associative containers with
  non-default tree policies.

Stable extensions should:

1. Use precise container-name headers, e.g.
   `<chaistl/containers/pairing_heap.hpp>` or
   `<chaistl/containers/avl_set.hpp>`.
2. State which standard-compatible container they alias or extend.
3. Document complexity, iterator invalidation, and exception guarantees when
   they differ from the default standard-compatible spelling.

## Experimental Extensions

Experimental extensions live in `chaistl::experimental::*` and are not bound
by standard wording. They should still follow STL conventions where
applicable, but their primary goal is utility and teachability, not standard
conformance. Each experimental header must include a doc comment explaining:

1. What problem it solves.
2. What standard (if any) inspired it.
3. Any known deviations from STL conventions.

Reference repositories may live under `reference/`, which is git-ignored.
