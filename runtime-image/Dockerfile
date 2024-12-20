
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

FROM --platform=$TARGETPLATFORM ghcr.io/tst-race/raceboat/raceboat-compile:latest as base
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


FROM ${TARGETARCH}-linux
RUN apt-get update && yes | apt-get install ncat # for example / test / debug 
