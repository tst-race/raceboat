
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

#include "BootstrapListenStateMachine.h"

#include "../../include/race/Race.h"
#include "Core.h"
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

void ApiBootstrapListenContext::updateBootstrapListen(
    const BootstrapConnectionOptions &_options,
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> _cb) {
  this->opts = _options;
  this->listenCb = _cb;
};
void ApiBootstrapListenContext::updateAccept(
    RaceHandle /* _handle */, std::function<void(ApiStatus, RaceHandle)> _cb) {
  this->acceptCb.push_back(_cb);
};
void ApiBootstrapListenContext::updateClose(RaceHandle /* handle */,
                                   std::function<void(ApiStatus)> _cb) {
  this->closeCb = _cb;
}

void ApiBootstrapListenContext::updateReceiveEncPkg(
    ConnectionID /* _connId */, std::shared_ptr<std::vector<uint8_t>> _data) {
  this->data.push(std::move(_data));
};

  // TODO Code Reuse
void ApiBootstrapListenContext::updateConnStateMachineConnected(
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
};



//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StateBootstrapListenInitial : public BootstrapListenState {
  explicit StateBootstrapListenInitial(StateType id = STATE_BOOTSTRAP_LISTEN_INITIAL)
      : BootstrapListenState(id, "STATE_BOOTSTRAP_LISTEN_INITIAL") {}
  virtual EventResult enter(Context &context) {
    //
    // Overall: check for provided addresses and create/load as appropriate. If necessary, create links and output their addresses for out-of-band sharing
    //
    TRACE_METHOD();
    auto &ctx = getContext(context);

    // *** INIT SEND ***
    // Handle initial server->client aka init_send
    bool create = ctx.shouldCreateSender(ctx.opts.init_send_channel);

    // We are going to need to create this link and then transmit the address out-of-band to the dialer before they run dial
    if (create) {
      bool sending = true;
      ctx.initSendConnSMHandle = ctx.manager.
        startConnStateMachine(ctx.handle,
                              ctx.opts.init_send_channel,
                              ctx.opts.init_send_role,
                              ctx.opts.init_send_address,
                              create, // is true
                              sending // is true
                              );
    if (ctx.initSendConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }
    ctx.manager.registerHandle(ctx, ctx.initSendConnSMHandle);
    } else if (!ctx.opts.init_send_address.empty()) {
      bool sending = true;
      ctx.initSendConnSMHandle = ctx.manager.
        startConnStateMachine(ctx.handle,
                              ctx.opts.init_send_channel,
                              ctx.opts.init_send_role,
                              ctx.opts.init_send_address,
                              create, // is false
                              sending // is true
                              );
    if (ctx.initSendConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }
    ctx.manager.registerHandle(ctx, ctx.initSendConnSMHandle);
    } 

    // *** INIT RECV ***
    // Skip initial recv channel if it is empty
    // This SHOULD indicate the init_send_channel is bidirectional, otherwise this is a mistake and should be caught earlier during input validation
    if (ctx.opts.init_recv_channel.empty()) {
      // TODO handle opening a receiving connection in addition to the sending connection on the "send" link above
    }
    else {
      // Handle initial client->server aka init_recv
      create = ctx.shouldCreateReceiver(ctx.opts.init_recv_channel);
      if (create) {
      bool sending = false;
      ctx.initRecvConnSMHandle = ctx.manager.
        startConnStateMachine(ctx.handle,
                              ctx.opts.init_recv_channel,
                              ctx.opts.init_recv_role,
                              ctx.opts.init_recv_address,
                              create, // is true
                              sending // is false
                              );
      }
      else if (ctx.opts.init_recv_address.empty()) {
        // Need an address to load
        helper::logError(logPrefix +
                         "Invalid options: initial recv address is required");
        ctx.listenCb(ApiStatus::CHANNEL_INVALID, "", {});
        ctx.listenCb = {};
        return EventResult::NOT_SUPPORTED;
      } else {
      // If we are loading we should have a recv_address, if we are creating we will send the address in the hello message
      bool sending = false;
      ctx.initRecvConnSMHandle = ctx.manager.
        startConnStateMachine(ctx.handle,
                              ctx.opts.init_recv_channel,
                              ctx.opts.init_recv_role,
                              ctx.opts.init_recv_address,
                              create, // is false
                              sending // is false
                              );

      }
      if (ctx.initRecvConnSMHandle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + " starting connection state machine failed");
        return EventResult::NOT_SUPPORTED;
      }

      ctx.manager.registerHandle(ctx, ctx.initRecvConnSMHandle);
    }

    // // Handle final server->client aka final_send
    // create = ctx.shouldCreateSender(ctx.opts.final_send_channel);

    // // If we are NOT creating then we have to wait for the client to create and send us the address as a hello-response
    // if (create) {
    //   bool sending = true;
    //   ctx.finalSendConnSMHandle = ctx.manager.
    //     startConnStateMachine(ctx.handle,
    //                           ctx.opts.final_send_channel,
    //                           ctx.opts.final_send_role,
    //                           "",
    //                           create, // is true
    //                           sending // is true
    //                           );
    //   if (ctx.finalSendConnSMHandle == NULL_RACE_HANDLE) {
    //     helper::logError(logPrefix + " starting connection state machine failed");
    //     return EventResult::NOT_SUPPORTED;
    //   }
    //   ctx.manager.registerHandle(ctx, ctx.finalSendConnSMHandle);
    // }
  
    // // Skip final recv channel if it is empty
    // // This SHOULD indicate the final_send_channel is bidirectional, otherwise this is a mistake and should be caught earlier during input validation
    // if (ctx.opts.final_recv_channel.empty()) {
    //   // TODO handle opening a receiving connection in addition to the sending connection on the "send" link above
    // }
    // else {
    //   // Handle finalial client->server aka final_recv
    //   create = ctx.shouldCreateReceiver(ctx.opts.final_recv_channel);

    //   // If we are NOT creating then we are waiting for the client to create and send the address as a hello-response
    //   if (create) {
    //     bool sending = false;
    //     ctx.finalRecvConnSMHandle = ctx.manager.
    //       startConnStateMachine(ctx.handle,
    //                             ctx.opts.final_recv_channel,
    //                             ctx.opts.final_recv_role,
    //                             "",
    //                             create, // is true
    //                             sending // is false
    //                             );
      
    //     if (ctx.finalRecvConnSMHandle == NULL_RACE_HANDLE) {
    //       helper::logError(logPrefix + " starting connection state machine failed");
    //       return EventResult::NOT_SUPPORTED;
    //     }
    //     ctx.manager.registerHandle(ctx, ctx.finalRecvConnSMHandle);
    //   }
    // }
    ctx.pendingEvents.push(EVENT_ALWAYS);
    return EventResult::SUCCESS;
  }
};

  // TODO Code Reuse
