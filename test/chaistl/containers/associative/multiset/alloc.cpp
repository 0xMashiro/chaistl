// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multiset.hpp>

#include <functional>

#include "../../sequence/support.hpp"

namespace {

using ::testing::ElementsAre;

TEST(MultisetAlloc, AssignmentHonorsAllocatorPropagationTraits) {
  {
    using Alloc = chaistl::test::TaggedAllocator<int>;
    using Multiset = chaistl::multiset<int, std::less<int>, Alloc>;
    Multiset target({9}, Alloc{1});
    Multiset source({2, 2, 3}, Alloc{2});

    target = source;

    EXPECT_EQ(target.get_allocator().id, 1);
    EXPECT_THAT(target, ElementsAre(2, 2, 3));
    EXPECT_TRUE(target.validate());
  }

  {
    using Alloc = chaistl::test::PropagatingAllocator<int>;
    using Multiset = chaistl::multiset<int, std::less<int>, Alloc>;
    Multiset target({9}, Alloc{3});
    Multiset source({4, 4}, Alloc{4});

    target = std::move(source);

    EXPECT_EQ(target.get_allocator().id, 4);
    EXPECT_THAT(target, ElementsAre(4, 4));
    EXPECT_TRUE(source.empty());
  }
}

TEST(MultisetAlloc, SwapHonorsAllocatorPropagationTraits) {
  {
    using Alloc = chaistl::test::CompatibleTaggedAllocator<int>;
    using Multiset = chaistl::multiset<int, std::less<int>, Alloc>;
    Multiset lhs({1, 1}, Alloc{1});
    Multiset rhs({2}, Alloc{2});

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 1);
    EXPECT_EQ(rhs.get_allocator().id, 2);
    EXPECT_THAT(lhs, ElementsAre(2));
    EXPECT_THAT(rhs, ElementsAre(1, 1));
  }

  {
    using Alloc = chaistl::test::PropagatingAllocator<int>;
    using Multiset = chaistl::multiset<int, std::less<int>, Alloc>;
    Multiset lhs({1}, Alloc{1});
    Multiset rhs({2, 2}, Alloc{2});

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 2);
    EXPECT_EQ(rhs.get_allocator().id, 1);
    EXPECT_THAT(lhs, ElementsAre(2, 2));
    EXPECT_THAT(rhs, ElementsAre(1));
  }
}

}  // namespace
