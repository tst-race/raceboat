
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

cmake_minimum_required(VERSION 3.2.0)

if ("${TARGET_ARCHITECTURE}" STREQUAL "ANDROID_x86_64")
    set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/code/racesdk/package/ANDROID_x86_64")
    # set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/android/x86_64")
elseif("${TARGET_ARCHITECTURE}" STREQUAL "ANDROID_arm64-v8a_M1")
    set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/code/racesdk/package/ANDROID_arm64-v8a_M1")
    # set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/android/arm64-v8a")
elseif("${TARGET_ARCHITECTURE}" STREQUAL "ANDROID_arm64-v8a")
    set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/code/racesdk/package/ANDROID_arm64-v8a")
    # set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/android/arm64-v8a")
elseif("${TARGET_ARCHITECTURE}" STREQUAL "LINUX_arm64-v8a")
    set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/code/racesdk/package/LINUX_arm64-v8a")
    # set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/linux/arm64-v8a")
elseif("${TARGET_ARCHITECTURE}" STREQUAL "LINUX_x86_64")
    set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/code/racesdk/package/LINUX_x86_64")
    # set(ENV{RACE_COMM_LIB_INSTALL_PATH} "/linux/x86_64")
else()
    message( FATAL_ERROR "unhandled target architecture ${TARGET_ARCHITECTURE}.  Exiting...")
endif()
set(ENV{RACE_COMM_LIBS_PATH} "$ENV{RACE_COMM_LIB_INSTALL_PATH}/lib")
set(ENV{RACE_COMM_INCLUDE_PATH} "$ENV{RACE_COMM_LIB_INSTALL_PATH}/include")

add_subdirectory(pkg/race_pt3)
add_subdirectory(cmd/race-dispatcher)
add_subdirectory(tests)
