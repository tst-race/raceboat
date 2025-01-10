
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

#include "SendReceiveStateMachine.h"

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

void ApiSendReceiveContext::updateSendReceive(
    const SendOptions &_sendOptions, std::vector<uint8_t> &&_data,
    std::function<void(ApiStatus, std::vector<uint8_t>)> _cb) {
  this->opts = _sendOptions;
  this->data = _data;
  this->callback = _cb;
}
void ApiSendReceiveContext::updateConnStateMachineLinkEstablished(
                                                                  RaceHandle contextHandle, LinkID /*linkId*/, std::string linkAddress) {
  if (this->recvConnSMHandle == contextHandle) {
    this->recvLinkAddress = linkAddress;
  }
};
void ApiSendReceiveContext::updateConnStateMachineConnected(
    RaceHandle contextHandle, ConnectionID connId, std::string linkAddress) {
  if (this->recvConnSMHandle == contextHandle) {
    this->recvConnId = connId;
    this->recvLinkAddress = linkAddress;
  } else if (this->sendConnSMHandle == contextHandle) {
    this->sendConnId = connId;
  }
};
void ApiSendReceiveContext::updateReceiveEncPkg(
    ConnectionID /* _connId */, std::shared_ptr<std::vector<uint8_t>> _data) {
  this->receivedMsg = std::move(_data);
};

//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------
struct StateSendReceiveInitial : public SendReceiveState {
  explicit StateSendReceiveInitial(StateType id = STATE_SEND_RECEIVE_INITIAL)
      : SendReceiveState(id, "STATE_SEND_RECEIVE_INITIAL") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ChannelId recvChannelId = ctx.opts.recv_channel;
    std::string recvRole = ctx.opts.recv_role;
    if (recvChannelId.empty()) {
      helper::logError(logPrefix + "Invalid recv channel id passed to recv");
      ctx.callback(ApiStatus::CHANNEL_INVALID, {});
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (recvRole.empty()) {
      helper::logError(logPrefix + "Invalid recv role passed to sendReceive");
      ctx.callback(ApiStatus::INVALID_ARGUMENT, {});
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    } 
    PluginContainer *recvContainer =
        ctx.manager.getCore().getChannel(recvChannelId);
    if (recvContainer == nullptr) {
      helper::logError(logPrefix + "Failed to get channel with id " +
                       recvChannelId);
      ctx.callback(ApiStatus::CHANNEL_INVALID, {});
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    }


    ctx.recvConnSMHandle = ctx.manager.startConnStateMachine(
                                                             ctx.handle, recvChannelId, recvRole, "", true, false);

    if (ctx.recvConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, ctx.recvConnSMHandle);

    return EventResult::SUCCESS;
  }
};

struct StateSendReceiveWaitingForSendConnection : public SendReceiveState {
  explicit StateSendReceiveWaitingForSendConnection(
      StateType id = STATE_SEND_RECEIVE_WAITING_FOR_SEND_CONNECTION)
      : SendReceiveState(id,
                         "STATE_SEND_RECEIVE_WAITING_FOR_SEND_CONNECTION") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    // Send connection exists, progress state
    if (not ctx.sendConnId.empty()) {
      helper::logDebug(logPrefix +
                       "emitting SATISFIED to move to next state");
        ctx.pendingEvents.push(EVENT_SATISFIED);
        return EventResult::SUCCESS;
    }
    helper::logDebug(logPrefix +
                     "recv link established, triggering connecting for send");
    
    ChannelId sendChannelId = ctx.opts.send_channel;
    std::string sendRole = ctx.opts.send_role;
    std::string sendLinkAddress = ctx.opts.send_address;
    if (sendChannelId.empty()) {
      helper::logError(logPrefix +
                       "Invalid send channel id passed to sendReceive");
      ctx.callback(ApiStatus::CHANNEL_INVALID, {});
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (sendRole.empty()) {
      helper::logError(logPrefix + "Invalid send role passed to sendReceive");
      ctx.callback(ApiStatus::INVALID_ARGUMENT, {});
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (sendLinkAddress.empty()) {
      helper::logError(logPrefix +
                       "Invalid send address passed to sendReceive");
      ctx.callback(ApiStatus::INVALID_ARGUMENT, {});
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    }
    PluginContainer *sendContainer =
        ctx.manager.getCore().getChannel(sendChannelId);
    if (sendContainer == nullptr) {
      helper::logError(logPrefix + "Failed to get channel with id " +
                       sendChannelId);
      ctx.callback(ApiStatus::CHANNEL_INVALID, {});
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    }

    ctx.sendConnSMHandle = ctx.manager.startConnStateMachine(
                                                             ctx.handle, sendChannelId, sendRole, sendLinkAddress, false, true);
    if (ctx.sendConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }
    ctx.manager.registerHandle(ctx, ctx.sendConnSMHandle);
    return EventResult::SUCCESS;
  }
};

struct StateSendReceiveSendOpen : public SendReceiveState {
  explicit StateSendReceiveSendOpen(
      StateType id = STATE_SEND_RECEIVE_SEND_OPEN)
      : SendReceiveState(id, "STATE_SEND_RECEIVE_SEND_OPEN") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    PluginWrapper &plugin = getPlugin(ctx, ctx.opts.send_channel);
    RaceHandle pkgHandle = ctx.manager.getCore().generateHandle();

