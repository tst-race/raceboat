
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
set(CMAKE_VERBOSE_MAKEFILE ON)

project(PluggableTransportsGolang)
message("building race pt library")

################################################################################
# race pt library
################################################################################
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -v")

set(PT_FILE_NAME "race_pt3")
set(PT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PT_FILE_NAME})

set(GOMOD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/go.mod)
set(GOCMD go build -work -o ${PT_FILE})

message("MESSAGE RACE_COMM_INCLUDE_PATH $ENV{RACE_COMM_INCLUDE_PATH}")
message("MESSAGE RACE_COMM_LIB_INSTALL_PATH $ENV{RACE_COMM_LIB_INSTALL_PATH}")
list(APPEND CGO_CXXFLAGS "-I$ENV{RACE_COMM_LIB_INSTALL_PATH}/include")
list(APPEND CGO_CXXFLAGS "-I$ENV{RACE_COMM_INCLUDE_PATH}")
list(APPEND CGO_LDFLAGS "-L$ENV{RACE_COMM_LIB_INSTALL_PATH}/lib")
list(APPEND CGO_LDFLAGS "-L$ENV{RACE_COMM_LIBS_PATH}")


if(ANDROID)
    # Setup common GOENV variables
    list(APPEND GOENV
        CC=${ANDROID_TOOLCHAIN_ROOT}/bin/${CMAKE_LIBRARY_ARCHITECTURE}${ANDROID_PLATFORM}-clang
        CXX=${ANDROID_TOOLCHAIN_ROOT}/bin/${CMAKE_LIBRARY_ARCHITECTURE}${ANDROID_PLATFORM}-clang++
        GOOS=android
        CGO_ENABLED=1
    )

    if ("${TARGET_ARCHITECTURE}" STREQUAL "ANDROID_x86_64")
        list(APPEND GOENV
            GOPATH=/android/x86_64/go:$ENV{RACE_COMM_LIBS_PATH}
            GOARCH=amd64
        )
    else()
        list(APPEND GOENV
            GOPATH=/android/arm64-v8a/go:$ENV{RACE_COMM_LIBS_PATH}
            GOARCH=arm64
        )
    endif()

else()
    list(APPEND CGO_LDFLAGS "-L/usr/local/lib")
    list(APPEND GOENV GOPATH=/usr/local/go:$ENV{RACE_COMM_LIBS_PATH})
    if ("${TARGET_ARCHITECTURE}" STREQUAL "LINUX_arm64-v8a")
        list(APPEND GOENV
            "CC=clang -target aarch64-linux-gnu"
            "CXX=clang++ -target aarch64-linux-gnu"
        )
        list(APPEND GOCMD -ldflags "-linkmode=external '-extldflags=-target aarch64-linux-gnu'")
        list(APPEND GOENV GOARCH=arm64)
    else()
        list(APPEND GOENV
            "CC=clang -target x86_64-linux-gnu"
            "CXX=clang++ -target x86_64-linux-gnu"
        )
        list(APPEND GOCMD -ldflags "-linkmode=external '-extldflags=-target x86_64-linux-gnu'")
        list(APPEND GOENV GOARCH=amd64)
    endif()
endif()

# Set common CXX/LD flags
list(APPEND CGO_CXXFLAGS
    -DBUILD_VERSION=\"${BUILD_VERSION}\"
    -I${CMAKE_CURRENT_BINARY_DIR}
)
list(APPEND CGO_CXXFLAGS
    -std=c++17
)

find_library(
    RACE_COMM_LIB raceSdkCommon REQUIRED
    PATHS $ENV{RACE_COMM_LIBS_PATH}
)
find_library(
    CORE_PT_LIB corePluginBindingsGolang.so REQUIRED
    PATHS $ENV{RACE_COMM_LIBS_PATH}
)


list(APPEND CGO_LDFLAGS
    -Wl,--unresolved-symbols=ignore-all
    -lraceSdkCommon
    -l:corePluginBindingsGolang.so
)

# Convert array variables to strings
string(REPLACE ";" " " CGO_CXXFLAGS "${CGO_CXXFLAGS}")
string(REPLACE ";" " " CGO_LDFLAGS "${CGO_LDFLAGS}")

list(APPEND GOENV
    CGO_CXXFLAGS=${CGO_CXXFLAGS}
    CGO_LDFLAGS=${CGO_LDFLAGS}
)

add_custom_command(
    COMMAND sed "s|CORE_BINDINGS_GOLANG_BINARY_DIR|${CORE_BINDINGS_GOLANG_BINARY_DIR}|g" ${GOMOD_PATH}.in > ${GOMOD_PATH}
    COMMAND ${CMAKE_COMMAND} -E env ${GOENV} ${GOCMD}
    DEPENDS "race_pt3.go"
    OUTPUT ${PT_FILE_NAME} ${GOMOD_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

add_custom_target(RacePt3 ALL DEPENDS ${PT_FILE_NAME})
