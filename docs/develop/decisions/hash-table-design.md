# Hash Table Design

## Status

Accepted, 2026-06-11

Revised on the same day after design review. The review overturned the first
draft's iteration strategy (a standard-conformance problem, not a performance
footnote), corrected the hash-caching rationale, fixed the exception-safety
ordering of insertion, split the heterogeneous-overload staging by standard
version, and added two scope decisions (`chaistl::hash`, P2363). Overturned
draft choices are preserved under Alternatives Considered.

Updated 2026-06-12: the standard containers still default to prime bucket
counts, but ChaiSTL now also exposes opt-in power-of-two aliases as an
extension. The default remains conservative because mask-based indexing depends
on useful low hash bits.

## Context

`unordered_set` and `unordered_map` need a shared hash table core. The design
should be useful as teaching material, compatible with the standard unordered
container surface, and still leave room for policy-based experiments.

The reference implementations show that this is not just a bucket-indexing
problem. A usable hash table needs clear choices for node ownership, bucket
layout, iterator traversal, rehash policy, hash-code caching, exception safety,
allocator use, and how much policy surface should be exposed.

ChaiSTL has a narrower goal than libstdc++ or libc++:

- It does not need ABI compatibility.
- It does not need decades of backward compatibility.
- It should make generic programming techniques visible without hiding the data
  structure behind vendor-level machinery.
- It should interoperate with the official standard library types where that is
  useful for tests and examples.

One constraint turned out to be load-bearing for the physical layout:
[container.reqmts] requires `begin()` and `end()` to be constant complexity
for every container, and iterator increment must be (amortized) constant. A
bucket-array scan cannot provide that after `rehash(N)` on a nearly-empty
table. This single requirement is the reason none of libstdc++, libc++, or
MSVC ships the naive bucket-chain layout, and it drives the layout decision
below.

The long-term goal is not to support only one hash table algorithm. A teaching
library can reasonably include chained hash tables, probing hash tables, and
PBDS-style policy combinations. The main design question is sequencing: which
core should be used to validate the standard `unordered_*` surface first, and
which policy boundaries should remain internal until real implementations prove
that they are useful.

## Reference Notes

### Standard Unordered Containers

The standard-facing containers have several semantic constraints that shape the
underlying table:

- Equivalent keys must produce the same hash value.
- Rehash invalidates iterators, but not references or pointers to elements.
- Erase invalidates only iterators and references to erased elements.
- Insert invalidates iterators only when it causes a rehash; if
  `(N + n) <= z * B` the insert must not affect iterator validity. The dual
  of that rule: an insert that turns out to be a duplicate has `n == 0` and
  must not rehash.
- `begin()`/`end()` are constant complexity ([container.reqmts]); this applies
  to unordered containers and is what the naive layout fails.
- A failed single-element insert/emplace must have no effect
  ([unord.req.except]); for `rehash`, an exception not from hash or comparison
  must leave the table unchanged.
- `erase(iterator)` is specified to throw nothing unless Hash or Pred throws —
  and with cached hash codes it does not need to call either.
- `operator==` is set semantics (element-wise lookup in the other container),
  is undefined if the two containers' Pred behavior differs, and unordered
  containers have no `operator<` and no `operator<=>`.
- Hash and Pred are stateful objects: they propagate on copy/move/swap, and
  the `swap` noexcept specification derives from their nothrow-swappability.
- `max_load_factor(z)` stores a hint; it does not itself rehash.
- The bucket interface requires local iterators with the same value/reference
  types as the container iterator.
- `unordered_set` iterators expose immutable keys; `unordered_map` stores
  `pair<const Key, T>` with mutable mapped values.
- In multi containers, equivalent elements must be adjacent in the iteration
  order (relevant to the layout even though v1 is unique-keys only).

These requirements make open addressing a poor default for the first standard
container implementation, because moving elements during erase or rehash makes
reference stability and node-style operations harder.

### libstdc++

The libstdc++ `_Hashtable` is highly decomposed. It has policies for hashing,
range hashing, rehashing, extraction, equality, allocation, and traits such as
cached hash codes, constant iterators, and unique keys.

