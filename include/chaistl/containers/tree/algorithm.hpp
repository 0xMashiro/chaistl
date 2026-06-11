// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>

#include <utility>

namespace chaistl::detail::tree {

// ============================================================================
// Generic BST algorithms (independent of balance policy)
//
// These operate on tree nodes and use a KeyOfValue + Compare to navigate.
// They are the shared foundation used by all binary_search_tree
// instantiations regardless of balancing scheme. None of them mutate the
// tree; mutation belongs to the balance policy.
// ============================================================================

/**
 * @brief Result of a lookup-style descent.
 *
 * @c result follows the usual STL lookup convention: it is the found/bound
 * node, or the header sentinel playing the role of end() when no such node
 * exists. @c last_visited is different from that sentinel: it is the last
 * real tree node compared during the descent. Self-adjusting policies such
 * as splay trees use that node as the access frontier on misses, while
 * ordinary RB/AVL/Treap policies ignore it.
 */
struct lookup_result {
  /// The public lookup result: matching/bound node, or header as end().
  bst_node_base* result;

  /// Last non-header node touched by the descent; null only for an empty tree.
  bst_node_base* last_visited;
};

/**
 * @brief Find the first node whose key is not less than @p key.
 *
 * Returns the node that would be at @c lower_bound(key). If all keys are
 * less than @p key, returns @p header (the end node).
 */
template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr lookup_result lower_bound_search(
    Node* root, bst_node_base* header, const Key& key, const KeyOfValue& kov, const Compare& comp) {
  bst_node_base* result = header;
  bst_node_base* last_visited = nullptr;
  while (root != nullptr) {
    last_visited = root;
    if (!comp(kov(root->value), key)) {
      result = root;
      root = static_cast<Node*>(root->left);
    } else {
      root = static_cast<Node*>(root->right);
    }
  }
  return {result, last_visited};
}

template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr bst_node_base* lower_bound_node(
    Node* root, bst_node_base* header, const Key& key, const KeyOfValue& kov, const Compare& comp) {
  return lower_bound_search(root, header, key, kov, comp).result;
}

/**
 * @brief Find the first node whose key is greater than @p key.
 *
 * Returns the node that would be at @c upper_bound(key). If all keys are
 * not greater than @p key, returns @p header (the end node).
 */
template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr lookup_result upper_bound_search(
    Node* root, bst_node_base* header, const Key& key, const KeyOfValue& kov, const Compare& comp) {
  bst_node_base* result = header;
  bst_node_base* last_visited = nullptr;
  while (root != nullptr) {
    last_visited = root;
    if (comp(key, kov(root->value))) {
      result = root;
      root = static_cast<Node*>(root->left);
    } else {
      root = static_cast<Node*>(root->right);
    }
  }
  return {result, last_visited};
}

template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr bst_node_base* upper_bound_node(
    Node* root, bst_node_base* header, const Key& key, const KeyOfValue& kov, const Compare& comp) {
  return upper_bound_search(root, header, key, kov, comp).result;
}

/**
 * @brief Find the node equivalent to @p key, if any.
 *
 * @c result is either the matching node or @p header. @c last_visited is
 * the final real node compared with @p key, which lets self-adjusting
 * policies splay the hit or the miss frontier without routing find()
 * through lower_bound().
 */
template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr lookup_result find_search(
    Node* root, bst_node_base* header, const Key& key, const KeyOfValue& kov, const Compare& comp) {
  bst_node_base* last_visited = nullptr;
  while (root != nullptr) {
    last_visited = root;
    if (comp(kov(root->value), key)) {
      root = static_cast<Node*>(root->right);
    } else if (comp(key, kov(root->value))) {
      root = static_cast<Node*>(root->left);
    } else {
      return {root, last_visited};
    }
  }
  return {header, last_visited};
}

/**
 * @brief Find the range of nodes with key equal to @p key (unique keys).
 *
 * Single descent: while searching, the last node we descended left from is
 * remembered as @p result — it is the in-order successor of the match if
 * the match has no right subtree. On a hit, the range is therefore
 * @c {node, min(node->right)} or @c {node, result}, without a second
 * descent from the root.
 *
 * For a unique-key tree the range contains at most one element; a tree
 * with equivalent keys needs @ref equal_range_multi_node instead.
 *
 * Reference: libc++ __tree::__equal_range_unique.
 */
template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr std::pair<bst_node_base*, bst_node_base*> equal_range_unique_node(
    Node* root, bst_node_base* header, const Key& key, const KeyOfValue& kov, const Compare& comp) {
  bst_node_base* result = header;
  while (root != nullptr) {
    if (comp(kov(root->value), key)) {
      root = static_cast<Node*>(root->right);
    } else if (comp(key, kov(root->value))) {
      result = root;
      root = static_cast<Node*>(root->left);
    } else {
      return {root, root->right != nullptr ? node_minimum(root->right) : result};
    }
  }
  return {result, result};
}

/**
 * @brief Find the range of nodes with key equivalent to @p key
 *        (equivalent keys allowed).
 *
 * The descent runs like @ref equal_range_unique_node until it hits an
 * equivalent node — but with duplicates that node is merely <em>some</em>
 * member of the run, so the search forks: lower bound in its left subtree
 * (everything left of the hit is smaller or equivalent), upper bound in
 * its right subtree. The hit node / the remembered successor serve as the
 * not-found fallbacks, which is exactly the role the @c header parameter
 * of those helpers plays.
 *
 * Reference: libstdc++ _Rb_tree::equal_range.
 */
template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr std::pair<bst_node_base*, bst_node_base*> equal_range_multi_node(
    Node* root, bst_node_base* header, const Key& key, const KeyOfValue& kov, const Compare& comp) {
  Node* x = root;
  bst_node_base* result = header;
  while (x != nullptr) {
    if (comp(kov(x->value), key)) {
      x = static_cast<Node*>(x->right);
    } else if (comp(key, kov(x->value))) {
      result = x;
      x = static_cast<Node*>(x->left);
    } else {
      return {lower_bound_node(static_cast<Node*>(x->left), x, key, kov, comp),
              upper_bound_node(static_cast<Node*>(x->right), result, key, kov, comp)};
    }
  }
  return {result, result};
}

/**
 * @brief Result of an insert-position search.
 *
 * Exactly one of the two cases holds:
 * - @c existing != nullptr: the key is already present at @c existing;
 *   nothing may be inserted.
 * - @c existing == nullptr: attach the new node under @c parent, as its
 *   left child if @c as_left_child, else as its right child. When the
 *   tree is empty @c parent is the header and the new node becomes root.
 *
 * This replaces the classic libstdc++ pair<ptr,ptr> encoding ({pos,parent}
 * to insert, {node,null} for duplicates) with named fields.
 */
struct insert_position {
  bst_node_base* existing;
  bst_node_base* parent;
  bool as_left_child;
};

/**
 * @brief Find the insert position for a unique key.
 *
 * Descends from @p root remembering the direction of the last comparison.
 * Deciding "duplicate or not" then costs exactly one extra comparison:
 *
 * - Descent ended going left: the only candidate equal to @p key is the
 *   in-order predecessor of the attach point. If the attach point is the
 *   leftmost node (or the tree is empty), there is no predecessor.
 * - Descent ended going right: the attach point itself is the candidate
 *   (we already know !comp(key, parent), so one comparison settles it).
 *
 * Reference: libstdc++ _Rb_tree::_M_get_insert_unique_pos.
 */
template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr insert_position get_insert_unique_pos(
    Node* root, bst_node_base& header, const Key& key, const KeyOfValue& kov, const Compare& comp) {
  Node* x = root;
  bst_node_base* parent = &header;
  bool went_left = true;

  while (x != nullptr) {
    parent = x;
    went_left = comp(key, kov(x->value));
    x = static_cast<Node*>(went_left ? x->left : x->right);
  }

  if (went_left) {
    if (parent == header.left) {
      // Inserting before begin() — covers the empty tree, where
      // header.left == &header == parent.
      return {nullptr, parent, true};
    }
    bst_node_base* pred = node_predecessor(parent);
    if (comp(kov(static_cast<Node*>(pred)->value), key)) {
      return {nullptr, parent, true};
    }
    return {pred, nullptr, false};  // pred <= key and key <= pred: duplicate
  }

  if (comp(kov(static_cast<Node*>(parent)->value), key)) {
    return {nullptr, parent, false};
  }
  return {parent, nullptr, false};  // parent <= key and key <= parent: duplicate
}

/**
 * @brief Find the insert position for a hint-based unique insertion.
 *
 * When the new key belongs immediately before @p hint, the position is
 * found in O(1) by checking the hint's neighborhood; otherwise this falls
 * back to @ref get_insert_unique_pos. An attach point next to an existing
 * node is always a free slot: if the predecessor gained the new node as a
 * right child it had none (otherwise the in-order neighbor would lie in
 * that subtree), and symmetrically for the successor's left.
 *
 * Reference: libstdc++ _Rb_tree::_M_get_insert_hint_unique_pos.
 */
template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr insert_position get_insert_hint_unique_pos(Node* root,
                                                                   bst_node_base& header,
                                                                   bst_node_base* hint,
                                                                   const Key& key,
                                                                   const KeyOfValue& kov,
                                                                   const Compare& comp) {
  const auto key_of = [&kov](bst_node_base* node) -> decltype(auto) {
    return kov(static_cast<Node*>(node)->value);
  };

  if (hint == &header) {  // end(): valid when key is greater than the maximum
    if (root != nullptr && comp(key_of(header.right), key)) {
      return {nullptr, header.right, false};
    }
    return get_insert_unique_pos(root, header, key, kov, comp);
  }

  if (comp(key, key_of(hint))) {  // key < hint: belongs immediately before it?
    if (hint == header.left) {
      return {nullptr, hint, true};  // inserting before begin()
    }
    bst_node_base* before = node_predecessor(hint);
    if (comp(key_of(before), key)) {
      return before->right == nullptr ? insert_position{nullptr, before, false} : insert_position{nullptr, hint, true};
    }
    return get_insert_unique_pos(root, header, key, kov, comp);
  }

  if (comp(key_of(hint), key)) {  // key > hint: belongs immediately after it?
    if (hint == header.right) {
      return {nullptr, hint, false};  // inserting past the maximum
    }
    bst_node_base* after = node_successor(hint);
    if (comp(key, key_of(after))) {
      return hint->right == nullptr ? insert_position{nullptr, hint, false} : insert_position{nullptr, after, true};
    }
    return get_insert_unique_pos(root, header, key, kov, comp);
  }

  return {hint, nullptr, false};  // hint itself holds an equal key
}

/**
 * @brief Find the insert position for an equivalent-keys insertion.
 *
 * @ref get_insert_unique_pos without the duplicate detection: the descent
 * simply runs until it falls off the tree, and a key equivalent to a
 * visited node continues into its <em>right</em> subtree. The attach point
 * is therefore the key's upper bound — a run of equivalent elements keeps
 * insertion order, as the resolution of LWG 233 requires for multiset /
 * multimap @c insert(value).
 *
 * @c existing is always null: an equivalent-keys tree never refuses.
 *
 * Reference: libstdc++ _Rb_tree::_M_get_insert_equal_pos.
 */
template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr insert_position get_insert_equal_pos(
    Node* root, bst_node_base& header, const Key& key, const KeyOfValue& kov, const Compare& comp) {
  Node* x = root;
  bst_node_base* parent = &header;
  bool went_left = true;  // empty tree: the node becomes root via the header

  while (x != nullptr) {
    parent = x;
    went_left = comp(key, kov(x->value));
    x = static_cast<Node*>(went_left ? x->left : x->right);
  }
  return {nullptr, parent, went_left};
}

/**
 * @brief Find the insert position for a hint-based equivalent-keys
 *        insertion.
 *
 * When the key may sit immediately before @p hint (predecessor ≤ key ≤
 * hint) the position is found in O(1) from the hint's neighborhood and the
 * element lands directly before @p hint — "as close as possible to hint"
 * ([associative.reqmts] for containers with equivalent keys). Symmetrically
 * for immediately after. Otherwise this falls back to
 * @ref get_insert_equal_pos and the element lands at the key's upper bound.
 *
 * The structure mirrors @ref get_insert_hint_unique_pos with non-strict
 * neighbor comparisons (equivalent neighbors are acceptable here) and no
 * duplicate exit; the free-slot argument for the chosen attach point is
 * the same.
 *
 * Reference: libstdc++ _Rb_tree::_M_get_insert_hint_equal_pos.
 */
template <class Key, class KeyOfValue, class Compare, class Node>
[[nodiscard]] constexpr insert_position get_insert_hint_equal_pos(Node* root,
                                                                  bst_node_base& header,
                                                                  bst_node_base* hint,
                                                                  const Key& key,
                                                                  const KeyOfValue& kov,
                                                                  const Compare& comp) {
  const auto key_of = [&kov](bst_node_base* node) -> decltype(auto) {
    return kov(static_cast<Node*>(node)->value);
  };

  if (hint == &header) {  // end(): valid when key is not less than the maximum
    if (root != nullptr && !comp(key, key_of(header.right))) {
      return {nullptr, header.right, false};
    }
    return get_insert_equal_pos(root, header, key, kov, comp);
  }

  if (!comp(key_of(hint), key)) {  // key <= hint: belongs immediately before it?
    if (hint == header.left) {
      return {nullptr, hint, true};  // inserting before begin()
    }
    bst_node_base* before = node_predecessor(hint);
    if (!comp(key, key_of(before))) {  // before <= key <= hint
      return before->right == nullptr ? insert_position{nullptr, before, false} : insert_position{nullptr, hint, true};
    }
    return get_insert_equal_pos(root, header, key, kov, comp);
  }

  // hint < key: belongs immediately after it?
  if (hint == header.right) {
    return {nullptr, hint, false};  // inserting past the maximum
  }
  bst_node_base* after = node_successor(hint);
  if (!comp(key_of(after), key)) {  // hint < key <= after
    return hint->right == nullptr ? insert_position{nullptr, hint, false} : insert_position{nullptr, after, true};
  }
  return get_insert_equal_pos(root, header, key, kov, comp);
}

}  // namespace chaistl::detail::tree
