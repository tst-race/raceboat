
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

#include "ApiManager.h"

#include <typeinfo>

#include "Core.h"
#include "PluginWrapper.h"
#include "base64.h"
#include "state-machine/Events.h"
#include "state-machine/BootstrapListenStateMachine.h"

namespace Raceboat {

ApiManager::ApiManager(Core &_core)
    : handler("api-mananger-thread", 0, 0), impl(_core, *this) {
  handler.create_queue("wait queue", std::numeric_limits<int>::min());
  handler.start();
}

ApiManager::~ApiManager() {
  TRACE_METHOD();
  handler.stop();
}

void ApiManager::waitForCallbacks() {
  auto [success, queueSize, future] = handler.post(
      "wait queue", 0, -1, [=] { return std::make_optional(true); });
  (void)success;
  (void)queueSize;
  future.wait();
}

template <typename T, typename... Args>
SdkResponse ApiManager::post(const std::string &logPrefix, T &&function,
                             Args &&... args) {
  uint64_t postHandle = nextPostId++;
  std::string postId = std::to_string(postHandle);
  helper::logDebug(logPrefix + "Posting postId: " + postId);

  try {
    auto workFunc = [logPrefix, postHandle, postId, function,
                     this](auto &... args) mutable {
      helper::logDebug(logPrefix + "Calling postId: " + postId);
      try {
        std::mem_fn(function)(impl, postHandle, std::move(args)...);
      } catch (std::exception &e) {
        helper::logError(logPrefix +
                         "Threw exception: " + std::string(e.what()));
      } catch (...) {
        helper::logError(logPrefix + "Threw unknown exception");
      }

      return std::make_optional(true);
    };

    auto [success, queueSize, future] = handler.post(
        "", 0, -1, std::bind(std::move(workFunc), std::forward<Args>(args)...));

    // this really shouldn't happen...
    if (success != Handler::PostStatus::OK) {
      helper::logError(logPrefix + "Post " + postId + " failed with error: " +
                       handlerPostStatusToString(success));
      return SDK_INVALID;
    }

    (void)queueSize;
    (void)future;
    return SDK_OK;
  } catch (std::out_of_range &e) {
    helper::logError(
        "default queue does not exist. This should never happen. what:" +
        std::string(e.what()));
    return SDK_INVALID;
  }
}

// Library Api calls
SdkResponse ApiManager::send(SendOptions sendOptions, std::vector<uint8_t> data,
                             std::function<void(ApiStatus)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::send, sendOptions, data,
              callback);
}

SdkResponse ApiManager::sendReceive(
    SendOptions sendOptions, std::vector<uint8_t> data,
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::sendReceive, sendOptions, data,
              callback);
}

SdkResponse
ApiManager::dial(SendOptions sendOptions, std::vector<uint8_t> data,
                 std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::dial, sendOptions, data,
              callback);
}

SdkResponse
ApiManager::resume(ResumeOptions resumeOptions, 
                 std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::resume, resumeOptions,
              callback);
}

SdkResponse
ApiManager::bootstrapDial(BootstrapConnectionOptions options, std::vector<uint8_t> data,
                 std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::bootstrapDial, options, data,
              callback);
}

SdkResponse ApiManager::getReceiveObject(
    ReceiveOptions recvOptions,
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::getReceiveObject, recvOptions,
              callback);
}

SdkResponse ApiManager::receive(
    OpHandle handle,
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::receive, handle, callback);
}

SdkResponse ApiManager::receiveRespond(
    OpHandle handle,
    std::function<void(ApiStatus, std::vector<uint8_t>, LinkAddress)>
        callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::receiveRespond, handle, callback);
}

SdkResponse ApiManager::listen(
    ReceiveOptions recvOptions,
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::listen, recvOptions, callback);
}

SdkResponse ApiManager::bootstrapListen(
    BootstrapConnectionOptions options,
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::bootstrapListen, options, callback);
}

SdkResponse
ApiManager::accept(OpHandle handle,
                   std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::accept, handle, callback);
}

SdkResponse ApiManager::read(
    OpHandle handle,
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback,
    int // timeoutSeconds
                             ) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  // if (timeoutSeconds != BLOCKING_READ) { 
  //   helper::logDebug(logPrefix + " NON-BLOCKING READ");
  //   std::thread timeout_thread([this, handle, callback, logPrefix, timeoutSeconds]() {
  //     std::promise<std::pair<ApiStatus, std::vector<uint8_t>>> promise;
  //     auto future = promise.get_future();
  //     if(std::future_status::ready != future.wait_for(std::chrono::seconds(timeoutSeconds))) {
  //           post(logPrefix, &ApiManagerInternal::cancelEvent, handle, nullptr);
  //         }
  //   });
  //   timeout_thread.detach();
  // }

  auto response = post(logPrefix, &ApiManagerInternal::read, handle, callback);
  handler.unblock_queue("");  // in case of timeout

  return response;
}

SdkResponse ApiManager::write(OpHandle handle, std::vector<uint8_t> bytes,
                              std::function<void(ApiStatus)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::write, handle, std::move(bytes),
              callback);
}

SdkResponse ApiManager::close(OpHandle handle,
                              std::function<void(ApiStatus)> callback) {
  TRACE_METHOD();

  if (!callback) {
    return SDK_INVALID_ARGUMENT;
  }

  return post(logPrefix, &ApiManagerInternal::close, handle, callback);
}

// internal callbacks
SdkResponse ApiManager::onStateMachineFailed(RaceHandle contextHandle) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::onStateMachineFailed,
              contextHandle);
}

SdkResponse ApiManager::onStateMachineFinished(RaceHandle contextHandle) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::onStateMachineFinished,
              contextHandle);
}

