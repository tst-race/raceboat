
// Copyright 2023 Two Six Technologies
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 

#pragma once

#include <unordered_set>

#include "ApiContext.h"

namespace RaceLib {

class ApiConnContext : public ApiContext {
public:
    ApiConnContext(ApiManagerInternal &manager, StateEngine &engine) :
        ApiContext(manager, engine) {}

    virtual void updateConnStateMachineStart(RaceHandle contextHandle, ChannelId channelId,
                                             std::string role, std::string linkAddress,
                                             bool sending) override;
    // virtual void updateConnStateMachineStop(RaceHandle /* contextHandle */) override;
    virtual void updateChannelStatusChanged(RaceHandle chanHandle, const ChannelId &channelGid,
                                            ChannelStatus status,
                                            const ChannelProperties &properties) override;
    virtual void updateLinkStatusChanged(RaceHandle linkHandle, const LinkID &linkId,
                                         LinkStatus status,
                                         const LinkProperties &properties) override;
    virtual void updateConnectionStatusChanged(RaceHandle connHandle, const ConnectionID &connId,
                                               ConnectionStatus status,
                                               const LinkProperties &properties) override;
    virtual void updateDependent(RaceHandle contextHandle) override;
    virtual void updateDetach(RaceHandle contextHandle) override;
    virtual void updateStateMachineFinished(RaceHandle contextHandle) override;
    virtual void updateStateMachineFailed(RaceHandle contextHandle) override;

public:
    std::unordered_set<RaceHandle> dependents;
    RaceHandle newestDependent = NULL_RACE_HANDLE;
    RaceHandle detachedDependent = NULL_RACE_HANDLE;
    bool send = false;
    ChannelId channelId;
    std::string channelRole;
    std::string linkAddress;
    std::string updatedLinkAddress;
    LinkID linkId;
    ConnectionID connId;
    bool connected = false;
};

class ConnStateEngine : public StateEngine {
public:
    ConnStateEngine();
    virtual std::string eventToString(EventType event);
};

using ConnState = BaseApiState<ApiConnContext>;

}  // namespace RaceLib
