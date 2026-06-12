// SPDX-License-Identifier: Apache-2.0

// References:
// - std::priority_queue:
//   https://en.cppreference.com/w/cpp/container/priority_queue
//   https://eel.is/c++draft/priority.queue
// - pb_ds priority_queue (point iterators, modify, join):
//   https://gcc.gnu.org/onlinedocs/libstdc++/manual/policy_data_structures.html
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/binomial_heap.hpp>
#include <chaistl/containers/d_ary_heap.hpp>
#include <chaistl/containers/fibonacci_heap.hpp>
#include <chaistl/containers/heap/policy/binary_heap.hpp>
#include <chaistl/containers/heap/policy/d_ary_heap.hpp>
#include <chaistl/containers/heap/policy/fibonacci_heap.hpp>
#include <chaistl/containers/leftist_heap.hpp>
#include <chaistl/containers/max_heap.hpp>
#include <chaistl/containers/min_heap.hpp>
#include <chaistl/containers/pairing_heap.hpp>
#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/containers/skew_heap.hpp>

#include <concepts>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using chaistl::heap_policy::binary_heap_policy;
using chaistl::heap_policy::binomial_heap_policy;
using chaistl::heap_policy::d_ary_heap_policy;
using chaistl::heap_policy::fibonacci_heap_policy;
using chaistl::heap_policy::leftist_heap_policy;
using chaistl::heap_policy::pairing_heap_policy;
using chaistl::heap_policy::skew_heap_policy;

struct MoveOnly {
  int value;

  explicit MoveOnly(int v) : value(v) {}
  MoveOnly(MoveOnly&&) = default;
  MoveOnly& operator=(MoveOnly&&) = default;
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;

  [[maybe_unused]] friend constexpr bool operator==(const MoveOnly&, const MoveOnly&) = default;
  [[maybe_unused]] friend constexpr auto operator<=>(const MoveOnly&, const MoveOnly&) = default;
};

// Stateful comparator: a default-constructed Flip orders the opposite way,
// so any constructor that "loses" the comparator inverts the queue — the
// original prototype's allocator-extended constructors did exactly that.
struct Flip {
  bool min = false;
  constexpr bool operator()(int a, int b) const noexcept { return min ? a > b : a < b; }
};

// Minimal stateful allocator: equality by id, no propagation. Forces the
// allocator-extended move constructor down both the take-storage (equal) and the
// rebuild (unequal) paths.
template <class T>
struct IdAlloc {
  using value_type = T;
  int id = 0;

  IdAlloc() = default;
  explicit IdAlloc(int i) : id(i) {}
  template <class U>
  IdAlloc(const IdAlloc<U>& other) : id(other.id) {}

  T* allocate(std::size_t n) { return std::allocator<T>().allocate(n); }
  void deallocate(T* p, std::size_t n) { std::allocator<T>().deallocate(p, n); }

  friend bool operator==(const IdAlloc&, const IdAlloc&) = default;
};

// ============================================================================
// Capability surface — the policy concept ladder, checked at compile time
// ============================================================================

template <class PQ>
concept has_erase = requires(PQ pq, typename PQ::point_iterator it) { pq.erase(it); };
template <class PQ>
concept has_modify = requires(PQ pq, typename PQ::point_iterator it, typename PQ::value_type v) { pq.modify(it, v); };
template <class PQ>
concept has_join = requires(PQ pq, PQ other) { pq.join(other); };

using BinaryQ = chaistl::priority_queue<int>;
using DaryQ = chaistl::d_ary_heap<int, 4>;
using MaxHeapQ = chaistl::max_heap<int>;
using MinHeapQ = chaistl::min_heap<int>;
using PairingQ = chaistl::pairing_heap<int>;
using BinomialQ = chaistl::binomial_heap<int>;
using SkewQ = chaistl::skew_heap<int>;
using LeftistQ = chaistl::leftist_heap<int>;
using FibonacciQ = chaistl::fibonacci_heap<int>;

