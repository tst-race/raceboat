
# Copyright 2023 Two Six Technologies
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 

cmake_minimum_required(VERSION 3.13)
project(raceboat LANGUAGES CXX)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -v")

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/race-cmake-modules)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

set(BUILD_VERSION "" CACHE STRING "Version of the SDK being built")
if("${BUILD_VERSION}" STREQUAL "")
    message(FATAL_ERROR "Missing or empty BUILD_VERSION argument")
endif()

include(race/clang-format)
include(race/coverage)
include(race/java-format)
include(race/lint)
include(race/test-targets)
include(race/valgrind)
include(race/warnings)

setup_project_test_targets(raceboat UNIT INTEGRATION)
setup_project_format_targets(raceboat)

add_subdirectory(language-shims)
add_subdirectory(source)
if(NOT ANDROID)
    add_subdirectory(app/race-cli)
    add_subdirectory(test/common/UnifiedTestStub EXCLUDE_FROM_ALL)
    add_subdirectory(test/common/DecomposedTestStub EXCLUDE_FROM_ALL)
    add_subdirectory(test/common/plugins/myOS/myArch/DecomposedTestImplementation EXCLUDE_FROM_ALL)
    add_subdirectory(test/source EXCLUDE_FROM_ALL)
endif()

# Create this after all subdirectories have been added
setup_project_coverage_target()

install(
    TARGETS
        raceboat
    EXPORT raceboat
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

install(
    EXPORT raceboat
    DESTINATION lib/cmake
)