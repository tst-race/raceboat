
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


function(setup_component_target)
    set(OPTIONS "")
    set(ONE_VALUE_ARGS TARGET)
    set(MULTI_VALUE_ARGS SOURCES)
    cmake_parse_arguments(COMPONENT "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

    add_library(${COMPONENT_TARGET} SHARED ${COMPONENT_SOURCES})
    target_include_directories(
        ${COMPONENT_TARGET}
        PRIVATE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../../include/race>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    )

    # TODO enable performance-* and readability-* checks (needs SDK fixes)
    setup_clang_tidy_for_target(TARGET ${COMPONENT_TARGET})
    setup_cppcheck_for_target(
        TARGET ${COMPONENT_TARGET}
        CHECKS all
        SUPPRESS unmatchedSuppression unusedFunction missingIncludeSystem
    )
    # TODO fix issue with multiple check_format targets
    # setup_clang_format_for_target(${COMPONENT_TARGET} PARENT raceSdkCommon)

    add_custom_command(TARGET ${COMPONENT_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json ${CMAKE_CURRENT_BINARY_DIR}/
    )
endfunction()

setup_component_target(
    TARGET DecomposedTestEncodingImplementation
    SOURCES
        DecomposedTestEncoding.cpp
)

setup_component_target(
    TARGET DecomposedTestUserModelImplementation
    SOURCES
        DecomposedTestUserModel.cpp
)

setup_component_target(
    TARGET DecomposedTestTransportImplementation
    SOURCES
        DecomposedTestTransport.cpp
)
