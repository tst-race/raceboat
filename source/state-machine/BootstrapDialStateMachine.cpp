
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

#include "BootstrapDialStateMachine.h"

#include "Core.h"
#include "Events.h"
#include "PluginWrapper.h"
#include "States.h"
#include "base64.h"
#include "race/common/EncPkg.h"

namespace Raceboat {

//-----------------------------------------------------------------------------------------------
// Context
//-----------------------------------------------------------------------------------------------

void ApiBootstrapDialContext::updateBootstrapDial(const BootstrapConnectionOptions &options,
                                std::vector<uint8_t> &&_data,
                                std::function<void(ApiStatus, RaceHandle, ConduitProperties)> cb) {
  this->opts = options;
  this->helloData = _data;
  this->dialCallback = cb;
}

void ApiBootstrapDialContext::updateReceiveEncPkg(
    ConnectionID /* _connId */, std::shared_ptr<std::vector<uint8_t>> _data) {
  this->responseData.push(std::move(_data));
}
  
  // TODO Code Reuse
void ApiBootstrapDialContext::updateConnStateMachineConnected(RaceHandle contextHandle,
                                                     ConnectionID connId,
                                                     std::string linkAddress) {
  helper::logDebug(" Received ConnStateMachineConnected for handle " + std::to_string(contextHandle) + " and ConnID: " + connId);
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
//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StateBootstrapDialInitial : public BootstrapDialState {
  explicit StateBootstrapDialInitial(StateType id = STATE_BOOTSTRAP_DIAL_INITIAL)
      : BootstrapDialState(id, "StateBootstrapDialInitial") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    // Generate and set packageId
    if (ctx.packageId.empty()) {
      std::vector<uint8_t> packageIdBytes =
        ctx.manager.getCore().getEntropy(packageIdLen);
      ctx.packageId = std::string(packageIdBytes.begin(), packageIdBytes.end());
      helper::logInfo(logPrefix +
                       "Set PackageID to " + ctx.packageId);
    }

    // Handle initial client->server aka init_send
    bool create = ctx.shouldCreateSender(ctx.opts.init_send_channel);

    // SPECIAL CASE: we are going to need to create this link and then transmit the address out-of-band to the listener before they run listen
    if (create) {
      helper::logError(logPrefix + " creating initial send link on the client is not yet supported (required for channel: " + ctx.opts.init_send_channel + ")");
      // TODO
    }
    // Normal case: we are provided an address from out-of-band to load
    else {
      helper::logInfo(logPrefix + " Loading initial-send link on " + ctx.opts.init_send_channel);
      if (ctx.opts.init_send_address.empty()) {
        // Need an address to load
        helper::logError(logPrefix +
                         "Invalid options: initial send address is required");
        ctx.dialCallback(ApiStatus::CHANNEL_INVALID, {}, {});
        ctx.dialCallback = {};
        return EventResult::NOT_SUPPORTED;
      }
      bool sending = true;
      ctx.initSendConnSMHandle = ctx.manager.
        startConnStateMachine(ctx.handle,
                              ctx.opts.init_send_channel,
                              ctx.opts.init_send_role,
                              ctx.opts.init_send_address,
                              create, // is false
                              sending // is true
                              );
    }
    if (ctx.initSendConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }
    ctx.manager.registerHandle(ctx, ctx.initSendConnSMHandle);


    // Skip initial recv channel if it is empty
    // This SHOULD indicate the init_send_channel is bidirectional, otherwise this is a mistake and should be caught earlier during input validation
    if (ctx.opts.init_recv_channel.empty()) {
      // TODO handle opening a receiving connection in addition to the sending connection on the "send" link above
    }
    else {
      // Handle initial server->client aka init_recv
      create = ctx.shouldCreateReceiver(ctx.opts.init_recv_channel);
      // No special cases here: if we are loading we should have a recv_address, if we are creating we will send the address in the hello message
      if (create) {
        helper::logInfo(logPrefix + " Creating init-recv link on " + ctx.opts.init_recv_channel);
      } else {
        helper::logInfo(logPrefix + " Loading init-recv link on " + ctx.opts.init_recv_channel);
        if (ctx.opts.init_recv_address.empty()) {
          // Need an address to load
          helper::logError(logPrefix +
                           "Invalid options: initial recv address is required");
          ctx.dialCallback(ApiStatus::CHANNEL_INVALID, {}, {});
          ctx.dialCallback = {};
          return EventResult::NOT_SUPPORTED;
        }
      }
      bool sending = false;
      ctx.initRecvConnSMHandle = ctx.manager.
        startConnStateMachine(ctx.handle,
                              ctx.opts.init_recv_channel,
                              ctx.opts.init_recv_role,
                              ctx.opts.init_recv_address,
                              create, // is true
                              sending // is false
                              );

      if (ctx.initRecvConnSMHandle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + " starting connection state machine failed");
        return EventResult::NOT_SUPPORTED;
      }

      ctx.manager.registerHandle(ctx, ctx.initRecvConnSMHandle);
    }