SdkResponse ApiManager::onConnectionStatusChanged(
    PluginContainer & /* plugin */, RaceHandle handle,
    const ConnectionID &connId, ConnectionStatus status,
    const LinkProperties &properties) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::onConnectionStatusChanged, handle,
              connId, status, properties);
}

SdkResponse ApiManager::onConnStateMachineConnected(RaceHandle contextHandle,
                                                    ConnectionID connId,
                                                    std::string linkAddress,
                                                    std::string channelId) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::onConnStateMachineConnected,
              contextHandle, connId, linkAddress, channelId);
}

SdkResponse ApiManager::onChannelStatusChangedForContext(
    RaceHandle contextHandle, RaceHandle callHandle,
    const ChannelId &channelGid, ChannelStatus status,
    const ChannelProperties &properties) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::onChannelStatusChangedForContext,
              contextHandle, callHandle, channelGid, status, properties);
}

SdkResponse ApiManager::onConnStateMachineConnectedForContext(
                                                            RaceHandle contextHandle, RaceHandle callHandle,
                                                            RaceHandle connContextHandle, ConnectionID connId, std::string linkAddress) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::onConnStateMachineConnectedForContext,
              contextHandle, callHandle, connContextHandle, connId, linkAddress);
}

// Plugin callbacks
SdkResponse ApiManager::onChannelStatusChanged(
    PluginContainer & /* plugin */, RaceHandle handle,
    const ChannelId &channelGid, ChannelStatus status,
    const ChannelProperties &properties) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::onChannelStatusChanged, handle,
              channelGid, status, properties);
}

SdkResponse ApiManager::onLinkStatusChanged(PluginContainer & /* plugin */,
                                            RaceHandle handle,
                                            const LinkID &linkId,
                                            LinkStatus status,
                                            const LinkProperties &properties) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::onLinkStatusChanged, handle,
              linkId, status, properties);
}

SdkResponse
ApiManager::receiveEncPkg(PluginContainer & /* plugin */, const EncPkg &pkg,
                          const std::vector<ConnectionID> &connIDs) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::receiveEncPkg, pkg, connIDs);
}

SdkResponse ApiManager::onPackageStatusChanged(PluginContainer & /* plugin */,
                                               RaceHandle handle,
                                               PackageStatus status) {
  TRACE_METHOD();
  return post(logPrefix, &ApiManagerInternal::onPackageStatusChanged, handle,
              status);
}

// ------------------------------------------------------------------------------------------------
// ApiManagerInternal
// ------------------------------------------------------------------------------------------------
ApiManagerInternal::ApiManagerInternal(Core &_core, ApiManager &manager)
    : core(_core), manager(manager), connEngine(), sendEngine(), recvEngine() {}

void ApiManagerInternal::send(uint64_t postId, SendOptions sendOptions,
                              std::vector<uint8_t> data,
                              std::function<void(ApiStatus)> cb) {
  TRACE_METHOD(postId, sendOptionsToString(sendOptions), data);

  auto context = newSendContext();
  context->updateSend(sendOptions, std::move(data), cb);
  sendEngine.start(*context);
}

void ApiManagerInternal::sendReceive(
    uint64_t postId, SendOptions sendOptions, std::vector<uint8_t> data,
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback) {
  TRACE_METHOD(postId, sendOptionsToString(sendOptions), data);

  auto context = newSendReceiveContext();
  context->updateSendReceive(sendOptions, std::move(data), callback);
  sendReceiveEngine.start(*context);
}

void ApiManagerInternal::dial(
    uint64_t postId, SendOptions sendOptions, std::vector<uint8_t> data,
    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback) {
  TRACE_METHOD(postId, sendOptionsToString(sendOptions), data);

  auto context = newDialContext();
  context->updateDial(sendOptions, std::move(data), callback);
  dialEngine.start(*context);
}

void ApiManagerInternal::resume(
    uint64_t postId, ResumeOptions resumeOptions,
    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback) {
  TRACE_METHOD(postId, resumeOptionsToString(resumeOptions));

  auto context = newResumeContext();
  context->updateResume(resumeOptions, callback);
  resumeEngine.start(*context);
}

void ApiManagerInternal::bootstrapDial(
    uint64_t postId, BootstrapConnectionOptions options, std::vector<uint8_t> data,
    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback) {
  TRACE_METHOD(postId, bootstrapConnectionOptionsToString(options), data);

  auto context = newBootstrapDialContext();
  context->updateBootstrapDial(options, std::move(data), callback);
  bootstrapDialEngine.start(*context);
}

void ApiManagerInternal::getReceiveObject(
    uint64_t postId, ReceiveOptions recvOptions,
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> cb) {
  // called by Race::receive() to get the ReceiveObject
  // ReceiveObject may in turn call this->receive()
  TRACE_METHOD(postId, recvOptionsToString(recvOptions));

  auto context = newRecvContext();
  context->updateGetReceiver(recvOptions, cb);
  recvEngine.start(*context);
}

void ApiManagerInternal::receive(
    uint64_t postId, OpHandle handle,
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback) {
  // API package request - request fulfilled by receiveEncPkg
  // may be called before or after receiveEncPkg

  TRACE_METHOD(postId, handle);

  auto contexts = getContexts(handle);
  if (contexts.size() != 1) {
    helper::logError(logPrefix + "Invalid handle passed to receive");
    callback(ApiStatus::INTERNAL_ERROR, {});
    callback = {};
  }

  for (auto context : contexts) {
    context->updateReceive(handle, callback);
    triggerEvent(*context, EVENT_RECEIVE_REQUEST);
  }
}

