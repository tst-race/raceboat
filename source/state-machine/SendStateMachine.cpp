
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

#include "SendStateMachine.h"

#include "../../include/race/Race.h"
#include "Core.h"
#include "Events.h"
#include "PluginContainer.h"
#include "PluginWrapper.h"
#include "States.h"
#include "api-managers/ApiManager.h"
#include "helper.h"

namespace Raceboat {

//-----------------------------------------------------------------------------------------------
// Context
//-----------------------------------------------------------------------------------------------

void ApiSendContext::updateSend(const SendOptions &_sendOptions,
                                std::vector<uint8_t> &&_data,
                                std::function<void(ApiStatus)> _cb) {
  this->opts = _sendOptions;
  this->data = _data;
  this->callback = _cb;
}
void ApiSendContext::updateConnStateMachineConnected(
    RaceHandle /* _contextHandle */, ConnectionID _connId,
    std::string /* _linkAddress */) {
  this->connId = _connId;
};

//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------
struct StateSendInitial : public SendState {
  explicit StateSendInitial(StateType id = STATE_SEND_INITIAL)
      : SendState(id, "STATE_SEND_INITIAL") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ChannelId channelId = ctx.opts.send_channel;
    std::string role = ctx.opts.send_role;
    std::string linkAddress = ctx.opts.send_address;
    if (ctx.data.empty()) {
      helper::logError(logPrefix + "empty data passed to send");
      ctx.callback(ApiStatus::INVALID_ARGUMENT);
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (channelId.empty()) {
      helper::logError(logPrefix + "Invalid channelId passed to send");
      ctx.callback(ApiStatus::CHANNEL_INVALID);
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (role.empty()) {
      helper::logError(logPrefix + "Invalid role passed to send");
      ctx.callback(ApiStatus::INVALID_ARGUMENT);
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (linkAddress.empty()) {
      helper::logError(logPrefix + "Invalid send address passed to send");
      ctx.callback(ApiStatus::INVALID_ARGUMENT);
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    }

    PluginContainer *container = ctx.manager.getCore().getChannel(channelId);
    if (container == nullptr) {
      helper::logError(logPrefix + "Failed to get channel with id " +
                       channelId);
      ctx.callback(ApiStatus::CHANNEL_INVALID);
      ctx.callback = {};
      return EventResult::NOT_SUPPORTED;
    }

    RaceHandle connStateMachineHandle = ctx.manager.startConnStateMachine(
        ctx.handle, channelId, role, linkAddress, true);

    if (connStateMachineHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, connStateMachineHandle);

    return EventResult::SUCCESS;
  }
};

struct StateSendConnectionOpen : public SendState {
  explicit StateSendConnectionOpen(StateType id = STATE_SEND_CONNECTION_OPEN)
      : SendState(id, "STATE_SEND_CONNECTION_OPEN") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    PluginWrapper &plugin = getPlugin(ctx, ctx.opts.send_channel);
    RaceHandle pkgHandle = ctx.manager.getCore().generateHandle();

    EncPkg pkg(0, 0, std::move(ctx.data));
    ctx.data.clear();

    SdkResponse response = plugin.sendPackage(pkgHandle, ctx.connId, pkg, 0, 0);

    if (response.status != SdkStatus::SDK_OK) {
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, pkgHandle);

    return EventResult::SUCCESS;
  }
};

struct StateSendFinished : public SendState {
  explicit StateSendFinished(StateType id = STATE_SEND_FINISHED)
      : SendState(id, "STATE_SEND_FINISHED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ctx.callback(ApiStatus::OK);
    ctx.callback = {};

    ctx.manager.stateMachineFinished(ctx);
    return EventResult::SUCCESS;
  }
  virtual bool finalState() { return true; }
};

struct StateSendFailed : public SendState {
  explicit StateSendFailed(StateType id = STATE_SEND_FAILED)
      : SendState(id, "STATE_SEND_FAILED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.callback) {
      ctx.callback(ApiStatus::INTERNAL_ERROR);
      ctx.callback = {};
    }

    ctx.manager.stateMachineFailed(ctx);
    return EventResult::SUCCESS;
  }
};

//-----------------------------------------------------------------------------------------------
// StateEngine
//-----------------------------------------------------------------------------------------------

SendStateEngine::SendStateEngine() {
  // calls startConnectionStateMachine on manager, waits for
  // connStateMachineConnected
  addInitialState<StateSendInitial>(STATE_SEND_INITIAL);
  // calls sendPackage on plugin, waits for
  // onPackageStatusChanged(status=PACKAGE_SENT)
  addState<StateSendConnectionOpen>(STATE_SEND_CONNECTION_OPEN);
  // calls state machine finished on manager, final state
  addState<StateSendFinished>(STATE_SEND_FINISHED);
  // call state machine failed, manager will forward event to connection state
  // machine
  addFailedState<StateSendFailed>(STATE_SEND_FAILED);

  // clang-format off
    declareStateTransition(STATE_SEND_INITIAL,         EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_SEND_CONNECTION_OPEN);
    declareStateTransition(STATE_SEND_CONNECTION_OPEN, EVENT_PACKAGE_SENT,                 STATE_SEND_FINISHED);
  // clang-format on
}

std::string SendStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
