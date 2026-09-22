
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
    RaceHandle helloConnSMHandle,
    const ConnectionID &helloConnId,
    const std::string &_packageId,
    std::vector<std::vector<uint8_t>> recvMessages) {
  this->parentHandle = contextHandle;
  this->opts = BootstrapConnectionOptions(parentContext.opts);

  // Use the specific connSM/connId that delivered THIS client's hello,
  // rather than the listen context's single (first-client-only) init
  // handles - required for multiple concurrent clients sharing a merged
  // single-bidi init link, where each client's own connection must be used
  // to send that client's own hello response (see StateBootstrapPreConduitSendResponse).
  this->initRecvConnSMHandle = helloConnSMHandle;
  this->initRecvConnId = helloConnId;
  this->initRecvLinkAddress = parentContext.initRecvLinkAddress;

  if (parentContext.initUsingSingleBidiConnection) {
    this->initSendConnSMHandle = helloConnSMHandle;
    this->initSendConnId = helloConnId;
    this->initSendLinkAddress = parentContext.initSendLinkAddress;
  } else {
    this->initSendConnSMHandle = parentContext.initSendConnSMHandle;
    this->initSendConnId = parentContext.initSendConnId;
    this->initSendLinkAddress = parentContext.initSendLinkAddress;
  }
  helper::logInfo("updateBootstrapPreConduitStateMachineStart initSendLinkAddress: " + parentContext.initSendLinkAddress);

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
    std::string linkAddress, LinkID /* linkId */) {
  if (this->initRecvConnSMHandle == contextHandle) {
    this->initRecvConnId = connId;
    this->initRecvLinkAddress = linkAddress;
  } else if (this->initSendConnSMHandle == contextHandle) {
    this->initSendConnId = connId;
    this->initSendLinkAddress = linkAddress;
  } else if (this->finalSendConnSMHandle == contextHandle) {
    this->finalSendConnId = connId;
    this->finalSendLinkAddress = linkAddress;
    // For a single bidirectional final link, finalRecvConnSMHandle is
    // aliased to the same handle - update both sides so neither is left
    // permanently empty (see StateBootstrapPreConduitWaitingForConnections).
    if (this->finalRecvConnSMHandle == contextHandle) {
      this->finalRecvConnId = connId;
      this->finalRecvLinkAddress = linkAddress;
    }
  } else if (this->finalRecvConnSMHandle == contextHandle) {
    this->finalRecvConnId = connId;
    this->finalRecvLinkAddress = linkAddress;
  }
}

void BootstrapPreConduitContext::updateConnStateMachineLinkEstablished(
    RaceHandle contextHandle, LinkID /* linkId */, std::string linkAddress) {
  if (this->finalSendConnSMHandle == contextHandle) {
    this->finalSendLinkAddress = linkAddress;
    // For a single bidirectional final link, finalRecvConnSMHandle is
    // aliased to the same handle - update both sides so neither is left
    // permanently empty (see StateBootstrapPreConduitWaitingForConnections).
    if (this->finalRecvConnSMHandle == contextHandle) {
      this->finalRecvLinkAddress = linkAddress;
    }
  } else if (this->finalRecvConnSMHandle == contextHandle) {
    this->finalRecvLinkAddress = linkAddress;
  }
}

// void BootstrapPreConduitContext::updateConnStateMachineConnected(
//     RaceHandle /* contextHandle */, ConnectionID connId,
//     std::string /* linkAddress */) {
//   this->finalSendConnId = connId;
// }

