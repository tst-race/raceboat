
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

#include "BootstrapPreConduitStateMachine.h"

#include "../../include/race/Race.h"
#include "Core.h"
#include "Events.h"
#include "PluginContainer.h"
#include "PluginWrapper.h"
#include "States.h"
#include "api-managers/ApiManager.h"
#include "base64.h"
#include "helper.h"
#include "BootstrapListenStateMachine.h"

namespace Raceboat {

//-----------------------------------------------------------------------------------------------
// Context
//-----------------------------------------------------------------------------------------------

void BootstrapPreConduitContext::updateBootstrapPreConduitStateMachineStart(
    RaceHandle contextHandle,
    const ApiBootstrapListenContext &parentContext,
    const std::string &_packageId,
    std::vector<std::vector<uint8_t>> recvMessages) {
  this->parentHandle = contextHandle;
  this->opts = BootstrapConnectionOptions(parentContext.opts);

  this->initSendConnSMHandle = parentContext.initSendConnSMHandle;
  this->initSendConnId = parentContext.initSendConnId;
  this->initSendLinkAddress = parentContext.initSendLinkAddress;
  helper::logInfo("updateBootstrapPreConduitStateMachineStart initSendLinkAddress: " + parentContext.initSendLinkAddress);
   
  this->initRecvConnSMHandle = parentContext.initRecvConnSMHandle;
  this->initRecvConnId = parentContext.initRecvConnId;
  this->initRecvLinkAddress = parentContext.initRecvLinkAddress;

  this->finalSendConnSMHandle = parentContext.finalSendConnSMHandle;
  this->finalSendConnId = parentContext.finalSendConnId;
  this->finalSendLinkAddress = parentContext.finalSendLinkAddress;
  helper::logInfo("updateBootstrapPreConduitStateMachineStart finalSendLinkAddress: " + parentContext.finalSendLinkAddress);
  
  this->finalRecvConnSMHandle = parentContext.finalRecvConnSMHandle;
  this->finalRecvConnId = parentContext.finalRecvConnId;
  this->finalRecvLinkAddress = parentContext.finalRecvLinkAddress;

  this->packageId = _packageId;
  this->recvQueue = recvMessages;
}

void BootstrapPreConduitContext::updateReceiveEncPkg(
    ConnectionID /* connId */, std::shared_ptr<std::vector<uint8_t>> data) {
  this->recvQueue.push_back(*data);
}

  // TODO Code Reuse
void BootstrapPreConduitContext::updateConnStateMachineConnected(
    RaceHandle contextHandle, ConnectionID connId,
    std::string linkAddress) {
  if (this->initRecvConnSMHandle == contextHandle) {
    this->initRecvConnId = connId;
    this->initRecvLinkAddress = linkAddress;
  } else if (this->initSendConnSMHandle == contextHandle) {
    this->initSendConnId = connId;
    this->initSendLinkAddress = linkAddress;
  } else if (this->finalRecvConnSMHandle == contextHandle) {
    this->finalRecvConnId = connId;
    this->finalRecvLinkAddress = linkAddress;
  } else if (this->finalSendConnSMHandle == contextHandle) {
    this->finalSendConnId = connId;
    this->finalSendLinkAddress = linkAddress;
  }
}
     
// void BootstrapPreConduitContext::updateConnStateMachineConnected(
//     RaceHandle /* contextHandle */, ConnectionID connId,
//     std::string /* linkAddress */) {
//   this->finalSendConnId = connId;
// }

void BootstrapPreConduitContext::updateListenAccept(
    std::function<void(ApiStatus, RaceHandle)> cb) {
  this->acceptCb = cb;
}

//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StateBootstrapPreConduitInitial : public BootstrapPreConduitState {
  explicit StateBootstrapPreConduitInitial(StateType id = STATE_BOOTSTRAP_PRE_CONN_OBJ_INITIAL)
      : BootstrapPreConduitState(id, "STATE_BOOTSTRAP_PRE_CONN_OBJ_INITIAL") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ctx.manager.registerPackageId(ctx, ctx.initRecvConnId, ctx.packageId);
    ctx.manager.registerHandle(ctx, ctx.parentHandle);

    return EventResult::SUCCESS;
  }
};