static_assert(chaistl::heap_policy::array_heap_policy<binary_heap_policy, int, std::less<int>>);
static_assert(chaistl::heap_policy::array_heap_policy<d_ary_heap_policy<4>, int, std::less<int>>);
static_assert(chaistl::heap_policy::node_heap_policy<pairing_heap_policy, int, std::less<int>>);
static_assert(chaistl::heap_policy::node_heap_policy<binomial_heap_policy, int, std::less<int>>);
static_assert(chaistl::heap_policy::node_heap_policy<skew_heap_policy, int, std::less<int>>);
static_assert(chaistl::heap_policy::node_heap_policy<leftist_heap_policy, int, std::less<int>>);
static_assert(chaistl::heap_policy::node_heap_policy<fibonacci_heap_policy, int, std::less<int>>);
static_assert(chaistl::heap_policy::mutable_node_heap_policy<pairing_heap_policy, int, std::less<int>>);
static_assert(chaistl::heap_policy::mutable_node_heap_policy<binomial_heap_policy, int, std::less<int>>);
static_assert(!chaistl::heap_policy::mutable_node_heap_policy<skew_heap_policy, int, std::less<int>>);
static_assert(!chaistl::heap_policy::mutable_node_heap_policy<leftist_heap_policy, int, std::less<int>>);
static_assert(!chaistl::heap_policy::mutable_node_heap_policy<fibonacci_heap_policy, int, std::less<int>>);

static_assert(BinaryQ::uses_array_storage && !BinaryQ::has_stable_handles);
static_assert(DaryQ::uses_array_storage && !DaryQ::has_stable_handles);
static_assert(MaxHeapQ::uses_array_storage && !MaxHeapQ::has_stable_handles);
static_assert(MinHeapQ::uses_array_storage && !MinHeapQ::has_stable_handles);
static_assert(!PairingQ::uses_array_storage && PairingQ::has_stable_handles);
static_assert(!BinomialQ::uses_array_storage && BinomialQ::has_stable_handles);
static_assert(!SkewQ::uses_array_storage && !SkewQ::has_stable_handles);
static_assert(!LeftistQ::uses_array_storage && !LeftistQ::has_stable_handles);
static_assert(!FibonacciQ::uses_array_storage && !FibonacciQ::has_stable_handles);

// erase/modify exist exactly for the handle-capable policies; join for both
// node policies. Unsupported operations are absent, not broken.
static_assert(has_erase<PairingQ> && has_modify<PairingQ>);
static_assert(has_erase<BinomialQ> && has_modify<BinomialQ>);
static_assert(!has_erase<DaryQ> && !has_modify<DaryQ>);
static_assert(!has_erase<SkewQ> && !has_modify<SkewQ>);
static_assert(!has_erase<LeftistQ> && !has_modify<LeftistQ>);
static_assert(!has_erase<FibonacciQ> && !has_modify<FibonacciQ>);
static_assert(!has_erase<BinaryQ> && !has_modify<BinaryQ>);
static_assert(has_join<PairingQ> && has_join<BinomialQ> && has_join<SkewQ> && has_join<LeftistQ> &&
              has_join<FibonacciQ> && !has_join<BinaryQ> && !has_join<DaryQ>);

// push returns a stable handle only where the policy can keep it stable.
static_assert(std::is_void_v<decltype(std::declval<BinaryQ&>().push(1))>);
static_assert(std::is_void_v<decltype(std::declval<DaryQ&>().push(1))>);
static_assert(std::is_void_v<decltype(std::declval<MaxHeapQ&>().push(1))>);
static_assert(std::is_void_v<decltype(std::declval<MinHeapQ&>().push(1))>);
static_assert(std::is_void_v<decltype(std::declval<SkewQ&>().push(1))>);
static_assert(std::is_void_v<decltype(std::declval<LeftistQ&>().push(1))>);
static_assert(std::is_void_v<decltype(std::declval<FibonacciQ&>().push(1))>);
static_assert(std::is_same_v<decltype(std::declval<BinomialQ&>().push(1)), BinomialQ::point_iterator>);
static_assert(std::is_same_v<decltype(std::declval<PairingQ&>().push(1)), PairingQ::point_iterator>);

