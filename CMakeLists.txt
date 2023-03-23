# Copyright 2023, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

project(common LANGUAGES CXX)

include(CMakeDependentOption)
include(ExternalProject)

find_package(nlohmann_json 3.2.0 REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(ASAN "Enable ASAN" OFF)

set(CK_REPO_TAG "cplusplus" CACHE STRING "Tag for concurrencykit repo")

if(ASAN)
  set(COMMON_ASAN_FLAGS "-fsanitize=address")
endif(ASAN)


#
# concurrency kit
#
ExternalProject_Add(ck
  PREFIX ck
  GIT_REPOSITORY git@github.com:deadeyegoodwin/ck.git
  GIT_TAG ${CK_REPO_TAG}
  GIT_SHALLOW ON
  GIT_PROGRESS ON
  UPDATE_DISCONNECTED ON
  EXCLUDE_FROM_ALL ON
  CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ck/src/ck/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/ck
)

add_library(ck-library STATIC IMPORTED GLOBAL)
add_dependencies(ck-library ck)
set_target_properties(
  ck-library
  PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/ck/lib/libck.a
)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ck/include)
target_include_directories(
  ck-library
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/ck/include>
)

#
# common library
#
add_library(
  common-library STATIC
  src/common_time.cc
  src/json_preprocessor.cc
  src/logger.cc
  src/random.cc
  src/settings.cc
  src/task_pool.cc
  src/task_timewheel.cc
)

target_compile_features(common-library PRIVATE cxx_std_17)
target_compile_options(
  common-library
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${COMMON_ASAN_FLAGS}
)

target_include_directories(
  common-library
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

set_target_properties(
  common-library
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME common
)

target_link_libraries(
  common-library
  PUBLIC
    Threads::Threads
    nlohmann_json::nlohmann_json
    ${COMMON_ASAN_FLAGS}
    ck-library
)

#
# Test target
#
add_custom_target(
  common-test
  COMMAND ${CMAKE_CTEST_COMMAND} -j8 --timeout 15 --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test
  COMMENT "Running common tests"
  DEPENDS common-library
)
add_subdirectory(test)