struct StateBootstrapPreConduitAccepted : public BootstrapPreConduitState {
  explicit StateBootstrapPreConduitAccepted(StateType id = STATE_BOOTSTRAP_PRE_CONN_OBJ_ACCEPTED)
      : BootstrapPreConduitState(id, "STATE_BOOTSTRAP_PRE_CONN_OBJ_ACCEPTED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    // Determine what connections/links we still need to get started
    // We should definitely have initRecv because we received the Hello from the client
    // We _may not_ have initSend if it is loader-to-creator
    // We _must not_ have finalSend or finalRecv becasue they should be per-connection

    // *** INIT SEND ***
    // initSend should be used and currently doesn't exist - we should be loading an address
    if (!ctx.opts.init_send_channel.empty() and ctx.initSendConnSMHandle == NULL_RACE_HANDLE) {
      bool create = ctx.shouldCreateSender(ctx.opts.init_send_channel);
      if (create) {
        helper::logError(logPrefix + " initSend should have been created during listener initialization (StateBootstrapListenInitial)");
        return EventResult::NOT_SUPPORTED;
      } else if (ctx.initSendLinkAddress.empty()) {
        helper::logError(logPrefix + " initSend address is missing but we are expecting to load it");
        return EventResult::NOT_SUPPORTED;
      } else {
        bool sending = true;
        ctx.initSendConnSMHandle = ctx.manager.
          startConnStateMachine(ctx.handle,
                                ctx.opts.init_send_channel,
                                ctx.opts.init_send_role,
                                ctx.initSendLinkAddress,
                                create, // is false
                                sending // is true
                                );
        if (ctx.initSendConnSMHandle == NULL_RACE_HANDLE) {
          helper::logError(logPrefix + " starting connection state machine failed");
          return EventResult::NOT_SUPPORTED;
        }
        ctx.manager.registerHandle(ctx, ctx.initSendConnSMHandle);
      } 
    }


     // *** FINAL SEND ***
    // Handle final server->client aka final_send
    bool create = ctx.shouldCreateSender(ctx.opts.final_send_channel);

    // We are creating, we will create and then send this address in a hello response
    if (create) {
      bool sending = true;
      ctx.finalSendConnSMHandle = ctx.manager.
        startConnStateMachine(ctx.handle,
                              ctx.opts.final_send_channel,
                              ctx.opts.final_send_role,
                              "",
                              create, // is true
                              sending // is true
                              );
      if (ctx.finalSendConnSMHandle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + " starting connection state machine failed");
        return EventResult::NOT_SUPPORTED;
      }
      ctx.manager.registerHandle(ctx, ctx.finalSendConnSMHandle);
      // We are loading, we should have an address to load from the hello
    } else if (ctx.initSendLinkAddress.empty()) {
      helper::logError(logPrefix + " finalSend address is missing (was it sent in the hello?)");
      return EventResult::NOT_SUPPORTED;
    } else {
    bool sending = true;
    ctx.finalSendConnSMHandle = ctx.manager.
      startConnStateMachine(ctx.handle,
                            ctx.opts.final_send_channel,
                            ctx.opts.final_send_role,
                            ctx.finalSendLinkAddress,
                            create, // is false
                            sending // is true
                            );
    if (ctx.finalSendConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }
    ctx.manager.registerHandle(ctx, ctx.finalSendConnSMHandle);
    } 
    

    // *** FINAL RECV ***
    create = ctx.shouldCreateReceiver(ctx.opts.final_recv_channel);

    // We are creating, we will create and then send this address in a hello response
    if (create) {
      bool sending = false;
      ctx.finalRecvConnSMHandle = ctx.manager.
        startConnStateMachine(ctx.handle,
                              ctx.opts.final_recv_channel,
                              ctx.opts.final_recv_role,
                              "",
                              create, // is true
                              sending // is false
                              );
      if (ctx.finalRecvConnSMHandle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + " starting connection state machine failed");
        return EventResult::NOT_SUPPORTED;
      }
      ctx.manager.registerHandle(ctx, ctx.finalRecvConnSMHandle);
    // We are loading, we should have an address to load from the hello
  } else if (ctx.initRecvLinkAddress.empty()) {
    helper::logError(logPrefix + " finalRecv address is missing (was it sent in the hello?)");
    return EventResult::NOT_SUPPORTED;
  } else {
    bool sending = false;
    ctx.finalRecvConnSMHandle = ctx.manager.
      startConnStateMachine(ctx.handle,
                            ctx.opts.final_recv_channel,
                            ctx.opts.final_recv_role,
                            ctx.finalRecvLinkAddress,
                            create, // is false
                            sending // is false
                            );
    if (ctx.finalRecvConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }
    ctx.manager.registerHandle(ctx, ctx.finalRecvConnSMHandle);
  } 
     
    ctx.manager.registerHandle(ctx, ctx.initSendConnSMHandle);
    ctx.pendingEvents.push(EVENT_ALWAYS);

    return EventResult::SUCCESS;
  }
};