// Handles never expose mutable references: values are modified only through
// modify(), which re-establishes heap order.
static_assert(std::is_same_v<decltype(*std::declval<PairingQ::point_iterator>()), const int&>);
static_assert(std::is_same_v<decltype(*std::declval<PairingQ::point_const_iterator>()), const int&>);
static_assert(std::is_convertible_v<PairingQ::point_iterator, PairingQ::point_const_iterator>);
static_assert(!std::is_convertible_v<PairingQ::point_const_iterator, PairingQ::point_iterator>);

// ============================================================================
// Compile-time execution — the whole heap layer is constexpr
// ============================================================================

template <class Policy>
constexpr bool compile_time_smoke() {
  chaistl::priority_queue<int, std::less<int>, chaistl::allocator<int>, Policy> pq;
  for (int v : {3, 1, 4, 1, 5, 9, 2, 6}) {
    pq.push(v);
  }
  if (pq.size() != 8 || !pq.verify()) {
    return false;
  }
  int prev = pq.top();
  while (!pq.empty()) {
    if (pq.top() > prev) {
      return false;
    }
    prev = pq.top();
    pq.pop();
  }
  return true;
}

template <class Policy>
constexpr bool compile_time_handles() {
  chaistl::priority_queue<int, std::less<int>, chaistl::allocator<int>, Policy> pq;
  pq.push(5);
  auto h = pq.push(2);
  pq.push(8);
  pq.modify(h, 11);
  if (pq.top() != 11) {
    return false;
  }
  pq.erase(h);
  return pq.size() == 2 && pq.top() == 8 && pq.verify();
}

static_assert(compile_time_smoke<binary_heap_policy>());
static_assert(compile_time_smoke<d_ary_heap_policy<4>>());
static_assert(compile_time_smoke<pairing_heap_policy>());
static_assert(compile_time_smoke<binomial_heap_policy>());
static_assert(compile_time_smoke<skew_heap_policy>());
static_assert(compile_time_smoke<leftist_heap_policy>());
static_assert(compile_time_smoke<fibonacci_heap_policy>());
static_assert(compile_time_handles<pairing_heap_policy>());
static_assert(compile_time_handles<binomial_heap_policy>());

// ============================================================================
// Shared interface, one suite over all three policies
// ============================================================================

template <class Policy>
class HeapPolicyTest : public ::testing::Test {
 protected:
  template <class T, class Compare = std::less<T>>
  using queue_t = chaistl::priority_queue<T, Compare, chaistl::allocator<T>, Policy>;
};

using PolicyTypes = ::testing::Types<binary_heap_policy,
                                     d_ary_heap_policy<4>,
                                     pairing_heap_policy,
                                     binomial_heap_policy,
                                     skew_heap_policy,
                                     leftist_heap_policy,
                                     fibonacci_heap_policy>;
TYPED_TEST_SUITE(HeapPolicyTest, PolicyTypes);

TYPED_TEST(HeapPolicyTest, DefaultConstructionIsEmpty) {
  typename TestFixture::template queue_t<int> pq;
  EXPECT_TRUE(pq.empty());
  EXPECT_EQ(pq.size(), 0uz);
  EXPECT_TRUE(pq.verify());
}

TYPED_TEST(HeapPolicyTest, PushPopSortsDescending) {
  typename TestFixture::template queue_t<int> pq;
  for (int v : {3, 1, 4, 1, 5, 9, 2, 6}) {
    pq.push(v);
    ASSERT_TRUE(pq.verify());
  }
  std::vector<int> drained;
  while (!pq.empty()) {
    drained.push_back(pq.top());
    pq.pop();
    ASSERT_TRUE(pq.verify());
  }
  EXPECT_EQ(drained, (std::vector<int>{9, 6, 5, 4, 3, 2, 1, 1}));
}

TYPED_TEST(HeapPolicyTest, GreaterComparatorMakesMinHeap) {
  typename TestFixture::template queue_t<int, std::greater<int>> pq;
  for (int v : {3, 1, 4, 1, 5}) {
    pq.push(v);
  }
  EXPECT_EQ(pq.top(), 1);
  pq.pop();
  pq.pop();
  EXPECT_EQ(pq.top(), 3);
  EXPECT_TRUE(pq.verify());
}

