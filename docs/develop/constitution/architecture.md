# Architecture & File Layout

## Namespace Architecture

```
chaistl/                             root — standard-library components (stable)
├── concepts/                        concept constraints (stable)
│   └── legacy/                      pre-C++20 named-requirement modeling (stable, deprecated intent)
├── meta/                            metaprogramming facilities (stable, standalone)
├── containers/                      public stable container entry headers
├── detail/                          implementation details (no stability promise)
└── experimental/                    non-standard extensions (no stability promise)
    └── containers/
        ├── sequence/
        ├── associative/
        ├── intrusive/               intrusive containers
        └── concurrent/              concurrent / lock-free data structures
```

**Dependency rules (enforced by review, not tooling):**

| Layer | May depend on | Must NOT depend on |
|-------|---------------|-------------------|
| `chaistl` | concepts, meta, detail, std | experimental |
| `concepts` | meta, std | chaistl, detail, experimental |
| `concepts::legacy` | concepts, meta, std | chaistl, detail, experimental |
| `meta` | std only | any chaistl layer |
| `detail` | meta, std | chaistl, concepts |
| `experimental` | chaistl, concepts, meta, detail, std | — |

Rule of thumb: **cross-layer calls only go downward**. No cycles. `meta` is
the leaf — it depends on nothing in chaistl. Public umbrella headers such as
`<chaistl/containers.hpp>` may include stable entry headers and implemented
experimental entry headers so users can opt into one broad include; stable
implementations must not depend on experimental facilities.

**What goes where:**

| If it... | Put it in... |
|----------|-------------|
| Is a concrete standard-library component | `chaistl` |
| Is a stable, documented non-standard extension | `chaistl` |
| Is unstable, exploratory, or not semver-protected | `chaistl::experimental` |
| Is a concept that constrains templates | `concepts` |
| Is a pre-C++20 named-requirement concept | `concepts::legacy` |
| Is a compile-time trait or type transform | `meta` |
| Is a workaround, RAII guard, or internal helper | `detail` |
| Could be useful without including any container | `meta` |

**Stability promises:**

- `chaistl`, `concepts`, `meta` — semantic versioning applies. This includes
  stable chaistl extensions such as `pairing_heap` and `avl_set`. Breaking
  changes get a major version bump.
- `detail` — no promise. Can be renamed, moved, or deleted in any commit.
  Tests may cover `detail` but are marked as internal.
- `experimental` — no promise. Interfaces may change in any commit. Use at
  your own risk.

## Container Taxonomy

Containers are classified by **interface semantics and iteration capability**,
not by implementation mechanism. The source tree is the authoritative inventory
of currently implemented containers.

| Category | Definition |
|----------|------------|
| **Sequence** | Linear order; user controls element position; may or may not expose iterators. |
| **Associative** | Element position determined by key + comparison / hash. |
| **Views** | Non-owning reference to contiguous or multidimensional storage. |

Notes:

- `stack`, `queue`, and `priority_queue` are sequence containers. They are
  traditionally called "container adaptors" in the standard, but their user-
  visible semantics are LIFO/FIFO sequences, and they satisfy the `Container`
  requirements (minus iterators).
- Intrusive containers live in `experimental/containers/intrusive/`. They do
  not satisfy standard `Container` requirements because the user retains
  ownership of the nodes.
- Concurrent data structures live in `experimental/containers/concurrent/`.
  They have thread-safety guarantees that standard concepts do not express.

## File Naming

One primary class or function family per file. The file name equals the class
name (or the standard term for a function family). Memory detail headers add
one shallow grouping level so the path teaches the boundary: storage ownership,
object lifetime, or small utility.

```
memory/detail/
├── storage/
│   ├── raw_storage_buffer.hpp              raw_storage_buffer<T, A>
│   ├── uninitialized_storage_builder.hpp   uninitialized_storage_builder<T, A>
│   └── node_owner.hpp                      node_owner<NodeAllocator>
├── lifetime/
│   ├── allocator_uninitialized.hpp         allocator-aware uninitialized operations
│   ├── construction_transaction.hpp        construction_transaction + constructed_range_guard
│   └── exception_guard.hpp                 exception_guard<F>
└── utility/
    └── forward_like.hpp                    forward_like<Like, T>() — temporary workaround
```

