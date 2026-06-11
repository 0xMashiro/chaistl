cmake_minimum_required(VERSION 3.28)

project(chaistl_compile_fail_case LANGUAGES CXX)

set(CHAISTL_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(CHAISTL_BUILD_BENCHMARKS
    OFF
    CACHE BOOL "" FORCE)
set(CHAISTL_ENABLE_CXX26
    "${CHAISTL_COMPILE_FAIL_ENABLE_CXX26}"
    CACHE BOOL "" FORCE)

add_subdirectory("${CHAISTL_SOURCE_DIR}" chaistl EXCLUDE_FROM_ALL)

add_executable(compile_fail_case "${CASE_SOURCE}")
target_link_libraries(compile_fail_case PRIVATE chaistl::chaistl)
target_compile_features(compile_fail_case PRIVATE cxx_std_23)
