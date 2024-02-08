
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

#include "ReceiveStateMachine.h"

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

void ApiRecvContext::updateGetReceiver(
    const ReceiveOptions &_recvOptions,
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> _cb) {
  this->opts = _recvOptions;
  this->getReceiverCb = _cb;
};
void ApiRecvContext::updateReceive(
    RaceHandle /* _handle */,
    std::function<void(ApiStatus, std::vector<uint8_t>)> _cb) {
  this->receiveCb = _cb;
};
void ApiRecvContext::updateClose(RaceHandle /* handle */,
                                 std::function<void(ApiStatus)> _cb) {
  this->closeCb = _cb;
}

void ApiRecvContext::updateReceiveEncPkg(
    ConnectionID /* _connId */, std::shared_ptr<std::vector<uint8_t>> _data) {
  this->data.push(std::move(_data));
};
void ApiRecvContext::updateConnStateMachineConnected(
    RaceHandle /* _contextHandle */, ConnectionID _connId,
    std::string _linkAddress) {
  this->connId = _connId;
  this->linkAddress = _linkAddress;
};

//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StateRecvInitial : public RecvState {
  explicit StateRecvInitial(StateType id = STATE_RECV_INITIAL)
      : RecvState(id, "STATE_RECV_INITIAL") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ChannelId channelId = ctx.opts.recv_channel;
    std::string role = ctx.opts.recv_role;
    std::string linkAddress = ctx.opts.recv_address;

    if (channelId.empty()) {
      helper::logError(logPrefix + "Invalid channelId passed to getReceiver");
      ctx.getReceiverCb(ApiStatus::CHANNEL_INVALID, "", {});
      ctx.getReceiverCb = {};
      return EventResult::NOT_SUPPORTED;
    } else if (role.empty()) {
      helper::logError(logPrefix + "Invalid role passed to getReceiver");
      ctx.getReceiverCb(ApiStatus::INVALID_ARGUMENT, "", {});
      ctx.getReceiverCb = {};
      return EventResult::NOT_SUPPORTED;
    }

    PluginContainer *container = ctx.manager.getCore().getChannel(channelId);
    if (container == nullptr) {
      helper::logError(logPrefix + "Failed to get channel with id " +
                       channelId);
      ctx.getReceiverCb(ApiStatus::CHANNEL_INVALID, "", {});
      ctx.getReceiverCb = {};
      return EventResult::NOT_SUPPORTED;
    }

    RaceHandle connStateMachineHandle = ctx.manager.startConnStateMachine(
        ctx.handle, channelId, role, linkAddress, false);

    if (connStateMachineHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, connStateMachineHandle);

    return EventResult::SUCCESS;
  }
};

struct StateRecvConnectionOpen : public RecvState {
  explicit StateRecvConnectionOpen(StateType id = STATE_RECV_CONNECTION_OPEN)
      : RecvState(id, "STATE_RECV_CONNECTION_OPEN") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    RaceHandle receiverHandle = ctx.manager.getCore().generateHandle();

    ctx.getReceiverCb(ApiStatus::OK, ctx.linkAddress, receiverHandle);
    ctx.getReceiverCb = {};

    ctx.manager.registerHandle(ctx, receiverHandle);
    ctx.manager.registerId(ctx, ctx.connId);

    ctx.pendingEvents.push(EVENT_ALWAYS);

    return EventResult::SUCCESS;
  }
};

struct StateRecvWaitingForAppAndPlugin : public RecvState {
  explicit StateRecvWaitingForAppAndPlugin(
      StateType id = STATE_RECV_WAITING_FOR_APP_AND_PLUGIN)
      : RecvState(id, "STATE_RECV_WAITING_FOR_APP_AND_PLUGIN") {}
  virtual EventResult enter(Context & /* context */) {
    // nothing to do, waiting for app to call receive()
    return EventResult::SUCCESS;
  }
};

struct StateRecvWaitingForApp : public RecvState {
  explicit StateRecvWaitingForApp(StateType id = STATE_RECV_WAITING_FOR_APP)
      : RecvState(id, "STATE_RECV_WAITING_FOR_APP") {}
  virtual EventResult enter(Context & /* context */) {
    // nothing to do, waiting for app to call receive()
    return EventResult::SUCCESS;
  }
};

struct StateRecvWaitingForPlugin : public RecvState {
  explicit StateRecvWaitingForPlugin(
      StateType id = STATE_RECV_WAITING_FOR_PLUGIN)
      : RecvState(id, "STATE_RECV_WAITING_FOR_PLUGIN") {}
  virtual EventResult enter(Context & /* context */) {
    // nothing to do, waiting for plugin to send us a package
    return EventResult::SUCCESS;
  }
};

struct StateRecvReceived : public RecvState {
  explicit StateRecvReceived(StateType id = STATE_RECV_RECEIVED)
      : RecvState(id, "STATE_RECV_RECEIVED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    auto data = std::move(ctx.data.front());
    ctx.data.pop();

    ctx.receiveCb(ApiStatus::OK, *data);
    ctx.receiveCb = {};

    if (ctx.data.empty()) {
      ctx.pendingEvents.push(EVENT_RECV_NO_PACKAGES_REMAINING);
    } else {
      ctx.pendingEvents.push(EVENT_RECV_PACKAGES_REMAINING);
    }

    return EventResult::SUCCESS;
  }
};