void ApiManagerInternal::receiveRespond(
    uint64_t postId, OpHandle handle,
    std::function<void(ApiStatus, std::vector<uint8_t>, LinkAddress)>
        callback) {
  TRACE_METHOD(postId, handle);

  auto contexts = getContexts(handle);
  if (contexts.size() != 1) {
    helper::logError(logPrefix + "Invalid handle passed to receive");
    callback(ApiStatus::INTERNAL_ERROR, {}, "");
    callback = {};
    return;
  }

  auto thisContext = *contexts.begin();
  auto thisRecvContext = getDerivedContext<ApiRecvContext>(thisContext);
  if (thisRecvContext == nullptr) {
    helper::logError(logPrefix + "Failed to cast to receive context");
    callback(ApiStatus::INTERNAL_ERROR, {}, "");
    callback = {};
    return;
  }

  auto sendChannel = thisRecvContext->opts.send_channel;
  bool multiChannel = thisRecvContext->opts.multi_channel;

  auto wrapperCallback = [callback, sendChannel, multiChannel](
                             ApiStatus status, std::vector<uint8_t> data) {
    std::string logPrefix =
        "ApiManagerInternal::receiveRespondCallbackWrapper: ";
    if (status != ApiStatus::OK) {
      callback(status, {}, "");
      return;
    }

    try {
      std::string str{data.begin(), data.end()};
      nlohmann::json json = nlohmann::json::parse(str);
      LinkAddress linkAddress = json.at("linkAddress");
      std::string replyChannel = json.at("replyChannel");
      std::string messageB64 = json.at("message");
      std::vector<uint8_t> messageBytes = base64::decode(messageB64);

      if ((replyChannel != sendChannel) && (multiChannel == false)) {
        helper::logInfo(
            logPrefix +
            "Mismatch between expected reply channel and requested reply "
            "channel. Expected: " +
            sendChannel + ", Requested: " + replyChannel);
        callback(ApiStatus::INTERNAL_ERROR, {}, "");
      } else {
        callback(status, messageBytes, linkAddress);
      }
    } catch (std::exception &e) {
      helper::logError(logPrefix +
                       "Failed to process received message: " + e.what());
      // This can happen if a bad package is received. What's the proper
      // behavior? Should we ignore the package and retry the receive request?
      callback(ApiStatus::INTERNAL_ERROR, {}, "");
    }
  };

  for (auto context : contexts) {
    context->updateReceive(handle, wrapperCallback);
    triggerEvent(*context, EVENT_RECEIVE_REQUEST);
  }
}

void ApiManagerInternal::listen(
    uint64_t postId, ReceiveOptions recvOptions,
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback) {
  TRACE_METHOD(postId, recvOptionsToString(recvOptions));

  auto context = newListenContext();
  context->updateListen(recvOptions, callback);
  listenEngine.start(*context);
}

void ApiManagerInternal::bootstrapListen(
    uint64_t postId, BootstrapConnectionOptions options,
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback) {
  TRACE_METHOD(postId, bootstrapConnectionOptionsToString(options));

  auto context = newBootstrapListenContext();
  context->updateBootstrapListen(options, callback);
  bootstrapListenEngine.start(*context);
}

void ApiManagerInternal::accept(
    uint64_t postId, OpHandle handle,
    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> callback) {
  // API read receive connection - should call callback when a package is
  // available
  TRACE_METHOD(postId, handle);

  auto contexts = getContexts(handle);
  if (contexts.size() != 1) {
    helper::logError(logPrefix + "Invalid handle passed to accept");
    callback(ApiStatus::INTERNAL_ERROR, {}, {});
    callback = {};
  }

  for (auto context : contexts) {
    context->updateAccept(handle, callback);
    triggerEvent(*context, EVENT_ACCEPT);
  }
}

void ApiManagerInternal::read(
    uint64_t postId, OpHandle handle,
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback) {
  // API read receive connection - should call callback when a package is
  // available
  TRACE_METHOD(postId, handle);

  auto contexts = getContexts(handle);
  if (contexts.size() != 1) {
    helper::logError(logPrefix + "Invalid handle passed to read");
    callback(ApiStatus::INTERNAL_ERROR, {});
    callback = {};
  }

  for (auto context : contexts) {
    context->updateRead(handle, callback);
    triggerEvent(*context, EVENT_READ);
  }
}

void ApiManagerInternal::write(uint64_t postId, OpHandle handle,
                               std::vector<uint8_t> bytes,
                               std::function<void(ApiStatus)> callback) {
  // API read receive connection - should call callback when a package is
  // available
  TRACE_METHOD(postId, handle);

  auto contexts = getContexts(handle);
  if (contexts.size() != 1) {
    helper::logError(logPrefix + "Invalid handle passed to write");
    callback(ApiStatus::INTERNAL_ERROR);
    callback = {};
  }

  for (auto context : contexts) {
    context->updateWrite(handle, std::move(bytes), callback);
    triggerEvent(*context, EVENT_WRITE);
  }
}

void ApiManagerInternal::close(uint64_t postId, OpHandle handle,
                               std::function<void(ApiStatus)> callback) {
  // API close receive connection - should call callback after completing
  // shutdown of link
  TRACE_METHOD(postId, handle);

  auto contexts = getContexts(handle);
  if (contexts.size() != 1) {
    helper::logError(logPrefix + "Invalid handle passed to close");
    callback(ApiStatus::INTERNAL_ERROR);
    callback = {};
  }

  for (auto context : contexts) {
    context->updateClose(handle, callback);
    triggerEvent(*context, EVENT_CLOSE);
  }
}

