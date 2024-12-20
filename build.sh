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

LINUX_PRESET=LINUX_x86_64
ARCH=x86_64
if [ "$(uname -m)" == "aarch64" ] || [ "$(uname -m)" == "arm64" ]
then
    LINUX_PRESET=LINUX_arm64-v8a
    ARCH=arm64-v8a
fi

cmake --preset=$LINUX_PRESET -DBUILD_VERSION="local"  --debug-output -DCXX="clang++ -std=c++17"
cmake --build -j --preset=$LINUX_PRESET --target install

echo "Packaging ${LINUX_PRESET}"
cmake --install build/${LINUX_PRESET}/ --component sdk --verbose

if [ "$(uname -m)" == "x86_64" ]
then
echo "Building ANDROID x86_64"
    cmake --preset=ANDROID_x86_64 -DBUILD_VERSION="local"
    cmake --build -j --preset=ANDROID_x86_64 --target install
    echo "Packaging ANDROID_x86_64"
    cmake --install build/ANDROID_x86_64/ --component sdk --verbose

echo "Building ANDROID arm64-v8a"
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

# mkdir -p /usr/local/go

# cp -r racesdk/package/${LINUX_PRESET}/lib/* /linux/${ARCH}/lib
# export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/linux/${ARCH}/lib" 
# mkdir -p /linux/${ARCH}/include/race/common
# cp -r racesdk/package/${LINUX_PRESET}/include/* /linux/${ARCH}/include/race/common/
# cp /linux/${ARCH}/include/race/common/Race.h /linux/${ARCH}/include/race/
# cp -r racesdk/package/${LINUX_PRESET}/go/* /usr/local/go/*

