
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

class ApiBootstrapListenContext : public ApiContext {
public:
  ApiBootstrapListenContext(ApiManagerInternal &manager, StateEngine &engine)
    : ApiContext(manager, engine),
      initSendConnSMHandle(NULL_RACE_HANDLE),
      initRecvConnSMHandle(NULL_RACE_HANDLE),
      finalSendConnSMHandle(NULL_RACE_HANDLE),
      finalRecvConnSMHandle(NULL_RACE_HANDLE) {}
  ApiBootstrapListenContext(const ApiContext &context)
    : ApiContext(context.manager, context.engine),
      initSendConnSMHandle(NULL_RACE_HANDLE),
      initRecvConnSMHandle(NULL_RACE_HANDLE),
      finalSendConnSMHandle(NULL_RACE_HANDLE),
      finalRecvConnSMHandle(NULL_RACE_HANDLE) {}
  virtual void updateBootstrapListen(
      const BootstrapConnectionOptions &options,
      std::function<void(ApiStatus, LinkAddress, RaceHandle)> cb) override;
  virtual void
  updateAccept(RaceHandle handle,
               std::function<void(ApiStatus, RaceHandle, ConduitProperties)> cb) override;
  virtual void updateClose(RaceHandle handle,
                           std::function<void(ApiStatus)> cb) override;
  virtual void
  updateReceiveEncPkg(ConnectionID connId,
                      std::shared_ptr<std::vector<uint8_t>> data) override;
  virtual void
  updateConnStateMachineConnected(RaceHandle contextHandle, ConnectionID connId,
                                  std::string linkAddress) override;

public:
  BootstrapConnectionOptions opts;
  std::queue<std::shared_ptr<std::vector<uint8_t>>> data;
  std::function<void(ApiStatus, LinkAddress, RaceHandle)> listenCb;
  std::deque<std::function<void(ApiStatus, RaceHandle, ConduitProperties)>> acceptCb;
  std::function<void(ApiStatus)> closeCb;

  RaceHandle initSendConnSMHandle;
  ConnectionID initSendConnId;
  std::string initSendLinkAddress;
  RaceHandle initRecvConnSMHandle;
  ConnectionID initRecvConnId;
  std::string initRecvLinkAddress;

  RaceHandle finalSendConnSMHandle;
  ConnectionID finalSendConnId;
  std::string finalSendLinkAddress;
  RaceHandle finalRecvConnSMHandle;
  ConnectionID finalRecvConnId;
  std::string finalRecvLinkAddress;

  std::queue<RaceHandle> preBootstrapConduitSM;
};

class BootstrapListenStateEngine : public StateEngine {
public:
  BootstrapListenStateEngine();
  virtual std::string eventToString(EventType event);
};

using BootstrapListenState = BaseApiState<ApiBootstrapListenContext>;

} // namespace Raceboat
