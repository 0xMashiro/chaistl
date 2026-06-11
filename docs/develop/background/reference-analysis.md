# Reference Implementation Analysis

This document records design decisions found in several reference
implementations that influence chaistl. It is a snapshot of observed patterns,
not a comparative ranking. Each library made different trade-offs based on its
goals: performance, standard compliance, zero-overhead, or education.

> **Note**: Section 4 covers **ChaiGO**, a non-open-source predecessor project
> to chaistl. Its design is discussed here because it directly shaped early
> chaistl decisions, but its source code is not publicly available.

---

## 1. EASTL (Electronic Arts Standard Template Library)

**Goal**: High-performance containers for game engines. Standard compliance is
a secondary concern; cache efficiency, debuggability, and deterministic
allocation matter more.

### Architecture: Base-Class Inheritance for Exception Safety

Every container has a `*Base` class that handles memory allocation and stores
core pointers. The derived container class handles element construction and the
public API.

```cpp
// vector.h:198
class vector : public VectorBase<T, Allocator> { ... };

// list.h:244
class list : public ListBase<T, Allocator> { ... };
```

**Rationale** (explicitly documented at `vector.h:117-143`):

> "The reason we do this is for exception safety. If an allocation throws in
the base constructor, the partially-constructed object is destroyed without
needing try/catch in the derived class."

This is a direct application of C++ standard §15.2/2 and §15.3/11: base-class
subobjects are destroyed before the constructor body runs.

### Memory Management: `compressed_pair` for EBO

`VectorBase` stores the allocator and raw pointers in a `compressed_pair`:

```cpp
// vector.h:164-166
compressed_pair<allocator_type, T*>  mpBegin;
T*                                   mpEnd;
T*                                   mpCapacity;
```

`compressed_pair` (defined in `bonus/compressed_pair.h`) applies Empty
Base-Class Optimization by inheriting from the allocator when it is stateless.
This avoids paying size for `std::allocator`-like empty types.

EASTL's allocator itself is non-templated (`eastl::allocator` allocates raw
bytes with debug naming), unlike `std::allocator<T>`.

### Iterator Design

| Container | Iterator | Notes |
|-----------|----------|-------|
| `vector` | `typedef T* iterator` | Raw pointer. Comment: "Do not write code that relies on iterator being T*" because debug iterators may come later. |
| `list` | `ListIterator<T, Pointer, Reference>` | Classic 3-param template. `Pointer`/`Reference` differentiate const/non-const. |
| `slist` | `SListIterator<T, Pointer, Reference>` | Same pattern. |
| `rbtree` | `rbtree_iterator<T, Pointer, Reference>` | Same pattern; `map`/`set` are thin wrappers. |

Conversion from non-const to const uses a templated constructor with
`enable_if_t`:

```cpp
// list.h:146-151
template <typename OtherPointer, typename OtherReference,
          enable_if_t<!is_same_v<this_type, iterator>, bool> = true>
ListIterator(const ListIterator<T, OtherPointer, OtherReference>& other);
```

### Key Decision: `rbtree` as Generic Base for All Tree Containers

`map`, `set`, `multimap`, `multiset` all inherit from `rbtree`, controlled by
policy flags:

```cpp
// red_black_tree.h:360-465
// ExtractKey: how to get a key from a value
// bMutableIterators: can iterators modify elements?
// bUniqueKeys: are duplicate keys allowed?
template <class Key, class Value, class ExtractKey,
          bool bMutableIterators, bool bUniqueKeys, ...>
class rbtree { ... };
```

This is policy-based design without virtual functions — all decisions are
compile-time.

### Trade-offs

| Pro | Con |
|-----|-----|
| Exception-safe by construction | Base-class inheritance adds cognitive load |
| `compressed_pair` gives zero-overhead EBO | Raw pointer iterators can't have debug checks in release |
| Debug naming built into allocator | Non-standard allocator interface |
| Policy-based tree is highly reusable | Heavy template machinery |

---

## 2. Boost.Container

**Goal**: Standard-compatible containers with additional features
(`flat_map`, `stable_vector`, etc.). Maximum code reuse via
Boost.Intrusive.

### Architecture: Composition + Intrusive Backend

Boost.Container does **not** implement node-based containers from scratch.
Instead, it builds on Boost.Intrusive:

```cpp
// list.hpp:86-93
// The underlying intrusive container
typedef typename dtl::bi::make_list<
    node_type,
    dtl::bi::base_hook<typename list_hook<void_pointer>::type>,
    dtl::bi::constant_time_size<true>,
    dtl::bi::size_type<typename allocator_traits_type::size_type>
>::type  container_type;
```