void ApiManagerInternal::cancelEvent(
  uint64_t postId, OpHandle handle,
  std::function<void(ApiStatus, std::vector<uint8_t>)>) {
    TRACE_METHOD(postId, handle);

  auto contexts = getContexts(handle);
  if (contexts.size() != 1) {
    helper::logDebug(logPrefix + "Invalid handle - has " + std::to_string(contexts.size()) + " contexts");
  }

  for (auto context : contexts) {
    // allow state machine to call the previously set callback to unblock futures, etc.
    triggerEvent(*context, EVENT_CANCELLED);
  }
}

//--------------------------------------------------------
// State Machine callbacks
//--------------------------------------------------------
ActivateChannelStatusCode
ApiManagerInternal::activateChannel(ApiContext &context, RaceHandle handle,
                                    ChannelId channelId, std::string role) {
  auto it = activatedChannels.find(channelId);
  if (it != activatedChannels.end()) {
    ChannelStatus status = it->second.first;
    ChannelProperties props = it->second.second;
    if (props.currentRole.roleName != role) {
      return ActivateChannelStatusCode::ACTIVATED_WITH_DIFFERENT_ROLE;
    }

    SdkResponse response = manager.onChannelStatusChangedForContext(
        context.handle, handle, channelId, status, props);
    if (response.status != SDK_OK) {
      // This shouldn't happen
      return ActivateChannelStatusCode::INVALID_STATE;
    }

    return ActivateChannelStatusCode::ALREADY_ACTIVATED;
  }

  return core.getChannelManager().activateChannel(handle, channelId, role);
}

void ApiManagerInternal::stateMachineFailed(ApiContext &context) {
  RaceHandle contextHandle = context.handle;
  TRACE_METHOD(contextHandle);
  manager.onStateMachineFailed(contextHandle);
}

void ApiManagerInternal::stateMachineFinished(ApiContext &context) {
  RaceHandle contextHandle = context.handle;
  TRACE_METHOD(contextHandle);
  manager.onStateMachineFinished(contextHandle);
}

void ApiManagerInternal::connStateMachineConnected(RaceHandle contextHandle,
                                                   ConnectionID connId,
                                                   std::string linkAddress,
                                                   std::string channelId) {
  TRACE_METHOD(contextHandle, connId, linkAddress, channelId);
  manager.onConnStateMachineConnected(contextHandle, connId, linkAddress, channelId);
}

void ApiManagerInternal::onStateMachineFailed(uint64_t postId,
                                              RaceHandle contextHandle) {
  TRACE_METHOD(postId, contextHandle);

  auto contextIt = activeContexts.find(contextHandle);
  if (contextIt == activeContexts.end()) {
    return;
  }

  ApiContext &context = *contextIt->second;
  removeContext(context);

  auto contexts = getContexts(contextHandle);
  for (auto triggeredContext : contexts) {
    triggeredContext->updateStateMachineFailed(contextHandle);
    triggerEvent(*triggeredContext, EVENT_STATE_MACHINE_FAILED);
  }
}

void ApiManagerInternal::onStateMachineFinished(uint64_t postId,
                                                RaceHandle contextHandle) {
  TRACE_METHOD(postId, contextHandle);

  auto contextIt = activeContexts.find(contextHandle);
  if (contextIt == activeContexts.end()) {
    return;
  }

  ApiContext &context = *contextIt->second;
  removeContext(context);

  auto contexts = getContexts(contextHandle);
  for (auto triggeredContext : contexts) {
    triggeredContext->updateStateMachineFinished(contextHandle);
    triggerEvent(*triggeredContext, EVENT_STATE_MACHINE_FINISHED);
  }
}

void ApiManagerInternal::onConnStateMachineConnected(uint64_t postId,
                                                     RaceHandle contextHandle,
                                                     ConnectionID connId,
                                                     std::string linkAddress,
                                                     std::string channelId) {
  TRACE_METHOD(postId, contextHandle, connId, linkAddress, channelId);

  // Map for when additional SMs want to re-use this link/connection
  // Note: this logic means we expect the channel to _always_ return a
  // _unique_ link when we ask for a link without providing an address.
  if (linkAddress != "") {
    std::string normalized_address = nlohmann::json::parse(linkAddress).dump();
    helper::logDebug(logPrefix + " compare normalized: " + linkAddress + " vs " + normalized_address);
    helper::logDebug(logPrefix + "Inserting $" + channelId + "$ + $" + normalized_address + "$ into the linkConnMap with connID " + connId);
    linkConnMap.insert({channelId+normalized_address, {contextHandle, connId}});
  }
  
  auto contexts = getContexts(contextHandle);
  for (auto context : contexts) {
    context->updateConnStateMachineConnected(contextHandle, connId,
                                             linkAddress);
    triggerEvent(*context, EVENT_CONN_STATE_MACHINE_CONNECTED);
  }
}

// This is triggered by activateChannel if onChannelStatusChanged was already
// called
void ApiManagerInternal::onChannelStatusChangedForContext(
    uint64_t postId, RaceHandle contextHandle, RaceHandle callHandle,
    const ChannelId &channelGid, ChannelStatus status,
    const ChannelProperties &properties) {
  TRACE_METHOD(postId, callHandle, channelGid, status);

  EventType event = EVENT_INVALID;
  if (status == CHANNEL_AVAILABLE) {
    event = EVENT_CHANNEL_ACTIVATED;
  } else {
    event = EVENT_FAILED;
  }

  auto contextIt = activeContexts.find(contextHandle);
  if (contextIt == activeContexts.end()) {
    return;
  }

  ApiContext &context = *contextIt->second;
  context.updateChannelStatusChanged(callHandle, channelGid, status,
                                     properties);
  triggerEvent(context, event);
}

  // This is triggered by startConnStateMachine if a connection for
  // the channelId+linkAddress is already ready
