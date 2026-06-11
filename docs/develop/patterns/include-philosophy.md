# Include Philosophy & File Layout

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
file (e.g., `construction_transaction` + `constructed_range_guard`). Allocator-
aware function families that intentionally differ from raw `<memory>` algorithms
name the allocator role first, e.g. `allocator_uninitialized.hpp`.

## Aggregation vs Individual Includes

Fine-grained files are good for maintainers. Public users include by component
name, while implementation headers include the specific internal helper they
use. Avoid broad internal aggregation headers for detail utilities: node-based
containers should not inherit contiguous-storage helpers accidentally, and
contiguous containers should not inherit node helpers accidentally.

```
user-facing entry header                implementation header
────────────────────────                ─────────────────────
containers/vector.hpp        ───────→   containers/sequence/vector.hpp
containers/map.hpp           ───────→   containers/associative/map.hpp
containers/pairing_heap.hpp  ───────→   priority_queue + pairing policy alias
```

**When to use which:**

| Consumer | Use | Reason |
|----------|-----|--------|
| Library users | component-name public header | no need to know sequence/associative/heap taxonomy |
| Users wanting everything | root umbrella header | `<chaistl/containers.hpp>` mirrors `import std;` ergonomics for containers |
| Heavy containers (vector, deque, list) | specific detail helpers | dependencies show whether storage is contiguous, segmented, or node-based |
| Lightweight types (array, span) | individual header | only need 1–2 deps, aggregation would pull in more |
| `detail/` files themselves | individual header | avoid circular includes and accidental helper coupling |
| Test files | individual header | explicit dependency tracking aids debugging |

## API Tiers

Headers are grouped by API commitment, not only by implementation category:

| Tier | Namespace | Example headers | Stability |
|------|-----------|-----------------|-----------|
| Standard-compatible | `chaistl` | `<chaistl/containers/vector.hpp>`, `<chaistl/containers/map.hpp>` | semver-protected |
| Stable chaistl extension | `chaistl` | `<chaistl/containers/pairing_heap.hpp>`, `<chaistl/containers/avl_set.hpp>` | semver-protected, documented as extension |
| Experimental | `chaistl::experimental` | `<chaistl/experimental/containers/ring_buffer.hpp>` | no semver promise |

Stable extensions are not placed in `chaistl::experimental`: the word
experimental is reserved for API surfaces we are still willing to change
freely. Conversely, a facility should not be promoted out of
`chaistl::experimental` until its name, semantics, complexity, and exception
contracts are ready to be maintained.

## Directory Layout

- Source headers live under `include/chaistl/`.
- User-facing container paths use the component name:
  `containers/vector.hpp`, `containers/priority_queue.hpp`,
  `containers/avl_set.hpp`, etc.
- Implementation paths may mirror cppreference categories:
  `containers/sequence/vector.hpp`, `iterator/adapter/reverse.hpp`, etc.
- `detail/` lives under the category it serves: `memory/detail/`, not a
  top-level `detail/` directory.
- Top-level headers (`containers.hpp`, `iterators.hpp`, `meta.hpp`) are
  aggregation points for their category.
- Broad `detail` aggregation headers are avoided. Prefer precise helper
  headers such as `<chaistl/memory/detail/lifetime/exception_guard.hpp>` and
  `<chaistl/memory/detail/storage/node_owner.hpp>`.
- Experimental components live under `experimental/` and mirror the public
  layout where applicable.
