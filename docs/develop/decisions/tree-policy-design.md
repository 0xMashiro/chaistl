# Tree Policy-Based Design

## Status

Accepted (2026-06-10)

## Context

`set` and `map` are both binary search trees with identical core logic
(position search, iteration, node lifetime, copy/move/swap) but different
balance requirements (red-black, AVL, treap, etc.) and different value types
(`Key` for set, `pair<Key, T>` for map).

Options for sharing implementation:

1. **Separate classes**: `rb_tree_set` and `rb_tree_map` as independent
   implementations. Maximum duplication.

2. **Inheritance**: `set` and `map` inherit from a common `rb_tree` base.
   Tight coupling; `set` would see `map`-specific internals.

3. **Policy-based design**: A generic `binary_search_tree` delegates balance
   mutations to a `Policy` concept. `set` and `map` are thin wrappers.

## Decision

Use policy-based design with a `balance_policy` concept.

```cpp
// binary_search_tree.hpp:58-70
template <class P, class Node>
concept balance_policy =
    requires { typename P::node_extension; } &&
    requires(bool insert_left, Node* node, bst_node_base* parent, bst_node_base& header) {
        { P::insert_and_rebalance(insert_left, node, parent, header) } noexcept;
        { P::unlink_and_rebalance(node, header) } noexcept -> std::same_as<bst_node_base*>;
    } &&
    requires(const Node* node) {
        { P::verify(node) } noexcept -> std::same_as<bool>;
    };
```

`set` and `map` are aliases over `binary_search_tree`:

```cpp
// set.hpp
using tree_type = detail::tree::binary_search_tree<
    Key, Key, identity, Compare, Allocator, rb_tree_policy>;

// map.hpp
using tree_type = detail::tree::binary_search_tree<
    Key, pair<const Key, T>, select_first, Compare, Allocator, rb_tree_policy>;
```

## Consequences

Positive:
- Single implementation for all tree containers
- Adding AVL or treap requires only a new policy, not a new tree class
- Policy is compile-time: zero runtime overhead
- `verify()` enables property-based testing per policy

Negative:
- Policy concept must be well-documented; incorrect policy breaks invariants
- `node_extension` is default-initialized; policy must fully initialize it in
  `insert_and_rebalance`
- Header pointer passing is subtle; policy must maintain root/leftmost/rightmost

## Alternatives Considered

1. **Separate `rb_tree_set` and `rb_tree_map` classes** — rejected:
   ~80% code duplication.

2. **Inheritance from `rb_tree`** — rejected: `set` should not see
   `map`-specific key extraction logic. Composition (via `tree_type` member)
   is cleaner.

3. **Virtual functions for balance operations** — rejected: unacceptable
   runtime overhead for a container.

## Amendment (2026-06-10): contract hardening and the second policy

The design was validated by adding `avl_tree_policy` (the "rule of two":
an abstraction is only proven when its second consumer arrives). The AVL
policy slotted in with zero changes to `binary_search_tree`, `set`, `map`,
the node handle, or the iterators. Three changes hardened the contract in
the process:

1. **Lifetime contract is now compile-time.** The concept requires
   `node_extension` to be trivially default constructible and trivially
   copyable: the tree creates nodes without starting the node object's
   lifetime, so default member initializers never run, and tree copies
   assign extensions as raw state. Policies must fully initialize the
   extension inside `insert_and_rebalance` (rb writes the color, avl
   writes the balance factor).

2. **Policies may carry state.** Operations are checked as instance calls
   on a `std::semiregular` policy; `binary_search_tree` stores a
   `[[no_unique_address]] Policy` and copies/moves/swaps it with the
   container. Stateless policies keep using static member functions
   (instance calls bind to them at zero cost); a future treap can hold
   its RNG.

3. **`set`/`map` expose the policy.** Both take a trailing `Policy`
   template parameter defaulting to `rb_tree_policy` — a documented
   non-standard extension so tests and benchmarks can instantiate the
   same wrapper over any policy. `set<K>` spelled with standard
   arguments is unaffected.