struct StateRecvFinished : public RecvState {
  explicit StateRecvFinished(StateType id = STATE_RECV_FINISHED)
      : RecvState(id, "STATE_RECV_FINISHED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.receiveCb) {
      ctx.receiveCb(ApiStatus::CLOSING, {});
      ctx.receiveCb = {};
    }

    ctx.manager.stateMachineFinished(ctx);

    ctx.closeCb(ApiStatus::OK);
    ctx.closeCb = {};
    return EventResult::SUCCESS;
  }
  virtual bool finalState() { return true; }
};

struct StateRecvFailed : public RecvState {
  explicit StateRecvFailed(StateType id = STATE_RECV_FAILED)
      : RecvState(id, "STATE_RECV_FAILED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.getReceiverCb) {
      ctx.getReceiverCb(ApiStatus::INTERNAL_ERROR, {}, {});
      ctx.getReceiverCb = {};
    }
    if (ctx.receiveCb) {
      ctx.receiveCb(ApiStatus::INTERNAL_ERROR, {});
      ctx.receiveCb = {};
    }
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

RecvStateEngine::RecvStateEngine() {
  // calls startConnectionStateMachine on manager, waits for
  // connStateMachineConnected
  addInitialState<StateRecvInitial>(STATE_RECV_INITIAL);
  // calls user supplied callback to return the receiver object, always
  // transitions to next state
  addState<StateRecvConnectionOpen>(STATE_RECV_CONNECTION_OPEN);
  // waits for a receive() call on the receiver, a recvEncPkg call from plugin,
  // or a close call from the app
  addState<StateRecvWaitingForAppAndPlugin>(
      STATE_RECV_WAITING_FOR_APP_AND_PLUGIN);
  // wait for a receive() call or a close() call from the app
  // If another packages is received here, it's added to a queue, but no
  // transition is made
  addState<StateRecvWaitingForApp>(STATE_RECV_WAITING_FOR_APP);
  // wait for a recvEncPkg() call from the plugin or a close() call from the app
  addState<StateRecvWaitingForPlugin>(STATE_RECV_WAITING_FOR_PLUGIN);
  // call user callback with received package, transitions to
  // WAITING_FOR_APP_AND_PLUGIN or WAITING_FOR_APP based on if there's still
  // packages in the queue
  addState<StateRecvReceived>(STATE_RECV_RECEIVED);
  // calls state machine finished on manager, final state
  addState<StateRecvFinished>(STATE_RECV_FINISHED);
  addFailedState<StateRecvFailed>(STATE_RECV_FAILED);

  // clang-format off
    declareStateTransition(STATE_RECV_INITIAL,                       EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_RECV_CONNECTION_OPEN);
    declareStateTransition(STATE_RECV_CONNECTION_OPEN,               EVENT_ALWAYS,                       STATE_RECV_WAITING_FOR_APP_AND_PLUGIN);
    declareStateTransition(STATE_RECV_WAITING_FOR_APP_AND_PLUGIN,    EVENT_RECEIVE_REQUEST,              STATE_RECV_WAITING_FOR_PLUGIN);
    declareStateTransition(STATE_RECV_WAITING_FOR_APP_AND_PLUGIN,    EVENT_RECEIVE_PACKAGE,              STATE_RECV_WAITING_FOR_APP);
    declareStateTransition(STATE_RECV_WAITING_FOR_APP,               EVENT_RECEIVE_REQUEST,              STATE_RECV_RECEIVED);
    declareStateTransition(STATE_RECV_WAITING_FOR_APP,               EVENT_RECEIVE_PACKAGE,              STATE_RECV_WAITING_FOR_APP);
    declareStateTransition(STATE_RECV_WAITING_FOR_PLUGIN,            EVENT_RECEIVE_PACKAGE,              STATE_RECV_RECEIVED);
    declareStateTransition(STATE_RECV_RECEIVED,                      EVENT_RECV_NO_PACKAGES_REMAINING,   STATE_RECV_WAITING_FOR_APP_AND_PLUGIN);
    declareStateTransition(STATE_RECV_RECEIVED,                      EVENT_RECV_PACKAGES_REMAINING,      STATE_RECV_WAITING_FOR_APP);
    declareStateTransition(STATE_RECV_WAITING_FOR_APP_AND_PLUGIN,    EVENT_CLOSE,                        STATE_RECV_FINISHED);
    declareStateTransition(STATE_RECV_WAITING_FOR_PLUGIN,            EVENT_CLOSE,                        STATE_RECV_FINISHED);
    declareStateTransition(STATE_RECV_WAITING_FOR_APP,               EVENT_CLOSE,                        STATE_RECV_FINISHED);
  // clang-format on
}

std::string RecvStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
