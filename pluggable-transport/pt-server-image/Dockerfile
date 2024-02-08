
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

# Install dependencies to add Tor's repository.
RUN apt-get update && apt-get install -y \
    curl \
    gpg \
    gpg-agent \
    less \
    netcat \
    ca-certificates \
    libcap2-bin \
    apt-transport-https \
    --no-install-recommends

COPY tor.list /etc/apt/sources.list.d/tor.list
RUN wget -qO- https://deb.torproject.org/torproject.org/A3C4F0F979CAA22CDBA8F512EE8CBC9E886DDD89.asc | gpg --dearmor | tee /usr/share/keyrings/tor-archive-keyring.gpg >/dev/null && \
    apt-get update && apt-get install -y tor

RUN apt-get update && apt-get install -y \
    obfs4proxy

RUN chown debian-tor:debian-tor /etc/tor
RUN chown debian-tor:debian-tor /var/log/tor
# RUN mkdir -p /tor-logs/tor && chown debian-tor:debian-tor -R /tor-logs


COPY get-bridge-line /usr/local/bin
RUN chmod 0755 /usr/local/bin/get-bridge-line

RUN chmod -R 777 /etc/tor
RUN chmod -R 777 /usr/local/bin

RUN mkdir -p /var/lib/tor/.ssh && \
    touch /var/lib/tor/.ssh/known_hosts && \
    chown -R debian-tor:debian-tor /var/lib/tor/.ssh

RUN mkdir -p /etc/race/logging/core/ && chmod -R 777 /etc/race/logging/core/

USER debian-tor

CMD [ "/usr/local/bin/start-tor.sh" ]