    // Handle final client->server aka final_send
    create = ctx.shouldCreateSender(ctx.opts.final_send_channel);

    // If we are NOT creating then we have to wait for the server to create and send us the address as a hello-response
    if (create) {
      helper::logDebug(logPrefix + " Creating final-send link on " + ctx.opts.final_send_channel);
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
    } else {
      // Explicitly do not expect this connection until the server response
      helper::logDebug(logPrefix + " waiting on server to provide final-send link");
      // ctx.finalSendConnSMHandle = NULL_RACE_HANDLE;
    }
  
    // Skip final recv channel if it is empty
    // This SHOULD indicate the final_send_channel is bidirectional, otherwise this is a mistake and should be caught earlier during input validation
    if (ctx.opts.final_recv_channel.empty()) {
      // TODO handle opening a receiving connection in addition to the sending connection on the "send" link above
    }
    else {
      // Handle finalial server->client aka final_recv
      create = ctx.shouldCreateReceiver(ctx.opts.final_recv_channel);

      // If we are NOT creating then we are waiting for the server to create and send the address as a hello-response
      if (create) {
        helper::logDebug(logPrefix + " Creating final-recv link on " + ctx.opts.final_recv_channel);
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
      } else {
        // Explicitly do not expect this connection until the server response
        helper::logDebug(logPrefix + " waiting on server to provide final-recv link");
        // ctx.finalRecvConnSMHandle = NULL_RACE_HANDLE;
      }
    }
    ctx.pendingEvents.push(EVENT_ALWAYS);
    return EventResult::SUCCESS;
  }
};

  // TODO Code Reuse
struct StateBootstrapDialWaitingForConnections : public BootstrapDialState {
  explicit StateBootstrapDialWaitingForConnections(
      StateType id = STATE_BOOTSTRAP_DIAL_WAITING_FOR_CONNECTIONS)
      : BootstrapDialState(id, "StateBootstrapDialWaitingForConnections") {}
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
    ctx.pendingEvents.push(EVENT_SATISFIED);
    return EventResult::SUCCESS;
  }
};

struct StateBootstrapDialSendHello : public BootstrapDialState {
  explicit StateBootstrapDialSendHello(StateType id = STATE_BOOTSTRAP_DIAL_SEND_HELLO)
      : BootstrapDialState(id, "StateBootstrapDialSendHello") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    PluginWrapper &plugin = getPlugin(ctx, ctx.opts.init_send_channel);

    RaceHandle connectionHandle = ctx.manager.getCore().generateHandle();

    ctx.manager.registerHandle(ctx, connectionHandle);
    // ctx.manager.registerId(ctx, ctx.recvConnId);

    // TODO: There's better ways to encode than base64 inside json
    helper::logDebug(logPrefix + "encoding packageId");
    nlohmann::json json = {
        {"packageId", base64::encode(std::vector<uint8_t>(
                          ctx.packageId.begin(), ctx.packageId.end()))},
    };
    helper::logDebug(logPrefix + "Creating links");

