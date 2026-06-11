# Standard-Library Toolbox

These are the `std` facilities most commonly used when implementing containers
and iterators in `chaistl`. They are battle-tested, well-specified, and often
`constexpr`-enabled.

This file is a quick-reference, not an exhaustive list. If another `std`
facility fits the problem better, use it. For patterns that show how to
combine these facilities, see [containers.md](containers.md).

## Type Traits (`<type_traits>`)

**Type property queries — gate optimizations and safety:**

```cpp
// Core lifetime / layout properties
std::is_trivially_destructible_v<T>       // skip destructor loops in clear()
std::is_trivially_copyable_v<T>           // memmove fast path in reallocation
std::is_trivially_constructible_v<T>      // trivial construction check
std::is_trivially_default_constructible_v<T>
std::is_trivially_move_constructible_v<T>
std::is_trivially_copy_constructible_v<T>
std::is_standard_layout_v<T>              // C-compatible layout
std::is_empty_v<T>                        // EBO optimization decisions

// Nothrow properties — drive noexcept specifications
std::is_nothrow_move_constructible_v<T>   // move-if-noexcept in vector growth
std::is_nothrow_default_constructible_v<T> // noexcept on constructors
std::is_nothrow_copy_constructible_v<T>
std::is_nothrow_move_assignable_v<T>
std::is_nothrow_copy_assignable_v<T>
std::is_nothrow_swappable_v<T>            // noexcept on swap()
std::is_nothrow_destructible_v<T>

// General constructibility / assignability — concept constraints
std::is_constructible_v<T, Args...>
std::is_default_constructible_v<T>
std::is_copy_constructible_v<T>
std::is_move_constructible_v<T>
std::is_assignable_v<T, U>
std::is_copy_assignable_v<T>
std::is_move_assignable_v<T>
std::is_destructible_v<T>
std::is_swappable_v<T>                    // C++17
std::is_swappable_with_v<T, U>            // C++17

// Type categories — tag dispatch, concept specialization
std::is_void_v<T>
std::is_null_pointer_v<T>
std::is_integral_v<T>
std::is_floating_point_v<T>
std::is_array_v<T>
std::is_enum_v<T>
std::is_union_v<T>
std::is_class_v<T>
std::is_function_v<T>
std::is_pointer_v<T>                      // excludes member pointers
std::is_lvalue_reference_v<T>
std::is_rvalue_reference_v<T>
std::is_member_object_pointer_v<T>
std::is_member_function_pointer_v<T>
std::is_reference_v<T>
std::is_scalar_v<T>
std::is_object_v<T>
std::is_compound_v<T>
std::is_fundamental_v<T>
std::is_arithmetic_v<T>

// Composite / advanced categories
std::is_polymorphic_v<T>
std::is_abstract_v<T>
std::is_final_v<T>                        // C++14
std::is_aggregate_v<T>                    // C++17
std::has_virtual_destructor_v<T>
std::has_unique_object_representations_v<T> // C++17, hashable bytewise

// C++20/23 additions
std::is_scoped_enum_v<T>                  // C++23
std::is_implicit_lifetime_v<T>            // C++23
std::is_layout_compatible_v<T, U>         // C++20
std::is_pointer_interconvertible_base_of_v<Base, Derived> // C++20
std::reference_constructs_from_temporary_v<T, U>          // C++23
std::reference_converts_from_temporary_v<T, U>            // C++23
std::is_virtual_base_of_v<Base, Derived>  // C++26
```

**Type transforms — adapt types to context:**

```cpp
// CV / reference manipulation
std::remove_cv_t<T> / std::remove_const_t<T> / std::remove_volatile_t<T>
std::add_cv_t<T> / std::add_const_t<T> / std::add_volatile_t<T>
std::remove_reference_t<T>
std::add_lvalue_reference_t<T>
std::add_rvalue_reference_t<T>
std::remove_cvref_t<T>                    // C++20: cv + ref in one step
std::remove_pointer_t<T>
std::add_pointer_t<T>

// Array manipulation
std::remove_extent_t<T>                   // T[N] → T
std::remove_all_extents_t<T>              // T[N][M] → T

// Sign / size transforms
std::make_signed_t<T>
std::make_unsigned_t<T>

// General transforms
std::conditional_t<B, T, F>               // select between two types
std::decay_t<T>                           // array→ptr, function→ptr, remove cvref
std::enable_if_t<B, T = void>             // SFINAE gate
std::type_identity_t<T>                   // C++20: block deduction
std::common_type_t<T...>                  // type all T can convert to
std::common_reference_t<T...>             // C++20: reference-compatible common type
```

