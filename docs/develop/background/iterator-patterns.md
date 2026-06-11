# Iterator Implementation Patterns

This document records the design space for implementing C++ iterators inside
chaistl. It is not a specification — it is a historical and comparative
analysis of four common approaches, with notes on why chaistl made the choices
it did.

The goal is to help readers understand that **there is no single "right" way**;
each technique reflects the language features and design priorities available at
the time it was developed.

---

## 1. Handwritten Pair (libstdc++ style)

Two completely separate classes: `iterator` and `const_iterator`.

```cpp
template <class T>
struct list_iterator {
    using reference = T&;
    using pointer   = T*;
    // ... all operators ...
    node_base* node_;
};

template <class T>
struct list_const_iterator {
    using reference = const T&;
    using pointer   = const T*;
    // ... identical operators, repeated ...
    const node_base* node_;

    // conversion from iterator
    list_const_iterator(const list_iterator<T>& other) noexcept;
};
```

### Historical context

This is the approach used by the GNU libstdc++ `std::list` implementation.
It predates modern template metaprogramming and was chosen for simplicity
and debuggability.

### Pros

- Zero template complexity; easy to step through in a debugger.
- Error messages are human-readable.
- Each type is self-contained; no hidden base-class behaviour.

### Cons

- ~2× code duplication. Every operator (`++`, `--`, `++(int)`, `->`, `==`)
  is written twice.
- Easy for the two types to drift out of sync during maintenance.
- Does not scale: four containers × two iterator types = eight classes to
  maintain, even though the logic is identical.

### When to study this

If you are new to iterator implementation, start here. Write a complete
`iterator` and `const_iterator` for a singly-linked list by hand. The
repetition is frustrating, but it makes the motivation for the later
abstractions viscerally clear.

---

## 2. Bool Parameterization (`iterator_impl<Const>`)

A single class template parameterized on `bool Const`.

```cpp
template <bool Const>
class iterator_impl {
    using reference = std::conditional_t<Const, const T&, T&>;
    using pointer   = std::conditional_t<Const, const T*, T*>;
    // ... operators defined once ...

    // implicit conversion: non-const → const
    constexpr iterator_impl(const iterator_impl<false>& other) noexcept
        requires Const;
};

using iterator       = iterator_impl<false>;
using const_iterator = iterator_impl<true>;
```

### Historical context

This pattern became popular in C++11/14 libraries. It exploits
`std::conditional_t` (C++11) and `requires` (C++20) to collapse two classes
into one template. It is the pattern currently used by chaistl for `list`,
`forward_list`, `deque`, and the tree iterator.

### Pros

- Single definition; no code duplication.
- `conditional_t` is a standard, well-understood mechanism.
- The `Const` parameter makes the intent explicit.

### Cons

- `conditional_t` appears repeatedly (reference, pointer, stored pointer type),
  which is noisy.
- Cross-const comparison requires `template <bool OtherConst>` friend operators.
- The abstraction is **ad-hoc**: every container writes its own
  `iterator_impl<Const>` from scratch. There is no shared base.

### Why chaistl uses this today

It is the sweet spot for a C++23 learning library:

- Simpler than CRTP (no `static_cast<Derived&>` gymnastics).
- More portable than deducing this (no compiler-edge-case risk).
- Still demonstrates modern features (`conditional_t`, `requires`,
  cross-const friends).

---

## 3. CRTP Facade (`iterator_facade<Derived, T, Category>`)

A base class uses the Curiously Recurring Template Pattern to generate
standard operators from a small set of core primitives.

```cpp
template <class Derived, class T, class Category, class Diff = std::ptrdiff_t>
struct iterator_facade {
    constexpr Derived operator++(int) {
        auto copy = static_cast<Derived&>(*this);
        ++static_cast<Derived&>(*this);
        return copy;
    }

    constexpr auto operator->() const {
        return std::addressof(*static_cast<const Derived&>(*this));
    }
    // ... similar for --(int), comparisons, etc.
};

class list_iterator
    : public iterator_facade<list_iterator, T, std::bidirectional_iterator_tag>
{
    // Only provide: operator*, operator++, operator--, base()
};
```

### Historical context

CRTP dates to the early 1990s but became widespread in C++98/03 with
Boost.Iterator's `iterator_facade`. It is the industrial-strength solution
used by Boost, Folly, and parts of libc++.

ChaiGO (a non-open-source predecessor project to chaistl) used CRTP for its
`CircularIterator` but kept handwritten pairs for `ListIterator`, suggesting
that the author viewed CRTP as powerful but not universally applicable.
ChaiGO's source is not publicly available; this note is preserved for
historical context.

### Pros

- **Reusable across containers**: the same `iterator_facade` works for list,
  deque, tree, and forward_list iterators.