    //
    if (ctx.shouldCreateReceiver(ctx.opts.init_recv_channel) and !ctx.initRecvLinkAddress.empty()) {
      // Json name is relative to the recipient, so this is a "send" link for them
        json["initSendLinkAddress"] = ctx.initRecvLinkAddress;
        json["initSendChannel"] = ctx.opts.init_recv_channel;
    }
    helper::logDebug(logPrefix + " sendlink");
    if (!ctx.finalSendLinkAddress.empty()) {
      // Json name is relative to the recipient, so this is a "recv" link for them
        json["finalRecvLinkAddress"] = ctx.finalSendLinkAddress;
        json["finalRecvChannel"] = ctx.opts.final_send_channel;
    }
    helper::logDebug(logPrefix + " recvLink");
    if (!ctx.finalRecvLinkAddress.empty()) {
      // Json name is relative to the recipient, so this is a "send" link for them
        json["finalSendLinkAddress"] = ctx.finalRecvLinkAddress;
        json["finalSendChannel"] = ctx.opts.final_recv_channel;
    }

    helper::logDebug(logPrefix + "encoding hello");
    std::string dataB64 = base64::encode(std::move(ctx.helloData));
    json["message"] = dataB64;
    ctx.helloData.clear();
    helper::logDebug(logPrefix + "converting packageId to bytes");

    helper::logDebug(logPrefix + " json.dump");
    std::string message = std::string(packageIdLen, '\0') + json.dump();
    std::vector<uint8_t> bytes(message.begin(), message.end());

    EncPkg pkg(0, 0, bytes);
    RaceHandle pkgHandle = ctx.manager.getCore().generateHandle();
    helper::logDebug(logPrefix + " send package");
    SdkResponse response =
        plugin.sendPackage(pkgHandle, ctx.initSendConnId, pkg, 0, 0);
    ctx.manager.registerHandle(ctx, pkgHandle);

    if (response.status != SdkStatus::SDK_OK) {
      return EventResult::NOT_SUPPORTED;
    }

    return EventResult::SUCCESS;
  }
};


struct StateBootstrapDialHelloSent : public BootstrapDialState {
  explicit StateBootstrapDialHelloSent(StateType id = STATE_BOOTSTRAP_DIAL_HELLO_SENT)
      : BootstrapDialState(id, "StateBootstrapDialHelloSent") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    // If any final links are missing, we must be waiting for the server to send us addresses in a response
    if (ctx.finalSendConnId.empty() or ctx.finalRecvConnId.empty()) {
      // Register to listen for the response from the server
      ctx.manager.registerPackageId(ctx, ctx.initRecvConnId, ctx.packageId);
    
      ctx.pendingEvents.push(EVENT_NEEDS_RECV);
      return EventResult::SUCCESS;
    }

    ctx.pendingEvents.push(EVENT_SATISFIED);
    return EventResult::SUCCESS;
  }
};
  