`list` inherits from `node_alloc_holder<Allocator, IntrusiveContainer>`,
which handles node allocation and derives from the rebound allocator for EBO.

### Memory Management: `node_alloc_holder`

`node_alloc_holder` (`detail/node_alloc_holder.hpp:198-253`) is a reusable
base for all node-based containers:

- Rebinds the allocator to `base_node<T, Hook>`
- Derives from the rebound allocator (EBO)
- Provides `create_node`, `destroy_node`, `allocate_node`, `deallocate_node`

The node itself contains a hook (for intrusive linking) plus aligned storage
for the value:

```cpp
// detail/node_alloc_holder.hpp:71-185
template <class T, class Hook>
struct base_node : public Hook {
    // aligned storage for T
    typename aligned_storage<sizeof(T), alignment_of<T>::value>::type m_storage;
};
```

### `flat_map` / `flat_set`: Sort a Vector

`flat_tree` holds a sequence container (default `vector`) and maintains sorted
order:

```cpp
// flat_tree.hpp:515-519
class flat_tree
    : private value_compare   // EBO for comparator
{
    container_type m_seq;  // e.g. vector<pair<Key, T>>
    // ...
};
```

No node allocation. Insertion is `O(n)` but cache-friendly.

### Iterator Design

| Container | Iterator |
|-----------|----------|
| `vector` | `vec_iterator<Pointer, IsConst>` — proper class, not raw pointer |
| `list` | `iterator_from_iiterator` — adapts Boost.Intrusive iterator |
| `flat_tree` | Reuses `container_type::iterator` directly |

`vec_iterator` is a class wrapping `Pointer m_ptr`, with conversion from
non-const via a `nonconst_iterator` typedef.

### Trade-offs

| Pro | Con |
|-----|-----|
| Maximum code reuse via Intrusive | Heavy metaprogramming and indirection |
| `node_alloc_holder` unifies all node containers | Harder to debug through layers |
| Flat containers are cache-friendly | `flat_map` insertion is linear |
| Extensive allocator version support | More complex than needed for learning |

---

## 3. Boost.Intrusive

**Goal**: Zero-allocation intrusive containers. The user owns the objects;
the container only manipulates pointers.

### Architecture: Pure Policy-Based Design

No inheritance, no allocation. The user-facing `slist<T, Options...>` is a
metafunction that packs options and instantiates `slist_impl`:

```cpp
// slist.hpp:76-84
// Options: base_hook<>, member_hook<>, constant_time_size<>, size_type<>, etc.
template <class T, class ...Options>
class slist : public slist_impl<T, Options...> { ... };
```

Options are merged via `pack_options` (`options.hpp:1-278`), which replaces
default values with user-provided ones.

### Memory Management: No Allocation

The container never allocates. `push_back` takes a reference:

```cpp
// list.hpp:277-283
void push_back(reference value) {
    this->icont().push_back(*this->create_node(value));
}
// Wait — actually, in pure intrusive, it's even simpler:
void push_back(reference value) {
    this->icont().push_back(this->priv_value_traits().to_node_ptr(value));
}
```

The user's type must contain a hook:

```cpp
// list_hook.hpp:84
struct my_type : public list_base_hook<> {
    int data;
};
```

Or embed a member hook:

```cpp
struct my_type {
    list_member_hook<> hook;
    int data;
};
```

### Iterator Design: `value_traits` Abstraction

Iterators are parameterized on `ValueTraits` and `IsConst`:

```cpp
// detail/slist_iterator.hpp:37-137
template <class ValueTraits, bool IsConst>
class slist_iterator {
    // node_ptr is stored; value_traits_ptr is only stored if stateful
    iiterator_members<ValueTraits, IsConst> m_members;
};
```

`iiterator_members` (`detail/iiterator.hpp:83-117`) optimizes away the traits
pointer when traits are stateless, reducing iterator size.

### Key Decision: `node_algorithms` as Free Functions

Algorithms (splice, reverse, sort) are implemented as free functions in
`circular_list_algorithms.hpp` and `linear_slist_algorithms.hpp`. They operate
on raw node pointers, independent of container semantics. This makes them
reusable across different intrusive container configurations.

### Tree Algorithms: Capability-Family Pattern

Boost.Intrusive's tree layer is the closest industrial analogue to chaistl's
`balance_policy` design. The algorithm hierarchy separates generic BST
operations from balance-specific mutations:

