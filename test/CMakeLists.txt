set(CHAISTL_GOOGLETEST_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/googletest")

if(NOT EXISTS "${CHAISTL_GOOGLETEST_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
          "GoogleTest submodule is missing. Run: git submodule update --init --recursive")
endif()

set(INSTALL_GTEST
    OFF
    CACHE BOOL "" FORCE)
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)

add_subdirectory("${CHAISTL_GOOGLETEST_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/third_party/googletest")

# Tests are directory-driven: adding a test .cpp should automatically add it to
# the suite. Production targets should still prefer explicit source lists.
file(GLOB_RECURSE CHAISTL_TEST_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false
     "${CMAKE_CURRENT_SOURCE_DIR}/chaistl/*.cpp")

add_executable(chaistl_tests)
target_sources(chaistl_tests PRIVATE ${CHAISTL_TEST_SOURCES})
target_link_libraries(chaistl_tests PRIVATE chaistl::chaistl chaistl::dev_flags GTest::gmock
                                            GTest::gtest_main)

# Public entry-point headers are also directory-driven. Keep this non-recursive
# so detail/ and implementation category directories are not treated as public.
file(GLOB CHAISTL_PUBLIC_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false
     RELATIVE "${PROJECT_SOURCE_DIR}/include"
     "${PROJECT_SOURCE_DIR}/include/chaistl/*.hpp"
     "${PROJECT_SOURCE_DIR}/include/chaistl/containers/*.hpp"
     "${PROJECT_SOURCE_DIR}/include/chaistl/experimental/*.hpp"
     "${PROJECT_SOURCE_DIR}/include/chaistl/experimental/containers/*.hpp")

set(CHAISTL_PUBLIC_HEADER_SELF_CONTAINED_SOURCES)
set(CHAISTL_PUBLIC_HEADER_SELF_CONTAINED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/public_header_self_contained")
file(MAKE_DIRECTORY "${CHAISTL_PUBLIC_HEADER_SELF_CONTAINED_DIR}")

foreach(header IN LISTS CHAISTL_PUBLIC_HEADERS)
  string(REGEX REPLACE "[^A-Za-z0-9_]" "_" source_name "${header}")
  set(source "${CHAISTL_PUBLIC_HEADER_SELF_CONTAINED_DIR}/${source_name}.cpp")
  file(CONFIGURE OUTPUT "${source}"
       CONTENT "#include <${header}>\n\nvoid ${source_name}_self_contained() {}\n"
       NEWLINE_STYLE UNIX)
  list(APPEND CHAISTL_PUBLIC_HEADER_SELF_CONTAINED_SOURCES "${source}")
endforeach()

add_library(chaistl_public_headers_self_contained OBJECT)
target_sources(chaistl_public_headers_self_contained
               PRIVATE ${CHAISTL_PUBLIC_HEADER_SELF_CONTAINED_SOURCES})
target_link_libraries(chaistl_public_headers_self_contained
                      PRIVATE chaistl::chaistl chaistl::dev_flags)

include(GoogleTest)
gtest_discover_tests(chaistl_tests TEST_PREFIX chaistl.)
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
             "${CMAKE_CURRENT_LIST_DIR}/cmake/label_tests.cmake")

function(add_chaistl_compile_fail_test name source)
  add_test(
    NAME chaistl.compile_fail.${name}
    COMMAND
      ${CMAKE_COMMAND}
      -DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/compile_fail
      -DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/compile_fail/${name}
      -DCHAISTL_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DCASE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/${source}
      -DCASE_NAME=${name}
      -DTEST_GENERATOR=${CMAKE_GENERATOR}
      -DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DTEST_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DTEST_ENABLE_CXX26=${CHAISTL_ENABLE_CXX26}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/run.cmake)
  set(labels compile_fail)
  if(name MATCHES "^containers\\.")
    list(APPEND labels containers)
  endif()
  if(name MATCHES "\\.sequence\\.")
    list(APPEND labels sequence)
  endif()
  if(name MATCHES "\\.vector\\.")
    list(APPEND labels vector)
  endif()
  set_tests_properties(chaistl.compile_fail.${name} PROPERTIES LABELS "${labels}")
endfunction()

add_chaistl_compile_fail_test(containers.sequence.vector.allocator_value_type
                              chaistl/containers/sequence/vector/allocator_value_type.cpp)
add_chaistl_compile_fail_test(containers.sequence.vector.allocator_missing_storage
                              chaistl/containers/sequence/vector/allocator_missing_storage.cpp)
add_chaistl_compile_fail_test(containers.sequence.vector.destructible_value_type
                              chaistl/containers/sequence/vector/destructible_value_type.cpp)