- **Separation of concerns**: core data-structure logic (how to advance)
  vs. standard boilerplate (post-increment, `operator->`).
- **Extensible**: adding `operator[]` for random-access iterators only
  requires a conditional base-class member.

### Cons

- **Higher cognitive load**: `static_cast<Derived&>(*this)` is non-obvious
  to beginners. The CRTP contract must be explained.
- **Heavier template machinery**: longer compile times and error messages
  compared to the bool-parameterization approach.
- **Friend operators across different `Derived` types** are more complex
  than cross-`Const` friends.

### When chaistl might adopt this

If the library grows to 6+ container iterators and the duplication of
`operator++(int)`, `operator->`, and comparison operators becomes painful,
a shared `iterator_facade` becomes worthwhile. The migration path is
mechanical: each `iterator_impl<Const>` becomes a thin derived class
providing only the core primitives.

---

## 4. Deducing This (C++23)

Use an explicit object parameter (`this auto&& self`) to unify const and
non-const members in a single class, without any `Const` template parameter.

```cpp
struct list_iterator {
    node_base* node_;

    // Return type deduced from self's constness
    [[nodiscard]] constexpr auto&& operator*(this auto&& self) noexcept {
        return static_cast<...>(self.node_)->value;
    }

    // Post-increment: works for both const and non-const *this
    constexpr auto operator++(this auto self, int) noexcept {
        auto copy = self;
        ++self;
        return copy;
    }

    // No conversion constructor needed: non-const → const is implicit
};
```

### Historical context

P0847 (Deducing This) was voted into C++23. It is the newest technique in
this list. GCC 14 and Clang 18+ support it; the project's CI baseline is
GCC 14+ / Clang 22+. Edge cases (`operator->` return-type deduction, friend
cross-type comparisons) are still stabilizing in some toolchains.

### Pros

- **No `Const` parameter, no `conditional_t`**: the cleanest code of the four.
- **Implicit const conversion**: no manual conversion constructor.
- **Natural teaching path**: "the return type depends on how you call it" is
  an intuitive concept.

### Cons

- **Compiler maturity**: debuggers, IDEs, and static analyzers lag behind
  the language feature.
- **Error messages**: `auto&& self` deduction failures produce verbose
  template diagnostics.
- **Not yet proven at scale**: no major standard-library implementation uses
  deducing this for iterators in production.

### When chaistl might adopt this

After C++26 is widely available and the feature has proven stable in
industrial codebases. Until then, it is an excellent **experimental**
teaching topic but not a production default.

---

## Comparative Summary

| Approach | Duplication | Abstraction Complexity | Compiler Maturity | Teaching Value |
|----------|------------|------------------------|-------------------|----------------|
| Handwritten pair | High | None | Universal | Foundational |
| `iterator_impl<Const>` | None | Low | Universal | Good |
| CRTP facade | None | Medium | Universal | High |
| Deducing this | None | Low (conceptually) | Maturing | High (future) |

---

## Chaistl's Current Position and Future Path

**Today**: chaistl uses `iterator_impl<Const>` for `list`, `forward_list`,
`deque`, and the tree iterator. This was a pragmatic starting point — simple
enough to ship, using well-understood mechanisms (`std::conditional_t`,
`requires`).

**Likely direction**: the project is evaluating a move to **deducing this**
(C++23 explicit object parameter). Deducing this eliminates `conditional_t`
boilerplate entirely and makes `iterator` and `const_iterator` the same type.
The project's C++23 baseline (GCC 14+, Clang 22+ in CI) makes this feasible,
and recent compiler support has matured enough for a learning project.
The language feature itself is available from Clang 18+ and GCC 14+.

See [ADR 001](../decisions/001-iterator-impl-pattern.md) for the decision
record and [`patterns/iterators.md`](../patterns/iterators.md) for
implementation guidance.

**Future options** (not mutually exclusive):

1. **Migrate to deducing this** (likely): significantly cleaner code, fewer
   template instantiations, no `conditional_t` repetition. Likely the next
   step.

2. **Stay with `iterator_impl<Const>`**: a fallback if compiler issues surface.
   Perfectly valid for the current scale.

3. **CRTP `iterator_facade`**: worthwhile if the library grows to 10+
   iterators or an intrusive / concurrent layer is added. Lower priority than
   deducing this.

---

## References

- **libstdc++ `std::list`** — handwritten pair approach.
- **Boost.Iterator `iterator_facade`** — CRTP approach.
  https://www.boost.org/doc/libs/release/libs/iterator/
- **P0847R7** — Deducing This. https://wg21.link/p0847r7
- **ChaiGO `CircularIterator`** — CRTP practice in a non-open-source
  predecessor project. The source path referenced here
  (`reference/ChaiGO/…`) is not publicly accessible; this entry is kept
  for historical completeness.
