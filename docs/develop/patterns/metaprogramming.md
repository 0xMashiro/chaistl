# Metaprogramming Patterns

## `if constexpr` Over Tag Dispatch

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

## `void_t` Detection Idiom

When you need to check whether a type has a member without using concepts:

```cpp
template<typename T, typename = void>
struct has_data : std::false_type {};

template<typename T>
struct has_data<T, std::void_t<decltype(std::declval<T&>().data())>> : std::true_type {};
```

In C++20, prefer a `requires`-expression concept. Use `void_t` only when you
need SFINAE that concepts can't express.

## `noexcept` Propagation

Derive `noexcept` specifications from type traits, not hardcoded values:

```cpp
constexpr vector(vector&& other) noexcept;  // unconditionally noexcept

constexpr vector& operator=(vector&& other) noexcept(
    allocator_traits::propagate_on_container_move_assignment::value ||
    allocator_traits::is_always_equal::value);
```

## `consteval` for Compile-Time Computation

Use `consteval` for functions that must run at compile time:

```cpp
consteval std::size_t next_power_of_two(std::size_t n) {
    // only callable in constant expressions
}
```

## Deduction Guides

Every container that has an iterator-pair constructor needs deduction guides:

```cpp
template <std::input_iterator InputIt, class Alloc = allocator<std::iter_value_t<InputIt>>>
vector(InputIt, InputIt, Alloc = Alloc()) -> vector<std::iter_value_t<InputIt>, Alloc>;
```

This is how `vector v{begin, end}` works without explicitly specifying `T`.
