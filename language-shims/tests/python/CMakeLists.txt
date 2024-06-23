
# 
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


add_executable(unitTestPythonBindings
    main.cpp
    PyDecomposeTest.cpp
    CommsPluginTest.cpp
)

target_include_directories(unitTestPythonBindings SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../test/common/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../source/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../source/plugin-loading/>
)

target_compile_definitions(unitTestPythonBindings
    PUBLIC PLUGIN_PATH="${CMAKE_CURRENT_SOURCE_DIR}/"
    PYTHON_PLUGIN_SUPPORT=1
)

# Link external dependencies
find_package(GTest REQUIRED CONFIG)

target_link_libraries(unitTestPythonBindings
    GTest::gmock
    raceSdkCommon
    commsPluginBindings
)

setup_coverage_for_target(
    TARGET unitTestPythonBindings
    SOURCE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..
    EXCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/*"
)

# add_dependencies(build_raceboat_tests unitTestPythonBindings)

add_test(python-bindings ${CMAKE_CURRENT_BINARY_DIR}/unitTestPythonBindings)
set_tests_properties(python-bindings PROPERTIES LABELS "unit;raceSdkCommon")

string(CONCAT PYTHON_BINDINGS_PATHDEPS_STR
            "${CMAKE_CURRENT_SOURCE_DIR}:"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../:"
            "${CMAKE_CURRENT_SOURCE_DIR}/stubs:"
            "${CMAKE_CURRENT_BINARY_DIR}/include:"
            "${CMAKE_CURRENT_BINARY_DIR}/../../source/include")
add_definitions(-DPYTHON_BINDINGS_PATHDEPS="${PYTHON_BINDINGS_PATHDEPS_STR}")