    // TODO: There's better ways to encode than base64 inside json
    std::string dataB64 = base64::encode(std::move(ctx.data));
    nlohmann::json json = {
        {"linkAddress", ctx.recvLinkAddress},
        {"replyChannel", ctx.opts.recv_channel},
        {"message", dataB64},
    };
    std::string message = json.dump();
    std::vector<uint8_t> bytes(message.begin(), message.end());

    EncPkg pkg(0, 0, bytes);
    ctx.data.clear();

    SdkResponse response =
        plugin.sendPackage(pkgHandle, ctx.sendConnId, pkg, 0, 0);

    if (response.status != SdkStatus::SDK_OK) {
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, pkgHandle);

    return EventResult::SUCCESS;
  }
};

struct StateSendReceivePackageSent : public SendReceiveState {
  explicit StateSendReceivePackageSent(
      StateType id = STATE_SEND_RECEIVE_PACKAGE_SENT)
    : SendReceiveState(id, "STATE_SEND_RECEIVE_PACKAGE_SENT") {}
  virtual EventResult enter(Context &context) {
    auto &ctx = getContext(context);
    if (not ctx.recvConnId.empty()) {
      ctx.manager.registerId(ctx, ctx.recvConnId);
    }
    return EventResult::SUCCESS;
  }
};

struct StateSendReceiveFinished : public SendReceiveState {
  explicit StateSendReceiveFinished(StateType id = STATE_SEND_RECEIVE_FINISHED)
      : SendReceiveState(id, "STATE_SEND_RECEIVE_FINISHED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    if (ctx.receivedMsg == nullptr) {
      return EventResult::NOT_SUPPORTED;
    }

    ctx.callback(ApiStatus::OK, *ctx.receivedMsg);
    ctx.callback = {};

    ctx.manager.stateMachineFinished(ctx);
    return EventResult::SUCCESS;
  }
  virtual bool finalState() { return true; }
};

struct StateSendReceiveFailed : public SendReceiveState {
  explicit StateSendReceiveFailed(StateType id = STATE_SEND_RECEIVE_FAILED)
      : SendReceiveState(id, "STATE_SEND_RECEIVE_FAILED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.callback) {
      ctx.callback(ApiStatus::INTERNAL_ERROR, {});
      ctx.callback = {};
    }

    ctx.manager.stateMachineFailed(ctx);
    return EventResult::SUCCESS;
  }
};

//-----------------------------------------------------------------------------------------------
// StateEngine
//-----------------------------------------------------------------------------------------------

SendReceiveStateEngine::SendReceiveStateEngine() {
  // calls startConnectionStateMachine twice, waits for
  // connStateMachineConnected
  addInitialState<StateSendReceiveInitial>(STATE_SEND_RECEIVE_INITIAL);

  // does nothing, waits for a send connStateMachineConnected call
  addState<StateSendReceiveWaitingForSendConnection>(
      STATE_SEND_RECEIVE_WAITING_FOR_SEND_CONNECTION);
  // calls sendPackage on plugin, waits for
  // onPackageStatusChanged(status=PACKAGE_SENT)
  addState<StateSendReceiveSendOpen>(
      STATE_SEND_RECEIVE_SEND_OPEN);
  // does nothing, waits for a receiveEncPkg call
  addState<StateSendReceivePackageSent>(STATE_SEND_RECEIVE_PACKAGE_SENT);
  // calls callback with received message, calls state machine finished on
  // manager, final state
  addState<StateSendReceiveFinished>(STATE_SEND_RECEIVE_FINISHED);
  // call state machine failed, manager will forward event to connection state
  // machine
  addFailedState<StateSendReceiveFailed>(STATE_SEND_RECEIVE_FAILED);

  // clang-format off
  declareStateTransition(STATE_SEND_RECEIVE_INITIAL,                       EVENT_CONN_STATE_MACHINE_LINK_ESTABLISHED, STATE_SEND_RECEIVE_WAITING_FOR_SEND_CONNECTION);
    declareStateTransition(STATE_SEND_RECEIVE_WAITING_FOR_SEND_CONNECTION, EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_SEND_RECEIVE_WAITING_FOR_SEND_CONNECTION);
    declareStateTransition(STATE_SEND_RECEIVE_WAITING_FOR_SEND_CONNECTION, EVENT_SATISFIED, STATE_SEND_RECEIVE_SEND_OPEN);
    declareStateTransition(STATE_SEND_RECEIVE_SEND_OPEN,
                           EVENT_PACKAGE_SENT,
                           STATE_SEND_RECEIVE_PACKAGE_SENT);
    declareStateTransition(STATE_SEND_RECEIVE_PACKAGE_SENT,                  EVENT_CONN_STATE_MACHINE_CONNECTED,              STATE_SEND_RECEIVE_PACKAGE_SENT);
    declareStateTransition(STATE_SEND_RECEIVE_PACKAGE_SENT,                  EVENT_RECEIVE_PACKAGE,              STATE_SEND_RECEIVE_FINISHED);
  // clang-format on
}

std::string SendReceiveStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
