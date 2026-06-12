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

file(GLOB_RECURSE CHAISTL_BENCHMARK_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false
     "${CMAKE_CURRENT_SOURCE_DIR}/chaistl/*.cpp")

add_executable(chaistl_benchmarks)
target_sources(chaistl_benchmarks PRIVATE main.cpp ${CHAISTL_BENCHMARK_SOURCES})
target_include_directories(chaistl_benchmarks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
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
  COMMAND $<TARGET_FILE:chaistl_benchmarks> --smoke
  DEPENDS chaistl_benchmarks
  VERBATIM
  USES_TERMINAL)

set(CHAISTL_BENCHMARK_SMOKE_DOMAINS sequence algorithm hash tree_policy heap_policy flat bit_structure policy)
foreach(domain IN LISTS CHAISTL_BENCHMARK_SMOKE_DOMAINS)
  add_custom_target(
    chaistl_benchmark_${domain}_smoke
    COMMAND $<TARGET_FILE:chaistl_benchmarks> --smoke=${domain}
    DEPENDS chaistl_benchmarks
    VERBATIM
    USES_TERMINAL)
endforeach()