void ApiManagerInternal::onConnStateMachineConnectedForContext(
    uint64_t postId, RaceHandle contextHandle, RaceHandle callHandle,
    RaceHandle connContextHandle, ConnectionID connId, std::string linkAddress) {
  TRACE_METHOD(postId, contextHandle, callHandle, connContextHandle, connId, linkAddress);

  auto contextIt = activeContexts.find(contextHandle);
  if (contextIt == activeContexts.end()) {
    helper::logError(logPrefix + " could not find calling context");
    return;
  }

  auto connContextIt = activeContexts.find(connContextHandle);
  if (connContextIt == activeContexts.end()) {
    helper::logError(logPrefix + " could not connContext");
    return;
  }

  contextIt->second->updateConnStateMachineConnected(connContextHandle,
                                                     connId,
                                                     linkAddress);
  connContextIt->second->updateDependent(contextHandle);
  triggerEvent(*contextIt->second, EVENT_CONN_STATE_MACHINE_CONNECTED);
}

//--------------------------------------------------------
// Plugin callbacks
//--------------------------------------------------------
void ApiManagerInternal::onChannelStatusChanged(
    uint64_t postId, RaceHandle chanHandle, const ChannelId &channelGid,
    ChannelStatus status, const ChannelProperties &properties) {
  TRACE_METHOD(postId, chanHandle, channelGid, status);

  activatedChannels[channelGid] = {status, properties};

  EventType event = EVENT_INVALID;
  if (status == CHANNEL_AVAILABLE) {
    event = EVENT_CHANNEL_ACTIVATED;
  } else {
    event = EVENT_FAILED;
  }

  auto contexts = getContexts(chanHandle, channelGid);
  for (auto context : contexts) {
    context->updateChannelStatusChanged(chanHandle, channelGid, status,
                                        properties);
    triggerEvent(*context, event);
  }
}

void ApiManagerInternal::onLinkStatusChanged(uint64_t postId,
                                             RaceHandle linkHandle,
                                             const LinkID &linkId,
                                             LinkStatus status,
                                             const LinkProperties &properties) {
  TRACE_METHOD(postId, linkHandle, linkId, status);

  EventType event = EVENT_INVALID;
  if (status == LINK_CREATED || status == LINK_LOADED) {
    event = EVENT_LINK_ESTABLISHED;
  } else if (status == LINK_DESTROYED) {
    event = EVENT_LINK_DESTROYED;
  } else {
    event = EVENT_FAILED;
  }

  auto contexts = getContexts(linkHandle, linkId);
  for (auto context : contexts) {
    context->updateLinkStatusChanged(linkHandle, linkId, status, properties);
    triggerEvent(*context, event);
  }
}

void ApiManagerInternal::onConnectionStatusChanged(
    uint64_t postId, RaceHandle connHandle, const ConnectionID &connId,
    ConnectionStatus status, const LinkProperties &properties) {
  TRACE_METHOD(postId, connHandle, connId, status);

  EventType event = EVENT_INVALID;
  if (status == CONNECTION_OPEN) {
    event = EVENT_CONNECTION_ESTABLISHED;
  } else if (status == CONNECTION_CLOSED) {
    event = EVENT_CONNECTION_DESTROYED;
  } else {
    event = EVENT_FAILED;
  }

  auto contexts = getContexts(connHandle, connId);
  for (auto context : contexts) {
    context->updateConnectionStatusChanged(connHandle, connId, status,
                                           properties);
    triggerEvent(*context, event);
  }
}

void ApiManagerInternal::receiveEncPkg(
    uint64_t postId, const EncPkg &pkg,
    const std::vector<ConnectionID> &connIDs) {
  TRACE_METHOD(postId, connIDs);
  // package received by plugin
  // called by SdkWrapper::receiveEncPkg() -> Core::receiveEncPkg() ->
  // this->receiveEncPkg()

  if (connIDs.empty()) {
    helper::logError(logPrefix + " no connection IDs");
    return;
  } else if (connIDs.size() > 1) {
    helper::logError(logPrefix + " too many connection IDs");
    return;
  }

  std::shared_ptr<std::vector<uint8_t>> contents =
      std::make_shared<std::vector<uint8_t>>(pkg.getCipherText());

  const ConnectionID &connId = connIDs.front();
  Contexts contexts;
  if (contents->size() < packageIdLen) {
    contexts = getContexts(connId);
  } else {
    std::vector<int> packageIdBytes{contents->begin(),
                                    contents->begin() + packageIdLen};
    helper::logDebug(logPrefix + "PackageId: " + json(packageIdBytes).dump());
    std::string packageId{contents->begin(), contents->begin() + packageIdLen};
    helper::logDebug(logPrefix + "PackageId+ConnId: " + json(packageIdBytes).dump() + connId);
    auto it = packageIdContextMap.find(packageId + connId);
    if (it != packageIdContextMap.end()) {
      contexts = it->second;
      *contents = std::vector<uint8_t>(contents->begin() + packageIdLen,
                                       contents->end());
      helper::logDebug(logPrefix + " found package id");

    } else {
      // buffer messages that might be for conduits/packageIds we have
      // not _yet_ resumed
      auto packagesIt = unassociatedPackages.find(packageId);
      if (packagesIt != unassociatedPackages.end()) {
          packagesIt->second.emplace_back(pkg);
      } else{
        unassociatedPackages.insert({packageId, {pkg}});
      }
      contexts = getContexts(connId);
      helper::logDebug(logPrefix + " did not find package id in an existing conduit");
    }
  }

  if (contexts.size() == 0) {
    helper::logError(logPrefix + " found 0 contexts");
  }

  for (auto context : contexts) {
    context->updateReceiveEncPkg(connId, contents);
    triggerEvent(*context, EVENT_RECEIVE_PACKAGE);
  }
}

