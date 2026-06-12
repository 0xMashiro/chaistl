// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multiset.hpp>
#include <chaistl/functional/hash.hpp>

#include <functional>

#include "../../sequence/support.hpp"

namespace {

using ::testing::UnorderedElementsAre;

TEST(UnorderedMultisetAlloc, AssignmentHonorsAllocatorPropagationTraits) {
  {
    using Alloc = chaistl::test::TaggedAllocator<int>;
    using Multiset = chaistl::unordered_multiset<int, chaistl::hash<int>, std::equal_to<int>, Alloc>;
    Multiset target(Alloc{1});
    target.insert(9);
    Multiset source({2, 2, 3}, Alloc{2});

    target = source;

    EXPECT_EQ(target.get_allocator().id, 1);
    EXPECT_THAT(target, UnorderedElementsAre(2, 2, 3));
    EXPECT_TRUE(target.validate());
  }

  {
    using Alloc = chaistl::test::PropagatingAllocator<int>;
    using Multiset = chaistl::unordered_multiset<int, chaistl::hash<int>, std::equal_to<int>, Alloc>;
    Multiset target(Alloc{3});
    target.insert(9);
    Multiset source({4, 4}, Alloc{4});

    target = std::move(source);

    EXPECT_EQ(target.get_allocator().id, 4);
    EXPECT_THAT(target, UnorderedElementsAre(4, 4));
    EXPECT_TRUE(source.empty());
  }
}

TEST(UnorderedMultisetAlloc, SwapHonorsAllocatorPropagationTraits) {
  {
    using Alloc = chaistl::test::CompatibleTaggedAllocator<int>;
    using Multiset = chaistl::unordered_multiset<int, chaistl::hash<int>, std::equal_to<int>, Alloc>;
    Multiset lhs({1, 1}, Alloc{1});
    Multiset rhs(Alloc{2});
    rhs.insert(2);

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 1);
    EXPECT_EQ(rhs.get_allocator().id, 2);
    EXPECT_THAT(lhs, UnorderedElementsAre(2));
    EXPECT_THAT(rhs, UnorderedElementsAre(1, 1));
  }

  {
    using Alloc = chaistl::test::PropagatingAllocator<int>;
    using Multiset = chaistl::unordered_multiset<int, chaistl::hash<int>, std::equal_to<int>, Alloc>;
    Multiset lhs(Alloc{1});
    lhs.insert(1);
    Multiset rhs({2, 2}, Alloc{2});

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 2);
    EXPECT_EQ(rhs.get_allocator().id, 1);
    EXPECT_THAT(lhs, UnorderedElementsAre(2, 2));
    EXPECT_THAT(rhs, UnorderedElementsAre(1));
  }
}

}  // namespace