Its physical layout is not a simple array of bucket heads. It stores all nodes
in a global forward list and lets each non-empty bucket point to the node before
the first node in that bucket. This makes insertion and erasure resemble
`forward_list::insert_after` and `forward_list::erase_after`, and it gives the
container efficient global iteration. The cost is more complicated bucket
maintenance, especially around the first node of a bucket.

Facts verified locally that shaped this design:

- `_M_reinsert_node` recomputes the hash code of an incoming node-handle node
  (`bits/hashtable.h:1223`, via `_M_locate`); the multi variant calls
  `_M_hash_code(__k)` directly. A cached hash is never trusted across
  containers, because a stateful Hash on the destination may differ.
- Node construction precedes the rehash decision (`_Scoped_node` RAII,
  `bits/hashtable.h:295`, used by the insert paths before
  `_M_insert_unique_node` decides on rehashing).
- `end()` is `iterator(nullptr)`; there is no dummy end node.
- `swap` is `noexcept(__is_nothrow_swappable<_Hash> && ...<_Equal>)`.

Relevant local source:

- `reference/gcc/libstdc++-v3/include/bits/hashtable.h`
- `reference/gcc/libstdc++-v3/include/bits/hashtable_policy.h`

### libc++

The libc++ `__hash_table` uses a similar node-based chained design with a bucket
list, a first-node sentinel, local iterators, and per-node cached hash values.
Its implementation is more compact than libstdc++ in some areas, but it still
optimizes for a production standard library rather than for introductory
readability. Its default-constructed table has zero buckets (lazy allocation),
which this design adopts.

Relevant local source:

- `reference/llvm-project/libcxx/include/__hash_table`

### EASTL

EASTL's hash table is closer to the structure most people first imagine:
buckets point directly to chains, and iterators keep both the current node and
the current bucket pointer. The bucket array has a sentinel slot so global
iteration can advance across empty buckets without storing the bucket count in
every iterator step.

Verified locally: `begin()` on a non-empty table starts at bucket 0 and calls
`increment_bucket()` (`internal/hashtable.h:970-984`), scanning empty buckets;
only the empty table takes an early-out. After `rehash(1'000'000)` with one
element, `begin()` walks ~10^6 slots. That is acceptable for EASTL's goals
(game workloads keep load factors high) and non-conforming for ours.

EASTL also keeps a practical policy surface around hash-code caching, mutable
iterators, unique keys, and rehash policy. This is a better teaching reference
than copying the full vendor standard-library structure.

Relevant local source:

- `reference/EASTL/include/EASTL/internal/hashtable.h`
- `reference/EASTL/include/EASTL/hash_set.h`
- `reference/EASTL/include/EASTL/hash_map.h`

### Other layouts (no local reference sources)

Recorded from literature for completeness of the design space:

- **MSVC STL** keeps all elements in one global doubly-linked list
  (`std::list`) and stores `[first, last)` iterator pairs per bucket. Fewer
  per-node pointers than the layout chosen here, but the bucket contents must
  stay adjacent in the global list — an invariant that couples insert, erase,
  and rehash to both structures at once.
- **boost.unordered (1.80+, FCA)** keeps direct bucket chains and makes
  iteration O(1) with per-word bucket-group bitmasks plus a linked list of
  non-empty groups. The best memory profile with conforming iteration, at the
  cost of bitmask/group bookkeeping.

### GNU PBDS

PBDS is valuable for thinking about policy-based design. It separates collision
chaining from probing tables and exposes policies for range hashing, probing,
resize triggers, size policy, and whether to store hash values.

That flexibility is educational, but it is too much public surface for
ChaiSTL's first unordered containers. If we start there, the project becomes a
policy framework before it has a clear standard container implementation.

Relevant local source:

- `reference/gcc/libstdc++-v3/include/ext/pb_ds/hash_policy.hpp`
- `reference/gcc/libstdc++-v3/include/ext/pb_ds/detail/cc_hash_table_map_`
- `reference/gcc/libstdc++-v3/include/ext/pb_ds/detail/gp_hash_table_map_`
- `reference/gcc/libstdc++-v3/include/ext/pb_ds/detail/hash_fn`
- `reference/gcc/libstdc++-v3/include/ext/pb_ds/detail/resize_policy`

### Boost.Intrusive

Boost.Intrusive separates node ownership from the hash table algorithm. Users
provide hooks and bucket storage, so the container does not own elements in the
same way as `std::unordered_set`.

