
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

FROM ubuntu:22.04

ARG TARGETARCH
ARG TARGETPLATFORM

RUN apt-get update && \
    apt-get install -y \
            ca-certificates \
            gpg \
            lsb-release \
            software-properties-common \
            wget

RUN for OS in linux android; do \
        for ARCH in x86_64 arm64-v8a; do \
            mkdir -p /${OS}/${ARCH}; \
        done; \
    done;
            
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | apt-key add - && \
    add-apt-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"

RUN apt-get update -y && apt-get install -y \
    cmake \
    clang-15 \
    clang-format-15 \
    clang-tidy-15

# Go
ARG GO_VERSION=1.23.0
RUN case ${TARGETPLATFORM} in \
        "linux/amd64") LINUX_ARCH=amd64 ;; \
        "linux/arm64") LINUX_ARCH=arm64 ;; \
    esac && \
    wget \
        --output-document=/tmp/go${GO_VERSION}.linux-${LINUX_ARCH}.tar.gz \
        https://go.dev/dl/go${GO_VERSION}.linux-${LINUX_ARCH}.tar.gz && \
    tar --extract \
        --file=/tmp/go${GO_VERSION}.linux-${LINUX_ARCH}.tar.gz \
        --directory=/usr/local && \
    rm /tmp/go*.tar.gz
ENV GOROOT=/usr/local/go \
    PATH=/usr/local/go/bin:$PATH

# ENV PATH=$PATH:/usr/lib/go-1.23/bin/

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-15 1 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-15 1

# SWIG (Linux-only)
ARG SWIG_VERSION=4.0.1-2
RUN case ${TARGETPLATFORM} in \
        "linux/amd64") LINUX_ARCH=x86_64    ;; \
        "linux/arm64") LINUX_ARCH=arm64-v8a ;; \
    esac && \
    wget \
        --output-document=/tmp/swig-${SWIG_VERSION}-linux-${LINUX_ARCH}.tar.gz \
        https://github.com/tst-race/ext-swig/releases/download/${SWIG_VERSION}/swig-${SWIG_VERSION}-linux-${LINUX_ARCH}.tar.gz && \
    tar --extract \
        --file=/tmp/swig-${SWIG_VERSION}-linux-${LINUX_ARCH}.tar.gz \
        --directory=/usr/local && \
    rm /tmp/swig-*.tar.gz

ENV DEBIAN_FRONTEND=noninteractive
ARG PYTHON_VERSION=3.12.7
RUN apt-get update -y && \
    add-apt-repository ppa:deadsnakes/ppa -y && \
    apt-get update -y && \
    apt-get install -y --no-install-recommends \
        python3.12=${PYTHON_VERSION}-* \
        python3.12-dev=${PYTHON_VERSION}-* \
        python3-pip \
        python3-setuptools \
        python3-wheel \
        python3.12-tk=${PYTHON_VERSION}-* \
        python3.12-distutils
        
RUN apt-get install -y \
    valgrind

ARG GOOGLETEST_VERSION=1.12.0-1
RUN for ARCH in x86_64 arm64-v8a; do \
        wget \
            --output-document=/tmp/googletest-${GOOGLETEST_VERSION}-linux-${ARCH}.tar.gz \
            https://github.com/tst-race/ext-googletest/releases/download/${GOOGLETEST_VERSION}/googletest-${GOOGLETEST_VERSION}-linux-${ARCH}.tar.gz && \
        tar --extract \
            --file=/tmp/googletest-${GOOGLETEST_VERSION}-linux-${ARCH}.tar.gz \
            --directory=/linux/${ARCH}; \
    done && \
    rm /tmp/googletest-*.tar.gz

# nlohmann-json
ARG NLOHMANN_JSON_VERSION=3.10.5-1
RUN for OS in linux android; do \
        for ARCH in x86_64 arm64-v8a; do \
            wget \
                --output-document=/tmp/nlohmann-json-${NLOHMANN_JSON_VERSION}-${OS}-${ARCH}.tar.gz \
                https://github.com/tst-race/ext-nlohmann-json/releases/download/${NLOHMANN_JSON_VERSION}/nlohmann-json-${NLOHMANN_JSON_VERSION}-${OS}-${ARCH}.tar.gz && \
            tar --extract \
                --file=/tmp/nlohmann-json-${NLOHMANN_JSON_VERSION}-${OS}-${ARCH}.tar.gz \
                --directory=/${OS}/${ARCH}; \
        done; \
    done && \
    rm /tmp/nlohmann-json-*.tar.gz

ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/linux/x86_64/lib:/linux/arm64-v8a/lib"

RUN apt-get install -y \
        lsb-release \
        openjdk-8-jdk \
        wget \
        unzip

# Install Android NDK
ARG ANDROID_TOOLS_VERSION=6609375
RUN mkdir -p /opt/android/cmdline-tools && \
    cd /opt/android/cmdline-tools && \
    wget https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_TOOLS_VERSION}_latest.zip && \
    unzip commandlinetools-*.zip && \
    rm commandlinetools-*.zip
ARG ANDROID_NDK_VERSION=23.2.8568313
RUN yes | /opt/android/cmdline-tools/tools/bin/sdkmanager --licenses && \
    /opt/android/cmdline-tools/tools/bin/sdkmanager --install "ndk;${ANDROID_NDK_VERSION}" && \
    ln -s /opt/android/ndk/${ANDROID_NDK_VERSION} /opt/android/ndk/default

ENV ANDROID_NDK=/opt/android/ndk/default \
    MIN_SDK_VERSION=34 \
    CC=clang \
    CXX=clang++ \
    PATH=${PATH}:/opt/android/cmdline-tools/tools/bin

ARG ANDROID_PYTHON_VERSION=3.7.16-3
RUN for ARCH in x86_64 arm64-v8a; do \
        mkdir -p /android/${ARCH} && \
        wget \
            --output-document=/tmp/python-${ANDROID_PYTHON_VERSION}-android-${ARCH}.tar.gz \
            https://github.com/tst-race/ext-python/releases/download/${ANDROID_PYTHON_VERSION}/python-${ANDROID_PYTHON_VERSION}-android-${ARCH}.tar.gz && \
        tar --extract \
            --file=/tmp/python-${ANDROID_PYTHON_VERSION}-android-${ARCH}.tar.gz \
            --directory=/android/${ARCH} && \
        wget \
            --output-document=/android/${ARCH}/python-packages-android-${ARCH}.tar.gz \
            https://github.com/tst-race/ext-python/releases/download/${ANDROID_PYTHON_VERSION}/python-packages-${ANDROID_PYTHON_VERSION}-android-${ARCH}.tar.gz; \
    done && \
    rm /tmp/python-*.tar.gz


COPY ./android-x86_64.toolchain.cmake /opt/android/ndk/default/
COPY ./android-arm64-v8a.toolchain.cmake /opt/android/ndk/default/
