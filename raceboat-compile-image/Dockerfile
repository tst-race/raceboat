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

RUN apt-get install -y \
    cmake \
    golang-1.23 \
    clang-15 \
    clang-format-15 \
    clang-tidy-15 

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
# PYTHON
# ARG PYTHON_VERSION=3.7.17
# RUN apt-get update -y && \
#     add-apt-repository ppa:deadsnakes/ppa -y && \
#     apt-get update -y && \
#     apt-get install -y --no-install-recommends \
#         python3.7=${PYTHON_VERSION}-* \
#         python3.7-dev=${PYTHON_VERSION}-* \
#         python3-pip \
#         python3-setuptools \
#         python3-wheel \
#         python3.7-tk=${PYTHON_VERSION}-* \
#         python3.7-distutils

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
