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
