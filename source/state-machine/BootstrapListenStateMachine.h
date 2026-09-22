
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
#include <unordered_set>
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
                                  std::string linkAddress, LinkID linkId) override;
  virtual void
  updateConnStateMachineLinkEstablished(RaceHandle contextHandle, LinkID linkId,
                                        std::string linkAddress) override;

public:
  BootstrapConnectionOptions opts;
  // Each queued hello is paired with the connId it arrived on, so it can be
  // matched back to the specific per-client connSM that received it (see
  // initRecvConnIdToHandle) instead of always using the first client's.
  std::queue<std::pair<ConnectionID, std::shared_ptr<std::vector<uint8_t>>>> data;
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
  bool finalRecvLinkReady = false;
  bool finalSendLinkReady = false;

  std::queue<RaceHandle> preBootstrapConduitSM;
  
  bool initUsingSingleBidiConnection = false;  // true if init uses one bidi connection
  bool finalUsingSingleBidiConnection = false;  // true if final uses one bidi connection

  // Support for multiple concurrent bootstrapping clients sharing the same
  // init-recv link: the first connection's LinkID is reused to open an
  // additional connection per extra accept(), mirroring ApiListenContext.
  LinkID firstInitRecvLinkId;
  bool initialInitRecvConnSMUsed = false;
  std::unordered_set<RaceHandle> initRecvConnSMHandles;
  std::queue<RaceHandle> pendingConnSMHandles;
  std::unordered_map<RaceHandle, std::function<void(ApiStatus, RaceHandle, ConduitProperties)>>
      connSMToAcceptCallback;
  // Maps each init-recv connId to the specific connSM handle that owns it,
  // so a per-client hello can be paired with its own connection (needed for
  // a merged single-bidi init link, where each client's connSM is also used
  // to send that client's hello response - see startBootstrapPreConduitStateMachine).
  std::unordered_map<ConnectionID, RaceHandle> initRecvConnIdToHandle;
};

class BootstrapListenStateEngine : public StateEngine {
public:
  BootstrapListenStateEngine();
  virtual std::string eventToString(EventType event);
};

using BootstrapListenState = BaseApiState<ApiBootstrapListenContext>;

} // namespace Raceboat