struct StateBootstrapListenWaitingForConnections : public BootstrapListenState {
  explicit StateBootstrapListenWaitingForConnections(
      StateType id = STATE_BOOTSTRAP_LISTEN_WAITING_FOR_CONNECTIONS)
      : BootstrapListenState(id, "StateBootstrapListenWaitingForConnections") {}
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
    nlohmann::json json = {};

    if (!ctx.initSendLinkAddress.empty()) {
      // Json name is relative to the recipient, so this is a "recv" link for them
        json["initRecvLinkAddress"] = ctx.initSendLinkAddress;
        json["initRecvChannel"] = ctx.opts.init_send_channel;
    }
    if (!ctx.initRecvLinkAddress.empty()) {
      // Json name is relative to the recipient, so this is a "send" link for them
        json["initSendLinkAddress"] = ctx.initRecvLinkAddress;
        json["initSendChannel"] = ctx.opts.init_recv_channel;
    }

    LinkAddress multiAddress = json.dump();
    RaceHandle receiverHandle = ctx.manager.getCore().generateHandle();
    ctx.listenCb(ApiStatus::OK, multiAddress, receiverHandle);
    ctx.listenCb = {};

    ctx.manager.registerHandle(ctx, receiverHandle);
    std::string packageId(packageIdLen, '\0');
    ctx.manager.registerPackageId(ctx, ctx.initRecvConnId, packageId);

