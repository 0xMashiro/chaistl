// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_map.hpp>

#include <cstddef>
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

using map_type = chaistl::flat_map<int,
                                   std::string,
                                   std::less<int>,
                                   chaistl::vector<int, tagged_allocator<int>>,
                                   chaistl::vector<std::string, tagged_allocator<std::string>>>;
using propagating_map_type =
    chaistl::flat_map<int,
                      std::string,
                      std::less<int>,
                      chaistl::vector<int, chaistl::test::PropagatingAllocator<int>>,
                      chaistl::vector<std::string, chaistl::test::PropagatingAllocator<std::string>>>;

TEST(FlatMapAlloc, AllocatorExtendedDefaultConstruction) {
  map_type m(tagged_allocator<int>(7));

  EXPECT_EQ(m.keys().get_allocator().id, 7);
  EXPECT_EQ(m.values().get_allocator().id, 7);
  EXPECT_TRUE(m.empty());
}

TEST(FlatMapAlloc, AllocatorExtendedIteratorConstruction) {
  std::vector<std::pair<int, std::string>> source{{2, "two"}, {1, "one"}};

  map_type m(source.begin(), source.end(), tagged_allocator<int>(3));

  EXPECT_EQ(m.keys().get_allocator().id, 3);
  EXPECT_EQ(m.values().get_allocator().id, 3);
  EXPECT_THAT(m.keys(), ElementsAre(1, 2));
}

TEST(FlatMapAlloc, AllocatorExtendedCopyConstruction) {
  map_type original({{1, "one"}, {2, "two"}}, tagged_allocator<int>(1));

  map_type copy(original, tagged_allocator<int>(9));

  EXPECT_EQ(copy.keys().get_allocator().id, 9);
  EXPECT_EQ(copy.at(2), "two");
  EXPECT_EQ(original.size(), 2uz);
}

TEST(FlatMapAlloc, AllocatorExtendedSortedUniqueContainers) {
  chaistl::vector<int, tagged_allocator<int>> keys{1, 2, 3};
  chaistl::vector<std::string, tagged_allocator<std::string>> values{"one", "two", "three"};

  map_type m(chaistl::sorted_unique, keys, values, tagged_allocator<int>(5));

  EXPECT_EQ(m.keys().get_allocator().id, 5);
  EXPECT_TRUE(m.validate());
  EXPECT_EQ(m.at(3), "three");
}

TEST(FlatMapAlloc, CopyAssignmentHonorsUnderlyingAllocatorPropagation) {
  map_type target({{9, "nine"}}, tagged_allocator<int>(1));
  map_type source({{1, "one"}, {2, "two"}}, tagged_allocator<int>(2));

  target = source;

  EXPECT_EQ(target.keys().get_allocator().id, 1);
  EXPECT_EQ(target.values().get_allocator().id, 1);
  EXPECT_THAT(target.keys(), ElementsAre(1, 2));

  propagating_map_type prop_target({{9, "nine"}}, chaistl::test::PropagatingAllocator<int>(3));
  propagating_map_type prop_source({{4, "four"}}, chaistl::test::PropagatingAllocator<int>(4));

  prop_target = prop_source;

  EXPECT_EQ(prop_target.keys().get_allocator().id, 4);
  EXPECT_EQ(prop_target.values().get_allocator().id, 4);
  EXPECT_EQ(prop_target.at(4), "four");
}

TEST(FlatMapAlloc, SwapHonorsUnderlyingAllocatorPropagation) {
  map_type lhs({{1, "one"}}, tagged_allocator<int>(1));
  map_type rhs({{2, "two"}}, tagged_allocator<int>(1));

  lhs.swap(rhs);

  EXPECT_EQ(lhs.keys().get_allocator().id, 1);
  EXPECT_EQ(lhs.values().get_allocator().id, 1);
  EXPECT_EQ(lhs.at(2), "two");

  propagating_map_type prop_lhs({{1, "one"}}, chaistl::test::PropagatingAllocator<int>(3));
  propagating_map_type prop_rhs({{2, "two"}}, chaistl::test::PropagatingAllocator<int>(4));

  prop_lhs.swap(prop_rhs);

  EXPECT_EQ(prop_lhs.keys().get_allocator().id, 4);
  EXPECT_EQ(prop_lhs.values().get_allocator().id, 4);
  EXPECT_EQ(prop_lhs.at(2), "two");
}

}  // namespace
