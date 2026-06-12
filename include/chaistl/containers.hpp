// SPDX-License-Identifier: Apache-2.0

#pragma once

// Standard-compatible container entry points.
#include <chaistl/containers/array.hpp>
#include <chaistl/containers/deque.hpp>
#include <chaistl/containers/flat_map.hpp>
#include <chaistl/containers/flat_set.hpp>
#include <chaistl/containers/forward_list.hpp>
#include <chaistl/containers/list.hpp>
#include <chaistl/containers/map.hpp>
#include <chaistl/containers/max_heap.hpp>
#include <chaistl/containers/min_heap.hpp>
#include <chaistl/containers/multimap.hpp>
#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/priority_deque.hpp>
#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/containers/queue.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/containers/span.hpp>
#include <chaistl/containers/stack.hpp>
#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/containers/unordered_multimap.hpp>
#include <chaistl/containers/unordered_multiset.hpp>
#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/containers/vector.hpp>

// Stable chaistl extensions: policy-specialized aliases over stable
// containers. These are intentionally not in chaistl::experimental.
#include <chaistl/containers/avl_map.hpp>
#include <chaistl/containers/avl_multimap.hpp>
#include <chaistl/containers/avl_multiset.hpp>
#include <chaistl/containers/avl_set.hpp>
#include <chaistl/containers/binomial_heap.hpp>
#include <chaistl/containers/d_ary_heap.hpp>
#include <chaistl/containers/fibonacci_heap.hpp>
#include <chaistl/containers/leftist_heap.hpp>
#include <chaistl/containers/order_statistic_map.hpp>
#include <chaistl/containers/order_statistic_multimap.hpp>
#include <chaistl/containers/order_statistic_multiset.hpp>
#include <chaistl/containers/order_statistic_set.hpp>
#include <chaistl/containers/pairing_heap.hpp>
#include <chaistl/containers/power2_unordered_map.hpp>
#include <chaistl/containers/power2_unordered_multimap.hpp>
#include <chaistl/containers/power2_unordered_multiset.hpp>
#include <chaistl/containers/power2_unordered_set.hpp>
#include <chaistl/containers/size_balanced_map.hpp>
#include <chaistl/containers/size_balanced_multimap.hpp>
#include <chaistl/containers/size_balanced_multiset.hpp>
#include <chaistl/containers/size_balanced_set.hpp>
#include <chaistl/containers/skew_heap.hpp>
#include <chaistl/containers/skip_list_multiset.hpp>
#include <chaistl/containers/skip_list_set.hpp>
#include <chaistl/containers/splay_map.hpp>
#include <chaistl/containers/splay_multimap.hpp>
#include <chaistl/containers/splay_multiset.hpp>
#include <chaistl/containers/splay_set.hpp>
#include <chaistl/containers/treap_map.hpp>
#include <chaistl/containers/treap_multimap.hpp>
#include <chaistl/containers/treap_multiset.hpp>
#include <chaistl/containers/treap_set.hpp>
#include <chaistl/containers/weight_balanced_map.hpp>
#include <chaistl/containers/weight_balanced_multimap.hpp>
#include <chaistl/containers/weight_balanced_multiset.hpp>
#include <chaistl/containers/weight_balanced_set.hpp>

// Experimental containers keep their names under chaistl::experimental.
#include <chaistl/experimental/containers.hpp>
