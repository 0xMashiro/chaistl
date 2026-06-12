// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_multimap.hpp>

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../sequence/support.hpp"

namespace {

using ::testing::ElementsAre;

template <class T>
struct tagged_allocator {
  using value_type = T;

  int id = 0;

  tagged_allocator() = default;
  explicit tagged_allocator(int identifier) : id(identifier) {}
  template <class U>
  tagged_allocator(const tagged_allocator<U>& other) : id(other.id) {}

  T* allocate(std::size_t n) { return std::allocator<T>{}.allocate(n); }
  void deallocate(T* ptr, std::size_t n) { std::allocator<T>{}.deallocate(ptr, n); }

  friend bool operator==(const tagged_allocator&, const tagged_allocator&) = default;
};

using map_type = chaistl::flat_multimap<int,
                                        std::string,
                                        std::less<int>,
                                        chaistl::vector<int, tagged_allocator<int>>,
                                        chaistl::vector<std::string, tagged_allocator<std::string>>>;
using propagating_map_type =
    chaistl::flat_multimap<int,
                           std::string,
                           std::less<int>,
                           chaistl::vector<int, chaistl::test::PropagatingAllocator<int>>,
                           chaistl::vector<std::string, chaistl::test::PropagatingAllocator<std::string>>>;

TEST(FlatMultimapAlloc, AllocatorExtendedDefaultConstruction) {
  map_type m(tagged_allocator<int>(7));

  EXPECT_EQ(m.keys().get_allocator().id, 7);
  EXPECT_EQ(m.values().get_allocator().id, 7);
  EXPECT_TRUE(m.empty());
}

TEST(FlatMultimapAlloc, AllocatorExtendedIteratorConstruction) {
  std::vector<std::pair<int, std::string>> source{{2, "a"}, {1, "one"}, {2, "b"}};

  map_type m(source.begin(), source.end(), tagged_allocator<int>(3));

  EXPECT_EQ(m.keys().get_allocator().id, 3);
  EXPECT_EQ(m.values().get_allocator().id, 3);
  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 2));
  EXPECT_THAT(m.values(), ElementsAre("one", "a", "b"));
  EXPECT_EQ(m.count(2), 2uz);
  auto [first, last] = m.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 2);
}

TEST(FlatMultimapAlloc, AllocatorExtendedCopyConstruction) {
  map_type original({{1, "one"}, {2, "a"}, {2, "b"}}, tagged_allocator<int>(1));

  map_type copy(original, tagged_allocator<int>(9));

  EXPECT_EQ(copy.keys().get_allocator().id, 9);
  const auto found = copy.find(2);
  ASSERT_NE(found, copy.end());
  EXPECT_EQ(found->second, "a");
  EXPECT_EQ(copy.count(2), 2uz);
  EXPECT_EQ(original.size(), 3uz);
}

TEST(FlatMultimapAlloc, AllocatorExtendedSortedEquivalentContainers) {
  chaistl::vector<int, tagged_allocator<int>> keys{1, 2, 2, 3};
  chaistl::vector<std::string, tagged_allocator<std::string>> values{"one", "a", "b", "three"};

  map_type m(chaistl::sorted_equivalent, keys, values, tagged_allocator<int>(5));

  EXPECT_EQ(m.keys().get_allocator().id, 5);
  EXPECT_TRUE(m.validate());
  auto [first, last] = m.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 2);
  EXPECT_EQ(first->second, "a");
}

TEST(FlatMultimapAlloc, BatchInsertKeepsExistingEquivalentKeysFirst) {
  map_type m({{1, "one"}, {2, "old-a"}, {2, "old-b"}, {5, "five"}}, tagged_allocator<int>(4));
  const std::vector<std::pair<int, std::string>> incoming{{4, "four"}, {2, "new-a"}, {3, "three"}, {2, "new-b"}};

  m.insert(incoming.begin(), incoming.end());

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 2, 2, 2, 3, 4, 5));
  EXPECT_THAT(m.values(), ElementsAre("one", "old-a", "old-b", "new-a", "new-b", "three", "four", "five"));
  EXPECT_EQ(m.count(2), 4uz);
  auto [first, last] = m.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 4);
  EXPECT_EQ(first->second, "old-a");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMultimapAlloc, CopyAssignmentHonorsUnderlyingAllocatorPropagation) {
  map_type target({{9, "nine"}}, tagged_allocator<int>(1));
  map_type source({{1, "one"}, {1, "uno"}}, tagged_allocator<int>(2));

  target = source;

  EXPECT_EQ(target.keys().get_allocator().id, 1);
  EXPECT_EQ(target.values().get_allocator().id, 1);
  EXPECT_THAT(target.keys(), ElementsAre(1, 1));

  propagating_map_type prop_target({{9, "nine"}}, chaistl::test::PropagatingAllocator<int>(3));
  propagating_map_type prop_source({{4, "four"}, {4, "quatre"}}, chaistl::test::PropagatingAllocator<int>(4));

  prop_target = prop_source;

  EXPECT_EQ(prop_target.keys().get_allocator().id, 4);
  EXPECT_EQ(prop_target.values().get_allocator().id, 4);
  EXPECT_EQ(prop_target.count(4), 2uz);
}

TEST(FlatMultimapAlloc, SwapHonorsUnderlyingAllocatorPropagation) {
  map_type lhs({{1, "one"}}, tagged_allocator<int>(1));
  map_type rhs({{2, "two"}, {2, "deux"}}, tagged_allocator<int>(1));

  lhs.swap(rhs);

  EXPECT_EQ(lhs.keys().get_allocator().id, 1);
  EXPECT_EQ(lhs.values().get_allocator().id, 1);
  EXPECT_EQ(lhs.count(2), 2uz);

  propagating_map_type prop_lhs({{1, "one"}}, chaistl::test::PropagatingAllocator<int>(3));
  propagating_map_type prop_rhs({{2, "two"}, {2, "deux"}}, chaistl::test::PropagatingAllocator<int>(4));

  prop_lhs.swap(prop_rhs);

  EXPECT_EQ(prop_lhs.keys().get_allocator().id, 4);
  EXPECT_EQ(prop_lhs.values().get_allocator().id, 4);
  EXPECT_EQ(prop_lhs.count(2), 2uz);
}

}  // namespace