struct StateBootstrapPreConduitWaitingForConnections : public BootstrapPreConduitState {
  explicit StateBootstrapPreConduitWaitingForConnections(StateType id = STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS)
      : BootstrapPreConduitState(id, "STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    // For each potential awaited connection, check if the handle is non-null (meaning we ARE expecting it) AND the connection ID is not set (meaning it has not finished opening yet)
    if (ctx.initRecvConnSMHandle != NULL_RACE_HANDLE and ctx.initRecvConnId.empty()) {
      return EventResult::SUCCESS;
    }
    if (ctx.initSendConnSMHandle != NULL_RACE_HANDLE and ctx.initSendConnId.empty()) {
      return EventResult::SUCCESS;
    }
    if (ctx.finalRecvConnSMHandle != NULL_RACE_HANDLE and ctx.finalRecvConnId.empty()) {
      return EventResult::SUCCESS;
    }
    if (ctx.finalSendConnSMHandle != NULL_RACE_HANDLE and ctx.finalSendConnId.empty()) {
      return EventResult::SUCCESS;
    }
    // No early returns indicate all expected connections are satisfied

    // We created one of these, so we will need to send the address back as a response
    if (ctx.shouldCreateSender(ctx.opts.final_send_channel) or
        ctx.shouldCreateReceiver(ctx.opts.final_recv_channel)) {
      ctx.pendingEvents.push(EVENT_NEEDS_SEND);
    }
    else {
      ctx.pendingEvents.push(EVENT_SATISFIED);
    }
    return EventResult::SUCCESS;
  }
};

struct StateBootstrapPreConduitSendResponse : public BootstrapPreConduitState {
  explicit StateBootstrapPreConduitSendResponse(StateType id = STATE_BOOTSTRAP_PRE_CONN_OBJ_SEND_RESPONSE)
      : BootstrapPreConduitState(id, "StateBootstrapPreConduitSendResponse") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    PluginWrapper &plugin = getPlugin(ctx, ctx.opts.init_send_channel);

    RaceHandle connectionHandle = ctx.manager.getCore().generateHandle();

    ctx.manager.registerHandle(ctx, connectionHandle);

    nlohmann::json json = {};

    if (ctx.shouldCreateSender(ctx.opts.final_send_channel)) {
      if (ctx.finalSendLinkAddress.empty()) {
        helper::logError(logPrefix + "finalSend should have been created but there is no address");
        return EventResult::NOT_SUPPORTED;
      } else {
      // Json name is relative to the recipient, so this is a "recv" link for them
        json["finalRecvLinkAddress"] = ctx.finalSendLinkAddress;
        json["finalRecvChannel"] = ctx.opts.final_send_channel;
      }
    }
    if (ctx.shouldCreateReceiver(ctx.opts.final_recv_channel)) {
      if (ctx.finalRecvLinkAddress.empty()) {
        helper::logError(logPrefix + "finalRecv should have been created but there is no address");
        return EventResult::NOT_SUPPORTED;
      } else {
        // Json name is relative to the recipient, so this is a "send" link for them
        json["finalSendLinkAddress"] = ctx.finalRecvLinkAddress;
        json["finalSendChannel"] = ctx.opts.final_recv_channel;
      }

    std::string message = json.dump();
    std::vector<uint8_t> bytes(message.begin(), message.end());
    std::vector<uint8_t> prefixedBytes;
    prefixedBytes.reserve(bytes.size() + packageIdLen);
    prefixedBytes.insert(prefixedBytes.end(), ctx.packageId.begin(),
                         ctx.packageId.end());
    prefixedBytes.insert(prefixedBytes.end(), bytes.begin(), bytes.end());
    EncPkg pkg(0, 0, prefixedBytes);
    RaceHandle pkgHandle = ctx.manager.getCore().generateHandle();
    SdkResponse response =
        plugin.sendPackage(pkgHandle, ctx.initSendConnId, pkg, 0, 0);
    ctx.manager.registerHandle(ctx, pkgHandle);

    if (response.status != SdkStatus::SDK_OK) {
      return EventResult::NOT_SUPPORTED;
    }
    }
    return EventResult::SUCCESS;
  }
};

