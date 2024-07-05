#!/usr/bin/env bash

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


set -e

CMAKE_ARGS=""
CORE_BINDINGS_GOLANG_BINARY_DIR=""

for i in "$@"
do
    key="$1"
    echo "key $1"
    case $key in
        -p=*)
        CORE_BINDINGS_GOLANG_BINARY_DIR="${1#*=}"
        shift
        ;;

        # echo "unknown argument \"$1\""
        # exit 1
        # ;;
    esac
done


LINUX_PRESET=LINUX_x86_64
ARCH=x86_64
if [ "$(uname -m)" == "aarch64" ] || [ "$(uname -m)" == "arm64" ]
then
    LINUX_PRESET=LINUX_arm64-v8a
    ARCH=arm64-v8a
fi

if [ -z "$CORE_BINDINGS_GOLANG_BINARY_DIR" ]
then
    DEFAULT_GOLANG_PATH="/code/build/LINUX_${ARCH}/language-shims/source/include/src/core"
    echo "INFO: No -p argument, assuming golang install path is: ${DEFAULT_GOLANG_PATH}"
    CMAKE_ARGS="-DCORE_BINDINGS_GOLANG_BINARY_DIR=${DEFAULT_GOLANG_PATH}"
    # echo "ERROR: golang install path required (eg ./build.sh -p build/LINUX/language-shims/source/include/src)"
else
    # CMAKE_ARGS="-DCORE_BINDINGS_GOLANG_BINARY_DIR=${CORE_BINDINGS_GOLANG_BINARY_DIR} -D CMAKE_LIBRARY_PATH=/code/racesdk/package/LINUX_x86_64/lib/"
    echo "path: $CORE_BINDINGS_GOLANG_BINARY_DIR"
    CMAKE_ARGS="-DCORE_BINDINGS_GOLANG_BINARY_DIR="$CORE_BINDINGS_GOLANG_BINARY_DIR

    echo "golang install path $CORE_BINDINGS_GOLANG_BINARY_DIR"
fi


cmake --preset=$LINUX_PRESET -DBUILD_VERSION="local"
cmake --build -j --preset=$LINUX_PRESET --target install
echo "Packaging ${LINUX_PRESET}"
cmake --install build/${LINUX_PRESET}/ --component sdk --verbose

if [ "$(uname -m)" == "x86_64" ]
then
    cmake --preset=ANDROID_x86_64 -DBUILD_VERSION="local"
    cmake --build -j --preset=ANDROID_x86_64 --target install
    echo "Packaging ANDROID_x86_64"
    cmake --install build/ANDROID_x86_64/ --component sdk --verbose

    cmake --preset=ANDROID_arm64-v8a -DBUILD_VERSION="local"
    cmake --build -j --preset=ANDROID_arm64-v8a --target install
    echo "Packaging ANDROID_arm64"
    cmake --install build/ANDROID_arm64-v8a/ --component sdk --verbose
else
    echo "android build not supported on aarch64 arch"
    # cmake -DANDROID_STL="c++_shared -frtti"  -DBUILD_VERSION="local" -DCXX="/opt/android/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++"
    # cmake --build -j -DANDROID_STL="c++_shared -frtti" -DCXX="/opt/android/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++" --target install
    # cmake --build -j --preset=ANDROID_arm64-v8a --target install
fi

cp -r racesdk/package/${LINUX_PRESET}/lib/* /linux/${ARCH}/lib
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/linux/${ARCH}/lib" 
mkdir -p /linux/${ARCH}/include/race/common
cp -r racesdk/package/${LINUX_PRESET}/include/* /linux/${ARCH}/include/race/common/
cp /linux/${ARCH}/include/race/common/Race.h /linux/${ARCH}/include/race/
cp -r racesdk/package/${LINUX_PRESET}/go/* /usr/local/go/*

pushd pluggable-transport
cmake --preset=$LINUX_PRESET -DBUILD_VERSION="local" ${CMAKE_ARGS}
cmake --build -j --preset=$LINUX_PRESET --target install 
cp -r racesdk/package/${LINUX_PRESET}/lib/race/raceDispatcher ../racesdk/package/${LINUX_PRESET}/lib/raceDispatcher
