// SPDX-License-Identifier: Apache-2.0

// References:
// - std::queue:
//   https://en.cppreference.com/w/cpp/container/queue
//   https://eel.is/c++draft/queue
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers.hpp>

#include <array>
#include <compare>
#include <queue>
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

TEST(QueueTest, DefaultConstruction) {
  chaistl::queue<int> q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0uz);
}

TEST(QueueTest, ContainerConstruction) {
  chaistl::deque<int> d{1, 2, 3};
  chaistl::queue<int> q(d);

  EXPECT_EQ(q.size(), 3uz);
  EXPECT_EQ(q.front(), 1);
  EXPECT_EQ(q.back(), 3);
}

TEST(QueueTest, MoveContainerConstruction) {
  chaistl::deque<int> d{1, 2, 3};
  chaistl::queue<int> q(std::move(d));

  EXPECT_EQ(q.size(), 3uz);
  EXPECT_EQ(q.front(), 1);
  EXPECT_EQ(q.back(), 3);
}

TEST(QueueTest, CopyAndMoveOperations) {
  chaistl::queue<int> original;
  original.push(1);
  original.push(2);
  original.push(3);

  chaistl::queue<int> copied(original);
  EXPECT_EQ(copied.size(), 3uz);
  EXPECT_EQ(copied.front(), 1);

  chaistl::queue<int> moved(std::move(original));
  EXPECT_EQ(moved.size(), 3uz);
  EXPECT_EQ(moved.front(), 1);

  chaistl::queue<int> assigned;
  assigned = copied;
  EXPECT_EQ(assigned.size(), 3uz);
  EXPECT_EQ(assigned.front(), 1);

  chaistl::queue<int> move_assigned;
  move_assigned = std::move(copied);
  EXPECT_EQ(move_assigned.size(), 3uz);
  EXPECT_EQ(move_assigned.front(), 1);
}

TEST(QueueTest, InitializerListConstruction) {
  chaistl::queue<int> q{1, 2, 3};

  EXPECT_EQ(q.size(), 3uz);
  EXPECT_EQ(q.front(), 1);
  EXPECT_EQ(q.back(), 3);
}

TEST(QueueTest, IteratorConstruction) {
  std::array source{1, 2, 3};
  chaistl::queue<int> q(source.begin(), source.end());

  EXPECT_EQ(q.size(), 3uz);
  EXPECT_EQ(q.front(), 1);
  EXPECT_EQ(q.back(), 3);
}

TEST(QueueTest, PushAndPop) {
  chaistl::queue<int> q;

  q.push(1);
  q.push(2);
  q.push(3);

  EXPECT_EQ(q.size(), 3uz);
  EXPECT_EQ(q.front(), 1);
  EXPECT_EQ(q.back(), 3);

  q.pop();
  EXPECT_EQ(q.size(), 2uz);
  EXPECT_EQ(q.front(), 2);

  q.pop();
  EXPECT_EQ(q.size(), 1uz);
  EXPECT_EQ(q.front(), 3);

  q.pop();
  EXPECT_TRUE(q.empty());
}

TEST(QueueTest, PushRvalue) {
  chaistl::queue<MoveOnly> q;

  q.push(MoveOnly(1));
  q.push(MoveOnly(2));

  EXPECT_EQ(q.size(), 2uz);
  EXPECT_EQ(q.front().value, 1);
  EXPECT_EQ(q.back().value, 2);
}

TEST(QueueTest, Emplace) {
  chaistl::queue<std::string> q;

  q.emplace(3, 'a');
  EXPECT_EQ(q.front(), "aaa");

  q.emplace("hello");
  EXPECT_EQ(q.back(), "hello");
}

TEST(QueueTest, Swap) {
  chaistl::queue<int> q1{1, 2, 3};
  chaistl::queue<int> q2{4, 5};

  q1.swap(q2);

  EXPECT_EQ(q1.size(), 2uz);
  EXPECT_EQ(q1.front(), 4);

  EXPECT_EQ(q2.size(), 3uz);
  EXPECT_EQ(q2.front(), 1);
}

TEST(QueueTest, NonMemberSwap) {
  chaistl::queue<int> q1{1, 2};
  chaistl::queue<int> q2{3, 4, 5};

  swap(q1, q2);

  EXPECT_EQ(q1.size(), 3uz);
  EXPECT_EQ(q2.size(), 2uz);
}

TEST(QueueTest, ComparisonOperators) {
  chaistl::queue<int> q1{1, 2, 3};
  chaistl::queue<int> q2{1, 2, 3};
  chaistl::queue<int> q3{1, 2};
  chaistl::queue<int> q4{1, 2, 4};

  EXPECT_TRUE(q1 == q2);
  EXPECT_FALSE(q1 == q3);

  EXPECT_EQ(q3 <=> q1, std::strong_ordering::less);
  EXPECT_EQ(q4 <=> q1, std::strong_ordering::greater);
  EXPECT_EQ(q1 <=> q2, std::strong_ordering::equal);
}

TEST(QueueTest, WithDequeContainer) {
  chaistl::deque<int> d{1, 2, 3};
  chaistl::queue<int, chaistl::deque<int>> q(d);

  EXPECT_EQ(q.front(), 1);
  EXPECT_EQ(q.back(), 3);

  q.pop();
  EXPECT_EQ(q.front(), 2);
}

TEST(QueueTest, FIFOSemantics) {
  chaistl::queue<int> q;

  for (int i = 0; i < 100; ++i) {
    q.push(i);
  }

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(q.front(), i);
    q.pop();
  }

  EXPECT_TRUE(q.empty());
}

}  // namespace
