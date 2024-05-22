
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

#include "ConduitStateMachine.h"

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

void ConduitContext::updateConduitectStateMachineStart(
    RaceHandle /* contextHandle */, RaceHandle recvHandle,
    const ConnectionID &_recvConnId, RaceHandle sendHandle,
    const ConnectionID &_sendConnId, const ChannelId &_sendChannel,
    const ChannelId &_recvChannel, const std::string &_packageId,
    std::vector<std::vector<uint8_t>> recvMessages, RaceHandle _apiHandle) {
  this->recvConnSMHandle = recvHandle;
  this->recvConnId = _recvConnId;
  this->sendConnSMHandle = sendHandle;
  this->sendConnId = _sendConnId;
  this->apiHandle = _apiHandle;
  this->sendChannel = _sendChannel;
  this->recvChannel = _recvChannel;
  this->packageId = _packageId;

  for (auto &message : recvMessages) {
    this->recvQueue.emplace(std::move(message));
  }
}

void ConduitContext::updateReceiveEncPkg(
    ConnectionID /* connId */, std::shared_ptr<std::vector<uint8_t>> data) {
  this->recvQueue.push(*data);
}

void ConduitContext::updatePackageStatusChanged(RaceHandle pkgHandle,
                                                         PackageStatus status) {
  if (status == PACKAGE_SENT) {
    sentList.push_back(pkgHandle);
  } else {
    failedList.push_back(pkgHandle);
  }
}

void ConduitContext::updateRead(
    RaceHandle /* handle */,
    std::function<void(ApiStatus, std::vector<uint8_t>)> cb) {
      TRACE_METHOD();
  if (this->readCallback) {
    // Call old callback
    helper::logError(logPrefix + "read callback not null for context handle " + std::to_string(handle));
    this->readCallback(ApiStatus::INTERNAL_ERROR, {});
  }
  this->readCallback = cb;
}

void ConduitContext::updateWrite(RaceHandle /* handle */,
                                          std::vector<uint8_t> bytes,
                                          std::function<void(ApiStatus)> cb) {
  this->sendQueue.push_back({cb, std::move(bytes)});
}

void ConduitContext::updateClose(RaceHandle /* handle */,
                                          std::function<void(ApiStatus)> cb) {
  this->closeCallback = cb;
}

//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StateConduitInitial : public ConduitState {
  explicit StateConduitInitial(
      StateType id = STATE_CONNECTION_OBJECT_INITIAL)
      : ConduitState(id, "STATE_CONNECTION_OBJECT_INITIAL") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.sendChannel.empty()) {
      helper::logError(logPrefix + "Invalid sendChannel");
      return EventResult::NOT_SUPPORTED;
    } else if (ctx.recvChannel.empty()) {
      helper::logError(logPrefix + "Invalid recvChannel");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, ctx.apiHandle);
    ctx.manager.registerHandle(ctx, ctx.sendConnSMHandle);
    ctx.manager.registerHandle(ctx, ctx.recvConnSMHandle);
    // ctx.manager.registerId(ctx, ctx.recvConnId);
    ctx.manager.registerPackageId(ctx, ctx.recvConnId, ctx.packageId);
    std::vector<uint8_t> packageIdBytes{ctx.packageId.begin(),
                                        ctx.packageId.end()};
    helper::logDebug(logPrefix + "PackageId: " + json(packageIdBytes).dump());

    ctx.pendingEvents.push(EVENT_ALWAYS);
    return EventResult::SUCCESS;
  }
};

struct StateConduitConnected : public ConduitState {
  explicit StateConduitConnected(
      StateType id = STATE_CONNECTION_OBJECT_CONNECTED)
      : ConduitState(id, "STATE_CONNECTION_OBJECT_CONNECTED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    PluginWrapper &plugin = getPlugin(ctx, ctx.sendChannel);

    if (ctx.readCallback && !ctx.recvQueue.empty()) {
      ctx.readCallback(ApiStatus::OK, ctx.recvQueue.front());
      ctx.readCallback = {};
      ctx.recvQueue.pop();
    } else if(!ctx.recvQueue.empty()) {
      helper::logWarning(logPrefix + "null read callback and non-empty queue!");
    }

    for (auto &[cb, bytes] : ctx.sendQueue) {
      RaceHandle pkgHandle = ctx.manager.getCore().generateHandle();

      std::vector<uint8_t> prefixedBytes;
      prefixedBytes.reserve(bytes.size() + packageIdLen);
      prefixedBytes.insert(prefixedBytes.end(), ctx.packageId.begin(),
                           ctx.packageId.end());
      prefixedBytes.insert(prefixedBytes.end(), bytes.begin(), bytes.end());

      EncPkg pkg(0, 0, prefixedBytes);
      SdkResponse response =
          plugin.sendPackage(pkgHandle, ctx.sendConnId, pkg, 0, 0);

      if (response.status != SdkStatus::SDK_OK) {
        helper::logError(logPrefix + "sendPackage returned " + std::to_string(response.status));
        cb(ApiStatus::INTERNAL_ERROR);
      } else {
        ctx.manager.registerHandle(ctx, pkgHandle);
        ctx.sentQueue[pkgHandle] = std::move(cb);
      }

      cb = {};
    }
    ctx.sendQueue.clear();

    for (auto handle : ctx.sentList) {
      auto it = ctx.sentQueue.find(handle);
      if (it == ctx.sentQueue.end()) {
        continue;
      }

      it->second(ApiStatus::OK);
      ctx.sentQueue.erase(it);
    }
    ctx.sentList.clear();

    for (auto handle : ctx.failedList) {
      auto it = ctx.sentQueue.find(handle);
      if (it == ctx.sentQueue.end()) {
        continue;
      }
      helper::logInfo(logPrefix + "failed list notify");
      it->second(ApiStatus::INTERNAL_ERROR);
      ctx.sentQueue.erase(it);
    }
    ctx.failedList.clear();

    return EventResult::SUCCESS;
  }
};

