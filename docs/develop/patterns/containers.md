# Implementation Patterns

How to build containers, iterators, and memory utilities in `chaistl`.

For the `std` facilities you will combine, see [toolbox.md](toolbox.md).

---

## Container Implementation Patterns

### Internal Helper Naming

Helper names should describe the semantic boundary they own, not merely the
lines of code they contain. Do not extract a helper just because a block is
used once; extract it when the name captures a stable container concept or
hides a fragile invariant such as raw storage, sentinels, allocator ownership,
or rollback state.

| Scenario | Preferred verb | Boundary |
|----------|----------------|----------|
| Allocate raw storage only | `allocate_*` | Obtains storage; does not construct elements or change container structure |
| Release raw storage only | `deallocate_*` | Releases storage that no longer contains live elements |
| Create a live node/element | `create_*` | Allocates and constructs, with rollback on partial failure |
| End a live node/element | `destroy_*` | Destroys and deallocates one live object or owned node |
| Wire existing nodes | `link_*` | Changes pointers only; does not construct, destroy, or update `size_` |
| Detach existing nodes | `unlink_*` | Changes pointers only; detached nodes remain live and owned by someone |
| Public-operation insertion core | `insert_*` / `emplace_*` | May construct, link, update size, and provide exception guarantees |
| Erase from this container | `erase_*` | Removes from the container and ends element lifetime |
| Standard remove-style operation | `remove_*` | Reserve for value/predicate removal semantics, matching list-style APIs |
| Take another container's storage | `take_*_from` | O(1) ownership transfer; source is left empty and valid |
| Destroy current storage, then take | `replace_*_from` | Releases current storage before taking from another container |
| Check whether storage can be taken | `storage_compatible_with` | Expresses the operation precondition, usually allocator equality |
| Build a temporary object | `make_*` | Returns a new value object; does not mutate the current container |
| Verify invariants | `verify()` / `validate()` | Full structural check for tests and teaching, not normal operation |

Use `*_impl` only when several public overloads funnel into the same semantic
operation and no more specific name exists. Prefer `insert_impl` for
`insert(const T&)` / `insert(T&&)` sharing; avoid `impl` for named algorithms
such as `normalize_appended_tail`, `trickle_down`, or `search_predecessors`.

For `detail` algorithms modeled on standard algorithms, keep the standard
algorithm name intact and add project-specific qualification around it. For
example, prefer `allocator_uninitialized_copy_n` over
`uninitialized_allocator_copy_n`: the former says "allocator-aware
`uninitialized_copy_n`", while the latter splits the standard algorithm name.

The naming contract matters:

- A function named `unlink_*` must not destroy nodes or decrement `size_`.
  If it does, name it `erase_*`, or split it into `unlink_*` plus `destroy_*`.
- A function named `create_node` may allocate auxiliary node storage, construct
  the value, and roll back all partial work. That is stronger and clearer than
  `make_node` for container-owned nodes.
- A function named `append_*` or `prepend_*` should describe container-end
  insertion semantics. Internal pointer wiring should use `link_*`.
- A function named `take_storage_from` must leave the source object empty and
  destructible. If it first releases current storage, call the wrapper
  `replace_storage_from`.

The usual private-section order is:

1. storage data members
2. node/raw-storage lifecycle helpers
3. link/unlink helpers
4. storage ownership helpers
5. lookup/order helpers
6. invariant checks

Small containers may collapse sections, but keep the order when several helper
families are present. It lets readers find the invariant they need without
learning each container from scratch.

### Constructor Categories

Every sequence container needs these constructor forms:

| Form | Pattern |
|------|---------|
| Default | `= default` with `noexcept(noexcept(Allocator{}))` |
| Allocator-only | `explicit constexpr ctor(const Alloc&) noexcept` |
| Count | allocate, then `uninitialized_default_construct_n` / `uninitialized_fill_n` |
| Count + value | allocate, then `uninitialized_fill_n` |
| Iterator pair | `if constexpr (forward_iterator)`: allocate + `uninitialized_copy`; else: `emplace_back` loop |
| Initializer list | delegate to iterator-pair ctor |
| Copy | `select_on_container_copy_construction`, then `uninitialized_copy` |
| Move | `exchange` the pointers; move the allocator |
| Copy + foreign alloc | allocate, then `uninitialized_copy` |
| Move + foreign alloc | if equal: take storage; else: `uninitialized_move_if_noexcept` |
| from_range (C++23) | allocate, then `append_range`, with exception guard |

### Assignment

The canonical copy-assignment pattern (strong exception safety via copy-and-swap):

```cpp
constexpr vector& operator=(const vector& other) {
    if (this == &other) return *this;
    if constexpr (allocator_traits::propagate_on_container_copy_assignment::value) {
        vector copy(other, other.allocator_);
        destroy_and_deallocate_storage();
        allocator_ = other.allocator_;
        take_storage_from(copy);
    } else {
        vector copy(other, allocator_);
        destroy_and_deallocate_storage();
        take_storage_from(copy);
    }
    return *this;
}
```

The canonical move-assignment pattern:

```cpp
constexpr vector& operator=(vector&& other) noexcept(...) {
    if (this == &other) return *this;
    if constexpr (allocator_traits::propagate_on_container_move_assignment::value) {
        destroy_and_deallocate_storage();
        allocator_ = std::move(other.allocator_);
        take_storage_from(other);
    } else if (allocator_ == other.allocator_) {
        destroy_and_deallocate_storage();
        take_storage_from(other);
    } else {
        // Different allocators that compare unequal: must move elements
        vector moved(std::move(other), allocator_);
        destroy_and_deallocate_storage();
        take_storage_from(moved);
    }
    return *this;
}
```

### Capacity: Reallocation with Exception Safety

