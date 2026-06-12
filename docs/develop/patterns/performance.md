# Performance Patterns

## Trivially-Copyable Fast Paths

For `uninitialized_copy` / `uninitialized_move` of trivially copyable types,
use `std::memmove`. The compiler will emit the same code as the element-by-element
loop, but the `memmove` call is clearer about intent:

```cpp
if constexpr (std::is_trivially_copyable_v<value_type>) {
    const auto count = static_cast<std::size_t>(std::distance(first, last));
    if (count > 0) [[likely]] {
        std::memmove(std::to_address(result), std::to_address(first),
                     count * sizeof(value_type));
    }
    return result + count;
}
```

## `[[likely]]` / `[[unlikely]]` Hints

Use `[[likely]]` on the common path, `[[unlikely]]` on error paths:

```cpp
if (count > max_size()) [[unlikely]] {
    throw std::length_error("...");
}
if (count > 0) [[likely]] {
    // ... normal construction
}
```

## Move-If-Noexcept for Reallocation

The key to strong exception safety in vector growth:

```cpp
// If T's move constructor is noexcept, elements are moved (fast).
// If T's move constructor can throw, elements are copied (safe).
storage.uninitialized_move_if_noexcept(old_first, old_last, new_first);
```

This is implemented as: for each element, `std::move_if_noexcept(*it)` returns
either `T&&` (move) or `const T&` (copy), depending on `is_nothrow_move_constructible_v<T>`.

## Floyd's "Bounce" Is Binary-Only

In a hole-based `sift_down`, racing the hole to a leaf and sifting the
displaced value back up skips the per-level "does the value stop here?"
test. In `pop()` the displaced value comes from a leaf and nearly always
sinks back to the bottom, so the skipped test almost never would have
stopped the descent — comparisons roughly halve. libstdc++'s
`__adjust_heap` uses the same strategy; see `binary_heap_policy::sift_down`.

Applicability: **d = 2 only.** With arity d the descent already pays d-1
comparisons per level to pick the largest child, so overshooting the value's
true stop level wastes whole child scans while the skipped test saves a
single comparison. Applying the bounce to `d_ary_heap_policy` was measured
significantly slower and reverted — do not retry it there.

## Anti-Pattern: Contorting Source for One Compiler's Codegen

When a benchmark gap exists under one compiler but not another, check the
generated code before touching the source: the fix may not be ours to make.
Case study: `vector::push_back` on a reserved vector. GCC keeps `last_` /
`capacity_last_` in registers across the loop; clang (as of clang-22)
reloads them every iteration because it declines to fully inline our growth
path, and shows a large artificial gap against `std::vector` on this
microbenchmark. Attribute experiments (`noinline`, `always_inline`, caching
the member in a local) do not fix it — they either change nothing or make
inlining worse. We keep the readable structure; re-check when the benchmark
toolchain's compiler version changes.