struct StateConduitFinished : public ConduitState {
  explicit StateConduitFinished(
      StateType id = STATE_CONNECTION_OBJECT_FINISHED)
      : ConduitState(id, "STATE_CONNECTION_OBJECT_FINISHED") {}
  virtual bool finalState() { return true; }
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.readCallback) {
      ctx.readCallback(ApiStatus::CLOSING, {});
      ctx.readCallback = {};
    }

    for (auto &[cb, bytes] : ctx.sendQueue) {
      helper::logWarning(logPrefix + "send queue not empty");
      cb(ApiStatus::INTERNAL_ERROR);
    }
    ctx.sendQueue.clear();

    for (auto &[handle, cb] : ctx.sentQueue) {
      helper::logWarning(logPrefix + "sent queue not empty");
      cb(ApiStatus::INTERNAL_ERROR);
    }
    ctx.sentQueue.clear();

    ctx.manager.stateMachineFinished(ctx);

    ctx.closeCallback(ApiStatus::OK);
    ctx.closeCallback = {};
    return EventResult::SUCCESS;
  }
};

struct StateConduitFailed : public ConduitState {
  explicit StateConduitFailed(
      StateType id = STATE_CONNECTION_OBJECT_FAILED)
      : ConduitState(id, "STATE_CONNECTION_OBJECT_FAILED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.dialCallback) {
      helper::logDebug(logPrefix + "dial callback not null");
      ctx.dialCallback(ApiStatus::INTERNAL_ERROR, {});
      ctx.dialCallback = {};
    }

    for (auto &[cb, bytes] : ctx.sendQueue) {
      helper::logDebug(logPrefix + "send queue not empty");
      cb(ApiStatus::INTERNAL_ERROR);
    }
    ctx.sendQueue.clear();

    for (auto &[handle, cb] : ctx.sentQueue) {
      helper::logDebug(logPrefix + "sent queue not empty");
      cb(ApiStatus::INTERNAL_ERROR);
    }
    ctx.sentQueue.clear();

    if (ctx.readCallback) {
      ctx.readCallback(ApiStatus::INTERNAL_ERROR, {});
      helper::logDebug(logPrefix + "clearing read callback");
      ctx.readCallback = {};
    }

    if (ctx.closeCallback) {
      helper::logDebug(logPrefix + "close callback not null");
      ctx.closeCallback(ApiStatus::INTERNAL_ERROR);
      ctx.closeCallback = {};
    }

    ctx.manager.stateMachineFailed(ctx);
    return EventResult::SUCCESS;
  }
};

//-----------------------------------------------------------------------------------------------
// StateEngine
//-----------------------------------------------------------------------------------------------

ConduitStateEngine::ConduitStateEngine() {
  // calls sendPackage on plugin with any package in queue, re-enters state if
  // read, write, or receiveEncPkg is called, transitions on close call
  addInitialState<StateConduitInitial>(
      STATE_CONNECTION_OBJECT_INITIAL);
  addState<StateConduitConnected>(STATE_CONNECTION_OBJECT_CONNECTED);
  // calls state machine finished on manager, final state
  addState<StateConduitFinished>(STATE_CONNECTION_OBJECT_FINISHED);
  // call state machine failed
  addFailedState<StateConduitFailed>(STATE_CONNECTION_OBJECT_FAILED);

  // clang-format off
    declareStateTransition(STATE_CONNECTION_OBJECT_INITIAL,   EVENT_ALWAYS, STATE_CONNECTION_OBJECT_CONNECTED);
    declareStateTransition(STATE_CONNECTION_OBJECT_CONNECTED, EVENT_CLOSE,  STATE_CONNECTION_OBJECT_FINISHED);

    // The following events just cause the state to loop back to itself. re-entering the state will
    // send queued packages and receive any received packages if there's a receive callback
    declareStateTransition(STATE_CONNECTION_OBJECT_CONNECTED, EVENT_READ,            STATE_CONNECTION_OBJECT_CONNECTED);
    declareStateTransition(STATE_CONNECTION_OBJECT_CONNECTED, EVENT_WRITE,           STATE_CONNECTION_OBJECT_CONNECTED);
    declareStateTransition(STATE_CONNECTION_OBJECT_CONNECTED, EVENT_RECEIVE_PACKAGE, STATE_CONNECTION_OBJECT_CONNECTED);
    declareStateTransition(STATE_CONNECTION_OBJECT_CONNECTED, EVENT_PACKAGE_SENT,    STATE_CONNECTION_OBJECT_CONNECTED);
    declareStateTransition(STATE_CONNECTION_OBJECT_CONNECTED, EVENT_PACKAGE_FAILED,  STATE_CONNECTION_OBJECT_CONNECTED);
  // clang-format on
}

std::string ConduitStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