TEST(HeapAliasTest, MinAndMaxHeapNameComparatorDirection) {
  chaistl::max_heap<int> max_heap;
  chaistl::min_heap<int> min_heap;
  for (int v : {3, 1, 4, 1, 5}) {
    max_heap.push(v);
    min_heap.push(v);
  }
  EXPECT_EQ(max_heap.top(), 5);
  EXPECT_EQ(min_heap.top(), 1);
}

TYPED_TEST(HeapPolicyTest, IteratorRangeAndInitializerListConstruction) {
  std::vector<int> values{3, 1, 4, 1, 5, 9, 2, 6};
  typename TestFixture::template queue_t<int> from_range(values.begin(), values.end());
  typename TestFixture::template queue_t<int> from_list{3, 1, 4, 1, 5, 9, 2, 6};
  EXPECT_EQ(from_range.size(), 8uz);
  EXPECT_TRUE(from_range.verify());
  while (!from_range.empty()) {
    ASSERT_EQ(from_range.top(), from_list.top());
    from_range.pop();
    from_list.pop();
  }
  EXPECT_TRUE(from_list.empty());
}

TYPED_TEST(HeapPolicyTest, FromRangeConstructionAndPushRange) {
  std::vector<int> values{4, 8, 2};
  typename TestFixture::template queue_t<int> pq(std::from_range, values);
  pq.push_range(std::vector<int>{9, 1});
  EXPECT_EQ(pq.size(), 5uz);
  EXPECT_TRUE(pq.verify());
  EXPECT_EQ(pq.top(), 9);
}

TYPED_TEST(HeapPolicyTest, EmplaceConstructsInPlace) {
  typename TestFixture::template queue_t<std::pair<int, std::string>> pq;
  pq.emplace(1, "one");
  pq.emplace(3, "three");
  pq.emplace(2, "two");
  EXPECT_EQ(pq.top().first, 3);
  EXPECT_EQ(pq.top().second, "three");
  EXPECT_TRUE(pq.verify());
}

TYPED_TEST(HeapPolicyTest, MoveOnlyElements) {
  typename TestFixture::template queue_t<MoveOnly> pq;
  pq.push(MoveOnly{10});
  pq.push(MoveOnly{30});
  pq.emplace(20);
  EXPECT_EQ(pq.top().value, 30);
  pq.pop();  // binomial pop builds no dummy node: T need not be default-constructible
  EXPECT_EQ(pq.top().value, 20);
  pq.pop();
  EXPECT_EQ(pq.top().value, 10);
  EXPECT_TRUE(pq.verify());
}

TYPED_TEST(HeapPolicyTest, CopyIsDeepAndPreservesEveryElement) {
  using PQ = typename TestFixture::template queue_t<int>;
  // Three elements put a binomial forest into its [B0, B1] two-tree shape —
  // the shape whose second tree the original prototype's copy and destructor
  // silently dropped.
  PQ original{1, 2, 3};
  PQ copy(original);
  EXPECT_EQ(copy.size(), 3uz);
  EXPECT_TRUE(copy.verify());
  copy.pop();
  EXPECT_EQ(original.size(), 3uz);  // deep: original untouched
  EXPECT_EQ(original.top(), 3);
  std::vector<int> rest;
  while (!copy.empty()) {
    rest.push_back(copy.top());
    copy.pop();
  }
  EXPECT_EQ(rest, (std::vector<int>{2, 1}));
}

TYPED_TEST(HeapPolicyTest, MultiTreeForestDestructsCleanly) {
  // Regression: scope-drop a 3-element heap without draining. Under the
  // ASan presets this fails if the destructor only walks the first tree.
  typename TestFixture::template queue_t<int> pq{1, 2, 3};
  EXPECT_EQ(pq.size(), 3uz);
}

TYPED_TEST(HeapPolicyTest, MoveConstructionAndAssignment) {
  using PQ = typename TestFixture::template queue_t<int>;
  PQ source{5, 1, 3};
  PQ moved(std::move(source));
  EXPECT_EQ(moved.size(), 3uz);
  EXPECT_EQ(moved.top(), 5);

  PQ assigned;
  assigned.push(42);
  assigned = std::move(moved);
  EXPECT_EQ(assigned.size(), 3uz);
  EXPECT_EQ(assigned.top(), 5);
  EXPECT_TRUE(assigned.verify());
}