**SFINAE / constraint building:**

```cpp
std::void_t<Ts...>                        // the detection idiom
std::conjunction<Ts...>                   // short-circuit AND
std::disjunction<Ts...>                   // short-circuit OR
std::negation<T>                          // logical NOT
std::bool_constant<V>                     // base of all type traits
std::integral_constant<T, v>              // generic compile-time constant
std::true_type / std::false_type
```

**Invocability and result types:**

```cpp
std::is_invocable_v<F, Args...>           // C++17
std::is_invocable_r_v<R, F, Args...>      // C++17
std::is_nothrow_invocable_v<F, Args...>   // C++17
std::invoke_result_t<F, Args...>          // C++17: return type of INVOKE(F, Args...)
// std::result_of — REMOVED in C++20, do not use
```

**Type relationships:**

```cpp
std::is_same_v<T, U>
std::is_base_of_v<Base, Derived>
std::is_convertible_v<From, To>
std::is_nothrow_convertible_v<From, To>   // C++20
```

**Property queries (integral results):**

```cpp
std::alignment_of_v<T>                    // alignof(T)
std::rank_v<T>                            // array dimensions
std::extent_v<T, N = 0>                   // Nth dimension size
```

**Deprecated / removed — avoid in new code:**

```cpp
std::is_pod<T>                            // deprecated C++20, use is_standard_layout && is_trivially_copyable
std::is_literal_type<T>                   // removed C++20
std::is_trivial<T>                        // deprecated C++26
std::aligned_storage<Len, Align>          // deprecated C++23, use alignas + std::byte[]
std::aligned_union<Len, Types...>         // deprecated C++23
std::result_of<F>                         // removed C++20, use invoke_result
```

## Utility (`<utility>`)

```cpp
std::move(x)                              // cast to rvalue reference
std::forward<T>(x)                        // perfect-forward a forwarding reference
std::move_if_noexcept(x)                  // move if safe, copy otherwise
std::exchange(obj, new_val)               // replace and return old
std::swap(a, b)                           // default two-move swap
std::declval<T>()                         // unevaluated expression of type T&&

// Compile-time integer sequences
std::index_sequence<N...>                 // compile-time unpacking
std::make_index_sequence<N>
std::index_sequence_for<T...>
std::integer_sequence<T, N...>            // C++14

// C++23
std::to_underlying(e)                     // enum → underlying type
```

## Memory (`<memory>`)

**The allocator interaction layer — ALWAYS use traits, never raw allocator:**

```cpp
std::allocator_traits<Alloc>              // the universal adapter
//  Key members:
//    ::allocate(a, n)
//    ::deallocate(a, p, n)
//    ::construct(a, p, args...)
//    ::destroy(a, p)
//    ::max_size(a)
//    ::select_on_container_copy_construction(a)
//    ::propagate_on_container_copy_assignment
//    ::propagate_on_container_move_assignment
//    ::propagate_on_container_swap
//    ::is_always_equal

std::pointer_traits<Ptr>                  // normalize fancy pointers
std::to_address(p)                        // fancy pointer → raw pointer, C++20
```

**Object lifetime primitives:**

```cpp
std::construct_at(p, args...)             // placement-new replacement, C++20
std::destroy_at(p)                        // explicit destructor call, C++17
std::destroy(first, last)                 // range destroy, C++17
```

**Uninitialized memory algorithms:**

```cpp
std::uninitialized_copy(first, last, dest)
std::uninitialized_copy_n(first, n, dest)
std::uninitialized_move(first, last, dest)              // C++17
std::uninitialized_move_n(first, n, dest)               // C++17
std::uninitialized_fill(first, last, value)
std::uninitialized_fill_n(first, n, value)
std::uninitialized_default_construct(first, last)       // C++17
std::uninitialized_default_construct_n(first, n)        // C++17
std::uninitialized_value_construct(first, last)         // C++17
std::uninitialized_value_construct_n(first, n)          // C++17
```

**Smart pointers (for container-internal use when appropriate):**

```cpp
std::unique_ptr<T, Deleter>               // exclusive ownership
std::shared_ptr<T>                        // shared ownership
std::weak_ptr<T>                          // non-owning observer
std::make_unique<T>(args...)              // C++14
std::make_shared<T>(args...)
```

## Iterator (`<iterator>`)

