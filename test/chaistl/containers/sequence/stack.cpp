// SPDX-License-Identifier: Apache-2.0

// References:
// - std::stack:
//   https://en.cppreference.com/w/cpp/container/stack
//   https://eel.is/c++draft/stack
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers.hpp>

#include <array>
#include <compare>
#include <stack>
#include <string>
#include <type_traits>

namespace {

struct MoveOnly {
  int value;

  explicit MoveOnly(int v) : value(v) {}
  MoveOnly(MoveOnly&&) = default;
  MoveOnly& operator=(MoveOnly&&) = default;

  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;

  [[maybe_unused]] friend constexpr bool operator==(const MoveOnly& lhs, const MoveOnly& rhs) {
    return lhs.value == rhs.value;
  }
  [[maybe_unused]] friend constexpr auto operator<=>(const MoveOnly& lhs, const MoveOnly& rhs) {
    return lhs.value <=> rhs.value;
  }
};

TEST(StackTest, DefaultConstruction) {
  chaistl::stack<int> s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0uz);
}

TEST(StackTest, ContainerConstruction) {
  chaistl::deque<int> d{1, 2, 3};
  chaistl::stack<int> s(d);

  EXPECT_EQ(s.size(), 3uz);
  EXPECT_EQ(s.top(), 3);
}

TEST(StackTest, MoveContainerConstruction) {
  chaistl::deque<int> d{1, 2, 3};
  chaistl::stack<int> s(std::move(d));

  EXPECT_EQ(s.size(), 3uz);
  EXPECT_EQ(s.top(), 3);
}

TEST(StackTest, CopyAndMoveOperations) {
  chaistl::stack<int> original;
  original.push(1);
  original.push(2);
  original.push(3);

  chaistl::stack<int> copied(original);
  EXPECT_EQ(copied.size(), 3uz);
  EXPECT_EQ(copied.top(), 3);

  chaistl::stack<int> moved(std::move(original));
  EXPECT_EQ(moved.size(), 3uz);
  EXPECT_EQ(moved.top(), 3);

  chaistl::stack<int> assigned;
  assigned = copied;
  EXPECT_EQ(assigned.size(), 3uz);
  EXPECT_EQ(assigned.top(), 3);

  chaistl::stack<int> move_assigned;
  move_assigned = std::move(copied);
  EXPECT_EQ(move_assigned.size(), 3uz);
  EXPECT_EQ(move_assigned.top(), 3);
}

TEST(StackTest, InitializerListConstruction) {
  chaistl::stack<int> s{1, 2, 3};

  EXPECT_EQ(s.size(), 3uz);
  EXPECT_EQ(s.top(), 3);
}

TEST(StackTest, IteratorConstruction) {
  std::array source{1, 2, 3};
  chaistl::stack<int> s(source.begin(), source.end());

  EXPECT_EQ(s.size(), 3uz);
  EXPECT_EQ(s.top(), 3);
}

TEST(StackTest, PushAndPop) {
  chaistl::stack<int> s;

  s.push(1);
  s.push(2);
  s.push(3);

  EXPECT_EQ(s.size(), 3uz);
  EXPECT_EQ(s.top(), 3);

  s.pop();
  EXPECT_EQ(s.size(), 2uz);
  EXPECT_EQ(s.top(), 2);

  s.pop();
  EXPECT_EQ(s.size(), 1uz);
  EXPECT_EQ(s.top(), 1);

  s.pop();
  EXPECT_TRUE(s.empty());
}

TEST(StackTest, PushRvalue) {
  chaistl::stack<MoveOnly> s;

  s.push(MoveOnly(1));
  s.push(MoveOnly(2));

  EXPECT_EQ(s.size(), 2uz);
  EXPECT_EQ(s.top().value, 2);
}

TEST(StackTest, Emplace) {
  chaistl::stack<std::string> s;

  s.emplace(3, 'a');
  EXPECT_EQ(s.top(), "aaa");

  s.emplace("hello");
  EXPECT_EQ(s.top(), "hello");
}

TEST(StackTest, Swap) {
  chaistl::stack<int> s1{1, 2, 3};
  chaistl::stack<int> s2{4, 5};

  s1.swap(s2);

  EXPECT_EQ(s1.size(), 2uz);
  EXPECT_EQ(s1.top(), 5);

  EXPECT_EQ(s2.size(), 3uz);
  EXPECT_EQ(s2.top(), 3);
}

TEST(StackTest, NonMemberSwap) {
  chaistl::stack<int> s1{1, 2};
  chaistl::stack<int> s2{3, 4, 5};

  swap(s1, s2);

  EXPECT_EQ(s1.size(), 3uz);
  EXPECT_EQ(s2.size(), 2uz);
}

TEST(StackTest, ComparisonOperators) {
  chaistl::stack<int> s1{1, 2, 3};
  chaistl::stack<int> s2{1, 2, 3};
  chaistl::stack<int> s3{1, 2};
  chaistl::stack<int> s4{1, 2, 4};

  EXPECT_TRUE(s1 == s2);
  EXPECT_FALSE(s1 == s3);

  EXPECT_EQ(s3 <=> s1, std::strong_ordering::less);
  EXPECT_EQ(s4 <=> s1, std::strong_ordering::greater);
  EXPECT_EQ(s1 <=> s2, std::strong_ordering::equal);
}

TEST(StackTest, WithDequeContainer) {
  chaistl::deque<int> d{1, 2, 3};
  chaistl::stack<int, chaistl::deque<int>> s(d);

  EXPECT_EQ(s.top(), 3);

  s.pop();
  EXPECT_EQ(s.top(), 2);
}

TEST(StackTest, LIFOSemantics) {
  chaistl::stack<int> s;

  for (int i = 0; i < 100; ++i) {
    s.push(i);
  }

  for (int i = 99; i >= 0; --i) {
    EXPECT_EQ(s.top(), i);
    s.pop();
  }

  EXPECT_TRUE(s.empty());
}

}  // namespace
