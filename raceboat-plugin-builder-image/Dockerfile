
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
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/linux/x86_64/lib" 
COPY racesdk/package/LINUX_x86_64/include /linux/x86_64/include/
COPY racesdk/package/LINUX_x86_64/go /usr/local/go
RUN cp /usr/local/go/src/shims/comm/* /usr/local/go/src/shims/

# arm64 linux
FROM base AS arm64-linux
COPY racesdk/package/LINUX_arm64-v8a/lib /linux/arm64-v8a/lib
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/linux/arm64-v8a/lib"
COPY racesdk/package/LINUX_arm64-v8a/include /linux/arm64-v8a/include/
COPY racesdk/package/LINUX_arm64-v8a/go /usr/local/go
RUN cp /usr/local/go/src/shims/comm/* /usr/local/go/src/shims/

FROM ${TARGETARCH}-linux
COPY race-cmake-modules /opt/race/race-cmake-modules

# Installing PluginCommsTwoSixStubDecomposed and Destini dependencies at system level
# TODO: refactor those plugins to bring their own dependencies with them
RUN apt-get update && yes | apt-get install -y \
    libboost-all-dev \
    libthrift-dev \
    libjpeg62-dev \
    libjsoncpp-dev \
    libboost-all-dev \
    libcpprest-dev \
    libcurl4-openssl-dev \
    libevent-dev \
    libxml2-dev \
    parallel


# Installing older ffmpeg version directly from the RACE external build to avoid _potential_ versioning issues in Destini video encoding
ARG FFMPEG_VERSION=4.2.2-1
ARG TARGETPLATFORM="linux/amd64"
RUN case ${TARGETPLATFORM} in \
        "linux/amd64") LINUX_ARCH=x86_64    ;; \
        "linux/arm64") LINUX_ARCH=arm64-v8a ;; \
    esac && \
    wget \
        --output-document=/tmp/ffmpeg-${FFMPEG_VERSION}-linux-${LINUX_ARCH}.tar.gz \
        https://github.com/tst-race/ext-ffmpeg/releases/download/${FFMPEG_VERSION}/ffmpeg-${FFMPEG_VERSION}-linux-${LINUX_ARCH}.tar.gz && \
    tar --extract \
        --file=/tmp/ffmpeg-${FFMPEG_VERSION}-linux-${LINUX_ARCH}.tar.gz \
        --directory=/usr/local && \
    rm /tmp/ffmpeg-*.tar.gz