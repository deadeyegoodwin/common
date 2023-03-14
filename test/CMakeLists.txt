# Copyright 2023, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

include(CTest)

find_package(nlohmann_json 3.2.0 REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#
# JsonPreprocessor
#
add_executable(
  json-preprocessor-test
  json_preprocessor_test.cc
)
add_dependencies(common-test json-preprocessor-test)

target_compile_features(json-preprocessor-test PRIVATE cxx_std_17)
target_compile_options(
  json-preprocessor-test
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${COMMON_ASAN_FLAGS}
)

set_target_properties(
  json-preprocessor-test
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME json_preprocessor_test
)

target_link_libraries(
  json-preprocessor-test
  PRIVATE
    nlohmann_json::nlohmann_json
    ${COMMON_ASAN_FLAGS}
    common-library
)

# Expected pass
file(GLOB_RECURSE PASS_TEST_IN_FILES "${TEST_SOURCE_DIR}/*.json.in")
foreach(TEST_IN_FILE ${PASS_TEST_IN_FILES})
  string(REGEX REPLACE "\\.json.in$" "" TEST_NAME ${TEST_IN_FILE})
  string(REGEX REPLACE "\\.json.in$" "\.json.out" TEST_OUT_FILE ${TEST_IN_FILE})
  add_test(NAME ${TEST_NAME}
    COMMAND json_preprocessor_test ${TEST_IN_FILE} ${TEST_OUT_FILE})
endforeach()

# Expected fail
file(GLOB_RECURSE FAIL_TEST_IN_FILES "${TEST_SOURCE_DIR}/*.json.in.fail")
foreach(TEST_IN_FILE ${FAIL_TEST_IN_FILES})
  string(REGEX REPLACE "\\.json.in.fail$" "_json_fail" TEST_NAME ${TEST_IN_FILE})
  string(REGEX REPLACE "\\.json.in.fail$" "\.json.out.fail" TEST_OUT_FILE ${TEST_IN_FILE})
  if(EXISTS "${TEST_OUT_FILE}")
    add_test(NAME ${TEST_NAME}
      COMMAND json_preprocessor_test ${TEST_IN_FILE} ${TEST_OUT_FILE})
  else()
    add_test(NAME ${TEST_NAME}
      COMMAND json_preprocessor_test ${TEST_IN_FILE} ${TEST_IN_FILE})
  endif()
  set_property(TEST ${TEST_NAME} PROPERTY WILL_FAIL ON)
endforeach()


#
# TaskTimewheelPool
#
add_executable(
  task-timewheel-pool-test
  task_timewheel_pool_test.cc
)
add_dependencies(common-test task-timewheel-pool-test)

target_compile_features(task-timewheel-pool-test PRIVATE cxx_std_17)
target_compile_options(
  task-timewheel-pool-test
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${COMMON_ASAN_FLAGS}
)

set_target_properties(
  task-timewheel-pool-test
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME task_timewheel_pool_test
)

target_link_libraries(
  task-timewheel-pool-test
  PRIVATE
    Threads::Threads
    ${COMMON_ASAN_FLAGS}
    common-library
)

add_test(NAME task-timewheel-pool-test0 COMMAND task_timewheel_pool_test 0)
add_test(NAME task-timewheel-pool-test1 COMMAND task_timewheel_pool_test 1)
add_test(NAME task-timewheel-pool-test2 COMMAND task_timewheel_pool_test 2)
add_test(NAME task-timewheel-pool-test3 COMMAND task_timewheel_pool_test 3)
add_test(NAME task-timewheel-pool-test4 COMMAND task_timewheel_pool_test 4)
add_test(NAME task-timewheel-pool-test5 COMMAND task_timewheel_pool_test 5)
add_test(NAME task-timewheel-pool-test6 COMMAND task_timewheel_pool_test 6)
add_test(NAME task-timewheel-pool-test7 COMMAND task_timewheel_pool_test 7)
add_test(NAME task-timewheel-pool-test8 COMMAND task_timewheel_pool_test 8)
add_test(NAME task-timewheel-pool-test9 COMMAND task_timewheel_pool_test 9)
add_test(NAME task-timewheel-pool-test10 COMMAND task_timewheel_pool_test 10)
add_test(NAME task-timewheel-pool-test11 COMMAND task_timewheel_pool_test 11)