Exceptions to one-class-per-file: two highly related RAII classes may share a
file (e.g. `construction_transaction` + `constructed_range_guard`). Allocator-
aware function families that intentionally differ from raw `<memory>` algorithms
name the allocator role first, e.g. `allocator_uninitialized.hpp`.

## Aggregation vs Individual Includes

Fine-grained files are good for maintainers. Public users include by component
name; implementation headers include the specific internal helper they use.
Avoid broad internal aggregation headers for detail utilities, because they
hide whether code needs contiguous-storage helpers, node-storage helpers, or
only a rollback guard.

**When to use which:**

| Consumer | Use | Reason |
|----------|-----|--------|
| Public users | component-name public header | no need to know sequence/associative/heap taxonomy |
| Users wanting everything | root umbrella header | `<chaistl/containers.hpp>` mirrors `import std;` ergonomics for containers |
| Heavy containers (vector, deque, list) | specific detail helpers | dependencies show whether storage is contiguous, segmented, or node-based |
| Lightweight types (array, span) | individual header | only need 1–2 deps, aggregation would pull in more |
| `detail/` files themselves | individual header | avoid circular includes and accidental helper coupling |
| Test files | individual header | explicit dependency tracking aids debugging |

## Public Include Layout

The public user-facing include path is organized by component name, not by the
internal taxonomy. Users should not need to know whether a container is stored
under `sequence/`, `associative/`, `heap/`, or `views/`.

```cpp
#include <chaistl/containers/vector.hpp>
#include <chaistl/containers/map.hpp>
#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/containers/min_heap.hpp>       // stable extension
#include <chaistl/containers/max_heap.hpp>       // stable extension
#include <chaistl/containers/d_ary_heap.hpp>    // stable extension
#include <chaistl/containers/pairing_heap.hpp>  // stable extension
#include <chaistl/containers/fibonacci_heap.hpp>  // stable extension
#include <chaistl/containers/skew_heap.hpp>     // stable extension
#include <chaistl/containers/leftist_heap.hpp>  // stable extension
#include <chaistl/containers/avl_set.hpp>       // stable extension
#include <chaistl/containers/splay_set.hpp>     // stable extension
#include <chaistl/experimental/containers/ring_buffer.hpp>
```

Internal category paths remain available for maintainers and existing code:

```cpp
#include <chaistl/containers/sequence/vector.hpp>
#include <chaistl/containers/associative/map.hpp>
#include <chaistl/containers/heap/priority_queue.hpp>
```

`<chaistl/containers.hpp>` is the one-shot containers umbrella. It includes
standard-compatible containers, stable chaistl extensions, and implemented
experimental container entry headers under `chaistl::experimental`. Research
placeholders and forward-declaration-only headers stay out of umbrella headers
until they expose a usable API.

## Directory Layout

- Source headers live under `include/chaistl/`.
- Public component entry paths use the component name:
  `containers/vector.hpp`, `containers/avl_set.hpp`, etc.
- Implementation paths may mirror cppreference or internal categories:
  `containers/sequence/vector.hpp`, `iterator/adapter/reverse.hpp`, etc.
- `detail/` lives under the category it serves: `memory/detail/`, not a
  top-level `detail/` directory.
- Root aggregation headers (`containers.hpp`, `iterator.hpp`, `meta.hpp`) are
  aggregation points for their category.
- Broad `detail` aggregation headers are avoided. Prefer
  `<chaistl/memory/detail/lifetime/exception_guard.hpp>`,
  `<chaistl/memory/detail/storage/node_owner.hpp>`, and other precise helper headers.
- Experimental components live under `experimental/` and mirror the public
  layout where applicable:
  - `experimental/containers/sequence/` for sequence-like extensions.
  - `experimental/containers/associative/` for associative-like extensions.
  - `experimental/containers/intrusive/` for intrusive containers.
  - `experimental/containers/concurrent/` for concurrent / lock-free
    data structures.
