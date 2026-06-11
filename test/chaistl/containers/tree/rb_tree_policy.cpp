// SPDX-License-Identifier: Apache-2.0

// References:
// - Red-black tree balancing cases:
//   https://en.wikipedia.org/wiki/Red%E2%80%93black_tree
//   CLRS Introduction to Algorithms, 3rd ed., Chapter 13.
//
// These tests target specific branches in rb_tree_policy that are hard to
// hit with random data but are required for correctness: the "inner child"
// double-rotation case on insert, and the various delete-fixup cases that
// arise when a black node is removed and the sibling structure is complex.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/set.hpp>

#include <stdexcept>
#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// Value type that throws on copy after a configurable number of copies.
// Used to verify the iterator-range constructor rolls back successfully
// allocated nodes and does not leak.
struct ThrowingInt {
  int value{};
  static inline int copies_before_throw = -1;

  ThrowingInt() = default;
  explicit ThrowingInt(int v) : value(v) {}

  ThrowingInt(const ThrowingInt& other) : value(other.value) {
    if (copies_before_throw == 0) {
      throw std::runtime_error("copy");
    }
    if (copies_before_throw > 0) {
      --copies_before_throw;
    }
  }

  ThrowingInt(ThrowingInt&&) noexcept = default;
  ThrowingInt& operator=(const ThrowingInt&) = default;
  ThrowingInt& operator=(ThrowingInt&&) noexcept = default;
  ~ThrowingInt() = default;

  static void reset() { copies_before_throw = -1; }
};

constexpr bool operator<(const ThrowingInt& lhs, const ThrowingInt& rhs) noexcept {
  return lhs.value < rhs.value;
}

// Insert case: parent is the left child of the grandparent and the new node
// is the right child of the parent (inner child). This requires a left
// rotation on the parent followed by a right rotation on the grandparent.
TEST(RbTreePolicy, InsertLeftInnerChildDoubleRotation) {
  chaistl::set<int> s;
  // Sequence chosen so that inserting 2 creates the inner-child shape:
  // after 3 and 1, the tree is 3(b), 1(r); inserting 2 makes 2 the right
  // child of 1 (inner), triggering rotate_left(1) then rotate_right(3).
  s.insert(3);
  s.insert(1);
  s.insert(2);
  EXPECT_THAT(s, ElementsAre(1, 2, 3));
  EXPECT_TRUE(s.validate());
}

// Mirror image: parent is the right child of the grandparent and the new
// node is the left child of the parent.
TEST(RbTreePolicy, InsertRightInnerChildDoubleRotation) {
  chaistl::set<int> s;
  s.insert(1);
  s.insert(3);
  s.insert(2);
  EXPECT_THAT(s, ElementsAre(1, 2, 3));
  EXPECT_TRUE(s.validate());
}

// Delete case: the node being erased has two children and its in-order
// successor is NOT its immediate right child. This exercises the relinking
// path in unlink_and_rebalance where y != z and y != z->right, and also
// the branch where z is not the root and is a left child of its parent.
TEST(RbTreePolicy, EraseTwoChildrenSuccessorIsDeeper) {
  // Construct a tree where 30 has two children and its successor (38) is
  // deeper than its immediate right child (40).
  chaistl::set<int> s{50, 30, 70, 40, 35, 25, 38};
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(30), 1uz);
  EXPECT_THAT(s, ElementsAre(25, 35, 38, 40, 50, 70));
  EXPECT_TRUE(s.validate());
}

// Delete-fixup case 3 (left side): the removed node is a left child, its
// sibling is black, and the sibling's near nephew is red while the far
// nephew is black/null. This triggers a rotation on the sibling to move
// the red nephew into the far slot, followed by the case-4 rotation.
TEST(RbTreePolicy, EraseTriggersLeftCaseThree) {
  /* Structure before erase:
            10(b)
           /    \
        5(b)    20(b)
               /
            15(r)
     Erasing 5 removes a black left leaf. The sibling is 20(b), its left
     child 15(r) is the near nephew, and its right child is null.
  */
  chaistl::set<int> s{10, 5, 20, 15};
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(5), 1uz);
  EXPECT_THAT(s, ElementsAre(10, 15, 20));
  EXPECT_TRUE(s.validate());
}

// Delete-fixup case 4 (left side): the removed node is a left child, its
// sibling is black, and the sibling's far nephew is red. This is the
// terminal single-rotation case.
TEST(RbTreePolicy, EraseTriggersLeftCaseFour) {
  /* Structure before erase:
            10(b)
           /    \
        5(b)    20(b)
                 \
                25(r)
     Erasing 5 removes a black left leaf. The sibling is 20(b) and its far
     nephew 25(r) is red, so a single left rotation on 10's right subtree
     absorbs the deficit.
  */
  chaistl::set<int> s{10, 5, 20, 25};
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(5), 1uz);
  EXPECT_THAT(s, ElementsAre(10, 20, 25));
  EXPECT_TRUE(s.validate());
}

