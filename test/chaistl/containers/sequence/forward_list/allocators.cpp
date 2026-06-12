// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list allocator-aware operations:
//   https://en.cppreference.com/w/cpp/container/forward_list
//   https://eel.is/c++draft/forwardlist

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

#include "../support.hpp"

namespace {

using chaistl::forward_list;

// =======================================================================
// TaggedAllocator (non-equal, non-propagating)
// =======================================================================

TEST(ForwardListAllocators, GetAllocator) {
  forward_list<int> fl;
  auto alloc = fl.get_allocator();
  EXPECT_TRUE((std::is_same_v<decltype(alloc), chaistl::allocator<int>>));
}

TEST(ForwardListAllocators, CopyAllocator) {
  chaistl::test::TaggedAllocator<int> alloc1(42);
  forward_list<int, chaistl::test::TaggedAllocator<int>> fl1(alloc1);
  EXPECT_EQ(fl1.get_allocator().id, 42);
}

TEST(ForwardListAllocators, CopyConstructWithAllocator) {
  forward_list<int, chaistl::test::TaggedAllocator<int>> fl1;
  fl1.push_front(1);
  fl1.push_front(2);

  chaistl::test::TaggedAllocator<int> alloc2(99);
  forward_list<int, chaistl::test::TaggedAllocator<int>> fl2(fl1, alloc2);
  EXPECT_EQ(fl2.size(), 2);
  EXPECT_EQ(fl2.get_allocator().id, 99);
}

TEST(ForwardListAllocators, MoveConstructWithAllocatorEqual) {
  forward_list<int, chaistl::test::TaggedAllocator<int>> fl1;
  fl1.push_front(1);
  fl1.push_front(2);

  forward_list<int, chaistl::test::TaggedAllocator<int>> fl2(std::move(fl1), fl1.get_allocator());
  EXPECT_EQ(fl2.size(), 2);
  EXPECT_TRUE(fl1.empty());
}

TEST(ForwardListAllocators, MoveConstructWithAllocatorUnequal) {
  forward_list<int, chaistl::test::TaggedAllocator<int>> fl1(chaistl::test::TaggedAllocator<int>(1));
  fl1.push_front(1);
  fl1.push_front(2);

  chaistl::test::TaggedAllocator<int> alloc2(99);
  forward_list<int, chaistl::test::TaggedAllocator<int>> fl2(std::move(fl1), alloc2);
  EXPECT_EQ(fl2.size(), 2);
  EXPECT_EQ(fl2.get_allocator().id, 99);
}

// =======================================================================
// PropagatingAllocator
// =======================================================================

TEST(ForwardListAllocators, PropagatingCopyAssignment) {
  using Alloc = chaistl::test::PropagatingAllocator<int>;
  forward_list<int, Alloc> fl1(Alloc{1});
  fl1.push_front(1);
  forward_list<int, Alloc> fl2(Alloc{2});
  fl2.push_front(99);

  fl2 = fl1;
  EXPECT_EQ(fl2.size(), 1);
  EXPECT_EQ(fl2.get_allocator().id, 1);  // POCMA
}

TEST(ForwardListAllocators, PropagatingMoveAssignment) {
  using Alloc = chaistl::test::PropagatingAllocator<int>;
  forward_list<int, Alloc> fl1(Alloc{1});
  fl1.push_front(42);
  forward_list<int, Alloc> fl2(Alloc{2});

  fl2 = std::move(fl1);
  EXPECT_EQ(fl2.size(), 1);
  EXPECT_EQ(fl2.get_allocator().id, 1);  // POCMA
  EXPECT_TRUE(fl1.empty());
}

TEST(ForwardListAllocators, PropagatingSwap) {
  using Alloc = chaistl::test::PropagatingAllocator<int>;
  forward_list<int, Alloc> fl1(Alloc{1});
  fl1.push_front(1);
  forward_list<int, Alloc> fl2(Alloc{2});
  fl2.push_front(2);

  fl1.swap(fl2);
  EXPECT_EQ(fl1.get_allocator().id, 2);  // POCS
  EXPECT_EQ(fl2.get_allocator().id, 1);  // POCS
  EXPECT_EQ(fl1.front(), 2);
  EXPECT_EQ(fl2.front(), 1);
}

// =======================================================================
// CompatibleTaggedAllocator (always equal, non-propagating)
// =======================================================================

TEST(ForwardListAllocators, AlwaysEqualMoveAssignment) {
  using Alloc = chaistl::test::CompatibleTaggedAllocator<int>;
  forward_list<int, Alloc> fl1(Alloc{1});
  fl1.push_front(42);
  forward_list<int, Alloc> fl2(Alloc{2});

  fl2 = std::move(fl1);
  EXPECT_EQ(fl2.size(), 1);
  EXPECT_EQ(fl2.front(), 42);
  EXPECT_TRUE(fl1.empty());
}

// =======================================================================
// Move-only element types
// =======================================================================

TEST(ForwardListAllocators, MoveOnlyElements) {
  forward_list<std::unique_ptr<int>> fl;
  fl.push_front(std::make_unique<int>(1));
  fl.push_front(std::make_unique<int>(2));
  fl.emplace_front(new int(3));
  EXPECT_EQ(fl.size(), 3);
  EXPECT_EQ(*fl.front(), 3);
  fl.pop_front();
  EXPECT_EQ(*fl.front(), 2);
}

TEST(ForwardListAllocators, MoveOnlyInsertAfter) {
  forward_list<std::unique_ptr<int>> fl;
  fl.emplace_front(new int(1));
  fl.insert_after(fl.begin(), std::make_unique<int>(2));
  EXPECT_EQ(fl.size(), 2);
  auto it = fl.begin();
  EXPECT_EQ(**it++, 1);
  EXPECT_EQ(**it++, 2);
}

// =======================================================================
// Exception safety
// =======================================================================

TEST(ForwardListAllocators, ExceptionSafetyPushFront) {
  using T = chaistl::test::ThrowOnCopy;
  T::reset();
  T::copies_before_throw = 0;

  forward_list<T> fl;
  T value(42);
  EXPECT_THROW(fl.push_front(value), std::runtime_error);
  EXPECT_TRUE(fl.empty());
  EXPECT_EQ(T::alive, 1);  // only the local value survives
}

TEST(ForwardListAllocators, ExceptionSafetyEmplaceFrontRollback) {
  using T = chaistl::test::ThrowOnDefault;
  T::reset();
  T::defaults_before_throw = 2;  // throw on 3rd default construction

  forward_list<T> fl;
  // Push front twice should succeed (element + internal).
  // Actually emplace_front does one default construction per element.
  fl.emplace_front();
  EXPECT_EQ(T::alive, 1);
  T::defaults_before_throw = 0;  //  throw on next default
  EXPECT_THROW(fl.emplace_front(), std::runtime_error);
  EXPECT_EQ(fl.size(), 1);
  EXPECT_EQ(T::alive, 1);
}

TEST(ForwardListAllocators, ExceptionSafetyResizeGrowthRollback) {
  using T = chaistl::test::ThrowOnDefault;
  T::reset();

  forward_list<T> fl;
  fl.emplace_back(1);
  fl.emplace_back(2);
  T::defaults_before_throw = 1;  // resize constructs one new node, then throws.

  EXPECT_THROW(fl.resize(5), std::runtime_error);
  EXPECT_EQ(fl.size(), 2);
  EXPECT_EQ(T::alive, 2);
}

// =======================================================================
// check_size overflow (max_size exceeded)
// =======================================================================

TEST(ForwardListAllocators, MaxSizeOverflowOnPushFront) {
  forward_list<int, chaistl::test::SmallMaxAllocator<int>> fl;
  fl.push_front(1);
  fl.push_front(1);
  fl.push_front(1);  // now at max_size == 3
  EXPECT_THROW(fl.push_front(1), std::length_error);
  EXPECT_EQ(fl.size(), 3);  // strong guarantee
}

TEST(ForwardListAllocators, MaxSizeOverflowOnEmplaceFront) {
  forward_list<int, chaistl::test::SmallMaxAllocator<int>> fl;
  fl.push_front(1);
  fl.push_front(1);
  fl.push_front(1);
  EXPECT_THROW(fl.emplace_front(1), std::length_error);
  EXPECT_EQ(fl.size(), 3);
}

TEST(ForwardListAllocators, MaxSizeOverflowOnInsertAfterCount) {
  forward_list<int, chaistl::test::SmallMaxAllocator<int>> fl;
  fl.push_front(1);
  EXPECT_THROW(fl.insert_after(fl.begin(), 3, 99), std::length_error);
  EXPECT_EQ(fl.size(), 1);  // strong guarantee
}

TEST(ForwardListAllocators, MaxSizeOverflowOnResize) {
  forward_list<int, chaistl::test::SmallMaxAllocator<int>> fl;
  EXPECT_THROW(fl.resize(4), std::length_error);
  EXPECT_TRUE(fl.empty());
}

// =======================================================================
// Exception safety — constructor exception rollback
// =======================================================================

TEST(ForwardListAllocators, ExceptionSafetyCountCtorRollback) {
  using T = chaistl::test::ThrowOnDefault;
  T::reset();
  T::defaults_before_throw = 2;  // fail on 3rd default

  EXPECT_THROW(forward_list<T>(5), std::runtime_error);
  EXPECT_EQ(T::alive, 0);  // all destructed on rollback
}

TEST(ForwardListAllocators, ExceptionSafetyCountValueCtorRollback) {
  using T = chaistl::test::ThrowOnCopy;
  T::reset();
  T::copies_before_throw = 2;  // fail on 3rd copy
  T value(42);

  EXPECT_THROW(forward_list<T>(5, value), std::runtime_error);
  EXPECT_EQ(T::alive, 1);  // only the local 'value' survives
}

TEST(ForwardListAllocators, ExceptionSafetyRangeCtorRollback) {
  using T = chaistl::test::ThrowOnCopy;
  T::reset();

  // Build a source range of ThrowOnCopy objects (not ints) so the copy
  // constructor is called during create_node(*first).
  forward_list<T> source;
  source.emplace_front(1);
  source.emplace_front(2);
  source.emplace_front(3);
  source.emplace_front(4);
  source.emplace_front(5);

  T::copies_before_throw = 2;  // fail on 3rd copy

  EXPECT_THROW((forward_list<T>(source.begin(), source.end())), std::runtime_error);
  EXPECT_EQ(T::alive, 5);  // only source objects survive
}

TEST(ForwardListAllocators, ExceptionSafetyCopyCtorRollback) {
  using T = chaistl::test::ThrowOnCopy;
  T::reset();
  T::copies_before_throw = -1;  // don't throw during setup

  forward_list<T> fl1;
  fl1.emplace_front(1);
  fl1.emplace_front(2);
  fl1.emplace_front(3);

  T::copies_before_throw = 1;  // throw on 2nd copy

  EXPECT_THROW((forward_list<T>(fl1)), std::runtime_error);
  EXPECT_EQ(fl1.size(), 3);  // source unchanged
  T::reset();
}

// =======================================================================
// Unequal-allocator move assignment (fallback to element-wise move)
// =======================================================================

TEST(ForwardListAllocators, MoveAssignWithUnequalAllocator) {
  // TaggedAllocator: POCMA=false, IAE=false, so move assignment with unequal
  // allocators falls back to element-wise move (via assign).
  using Alloc = chaistl::test::TaggedAllocator<int>;
  forward_list<int, Alloc> fl1(Alloc{1});
  fl1.push_front(1);
  fl1.push_front(2);

  forward_list<int, Alloc> fl2(Alloc{2});
  fl2.push_front(99);

  fl2 = std::move(fl1);                  // allocators are unequal
  EXPECT_EQ(fl2.size(), 2);              // elements were moved
  EXPECT_EQ(fl2.get_allocator().id, 2);  // allocator NOT changed (POCMA=false)
  // fl1 is in a valid-but-unspecified state (not necessarily empty).

  auto it = fl2.begin();
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 1);
}

// =======================================================================
// Propagating allocator copy assignment — unequal allocators
// =======================================================================

TEST(ForwardListAllocators, PropagatingCopyAssignUnequalAllocator) {
  using Alloc = chaistl::test::PropagatingAllocator<int>;
  forward_list<int, Alloc> fl1(Alloc{1});
  fl1.push_front(2);
  fl1.push_front(4);

  forward_list<int, Alloc> fl2(Alloc{2});
  fl2.push_front(3);
  fl2.push_front(6);

  fl2 = fl1;
  EXPECT_EQ(fl2.size(), 2);              // copy happened
  EXPECT_EQ(fl2.get_allocator().id, 1);  // POCMA: allocator propagated
  EXPECT_EQ(fl1.size(), 2);              // source unchanged

  auto it = fl2.begin();
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 2);
}

}  // namespace
