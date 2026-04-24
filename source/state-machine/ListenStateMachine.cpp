
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

#include "ListenStateMachine.h"

#include "../../include/race/Race.h"
#include "Core.h"
#include "ConnectionStateMachine.h"
#include "Events.h"
#include "PluginContainer.h"
#include "PluginWrapper.h"
#include "States.h"
#include "api-managers/ApiManager.h"
#include "base64.h"
#include "helper.h"

namespace Raceboat {

//-----------------------------------------------------------------------------------------------
// Context
//-----------------------------------------------------------------------------------------------

void ApiListenContext::updateListen(
    const ReceiveOptions &_recvOptions,
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> _cb) {
  this->opts = _recvOptions;
  this->listenCb = _cb;
};
void ApiListenContext::updateAccept(
                                    RaceHandle /* _handle */, std::function<void(ApiStatus, RaceHandle, ConduitProperties)> _cb) {
  this->acceptCb.push_back(_cb);
};
void ApiListenContext::updateClose(RaceHandle /* handle */,
                                   std::function<void(ApiStatus)> _cb) {
  this->closeCb = _cb;
}

void ApiListenContext::updateReceiveEncPkg(
    ConnectionID connId, std::shared_ptr<std::vector<uint8_t>> _data) {
  // Legacy queue for backwards compatibility (dial messages as JSON)
  this->data.push(_data);
  
  // For new accept() model: track which connection each message came from
  // This allows us to match hello messages to specific accept() calls
  helper::logDebug("ApiListenContext::updateReceiveEncPkg: received message from connection " + connId);
};
void ApiListenContext::updateConnStateMachineConnected(
    RaceHandle /* connSMHandle */, ConnectionID connId,
    std::string linkAddress, LinkID linkId) {
  const std::string logPrefix = "ApiListenContext::updateConnStateMachineConnected: ";
  
  // Store the LinkID from the first connection - all subsequent accepts will reuse this
  if (firstLinkId.empty() && !linkId.empty()) {
    firstLinkId = linkId;
    helper::logDebug(logPrefix + "Stored first LinkID: " + firstLinkId + 
                     " for reuse in subsequent accepts");
  }
  
  // Store connection info - conduit will be created when dial message arrives
  this->recvConnId = connId;
  this->recvLinkAddress = linkAddress;
};

//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StateListenInitial : public ListenState {
  explicit StateListenInitial(StateType id = STATE_LISTEN_INITIAL)
      : ListenState(id, "STATE_LISTEN_INITIAL") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ChannelId channelId = ctx.opts.recv_channel;
    std::string role = ctx.opts.recv_role;
    std::string linkAddress = ctx.opts.recv_address;

    if (channelId.empty()) {
      helper::logError(logPrefix +
                       "Invalid receive channel id passed to getReceiver");
      ctx.listenCb(ApiStatus::CHANNEL_INVALID, "", {});
      ctx.listenCb = {};
      return EventResult::NOT_SUPPORTED;
    } else if (role.empty()) {
      helper::logError(logPrefix +
                       "Invalid receive role passed to getReceiver");
      ctx.listenCb(ApiStatus::INVALID_ARGUMENT, "", {});
      ctx.listenCb = {};
      return EventResult::NOT_SUPPORTED;
    } else if (ctx.opts.send_channel.empty()) {
      helper::logError(logPrefix +
                       "Invalid send channel id passed to getReceiver");
      ctx.listenCb(ApiStatus::INVALID_ARGUMENT, "", {});
      ctx.listenCb = {};
      return EventResult::NOT_SUPPORTED;
    } else if (ctx.opts.send_role.empty()) {
      helper::logError(logPrefix + "Invalid send role passed to getReceiver");
      ctx.listenCb(ApiStatus::INVALID_ARGUMENT, "", {});
      ctx.listenCb = {};
      return EventResult::NOT_SUPPORTED;
    }

    PluginContainer *container = ctx.manager.getCore().getChannel(channelId);
    if (container == nullptr) {
      helper::logError(logPrefix + "Failed to get channel with id " +
                       channelId);
      ctx.listenCb(ApiStatus::CHANNEL_INVALID, "", {});
      ctx.listenCb = {};
      return EventResult::NOT_SUPPORTED;
    }

    // Store channel info for creating connections later
    ctx.recvChannelId = channelId;
    ctx.recvRole = role;
    ctx.recvLinkAddressStored = linkAddress;
    
    // Create the FIRST connection state machine for the initial listener
    // This creates LinkID_0 and waits for the first client to connect
    ctx.recvConnSMHandle = ctx.manager.startConnStateMachineBidi(
        ctx.handle, channelId, role, linkAddress, true);

    if (ctx.recvConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, ctx.recvConnSMHandle);

    return EventResult::SUCCESS;
  }
};