Shared structural mutations (`node_link_leaf`, `node_rotate_left/right`)
moved to `tree/node.hpp`; both policies use them. Every policy is hammered
by the typed property suite in
`test/chaistl/containers/tree/policy_property.cpp`: randomized op streams
checked against a `std::set` oracle with `validate()` after every single
mutation, plus sorted-insert and erase-cascade adversarial workloads. New
policies join the `Policies` type list and inherit the whole suite.

## Amendment (2026-06-12): access hooks and subtree-size capabilities

The policy design has now grown beyond "choose one balancing algorithm" into a
small capability ladder. The tree core still owns allocation, lookup, node
handles, iteration, copy/move/swap, and merge; policies still own structural
mutation and their node metadata. Optional concepts describe the extra powers a
policy can provide without letting it reach into private tree state.

### Access-changing policies

`splay_tree_policy` proved that some trees must mutate on lookup, not only on
insert/erase. Rather than expose tree internals, `binary_search_tree` detects an
optional `after_access(node, header)` hook:

```cpp
template <class P, class Node>
concept access_rebalance_policy =
    balance_policy<P, Node> &&
    requires(P policy, Node* node, bst_node_base& header) {
      { policy.after_access(node, header) } noexcept -> std::same_as<void>;
    };
```

`find`, mutable `lower_bound`, and mutable `upper_bound` notify the policy with
the last real node visited by the search. Const lookup deliberately does not
notify, preserving const semantics.

### Subtree-size policies

Three policies now maintain `extension.subtree_size`:

- `order_statistic_tree_policy`: treap priority + subtree size.
- `weight_balanced_tree_policy`: ratio-based balancing by subtree weight.
- `size_balanced_tree_policy`: Size Balanced Tree invariants over grandchild
  subtree sizes.

This metadata is useful in two ways:

1. It can be an implementation detail for balancing.
2. It can power public rank/select operations.

The code therefore names the metadata capability separately:

```cpp
template <class P, class Node>
concept subtree_size_policy =
    balance_policy<P, Node> &&
    requires(const Node* node) {
      { node->extension.subtree_size } -> std::convertible_to<std::size_t>;
    };

template <class P, class Node>
concept order_statistic_policy = subtree_size_policy<P, Node>;
```

Today every subtree-size policy can answer `find_by_order(k)` and
`order_of_key(key)`. Keeping the `order_statistic_policy` name in the public
operation gate documents the algorithmic interface separately from the node
metadata convention.

### Policy matrix

| Policy | Metadata | Extra capability | Primary teaching point |
|--------|----------|------------------|------------------------|
| `rb_tree_policy` | color | none | Standard-library baseline |
| `avl_tree_policy` | balance factor | none | Height strictness vs mutation cost |
| `splay_tree_policy` | none | `after_access` | Lookup can restructure the tree |
| `treap_tree_policy` | random priority | none | Randomized balance as heap order |
| `order_statistic_tree_policy` | priority + subtree size | rank/select | Augmentation as user-visible API |
| `weight_balanced_tree_policy` | subtree size | rank/select | Balance directly from subtree weight |
| `size_balanced_tree_policy` | subtree size | rank/select | SBT grandchild-size invariants |

### Public aliases

Stable extension aliases live beside the standard-compatible containers:

- Policy-specialized ordered containers:
  `avl_*`, `splay_*`, `treap_*`, `weight_balanced_*`,
  `size_balanced_*`.
- Rank/select containers:
  `order_statistic_set`, `order_statistic_multiset`,
  `order_statistic_map`, `order_statistic_multimap`.

The rank/select member functions are also conditionally available on any
ordered associative container instantiated with a subtree-size policy. For
example, `weight_balanced_set<int>` and `size_balanced_set<int>` support
`find_by_order` and `order_of_key` even though their names describe the
balancing policy rather than the operation family.

### Why not a rebuild-capable policy yet?

Scapegoat trees need bulk subtree rebuilding. That is a different capability
from local rotations and access hooks: a policy would need a way to detach,
flatten, rebuild, and relink a whole subtree while preserving allocator and
node-handle invariants. The current contract intentionally stops at local
mutation. A rebuild-capable policy should be introduced as a separate concept
only when a concrete container needs it.
