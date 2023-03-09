# Copyright 2023, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

include(CTest)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

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