void BootstrapPreConduitContext::updateListenAccept(
    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> cb) {
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
    helper::logDebug(logPrefix + " readiness handles/ids: finalRecv=" +
                     std::to_string(ctx.finalRecvConnSMHandle) + "/" +
                     ctx.finalRecvConnId + " finalSend=" +
                     std::to_string(ctx.finalSendConnSMHandle) + "/" +
                     ctx.finalSendConnId);

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
        helper::logInfo(logPrefix + "Loading init-send link on " +ctx.opts.init_send_channel + " with address: " + ctx.initSendLinkAddress);
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


     // *** FINAL SEND/RECV ***
    const bool finalUsesSingleBidiLink =
        ctx.opts.final_recv_channel.empty() ||
        ctx.shouldUseSingleBidiLink(ctx.opts.final_send_channel,
                                   ctx.opts.final_recv_channel);

    if (finalUsesSingleBidiLink) {
      // shouldCreateSender() only consults the channel's static LD_BIDI
      // manifest property, so it returns the same answer on both the
      // listener and the dialer - it can't break the tie for a merged
      // bidirectional link. Mirror the non-bootstrap convention instead
      // (DialStateMachine/ListenStateMachine): the listener always creates
      // the shared final link here and reports its address back to the
      // dialer in the hello response (StateBootstrapPreConduitSendResponse);
      // the dialer always loads it (StateBootstrapDialRecvResponse).
      const bool create = true;
      const bool sending = true;
      helper::logInfo(logPrefix + "Using a single bidirectional final link for channel '" +
                      ctx.opts.final_send_channel + "'");
      if (create) {
        ctx.finalSendConnSMHandle = ctx.manager.startConnStateMachine(
            ctx.handle, ctx.opts.final_send_channel, ctx.opts.final_send_role,
            "", create, sending);
      } else if (ctx.finalSendLinkAddress.empty()) {
        helper::logError(logPrefix + " finalSend address is missing (was it sent in the hello?)");
        return EventResult::NOT_SUPPORTED;
      } else {
        ctx.finalSendConnSMHandle = ctx.manager.startConnStateMachine(
            ctx.handle, ctx.opts.final_send_channel, ctx.opts.final_send_role,
            ctx.finalSendLinkAddress, create, sending);
      }
      if (ctx.finalSendConnSMHandle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + " starting connection state machine failed");
        return EventResult::NOT_SUPPORTED;
      }
      ctx.manager.registerHandle(ctx, ctx.finalSendConnSMHandle);
      ctx.finalRecvConnSMHandle = ctx.finalSendConnSMHandle;
      ctx.finalRecvConnId = ctx.finalSendConnId;
      ctx.finalRecvLinkAddress = ctx.finalSendLinkAddress;
      ctx.finalUsingSingleBidiConnection = true;
    } else {
      // Handle final server->client aka final_send
      // shouldCreateSender()/shouldCreateReceiver() only consult the channel's
      // static LD_BIDI manifest property, so they return the same answer on
      // both listener and dialer regardless of role - the listener must
      // always create both final links here (and report their addresses in
      // the hello response below) while the dialer always loads them.
      bool create = true;

      // We are creating, we will create and then send this address in a hello response
      if (create) {
        bool sending = true;
        helper::logInfo(logPrefix + "Creating final-send link on " + ctx.opts.final_send_channel);
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
      helper::logInfo(logPrefix + "Loading final-send link on " + ctx.opts.final_send_channel + " with address: " + ctx.finalSendLinkAddress);
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
      create = true;

      // We are creating, we will create and then send this address in a hello response
      if (create) {
        bool sending = false;
        helper::logInfo(logPrefix + "Creating final-send link on " + ctx.opts.final_recv_channel);
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
      helper::logInfo(logPrefix + "Loading final-recv link on " + ctx.opts.final_recv_channel + " with address: " + ctx.finalRecvLinkAddress);
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
    if (ctx.finalSendConnSMHandle != NULL_RACE_HANDLE && ctx.finalSendConnId.empty() &&
        !ctx.finalUsingSingleBidiConnection) {
      // A single bidirectional final link we created is a listening socket;
      // its "connection" won't exist until a peer dials in, which can't
      // happen until we've told them the address via the hello response
      // below. Only gate on the real connection for the non-merged case.
      return EventResult::SUCCESS;
    }

    if (ctx.shouldCreateSender(ctx.opts.final_send_channel) &&
        ctx.finalSendLinkAddress.empty()) {
      return EventResult::SUCCESS;
    }
    if (ctx.shouldCreateReceiver(ctx.opts.final_recv_channel) &&
        ctx.finalRecvLinkAddress.empty()) {
      return EventResult::SUCCESS;
    }

    if (!ctx.responseSent &&
        (ctx.shouldCreateSender(ctx.opts.final_send_channel) or
       ctx.shouldCreateReceiver(ctx.opts.final_recv_channel))) {
      ctx.pendingEvents.push(EVENT_NEEDS_SEND);
    } else if (ctx.finalRecvConnSMHandle != NULL_RACE_HANDLE &&
               ctx.finalRecvConnId.empty()) {
      return EventResult::SUCCESS;
    } else {
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

    // The listener always creates both final links (see
    // StateBootstrapPreConduitAccepted) and reports their addresses here for
    // the dialer to load - shouldCreateSender()/shouldCreateReceiver() can't
    // gate this, they return the same answer regardless of role for LD_BIDI
    // channels (this previously silently omitted finalRecvLinkAddress
    // whenever shouldCreateSender() was false, which is unconditionally the
    // case for every LD_BIDI channel).
    if (ctx.finalSendLinkAddress.empty()) {
      helper::logError(logPrefix + "finalSend should have been created but there is no address");
      return EventResult::NOT_SUPPORTED;
    }
    // Json name is relative to the recipient, so this is a "recv" link for them
    json["finalRecvLinkAddress"] = ctx.finalSendLinkAddress;
    json["finalRecvChannel"] = ctx.opts.final_send_channel;

    if (ctx.finalRecvLinkAddress.empty()) {
      helper::logError(logPrefix + "finalRecv should have been created but there is no address");
      return EventResult::NOT_SUPPORTED;
    }
    // Json name is relative to the recipient, so this is a "send" link for them
    json["finalSendLinkAddress"] = ctx.finalRecvLinkAddress;
    json["finalSendChannel"] = ctx.opts.final_recv_channel;

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
    ctx.responseSent = true;
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

    // No longer using the initial recv link - skip if aliased to the same
    // handle as initSend (merged single-bidi init link), already detached above.
    if (ctx.initRecvConnSMHandle != ctx.initSendConnSMHandle) {
      ctx.manager.unregisterHandle(ctx, ctx.initRecvConnSMHandle);
      success = ctx.manager.detachConnSM(ctx.handle, ctx.initRecvConnSMHandle);
      if (!success) {
        helper::logError(logPrefix + "detachConnSM failed");
        return EventResult::NOT_SUPPORTED;
      }
    }

    ctx.acceptCb(ApiStatus::OK, connObjectApiHandle, {});
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
      helper::logDebug(logPrefix + "accept callback not null");
      ctx.acceptCb(ApiStatus::INTERNAL_ERROR, {}, {});
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
    declareStateTransition(STATE_BOOTSTRAP_PRE_CONN_OBJ_SEND_RESPONSE,      EVENT_PACKAGE_SENT,              STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS);
  // clang-format on
}

std::string BootstrapPreConduitStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
