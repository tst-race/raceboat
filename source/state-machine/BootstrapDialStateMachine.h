
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

class ApiBootstrapDialContext : public ApiContext {
public:
  ApiBootstrapDialContext(ApiManagerInternal &manager, StateEngine &engine)
      : ApiContext(manager, engine) {}

  virtual void
  updateBootstrapDial(const BootstrapConnectionOptions &options, std::vector<uint8_t> &&data,
             std::function<void(ApiStatus, RaceHandle)> cb) override;

  virtual void
  updateConnStateMachineConnected(RaceHandle contextHandle, ConnectionID connId,
                                  std::string linkAddress) override;
  
  virtual void
  updateReceiveEncPkg(ConnectionID connId,
                      std::shared_ptr<std::vector<uint8_t>> data) override;

public:
  BootstrapConnectionOptions opts;
  std::vector<uint8_t> helloData;
  std::queue<std::shared_ptr<std::vector<uint8_t>>> responseData;
  std::function<void(ApiStatus, RaceHandle)> dialCallback;

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

  std::string packageId;
};

class BootstrapDialStateEngine : public StateEngine {
public:
  BootstrapDialStateEngine();
  virtual std::string eventToString(EventType event);
};

using BootstrapDialState = BaseApiState<ApiBootstrapDialContext>;

} // namespace Raceboat