That is not the right default for ChaiSTL's standard containers, but it is a
useful reminder: the owning table, bucket traits, node links, and public
container API are separable ideas.

Relevant local source:

- `reference/boost-intrusive/include/boost/intrusive/hashtable.hpp`
- `reference/boost-intrusive/include/boost/intrusive/unordered_set.hpp`

## Decision

Use a layered design:

1. Public standard containers: `unordered_set` and `unordered_map`.
2. Internal standard-container core: an owning, node-based, separate-chaining
   hash table.
3. Experimental hash table family: future public containers or policies for
   probing, alternate chaining layouts, and PBDS-inspired combinations.

The first implementation targets unique keys only, but the core keeps the
structural room for the equal family (the tree core grew the same way).

### Physical layout

Direct bucket chains for lookup, plus a separate intrusive global
doubly-linked iteration list. Each node carries:

```
next_in_bucket            singly-linked collision chain of its bucket
prev_in_order
next_in_order             global iteration list
cached_hash               full hash code
value
```

The bucket array stores the head of each chain. `local_iterator` walks
`next_in_bucket` to null; `iterator` walks the global list; both hold a single
node pointer. `end()` is a null iterator. The container stores the head and
tail of the global list and contains no dummy or sentinel nodes (the heap
layer's discipline).

Why this layout and not the alternatives:

| layout | per-node metadata | begin / ++ | structural coupling |
|---|---|---|---|
| bucket scan + cached first non-empty | next + hash | O(1) / O(empty buckets) | none, but iteration non-conforming |
| libstdc++/libc++ global forward list | next + hash | O(1) / O(1) | bucket points *before* its first node |
| MSVC global list + per-bucket [first, last) | prev + next | O(1) / O(1) | bucket contents must stay globally adjacent |
| **chosen: chains + global doubly-linked list** | 3 pointers + hash | O(1) / O(1) | none — the two structures are orthogonal |
| boost FCA bucket-group bitmaps | next + hash | O(1) / O(1) | bitmask + non-empty-group list upkeep |

- `begin()`/`end()` and `++iterator` are O(1) as required; no observable
  behavior depends on scanning the bucket array.
- `erase(iterator)` returns the next iterator in O(1) (`next_in_order`); the
  bucket-scan layout would have to search for the next non-empty bucket.
- The iteration list must be doubly linked. `erase(iterator)` needs the global
  predecessor, which can live in any other bucket; finding it through the
  bucket structure is O(n). The only alternatives are a `prev` pointer or the
  libstdc++ before-first-node invariant. This is why the `prev` pointer must
  not be "optimized away" later.
- Rehash never touches the iteration list. It allocates the new bucket array
  (the only failure point — the old table is untouched if that throws), then
  relinks `next_in_bucket` per node from `cached_hash`, calling no user code.
  Migration is noexcept, so the strong guarantee is structural rather than
  effortful. Iteration order is stable across rehash as a side effect.
- Future multi containers need equivalent elements adjacent in iteration
  order; splicing into a doubly-linked list at the located position is the
  natural mechanism. The bucket-order layouts make that adjacency awkward.
- Cost, priced honestly: three pointers plus one `size_t` of metadata per
  node — roughly twice libstdc++ — and insert/erase maintain two link
  structures. That buys the absence of cross-structure invariants. Teaching
  clarity is the point of this container; the experimental families are the
  place for lean layouts.

The global list inserts at the tail, so iteration order is insertion order.
This is an implementation detail, not an API promise: oracle tests compare
with set semantics and must not lock the order in. (The tree layer locks
LWG 233 ordering because the standard requires it; here the standard
explicitly does not.)

### Hash-code caching

Unconditional in the first implementation. The honest reasons:

- Rehash migration calls no user code (above) — caching is what makes that
  possible when Hash may throw.
- `erase(iterator)` must not throw; with the cached hash it does not even call
  Hash to find the bucket. Without caching, the no-throw expectation cannot be
  met cleanly.
- Lookup compares the full cached hash before calling KeyEqual, filtering
  collisions cheaply.
- Explicitly *not* a reason in this layout: local-iterator traversal. Chains
  end at null; only the global-forward-list layouts need hash codes to detect
  bucket boundaries.
- Caching does *not* extend across containers: node-handle reinsert and merge
  must recompute the hash with the destination's Hash, which may be a
  different stateful instance. (libstdc++ does the same; see Reference Notes.)

Once the containers are stable, caching can become an internal trait. A public
knob waits until a second table family exists to justify it.

### Insert paths and exception safety

Two paths, fixed by what can be known before construction:

- **key-first** — `insert(value)`, `try_emplace`, `operator[]`,
  `insert_or_assign`: the key is available without constructing the element.
  Order: hash → find (KeyEqual may throw: no effect) → construct node →
  rehash if needed → link both structures → `++size`.
- **construct-first** — `emplace(args...)`: the key does not exist until the
  value does. Order: construct node → hash (throws: destroy node, no effect) →
  find (duplicate: destroy node, no rehash) → rehash if needed → link →
  `++size`.

Invariants shared by both paths:

- **Node construction precedes rehash.** If the new bucket array allocation
  throws, the node guard releases and the old table is untouched. The reverse
  order would invalidate every iterator and then fail the insert — not
  compatible with "the insertion has no effect" ([unord.req.except]).
- A duplicate insert never rehashes (`n == 0` in the `(N + n) <= z * B`
  rule). The key-first ordering gives this for free; the construct-first path
  discovers the duplicate after construction and destroys the node without
  touching the table.
- Node ownership in the vulnerable window uses RAII guards (the hash-layer
  analogue of libstdc++ `_Scoped_node`); no bare allocate/construct pairs.

### Rehash policy

`prime_rehash_policy` is the default policy, with an intentionally small
surface:

```
next_bucket_count(requested)          smallest policy size >= requested
bucket_for_hash(hash, bucket_count)   maps a cached hash to a bucket index
need_rehash(size, incoming, bucket_count, max_load_factor)
```

It owns nothing else: no range hashing, no probing, no tombstones, no growth
exotica. Prime-based sizing is the default because it tolerates weak user
hashes, which suits a teaching library and makes simple hash functors less
catastrophic.

`power2_rehash_policy` is the opt-in extension behind
`power2_unordered_set`, `power2_unordered_multiset`,
`power2_unordered_map`, and `power2_unordered_multimap`. It rounds bucket
counts to powers of two and computes buckets with `hash & (bucket_count - 1)`.
That removes a modulo from the hot lookup path, but it also makes low hash
bits load-bearing. The hash-quality benchmarks intentionally include a
`shifted_hash` whose low bits are zero and a `mixed_hash` with basic bit
mixing: the former demonstrates why power-of-two indexing is not the default;
the latter demonstrates when the opt-in policy is useful.

### Allocator discipline

Three allocator views, all used through `allocator_traits`:

```
value allocator    the user's Alloc (interface only)
node allocator     rebind to the node type; allocates and constructs nodes
bucket allocator   rebind to the bucket pointer type; allocates the array
```

All construction and destruction goes through `allocator_traits`
construct/destroy; nodes are complete objects (no "allocate node, construct
only the value subobject" — one of the UB pitfalls P3372 records in vendor
implementations). POCCA/POCMA/POCS handling follows the tree layer. The
fancy-pointer depth must match whatever the tree layer commits to — the hash
layer must not be the odd one out in either direction (audit in stage 1).

### Empty state

A default-constructed table has zero buckets and allocates nothing (libc++'s
choice, and the heap layer's zero-allocation discipline). `end()` is a null
iterator. Bucket-indexed operations on a zero-bucket table are precondition
violations guarded by hardening asserts; `bucket_for_hash` is never called
with a zero bucket count.

### Semantics worth pinning (test obligations)

- `operator==` is set semantics; no `<`, no `<=>`. Tests must not inherit
  lexicographic expectations from the ordered/flat families.
- Stateful Hash and Pred propagate on copy/move/swap; `swap`'s noexcept
  specification derives from `is_nothrow_swappable` of both.
- `erase(iterator)` calls no user code at all.
- `max_load_factor(z)` stores the hint and never rehashes by itself.
- Reference/pointer stability across rehash; invalidation only as specified.

### Node handles

Deferred to their staging step, but the node representation must allow
detaching a node while preserving its allocator association. The cached hash
field is preserved by layout and semantically stale outside a table (see
caching). The hash layer gets its own node handle first, mirroring
`bst_node_handle` (`containers/tree/node_handle.hpp`); folding both into one
shared skeleton is a refactor to evaluate once both exist. The mutable-key
problem of `pair<const Key, T>` in map handles follows the stance the tree
layer already took — and is worth documenting as a teaching point: it is one
of the places where conforming portable user code cannot do what
standard-library implementations do.

### constexpr and `chaistl::hash`

The table and containers are constexpr end to end, continuing the tree and
heap precedent and matching the C++26 direction (P3372, constexpr
containers). The UB pitfalls P3372 catalogs (partial node construction,
`static_cast` type punning) are avoided by construction in a from-scratch
implementation.

`std::hash` remains the default Hash — P3372 deliberately leaves `std::hash`
non-constexpr (salted implementations are conforming), so constant-evaluated
use requires a user-supplied hasher. To make that path real, and to avoid
teaching hash tables on top of a black box, chaistl adds a minimal
`chaistl::hash` in `chaistl/functional/hash.hpp`: a constexpr FNV-1a for
integral, enumeration, pointer, and string-like keys. It is not the default
Hash of any container; it is the documented constexpr/teaching option.
(Scope decision 2026-06-11: minimal version only; no `hash_combine`/range
hashing until the table itself is proven.)

### Verification and tests

- `validate()` is a public member (associative-layer convention, matching the tree). It checks that every
  node's `bucket_for_hash(cached_hash)` equals the bucket whose chain holds
  it, that the bucket chains and the iteration list contain exactly the same
  nodes, and that `size()` matches both. The two orthogonal structures
  cross-check each other — a diagnostic the single-list layouts cannot offer
  this cheaply, and the antidote to this layout's characteristic bug class
  (updating one structure and forgetting the other).
- Tests target observable standard behavior, including the bucket interface:
  oracle/property suites against `std::unordered_set`/`map` with set-semantics
  comparison, exception-injection sweeps over every Hash/KeyEqual call site,
  stateful-functor and stateful-allocator propagation, constexpr
  `static_assert` coverage, and hardening death tests — the same battery the
  tree and heap layers field.

### Public surface restraint

The core stays in a detail namespace. The first internal policy boundaries
remain small: `Hash`, `KeyEqual`, `ExtractKey`, hash-code storage, rehash
policy, bucket traversal helpers. No PBDS-style public template signature
until multiple real table variants exist; a public
`chaistl::experimental::hash_table` is reconsidered only after the standard
containers stabilize.

## Staging

1. Internal core: hash node, `prime_rehash_policy`, `hash_table` with both
   insert paths, lookup, erase, clear, rehash/reserve, iterators, local
   iterators, bucket interface, `validate()`.
2. `chaistl::unordered_set` on the core, with `chaistl::hash` (minimal,
   `functional/hash.hpp`) for the constexpr path. Full C++23 surface except
   node handles/merge — including C++20 heterogeneous lookup
   (`find`/`count`/`contains`/`equal_range`), `from_range_t` construction and
   `insert_range` (P1206), CTAD guides with the qualifies-as-allocator gate.
3. Bucket-interface, iterator-invalidation, exception-safety, and
   allocator-propagation suites — the acceptance gate for the core.
4. `chaistl::unordered_map`: `pair<const Key, T>`, `operator[]`, `at`,
   `try_emplace`, `insert_or_assign`, piecewise construction; the
   construct-first path gets its full battery here.
5. C++23 heterogeneous erase/extract (P2077), node handles and `merge`
   (recompute-hash discipline).
6. C++26 alignment, deliberately late (decision 2026-06-11): P2363
   heterogeneous `insert`/`try_emplace`/`insert_or_assign`/`operator[]`/`at`/
   `bucket`, in the stable namespace, documented as C++26-aligned extensions
   (reference point: `__cpp_lib_associative_heterogeneous_insertion`).
7. Evaluate `unordered_multiset`/`unordered_multimap` (the iteration list
   makes equivalent-key adjacency natural).
8. Experimental table families: direct-chain study target, global-forward-list
   chained table (the libstdc++ comparison), linear probing, quadratic
   probing, Robin Hood, PBDS-inspired policy table; the power-of-two + mixing
   rehash policy joins here.
9. Promote only proven policy boundaries into public APIs.

## Consequences

Positive effects:

- Standard-conforming iteration complexity without the before-first-node
  invariant; each of the two link structures is simple on its own, and they
  are orthogonal.
- Rehash is allocate-then-noexcept-relink: strong guarantee by construction,
  no user code during migration, iteration order stable across rehash.
- `erase(iterator)` calls no user code and returns the next element in O(1).
- The core is shared by set and map without exposing a policy framework.
- `validate()` cross-checks two independent structures against each other.
- Paved paths exist for multi containers (splice adjacency) and node handles
  (allocator-preserving detach).

Negative effects:

- Per-node metadata is three pointers plus a hash, roughly twice
  libstdc++/libc++. A deliberate teaching trade; the lean layouts belong to
  the experimental families.
- Insert/erase maintain two link structures; forgetting one is this layout's
  characteristic bug class (which `validate()` exists to catch).
- Insertion-order iteration is pleasant but non-standard; tests and docs must
  not promise it.
- A later switch to a memory-lean layout remains a meaningful internal
  rewrite.
- Keeping the hash table private means users cannot immediately study it as a
  standalone public container.
- Experimental probing tables will need a separate API story instead of being
  hidden behind `unordered_map`.

## Alternatives Considered

### Bucket-scan global iteration (the first draft's implicit choice)

The first draft accepted "global iteration may scan empty buckets" as a
consequence. Review reclassified it as a conformance bug:
[container.reqmts] requires constant-complexity `begin()`, and EASTL — the
draft's layout model — demonstrably scans (`internal/hashtable.h:970`,
verified). Caching the first non-empty bucket repairs `begin()` but not
`++iterator` across empty runs. Rejected.

### Start with the libstdc++/libc++ layout

This would give efficient global iteration with minimal node metadata and
would mirror production standard libraries most closely. Rejected for the
first version because the bucket-before-first-node invariant is harder to
teach, harder to debug, and likely to distract from standard
unordered-container behavior. It remains the planned comparison family in the
experimental stage.

### MSVC layout (global list + per-bucket [first, last))

One pointer less per node than the chosen layout and no separate bucket
chain, but buckets and the global list stop being orthogonal: the
"same-bucket elements stay globally adjacent" invariant touches insert,
erase, and rehash simultaneously. The chosen layout pays one extra pointer
for zero cross-structure invariants.

### boost FCA bucket-group bitmaps

The best memory profile with conforming iteration. Rejected for v1 because
bitmask plus group-list maintenance is exactly the vendor-machinery noise the
first container should avoid. A strong candidate for the experimental
comparison family.

### Start with a PBDS-style policy framework

This would make ChaiSTL a strong policy-based design playground. It is rejected
for the first version because the abstraction surface would be larger than the
container being taught. It remains a good long-term experiment once the
standard unordered containers have proven the core requirements.

### Start with open addressing

Open addressing is useful for high-performance hash maps, and PBDS already
shows how a probing table can be policy-driven. It is rejected for the standard
unordered containers because node stability, erase semantics, and future node
handles are easier with separate chaining. It should still be implemented later
as an experimental table family.

### Expose `hash_table` as a public container immediately

This would make the underlying structure easier to study directly. It is
deferred because a public container creates API commitments before the shared
set/map requirements have forced the right boundaries to appear.

## Open Questions

Closed during the 2026-06-11 review (answers above):

- Hash-code caching → unconditional in v1; internal trait later; public knob
  only with a second table family.
- Power-of-two rehash policy → shipped as opt-in aliases for the chained
  standard containers; still not the default because hash low-bit quality is a
  caller responsibility.
- Transparent lookup staging → C++20 lookup in stage 2, C++23 erase/extract
  in stage 5, C++26 P2363 in stage 6.
- Direct-chain layout permanence → kept, but with the explicit global
  iteration list; the global-forward-list layout becomes a comparison family.

Still open:

- When to fold `bst_node_handle` and the hash node handle into one shared
  skeleton (evaluate after both exist).
- Fancy-pointer depth: match the tree layer; audit its actual commitment
  during stage 1.
- Which policy concepts become public once experimental families exist, and
  whether probing tables share vocabulary with chained tables beyond
  Hash/KeyEqual/ExtractKey and load-control (current lean: share only the
  top-level concepts; tombstones, probe sequences, and backshift deletion get
  their own interface).
- Whether `chaistl::hash` ever grows `hash_combine`/range hashing
  (deliberately out of scope now).
