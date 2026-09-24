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

#include "LinkEstablishment.h"
#include "api-managers/ApiManager.h"

namespace Raceboat {

// A single link/connection to establish, in terms of the ConnEstablishment
// primitive (Step 1-3) rather than the raw creating/sending bools each
// caller used to pick apart by hand. Every "channel" slot (Listen/Dial) and
// every bootstrap "initial"/"final" slot request now goes through the same
// Socket::establish() call site instead of choosing between
// startConnStateMachine/startConnStateMachineBidi independently.
struct SocketRequest {
  ChannelId channelId;
  std::string role;
  std::string linkAddress;
  ConnEstablishment establishment;
  LinkID existingLinkId = "";
};

class Socket {
public:
  // Starts the connection state machine appropriate for `req.establishment`
  // (genuinely bidirectional via startConnStateMachineBidi when merged,
  // directional via startConnStateMachine otherwise) and returns its handle.
  static RaceHandle establish(ApiManagerInternal &manager,
                             RaceHandle contextHandle,
                             const SocketRequest &req) {
    if (req.establishment.isBidi()) {
      return manager.startConnStateMachineBidi(
          contextHandle, req.channelId, req.role, req.linkAddress,
          req.establishment.isCreator(), req.existingLinkId);
    }
    return manager.startConnStateMachine(
        contextHandle, req.channelId, req.role, req.linkAddress,
        req.establishment.isCreator(), req.establishment.isSend(),
        req.existingLinkId);
  }
};

} // namespace Raceboat
