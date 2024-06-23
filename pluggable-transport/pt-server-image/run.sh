#!/bin/bash

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


RACEBOAT_HOME=$(dirname $(dirname $(pwd)))
PLUGIN_PATH=$1 # e.g. /path/to/private-race-core/plugin-comms-twosix-cpp/kit/artifacts/linux-x86_64-server/

docker run --rm --name=pt-server -it \
       --network=rib-overlay-network --ip=10.11.1.2 \
       -e OR_PORT=2345 -p 2345:2345 \
       -e PT_PORT=5432 \
       -v $RACEBOAT_HOME:/code \
       -v $RACEBOAT_HOME/pluggable-transport/pt-server-image/bridge-mount/:/bridge-mount \
       -v $RACEBOAT_HOME/pluggable-transport/log/bridge/:/log \
       -v $RACEBOAT_HOME/pluggable-transport/pt-server-image/etc-race/:/etc/race/ \
       -v $RACEBOAT_HOME/racesdk/package/LINUX_x86_64/:/usr/local/bin/race/ \
       -v $PLUGIN_PATH/:/etc/race/plugins \
       -u root pt-server bash -c "/bridge-mount/start-tor.sh"

