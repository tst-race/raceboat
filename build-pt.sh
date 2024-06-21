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

if [ -z "$CORE_BINDINGS_GOLANG_BINARY_DIR" ]
then
    echo "path: $CORE_BINDINGS_GOLANG_BINARY_DIR"
    echo "ERROR: golang install path required (eg ./build.sh -p build/LINUX/language-shims/source/include/src)"
    exit 1
else
    # CMAKE_ARGS="-DCORE_BINDINGS_GOLANG_BINARY_DIR=${CORE_BINDINGS_GOLANG_BINARY_DIR} -D CMAKE_LIBRARY_PATH=/code/racesdk/package/LINUX_x86_64/lib/"
    CMAKE_ARGS="-DCORE_BINDINGS_GOLANG_BINARY_DIR="$CORE_BINDINGS_GOLANG_BINARY_DIR

    echo "golang install path $CORE_BINDINGS_GOLANG_BINARY_DIR"
fi

LINUX_PRESET=LINUX_x86_64
if [ "$(uname -m)" == "aarch64" ] || [ "$(uname -m)" == "arm64" ]
then
    LINUX_PRESET=LINUX_arm64-v8a
fi

cmake --preset=$LINUX_PRESET -DBUILD_VERSION="local"
cmake --build -j --preset=$LINUX_PRESET --target install

if [ "$(uname -m)" == "x86_64" ]
then
    cmake --preset=ANDROID_x86_64 -DBUILD_VERSION="local"
    cmake --build -j --preset=ANDROID_x86_64 --target install

    cmake --preset=ANDROID_arm64-v8a -DBUILD_VERSION="local"
    cmake --build -j --preset=ANDROID_arm64-v8a --target install
else
    echo "android build not supported on aarch64 arch"
    # cmake -DANDROID_STL="c++_shared -frtti"  -DBUILD_VERSION="local" -DCXX="/opt/android/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++"
    # cmake --build -j -DANDROID_STL="c++_shared -frtti" -DCXX="/opt/android/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++" --target install
    # cmake --build -j --preset=ANDROID_arm64-v8a --target install
fi

pushd pluggable-transport
echo PWD $PWD

###
# docker run -it --rm --name=build-pt \
#        -e MAKEFLAGS="-j" \
#        -v $(pwd)/:/code/ \
#        -w /code \
#        ghcr.io/tst-race/race-images/race-compile:main bash
# Manual steps so far:
# cp /code/build/LINUX_x86_64/source/libraceSdkCommon.so /linux/x86_64/lib/ 
# cp /code/build/LINUX_x86_64/language-shims/source/corePluginBindingsGolang.so /linux/x86_64/lib/
# next error:
# corePluginBindingsGO_wrap.cxx:353:10: fatal error: 'race/common/ChannelId.h' file not found
# Believe the current system expects $PATH/race/common/<.h files>
###

cmake --debug-find --debug-output --preset=$LINUX_PRESET -DBUILD_VERSION="local" ${CMAKE_ARGS}
cmake --build -j --preset=$LINUX_PRESET --target install