TYPED_TEST(HeapPolicyTest, CopyAssignment) {
  using PQ = typename TestFixture::template queue_t<int>;
  PQ a{7, 2};
  PQ b{3, 5, 1};
  a = b;
  EXPECT_EQ(a.size(), 3uz);
  EXPECT_EQ(a.top(), 5);
  EXPECT_EQ(b.size(), 3uz);
  EXPECT_TRUE(a.verify());
}

TYPED_TEST(HeapPolicyTest, SwapMemberAndNonMember) {
  using PQ = typename TestFixture::template queue_t<int>;
  PQ a{1, 2};
  PQ b{10, 20, 30};
  a.swap(b);
  EXPECT_EQ(a.size(), 3uz);
  EXPECT_EQ(a.top(), 30);
  EXPECT_EQ(b.top(), 2);
  using std::swap;
  swap(a, b);
  EXPECT_EQ(a.top(), 2);
  EXPECT_EQ(b.top(), 30);
}

TYPED_TEST(HeapPolicyTest, AllocatorExtendedCopyKeepsStatefulComparator) {
  // Regression: the prototype's node backend default-constructed compare_ in
  // the allocator-extended constructors, silently flipping a min-heap into a
  // max-heap.
  using PQ = chaistl::priority_queue<int, Flip, chaistl::allocator<int>, TypeParam>;
  PQ a(Flip{.min = true});
  a.push(1);
  a.push(2);
  a.push(3);
  ASSERT_EQ(a.top(), 1);

  PQ b(a, chaistl::allocator<int>{});
  EXPECT_EQ(b.top(), 1);
  EXPECT_TRUE(b.verify());

  PQ c(std::move(a), chaistl::allocator<int>{});
  EXPECT_EQ(c.top(), 1);
  EXPECT_EQ(c.size(), 3uz);
  EXPECT_TRUE(c.verify());
}

TYPED_TEST(HeapPolicyTest, AllocatorExtendedMoveWithUnequalAllocatorRebuilds) {
  // Regression: this constructor did not even compile for node backends in
  // the prototype (it range-for'd over a type with no begin()).
  using PQ = chaistl::priority_queue<int, std::less<int>, IdAlloc<int>, TypeParam>;
  PQ a{IdAlloc<int>{1}};
  for (int v : {4, 9, 6}) {
    a.push(v);
  }

  PQ same_alloc(std::move(a), IdAlloc<int>{1});  // take-storage path
  EXPECT_EQ(same_alloc.size(), 3uz);
  EXPECT_EQ(same_alloc.top(), 9);

  PQ other_alloc(std::move(same_alloc), IdAlloc<int>{2});  // rebuild path
  EXPECT_EQ(other_alloc.size(), 3uz);
  EXPECT_EQ(other_alloc.top(), 9);
  EXPECT_TRUE(other_alloc.verify());
}

TYPED_TEST(HeapPolicyTest, DeepAscendingChainDestroysAndClonesIteratively) {
  // 200k ascending pushes degenerate a pairing heap into a depth-200k chain.
  // Destruction, structural clone, and clear are all iterative; a recursive
  // implementation overflows the stack here (the prototype segfaulted).
  using PQ = typename TestFixture::template queue_t<int>;
  constexpr int n = 200'000;
  PQ pq;
  for (int i = 0; i < n; ++i) {
    pq.push(i);
  }
  PQ copy(pq);  // iterative structural clone of the chain
  EXPECT_EQ(copy.size(), static_cast<std::size_t>(n));
  EXPECT_EQ(copy.top(), n - 1);
  // Both destructors run at scope exit over the full chain.
}

TYPED_TEST(HeapPolicyTest, GetAllocatorAndValueComp) {
  using PQ = chaistl::priority_queue<int, Flip, IdAlloc<int>, TypeParam>;
  PQ pq(Flip{.min = true}, IdAlloc<int>{7});
  EXPECT_EQ(pq.get_allocator().id, 7);
  EXPECT_TRUE(pq.value_comp().min);
}

