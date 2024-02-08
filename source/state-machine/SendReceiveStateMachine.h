
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

#include "ApiContext.h"

namespace Raceboat {

class ApiSendReceiveContext : public ApiContext {
public:
    ApiSendReceiveContext(ApiManagerInternal &manager, StateEngine &engine) :
        ApiContext(manager, engine) {}

    virtual void updateSendReceive(
        const SendOptions &sendOptions, std::vector<uint8_t> &&data,
        std::function<void(ApiStatus, std::vector<uint8_t>)> cb) override;
    virtual void updateConnStateMachineConnected(RaceHandle contextHandle, ConnectionID connId,
                                                 std::string linkAddress) override;
    virtual void updateReceiveEncPkg(ConnectionID connId,
                                     std::shared_ptr<std::vector<uint8_t>> data) override;

public:
    SendOptions opts;
    std::vector<uint8_t> data;
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback;
    RaceHandle sendConnSMHandle;
    ConnectionID sendConnId;

    ConnectionID recvConnId;
    RaceHandle recvConnSMHandle;
    std::string recvLinkAddress;

    std::shared_ptr<std::vector<uint8_t>> receivedMsg;
};

class SendReceiveStateEngine : public StateEngine {
public:
    SendReceiveStateEngine();
    virtual std::string eventToString(EventType event);
};

using SendReceiveState = BaseApiState<ApiSendReceiveContext>;

}  // namespace Raceboat