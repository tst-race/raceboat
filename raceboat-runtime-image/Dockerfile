
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

FROM --platform=$TARGETPLATFORM ghcr.io/tst-race/raceboat/raceboat-plugin-builder:latest as base
LABEL maintainer="Two Six Labs <race@twosixlabs.com>"

ARG TARGETARCH
RUN echo "building for architecture $TARGETARCH"

# x86-64 linux
FROM base as amd64-linux
COPY racesdk/package/LINUX_x86_64/app/race-cli /usr/local/bin/race-cli
COPY racesdk/package/LINUX_x86_64/app/bridge-distro /usr/local/bin/bridge-distro

# arm64 linux
FROM base as arm64-linux
COPY racesdk/package/LINUX_arm64-v8a/app/race-cli /usr/local/bin/race-cli
COPY racesdk/package/LINUX_arm64-v8a/app/bridge-distro /usr/local/bin/bridge-distro

# Installing ncat for testing purposes
RUN apt-get update && yes | apt-get install ncat # for example / test / debug

# Installing PluginCommsTwoSixStubDecomposed and Destini dependencies at system level
# TODO: refactor those plugins to bring their own dependencies with them
FROM ${TARGETARCH}-linux
RUN apt-get update && yes | apt-get install -y \
    libboost-system1.74.0 \
    libboost-filesystem1.74.0 \
    libjsoncpp25 \
    libevent-2.1-7 \
    parallel \
    libjpeg62-dev \
    libavdevice-dev
    # ffmpeg \

COPY libflickcurl.so /usr/local/lib/libflickcurl.so
RUN pip install pillow psutil

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