    ctx.pendingEvents.push(EVENT_SATISFIED);
    return EventResult::SUCCESS;
  }
};

struct StateBootstrapListenWaitingForHellos : public BootstrapListenState {
  explicit StateBootstrapListenWaitingForHellos(StateType id = STATE_BOOTSTRAP_LISTEN_WAITING_FOR_HELLOS)
      : BootstrapListenState(id, "STATE_BOOTSTRAP_LISTEN_WAITING_FOR_HELLOS") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    while (!ctx.data.empty()) {
      auto data = std::move(ctx.data.front());
      ctx.data.pop();

      try {
        std::string str{data->begin(), data->end()};
        nlohmann::json json = nlohmann::json::parse(str);

        ChannelId initSendChannel = "";
        // LinkAddress initSendLinkAddress = "";
        ChannelId finalRecvChannel = "";
        // LinkAddress finalRecvLinkAddress = "";
        ChannelId finalSendChannel = "";
        // LinkAddress finalSendLinkAddress = "";

        if (json.contains("initSendLinkAddress")) {
          ctx.initSendLinkAddress = json["initSendLinkAddress"];
          initSendChannel = json["initSendChannel"];
          helper::logInfo("Setting initSendLinkAddress from hello message: " + ctx.initSendLinkAddress);
        }
        if (json.contains("finalSendLinkAddress")) {
          ctx.finalSendLinkAddress = json["finalSendLinkAddress"];
          finalSendChannel = json["finalSendChannel"];
          helper::logInfo("Setting finalSendLinkAddress from hello message: " + ctx.finalSendLinkAddress);
        }
        if (json.contains("finalRecvLinkAddress")) {
          ctx.finalRecvLinkAddress = json["finalRecvLinkAddress"];
          finalRecvChannel = json["finalRecvChannel"];
          helper::logInfo("Setting finalRecvLinkAddress from hello message: " + ctx.finalRecvLinkAddress);
        }

        // TODO: validate the channel statements align with the args of the listen run unless we intend to do it dynamically based on client request

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

        std::vector<uint8_t> dialMessage = base64::decode(messageB64);

        // RaceHandle preBootstrapConnSMHandle = ctx.manager.startBootstrapPreConduitStateMachine(
        //     ctx.handle, ctx.recvConnSMHandle, ctx.recvConnId,
        //     ctx.opts.recv_channel, ctx.opts.send_channel, ctx.opts.send_role,
        //     linkAddress, replyPackageId, {std::move(dialMessage)});
        helper::logInfo(logPrefix +
                         " startBootstrapPreConduitStateMachine being called");
        RaceHandle preBootstrapConnSMHandle = ctx.manager.startBootstrapPreConduitStateMachine(
            ctx.handle,
            ctx,
            replyPackageId, {std::move(dialMessage)});


        if (preBootstrapConnSMHandle == NULL_RACE_HANDLE) {
          helper::logError(logPrefix +
                           " starting connection state machine failed");
          return EventResult::NOT_SUPPORTED;
        }

        ctx.preBootstrapConduitSM.push(preBootstrapConnSMHandle);

        break;
      } catch (std::exception &e) {
        helper::logError(logPrefix +
                         "Failed to process received message: " + e.what());
      }
    }

    while (!ctx.acceptCb.empty() && !ctx.preBootstrapConduitSM.empty()) {
      auto cb = std::move(ctx.acceptCb.front());
      ctx.acceptCb.pop_front();
      RaceHandle preBootstrapConnSMHandle = std::move(ctx.preBootstrapConduitSM.front());
      ctx.preBootstrapConduitSM.pop();
      if (!ctx.manager.onBootstrapListenAccept(preBootstrapConnSMHandle, cb)) {
        cb(ApiStatus::INTERNAL_ERROR, {});
      };
    }

