# Heap Policy-Based Design

## Status

Accepted (2026-06-11)

## Context

`chaistl::priority_queue` modernizes pb_ds's policy-based priority queue:
one container, multiple heap structures (binary, pairing, binomial, later
thin/rank-pairing), with the extended operations pb_ds offers — stable
point iterators, `modify`, `erase`, `join`.

The structures genuinely differ in capability and storage:

- a binary heap lives in an array and cannot give stable handles;
- a pairing heap gives O(1) handles and cheap arbitrary `detach`;
- a binomial heap gives O(log n) `join` but cannot detach an interior node
  without either moving values between nodes (which breaks handle
  stability) or structural parent/child swaps (substantial extra machinery).

A first prototype failed by papering over these differences with one
implicit interface. The post-mortem found three contract gaps, not three
bugs: the container assumed the root was simultaneously a forest (in
`top()`) and a single tree (in destroy/copy); `erase` could not learn which
node had actually left the structure and freed a still-linked one; and a
blanket `noexcept` requirement copied from the tree policy concept forced
policies that *do* run the comparator into lying signatures and dummy-node
hacks.

## Decision

**1. Three-way responsibility split.**
`detail::heap::node_forest` owns *memory* (node lifetime, allocator,
POCCA/POCMA/POCS, whole-forest copy/move/destroy). The policy owns
*structure* (how trees are linked). `priority_queue` owns *meaning* (the
comparator, the public API, the choreography). Because both storage
backends are regular RAII values, the container's special members are
`= default` and constructor bodies need no rollback guards.

**2. One node representation for every node policy.**
Left-child/next-sibling with pb_ds's overloaded back edge
(`prev_or_parent`): three pointers per node, every list doubly linked, and
one uniform O(1) `splice_out` that works identically for a first child, a
middle sibling, and a root-list entry (`node.hpp`). The container-level
invariant is always *forest* — a pairing heap is a forest of one tree — so
destroy/clone/count/verify are shared, iterative (the back edges are the
traversal stack), and immune to chain-shaped trees.

**3. Capability ladder as concepts, surfaced in the API.**
`array_heap_policy` (algorithms over `std::span`) and `node_heap_policy`
select the storage backend; `mutable_node_heap_policy` adds
`detach(node, root, cmp) -> Node*` and unlocks `erase`/`modify`, and
`push`/`emplace` then return a stable `point_iterator` (the Boost.Heap
convention). `detach` *returns* the node to destroy for the same reason
the tree's `unlink_and_rebalance` returns one: only the restructuring
party knows which node left. Unsupported operations are constrained away,
not emulated.

**4. Exception contract by direction, not blanket noexcept.**
The tree concept can demand `noexcept` because tree policies never invoke
the comparator; heap policies cannot avoid it. Instead every policy obeys
the *link discipline*: comparisons happen only while all nodes are
reachable from the forest, and relinking around them is pure noexcept
pointer surgery. The resulting container-visible contract:

| operation             | comparator throws ⇒                                  |
|-----------------------|------------------------------------------------------|
| `extract_top`/`detach`| strong — nothing left the forest, invariant intact   |
| `insert`/`join`       | absorbed — ownership transferred unconditionally     |

so the container's choreography is branch-free: count before insert/join,
destroy only what `detach`/`extract_top` returned. Memory safety is
unconditional; heap *order* after a throwing comparator is unspecified on
the absorb paths (a throwing strict weak order has broken its own
contract). The binomial policy additionally keeps a deliberately *relaxed*
root-list invariant (nondecreasing rather than strictly increasing
degrees) that every operation accepts and re-canonicalizes, which is what
makes its `extract_top` rollback a one-pointer fix.
The Fibonacci policy follows the same stateless policy rule: it caches no
root-list tail pointer, so its `join` is O(roots) rather than textbook O(1)
meld.

**5. Binomial `detach` by structural, comparison-free bubbling.**
Value-bubbling would break handle stability (it is also exactly how the
prototype came to free a wrong node), so an interior node is bubbled to
its tree root with `swap_with_parent` — an O(1) structural trade made
possible by the back-edge layout (only a first child points up, so no
child list is re-parented) plus one rule: *degrees are positional* (the
slot keeps the degree, the nodes trade), which preserves the exact Bₖ
shape. The bubble needs no comparisons because the node is leaving:
after every swap, all order violations have the leaving node as parent,
so removing it — or re-linking it as a childless B₀ in the carry-pass
rollback — restores full heap order. detach therefore inherits
extract_top's strong guarantee, and its only cost over pb_ds is the same
back-edge parent walks (O(log² n) worst). Both shipped node policies are
now mutable; the node/mutable concept tiers remain separate for future
policies that cannot afford parent navigation.

## Consequences

Positive: std::priority_queue interface parity including C++23
`from_range`/`push_range` and gated CTAD; allocator-extended everything;
fully `constexpr` (compile-time smoke tests run all three policies);
policy authors get a written exception contract and a shared, verified
link toolbox; `verify()` exposes the full invariant to tests.

Negative: binomial `erase`/`modify` cost O(log² n) worst case (back-edge
parent walks during the bubble); point iterators dereference to `const`
only (mutation must go through `modify` — intentional, but a difference
from naive expectations); the absorb-on-throw rule means a failed `push`
on a node policy leaves the element inserted, which is documented but
unusual.

## Alternatives Considered

- **Separate containers per structure** (`pairing_queue`, `binomial_queue`):
  rejected — N copies of the interface and of the allocator choreography;
  pb_ds's single-front-end design is the point of the exercise.
- **Value-swap erase for binomial**: rejected — violates the pb_ds point
  iterator guarantee the library documents; caused the prototype's
  use-after-free.
- **Requiring non-throwing comparators via concept**: rejected — would ban
  ordinary lambdas (never `noexcept` unless annotated) and silently
  narrows `std::priority_queue` compatibility.
- **Untyped `heap_node_base` + downcasts** (prototype, mirroring the
  tree's header-node idiom): rejected — heaps have no header sentinel, so
  the base bought nothing and cost ~38 `static_cast`s; typed nodes plus
  the overloaded back edge eliminate every cast.