The core reallocation pattern (vector growth, deque map growth, etc.):

```cpp
// 1. Allocate new storage (raw_storage_buffer or similar RAII)
// 2. Construct the new element FIRST if args reference existing elements
// 3. Move/copy old elements to new storage (move_if_noexcept)
// 4. Destroy old elements
// 5. Deallocate old storage
// 6. Transfer ownership of new storage
```

**Critical detail**: when `emplace_back` triggers reallocation, construct the
new element in the new storage *before* moving the old elements. This avoids
use-after-move if the argument references an existing element.

### Swap

```cpp
constexpr void swap(vector& other) noexcept(
    allocator_traits::propagate_on_container_swap::value ||
    allocator_traits::is_always_equal::value) {
    using std::swap;
    swap(first_, other.first_);
    swap(last_, other.last_);
    swap(capacity_last_, other.capacity_last_);
    if constexpr (allocator_traits::propagate_on_container_swap::value) {
        swap(allocator_, other.allocator_);
    }
    // NOTE: if !propagate_on_container_swap && !is_always_equal && allocators
    //       are unequal, swap is UB per the standard. We don't guard against it.
}
```

### Insert / Erase in the Middle

For non-node-based containers (vector), prefer these strategies in order:

1. **If spare capacity at the end**: shift the shorter segment, construct/destroy at edges.
2. **If no spare capacity**: reallocate with the new element constructed first.
3. **For fill-insert**: short-insert vs long-insert branches (compare `count` vs `suffix_count`).

For node-based containers (list), splice nodes directly — O(1).

### Trivially-Destructible Fast Path

```cpp
constexpr void clear() noexcept {
    if constexpr (std::is_trivially_destructible_v<T>) {
        last_ = first_;     // no-op: bytes stay, lifetime effectively ended
    } else {
        detail::allocator_destroy_reverse(allocator_, first_, last_);
        last_ = first_;
    }
}
```

The same pattern applies in `pop_back`, `erase`, and destructors.

---

## Iterator Implementation Patterns

> Canonical iterator documentation has moved to
> [`iterators.md`](iterators.md). The section below is kept as a minimal
> pointer to the two approaches currently under discussion.

The project is evaluating a transition from `iterator_impl<Const>` to
**deducing this** (C++23). For the full rationale and code examples, see
[`patterns/iterators.md`](iterators.md) and
[ADR 001](../decisions/001-iterator-impl-pattern.md).

---

## Memory & Lifetime Patterns

### Always Route Through allocator_traits

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

### Exception Safety: RAII Guards

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

### Storage Builder Workflow

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

### Exception Safety Guarantees

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

### ADL Two-Phase Swap

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

### `[[no_unique_address]]` for Allocators

Always apply `[[no_unique_address]]` to allocator data members. Stateless
allocators (like `chaistl::allocator<T>`) then occupy zero bytes:

```cpp
[[no_unique_address]] allocator_type allocator_{};
```

---

## Metaprogramming Patterns

### `if constexpr` Over Tag Dispatch

In C++17+, prefer `if constexpr` over tag dispatch for branching on type traits:

```cpp
// ✅ Modern
if constexpr (std::forward_iterator<InputIt>) {
    const auto count = static_cast<size_type>(std::distance(first, last));
    // ... pre-allocate and copy
} else {
    for (; first != last; ++first) emplace_back(*first);
}

// ❌ Legacy (still valid, just noisier)
template<typename InputIt>
vector(InputIt first, InputIt last, std::forward_iterator_tag) { ... }
```

### `void_t` Detection Idiom

When you need to check whether a type has a member without using concepts:

```cpp
template<typename T, typename = void>
struct has_data : std::false_type {};

template<typename T>
struct has_data<T, std::void_t<decltype(std::declval<T&>().data())>> : std::true_type {};
```

In C++20, prefer a `requires`-expression concept. Use `void_t` only when you
need SFINAE that concepts can't express.

### `noexcept` Propagation

Derive `noexcept` specifications from type traits, not hardcoded values:

```cpp
constexpr vector(vector&& other) noexcept;  // unconditionally noexcept

constexpr vector& operator=(vector&& other) noexcept(
    allocator_traits::propagate_on_container_move_assignment::value ||
    allocator_traits::is_always_equal::value);
```

### `consteval` for Compile-Time Computation

Use `consteval` for functions that must run at compile time:

```cpp
consteval std::size_t next_power_of_two(std::size_t n) {
    // only callable in constant expressions
}
```

### Deduction Guides

Every container that has an iterator-pair constructor needs deduction guides:

```cpp
template <std::input_iterator InputIt, class Alloc = allocator<std::iter_value_t<InputIt>>>
vector(InputIt, InputIt, Alloc = Alloc()) -> vector<std::iter_value_t<InputIt>, Alloc>;
```

This is how `vector v{begin, end}` works without explicitly specifying `T`.

---

## Performance Patterns

### Trivially-Copyable Fast Paths

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

### `[[likely]]` / `[[unlikely]]` Hints

Use `[[likely]]` on the common path, `[[unlikely]]` on error paths:

```cpp
if (count > max_size()) [[unlikely]] {
    throw std::length_error("...");
}
if (count > 0) [[likely]] {
    // ... normal construction
}
```

### Move-If-Noexcept for Reallocation

The key to strong exception safety in vector growth:

```cpp
// If T's move constructor is noexcept, elements are moved (fast).
// If T's move constructor can throw, elements are copied (safe).
storage.uninitialized_move_if_noexcept(old_first, old_last, new_first);
```

This is implemented as: for each element, `std::move_if_noexcept(*it)` returns
either `T&&` (move) or `const T&` (copy), depending on `is_nothrow_move_constructible_v<T>`.
