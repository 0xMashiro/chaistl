# Iterator Implementation Patterns

## Current State

chaistl uses two composable mechanisms:

1. **`iterator_impl<Const>`** — a single class template parameterized on a
   compile-time `bool` that produces both `iterator` and `const_iterator`.
   Used by `list`, `forward_list`, `deque`, and tree iterators.

2. **`iterator_interface<IteratorConcept>`** — a deducing-this base class that
   derives postfix `++`/`--`, `operator->`, `operator+`/`-`, and `operator[]`
   from the container's core primitives (`++`, `--`, `*`, `+=`).

Both mechanisms are orthogonal and compose directly.

### Required typedefs for every derived iterator

`iterator_interface` uses the derived class's nested `difference_type` for
`operator+`, `operator-`, and `operator[]`. Every iterator must therefore
provide the full set of iterator typedefs:

```cpp
using value_type        = T;
using difference_type   = std::ptrdiff_t;   // or allocator_traits::difference_type
using iterator_concept  = std::bidirectional_iterator_tag;   // C++20 ranges
using iterator_category = std::bidirectional_iterator_tag;   // legacy interop
using reference         = std::conditional_t<Const, const T&, T&>;
using pointer           = std::conditional_t<Const, const T*, T*>;
```

`iterator_concept` is required for C++20 ranges concepts;
`iterator_category` is still valuable for legacy algorithms and
`std::iterator_traits` stability.

---

## `iterator_impl<Const>` — iterator/const_iterator generation

```cpp
template <bool Const>
class iterator_impl {
    using pointer_type = std::conditional_t<Const, const_pointer, pointer>;
    using ref_type    = std::conditional_t<Const, const_reference, reference>;
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using iterator_concept  = std::bidirectional_iterator_tag;
    using value_type        = T;
    using difference_type   = ...;
    using reference         = ref_type;
    using pointer           = pointer_type;

    // Implicit conversion: non-const → const
    constexpr iterator_impl(const iterator_impl<false>& other) noexcept
        requires Const
        : ... {}
};

using iterator       = iterator_impl<false>;
using const_iterator = iterator_impl<true>;
```

**Trade-offs**: single definition, no duplication between iterator and
const_iterator, but `conditional_t` has to appear for every typedef and
cross-const comparison needs `template <bool OtherConst>` friend operators.

---

## `iterator_interface` — boilerplate deduction

```cpp
template <bool Const>
class list::iterator_impl : public chaistl::iterator_interface<std::bidirectional_iterator_tag> {
public:
    // ... typedefs, constructors ...

    // Core primitives — list-specific, not deducible
    [[nodiscard]] constexpr reference operator*() const noexcept {
        return static_cast<node*>(node_)->value;
    }
    constexpr iterator_impl& operator++() noexcept {
        node_ = node_->next;
        return *this;
    }
    constexpr iterator_impl& operator--() noexcept {
        node_ = node_->prev;
        return *this;
    }

    // Bring in deduced postfix forms (name hiding)
    using iterator_interface::operator++;
    using iterator_interface::operator--;

    // Comparison stays explicit (operator== cannot use = default because
    // defaulted comparison includes the iterator_interface base subobject,
    // which has no meaningful equality state)
    template <bool OtherConst>
    friend constexpr bool operator==(const iterator_impl& lhs,
                                     const iterator_impl<OtherConst>& rhs) noexcept {
        return lhs.node_ == rhs.node_;
    }
};
```

**What `iterator_interface` provides**:

| Operation | Derived from | Available when |
|-----------|-------------|----------------|
| `operator->()` | `operator*()` | always (real reference only) |
| `operator++(int)` | `operator++()` | always (requires copyable) |
| `operator--(int)` | `operator--()` | bidirectional+ (requires copyable) |
| `operator+(it, n)` | `operator+=()` | random-access |
| `operator-(it, n)` | `operator-=` or `operator+=()` | random-access |
| `operator[](n)` | `operator+()` + `operator*()` | random-access |

**Note**: `operator!=` is **not** provided by `iterator_interface`. In C++20,
it is generated automatically from `operator==` via rewritten comparison
candidates. Do not add an explicit `operator!=` to either the facade or the
derived class.

**Caveat for random-access iterators**: `iterator_interface::operator+` uses
the derived class's nested `difference_type` as its offset type. Random-access
iterators therefore keep explicit `operator+`, `operator-`, and `operator[]`
that also provide `n + it` (symmetric addition), which **cannot** be deduced
from a member `operator+` because deducing-this members are not found by ADL
for the left-hand operand. A random-access iterator that needs to satisfy
`std::random_access_iterator` must provide its own hidden friend or
namespace-level overload:

```cpp
[[nodiscard]] constexpr iterator_impl operator+(difference_type offset) const noexcept {
    auto copy = *this;
    copy += offset;
    return copy;
}

[[nodiscard]] friend constexpr iterator_impl operator+(difference_type offset,
                                                       const iterator_impl& it) noexcept {
    return it + offset;
}
```

---

## Why Not Full Deducing This?

A pure deducing-this approach eliminates `bool Const` entirely:

```cpp
class list_iterator {
    node_base* node_{nullptr};
public:
    template <typename Self>
    decltype(auto) operator*(this Self&& self) {
        return *(static_cast<node_type*>(self.node_));
    }
    // ...
};

using iterator       = list_iterator;
using const_iterator = list_iterator;  // same type
```

This was evaluated and rejected because:
- `iterator` and `const_iterator` being the same type breaks code that relies
  on them being distinct (e.g. `std::same_as<It, const It>` checks)
- Cross-const comparison becomes implicit, changing overload resolution
- Higher risk than composing `iterator_impl<Const>` with `iterator_interface`

See [ADR 001](../decisions/001-iterator-impl-pattern.md) and
[ADR 006](../decisions/006-iterator-interface.md) for full analysis.

---

## Iterator Traits for Raw Pointers

For contiguous containers backed by raw pointers, `iterator_traits<T*>` is
already specialized. Use it directly:

```cpp
using difference_type = std::iter_difference_t<Iterator>;
using value_type      = std::iter_value_t<Iterator>;
using reference       = std::iter_reference_t<Iterator>;
```

## Reverse Iterator

Wrap with `chaistl::reverse_iterator<Iter>`. Provide `rbegin()`/`rend()` +
`crbegin()`/`crend()`. The standard `reverse_iterator` model: `*rit` makes a
**copy** of the stored base iterator, decrements the copy, and dereferences it.
It does **not** modify `base_`:

```cpp
constexpr decltype(auto) operator*() const {
    Iterator tmp = base_;
    return *--tmp;
}
```

`operator[]` is only available when the underlying iterator satisfies
`std::random_access_iterator`:

```cpp
constexpr decltype(auto) operator[](difference_type n) const
    requires std::random_access_iterator<Iterator>
{
    return base_[-n - 1];
}
```

## Move Iterator

Use `std::make_move_iterator(it)` when constructing from an rvalue range or
when moving elements between containers with different allocators.

## `iter_move` / `iter_swap` for Custom Iterators

If a custom iterator needs to participate in `std::ranges` algorithms,
provide `iter_move` and `iter_swap` as **hidden friends** (or namespace-level
functions found by ADL). They are customization points for ranges CPOs, not
overloads inside `namespace std`:

```cpp
friend constexpr decltype(auto) iter_move(const my_iterator& it)
    noexcept(noexcept(std::ranges::iter_move(it.base())))
{
    return std::ranges::iter_move(it.base());
}

template <class Other>
friend constexpr void iter_swap(const my_iterator& lhs, const Other& rhs)
    noexcept(noexcept(std::ranges::iter_swap(lhs.base(), rhs)))
    requires std::indirectly_swappable<Iterator, Other>
{
    std::ranges::iter_swap(lhs.base(), rhs);
}
```

## Compile-Time Negative Tests

In addition to runtime tests, add `static_assert` negative tests to verify
that iterators do not expose operations they should not:

```cpp
template <class I>
concept has_post_decrement = requires(I i) { i--; };

template <class I>
concept has_subscript = requires(I i, typename I::difference_type n) { i[n]; };

static_assert(!has_post_decrement<chaistl::forward_list<int>::iterator>);
static_assert(!has_subscript<chaistl::list<int>::iterator>);
static_assert(std::bidirectional_iterator<chaistl::list<int>::iterator>);
static_assert(std::forward_iterator<chaistl::forward_list<int>::iterator>);
static_assert(std::random_access_iterator<chaistl::deque<int>::iterator>);

// Cross-const comparison
static_assert(std::sentinel_for<
    chaistl::list<int>::const_iterator,
    chaistl::list<int>::iterator>);

chaistl::list<int> xs{1, 2, 3};
chaistl::list<int>::iterator it = xs.begin();
chaistl::list<int>::const_iterator cit = xs.cbegin();
static_assert(requires { it == cit; });
static_assert(requires { cit == it; });
```
