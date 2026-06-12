// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multimap.hpp>

#include <functional>
#include <string>

#include "../../sequence/support.hpp"

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(MultimapAlloc, AssignmentHonorsAllocatorPropagationTraits) {
  {
    using Alloc = chaistl::test::TaggedAllocator<std::pair<const int, std::string>>;
    using Multimap = chaistl::multimap<int, std::string, std::less<int>, Alloc>;
    Multimap target({{9, "nine"}}, Alloc{1});
    Multimap source({{2, "two"}, {2, "deux"}}, Alloc{2});

    target = source;

    EXPECT_EQ(target.get_allocator().id, 1);
    EXPECT_THAT(target, ElementsAre(Pair(2, "two"), Pair(2, "deux")));
    EXPECT_TRUE(target.validate());
  }

  {
    using Alloc = chaistl::test::PropagatingAllocator<std::pair<const int, std::string>>;
    using Multimap = chaistl::multimap<int, std::string, std::less<int>, Alloc>;
    Multimap target({{9, "nine"}}, Alloc{3});
    Multimap source({{4, "four"}, {4, "quatre"}}, Alloc{4});

    target = std::move(source);

    EXPECT_EQ(target.get_allocator().id, 4);
    EXPECT_THAT(target, ElementsAre(Pair(4, "four"), Pair(4, "quatre")));
    EXPECT_TRUE(source.empty());
  }
}

TEST(MultimapAlloc, SwapHonorsAllocatorPropagationTraits) {
  {
    using Alloc = chaistl::test::CompatibleTaggedAllocator<std::pair<const int, std::string>>;
    using Multimap = chaistl::multimap<int, std::string, std::less<int>, Alloc>;
    Multimap lhs({{1, "one"}, {1, "uno"}}, Alloc{1});
    Multimap rhs({{2, "two"}}, Alloc{2});

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 1);
    EXPECT_EQ(rhs.get_allocator().id, 2);
    EXPECT_THAT(lhs, ElementsAre(Pair(2, "two")));
    EXPECT_THAT(rhs, ElementsAre(Pair(1, "one"), Pair(1, "uno")));
  }

  {
    using Alloc = chaistl::test::PropagatingAllocator<std::pair<const int, std::string>>;
    using Multimap = chaistl::multimap<int, std::string, std::less<int>, Alloc>;
    Multimap lhs({{1, "one"}}, Alloc{1});
    Multimap rhs({{2, "two"}, {2, "deux"}}, Alloc{2});

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 2);
    EXPECT_EQ(rhs.get_allocator().id, 1);
    EXPECT_THAT(lhs, ElementsAre(Pair(2, "two"), Pair(2, "deux")));
    EXPECT_THAT(rhs, ElementsAre(Pair(1, "one")));
  }
}

}  // namespace
