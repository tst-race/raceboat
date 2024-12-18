
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

FROM --platform=$TARGETPLATFORM ghcr.io/tst-race/raceboat/raceboat-builder:latest AS base
LABEL maintainer="Two Six Labs <race@twosixlabs.com>"

ARG TARGETARCH
RUN echo "building for architecture $TARGETARCH"

# x86-64 linux
FROM base AS amd64-linux
COPY racesdk/package/LINUX_x86_64/lib /linux/x86_64/lib
# RUN ln -s /linux/x86_64/lib/libraceboat.so /linux/x86_64/lib/libraceSdkCommon.so
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/linux/x86_64/lib" 
COPY racesdk/package/LINUX_x86_64/include /linux/x86_64/include/
COPY racesdk/package/LINUX_x86_64/go /usr/local/go
RUN cp /usr/local/go/src/shims/comm/* /usr/local/go/src/shims/

# arm64 linux
FROM base AS arm64-linux
COPY racesdk/package/LINUX_arm64-v8a/lib /linux/arm64-v8a/lib
# RUN ln -s /linux/arm64-v8a/lib/libraceboat.so /linux/arm64-v8a/lib/libraceSdkCommon.so 
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/linux/arm64-v8a/lib"
COPY racesdk/package/LINUX_arm64-v8a/include /linux/arm64-v8a/include/
COPY racesdk/package/LINUX_arm64-v8a/go /usr/local/go
RUN cp /usr/local/go/src/shims/comm/* /usr/local/go/src/shims/


FROM ${TARGETARCH}-linux
COPY race-cmake-modules /opt/race/race-cmake-modules


