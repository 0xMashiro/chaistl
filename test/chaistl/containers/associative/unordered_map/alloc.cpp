// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/functional/hash.hpp>

#include <functional>
#include <string>

#include "../../sequence/support.hpp"

namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(UnorderedMapAlloc, AssignmentPreservesNonPropagatingAllocator) {
  using Alloc = chaistl::test::TaggedAllocator<std::pair<const int, std::string>>;
  using Map = chaistl::unordered_map<int, std::string, chaistl::hash<int>, std::equal_to<int>, Alloc>;

  Map copy_target({{1, "one"}}, Alloc{1});
  Map copy_source({{2, "two"}, {3, "three"}}, Alloc{2});

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 1);
  EXPECT_THAT(copy_target, UnorderedElementsAre(Pair(2, "two"), Pair(3, "three")));
  EXPECT_TRUE(copy_target.validate());

  Map move_target({{4, "four"}}, Alloc{3});
  Map move_source({{5, "five"}, {6, "six"}}, Alloc{4});

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 3);
  EXPECT_THAT(move_target, UnorderedElementsAre(Pair(5, "five"), Pair(6, "six")));
  EXPECT_TRUE(move_target.validate());
}

TEST(UnorderedMapAlloc, AssignmentPropagatesPropagatingAllocator) {
  using Alloc = chaistl::test::PropagatingAllocator<std::pair<const int, std::string>>;
  using Map = chaistl::unordered_map<int, std::string, chaistl::hash<int>, std::equal_to<int>, Alloc>;

  Map copy_target({{1, "one"}}, Alloc{1});
  Map copy_source({{2, "two"}}, Alloc{2});

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 2);
  EXPECT_THAT(copy_target, UnorderedElementsAre(Pair(2, "two")));

  Map move_target({{3, "three"}}, Alloc{3});
  Map move_source({{4, "four"}}, Alloc{4});

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 4);
  EXPECT_THAT(move_target, UnorderedElementsAre(Pair(4, "four")));
  EXPECT_TRUE(move_source.empty());
}

TEST(UnorderedMapAlloc, SwapHonorsAllocatorPropagationTraits) {
  {
    using Alloc = chaistl::test::CompatibleTaggedAllocator<std::pair<const int, std::string>>;
    using Map = chaistl::unordered_map<int, std::string, chaistl::hash<int>, std::equal_to<int>, Alloc>;
    Map lhs({{1, "one"}}, Alloc{1});
    Map rhs({{2, "two"}}, Alloc{2});

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 1);
    EXPECT_EQ(rhs.get_allocator().id, 2);
    EXPECT_THAT(lhs, UnorderedElementsAre(Pair(2, "two")));
    EXPECT_THAT(rhs, UnorderedElementsAre(Pair(1, "one")));
  }

  {
    using Alloc = chaistl::test::PropagatingAllocator<std::pair<const int, std::string>>;
    using Map = chaistl::unordered_map<int, std::string, chaistl::hash<int>, std::equal_to<int>, Alloc>;
    Map lhs({{1, "one"}}, Alloc{1});
    Map rhs({{2, "two"}}, Alloc{2});

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 2);
    EXPECT_EQ(rhs.get_allocator().id, 1);
    EXPECT_THAT(lhs, UnorderedElementsAre(Pair(2, "two")));
    EXPECT_THAT(rhs, UnorderedElementsAre(Pair(1, "one")));
  }
}

}  // namespace