    return EventResult::SUCCESS;
  }
};

struct StateBootstrapListenFinished : public BootstrapListenState {
  explicit StateBootstrapListenFinished(StateType id = STATE_BOOTSTRAP_LISTEN_FINISHED)
      : BootstrapListenState(id, "STATE_BOOTSTRAP_LISTEN_FINISHED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    for (auto &cb : ctx.acceptCb) {
      cb(ApiStatus::CLOSING, {});
    }
    ctx.acceptCb = {};

    ctx.manager.stateMachineFinished(ctx);

    ctx.closeCb(ApiStatus::OK);
    ctx.closeCb = {};
    return EventResult::SUCCESS;
  }
  virtual bool finalState() { return true; }
};

struct StateBootstrapListenFailed : public BootstrapListenState {
  explicit StateBootstrapListenFailed(StateType id = STATE_BOOTSTRAP_LISTEN_FAILED)
      : BootstrapListenState(id, "STATE_BOOTSTRAP_LISTEN_FAILED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.listenCb) {
      ctx.listenCb(ApiStatus::INTERNAL_ERROR, {}, {});
      ctx.listenCb = {};
    }

    for (auto &cb : ctx.acceptCb) {
      cb(ApiStatus::INTERNAL_ERROR, {});
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

BootstrapListenStateEngine::BootstrapListenStateEngine() {
  // calls startConnectionStateMachine on manager, waits for
  // connStateMachineConnected
  addInitialState<StateBootstrapListenInitial>(STATE_BOOTSTRAP_LISTEN_INITIAL);
  // calls user supplied callback to return the receiver object, always
  // transitions to next state
  addState<StateBootstrapListenWaitingForConnections>(STATE_BOOTSTRAP_LISTEN_WAITING_FOR_CONNECTIONS);
  // do nothing, wait for a package to be received, accept to be called, or
  // bootstrapListener to be closed
  addState<StateBootstrapListenWaitingForHellos>(STATE_BOOTSTRAP_LISTEN_WAITING_FOR_HELLOS);
  // calls state machine finished on manager, final state
  addState<StateBootstrapListenFinished>(STATE_BOOTSTRAP_LISTEN_FINISHED);
  addFailedState<StateBootstrapListenFailed>(STATE_BOOTSTRAP_LISTEN_FAILED);

  // clang-format off
    declareStateTransition(STATE_BOOTSTRAP_LISTEN_INITIAL,                       EVENT_ALWAYS, STATE_BOOTSTRAP_LISTEN_WAITING_FOR_CONNECTIONS);
    declareStateTransition(STATE_BOOTSTRAP_LISTEN_WAITING_FOR_CONNECTIONS,       EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_BOOTSTRAP_LISTEN_WAITING_FOR_CONNECTIONS);
    declareStateTransition(STATE_BOOTSTRAP_LISTEN_WAITING_FOR_CONNECTIONS,       EVENT_SATISFIED,                    STATE_BOOTSTRAP_LISTEN_WAITING_FOR_HELLOS);
    declareStateTransition(STATE_BOOTSTRAP_LISTEN_WAITING_FOR_HELLOS,            EVENT_RECEIVE_PACKAGE,              STATE_BOOTSTRAP_LISTEN_WAITING_FOR_HELLOS);
    declareStateTransition(STATE_BOOTSTRAP_LISTEN_WAITING_FOR_HELLOS,            EVENT_ACCEPT,                       STATE_BOOTSTRAP_LISTEN_WAITING_FOR_HELLOS);
    declareStateTransition(STATE_BOOTSTRAP_LISTEN_WAITING_FOR_HELLOS,            EVENT_CLOSE,                        STATE_BOOTSTRAP_LISTEN_FINISHED);
  // clang-format on
}

std::string BootstrapListenStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