struct StateListenConnectionOpen : public ListenState {
  explicit StateListenConnectionOpen(
      StateType id = STATE_LISTEN_CONNECTION_OPEN)
      : ListenState(id, "STATE_LISTEN_CONNECTION_OPEN") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    RaceHandle receiverHandle = ctx.manager.getCore().generateHandle();

    ctx.listenCb(ApiStatus::OK, ctx.recvLinkAddress, receiverHandle);
    ctx.listenCb = {};

    ctx.manager.registerHandle(ctx, receiverHandle);

    // Register packageId all zeros so dial messages route to this Listen SM
    std::string packageId(packageIdLen, '\0');
    ctx.manager.registerPackageId(ctx, ctx.recvConnId, packageId);

    ctx.pendingEvents.push(EVENT_ALWAYS);

    return EventResult::SUCCESS;
  }
};

struct StateListenWaiting : public ListenState {
  explicit StateListenWaiting(StateType id = STATE_LISTEN_WAITING)
      : ListenState(id, "STATE_LISTEN_WAITING") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    // Handle accept() calls by starting openConnection() for each accept
    // Each accept() waits for a client to connect, then waits for a dial message
    while (!ctx.acceptCb.empty()) {
      auto cb = std::move(ctx.acceptCb.front());
      ctx.acceptCb.pop_front();
      
      RaceHandle connSMHandle = NULL_RACE_HANDLE;
      
      // Check if this is the first accept() and we have the initial connection SM
      if (ctx.recvConnSMHandle != NULL_RACE_HANDLE && 
          ctx.connSMToAcceptCallback.find(ctx.recvConnSMHandle) == ctx.connSMToAcceptCallback.end()) {
        // Use the existing connection SM from StateListenInitial for the first accept
        connSMHandle = ctx.recvConnSMHandle;
        helper::logDebug(logPrefix + "Mapping initial connection SM " + std::to_string(connSMHandle) + 
                         " to first accept() call");
      } else {
        // Create a new connection SM for subsequent accepts
        helper::logDebug(logPrefix + "Creating new connection SM for accept() call");
        
        // Start a new connection state machine for this accept
        // Pass the existing LinkID from the first connection - all accepts share the same link
        // Each openConnection() on that link will get a new ConnectionID
        connSMHandle = ctx.manager.startConnStateMachineBidi(
            ctx.handle, 
            ctx.recvChannelId, 
            ctx.recvRole, 
            ctx.recvLinkAddress,  // Link address from first connection
            false,  // NOT creating - reusing existing link
            ctx.firstLinkId  // Reuse the existing LinkID_0
        );
        
        if (connSMHandle == NULL_RACE_HANDLE) {
          helper::logError(logPrefix + "Failed to start connection state machine for accept()");
          cb(ApiStatus::INTERNAL_ERROR, {}, {});
          continue;
        }
        
        ctx.manager.registerHandle(ctx, connSMHandle);
        helper::logDebug(logPrefix + "Created connection SM " + std::to_string(connSMHandle) + " for accept()");
      }
      
      // Map this connection SM to its accept callback
      // Conduit will be created when dial message arrives
      ctx.connSMToAcceptCallback[connSMHandle] = std::move(cb);
      ctx.pendingConnSMHandles.push(connSMHandle);
    }

    // Process dial messages from clients
    // ALL clients send dial messages specifying packageId, replyChannel, and linkAddress
    while (!ctx.data.empty()) {
      auto data = std::move(ctx.data.front());
      ctx.data.pop();

      try {
        std::string str{data->begin(), data->end()};
        nlohmann::json json = nlohmann::json::parse(str);
        LinkAddress linkAddress = json.at("linkAddress");
        std::string replyChannel = json.at("replyChannel");
        std::string packageId = json.at("packageId");
        std::string messageB64 = json.at("message");
        std::vector<uint8_t> packageIdBytes = base64::decode(packageId);

        if (packageIdBytes.size() != packageIdLen) {
          helper::logError(logPrefix + "Invalid package id len: " +
                           std::to_string(packageIdBytes.size()));
          continue;
        }

        std::string replyPackageId =
            std::string(packageIdBytes.begin(), packageIdBytes.end());

        if (replyChannel != ctx.opts.send_channel) {
          helper::logError(
              logPrefix +
              "Mismatch between expected reply channel and requested reply "
              "channel. Expected: " +
              ctx.opts.send_channel + ", Requested: " + replyChannel);
          continue;
        }

        std::vector<uint8_t> dialMessage = base64::decode(messageB64);

        // Note: If linkAddress is empty in dial message, PreConduitSM will reuse the existing connection
        // If linkAddress is specified, PreConduitSM will create a new link/connection
        
        RaceHandle preConnSMHandle = ctx.manager.startPreConduitStateMachine(
            ctx.handle, ctx.recvConnSMHandle, ctx.recvConnId,
            ctx.opts.recv_channel, ctx.opts.send_channel, ctx.opts.send_role,
            linkAddress, replyPackageId, {std::move(dialMessage)});

        if (preConnSMHandle == NULL_RACE_HANDLE) {
          helper::logError(logPrefix +
                           " starting connection state machine failed");
          return EventResult::NOT_SUPPORTED;
        }

        ctx.preConduitSM.push(preConnSMHandle);

        break;
      } catch (std::exception &e) {
        helper::logError(logPrefix +
                         "Failed to process received message: " + e.what());
      }
    }

