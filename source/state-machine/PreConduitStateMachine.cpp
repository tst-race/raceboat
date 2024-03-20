
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

#include "PreConduitStateMachine.h"

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

void PreConduitContext::updatePreConduitStateMachineStart(
    RaceHandle contextHandle, RaceHandle recvHandle,
    const ConnectionID &_recvConnId, const ChannelId &_recvChannel,
    const ChannelId &_sendChannel, const std::string &_sendRole,
    const std::string &_sendLinkAddress, const std::string &_packageId,
    std::vector<std::vector<uint8_t>> recvMessages) {
  this->parentHandle = contextHandle;
  this->recvConnSMHandle = recvHandle;
  this->recvConnId = _recvConnId;
  this->sendChannel = _sendChannel;
  this->sendRole = _sendRole;               // TODO:
  this->sendLinkAddress = _sendLinkAddress; // TODO:
  this->recvChannel = _recvChannel;
  this->packageId = _packageId;
  this->recvQueue = recvMessages;
}

void PreConduitContext::updateReceiveEncPkg(
    ConnectionID /* connId */, std::shared_ptr<std::vector<uint8_t>> data) {
  this->recvQueue.push_back(*data);
}

void PreConduitContext::updateConnStateMachineConnected(
    RaceHandle /* contextHandle */, ConnectionID connId,
    std::string /* linkAddress */) {
  this->sendConnId = connId;
}

void PreConduitContext::updateListenAccept(
    std::function<void(ApiStatus, RaceHandle)> cb) {
  this->acceptCb = cb;
}

//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StatePreConduitInitial : public PreConduitState {
  explicit StatePreConduitInitial(StateType id = STATE_PRE_CONN_OBJ_INITIAL)
      : PreConduitState(id, "STATE_PRE_CONN_OBJ_INITIAL") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ctx.manager.registerPackageId(ctx, ctx.recvConnId, ctx.packageId);
    ctx.manager.registerHandle(ctx, ctx.parentHandle);

    return EventResult::SUCCESS;
  }
};

struct StatePreConduitAccepted : public PreConduitState {
  explicit StatePreConduitAccepted(StateType id = STATE_PRE_CONN_OBJ_ACCEPTED)
      : PreConduitState(id, "STATE_PRE_CONN_OBJ_ACCEPTED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ctx.sendConnSMHandle = ctx.manager.startConnStateMachine(
                                                             ctx.handle, ctx.sendChannel, ctx.sendRole, ctx.sendLinkAddress, false, true);

    if (ctx.sendConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, ctx.sendConnSMHandle);
    ctx.pendingEvents.push(EVENT_ALWAYS);

    return EventResult::SUCCESS;
  }
};

struct StatePreConduitOpening : public PreConduitState {
  explicit StatePreConduitOpening(StateType id = STATE_PRE_CONN_OBJ_OPENING)
      : PreConduitState(id, "STATE_PRE_CONN_OBJ_OPENING") {}
  virtual EventResult enter(Context & /* context */) {
    TRACE_METHOD();
    return EventResult::SUCCESS;
  }
};

struct StatePreConduitOpen : public PreConduitState {
  explicit StatePreConduitOpen(StateType id = STATE_PRE_CONN_OBJ_FINISHED)
      : PreConduitState(id, "STATE_PRE_CONN_OBJ_FINISHED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    RaceHandle connObjectApiHandle = ctx.manager.getCore().generateHandle();
    RaceHandle connObjectHandle = ctx.manager.startConduitectStateMachine(
        ctx.handle, ctx.recvConnSMHandle, ctx.recvConnId, ctx.sendConnSMHandle,
        ctx.sendConnId, ctx.sendChannel, ctx.recvChannel, ctx.packageId,
        {std::move(ctx.recvQueue)}, connObjectApiHandle);
    ctx.recvQueue.clear();
    if (connObjectHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix +
                       " starting connection object state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.unregisterHandle(ctx, ctx.sendConnSMHandle);
    bool success = ctx.manager.detachConnSM(ctx.handle, ctx.sendConnSMHandle);
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

struct StatePreConduitFailed : public PreConduitState {
  explicit StatePreConduitFailed(StateType id = STATE_PRE_CONN_OBJ_FAILED)
      : PreConduitState(id, "STATE_PRE_CONN_OBJ_FAILED") {}
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

PreConduitStateEngine::PreConduitStateEngine() {
  addInitialState<StatePreConduitInitial>(STATE_PRE_CONN_OBJ_INITIAL);
  addState<StatePreConduitAccepted>(STATE_PRE_CONN_OBJ_ACCEPTED);
  addState<StatePreConduitOpening>(STATE_PRE_CONN_OBJ_OPENING);
  addState<StatePreConduitOpen>(STATE_PRE_CONN_OBJ_FINISHED);
  addFailedState<StatePreConduitFailed>(STATE_PRE_CONN_OBJ_FAILED);

  // clang-format off
    // initial -> opening -> open
    declareStateTransition(STATE_PRE_CONN_OBJ_INITIAL,   EVENT_RECEIVE_PACKAGE,              STATE_PRE_CONN_OBJ_INITIAL);
    declareStateTransition(STATE_PRE_CONN_OBJ_INITIAL,   EVENT_LISTEN_ACCEPTED,              STATE_PRE_CONN_OBJ_ACCEPTED);
    declareStateTransition(STATE_PRE_CONN_OBJ_ACCEPTED,  EVENT_ALWAYS,                       STATE_PRE_CONN_OBJ_OPENING);
    declareStateTransition(STATE_PRE_CONN_OBJ_OPENING,   EVENT_RECEIVE_PACKAGE,              STATE_PRE_CONN_OBJ_OPENING);
    declareStateTransition(STATE_PRE_CONN_OBJ_OPENING,   EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_PRE_CONN_OBJ_FINISHED);
  // clang-format on
}

std::string PreConduitStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