struct StateBootstrapDialAwaitResponse : public BootstrapDialState {
  explicit StateBootstrapDialAwaitResponse(StateType id = STATE_BOOTSTRAP_DIAL_AWAIT_RESPONSE)
      : BootstrapDialState(id, "StateBootstrapDialAwaitResponse") {}
};

    
struct StateBootstrapDialRecvResponse : public BootstrapDialState {
  explicit StateBootstrapDialRecvResponse(StateType id = STATE_BOOTSTRAP_DIAL_RECV_RESPONSE)
      : BootstrapDialState(id, "StateBootstrapDialRecvResponse") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    while (!ctx.responseData.empty()) {
      auto data = std::move(ctx.responseData.front());
      ctx.responseData.pop();

      try {
        std::string str{data->begin(), data->end()};
        nlohmann::json json = nlohmann::json::parse(str);
        if (ctx.finalSendConnId.empty()) {
          LinkAddress finalSendLinkAddress = json.at("finalSendLinkAddress");
          std::string finalSendChannel = json.at("finalSendChannel");
          helper::logInfo(logPrefix + "loading finalSendLink: " + ctx.opts.final_send_channel + " " + finalSendLinkAddress);
          if (ctx.opts.final_send_channel != finalSendChannel) {
            helper::logError(logPrefix + "Requested final channel does not match specified final channel: " + finalSendChannel + " vs. " + ctx.opts.final_send_channel);
            continue;
          }

          bool sending = true;
          bool create = false;
          ctx.finalSendConnSMHandle = ctx.manager.
            startConnStateMachine(ctx.handle,
                                  ctx.opts.final_send_channel,
                                  ctx.opts.final_send_role,
                                  finalSendLinkAddress,
                                  create, // is false
                                  sending // is true
                                  );
          
          if (ctx.finalSendConnSMHandle == NULL_RACE_HANDLE) {
            helper::logError(logPrefix + " starting connection state machine failed");
            return EventResult::NOT_SUPPORTED;
          }
          ctx.manager.registerHandle(ctx, ctx.finalSendConnSMHandle);
          
        }
        if (ctx.finalRecvConnId.empty()) {
          LinkAddress finalRecvLinkAddress = json.at("finalRecvLinkAddress");
          std::string finalRecvChannel = json.at("finalRecvChannel");
          if (ctx.opts.final_recv_channel != finalRecvChannel) {
            helper::logError(logPrefix + "Requested final channel does not match specified final channel: " + finalRecvChannel + " vs. " + ctx.opts.final_recv_channel);
            continue;
          }

          bool sending = false;
          bool create = false;
          ctx.finalRecvConnSMHandle = ctx.manager.
            startConnStateMachine(ctx.handle,
                                  ctx.opts.final_recv_channel,
                                  ctx.opts.final_recv_role,
                                  finalRecvLinkAddress,
                                  create, // is false
                                  sending // is false
                                  );
          
          if (ctx.finalRecvConnSMHandle == NULL_RACE_HANDLE) {
            helper::logError(logPrefix + " starting connection state machine failed");
            return EventResult::NOT_SUPPORTED;
          }
          ctx.manager.registerHandle(ctx, ctx.finalRecvConnSMHandle);

        }
 
        // Processing succeeded
        ctx.pendingEvents.push(EVENT_SATISFIED);
        return EventResult::SUCCESS;
      } catch (std::exception &e) {
        helper::logError(logPrefix +
                         "Failed to process received message: " + e.what());
      }
    }
    return EventResult::SUCCESS;
  }
};

  // TODO Code Reuse
struct StateBootstrapDialWaitingForFinalConnections : public BootstrapDialState {
  explicit StateBootstrapDialWaitingForFinalConnections(StateType id = STATE_BOOTSTRAP_DIAL_WAITING_FOR_FINAL_CONNECTIONS)
      : BootstrapDialState(id, "StateBootstrapDialWaitingForFinalConnections") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    if (ctx.finalRecvConnSMHandle != NULL_RACE_HANDLE and ctx.finalRecvConnId.empty()) {
      return EventResult::SUCCESS;
    }
    if (ctx.finalSendConnSMHandle != NULL_RACE_HANDLE and ctx.finalSendConnId.empty()) {
      return EventResult::SUCCESS;
    }

    // No early returns indicate all expected connections are satisfied
    ctx.pendingEvents.push(EVENT_SATISFIED);
    return EventResult::SUCCESS;

  }
};
  
struct StateBootstrapDialFinished : public BootstrapDialState {
  explicit StateBootstrapDialFinished(StateType id = STATE_BOOTSTRAP_DIAL_FINISHED)
      : BootstrapDialState(id, "StateBootstrapDialFinished") {}
  virtual bool finalState() { return true; }
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    RaceHandle connObjectApiHandle = ctx.manager.getCore().generateHandle();

    RaceHandle connObjectHandle = ctx.manager.startConduitectStateMachine(
        ctx.handle, ctx.finalRecvConnSMHandle, ctx.finalRecvConnId, ctx.finalSendConnSMHandle,
        ctx.finalSendConnId, ctx.opts.final_send_channel, ctx.opts.final_recv_channel,
        ctx.packageId, {}, connObjectApiHandle);
    if (connObjectHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix +
                       " starting connection object state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    // TODO
    ctx.dialCallback(ApiStatus::OK, connObjectApiHandle, {});
    ctx.dialCallback = {};

    ctx.manager.stateMachineFinished(ctx);
    return EventResult::SUCCESS;
  }
};

struct StateBootstrapDialFailed : public BootstrapDialState {
  explicit StateBootstrapDialFailed(StateType id = STATE_BOOTSTRAP_DIAL_FAILED)
      : BootstrapDialState(id, "StateBootstrapDialFailed") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.dialCallback) {
      helper::logDebug(logPrefix + "dial callback not null");
      ctx.dialCallback(ApiStatus::INTERNAL_ERROR, {}, {});
      ctx.dialCallback = {};
    }

    ctx.manager.stateMachineFailed(ctx);
    return EventResult::SUCCESS;
  }
};