#ifndef NDEBUG
TYPED_TEST(HeapPolicyTest, HardenedPreconditionsAbort) {
  typename TestFixture::template queue_t<int> pq;
  EXPECT_DEATH((void)pq.top(), "");
  EXPECT_DEATH(pq.pop(), "");
}
#endif

// ============================================================================
// Node-policy operations: join
// ============================================================================

TYPED_TEST(HeapPolicyTest, JoinMovesEveryElement) {
  using PQ = typename TestFixture::template queue_t<int>;
  if constexpr (PQ::uses_array_storage) {
    GTEST_SKIP() << "join() requires a node policy";
  } else {
    PQ a{1, 3};
    PQ b{5, 2};
    a.join(b);
    EXPECT_EQ(a.size(), 4uz);
    EXPECT_TRUE(b.empty());
    EXPECT_TRUE(a.verify());
    std::vector<int> drained;
    while (!a.empty()) {
      drained.push_back(a.top());
      a.pop();
    }
    EXPECT_EQ(drained, (std::vector<int>{5, 3, 2, 1}));

    PQ c{8};
    PQ empty;
    c.join(empty);  // empty rhs
    empty.join(c);  // empty lhs absorbs
    EXPECT_EQ(empty.size(), 1uz);
    EXPECT_TRUE(c.empty());
  }
}

// ============================================================================
// Handle (point iterator) operations — every mutable policy
// ============================================================================

template <class Policy>
class MutableHeapTest : public ::testing::Test {
 protected:
  template <class T, class Compare = std::less<T>>
  using queue_t = chaistl::priority_queue<T, Compare, chaistl::allocator<T>, Policy>;
};

using MutablePolicyTypes = ::testing::Types<pairing_heap_policy, binomial_heap_policy>;
TYPED_TEST_SUITE(MutableHeapTest, MutablePolicyTypes);

TYPED_TEST(MutableHeapTest, PushReturnsStableHandle) {
  typename TestFixture::template queue_t<int> pq;
  pq.push(100);
  auto h = pq.push(42);
  for (int i = 0; i < 50; ++i) {
    pq.push(i);
  }
  EXPECT_EQ(*h, 42);
  pq.pop();  // removes 100
  EXPECT_EQ(*h, 42);
  EXPECT_TRUE(pq.verify());
}

TYPED_TEST(MutableHeapTest, ModifyIncreaseAndDecrease) {
  typename TestFixture::template queue_t<int> pq;
  pq.push(5);
  pq.push(10);
  auto h = pq.push(3);

  pq.modify(h, 15);  // increase to new maximum
  EXPECT_EQ(pq.top(), 15);
  EXPECT_EQ(*h, 15);
  ASSERT_TRUE(pq.verify());

  pq.modify(h, 1);  // decrease below everything
  EXPECT_EQ(pq.top(), 10);
  EXPECT_EQ(*h, 1);
  ASSERT_TRUE(pq.verify());

  pq.pop();
  pq.pop();
  EXPECT_EQ(pq.top(), 1);  // the modified element survived with its handle
  EXPECT_EQ(pq.size(), 1uz);
}

TYPED_TEST(MutableHeapTest, EraseInteriorNodeKeepsEverythingElse) {
  // Regression for the prototype's two flagship memory bugs: pairing erase
  // dropped the erased node's sibling list (LeakSanitizer-visible in the old
  // suite), and binomial erase unlinked a *different* node than it freed
  // (use-after-free). The erased element here is interior for both shapes.
  typename TestFixture::template queue_t<int> pq;
  pq.push(10);
  pq.push(1);
  auto h = pq.push(2);
  pq.push(3);

  pq.erase(h);
  EXPECT_EQ(pq.size(), 3uz);
  ASSERT_TRUE(pq.verify());

  std::vector<int> drained;
  while (!pq.empty()) {
    drained.push_back(pq.top());
    pq.pop();
  }
  EXPECT_EQ(drained, (std::vector<int>{10, 3, 1}));
}

