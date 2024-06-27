
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
#include <memory> // shared_ptr
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../include/race/Race.h"
#include "../../include/race/unified/SdkResponse.h"
#include "../state-machine/ApiContext.h"
#include "../state-machine/ConduitStateMachine.h"
#include "../state-machine/ConnectionStateMachine.h"
#include "../state-machine/DialStateMachine.h"
#include "../state-machine/ResumeStateMachine.h"
#include "../state-machine/ListenStateMachine.h"
#include "../state-machine/PreConduitStateMachine.h"
#include "../state-machine/BootstrapDialStateMachine.h"
#include "../state-machine/BootstrapListenStateMachine.h"
#include "../state-machine/BootstrapPreConduitStateMachine.h"
#include "../state-machine/ReceiveStateMachine.h"
#include "../state-machine/SendReceiveStateMachine.h"
#include "../state-machine/SendStateMachine.h"
#include "Handler.h"
#include "race/common/ChannelProperties.h"
#include "race/common/ConnectionStatus.h"
#include "race/common/LinkProperties.h"
#include "race/common/LinkStatus.h"
#include "race/common/PackageStatus.h"
#include "race/common/PluginResponse.h"
#include "race/common/RaceHandle.h"

class EncPkg;

// ApiManager drives several state engines (connection, send, recv, send-reply,
// listen, dial, pre-conn-obj, conn-obj). A conn state machine context is
// created by the startConnStateMachine() call which gets called by the other
// state machines. Pre-conn-obj is created by a listen state machine after
// receiving a message from a client. It turns into a conn-obj state machine
// after accept is called. The other state machines are created directely by
// calls from the app.

// The manager is responsible to forwarding events to the appropriate state
// machine and context

namespace Raceboat {

static const int packageIdLen = 16;

class PluginContainer;

class ApiManager;
class BootstrapPreConduitContext;
enum class ActivateChannelStatusCode;

class ApiManagerInternal {
public:
  explicit ApiManagerInternal(Core &_core, ApiManager &manager);

  // Library Api calls
  virtual void send(uint64_t postId, SendOptions sendOptions,
                    std::vector<uint8_t> data,
                    std::function<void(ApiStatus)> callback);
  virtual void
  sendReceive(uint64_t postId, SendOptions sendOptions,
              std::vector<uint8_t> data,
              std::function<void(ApiStatus, std::vector<uint8_t>)> callback);
  virtual void dial(uint64_t postId, SendOptions sendOptions,
                    std::vector<uint8_t> data,
                    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback);
  virtual void resume(uint64_t postId, ResumeOptions resumeOptions,
                    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback);
  virtual void bootstrapDial(uint64_t postId, BootstrapConnectionOptions options,
                    std::vector<uint8_t> data,
                    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback);
  virtual void getReceiveObject(
      uint64_t postId, ReceiveOptions recvOptions,
      std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback);
  virtual void
  receive(uint64_t postId, OpHandle handle,
          std::function<void(ApiStatus, std::vector<uint8_t>)> callback);
  virtual void receiveRespond(
      uint64_t postId, OpHandle handle,
      std::function<void(ApiStatus, std::vector<uint8_t>, LinkAddress)>
          callback);

  virtual void
  listen(uint64_t postId, ReceiveOptions recvOptions,
         std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback);
  virtual void
  bootstrapListen(uint64_t postId, BootstrapConnectionOptions options,
         std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback);
  virtual void accept(uint64_t postId, OpHandle handle,
                      std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback);

  virtual void
  read(uint64_t postId, OpHandle handle,
       std::function<void(ApiStatus, std::vector<uint8_t>)> callback);
  virtual void write(uint64_t postId, OpHandle handle,
                     std::vector<uint8_t> bytes,
                     std::function<void(ApiStatus)> callback);
  virtual void close(uint64_t postId, OpHandle handle,
                     std::function<void(ApiStatus)> callback);
  virtual void
  cancelEvent(uint64_t postId, OpHandle handle,
             std::function<void(ApiStatus, std::vector<uint8_t>)> callback);