```
bstree_algorithms<NodeTraits>      // generic: search, rotate, link, unlink
    ├── rbtree_algorithms          // red-black fixup
    ├── avltree_algorithms         // AVL retrace
    ├── splaytree_algorithms       // splay operations
    ├── treap_algorithms           // heap-order maintenance
    └── sgtree_algorithms          // scapegoat rebuild
```

Each algorithm family inherits from `bstree_algorithms` and adds only what it
needs. The container (`rbtree`, `avltree`, `splaytree`, `treap`, `sgtree`)
selects the algorithm family at compile time. This is the **capability-family
pattern** that chaistl's optional capability concepts generalize.

**Key difference**: Boost.Intrusive uses inheritance (`treap_algorithms :
public bstree_algorithms`), while chaistl uses composition (`binary_search_tree`
holds a `Policy` member). The structural contract — header sentinel, root
pointer maintenance, successor-splice erase — is identical.

### Trade-offs

| Pro | Con |
|-----|-----|
| Zero allocation overhead | User must manage object lifetime |
| Object can be in multiple containers simultaneously | Container does not "own" elements |
| Excellent for embedded/real-time | Not a drop-in replacement for std containers |
| Hook flexibility (base or member) | More complex API |
| Tree algorithm families prove capability pattern works | Intrusive semantics leak into algorithm design |

---

## 4. ChaiGO (Predecessor Project — Non-Open-Source)

> ChaiGO is a closed-source early prototype. The analysis below is preserved
> because many chaistl design decisions were reactions to ChaiGO's trade-offs.
> References to ChaiGO source paths in this section point to a private
> repository and are not publicly accessible.

**Goal**: Educational containers with modern C++ features. Simplicity and
readability over performance or standard compliance.

### Architecture: Direct Composition, No Inheritance

`Vector<T>`, `List<T>`, `Deque<T>` are standalone classes. No base classes.

```cpp
// vector.hpp:53
class Vector {
    pointer start_;
    pointer finish_;
    pointer end_of_storage_;
    [[no_unique_address]] allocator_type allocator_;
};
```

Allocator is fixed (`chaigo::allocator<T>` using PMR), not a template
parameter.

### Memory Management: Raw Pointers + `no_unique_address`

- **Vector**: Three raw pointers. Explicit comment: "we do not use iterator
  here because we hope to manipulate pointer directly to improve efficiency.
  iterator class cost too much in practice."
- **List**: Dummy head and tail nodes (`head_`, `tail_`), plus `size_` member.
  Comment: "Use dummy head and tail nodes to simplify the implementation. Not
  a circular list."
- **Deque**: Map of subarrays with two allocators (one for elements, one for
  map nodes).

### Iterator Design: Two Patterns Demonstrated

| Container | Pattern |
|-----------|---------|
| `Vector` / `Deque` | 3-param template: `VectorIterator<T, Ptr, Ref>` |
| `List` / `ForwardList` | Handwritten pair: `ListIterator<T>` + `ListConstIterator<T>` |

This is **intentional educational variety**. The `ListIterator` header
explicitly comments:

> "Note that iterator and const iterator are defined independently. This is
the current approach used by the GNU C++ standard library."
>
> "In most cases, iterator and const iterator could defined in the same
template struct... If you don't want to write too much redundant code, you
can try designing a base template class. Refer to `CircularIteratorBase`."

`CircularIterator` (`iterator/adapter/circular.hpp`) is the CRTP example:

```cpp
// circular.hpp:4
// "Practice CRTP (Curiously Recurring Template Pattern)."

struct CircularIterator
    : public internal::CircularIteratorBase<
          Container, typename Container::pointer,
          typename Container::reference, CircularIterator<Container>>
{ ... };
```

### Trade-offs

| Pro | Con |
|-----|-----|
| Clean, educational code | Fixed allocator reduces flexibility |
| Modern C++20 features (`no_unique_address`, `<=>`, concepts) | No exception-safety separation (no base class) |
| PMR allocator | Some iterator duplication |
| Shows multiple iterator patterns | Not production-hardened |

---

## 5. GCC libstdc++ PBDS (Policy-Based Data Structures)

**Goal**: Standard-library-compatible associative containers with pluggable
underlying data structures and node update policies.

### Architecture: Tag-Based Policy Selection

PBDS exposes policy selection through template parameters, not inheritance:

```cpp
// assoc_container.hpp:635
template<
    typename Key,
    typename Mapped,                           // null_type = set
    typename Cmp_Fn = std::less<Key>,
    typename Tag = rb_tree_tag,                // ← tree family selector
    typename Node_Update = null_node_update,   // ← node invariant policy
    typename Allocator = std::allocator<char>
> class tree;
```

Tag choices: `rb_tree_tag`, `splay_tree_tag`, `ov_tree_tag` (ordered vector).
Node update choices: `null_node_update`,
`tree_order_statistics_node_update` (enables `find_by_order`, `order_of_key`).

This is policy-based design with **coarse-grained tags**: the tag selects the
entire algorithm family, not individual capabilities.

### Hash Table: Fine-Grained Policy Decomposition

PBDS hash tables decompose further into orthogonal policies:

```cpp
// cc_hash_table: collision chaining
template<
    typename Key, typename Mapped,
    typename Hash_Fn = std::hash<Key>,
    typename Eq_Fn = std::equal_to<Key>,
    typename Comb_Hash_Fn = direct_mask_range_hashing<>,  // range hashing policy
    typename Resize_Policy = hash_standard_resize_policy<  // resize policy
        hash_exponential_size_policy<>,
        hash_load_check_resize_trigger<>, true>,
    bool Store_Hash = false,      // ← cache hash value in node?
    typename Allocator = std::allocator<char>
> class cc_hash_table;
```

The `Store_Hash` boolean is a compile-time policy switch with zero overhead
when false — similar in spirit to chaistl's `[[no_unique_address]]`
`node_extension`.

### Rope: Heavyweight String

```cpp
#include <ext/rope>
__gnu_cxx::rope<char> r;
r.insert(pos, "hello");   // O(log n)
r.erase(pos, count);      // O(log n)
```

`rope` is a balanced tree of string fragments (similar to a finger tree),
designed for text editors and version control systems where mid-string
insertion/deletion is frequent.

### Trade-offs

| Pro | Con |
|-----|-----|
| Tag switching is simple to use | Coarse tags don't mix capabilities (can't have RB + order stats + splay) |
| Hash policy decomposition is sophisticated | Not standard; ABI tied to GCC |
| `tree_order_statistics_node_update` is powerful | Node update policy is intrusive (must be specified at container type) |
| `rope` fills a gap std::string cannot | 3000+ lines of complex code; not widely used |

---

## 6. libc++ (LLVM)

**Goal**: Strict standard compliance with minimal extensions. Clean,
maintainable implementation over feature richness.

### Architecture: Direct Implementation

libc++ does not use an intrusive backend or policy tags. `std::map` is
implemented directly:

```cpp
// __tree (internal)
template <class _Tp, class _Compare, class _Allocator>
class __tree {
    // red-black tree, hardcoded
};
```

There is no `__tree<Tag>` or policy parameter. The tree type is the
implementation detail, not a customization point.

### Extensions

- `std::flat_map` (C++23) — sorted container adaptor
- `__libcpp_verbose_abort` — internal error hook
- ASan container annotations — customizable per allocator

libc++ is the **negative reference** for policy-based design: it proves that
a direct, non-parameterized implementation can be correct and maintainable,
at the cost of flexibility.

### Trade-offs

| Pro | Con |
|-----|-----|
| Maximum standard compliance | No alternative tree types |
| Clean, debuggable code | Cannot swap RB for AVL without rewriting |
| Fast to iterate on standard features | Not a source of design patterns for extensibility |

---

## Cross-Reference Summary