```cpp
std::iterator_traits<I>                   // unified access point
//  ::value_type, ::difference_type, ::pointer, ::reference, ::iterator_category

std::iter_value_t<I>                      // C++20
std::iter_difference_t<I>                 // C++20
std::iter_reference_t<I>                  // C++20
std::iter_rvalue_reference_t<I>           // C++20
std::iter_common_reference_t<I>           // C++20

std::advance(it, n)
std::distance(first, last)
std::next(it, n=1)
std::prev(it, n=1)

std::reverse_iterator<I>
std::make_reverse_iterator(it)
std::move_iterator<I>
std::make_move_iterator(it)

std::back_inserter(c)
std::front_inserter(c)
std::inserter(c, pos)

// C++20 iterator concepts (also in <iterator>)
std::input_iterator<I>
std::forward_iterator<I>
std::bidirectional_iterator<I>
std::random_access_iterator<I>
std::contiguous_iterator<I>
```

**Iterator tag types:**

```cpp
std::input_iterator_tag
std::forward_iterator_tag
std::bidirectional_iterator_tag
std::random_access_iterator_tag
std::contiguous_iterator_tag              // C++20
```

## Algorithm (`<algorithm>`)

```cpp
std::copy(first, last, dest)
std::copy_n(first, n, dest)
std::copy_backward(first, last, dest)
std::move(first, last, dest)
std::move_backward(first, last, dest)
std::fill(first, last, value)
std::fill_n(first, n, value)
std::equal(first1, last1, first2)
std::lexicographical_compare_three_way    // for operator<=>
std::remove(first, last, value)
std::remove_if(first, last, pred)
std::rotate(first, mid, last)
std::swap_ranges(first1, last1, first2)
std::min(a, b) / std::max(a, b)
std::minmax(a, b)
std::clamp(v, lo, hi)                     // C++17
```

## Ranges (`<ranges>`) — C++20

```cpp
std::ranges::begin(r) / end(r)            // CPOs
std::ranges::cbegin(r) / cend(r)          // C++20
std::ranges::rbegin(r) / rend(r)
std::ranges::crbegin(r) / crend(r)
std::ranges::size(r)                      // CPO
std::ranges::ssize(r)                     // C++20
std::ranges::empty(r)
std::ranges::data(r)
std::ranges::range_value_t<R>
std::ranges::range_difference_t<R>
std::ranges::range_reference_t<R>
std::ranges::input_range<R>
std::ranges::forward_range<R>
std::ranges::bidirectional_range<R>
std::ranges::random_access_range<R>
std::ranges::contiguous_range<R>
std::ranges::sized_range<R>
std::ranges::borrowed_range<R>
std::ranges::view<R>
```

## Concepts (`<concepts>`) — C++20

Reference: https://en.cppreference.com/w/cpp/concepts

### Core concepts

```cpp
std::same_as<T, U>
std::derived_from<D, B>
std::convertible_to<From, To>
std::constructible_from<T, Args...>
```

### Object concepts — from construction to composition

```cpp
// Individual properties (building blocks)
std::destructible<T>
std::default_initializable<T>             // Named Requirement: DefaultConstructible
std::move_constructible<T>                // Named Requirement: MoveConstructible
std::copy_constructible<T>                // Named Requirement: CopyConstructible (weaker)
std::assignable_from<T, U>                // Named Requirement: MoveAssignable / CopyAssignable
std::swappable<T>

// Composite concepts (prefer these over manual composition)
std::movable<T>                           // = move_constructible + assignable_from<T&, T> + swappable
std::copyable<T>                          // = movable + copy_constructible + assignable_from<T&, const T&>
std::semiregular<T>                       // = copyable + default_initializable
std::regular<T>                           // = semiregular + equality_comparable
```