  // state machine callbacks
  virtual ActivateChannelStatusCode activateChannel(ApiContext &context,
                                                    RaceHandle handle,
                                                    ChannelId channelId,
                                                    std::string role);
  virtual void stateMachineFailed(ApiContext &context);
  virtual void stateMachineFinished(ApiContext &context);
  virtual void connStateMachineConnected(RaceHandle contextHandle,
                                         ConnectionID connId,
                                         std::string linkAddress,
                                         std::string channelId);

  virtual void onStateMachineFailed(uint64_t postId, RaceHandle contextHandle);
  virtual void onStateMachineFinished(uint64_t postId,
                                      RaceHandle contextHandle);
  virtual void onConnStateMachineConnected(uint64_t postId,
                                           RaceHandle contextHandle,
                                           ConnectionID connId,
                                           std::string linkAddress,
                                           std::string channelId);

  virtual void onChannelStatusChangedForContext(
      uint64_t postId, RaceHandle contextHandle, RaceHandle callHandle,
      const ChannelId &channelGid, ChannelStatus status,
      const ChannelProperties &properties);
  virtual void onConnStateMachineConnectedForContext(
      uint64_t postId, RaceHandle contextHandle, RaceHandle callHandle,
      RaceHandle connContextHandle, ConnectionID connId, std::string linkAddress);
  // Plugin callbacks
  virtual void onChannelStatusChanged(uint64_t postId, RaceHandle handle,
                                      const ChannelId &channelGid,
                                      ChannelStatus status,
                                      const ChannelProperties &properties);
  virtual void onLinkStatusChanged(uint64_t postId, RaceHandle handle,
                                   const LinkID &linkId, LinkStatus status,
                                   const LinkProperties &properties);
  virtual void onConnectionStatusChanged(uint64_t postId, RaceHandle handle,
                                         const ConnectionID &connId,
                                         ConnectionStatus status,
                                         const LinkProperties &properties);
  virtual void receiveEncPkg(uint64_t postId, const EncPkg &pkg,
                             const std::vector<ConnectionID> &connIDs);
  virtual void onPackageStatusChanged(uint64_t postId, RaceHandle handle,
                                      PackageStatus status);

  // connection state triggers
  virtual RaceHandle startConnStateMachine(RaceHandle contextHandle,
                                           ChannelId channelId,
                                           std::string role,
                                           std::string linkAddress,
                                           bool creating,
                                           bool sending);
  virtual RaceHandle startConduitectStateMachine(
      RaceHandle contextHandle, RaceHandle recvHandle,
      const ConnectionID &recvConnId, RaceHandle sendHandle,
      const ConnectionID &sendConnId, const ChannelId &sendChannel,
      const ChannelId &recvChannel, const std::string &packageId,
      std::vector<std::vector<uint8_t>> recvMessages, RaceHandle apiHandle);
  virtual RaceHandle startPreConduitStateMachine(
      RaceHandle contextHandle, RaceHandle recvHandle,
      const ConnectionID &recvConnId, const ChannelId &recvChannel,
      const ChannelId &sendChannel, const std::string &sendRole,
      const std::string &sendLinkAddress, const std::string &packageId,
      std::vector<std::vector<uint8_t>> recvMessages);
  virtual RaceHandle startBootstrapPreConduitStateMachine(
      RaceHandle contextHandle, 
      const ApiBootstrapListenContext &listenContext,
      // const ConnectionID &initSendConnId, const ChannelId &initSendChannel, const std::string &initSendRole,
      // const ConnectionID &initRecvConnId, const ChannelId &initRecvChannel, const std::string &initRecvRole,
      // const ConnectionID &finalSendConnId, const ChannelId &finalSendChannel, const std::string &finalSendRole, const LinkAddress &finalSendAddress
      // const ConnectionID &finalRecvConnId, const ChannelId &finalRecvChannel, const std::string &finalRecvRole, const LinkAddress &finalRecvAddress
      const std::string &packageId,
      std::vector<std::vector<uint8_t>> recvMessages);
  virtual bool
  onListenAccept(RaceHandle contextHandle,
                 std::function<void(ApiStatus, RaceHandle, ConduitProperties)> acceptCb);
  virtual bool
  onBootstrapListenAccept(RaceHandle contextHandle,
                          std::function<void(ApiStatus, RaceHandle, ConduitProperties)> acceptCb);
  virtual bool detachConnSM(RaceHandle contextHandle,
                            RaceHandle connSMContextHandle);
  virtual void addDependent(RaceHandle contextHandle, RaceHandle newDependentHandle);

