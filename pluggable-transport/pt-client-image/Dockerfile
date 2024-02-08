
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

# Base docker image
FROM raceboat:latest

# ENV VERSION=11.0.9
ENV VERSION=13.5a1
RUN useradd -m -d /home/anon anon

WORKDIR /home/anon
RUN apt-get update && apt-get install -y \
    curl \
    less \
    netcat \
    firefox # to get all the dependencies of the TorBrowser
    
RUN \
    curl -sSL -o /home/anon/tor.tar.xz \
      https://www.torproject.org/dist/torbrowser/${VERSION}/tor-browser-linux-x86_64-${VERSION}.tar.xz && \
    curl -sSL -o /home/anon/tor.tar.xz.asc \
      https://www.torproject.org/dist/torbrowser/${VERSION}/tor-browser-linux-x86_64-${VERSION}.tar.xz.asc && \
    tar xvf /home/anon/tor.tar.xz && \
    rm -f /home/anon/tor.tar.xz*

    
RUN mkdir -p /home/anon/Downloads && \
    mkdir -p /home/anon/.ssh && \
    touch /home/anon/.ssh/known_hosts && \
    mkdir -p /home/anon/tor-browser_en-US/Browser/.ssh/ && \
    touch /home/anon/tor-browser_en-US/Browser/.ssh/known_hosts && \
    chown -R anon:anon /home/anon && \
    apt-get autoremove

# COPY racesdk/package/LINUX_x86_64/lib/libraceboat.so /linux/x86_64/lib/libraceSdkCommon.so

RUN mkdir -p /etc/race/logging/core/ && chown -R anon /etc/race/

COPY torrc-defaults /home/anon/tor-browser/Browser/TorBrowser/Data/Tor/torrc-defaults
USER anon

# Bridge obfs4 10.11.0.19:5432 F0D8DDD5459FD10555FAE0D31EADBA21C526567C cert=7RLDVRwzn2kzffFsOlRCgk5yt/01m288G5769W6DY+qBrAso28Xav9xN1hn4OroKyMkIWg iat-mode=0
# CircuitBuildTimeout 900
# ClientOnionAuthDir /home/anon/tor-browser_en-US/Browser/TorBrowser/Data/Tor/onion-auth
# DataDirectory /home/anon/tor-browser_en-US/Browser/TorBrowser/Data/Tor
# GeoIPFile /home/anon/tor-browser_en-US/Browser/TorBrowser/Data/Tor/geoip
# GeoIPv6File /home/anon/tor-browser_en-US/Browser/TorBrowser/Data/Tor/geoip6
# LearnCircuitBuildTimeout 0
# Log debug file /log/tordebug.log
# MaxCircuitDirtiness 9000
# NewCircuitPeriod 9000
# PathsNeededToBuildCircuits 0.250000
# UseBridges 1

# CMD /home/anon/tor-browser_en-US/Browser/start-tor-browser