//-----------------------------------------------------------------------------------------------
// StateEngine
//-----------------------------------------------------------------------------------------------

BootstrapDialStateEngine::BootstrapDialStateEngine() {
  // calls startConnectionStateMachine twice, waits for
  // connStateMachineConnected
  addInitialState<StateBootstrapDialInitial>(STATE_BOOTSTRAP_DIAL_INITIAL);
  // does nothing, waits for a second connStateMachineConnected call
  addState<StateBootstrapDialWaitingForConnections>(
      STATE_BOOTSTRAP_DIAL_WAITING_FOR_CONNECTIONS);
  // sends initial package with recvLinkAddress and message if any, waits for
  // package sent
  addState<StateBootstrapDialSendHello>(STATE_BOOTSTRAP_DIAL_SEND_HELLO);
  // creates connection object, calls dial callback, calls state machine
  addState<StateBootstrapDialHelloSent>(STATE_BOOTSTRAP_DIAL_HELLO_SENT);
  addState<StateBootstrapDialAwaitResponse>(STATE_BOOTSTRAP_DIAL_AWAIT_RESPONSE);
  addState<StateBootstrapDialRecvResponse>(STATE_BOOTSTRAP_DIAL_RECV_RESPONSE);
  addState<StateBootstrapDialWaitingForFinalConnections>(STATE_BOOTSTRAP_DIAL_WAITING_FOR_FINAL_CONNECTIONS);
   // finished on manager, final state
  addState<StateBootstrapDialFinished>(STATE_BOOTSTRAP_DIAL_FINISHED);
  // call state machine failed, manager will forward event to connection state
  // machine
  addFailedState<StateBootstrapDialFailed>(STATE_BOOTSTRAP_DIAL_FAILED);

  // clang-format off
    declareStateTransition(STATE_BOOTSTRAP_DIAL_INITIAL,                 EVENT_ALWAYS, STATE_BOOTSTRAP_DIAL_WAITING_FOR_CONNECTIONS);
    declareStateTransition(STATE_BOOTSTRAP_DIAL_WAITING_FOR_CONNECTIONS, EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_BOOTSTRAP_DIAL_WAITING_FOR_CONNECTIONS);
    declareStateTransition(STATE_BOOTSTRAP_DIAL_WAITING_FOR_CONNECTIONS, EVENT_SATISFIED,                    STATE_BOOTSTRAP_DIAL_SEND_HELLO);
    declareStateTransition(STATE_BOOTSTRAP_DIAL_SEND_HELLO,              EVENT_PACKAGE_SENT,                 STATE_BOOTSTRAP_DIAL_HELLO_SENT);
    declareStateTransition(STATE_BOOTSTRAP_DIAL_HELLO_SENT,              EVENT_NEEDS_RECV,                STATE_BOOTSTRAP_DIAL_AWAIT_RESPONSE);
    declareStateTransition(STATE_BOOTSTRAP_DIAL_AWAIT_RESPONSE,          EVENT_RECEIVE_PACKAGE,                STATE_BOOTSTRAP_DIAL_RECV_RESPONSE);
    declareStateTransition(STATE_BOOTSTRAP_DIAL_RECV_RESPONSE,           EVENT_SATISFIED,              STATE_BOOTSTRAP_DIAL_WAITING_FOR_FINAL_CONNECTIONS);
    declareStateTransition(STATE_BOOTSTRAP_DIAL_WAITING_FOR_FINAL_CONNECTIONS,              EVENT_CONN_STATE_MACHINE_CONNECTED,                    STATE_BOOTSTRAP_DIAL_WAITING_FOR_FINAL_CONNECTIONS);
    declareStateTransition(STATE_BOOTSTRAP_DIAL_WAITING_FOR_FINAL_CONNECTIONS,              EVENT_SATISFIED,                    STATE_BOOTSTRAP_DIAL_FINISHED);
    declareStateTransition(STATE_BOOTSTRAP_DIAL_HELLO_SENT,              EVENT_SATISFIED,                    STATE_BOOTSTRAP_DIAL_FINISHED);
  // clang-format on
}

std::string BootstrapDialStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
