if(NOT DEFINED TEST_SOURCE_DIR)
  message(FATAL_ERROR "TEST_SOURCE_DIR is required")
endif()
if(NOT DEFINED TEST_BINARY_DIR)
  message(FATAL_ERROR "TEST_BINARY_DIR is required")
endif()
if(NOT DEFINED CHAISTL_SOURCE_DIR)
  message(FATAL_ERROR "CHAISTL_SOURCE_DIR is required")
endif()
if(NOT DEFINED CASE_SOURCE)
  message(FATAL_ERROR "CASE_SOURCE is required")
endif()
if(NOT DEFINED CASE_NAME)
  message(FATAL_ERROR "CASE_NAME is required")
endif()

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${TEST_SOURCE_DIR}"
    -B "${TEST_BINARY_DIR}"
    -G "${TEST_GENERATOR}"
    "-DCHAISTL_SOURCE_DIR=${CHAISTL_SOURCE_DIR}"
    "-DCASE_SOURCE=${CASE_SOURCE}"
    "-DCHAISTL_COMPILE_FAIL_ENABLE_CXX26=${TEST_ENABLE_CXX26}"
    "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${TEST_BUILD_TYPE}"
  RESULT_VARIABLE configure_result
  OUTPUT_VARIABLE configure_output
  ERROR_VARIABLE configure_error)

if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR
          "Compile-fail case ${CASE_NAME} failed to configure.\n${configure_output}\n${configure_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${TEST_BINARY_DIR}"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error)

if(build_result EQUAL 0)
  message(FATAL_ERROR "Compile-fail case ${CASE_NAME} unexpectedly built successfully")
endif()

message(STATUS "Compile-fail case ${CASE_NAME} failed as expected")