    // Pair dial-based PreConduitSMs with accept callbacks
    // Use pendingConnSMHandles (not acceptCb which was already drained above)
    while (!ctx.pendingConnSMHandles.empty() && !ctx.preConduitSM.empty()) {
      RaceHandle connSMHandle = ctx.pendingConnSMHandles.front();
      ctx.pendingConnSMHandles.pop();
      RaceHandle preConnSMHandle = ctx.preConduitSM.front();
      ctx.preConduitSM.pop();
      
      // Get the accept callback for this connection SM
      auto it = ctx.connSMToAcceptCallback.find(connSMHandle);
      if (it != ctx.connSMToAcceptCallback.end()) {
        auto cb = std::move(it->second);
        ctx.connSMToAcceptCallback.erase(it);
        if (!ctx.manager.onListenAccept(preConnSMHandle, cb)) {
          cb(ApiStatus::INTERNAL_ERROR, {}, {});
        }
      } else {
        helper::logError(logPrefix + "No accept callback found for connection SM " + 
                         std::to_string(connSMHandle));
      }
    }

    return EventResult::SUCCESS;
  }
};

struct StateListenFinished : public ListenState {
  explicit StateListenFinished(StateType id = STATE_LISTEN_FINISHED)
      : ListenState(id, "STATE_LISTEN_FINISHED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    for (auto &cb : ctx.acceptCb) {
      cb(ApiStatus::CLOSING, {}, {});
    }
    ctx.acceptCb = {};

    ctx.manager.stateMachineFinished(ctx);

    ctx.closeCb(ApiStatus::OK);
    ctx.closeCb = {};
    return EventResult::SUCCESS;
  }
  virtual bool finalState() { return true; }
};

struct StateListenFailed : public ListenState {
  explicit StateListenFailed(StateType id = STATE_LISTEN_FAILED)
      : ListenState(id, "STATE_LISTEN_FAILED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.listenCb) {
      ctx.listenCb(ApiStatus::INTERNAL_ERROR, {}, {});
      ctx.listenCb = {};
    }

    for (auto &cb : ctx.acceptCb) {
      cb(ApiStatus::INTERNAL_ERROR, {}, {});
    }
    ctx.acceptCb = {};

    if (ctx.closeCb) {
      ctx.closeCb(ApiStatus::INTERNAL_ERROR);
      ctx.closeCb = {};
    }

    ctx.manager.stateMachineFailed(ctx);
    return EventResult::SUCCESS;
  }
};

//-----------------------------------------------------------------------------------------------
// StateEngine
//-----------------------------------------------------------------------------------------------

ListenStateEngine::ListenStateEngine() {
  // calls startConnectionStateMachine on manager, waits for
  // connStateMachineConnected
  addInitialState<StateListenInitial>(STATE_LISTEN_INITIAL);
  // calls user supplied callback to return the receiver object, always
  // transitions to next state
  addState<StateListenConnectionOpen>(STATE_LISTEN_CONNECTION_OPEN);
  // do nothing, wait for a package to be received, accept to be called, or
  // listener to be closed
  addState<StateListenWaiting>(STATE_LISTEN_WAITING);
  // calls state machine finished on manager, final state
  addState<StateListenFinished>(STATE_LISTEN_FINISHED);
  addFailedState<StateListenFailed>(STATE_LISTEN_FAILED);

  // clang-format off
    declareStateTransition(STATE_LISTEN_INITIAL,                       EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_LISTEN_CONNECTION_OPEN);
    declareStateTransition(STATE_LISTEN_CONNECTION_OPEN,               EVENT_ALWAYS,                       STATE_LISTEN_WAITING);
    declareStateTransition(STATE_LISTEN_WAITING,                       EVENT_RECEIVE_PACKAGE,              STATE_LISTEN_WAITING);
    declareStateTransition(STATE_LISTEN_WAITING,                       EVENT_ACCEPT,                       STATE_LISTEN_WAITING);
    declareStateTransition(STATE_LISTEN_WAITING,                       EVENT_CLOSE,                        STATE_LISTEN_FINISHED);
  // clang-format on
}

std::string ListenStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
