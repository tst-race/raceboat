
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

# TODO: from race-compile:main once arm platform supported
FROM --platform=$TARGETPLATFORM ghcr.io/tst-race/race-images/race-runtime-linux:main as base
LABEL maintainer="Two Six Labs <race@twosixlabs.com>"

ARG TARGETARCH
RUN echo "building for architecture $TARGETARCH"

# Copy locally-built artifacts into the image
# Note: If you are adding new artifacts you may
#       need to update the .dockerignore file
#       so that they are included in the docker
#       context. Otherwise the COPY may fail.


# x86-64 linux
FROM base as amd64-linux
COPY racesdk/package/LINUX_x86_64/lib /linux/x86_64/lib
# RUN ln -s /linux/x86_64/lib/libraceboat.so /linux/x86_64/lib/libraceSdkCommon.so
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/linux/x86_64/lib" 
COPY build/LINUX_x86_64/app/race-cli/race-cli /usr/local/bin/race-cli
COPY build/LINUX_x86_64/app/bridge-distro/bridge-distro /usr/local/bin/bridge-distro

# preserve include structure assumed by go SWIG
RUN mkdir -p /linux/x86_64/include/race/common
COPY racesdk/package/LINUX_x86_64/include /linux/x86_64/include/race/common
RUN cp /linux/x86_64/include/race/common/Race.h /linux/x86_64/include/race/
COPY racesdk/package/LINUX_x86_64/go /usr/local/go

# arm64 linux
FROM base as arm64-linux
COPY racesdk/package/LINUX_arm64-v8a/lib /linux/arm64-v8a/lib
# RUN ln -s /linux/arm64-v8a/lib/libraceboat.so /linux/arm64-v8a/lib/libraceSdkCommon.so 
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/linux/arm64-v8a/lib"
COPY build/LINUX_arm64-v8a/app/race-cli/race-cli/ /usr/local/bin/race-cli
COPY build/LINUX_arm64-v8a/app/bridge-distro/bridge-distro/ /usr/local/bin/bridge-distro

# preserve include structure assumed by go SWIG
RUN mkdir -p /linux/arm64-v8a/include/race/common
COPY racesdk/package/LINUX_arm64-v8a/include /linux/arm64-v8a/include/race/common
RUN cp /linux/arm64-v8a/include/race/common/Race.h /linux/arm64-v8a/include/race/
COPY racesdk/package/LINUX_arm64-v8a/go /usr/local/go


FROM ${TARGETARCH}-linux
COPY racesdk/package/race-cmake-modules /opt/race/race-cmake-modules

RUN apt-get update && apt-get install ncat