TYPED_TEST(MutableHeapTest, EraseDeepLeafBubblesStructurally) {
  // Ascending pushes leave the first element at the bottom: a depth-n leaf
  // in a pairing chain, the deepest leaf of the largest tree in a binomial
  // forest. Erasing it exercises the full bubble path; verify() then checks
  // shape (binomial degree slots included), links, and order.
  typename TestFixture::template queue_t<int> pq;
  auto h = pq.push(0);
  for (int v = 1; v <= 64; ++v) {
    pq.push(v);
  }
  pq.erase(h);
  EXPECT_EQ(pq.size(), 64uz);
  ASSERT_TRUE(pq.verify());
  for (int expected = 64; expected >= 1; --expected) {
    ASSERT_EQ(pq.top(), expected);
    pq.pop();
  }
  EXPECT_TRUE(pq.empty());
}

TYPED_TEST(MutableHeapTest, EraseRootAndEraseLastElement) {
  typename TestFixture::template queue_t<int> pq;
  auto h_root = pq.push(9);
  pq.push(4);
  pq.erase(h_root);  // erase the maximum through its handle
  EXPECT_EQ(pq.top(), 4);
  auto h_last = pq.push(4);
  pq.erase(h_last);
  pq.pop();
  EXPECT_TRUE(pq.empty());
  EXPECT_TRUE(pq.verify());
}

TYPED_TEST(MutableHeapTest, ModifyMoveOnlyValue) {
  typename TestFixture::template queue_t<MoveOnly> pq;
  auto h = pq.push(MoveOnly{5});
  pq.push(MoveOnly{7});
  pq.modify(h, MoveOnly{9});
  EXPECT_EQ(pq.top().value, 9);
  EXPECT_TRUE(pq.verify());
}

TYPED_TEST(MutableHeapTest, ConstHandleReadsButCannotMutate) {
  using PQ = typename TestFixture::template queue_t<int>;
  PQ pq;
  auto h = pq.push(42);
  typename PQ::point_const_iterator ch = h;  // converting ctor
  EXPECT_EQ(*ch, 42);
  EXPECT_EQ(ch, typename PQ::point_const_iterator(h));
}

#ifndef NDEBUG
TYPED_TEST(MutableHeapTest, NullHandleAborts) {
  using PQ = typename TestFixture::template queue_t<int>;
  PQ pq;
  pq.push(1);
  EXPECT_DEATH(pq.erase(typename PQ::point_iterator{}), "");
  EXPECT_DEATH(pq.modify(typename PQ::point_iterator{}, 2), "");
}

TYPED_TEST(MutableHeapTest, SelfJoinAborts) {
  using PQ = typename TestFixture::template queue_t<int>;
  PQ pq;
  pq.push(1);
  EXPECT_DEATH(pq.join(pq), "");
}
#endif

// ============================================================================
// CTAD
// ============================================================================

TEST(PriorityQueueDeductionTest, GuidesMatchStdConventions) {
  std::vector<int> v{1, 2, 3};

  chaistl::priority_queue from_iters(v.begin(), v.end());
  static_assert(std::is_same_v<decltype(from_iters), chaistl::priority_queue<int>>);

  chaistl::priority_queue with_cmp(v.begin(), v.end(), std::greater<int>{});
  static_assert(std::is_same_v<decltype(with_cmp), chaistl::priority_queue<int, std::greater<int>>>);

  chaistl::priority_queue with_alloc(v.begin(), v.end(), chaistl::allocator<int>{});
  static_assert(std::is_same_v<decltype(with_alloc), chaistl::priority_queue<int>>);

  chaistl::priority_queue from_range(std::from_range, v);
  static_assert(std::is_same_v<decltype(from_range), chaistl::priority_queue<int>>);

  chaistl::priority_queue from_list{1, 2, 3};
  static_assert(std::is_same_v<decltype(from_list), chaistl::priority_queue<int>>);

  EXPECT_EQ(from_iters.top(), 3);
  EXPECT_EQ(with_cmp.top(), 1);
  EXPECT_EQ(from_range.top(), 3);
}

}  // namespace