void ApiManagerInternal::onPackageStatusChanged(uint64_t postId,
                                                RaceHandle pkgHandle,
                                                PackageStatus status) {
  TRACE_METHOD(postId, pkgHandle, status);

  EventType event = EVENT_INVALID;
  if (status == PACKAGE_SENT) {
    event = EVENT_PACKAGE_SENT;
  } else if (status == PACKAGE_RECEIVED) {
    event = EVENT_PACKAGE_RECEIVED;
  } else if (status == PACKAGE_FAILED_GENERIC ||
             status == PACKAGE_FAILED_NETWORK_ERROR ||
             status == PACKAGE_FAILED_TIMEOUT) {
    event = EVENT_PACKAGE_FAILED;
  } else {
    event = EVENT_FAILED;
  }

  auto contexts = getContexts(pkgHandle);
  for (auto context : contexts) {
    context->updatePackageStatusChanged(pkgHandle, status);
    triggerEvent(*context, event);
  }
}

RaceHandle ApiManagerInternal::startConnStateMachine(RaceHandle contextHandle,
                                                     ChannelId channelId,
                                                     std::string role,
                                                     std::string linkAddress,
                                                     bool creating,
                                                     bool sending) {
  TRACE_METHOD(contextHandle);

  // We already made this link/connection
  // Note: this is _only_ valid when we are specifying the link address
  // Otherwise the address is being dynamically generated and will be unique
  if (linkAddress != "") {
    std::string normalized_address = nlohmann::json::parse(linkAddress).dump();
    helper::logDebug(logPrefix + " compare normalized: " + linkAddress + " vs " + normalized_address);
    auto connContextIt = linkConnMap.find(channelId + normalized_address);
    if (connContextIt != linkConnMap.end()) {
      helper::logDebug(logPrefix + "got existing entry for $" + channelId + "$ $" + normalized_address + "$ in the linkConnMap with ConnID=" + connContextIt->second.second);
    RaceHandle callHandle = getCore().generateHandle();
    manager.onConnStateMachineConnectedForContext(contextHandle,
                                                  callHandle,
                                                  connContextIt->second.first,
                                                  connContextIt->second.second,
                                                  linkAddress);
    return connContextIt->second.first;
    }
  }

  // TODO do validation checks
  
  // create a connection context and copy information from the send/recv context
  auto context = newConnContext();
  context->updateConnStateMachineStart(contextHandle, channelId, role,
                                       linkAddress, creating, sending);

  EventResult result = connEngine.start(*context);
  if (result != EventResult::SUCCESS) {
    return NULL_RACE_HANDLE;
  }

  return context->handle;
}

RaceHandle ApiManagerInternal::startConduitectStateMachine(
    RaceHandle contextHandle, RaceHandle recvHandle,
    const ConnectionID &recvConnId, RaceHandle sendHandle,
    const ConnectionID &sendConnId, const ChannelId &sendChannel,
    const ChannelId &recvChannel, const std::string &packageId,
    std::vector<std::vector<uint8_t>> recvMessages, RaceHandle apiHandle) {
  TRACE_METHOD(contextHandle, recvHandle, recvConnId, sendChannel, recvChannel,
               sendHandle, sendConnId);

  // create a connection context and copy information from the send/recv context
  auto context = newConduitectContext();
  context->updateConduitectStateMachineStart(
      contextHandle, recvHandle, recvConnId, sendHandle, sendConnId,
      sendChannel, recvChannel, packageId, std::move(recvMessages), apiHandle);

  EventResult result = connObjectEngine.start(*context);
  if (result != EventResult::SUCCESS) {
    helper::logError(logPrefix + "connObjectEngine.start failed");
    return NULL_RACE_HANDLE;
  }

  auto recvContextIt = activeContexts.find(recvHandle);
  if (recvContextIt == activeContexts.end()) {
    helper::logError(logPrefix + "recvContextIt could not be found for recvHandle");
    return NULL_RACE_HANDLE;
  }

  recvContextIt->second->updateDependent(context->handle);
  triggerEvent(*recvContextIt->second, EVENT_ADD_DEPENDENT);

  auto sendContextIt = activeContexts.find(sendHandle);
  if (sendContextIt == activeContexts.end()) {
    helper::logError(logPrefix + "sendContextIt could not be found for sendHandle");
    return NULL_RACE_HANDLE;
  }

  sendContextIt->second->updateDependent(context->handle);
  triggerEvent(*sendContextIt->second, EVENT_ADD_DEPENDENT);

  if (context->handle == NULL_RACE_HANDLE) {
    helper::logError(logPrefix + "context->handle is NULL");
  }
  return context->handle;
}

RaceHandle ApiManagerInternal::startPreConduitStateMachine(
    RaceHandle contextHandle, RaceHandle recvHandle,
    const ConnectionID &recvConnId, const ChannelId &recvChannel,
    const ChannelId &sendChannel, const std::string &sendRole,
    const std::string &sendLinkAddress, const std::string &packageId,
    std::vector<std::vector<uint8_t>> recvMessages) {
        helper::logInfo(
                         " START PRECONN OBJECT being called");
  // create a connection context and copy information from the send/recv context
  auto context = newPreConduitContext();
  context->updatePreConduitStateMachineStart(
      contextHandle, recvHandle, recvConnId, recvChannel, sendChannel, sendRole,
      sendLinkAddress, packageId, recvMessages);

  EventResult result = preConduitEngine.start(*context);
  if (result != EventResult::SUCCESS) {
    return NULL_RACE_HANDLE;
  }

  auto recvContextIt = activeContexts.find(recvHandle);
  if (recvContextIt == activeContexts.end()) {
    return NULL_RACE_HANDLE;
  }

  recvContextIt->second->updateDependent(context->handle);
  triggerEvent(*recvContextIt->second, EVENT_ADD_DEPENDENT);

  return context->handle;
}

