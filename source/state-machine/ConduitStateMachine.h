
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

#include <deque>
#include <queue>
#include <mutex>

#include "ApiContext.h"

namespace Raceboat {

class ConduitContext : public ApiContext {
public:
  ConduitContext(ApiManagerInternal &manager, StateEngine &engine)
      : ApiContext(manager, engine) {}

  virtual void updateConduitectStateMachineStart(
      RaceHandle contextHandle, RaceHandle recvHandle,
      const ConnectionID &recvConnId, RaceHandle sendHandle,
      const ConnectionID &sendConnId, const ChannelId &sendChannel,
      const ChannelId &recvChannel, const std::string &packageId,
      std::vector<std::vector<uint8_t>> recvMessages,
      RaceHandle apiHandle) override;
  virtual void
  updateReceiveEncPkg(ConnectionID connId,
                      std::shared_ptr<std::vector<uint8_t>> data) override;

  virtual void updatePackageStatusChanged(RaceHandle pkgHandle,
                                          PackageStatus status) override;

  virtual void
  updateRead(RaceHandle handle,
             std::function<void(ApiStatus, std::vector<uint8_t>)> cb) override;
  virtual void updateWrite(RaceHandle handle, std::vector<uint8_t> bytes,
                           std::function<void(ApiStatus)> cb) override;
  virtual void updateClose(RaceHandle handle,
                           std::function<void(ApiStatus)> cb) override;

public:
  // list of packages to send out and the callback to call once we get
  // PACKAGE_SENT
  std::deque<std::pair<std::function<void(ApiStatus)>, std::vector<uint8_t>>>
      sendQueue;

  // map of race handle to write callback to call when we get the
  // onPackageStatusChanged call
  std::unordered_map<RaceHandle, std::function<void(ApiStatus)>> sentQueue;

  // handles for PACKAGE_SENT events
  std::deque<RaceHandle> sentList;

  // handles for PACKAGE_FAILED events
  std::deque<RaceHandle> failedList;

  // packages received but not yet read
  std::queue<std::vector<uint8_t>> recvQueue;

  std::function<void(ApiStatus, RaceHandle)> dialCallback;
  std::function<void(ApiStatus)> closeCallback;

protected:
  // ensure readCallback is mutex'ed since its subject to being called on another thread due to timeout
  std::function<void(ApiStatus, std::vector<uint8_t>)> readCallback;
  std::mutex readCallbackMutex;

public:
  bool callReadCallback(const ApiStatus &status, const std::vector<uint8_t>& bytes);

public:
  RaceHandle sendConnSMHandle;
  ConnectionID sendConnId;

  RaceHandle recvConnSMHandle;
  ConnectionID recvConnId;

  ChannelId sendChannel;
  ChannelId recvChannel;

  std::string packageId;
  RaceHandle apiHandle;
};

class ConduitStateEngine : public StateEngine {
public:
  ConduitStateEngine();
  virtual std::string eventToString(EventType event);
};

using ConduitState = BaseApiState<ConduitContext>;

} // namespace Raceboat