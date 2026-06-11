set(CHAISTL_BENCHMARK_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/benchmark")

if(NOT EXISTS "${CHAISTL_BENCHMARK_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
          "Google Benchmark submodule is missing. Run: git submodule update --init --recursive")
endif()

set(BENCHMARK_ENABLE_TESTING
    OFF
    CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL
    OFF
    CACHE BOOL "" FORCE)

add_subdirectory("${CHAISTL_BENCHMARK_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/third_party/benchmark")

set(CHAISTL_BENCHMARK_SEQUENCE_SMOKE_FILTER
    "^(std|chaistl)::(deque|forward_list|list|vector)<int>/default_construct$")
set(CHAISTL_BENCHMARK_TREE_POLICY_SMOKE_FILTER
    "^(set_(rb|avl|treap|splay|size_balanced|weight_balanced|skip_list|std)|multiset_skip_list)/(insert_random|lookup_hit|lookup_sequential_scan)/1024$")
set(CHAISTL_BENCHMARK_ORDER_STATISTIC_SMOKE_FILTER
    "^(order_statistic_set|set_size_balanced|set_weight_balanced|set_std)/(find_by_order|order_of_key)(_linear)?/1024$")
set(CHAISTL_BENCHMARK_HEAP_POLICY_SMOKE_FILTER
    "^heap_(std|binary|dary4|pairing|binomial|skew|leftist|fibonacci)/(push_random|push_pop_random)/1024$|^heap_(pairing|binomial|skew|leftist|fibonacci)/join_random/1024$")
set(CHAISTL_BENCHMARK_FLAT_SMOKE_FILTER
    "^(flat_set|set_rb|set_std)/(insert_random|lookup_hit)/1024$|^flat_map/(bulk_insert|single_insert)/1024$|^(flat_multiset|multiset_rb|multiset_std)/(insert_random|lookup_hit)/1024$|^flat_multimap/(bulk_insert|single_insert)/1024$")
set(CHAISTL_BENCHMARK_BIT_STRUCTURE_SMOKE_FILTER
    "^(std::bitset<4096>|chaistl::bitset<4096>)/(count_dense|bitwise_xor)$|^chaistl::dynamic_bitset/(count_dense|find_next_sparse|bitwise_xor)/1024$|^(chaistl::priority_deque|std::multiset)/(push_random|pop_minmax_random)/1024$")
set(CHAISTL_BENCHMARK_POLICY_SMOKE_FILTER
    "(${CHAISTL_BENCHMARK_TREE_POLICY_SMOKE_FILTER}|${CHAISTL_BENCHMARK_ORDER_STATISTIC_SMOKE_FILTER}|${CHAISTL_BENCHMARK_HEAP_POLICY_SMOKE_FILTER})")
set(CHAISTL_BENCHMARK_SMOKE_FILTER
    "(${CHAISTL_BENCHMARK_SEQUENCE_SMOKE_FILTER}|${CHAISTL_BENCHMARK_POLICY_SMOKE_FILTER}|${CHAISTL_BENCHMARK_FLAT_SMOKE_FILTER}|${CHAISTL_BENCHMARK_BIT_STRUCTURE_SMOKE_FILTER})")

add_executable(chaistl_benchmarks)
target_sources(
  chaistl_benchmarks
  PRIVATE main.cpp
          chaistl/comparison_reporter.cpp
          chaistl/containers/sequence/array.cpp
          chaistl/containers/sequence/deque.cpp
          chaistl/containers/sequence/forward_list.cpp
          chaistl/containers/sequence/list.cpp
          chaistl/containers/sequence/vector.cpp
          chaistl/containers/heap/policy_comparison.cpp
          chaistl/containers/tree/policy_comparison.cpp
          chaistl/containers/associative/flat_containers.cpp
          chaistl/containers/utility/bit_structures.cpp
          chaistl/memory/storage_builder_overhead.cpp
          chaistl/memory/construction_transaction_overhead.cpp
          chaistl/memory/allocator_benchmark.cpp)
target_link_libraries(chaistl_benchmarks PRIVATE chaistl::chaistl chaistl::dev_flags
                                                 benchmark::benchmark)

add_custom_target(
  chaistl_benchmark_list
  COMMAND $<TARGET_FILE:chaistl_benchmarks> --benchmark_list_tests
  DEPENDS chaistl_benchmarks
  VERBATIM
  USES_TERMINAL)

add_custom_target(
  chaistl_benchmark_smoke
  COMMAND
    $<TARGET_FILE:chaistl_benchmarks>
    "--benchmark_filter=${CHAISTL_BENCHMARK_SMOKE_FILTER}"
    --benchmark_dry_run
  DEPENDS chaistl_benchmarks
  VERBATIM
  USES_TERMINAL)

add_custom_target(
  chaistl_benchmark_sequence_smoke
  COMMAND
    $<TARGET_FILE:chaistl_benchmarks>
    "--benchmark_filter=${CHAISTL_BENCHMARK_SEQUENCE_SMOKE_FILTER}"
    --benchmark_dry_run
  DEPENDS chaistl_benchmarks
  VERBATIM
  USES_TERMINAL)

add_custom_target(
  chaistl_benchmark_policy_smoke
  COMMAND
    $<TARGET_FILE:chaistl_benchmarks>
    "--benchmark_filter=${CHAISTL_BENCHMARK_POLICY_SMOKE_FILTER}"
    --benchmark_dry_run
  DEPENDS chaistl_benchmarks
  VERBATIM
  USES_TERMINAL)

add_custom_target(
  chaistl_benchmark_heap_policy_smoke
  COMMAND
    $<TARGET_FILE:chaistl_benchmarks>
    "--benchmark_filter=${CHAISTL_BENCHMARK_HEAP_POLICY_SMOKE_FILTER}"
    --benchmark_dry_run
  DEPENDS chaistl_benchmarks
  VERBATIM
  USES_TERMINAL)

add_custom_target(
  chaistl_benchmark_flat_smoke
  COMMAND
    $<TARGET_FILE:chaistl_benchmarks>
    "--benchmark_filter=${CHAISTL_BENCHMARK_FLAT_SMOKE_FILTER}"
    --benchmark_dry_run
  DEPENDS chaistl_benchmarks
  VERBATIM
  USES_TERMINAL)

add_custom_target(
  chaistl_benchmark_bit_structure_smoke
  COMMAND
    $<TARGET_FILE:chaistl_benchmarks>
    "--benchmark_filter=${CHAISTL_BENCHMARK_BIT_STRUCTURE_SMOKE_FILTER}"
    --benchmark_dry_run
  DEPENDS chaistl_benchmarks
  VERBATIM
  USES_TERMINAL)

add_custom_target(
  chaistl_benchmark_tree_policy_smoke
  COMMAND
    $<TARGET_FILE:chaistl_benchmarks>
    "--benchmark_filter=${CHAISTL_BENCHMARK_TREE_POLICY_SMOKE_FILTER}"
    --benchmark_dry_run
  DEPENDS chaistl_benchmarks
  VERBATIM
  USES_TERMINAL)
