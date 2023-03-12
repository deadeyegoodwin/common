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
  task-timewheel-pool
  task_timewheel_pool.cc
)
add_dependencies(common-test task-timewheel-pool)

target_compile_features(task-timewheel-pool PRIVATE cxx_std_17)
target_compile_options(
  task-timewheel-pool
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
)

set_target_properties(
  task-timewheel-pool
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME task_timewheel_pool
)

target_link_libraries(
  task-timewheel-pool
  PRIVATE
    Threads::Threads
    common-library
)

add_test(NAME task-timewheel-pool0 COMMAND task_timewheel_pool 0)
add_test(NAME task-timewheel-pool1 COMMAND task_timewheel_pool 1)
add_test(NAME task-timewheel-pool2 COMMAND task_timewheel_pool 2)
add_test(NAME task-timewheel-pool3 COMMAND task_timewheel_pool 3)
add_test(NAME task-timewheel-pool4 COMMAND task_timewheel_pool 4)
add_test(NAME task-timewheel-pool5 COMMAND task_timewheel_pool 5)
add_test(NAME task-timewheel-pool6 COMMAND task_timewheel_pool 6)
add_test(NAME task-timewheel-pool7 COMMAND task_timewheel_pool 7)
add_test(NAME task-timewheel-pool8 COMMAND task_timewheel_pool 8)
add_test(NAME task-timewheel-pool9 COMMAND task_timewheel_pool 9)
