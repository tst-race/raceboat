
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

# Add the source to the test executable
add_executable(testStandaloneLibrary
    headers/ChannelRoleTest.cpp
    headers/LinkPropertiesTest.cpp
    headers/LinkPropertyPairTest.cpp
    headers/LinkPropertySetTest.cpp
    ComponentActionManagerTest.cpp
    ComponentLifetimeManagerTest.cpp
    ComponentManagerTest.cpp
    ComponentPackageManagerTest.cpp
    ComponentReceivePackageManagerTest.cpp
    # ConfigTest.cpp
    CoreTest.cpp
    main.cpp
    ExternalApiTest.cpp
    # FileSystemTest.cpp
    HandlerTest.cpp
    IntegrationTest.cpp
    PluginLoadingTest.cpp
    PluginWrapperTest.cpp
    RaceLogTests.cpp
    SdkWrapperTest.cpp
    UserInputTest.cpp
    # StateMachineTest.cpp
    # ApiManager/DialTest.cpp
    # ApiManager/ListenTest.cpp
    # ApiManager/ReceiveRespondTest.cpp
    # ApiManager/RecvTest.cpp
    # ApiManager/SendTest.cpp
    # ApiManager/SendReceiveTest.cpp
    # ApiManager/BootstrapListenTest.cpp
)

target_compile_definitions(testStandaloneLibrary 
    PUBLIC EXPECT_LOG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../test-output"
    PUBLIC INPUT_FILE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../input-files"
    PUBLIC UNIFIED_STUB="$<TARGET_FILE:unified-test-stub>"
    PUBLIC DECOMPOSED_ENCODING="$<TARGET_FILE:DecomposedTestEncoding>"
    PUBLIC DECOMPOSED_USERMODEL="$<TARGET_FILE:DecomposedTestUserModel>"
    PUBLIC DECOMPOSED_TRANSPORT="$<TARGET_FILE:DecomposedTestTransport>"
    PUBLIC DECOMPOSED_IMPLEMENTATION="$<TARGET_FILE:DecomposedTestTransportImplementation>"
)

message("CMAKE_CURRENT_SOURCE_DIR " ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(testStandaloneLibrary PRIVATE
    $<BUILD_INTERFACE:/code/include/race/common/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/race>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/race/unified>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/race/decomposed>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/race/common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../source/api-managers>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../source/plugin-loading>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    )

# Link external dependencies
find_package(GTest REQUIRED CONFIG)
find_package(Threads REQUIRED)

target_link_libraries(testStandaloneLibrary
    GTest::gmock
    GTest::gtest
    Threads::Threads
    raceSdkCommon
)

add_dependencies(testStandaloneLibrary
    unified-test-stub
    DecomposedTestEncoding
    DecomposedTestUserModel
    DecomposedTestTransport
    DecomposedTestEncodingImplementation
    DecomposedTestUserModelImplementation
    DecomposedTestTransportImplementation
)

# add_dependencies(build_raceSdkCommon_tests testStandaloneLibrary)
add_test(race-lib ${CMAKE_CURRENT_BINARY_DIR}/testStandaloneLibrary)
set_tests_properties(race-lib PROPERTIES LABELS "unit;raceSdkCommon")
setup_coverage_for_target(
    TARGET testStandaloneLibrary
    SOURCE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../..
)
setup_valgrind_for_target(testStandaloneLibrary)

# Setup style validation
setup_clang_format_for_target(testStandaloneLibrary PARENT raceSdkCommon)
