# Iterator Interface

## Status

Accepted (2026-06-10)

## Context

Every container needs both `iterator` and `const_iterator`. The project
evaluated four implementation techniques:

1. **Handwritten pair**: `ListIterator` + `ListConstIterator` (libstdc++ style)
2. **Bool parameterization**: `iterator_impl<Const>` (single template, `std::conditional_t`)
3. **CRTP facade**: `iterator_facade<Derived, T, Category>` (Boost style)
4. **Deducing this**: C++23 `this auto&& self` (explicit object parameter)

See [`background/iterator-patterns.md`](../background/iterator-patterns.md) for
detailed analysis of all four.

## Decision

Use **two composable mechanisms**:

### 1. `iterator_impl<Const>` — const/non-const generation

A single template parameterizes `iterator`/`const_iterator` from one definition:

```cpp
template <bool Const>
class list::iterator_impl : public chaistl::iterator_interface<std::bidirectional_iterator_tag> {
    using node_ptr = std::conditional_t<Const, const node*, node*>;
    node_ptr node_;
    // ... navigation kernel ...
};

using iterator = iterator_impl<false>;
using const_iterator = iterator_impl<true>;
```

### 2. `iterator_interface<IteratorConcept>` — boilerplate deduction

A deducing-this base class derives mechanical operations from core primitives:

| Derived provides | `iterator_interface` deduces |
|-----------------|------------------------------|
| `operator*()` | `operator->()` |
| `operator++()` | `operator++(int)` |
| `operator--()` | `operator--(int)` |
| `operator+=()` | `operator+(it, n)`, `operator-(it, n)`, `operator[]` |

```cpp
template <class IteratorConcept>
struct iterator_interface {
    [[nodiscard]] constexpr auto operator->(this const auto& self)
        requires requires { std::addressof(*self); }
    { return std::addressof(*self); }

    [[nodiscard]] constexpr auto operator++(this auto& self, int)
        requires (std::copy_constructible<std::remove_cvref_t<decltype(self)>> &&
                  requires { ++self; })
    { auto tmp = self; ++self; return tmp; }

    // ... operator--(int), operator+, operator-, operator[] ...
};
```

Containers keep their navigation kernel and inherit the rest:

```cpp
template <bool Const>
class list::iterator_impl : public chaistl::iterator_interface<std::bidirectional_iterator_tag> {
    [[nodiscard]] constexpr reference operator*() const noexcept {
        return static_cast<node*>(node_)->value;
    }
    constexpr iterator_impl& operator++() noexcept {
        node_ = node_->next;
        return *this;
    }
    // ...
    using iterator_interface::operator++;  // unhide postfix
};
```

## Consequences

### Positive

- Each container's iterator shrank by 8–20 lines of mechanical code
- Single source of truth for postfix `++`/`--`, `->`, `+`, `-`, `[]`
- No drift: all iterators inherit the same implementations
- Aligns with P2727R4 (`std::iterator_interface`), a WG21 library proposal
  targeting C++29

### Negative

- `using iterator_interface::operator++;` is a small naming ceremony
- `operator==` / `operator<=>` do not use `= default` because defaulted
  comparisons include the base-class subobject in memberwise comparison.
  An explicit body comparing only the position state is required.
- Random-access `operator+`/`operator-`/`operator[]` stay explicit in deque
  because `iterator_interface`'s versions do not provide `n + it` (symmetric
  addition). Deducing-this members are not found by ADL for the left-hand
  operand.

## Migration Log

| Container | Migrated | Lines Removed |
|-----------|----------|---------------|
| `bst_iterator` (tree) | 2026-06-10 | 3 |
| `list::iterator_impl` | 2026-06-10 | 3 |
| `forward_list::iterator_impl` | 2026-06-10 | 2 |
| `deque::iterator_impl` | 2026-06-10 | 3 |

## References

- P2727R4: `std::iterator_interface` (WG21 open proposal, C++29 targeted)
- P0847R7: Deducing This (C++23)
- [`patterns/iterators.md`](../patterns/iterators.md)
- [`background/iterator-patterns.md`](../background/iterator-patterns.md)
