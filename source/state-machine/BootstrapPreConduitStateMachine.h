
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

namespace Raceboat {

class BootstrapPreConduitContext : public ApiContext {
public:
  BootstrapPreConduitContext(ApiManagerInternal &manager, StateEngine &engine)
    : ApiContext(manager, engine),
      initSendConnSMHandle(NULL_RACE_HANDLE),
      initRecvConnSMHandle(NULL_RACE_HANDLE),
      finalSendConnSMHandle(NULL_RACE_HANDLE),
      finalRecvConnSMHandle(NULL_RACE_HANDLE) {}

  virtual void updateBootstrapPreConduitStateMachineStart(
      RaceHandle contextHandle,
      const ApiBootstrapListenContext &parentContext,
      // RaceHandle recvHandle,
      // const ConnectionID &_recvConnId, const ChannelId &_recvChannel,
      // const ChannelId &_sendChannel, const std::string &_sendRole,
      // const std::string &_sendLinkAddress,
      const std::string &_packageId,
      std::vector<std::vector<uint8_t>> recvMessages) override;
  virtual void
  updateReceiveEncPkg(ConnectionID connId,
                      std::shared_ptr<std::vector<uint8_t>> data) override;

  virtual void
  updateConnStateMachineConnected(RaceHandle contextHandle, ConnectionID connId,
                                  std::string linkAddress) override;
  virtual void
  updateListenAccept(std::function<void(ApiStatus, RaceHandle, ConduitProperties)> cb) override;

public:
  std::vector<std::vector<uint8_t>> recvQueue;
  std::function<void(ApiStatus, RaceHandle, ConduitProperties)> acceptCb;

  RaceHandle parentHandle;

  BootstrapConnectionOptions opts;
  
  RaceHandle initSendConnSMHandle;
  LinkAddress initSendLinkAddress;
  ConnectionID initSendConnId;

  RaceHandle initRecvConnSMHandle;
  LinkAddress initRecvLinkAddress;
  ConnectionID initRecvConnId;

  RaceHandle finalSendConnSMHandle;
  LinkAddress finalSendLinkAddress;
  ConnectionID finalSendConnId;

  RaceHandle finalRecvConnSMHandle;
  LinkAddress finalRecvLinkAddress;
  ConnectionID finalRecvConnId;

  std::string packageId;
  RaceHandle apiHandle;
};

class BootstrapPreConduitStateEngine : public StateEngine {
public:
  BootstrapPreConduitStateEngine();
  virtual std::string eventToString(EventType event);
};

using BootstrapPreConduitState = BaseApiState<BootstrapPreConduitContext>;

} // namespace Raceboat
