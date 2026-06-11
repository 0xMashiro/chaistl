// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/heap/node.hpp>

#include <concepts>
#include <span>
#include <type_traits>

namespace chaistl::heap_policy {

// ============================================================================
// Heap policy concepts
//
// A heap policy is a stateless bundle of static algorithms — the modern
// replacement for pb_ds's tag-dispatch class hierarchy. The container
// supplies storage, allocator, comparator and lifetime; the policy supplies
// structure. Which policy *category* is selected decides both the storage
// backend and the public API surface of priority_queue:
//
//   array_heap_policy          → contiguous storage (chaistl::vector),
//                                std::priority_queue interface.
//   node_heap_policy           → linked forest storage (node_forest),
//                                adds join().
//   mutable_node_heap_policy   → additionally supports detaching an
//                                arbitrary node, which unlocks erase(it),
//                                modify(it, v), and handle-returning
//                                push/emplace. Both shipped node policies
//                                (pairing, binomial) model it; the tiers
//                                stay separate for policies that cannot
//                                (e.g. meldable heaps that intentionally
//                                expose join() but not stable handles).
//
// The capability ladder is deliberate: rather than every policy faking every
// operation (and lying about complexity or, worse, correctness), a policy
// models exactly the concepts it can honor, and priority_queue's members
// constrain themselves on those concepts. Unsupported operations fail to
// compile with the concept's name in the diagnostic.
//
// Exception contract (see docs/develop/decisions/heap-policy-design.md):
// policy operations are NOT required to be noexcept — they invoke the
// user's comparator, which may throw. Instead, every policy obeys the link
// discipline: the comparator is only ever invoked while all nodes are
// reachable from a forest the container owns, and pure pointer surgery
// (which is noexcept) is sequenced around those comparison points. A
// throwing comparator can therefore abandon an operation midway without
// leaking a node or leaving the forest untraversable.
// ============================================================================

// ----------------------------------------------------------------------------
// node_extension_of — per-node state, with detection
//
// A policy that needs per-node bookkeeping (a binomial node's degree, a thin
// heap's rank, ...) declares a nested `node_extension`. Policies that need
// none simply omit it and this trait substitutes the empty placeholder —
// detection through a constrained partial specialization, the concepts-era
// successor of the detection idiom.
// ----------------------------------------------------------------------------

template <class Policy>
struct node_extension_of {
  using type = detail::heap::no_extension;
};

template <class Policy>
  requires requires { typename Policy::node_extension; }
struct node_extension_of<Policy> {
  using type = typename Policy::node_extension;
};

template <class Policy>
using node_extension_of_t = typename node_extension_of<Policy>::type;

/// The node type a node policy operates on, for element type T.
template <class Policy, class T>
using policy_node_t = detail::heap::heap_node<T, node_extension_of_t<Policy>>;

// ----------------------------------------------------------------------------
// array_heap_policy
//
// Operates on the contiguous element range the container owns, expressed as
// std::span — the policy never sees (or needs) the vector, the allocator,
// or element lifetime. Contract:
//
//   push(heap, cmp)  pre:  heap.first(n-1) is a heap, heap.back() is new.
//                    post: heap is a heap.
//   pop(heap, cmp)   pre:  heap is a non-empty heap.
//                    post: heap.first(n-1) is a heap; the old top value is
//                          at heap.back(), ready for the container's
//                          pop_back(). (The value may pass through a
//                          moved-from state if the comparator throws.)
//   make(heap, cmp)  post: heap is a heap, from arbitrary input.
//   verify(heap,cmp) the heap property, for tests.
//
// The top element is heap.front() by definition of an array heap — no
// accessor needed. "Heap" always means max-heap with respect to cmp, the
// std::push_heap convention.
// ----------------------------------------------------------------------------

template <class P, class T, class Compare>
concept array_heap_policy = std::is_empty_v<P> && std::semiregular<P> &&
                            requires(std::span<T> heap, std::span<const T> const_heap, const Compare& cmp) {
                              { P::push(heap, cmp) } -> std::same_as<void>;
                              { P::pop(heap, cmp) } -> std::same_as<void>;
                              { P::make(heap, cmp) } -> std::same_as<void>;
                              { P::verify(const_heap, cmp) } -> std::same_as<bool>;
                            };

// ----------------------------------------------------------------------------
// node_heap_policy
//
// Operates on a left-child/next-sibling forest (see node.hpp) whose head
// pointer the container passes by reference. Contract:
//
//   insert(node, root, cmp)   link a detached singleton into the forest.
//   extract_top(root, cmp)    unlink and return the top node, fully
//                             detached and childless; the container
//                             destroys it.
//   top(root, cmp)            the node holding the top element; the forest
//                             is not modified. Never null (container checks
//                             emptiness first).
//   join(root, other, cmp)    absorb the forest `other` (already detached
//                             from its previous owner) into root.
//   verify(root, cmp)         full structural + order check, for tests.
//
// Exception contract, the rule the container's choreography is written
// against (each direction is what makes the corresponding caller simple):
//
//   insert / join:            ownership transfers UNCONDITIONALLY. When the
//                             comparator throws, the node/forest has still
//                             been absorbed, so the container neither
//                             destroys nor re-owns anything — it only has
//                             to count the elements in before the call.
//   extract_top / detach:     STRONG. When the comparator throws, no node
//                             has left the forest and the heap invariant
//                             still holds; the operation simply did not
//                             happen.
//
// After a throwing comparator, memory safety and traversability are
// unconditional; heap *order* on the insert/join paths is unspecified (a
// strict weak ordering that throws has already broken its own contract).
//
// Node identity is sacred: implementations relink nodes but never move a
// value between nodes, because point iterators are defined to track the
// element they were created for (pb_ds's invalidation guarantee).
// ----------------------------------------------------------------------------

template <class P, class T, class Compare>
concept node_heap_policy = std::is_empty_v<P> && std::semiregular<P> &&
                           requires(policy_node_t<P, T>* node,
                                    policy_node_t<P, T>*& root,
                                    const policy_node_t<P, T>* const_root,
                                    const Compare& cmp) {
                             { P::insert(node, root, cmp) } -> std::same_as<void>;
                             { P::extract_top(root, cmp) } -> std::same_as<policy_node_t<P, T>*>;
                             { P::top(const_root, cmp) } -> std::same_as<const policy_node_t<P, T>*>;
                             { P::join(root, node, cmp) } -> std::same_as<void>;
                             { P::verify(const_root, cmp) } -> std::same_as<bool>;
                           };

// ----------------------------------------------------------------------------
// mutable_node_heap_policy
//
// The capability that unlocks erase(it) and modify(it, v):
//
//   detach(node, root, cmp)   unlink an *arbitrary* node, reattaching its
//                             children so the heap invariant holds for the
//                             remaining forest; returns the detached node,
//                             fully unlinked and childless.
//
// detach returns the node for the same reason the tree layer's
// unlink_and_rebalance returns one (tree-policy-design.md): the party that
// restructures is the only party that knows which node left the structure,
// and the party that owns memory must be handed exactly that node. The
// original heap prototype lost this rule and freed still-linked nodes.
// ----------------------------------------------------------------------------

template <class P, class T, class Compare>
concept mutable_node_heap_policy = node_heap_policy<P, T, Compare> &&
                                   requires(policy_node_t<P, T>* node, policy_node_t<P, T>*& root, const Compare& cmp) {
                                     { P::detach(node, root, cmp) } -> std::same_as<policy_node_t<P, T>*>;
                                   };

/// Any policy priority_queue can be instantiated with.
template <class P, class T, class Compare>
concept heap_policy_for = array_heap_policy<P, T, Compare> || node_heap_policy<P, T, Compare>;

}  // namespace chaistl::heap_policy
