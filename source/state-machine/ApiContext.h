
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

#include <functional>
#include <string>
#include <vector>

#include "PluginContainer.h"
#include "StateMachine.h"
#include "race/Race.h"
#include "race/common/ChannelId.h"
#include "race/common/ChannelProperties.h"
#include "race/common/ChannelStatus.h"
#include "race/common/ConnectionStatus.h"
#include "race/common/LinkProperties.h"
#include "race/common/LinkStatus.h"
#include "race/common/PackageStatus.h"

namespace Raceboat {

class ApiManagerInternal;
class ApiBootstrapListenContext;
class PluginWrapper;

class ApiContext : public Context {
public:
  ApiContext(ApiManagerInternal &_manager, StateEngine &_engine);

  // prevent contexts from being copied accidentally
  ApiContext(const ApiContext &) = delete;

  bool shouldCreate(const ChannelId &channelId,
                    bool useForRecv);
  bool shouldCreateSender(const ChannelId &channelId);
  bool shouldCreateReceiver(const ChannelId &channelId);
   virtual void updateSend(const SendOptions & /* sendOptions */,
                          std::vector<uint8_t> && /* data */,
                          std::function<void(ApiStatus)> /* cb */){};
  virtual void updateSendReceive(
      const SendOptions & /* sendOptions */, std::vector<uint8_t> && /* data */,
      std::function<void(ApiStatus, std::vector<uint8_t>)> /* cb */) {}
  virtual void updateDial(const SendOptions & /* sendOptions */,
                          std::vector<uint8_t> && /* data */,
                          std::function<void(ApiStatus, RaceHandle)> /* cb */) {
  }
  virtual void updateBootstrapDial(const BootstrapConnectionOptions & /* options */,
                          std::vector<uint8_t> && /* data */,
                          std::function<void(ApiStatus, RaceHandle)> /* cb */) {
  }
  virtual void updateListen(
      const ReceiveOptions & /* recvOptions */,
      std::function<void(ApiStatus, LinkAddress, RaceHandle)> /* cb */) {}
  virtual void
  updateAccept(RaceHandle /* handle */,
               std::function<void(ApiStatus, RaceHandle)> /* cb */) {}
  virtual void updateBootstrapListen(
      const BootstrapConnectionOptions & /* options */,
      std::function<void(ApiStatus, LinkAddress, RaceHandle)> /* cb */) {}
  virtual void updateGetReceiver(
      const ReceiveOptions & /* recvOptions */,
      std::function<void(ApiStatus, LinkAddress, RaceHandle)> /* cb */){};
  virtual void updateReceive(
      RaceHandle /* handle */,
      std::function<void(ApiStatus, std::vector<uint8_t>)> /* cb */){};
  virtual void updateRead(
      RaceHandle /* handle */,
      std::function<void(ApiStatus, std::vector<uint8_t>)> /* cb */){};
  virtual void updateWrite(RaceHandle /* handle */, std::vector<uint8_t>,
                           std::function<void(ApiStatus)> /* cb */){};
  virtual void updateClose(RaceHandle /* handle */,
                           std::function<void(ApiStatus)> /* cb */){};

  virtual void updateChannelStatusChanged(
      RaceHandle /* chanHandle */, const ChannelId & /* channelGid */,
      ChannelStatus /* status */, const ChannelProperties & /* properties */){};
  virtual void
  updateLinkStatusChanged(RaceHandle /* linkHandle */,
                          const LinkID & /* linkId */, LinkStatus /* status */,
                          const LinkProperties & /* properties */){};
  virtual void updateConnectionStatusChanged(
      RaceHandle /* connHandle */, const ConnectionID & /* connId */,
      ConnectionStatus /* status */, const LinkProperties & /* properties */){};
  virtual void
      updateReceiveEncPkg(ConnectionID /* connId */,
                          std::shared_ptr<std::vector<uint8_t>> /* data */){};
  virtual void updatePackageStatusChanged(RaceHandle /* pkgHandle */,
                                          PackageStatus /* status */){};

  virtual void updateStateMachineFailed(RaceHandle /* contextHandle */){};
  virtual void updateStateMachineFinished(RaceHandle /* contextHandle */){};
  virtual void updateDependent(RaceHandle /* contextHandle */){};
  virtual void updateDetach(RaceHandle /* contextHandle */){};
  virtual void updateConnStateMachineConnected(RaceHandle /* contextHandle */,
                                               ConnectionID /* connId */,
                                               std::string /* linkAddress */){};
  virtual void updateConnStateMachineStart(RaceHandle /* contextHandle */,
                                           ChannelId /* channelId */,
                                           std::string /* role */,
                                           std::string /* linkAddress */,
                                           bool /* creating */,
                                           bool /* sending */){};
  virtual void updateConduitectStateMachineStart(
      RaceHandle /* contextHandle */, RaceHandle /* recvHandle */,
      const ConnectionID & /* recvConnId */, RaceHandle /* sendHandle */,
      const ConnectionID & /* sendConnId */,
      const ChannelId & /* sendChannel */, const ChannelId & /* recvChannel */,
      const std::string & /* packageId */,
      std::vector<std::vector<uint8_t>> /* recvMessages */,
      RaceHandle /* apiHandle */){};

  virtual void updatePreConduitStateMachineStart(
      RaceHandle /* contextHandle */, RaceHandle /* recvHandle */,
      const ConnectionID & /* _recvConnId */,
      const ChannelId & /* _recvChannel */,
      const ChannelId & /* _sendChannel */, const std::string & /* _sendRole */,
      const std::string & /* _sendLinkAddress */,
      const std::string & /* _packageId */,
      std::vector<std::vector<uint8_t>> /* recvMessages */){};

  virtual void updateBootstrapPreConduitStateMachineStart(
      RaceHandle /* contextHandle */,
      const ApiBootstrapListenContext &/* parentContext */,
      // RaceHandle /* recvHandle */,
      // const ConnectionID & /* _recvConnId */,
      // const ChannelId & /* _recvChannel */,
      // const ChannelId & /* _sendChannel */, const std::string & /* _sendRole */,
      // const std::string & /* _sendLinkAddress */,
      const std::string & /* _packageId */,
      std::vector<std::vector<uint8_t>> /* recvMessages */){};

  virtual void
  updateListenAccept(std::function<void(ApiStatus, RaceHandle)> /* cb */) {}

public:
  ApiManagerInternal &manager;
  StateEngine &engine;
  RaceHandle handle;
};

template <typename ContextType> struct BaseApiState : public State {
  BaseApiState(StateType id, std::string name) : State(id, name) {}

  ContextType &getContext(Context &context) {
    MAKE_LOG_PREFIX();
    ContextType *ctx = getDerivedContext<ContextType>(&context);
    if (ctx == nullptr) {
      helper::logError(logPrefix + " invalid context");
      throw std::logic_error("invalid context");
    }
    return *ctx;
  }

  PluginWrapper &getPlugin(ContextType &ctx, const std::string &channelId) {
    MAKE_LOG_PREFIX();
    PluginContainer *container = ctx.manager.getCore().getChannel(channelId);
    if (container == nullptr) {
      helper::logError(logPrefix + " invalid channel id");
      throw std::invalid_argument("invalid channel id");
    }

    // this should never happen
    if (container->plugin == nullptr) {
      helper::logError(logPrefix + " invalid plugin");
      throw std::logic_error("invalid plugin");
    }
    return *container->plugin;
  }
};

} // namespace Raceboat