struct StateBootstrapPreConduitFinished : public BootstrapPreConduitState {
  explicit StateBootstrapPreConduitFinished(StateType id = STATE_BOOTSTRAP_PRE_CONN_OBJ_FINISHED)
      : BootstrapPreConduitState(id, "STATE_BOOTSTRAP_PRE_CONN_OBJ_FINISHED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    RaceHandle connObjectApiHandle = ctx.manager.getCore().generateHandle();
    RaceHandle connObjectHandle = ctx.manager.startConduitectStateMachine(
        ctx.handle, ctx.finalRecvConnSMHandle, ctx.finalRecvConnId, ctx.finalSendConnSMHandle,
        ctx.finalSendConnId, ctx.opts.final_send_channel, ctx.opts.final_recv_channel, ctx.packageId,
        {std::move(ctx.recvQueue)}, connObjectApiHandle);
    ctx.recvQueue.clear();
    if (connObjectHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix +
                       " starting connection object state machine failed");
      return EventResult::NOT_SUPPORTED;
    }


    // No longer using the initial send link
    ctx.manager.unregisterHandle(ctx, ctx.initSendConnSMHandle);
    bool success = ctx.manager.detachConnSM(ctx.handle, ctx.initSendConnSMHandle);
    if (!success) {
      helper::logError(logPrefix + "detachConnSM failed");
      return EventResult::NOT_SUPPORTED;
    }

    // No longer using the initial recv link
    ctx.manager.unregisterHandle(ctx, ctx.initRecvConnSMHandle);
    success = ctx.manager.detachConnSM(ctx.handle, ctx.initRecvConnSMHandle);
    if (!success) {
      helper::logError(logPrefix + "detachConnSM failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.acceptCb(ApiStatus::OK, connObjectApiHandle);
    ctx.acceptCb = {};

    ctx.manager.stateMachineFinished(ctx);
    return EventResult::SUCCESS;
  }
  virtual bool finalState() { return true; }
};

struct StateBootstrapPreConduitFailed : public BootstrapPreConduitState {
  explicit StateBootstrapPreConduitFailed(StateType id = STATE_BOOTSTRAP_PRE_CONN_OBJ_FAILED)
      : BootstrapPreConduitState(id, "STATE_BOOTSTRAP_PRE_CONN_OBJ_FAILED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.acceptCb) {
      ctx.acceptCb(ApiStatus::INTERNAL_ERROR, {});
      ctx.acceptCb = {};
    }

    ctx.manager.stateMachineFailed(ctx);
    return EventResult::SUCCESS;
  }
};

//-----------------------------------------------------------------------------------------------
// StateEngine
//-----------------------------------------------------------------------------------------------

BootstrapPreConduitStateEngine::BootstrapPreConduitStateEngine() {
  addInitialState<StateBootstrapPreConduitInitial>(STATE_BOOTSTRAP_PRE_CONN_OBJ_INITIAL);
  addState<StateBootstrapPreConduitAccepted>(STATE_BOOTSTRAP_PRE_CONN_OBJ_ACCEPTED);
  addState<StateBootstrapPreConduitWaitingForConnections>(STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS);
  addState<StateBootstrapPreConduitSendResponse>(STATE_BOOTSTRAP_PRE_CONN_OBJ_SEND_RESPONSE);
  addState<StateBootstrapPreConduitFinished>(STATE_BOOTSTRAP_PRE_CONN_OBJ_FINISHED);
  addFailedState<StateBootstrapPreConduitFailed>(STATE_BOOTSTRAP_PRE_CONN_OBJ_FAILED);

  // clang-format off
    // initial -> opening -> open
    declareStateTransition(STATE_BOOTSTRAP_PRE_CONN_OBJ_INITIAL,   EVENT_RECEIVE_PACKAGE,              STATE_BOOTSTRAP_PRE_CONN_OBJ_INITIAL);
    declareStateTransition(STATE_BOOTSTRAP_PRE_CONN_OBJ_INITIAL,   EVENT_LISTEN_ACCEPTED,              STATE_BOOTSTRAP_PRE_CONN_OBJ_ACCEPTED);
    declareStateTransition(STATE_BOOTSTRAP_PRE_CONN_OBJ_ACCEPTED,  EVENT_ALWAYS,                       STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS);
    declareStateTransition(STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS,      EVENT_CONN_STATE_MACHINE_CONNECTED,              STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS);
    declareStateTransition(STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS,      EVENT_NEEDS_SEND,              STATE_BOOTSTRAP_PRE_CONN_OBJ_SEND_RESPONSE);
    declareStateTransition(STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS,      EVENT_SATISFIED,              STATE_BOOTSTRAP_PRE_CONN_OBJ_FINISHED);
    declareStateTransition(STATE_BOOTSTRAP_PRE_CONN_OBJ_SEND_RESPONSE,      EVENT_PACKAGE_SENT,              STATE_BOOTSTRAP_PRE_CONN_OBJ_FINISHED);
  // clang-format on
}

std::string BootstrapPreConduitStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
