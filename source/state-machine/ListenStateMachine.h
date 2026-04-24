
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

#include <unordered_map>
#include "ApiContext.h"

namespace Raceboat {

class ApiListenContext : public ApiContext {
public:
  ApiListenContext(ApiManagerInternal &manager, StateEngine &engine)
      : ApiContext(manager, engine) {}
  virtual void updateListen(
      const ReceiveOptions &recvOptions,
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
                                  std::string linkAddress, LinkID linkId) override;

public:
  ReceiveOptions opts;
  std::queue<std::shared_ptr<std::vector<uint8_t>>> data;
  std::function<void(ApiStatus, LinkAddress, RaceHandle)> listenCb;
  std::deque<std::function<void(ApiStatus, RaceHandle, ConduitProperties)>> acceptCb;
  std::function<void(ApiStatus)> closeCb;

  // Legacy single connection handle (for backwards compatibility)
  RaceHandle recvConnSMHandle;
  ConnectionID recvConnId;
  std::string recvLinkAddress;

  // Support for multiple accept() calls - each creates a connection SM
  std::queue<RaceHandle> pendingConnSMHandles;
  std::unordered_map<RaceHandle, std::function<void(ApiStatus, RaceHandle, ConduitProperties)>> 
      connSMToAcceptCallback;
  
  // Store channel info for creating new connections on each accept()
  ChannelId recvChannelId;
  std::string recvRole;
  LinkAddress recvLinkAddressStored;
  
  // Store the LinkID from the first connection - all accepts share this link
  LinkID firstLinkId;

  std::queue<RaceHandle> preConduitSM;
};

class ListenStateEngine : public StateEngine {
public:
  ListenStateEngine();
  virtual std::string eventToString(EventType event);
};

using ListenState = BaseApiState<ApiListenContext>;

} // namespace Raceboat
