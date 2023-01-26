# Copyright 2023, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

project(common LANGUAGES CXX)

include(CMakeDependentOption)
find_package(nlohmann_json 3.2.0 REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

#
# common library
#
add_library(
  common-library STATIC
  src/logger.cc
  src/random.cc
  src/settings.cc
)

target_compile_features(common-library PRIVATE cxx_std_17)
target_compile_options(
  common-library
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
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
    nlohmann_json::nlohmann_json
)
