# Memory & Lifetime Patterns

## Always Route Through allocator_traits

Every allocation, deallocation, construction, and destruction call must go
through `std::allocator_traits<Alloc>`, never through the allocator directly:

```cpp
// ✅ Correct
allocator_traits::allocate(allocator_, count);
allocator_traits::deallocate(allocator_, ptr, count);
allocator_traits::construct(allocator_, ptr, args...);
allocator_traits::destroy(allocator_, ptr);

// ❌ Wrong
allocator_.allocate(count);
```

The traits class supplies defaults for methods the allocator might not provide.

## Exception Safety: RAII Guards

Every operation that constructs multiple objects in raw storage must have a
rollback path. Use `detail::exception_guard` (or `detail::constructed_range_guard`
for in-place construction):

```cpp
Pointer current = result;
auto guard = detail::make_exception_guard([&] {
    detail::allocator_destroy_reverse(allocator, result, current);
});
for (; first != last; ++first, ++current) {
    allocator_traits::construct(allocator, std::to_address(current), *first);
}
guard.complete();  // disarm — construction succeeded
return current;
```

The same rollback pattern is used in
[`allocator_uninitialized.hpp`](../../include/chaistl/memory/detail/lifetime/allocator_uninitialized.hpp).

## Storage Builder Workflow

For complex multi-step construction (like vector reallocation), use
`detail::uninitialized_storage_builder` which combines allocation,
construction tracking, and rollback:

```cpp
detail::uninitialized_storage_builder<T, Allocator> storage(allocator_, new_capacity);
pointer new_first = storage.data();
pointer new_last  = storage.uninitialized_move_if_noexcept(first_, last_, new_first);
destroy_and_deallocate_storage();
first_ = storage.release();   // transfer ownership, disarm rollback
```

## Exception Safety Guarantees

The C++ standard defines four exception safety levels. Every public function in
a container must satisfy one of them, and the choice must be documented:

| Level | Promise | chaistl examples |
|-------|---------|-----------------|
| **Nothrow** | Never throws. `noexcept` in signature. | destructors, `pop_back`, `swap` (conditional), `clear` |
| **Strong** | On failure, state is rolled back to before the call. | `vector::reserve`, `vector::push_back` (with reallocation) |
| **Basic** | No leaks. Object remains in a valid (but unspecified) state. | `vector::emplace` in the middle, `deque::insert` |
| **No guarantee** | Anything can happen. | NEVER use in a library. |

The canonical strong-guarantee pattern is **"construct in new storage → swap → destroy old"**:

```cpp
// Allocate first. If it throws, *this is untouched.           ← basic guarantee covered
// Construct in new storage. If it throws, RAII deallocates.   ← new storage is self-contained
// Destroy old storage (noexcept). Swap pointers.              ← commit
detail::uninitialized_storage_builder<T, Allocator> storage(allocator_, new_capacity);
storage.uninitialized_move_if_noexcept(first_, last_, storage.data());
destroy_and_deallocate_storage();   // noexcept
first_ = storage.release();
```

For the basic guarantee, the pattern is simpler: ensure that every resource
is owned by an RAII object before any operation that might throw.

## ADL Two-Phase Swap

Always use the two-phase pattern when swapping objects whose types might
have user-defined `swap` overloads:

```cpp
// ✅ Correct: lets ADL find user-defined swap, falls back to std::swap
using std::swap;
swap(first_, other.first_);
swap(last_, other.last_);

// ❌ Wrong: hardcodes std::swap, bypasses ADL
std::swap(first_, other.first_);
```

This is one of the most commonly missed rules in generic C++. It matters
because types in the `std` namespace (or user namespaces) can provide
their own `swap` found via ADL that is more efficient than the default.

## `[[no_unique_address]]` for Allocators

Always apply `[[no_unique_address]]` to allocator data members. Stateless
allocators (like `chaistl::allocator<T>`) then occupy zero bytes:

```cpp
[[no_unique_address]] allocator_type allocator_{};
```
