#!/usr/bin/env bash

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


NICK=${NICKNAME:-DockerObfs4Bridge}

echo "Using NICKNAME=${NICK}, OR_PORT=${OR_PORT}, PT_PORT=${PT_PORT}, and EMAIL=${EMAIL}."

ADDITIONAL_VARIABLES_PREFIX="OBFS4V_"
ADDITIONAL_VARIABLES=

if [[ "$OBFS4_ENABLE_ADDITIONAL_VARIABLES" == "1" ]]
then
    ADDITIONAL_VARIABLES="# Additional properties from processed '$ADDITIONAL_VARIABLES_PREFIX' environment variables"
    echo "Additional properties from '$ADDITIONAL_VARIABLES_PREFIX' environment variables processing enabled"

    IFS=$'\n'
    for V in $(env | grep "^$ADDITIONAL_VARIABLES_PREFIX"); do
        VKEY_ORG="$(echo $V | cut -d '=' -f1)"
        VKEY="${VKEY_ORG#$ADDITIONAL_VARIABLES_PREFIX}"
        VVALUE="$(echo $V | cut -d '=' -f2)"
        echo "Overriding '$VKEY' with value '$VVALUE'"
        ADDITIONAL_VARIABLES="$ADDITIONAL_VARIABLES"$'\n'"$VKEY $VVALUE"
    done
fi

cat > /etc/tor/torrc << EOF
RunAsDaemon 0
# We don't need an open SOCKS port.
SocksPort 0
BridgeRelay 1
Nickname ${NICK}
Log debug file /log/tor/bridge.log
Log notice stdout
# ServerTransportPlugin obfs4 exec /usr/bin/obfs4proxy --enableLogging --logLevel=DEBUG
# ServerTransportListenAddr race_pt3 0.0.0.0:${PT_PORT}

# Turns into TOR_PT_SERVER_TRANSPORTS
ServerTransportPlugin race_pt3 exec /code/pluggable-transport/plugin/artifacts/linux-x86_64-server/RaceDispatcher/raceDispatcher
# ServerTransportPlugin raceproxy exec /pt-dispatcher/DispatcherGolang/dispatcherGolang 
ExtORPort auto
DataDirectory /log/tor/

# The variable "OR_PORT" is replaced with the OR port.
ORPort ${OR_PORT}

# The variable "PT_PORT" is replaced with the obfs4 port.
# ServerTransportListenAddr obfs4 0.0.0.0:${PT_PORT}

# # Turns into TOR_PT_SERVER_BINDADDR?
ServerTransportListenAddr race_pt3 0.0.0.0:${PT_PORT} 
ServerTransportOptions race_pt3 recvLinkAddress={"hostname":"172.17.0.2","port":26262} send=twoSixDirectCpp recv=twoSixDirectCpp
# ServerTransportOptions raceproxy address={"service":{"host_address":"race.email.str","use_ssl":true,"out_port":465,"in_port":993},"account":{"login_user":"race-server-2@racemail.com","login_pass":"Password12345!"},"user":{"to_user":[],"user_type":"moderate","poll_time":5.0,"image_limit":1,"send_timeout":5.0,"disable_network_flow":true},"encoding":{"model":"Waterfall128_Glow_Spread_NoECC_MB1","qsize":100,"image_out":{"height":128,"width":128,"pixel_depth":8}}} send=twoSixDirectCpp recv=strEmail
# # ServerTransportOptions raceproxy address={"service":{"host_address":"race.email.str","use_ssl":true,"out_port":465,"in_port":993},"account":{"login_user":"race-server-2@racemail.com","login_pass":"Password12345!"},"user":{"to_user":[],"user_type":"moderate","poll_time":5.0,"image_limit":1,"send_timeout":5.0,"disable_network_flow":true},"encoding":{"model":"Waterfall128_Glow_Spread_NoECC_MB1","qsize":100,"image_out":{"height":128,"width":128,"pixel_depth":8}}} send=strEmail recv=strEmail
  

# The variable "EMAIL" is replaced with the operator's email address.
ContactInfo ${EMAIL}

LearnCircuitBuildTimeout 0
CircuitBuildTimeout 900

$ADDITIONAL_VARIABLES
EOF

echo "Starting tor."
tor -f /etc/tor/torrc
