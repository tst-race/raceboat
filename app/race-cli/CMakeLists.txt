
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


add_executable(race-cli main.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_include_directories(race-cli PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)


target_link_libraries(race-cli
    raceboat
    stdc++fs
    Threads::Threads
    ${CMAKE_DL_LIBS} # Contains dlopen and dlcose for dynamically loading plugins.
)

# TODO enable performance-* and readability-* checks (needs fixes)
setup_clang_tidy_for_target(TARGET race-cli)
# setup_cppcheck_for_target(
#     TARGET race-cli
#     CHECKS all
#     SUPPRESS unmatchedSuppression unusedFunction missingIncludeSystem useStlAlgorithm
# )
setup_clang_format_for_target(race-cli PARENT raceboat)