RaceHandle ApiManagerInternal::startBootstrapPreConduitStateMachine(
                                                RaceHandle contextHandle,
                                                const ApiBootstrapListenContext &listenContext,
                                                const std::string &packageId,
                                                std::vector<std::vector<uint8_t>> recvMessages) {
        helper::logInfo(
                         " START BOOTSTRAP PRECONN OBJECT being called");
  // create a connection context and copy information from the send/recv context
  auto context = newBootstrapPreConduitContext();
  // auto listenContextIt = activeContexts.find(contextHandle);
  // if (listenContextIt == activeContexts.end()) {
  //   return NULL_RACE_HANDLE;
  // }

  // This may need to be a reinterpret_cast - don't want to lose the extended link info
  // ApiBootstrapListenContext listenContext = static_cast<ApiBootstrapListenContext>(*listenContextIt->second);
  context->updateBootstrapPreConduitStateMachineStart(contextHandle,
                                                      listenContext,
                                                      packageId, recvMessages);
  EventResult result = bootstrapPreConduitEngine.start(*context);
  if (result != EventResult::SUCCESS) {
    return NULL_RACE_HANDLE;
  }

  addDependent(listenContext.initSendConnSMHandle, context->handle);
  addDependent(listenContext.initRecvConnSMHandle, context->handle);
  return context->handle;
}

void ApiManagerInternal::addDependent(RaceHandle contextHandle, RaceHandle newDependentHandle) {
  auto connContextIt = activeContexts.find(contextHandle);
  if (connContextIt == activeContexts.end()) {
    return;
  }
  connContextIt->second->updateDependent(newDependentHandle);
  triggerEvent(*connContextIt->second, EVENT_ADD_DEPENDENT);
}
bool ApiManagerInternal::onListenAccept(
    RaceHandle contextHandle,
    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> acceptCb) {
  TRACE_METHOD(contextHandle);

  EventType event = EVENT_LISTEN_ACCEPTED;
  auto context = activeContexts.find(contextHandle);
  if (context == activeContexts.end()) {
    helper::logError(logPrefix + "Could not find context for handle");
    return false;
  }
  context->second->updateListenAccept(acceptCb);
  triggerEvent(*context->second, event);
  return true;
}

bool ApiManagerInternal::onBootstrapListenAccept(
    RaceHandle contextHandle,
    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> acceptCb) {
  TRACE_METHOD(contextHandle);

  EventType event = EVENT_LISTEN_ACCEPTED;
  auto context = activeContexts.find(contextHandle);
  if (context == activeContexts.end()) {
    helper::logError(logPrefix + "Could not find context for handle");
    return false;
  }
  context->second->updateListenAccept(acceptCb);
  triggerEvent(*context->second, event);
  return true;
}

bool ApiManagerInternal::detachConnSM(RaceHandle contextHandle,
                                      RaceHandle connSMContextHandle) {
  TRACE_METHOD(contextHandle, connSMContextHandle);

  auto connSMContextIt = activeContexts.find(connSMContextHandle);
  if (connSMContextIt == activeContexts.end()) {
    return false;
  }

  connSMContextIt->second->updateDetach(contextHandle);
  triggerEvent(*connSMContextIt->second, EVENT_DETACH_DEPENDENT);

  return true;
}

Core &ApiManagerInternal::getCore() { return core; }

// register contexts
void ApiManagerInternal::registerHandle(ApiContext &context,
                                        RaceHandle handle) {
  TRACE_METHOD(context.handle, handle);
  handleContextMap[handle].insert(&context);
}

void ApiManagerInternal::registerId(ApiContext &context,
                                    const std::string &id) {
  TRACE_METHOD(context.handle, id);
  idContextMap[id].insert(&context);
}

void ApiManagerInternal::registerPackageId(ApiContext &context,
                                           const ConnectionID &connId,
                                           const std::string &id) {
  std::string packageId =
      json(std::vector<uint8_t>(id.begin(), id.end())).dump();
  TRACE_METHOD(context.handle, packageId, connId);
  packageIdContextMap[id + connId].insert(&context);

  // Check for buffered received messages that came before this packageId was registered
  auto packageListIt = unassociatedPackages.find(packageId);
  if (packageListIt != unassociatedPackages.end()) {
    helper::logDebug(logPrefix + "Found " + std::to_string(packageListIt->second.size()) + " packages waiting for this packageId");
    for (auto packageIt : packageListIt->second) {
      Contexts contexts;
      std::shared_ptr<std::vector<uint8_t>> contents =
        std::make_shared<std::vector<uint8_t>>(packageIt.getCipherText());

      *contents = std::vector<uint8_t>(contents->begin() + packageIdLen,
                                       contents->end());
      context.updateReceiveEncPkg(connId, contents);
      triggerEvent(context, EVENT_RECEIVE_PACKAGE);
    }
  }  else {
    helper::logDebug(logPrefix + "No packages were waiting for this packageId");
  }
}

void ApiManagerInternal::unregisterHandle(ApiContext &context,
                                          RaceHandle handle) {
  TRACE_METHOD(context.handle, handle);
  auto pairIt = handleContextMap.find(handle);
  if (pairIt != handleContextMap.end()) {
    pairIt->second.erase(&context);
    if (pairIt->second.empty()) {
      handleContextMap.erase(pairIt);
    }
  }
}

