cmake_minimum_required(VERSION 3.28)

project(
  chaistl
  VERSION 0.1.0
  DESCRIPTION "A learning-first modern C++ STL subset"
  LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CHAISTL_BUILD_TESTS_DEFAULT "${PROJECT_IS_TOP_LEVEL}")
option(CHAISTL_BUILD_TESTS "Build chaistl tests" "${CHAISTL_BUILD_TESTS_DEFAULT}")
option(CHAISTL_BUILD_BENCHMARKS "Build chaistl benchmarks" OFF)
option(CHAISTL_BUILD_VISUALIZER "Build the experimental WebAssembly visualizer" OFF)
option(CHAISTL_ENABLE_CXX26 "Build chaistl in experimental C++26 mode" OFF)
option(CHAISTL_ENABLE_COVERAGE "Enable Clang source-based coverage instrumentation" OFF)
option(CHAISTL_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(CHAISTL_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(CHAISTL_DEV_WARNINGS "Enable developer warning flags on chaistl's own targets" ON)
option(CHAISTL_WERROR "Treat warnings as errors (used by CI)" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Compiler version check
# ------------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14")
  message(FATAL_ERROR
    "chaistl requires GCC 14+. Found GCC ${CMAKE_CXX_COMPILER_VERSION}. "
    "Use a preset (e.g. cmake --preset gcc-debug) or set CMAKE_CXX_COMPILER to g++-14.")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "22")
  message(FATAL_ERROR
    "chaistl requires Clang 22+. Found Clang ${CMAKE_CXX_COMPILER_VERSION}. "
    "Use a preset (e.g. cmake --preset clang-debug) or set CMAKE_CXX_COMPILER to clang++-22.")
endif()

add_library(chaistl INTERFACE)
add_library(chaistl::chaistl ALIAS chaistl)

file(GLOB_RECURSE CHAISTL_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false
     "${PROJECT_SOURCE_DIR}/include/chaistl/*.hpp")

target_sources(chaistl INTERFACE
               FILE_SET HEADERS
               BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
               FILES ${CHAISTL_HEADERS})
target_include_directories(
  chaistl INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(chaistl INTERFACE cxx_std_23)

if(CHAISTL_ENABLE_COVERAGE)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "CHAISTL_ENABLE_COVERAGE currently requires Clang")
  endif()

  target_compile_options(chaistl INTERFACE -fprofile-instr-generate -fcoverage-mapping)
  target_link_options(chaistl INTERFACE -fprofile-instr-generate -fcoverage-mapping)
endif()

if(CHAISTL_ENABLE_ASAN)
  target_compile_options(chaistl INTERFACE -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(chaistl INTERFACE -fsanitize=address)
endif()

if(CHAISTL_ENABLE_UBSAN)
  # -fno-sanitize-recover=all: die on the first UB report so tests fail
  # instead of logging and continuing.
  target_compile_options(chaistl INTERFACE
    -fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(chaistl INTERFACE -fsanitize=undefined)
endif()

# Developer warnings live on a separate INTERFACE target consumed by
# chaistl's own tests and benchmarks — never on chaistl::chaistl itself,
# so downstream consumers are unaffected.
add_library(chaistl_dev_flags INTERFACE)
add_library(chaistl::dev_flags ALIAS chaistl_dev_flags)

if(CHAISTL_DEV_WARNINGS)
  target_compile_options(chaistl_dev_flags INTERFACE -Wall -Wextra -Wpedantic)
endif()

if(CHAISTL_WERROR)
  target_compile_options(chaistl_dev_flags INTERFACE -Werror)
endif()

if(CHAISTL_ENABLE_CXX26)
  target_compile_options(chaistl INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,Clang>:-std=c++26>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-std=c++26>)
endif()

if(CHAISTL_BUILD_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(test)
  endif()
endif()

if(CHAISTL_BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()

if(CHAISTL_BUILD_VISUALIZER)
  add_subdirectory(tools/visualizer)
endif()

install(
  TARGETS chaistl
  EXPORT chaistlTargets
  FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(
  EXPORT chaistlTargets
  NAMESPACE chaistl::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/chaistl")

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/chaistlConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/chaistlConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/chaistl")

write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/chaistlConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion)

install(FILES "${PROJECT_BINARY_DIR}/chaistlConfig.cmake"
              "${PROJECT_BINARY_DIR}/chaistlConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/chaistl")
