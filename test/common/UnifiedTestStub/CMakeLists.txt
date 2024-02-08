
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

# Add the source to the shared library
add_library(unified-test-stub SHARED
    UnifiedTestStub.cpp
)

target_include_directories(unified-test-stub PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include/race>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# TODO enable performance-* and readability-* checks (needs fixes)
# setup_clang_tidy_for_target(TARGET unified-test-stub)
# setup_cppcheck_for_target(
#     TARGET unified-test-stub
#     CHECKS all
#     SUPPRESS unmatchedSuppression unusedFunction missingIncludeSystem useStlAlgorithm
# )
setup_clang_format_for_target(unified-test-stub PARENT raceboat)