//--------------------------------------------------------
// internal helpers
//--------------------------------------------------------
ApiContext *ApiManagerInternal::newConnContext() {
  auto newContext = std::make_unique<ApiConnContext>(*this, connEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newConduitectContext() {
  auto newContext =
      std::make_unique<ConduitContext>(*this, connObjectEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newPreConduitContext() {
  auto newContext =
      std::make_unique<PreConduitContext>(*this, preConduitEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newBootstrapPreConduitContext() {
  auto newContext =
      std::make_unique<BootstrapPreConduitContext>(*this, bootstrapPreConduitEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newSendContext() {
  auto newContext = std::make_unique<ApiSendContext>(*this, sendEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newSendReceiveContext() {
  auto newContext =
      std::make_unique<ApiSendReceiveContext>(*this, sendReceiveEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newRecvContext() {
  auto newContext = std::make_unique<ApiRecvContext>(*this, recvEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newDialContext() {
  auto newContext = std::make_unique<ApiDialContext>(*this, dialEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newResumeContext() {
  auto newContext = std::make_unique<ApiResumeContext>(*this, resumeEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newBootstrapDialContext() {
  auto newContext = std::make_unique<ApiBootstrapDialContext>(*this, bootstrapDialEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}


ApiContext *ApiManagerInternal::newListenContext() {
  auto newContext = std::make_unique<ApiListenContext>(*this, listenEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiContext *ApiManagerInternal::newBootstrapListenContext() {
  auto newContext = std::make_unique<ApiBootstrapListenContext>(*this, bootstrapListenEngine);
  auto handle = newContext->handle;
  activeContexts[handle] = (std::move(newContext));
  return activeContexts[handle].get();
}

ApiManagerInternal::Contexts
ApiManagerInternal::getContexts(RaceHandle handle) {
  auto handleContexts = handleContextMap.find(handle);
  if (handleContexts != handleContextMap.end()) {
    return handleContexts->second;
  }
  return {};
}

ApiManagerInternal::Contexts
ApiManagerInternal::getContexts(const std::string &id) {
  Contexts contexts;
  auto idContexts = idContextMap.find(id);
  if (idContexts != idContextMap.end()) {
    return idContexts->second;
  }
  return {};
}

ApiManagerInternal::Contexts
ApiManagerInternal::getContextsByPackageId(const std::string &packageId) {
  Contexts contexts;
  auto it = packageIdContextMap.find(packageId);
  if (it != packageIdContextMap.end()) {
    return it->second;
  }
  return {};
}

ApiManagerInternal::Contexts
ApiManagerInternal::getContexts(RaceHandle handle, const std::string &id) {
  auto handleContexts = getContexts(handle);
  auto idContexts = getContexts(id);

  Contexts contexts(handleContexts);
  contexts.insert(idContexts.begin(), idContexts.end());
  return contexts;
}

void ApiManagerInternal::removeContext(ApiContext &context) {
  TRACE_METHOD();
  for (auto pairIt = handleContextMap.begin();
       pairIt != handleContextMap.end();) {
    pairIt->second.erase(&context);
    if (pairIt->second.size() == 0) {
      pairIt = handleContextMap.erase(pairIt);
    } else {
      ++pairIt;
    }
  }

  for (auto pairIt = idContextMap.begin(); pairIt != idContextMap.end();) {
    pairIt->second.erase(&context);
    if (pairIt->second.size() == 0) {
      pairIt = idContextMap.erase(pairIt);
    } else {
      ++pairIt;
    }
  }

  for (auto pairIt = packageIdContextMap.begin();
       pairIt != packageIdContextMap.end();) {
    pairIt->second.erase(&context);
    if (pairIt->second.size() == 0) {
      std::vector<int> int_vector{pairIt->first.begin(), pairIt->first.begin() + packageIdLen};
      helper::logDebug("removeContext: Removing packageId+ConnectionID=" + json(int_vector).dump() + pairIt->first.substr(packageIdLen));
      pairIt = packageIdContextMap.erase(pairIt);
    } else {
      ++pairIt;
    }
  }

  activeContexts.erase(context.handle);
}

EventResult ApiManagerInternal::triggerEvent(ApiContext &context,
                                             EventType event) {
  EventResult result = context.engine.handleEvent(context, event);
  
  if (result != EventResult::SUCCESS) {
    helper::logDebug("triggerEvent " + eventToString(event) + " failed");
  }

  return result;
}

//--------------------------------------------------------
// debug
//--------------------------------------------------------
void ApiManagerInternal::dumpContexts(std::string context){
  TRACE_METHOD();

  if(context.size()) {
    printf("%s\n", context.c_str());
  }
  printf("dumping activeContexts handles ---\n");
  for(std::unordered_map<RaceHandle, std::unique_ptr<Raceboat::ApiContext>>::iterator it = activeContexts.begin(); it != activeContexts.end(); ++it) { 
    printf("  %lu --  ", it->first);
    it->second->dumpContext();
  }

  printf("dumping handleContextMap ---\n");
  for (auto pair: handleContextMap) {
    // second is Contexts = std::unordered_set<ApiContext *>;
    printf("  %lu: \n", pair.first);
    for (auto ctx: pair.second)  {
      ctx->dumpContext();
    }
  }

  printf("dumping idContextMap ---\n");
  for (auto pair: idContextMap) {
    printf("  %s: \n", pair.first.c_str());
    for (auto ctx: pair.second)  {
      ctx->dumpContext();
    }
  }

  printf("dumping packageIdContextMap ---\n");
  for (auto pair: packageIdContextMap) {
    printf("  %s: \n", pair.first.c_str());
    for (auto ctx: pair.second)  {
      ctx->dumpContext();
    }
  }
}
} // namespace Raceboat