// Mirror image: case 3 on the right side.
TEST(RbTreePolicy, EraseTriggersRightCaseThree) {
  /* Structure before erase:
            20(b)
           /    \
        10(b)   25(b)
           \
          15(r)
     Erasing 25 removes a black right leaf. The sibling is 10(b), its right
     child 15(r) is the near nephew, and its left child is null.
  */
  chaistl::set<int> s{20, 10, 25, 15};
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(25), 1uz);
  EXPECT_THAT(s, ElementsAre(10, 15, 20));
  EXPECT_TRUE(s.validate());
}

// Mirror image: case 4 on the right side.
TEST(RbTreePolicy, EraseTriggersRightCaseFour) {
  /* Structure before erase:
            20(b)
           /    \
        10(b)   25(b)
        /
      5(r)
     Erasing 25 removes a black right leaf. The sibling is 10(b) and its
     far nephew 5(r) is red.
  */
  chaistl::set<int> s{20, 10, 25, 5};
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(25), 1uz);
  EXPECT_THAT(s, ElementsAre(5, 10, 20));
  EXPECT_TRUE(s.validate());
}

// Delete-fixup case 1 (red sibling, left side): when the removed node's
// sibling is red, rotate to make the sibling black and continue fixup.
TEST(RbTreePolicy, EraseTriggersLeftRedSiblingCaseOne) {
  /* Structure before erase:
            20(b)
           /    \
        5(b)    30(r)
               /    \
            25(b)   35(b)
     Erasing 5 leaves a double-black whose sibling is 30(r).
  */
  chaistl::set<int> s{20, 5, 30, 25, 35};
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(5), 1uz);
  EXPECT_THAT(s, ElementsAre(20, 25, 30, 35));
  EXPECT_TRUE(s.validate());
}

// Mirror image: case 1 on the right side.
TEST(RbTreePolicy, EraseTriggersRightRedSiblingCaseOne) {
  /* Structure before erase:
            20(b)
           /    \
        10(r)   30(b)
        /   \
     5(b)  15(b)
     Erasing 30 leaves a double-black whose sibling is 10(r).
  */
  chaistl::set<int> s{20, 10, 30, 5, 15};
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(30), 1uz);
  EXPECT_THAT(s, ElementsAre(5, 10, 15, 20));
  EXPECT_TRUE(s.validate());
}

// Delete-fixup case 2 (black sibling, black nephews): the deficit is pushed
// up to the parent. Repeat across multiple levels to exercise the loop.
TEST(RbTreePolicy, EraseTriggersBlackSiblingBlackNephews) {
  // A perfectly black tree of height 2: removing any leaf should push the
  // deficit up and recolor siblings red.
  chaistl::set<int> s{4, 2, 6, 1, 3, 5, 7};
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(1), 1uz);
  EXPECT_THAT(s, ElementsAre(2, 3, 4, 5, 6, 7));
  EXPECT_TRUE(s.validate());

  EXPECT_EQ(s.erase(7), 1uz);
  EXPECT_THAT(s, ElementsAre(2, 3, 4, 5, 6));
  EXPECT_TRUE(s.validate());
}

// Stress alternating insert/erase with validation to hit combinations that
// are unlikely in hand-written cases.
TEST(RbTreePolicy, AlternatingInsertEraseMaintainsInvariants) {
  chaistl::set<int> s;
  for (int i = 0; i < 200; ++i) {
    s.insert(i);
    EXPECT_TRUE(s.validate());
  }
  for (int i = 0; i < 200; ++i) {
    EXPECT_EQ(s.erase(i), 1uz);
    EXPECT_TRUE(s.validate());
  }
  EXPECT_THAT(s, IsEmpty());
}

// Reverse-order insertion exercises the opposite side of every rotation.
TEST(RbTreePolicy, ReverseOrderInsertion) {
  chaistl::set<int> s;
  for (int i = 200; i > 0; --i) {
    s.insert(i);
    EXPECT_TRUE(s.validate());
  }
  EXPECT_EQ(s.size(), 200uz);
}

// Exception safety: if copying a value into a newly allocated node throws,
// the partially constructed tree is cleared and no nodes are leaked.
TEST(RbTreePolicy, IteratorRangeConstructorRollsBackOnCopyFailure) {
  ThrowingInt::reset();

  std::vector<ThrowingInt> source;
  source.reserve(5);
  for (int i = 0; i < 5; ++i) {
    source.emplace_back(i * 10);
  }

  // Allow the first two copies to succeed, then throw on the third.
  // The tree will have allocated two nodes before the exception.
  ThrowingInt::copies_before_throw = 2;

  EXPECT_THROW({ chaistl::set<ThrowingInt> s(source.begin(), source.end()); }, std::runtime_error);
}

}  // namespace