  virtual Core &getCore();

  // register contexts
  virtual void registerHandle(ApiContext &context, RaceHandle handle);
  virtual void registerId(ApiContext &context, const std::string &id);
  virtual void registerPackageId(ApiContext &context,
                                 const ConnectionID &connId,
                                 const std::string &id);
  virtual void unregisterHandle(ApiContext &context, RaceHandle handle);
  virtual void removeLinkConn(ApiContext &/*context*/, std::string channelId, std::string linkAddress);

  void dumpContexts(std::string context="");  // debug
  using Contexts = std::unordered_set<ApiContext *>;

protected:
  virtual ApiContext *newSendContext();
  virtual ApiContext *newSendReceiveContext();
  virtual ApiContext *newRecvContext();
  virtual ApiContext *newDialContext();
  virtual ApiContext *newListenContext();
  virtual ApiContext *newConnContext();
  virtual ApiContext *newConduitectContext();
  virtual ApiContext *newPreConduitContext();
  virtual ApiContext *newBootstrapDialContext();
  virtual ApiContext *newBootstrapListenContext();
  virtual ApiContext *newBootstrapPreConduitContext();
  virtual ApiContext *newResumeContext();

  virtual Contexts getContexts(RaceHandle handle);
  virtual Contexts getContexts(const std::string &id);
  virtual Contexts getContextsByPackageId(const std::string &packageId);
  virtual Contexts getContexts(RaceHandle handle, const std::string &id);

  virtual void removeContext(ApiContext &context);

  virtual EventResult triggerEvent(ApiContext &context, EventType event);

public:
  Core &core;
  ApiManager &manager;

  // stateless state engines support multiple contexts simultaneously
  ConnStateEngine connEngine;
  ConduitStateEngine connObjectEngine;
  PreConduitStateEngine preConduitEngine;
  SendStateEngine sendEngine;
  SendReceiveStateEngine sendReceiveEngine;
  DialStateEngine dialEngine;
  ListenStateEngine listenEngine;
  RecvStateEngine recvEngine;
  BootstrapDialStateEngine bootstrapDialEngine;
  BootstrapListenStateEngine bootstrapListenEngine;
  BootstrapPreConduitStateEngine bootstrapPreConduitEngine;
  ResumeStateEngine resumeEngine;

  // channel, link, and connection IDs are strings, and will never conflict
  // map IDs and system event handles to the appropriate context
  std::unordered_map<RaceHandle, std::unique_ptr<ApiContext>> activeContexts;
  std::unordered_map<std::string, Contexts> idContextMap;
  std::unordered_map<RaceHandle, Contexts> handleContextMap;

  // packageId is a random identifier that should be globally unique
  std::unordered_map<std::string, Contexts> packageIdContextMap;
  std::unordered_map<ChannelId, std::pair<ChannelStatus, ChannelProperties>>
      activatedChannels;
  std::unordered_map<std::string, std::pair<RaceHandle, ConnectionID>> linkConnMap;

  std::unordered_map<std::string, std::vector<EncPkg>> unassociatedPackages;
};

class ApiManager {
public:
  explicit ApiManager(Core &_core);
  virtual ~ApiManager();