| Aspect | EASTL | Boost.Container | Boost.Intrusive | GCC PBDS | libc++ | ChaiGO |
|--------|-------|-----------------|-----------------|----------|--------|--------|
| **Architecture** | Inheritance (Base classes) | Composition + Intrusive backend | Policy/options, no inheritance | Tag-based policy selection | Direct implementation | Direct composition |
| **Memory mgmt** | Base class owns alloc | `node_alloc_holder` + Intrusive | No allocation (user owns) | Standard allocator | Standard allocator | Raw pointers + `no_unique_address` |
| **Allocator** | Non-templated `eastl::allocator` | Templated, versioned, EBO via base | N/A (no allocation) | Templated | Templated | Fixed `chaigo::allocator<T>` (PMR) |
| **EBO technique** | `compressed_pair` | Derive from allocator/comparator | Stateless traits optimized out | — | — | `[[no_unique_address]]` |
| **Tree policy** | Template flags (`bUniqueKeys`) | `tree_type_enum` (RB/AVL/splay/scapegoat) | Algorithm family inheritance | Tag (`rb_tree_tag`, etc.) | Hardcoded RB | `balance_policy` concept |
| **Policy granularity** | Flags on generic rbtree | Enum selects entire backend | Algorithm family inheritance | Tag selects entire backend | None | Concept with optional capabilities |
| **Vector iterator** | `T*` (raw pointer) | `vec_iterator<Pointer, IsConst>` class | N/A | N/A | `__wrap_iter` | `VectorIterator<T, Ptr, Ref>` class |
| **List iterator** | `ListIterator<T, Ptr, Ref>` | `iterator_from_iiterator` | `list_iterator<ValueTraits, IsConst>` | N/A | `__list_iterator` | `ListIterator<T>` / `ListConstIterator<T>` |
| **Iterator constness** | 3-param template | `IsConst` bool param | `IsConst` bool param | N/A | `IsConst` bool param | Both 3-param (Vector) and separate structs (List) |
| **Node design** | `ListNodeBase` + `ListNode<T>` | `base_node<T, Hook>` with aligned storage | Hook embedded in user type | Intrusive hooks | `__tree_node` | `ListNode<T>` with prev/next/data |
| **String SSO** | Union of `HeapLayout`/`SSOLayout` | Union of `long_t`/`short_t` | N/A | N/A | Union with pointer tagging | Not examined |
| **Key rationale** | Exception safety, performance | Code reuse, standards compliance | Zero overhead, user control | Extensibility within std-like interface | Standard compliance, maintainability | Simplicity, education, modern C++ |

---

## Implications for chaistl

### What chaistl currently does

- **No base-class inheritance** (like ChaiGO, unlike EASTL)
- **`iterator_impl<Const>`** (like Boost) — **under revision**, likely moving to
  C++23 deducing this; see [ADR 001](../decisions/001-iterator-impl-pattern.md)
- **`[[no_unique_address]]`** for allocator EBO (like ChaiGO, unlike EASTL's `compressed_pair`)
- **Policy-based tree** (like EASTL's `rbtree`, but with a concept instead of template flags)
- **Single-class containers** (like ChaiGO and libc++, unlike EASTL)

### What chaistl might adopt from each

| From | Technique | When it makes sense |
|------|-----------|---------------------|
| EASTL | `compressed_pair` for EBO | If `[[no_unique_address]]` proves insufficient for some compiler |
| EASTL | Base-class exception safety | If chaistl adds complex multi-step constructors that need rollback |
| Boost.Container | `node_alloc_holder` reuse | If chaistl adds 3+ more node-based containers (multiset, multimap, unordered) |
| Boost.Intrusive | Hook + intrusive containers | `chaistl::experimental::intrusive::*` |
| Boost.Intrusive | `pack_options` / policy options | If intrusive containers need configurable `cache_last`, `constant_time_size`, etc. |
| Boost.Intrusive | Tree algorithm family inheritance | Reference for capability-family pattern; chaistl uses composition instead |
| GCC PBDS | `tree_order_statistics_node_update` | When WBT policy lands: `node_update` concept for rank/select |
| GCC PBDS | `Store_Hash` boolean policy | Hash table `bCacheHashCode` equivalent |
| GCC PBDS | `rope` | `chaistl::experimental::rope` if heavy string manipulation is needed |
| libc++ | `__wrap_iter` / debug iterator mode | When debug build instrumentation is added |
| libc++ | ASan annotation customization | Per-allocator container annotations |
| ChaiGO | CRTP `iterator_facade` | Stage 2 refactor: unify list/forward_list/deque/tree iterators |
| ChaiGO | Dummy head/tail nodes | Already used in chaistl's `list` |

### What chaistl should not adopt

| From | Technique | Why not |
|------|-----------|---------|
| EASTL | Raw pointer `vector::iterator` | Prevents future debug-iterator mode; `wrap_iterator` is better |
| EASTL | Non-templated allocator | Breaks standard compatibility |
| Boost.Container | Intrusive backend for standard containers | Adds indirection; learning value decreases |
| Boost.Intrusive | No allocation semantics | Only for `experimental::intrusive::*`, not standard containers |
| ChaiGO | Fixed allocator type | Breaks standard compatibility |
| ChaiGO | Handwritten `ListIterator` + `ListConstIterator` pair | More duplication than `iterator_impl<Const>` |
| GCC PBDS | Tag-based coarse policy selection | Cannot compose capabilities; chaistl's concept-based optional capabilities are more flexible |
| libc++ | Hardcoded tree type | No flexibility; chaistl's policy design is the explicit differentiator |