> **Named Requirements vs Concepts**: cppreference documents two parallel systems.
> [Named Requirements](https://en.cppreference.com/w/cpp/named_req) (pre-C++20) describe
> semantic contracts including runtime postconditions. C++20
> [Concepts](https://en.cppreference.com/w/cpp/concepts) are compile-time constraints.
> They are related but not identical — e.g. `std::copy_constructible` is syntactic only,
> while the `CopyConstructible` named requirement additionally requires `MoveConstructible`.
> Our `chaistl::concepts::*` modules express named requirements as C++20 concepts.

### Comparison concepts

```cpp
std::equality_comparable<T>
std::totally_ordered<T>
std::three_way_comparable<T>              // C++20, for <=>
std::strict_weak_order<F, T, U>
```

### Callable concepts

```cpp
std::invocable<F, Args...>
std::regular_invocable<F, Args...>        // equality-preserving
std::predicate<F, Args...>
```

## Compare (`<compare>`) — C++20

```cpp
std::strong_ordering
std::weak_ordering
std::partial_ordering
std::compare_three_way{}                  // default <=> functor
std::strong_order(x, y)                   // algorithm customization point
std::weak_order(x, y)
std::partial_order(x, y)
std::compare_strong_order_fallback(x, y)  // C++20
```

## Numeric (`<limits>`, `<cmath>`, `<numeric>`)

```cpp
std::numeric_limits<T>::max()
std::numeric_limits<T>::min()
std::numeric_limits<T>::lowest()
std::numeric_limits<T>::digits
std::numeric_limits<T>::is_integer
std::numeric_limits<T>::is_signed

std::midpoint(a, b)                       // C++20, avoids overflow
std::lerp(a, b, t)                        // C++20, linear interpolation
std::gcd(m, n)                            // C++17
std::lcm(m, n)                            // C++17
```

## Functional (`<functional>`)

```cpp
std::less<T>{}
std::greater<T>{}
std::equal_to<T>{}
std::not_equal_to<T>{}
std::less_equal<T>{}
std::greater_equal<T>{}
std::hash<T>                              // hash function object
std::reference_wrapper<T>                 // copyable reference
std::ref(x) / std::cref(x)
std::bind_front(f, args...)               // C++20
std::not_fn(f)                            // C++17
std::invoke(f, args...)                   // C++17, unified call syntax
```

## Exception (`<stdexcept>`, `<exception>`)

```cpp
std::out_of_range("...")
std::length_error("...")
std::bad_alloc
std::bad_array_new_length
std::exception_ptr                          // C++11
std::current_exception()
std::rethrow_exception(ep)
```

## Type Support (`<cstddef>`, `<typeinfo>`)

```cpp
std::size_t / std::ptrdiff_t
std::align_val_t                          // aligned allocation, C++17
std::byte                                 // raw storage, C++17
std::nullptr_t
std::max_align_t
```

## Compile-Time Introspection (`<type_traits>` misc)

```cpp
std::is_constant_evaluated()              // C++20, detect constexpr context
```

---

## Other Headers Worth Exploring

Beyond what's listed above, these headers contain facilities that become
relevant as chaistl grows. They are **not immediate priorities** for the
current container set (vector, deque, list, array, stack), but should be
revisited when extending into the areas noted.

### Priority 1 — Near-term (next 2–3 component types)

| Header | What to look for | When it matters |
|--------|-----------------|-----------------|
| `<tuple>` | `std::tuple`, `tuple_size`, `tuple_element`, `get<I>`, `apply` | Implementing `pair`, structured binding support, unpacking parameter packs into constructors |
| `<optional>` | `std::optional`, `nullopt`, `make_optional`, `value_or` | `optional<T>` container; also useful for allocator operations that may fail |
| `<variant>` | `std::variant`, `visit`, `get`, `holds_alternative`, `monostate` | `any` or `expected<T, E>` implementations |
| `<compare>` | `strong_order`, `weak_order`, `partial_order`, `compare_strong_order_fallback` | Full three-way comparison for custom types beyond what's already listed |

### Priority 2 — Medium-term (algorithm library, string containers, diagnostics)

| Header | What to look for | When it matters |
|--------|-----------------|-----------------|
| `<string_view>` | `std::string_view` | Non-owning string reference for `basic_string`-like containers |
| `<span>` | `std::span`, `dynamic_extent` | C++20 contiguous range view — conceptual model for iterator + size pairs |
| `<bit>` | `bit_cast`, `has_single_bit`, `countl_zero`, `popcount`, `byteswap`, `bit_ceil`, `bit_floor`, `bit_width` | Hash functions, bucket size calculations, low-level memory optimizations |
| `<format>` | `std::format`, `formatter` | C++20 debugging output, custom container formatting |
| `<source_location>` | `source_location::current()` | C++20 assertion/concept failure diagnostics |
| `<version>` | Feature-test macros (`__cpp_lib_*`) | Conditional compilation for compiler-specific capabilities |

### Priority 3 — Long-term (specialized containers, testing infrastructure)

| Header | What to look for | When it matters |
|--------|-----------------|-----------------|
| `<numbers>` | `numbers::pi`, `numbers::e` | Math constants for numeric algorithm containers |
| `<chrono>` | `chrono::duration`, `chrono::time_point` | Time-aware containers or benchmark timing |
| `<random>` | `random_device`, distribution types | Fuzzing infrastructure, randomized test data |
| `<stacktrace>` | `stacktrace`, `stacktrace_entry` | C++23 assertion backtraces |
| `<expected>` | `expected<T, E>`, `unexpected` | C++23 error-handling containers |
| `<print>` | `print`, `println` | C++23 diagnostic output (simpler than iostream) |
| `<syncstream>` | `osyncstream` | Thread-safe logging for concurrent container tests |
