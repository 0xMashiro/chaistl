# Testing Generic Code

## The Tracker Type Pattern

Generic containers must be exercised with types that expose misuse, not just
`int` and `std::string`. The standard technique is an instrumented "tracker"
type that counts every operation:

```cpp
struct tracked {
    int value{};
    static inline int alive = 0;
    static inline int copies = 0;
    static inline int moves = 0;

    tracked() { ++alive; }
    tracked(const tracked& other) : value(other.value) { ++alive; ++copies; }
    tracked(tracked&& other) noexcept : value(other.value) { other.value = 0; ++alive; ++moves; }
    ~tracked() { --alive; }
};
```

Use these to verify:
- No leaks: `alive == 0` after destruction
- Move optimization: `moves > 0` when `push_back` triggers reallocation
- No unnecessary copies: `copies == expected_count`

## Exception Injection

To verify exception safety, inject failures at controlled points:

```cpp
struct throwing_tracker {
    static inline int copies_before_throw = -1;  // -1 = never throw
    static inline int copy_count = 0;

    throwing_tracker(const throwing_tracker&) {
        ++copy_count;
        if (copy_count == copies_before_throw) throw std::runtime_error("injected");
    }
};
```

Test pattern:
```cpp
throwing_tracker::copies_before_throw = 3;  // fail on 3rd copy
EXPECT_THROW(vec.push_back(value), std::runtime_error);
EXPECT_EQ(vec.size(), old_size);  // strong guarantee: size unchanged
```

## Dimension Coverage Matrix

Every container operation should be tested against these type dimensions:

| Dimension | Test type | Verifies |
|-----------|-----------|----------|
| Trivial | `int` | fast paths (memmove, skip-destructor) |
| Movable only | `std::unique_ptr<int>` | move-semantics without copy fallback |
| Copyable only | type with `= delete` move | copy fallback in `move_if_noexcept` |
| Non-trivial dtor | `tracked` (above) | destroy-reverse rollback, leak check |
| Throwing copy | `throwing_tracker` | exception safety guarantees |
| Throwing move | type with throwing move ctor | `move_if_noexcept` → copy path |
| Empty type | `std::is_empty_v<T> == true` | EBO, zero-size optimization |
| Const element | `const int` (where allowed) | const-correctness of accessors |

Not every operation needs every dimension. Prioritize: trivial, tracked,
throwing_copy. The others are targeted at specific optimizations.

## Compile-Fail Tests

For constraints that must reject invalid template arguments at compile time:

```cpp
// compile_fail/vector/allocator_value_type.cpp
// This file should NOT compile — the concept constraint must fire.
chaistl::vector<int, std::allocator<double>> v;  // value_type mismatch
```

These verify that `requires` clauses and `static_assert` constraints work.
Each concept should have at least one corresponding negative compile test.

## Interop Tests

Standard-compatible chaistl containers should have at least one focused
interop path against the matching `std` container where the standard library
offers the same interface on the supported toolchains. These tests use
identical operations to verify behavioral equivalence for representative
workflows:

```cpp
TYPED_TEST(VectorCompatibilityTest, ModifiersPreserveStandardOrderSemantics) {
    TypeParam values{1, 4};       // TypeParam is either std::vector or chaistl::vector
    values.insert(values.begin() + 1, 2);
    EXPECT_THAT(values, ElementsAre(1, 2, 4));
}
```

Interop tests catch deviations from standard-specified behavior without
needing to re-specify every test for each container. They complement, but do
not replace, focused chaistl-only tests for exception safety, allocator
interaction, compile-time constraints, heterogeneous lookup, and extension
APIs. When a container or overload lacks std interop coverage, keep the focused
test names specific enough that the missing comparison is visible during
review.

## Coverage

- Use coverage as a gap-finding tool, not as proof of correctness.
- 100% line coverage is not a goal. 100% **branch** coverage for the exception
  safety paths (the rollback branches in `if constexpr (needs_rollback)`) is.