  // Library Api calls
  virtual SdkResponse send(SendOptions sendOptions, std::vector<uint8_t> data,
                           std::function<void(ApiStatus)> callback);
  virtual SdkResponse
  sendReceive(SendOptions sendOptions, std::vector<uint8_t> data,
              std::function<void(ApiStatus, std::vector<uint8_t>)> callback);
  virtual SdkResponse dial(SendOptions sendOptions, std::vector<uint8_t> data,
                           std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback);
  virtual SdkResponse resume(ResumeOptions resumeOptions,
                             std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback);
  virtual SdkResponse bootstrapDial(BootstrapConnectionOptions options, std::vector<uint8_t> data,
                           std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback);
  virtual SdkResponse getReceiveObject(
      ReceiveOptions recvOptions,
      std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback);
  virtual SdkResponse
  receive(OpHandle handle,
          std::function<void(ApiStatus, std::vector<uint8_t>)> callback);
  virtual SdkResponse receiveRespond(
      OpHandle handle,
      std::function<void(ApiStatus, std::vector<uint8_t>, LinkAddress)>
          callback);
  virtual SdkResponse
  listen(ReceiveOptions recvOptions,
         std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback);
  virtual SdkResponse
  bootstrapListen(BootstrapConnectionOptions options,
         std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback);
  virtual SdkResponse
  accept(OpHandle handle, std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback);

  virtual SdkResponse
  read(OpHandle handle,
       std::function<void(ApiStatus, std::vector<uint8_t>)> callback,
       int timeoutSeconds=BLOCKING_READ);
  virtual SdkResponse write(OpHandle handle, std::vector<uint8_t> bytes,
                            std::function<void(ApiStatus)> callback);
  virtual SdkResponse close(OpHandle handle,
                            std::function<void(ApiStatus)> callback);

  // State machine callbacks
  // These are queued on the work thread queue to prevent stack issues
  virtual SdkResponse onStateMachineFailed(RaceHandle contextHandle);
  virtual SdkResponse onStateMachineFinished(RaceHandle contextHandle);
  virtual SdkResponse onConnStateMachineConnected(RaceHandle contextHandle,
                                                  ConnectionID connId,
                                                  std::string linkAddress,
                                                  std::string channelId);
  virtual SdkResponse onChannelStatusChangedForContext(
      RaceHandle contextHandle, RaceHandle callHandle,
      const ChannelId &channelGid, ChannelStatus status,
      const ChannelProperties &properties);

  virtual SdkResponse onConnStateMachineConnectedForContext(
      RaceHandle contextHandle,
      RaceHandle callHandle,
      RaceHandle connContextHandle, ConnectionID connId, std::string linkAddress);


  // Plugin callbacks
  virtual SdkResponse
  onChannelStatusChanged(PluginContainer &plugin, RaceHandle handle,
                         const ChannelId &channelGid, ChannelStatus status,
                         const ChannelProperties &properties);
  virtual SdkResponse onLinkStatusChanged(PluginContainer &plugin,
                                          RaceHandle handle,
                                          const LinkID &linkId,
                                          LinkStatus status,
                                          const LinkProperties &properties);
  virtual SdkResponse
  onConnectionStatusChanged(PluginContainer &plugin, RaceHandle handle,
                            const ConnectionID &connId, ConnectionStatus status,
                            const LinkProperties &properties);
  virtual SdkResponse receiveEncPkg(PluginContainer &plugin, const EncPkg &pkg,
                                    const std::vector<ConnectionID> &connIDs);
  virtual SdkResponse onPackageStatusChanged(PluginContainer &plugin,
                                             RaceHandle handle,
                                             PackageStatus status);

  // For testing
  void waitForCallbacks();

  static const int BLOCKING_READ = -1;

protected:
  template <typename T, typename... Args>
  SdkResponse post(const std::string &logPrefix, T &&function, Args &&... args);

protected:
  Handler handler;
  ApiManagerInternal impl;
  std::atomic<uint64_t> nextPostId = 1;
};

} // namespace Raceboat
