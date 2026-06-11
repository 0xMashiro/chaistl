# Rotation Primitives Analysis: Per-Rotation vs 3+4 Reassembly

> This document analyzes the trade-off between per-rotation primitives and
> connect34-style reassembly for BST rebalancing. It informed the decision
> recorded in ADR-004 (tree policy design) and the implementation of
> `rb_tree_policy` and `avl_tree_policy`.
>
> Status: Analysis complete, decision implemented.

## Context

dsacpp (Junhui Deng, Tsinghua; see `reference/dsacpp-src`) unifies all BST
rebalancing through "3+4 reconstruction": every single or double rotation is
expressed as tearing three nodes and four subtrees apart and reassembling
them in-order. The mechanism has two layers:

- `connect34(a, b, c, T0..T3)` — reassembles seven pointers into the
  canonical locally-balanced shape, returns `b` as the new subtree root.
- `rotateAt(v)` — classifies the v/parent/grandparent shape (zig-zig,
  zig-zag, ...), picks the in-order permutation of the seven arguments,
  and relinks the result into the ancestor structure.

dsacpp's AVL and red-black trees never call `connect34` directly; all uses
go through `rotateAt`. Its red-black double-red repair recolors first, then
performs one `rotateAt` call.

chaistl's tree layer instead exposes `node_rotate_left` / `node_rotate_right`
(`node.hpp`), consumed by the policy fixups in the libstdc++ style:
recoloring/balance updates interleaved with individual rotations
(`rb_tree_policy`, `avl_tree_policy`).

A faithful port of `connect34` (named `node_reassemble`) was prototyped and
verified correct via compile-time `static_assert`, then evaluated for
adoption.

## Analysis

### Decision

Keep the per-rotation primitives. Do not adopt a `connect34`-style interface
in the tree layer.

### Consequences

Positive:
- The RB/AVL fixups stay as verified: 670 tests, the cross-policy property
  suite (std::set oracle + per-step validate), and the clang/gcc/ASan/UBSan
  CI matrix all exercise the current rotation paths.
- Single rotations write ~6 pointer fields; a full reassembly writes 10+
  plus four null checks. RB insert performs <1 rotation amortized, so the
  reassembly cost would be pure overhead on the common path.
- No interface whose correctness rests on the caller ordering seven
  same-typed `bst_node_base*` arguments correctly — every permutation
  compiles, and no concept or type distinction can catch a mistake. This
  follows the contract-hardening direction of ADR 004 and its amendment.

Negative:
- Double rotations are expressed as two `node_rotate_*` calls with some
  redundant intermediate pointer writes (cost-equivalent to one reassembly,
  but less direct).
- The unifying insight — every rebalancing case is the same in-order
  reassembly of three nodes and four subtrees — lives only in this document
  rather than in code.

### Alternatives Considered

1. **Port `connect34` alone** (the prototyped `node_reassemble`) — rejected:
   without the `rotateAt` layer, the error-prone part (choosing the argument
   permutation per shape, plus the header-protocol ancestor relink) is left
   at every call site. This is the worst half of the design to expose.

2. **Port both layers (`rotateAt` + `connect34`)** — rejected: the case
   dispatch would be encapsulated, but adopting it means rewriting both
   policy fixups and re-deriving every recoloring/balance-update case
   against the new primitive. Zero correctness gain over the verified
   implementation; real regression risk. Industrial implementations
   (libstdc++, libc++, Boost.intrusive, EASTL) uniformly use per-rotation
   primitives — 3+4 reconstruction optimizes pedagogical uniformity, not
   implementation quality.

3. **Local rebuilding for future policies** — left open: scapegoat and
   weight-balanced trees rebalance by rebuilding subtrees wholesale, where
   a reassembly-style helper is the natural vocabulary. If such a policy is
   added, the helper should be introduced as part of that policy, private to
   it, rather than retrofitted onto the RB/AVL paths.

## References

- dsacpp `src/BST/BST_connect34.h`, `src/BST/BST_RotateAt.h`,
  `src/redBlack/RedBlack_solveDoubleRed.h`
- libstdc++ `tree.cc` (`_Rb_tree_rotate_left` / `_Rb_tree_rotate_right`),
  Boost.intrusive `bstree_algorithms` — per-rotation primitives throughout
- ADR 004: Tree Policy-Based Design (balance state belongs to the policy)
