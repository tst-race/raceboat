
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

#include "ResumeStateMachine.h"

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

void ApiResumeContext::updateResume(const ResumeOptions &resumeOptions,
                                    std::function<void(ApiStatus, RaceHandle, ConduitProperties)> cb) {
  this->opts = resumeOptions;
  this->resumeCallback = cb;
}

void ApiResumeContext::updateConnStateMachineConnected(RaceHandle contextHandle,
                                                     ConnectionID connId,
                                                     std::string linkAddress) {
  if (this->recvConnSMHandle == contextHandle) {
    this->recvConnId = connId;
    this->recvLinkAddress = linkAddress;
  } else if (this->sendConnSMHandle == contextHandle) {
    this->sendConnId = connId;
  }
}
//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StateResumeInitial : public ResumeState {
  explicit StateResumeInitial(StateType id = STATE_RESUME_INITIAL)
      : ResumeState(id, "StateResumeInitial") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ChannelId sendChannelId = ctx.opts.send_channel;
    ChannelId recvChannelId = ctx.opts.recv_channel;
    std::string sendRole = ctx.opts.send_role;
    std::string recvRole = ctx.opts.recv_role;
    std::string sendLinkAddress = ctx.opts.send_address;
    std::string recvLinkAddress = ctx.opts.recv_address;
    std::string packageId = ctx.opts.package_id;
    if (sendChannelId.empty()) {
      helper::logError(logPrefix +
                       "Invalid send channel id passed to resume");
      ctx.resumeCallback(ApiStatus::CHANNEL_INVALID, {}, {});
      ctx.resumeCallback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (recvChannelId.empty()) {
      helper::logError(logPrefix + "Invalid recv channel id passed to resume");
      ctx.resumeCallback(ApiStatus::CHANNEL_INVALID, {}, {});
      ctx.resumeCallback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (sendRole.empty()) {
      helper::logError(logPrefix + "Invalid send role passed to resume");
      ctx.resumeCallback(ApiStatus::INVALID_ARGUMENT, {}, {});
      ctx.resumeCallback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (recvRole.empty()) {
      helper::logError(logPrefix + "Invalid recv role passed to resume");
      ctx.resumeCallback(ApiStatus::INVALID_ARGUMENT, {}, {});
      ctx.resumeCallback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (sendLinkAddress.empty()) {
      helper::logError(logPrefix +
                       "Invalid send address passed to resume");
      ctx.resumeCallback(ApiStatus::INVALID_ARGUMENT, {}, {});
      ctx.resumeCallback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (recvLinkAddress.empty()) {
      helper::logError(logPrefix +
                       "Invalid recv address passed to resume");
      ctx.resumeCallback(ApiStatus::INVALID_ARGUMENT, {}, {});
      ctx.resumeCallback = {};
      return EventResult::NOT_SUPPORTED;
    } else if (packageId.empty()) {
      helper::logError(logPrefix +
                       "Invalid packageID passed to resume");
      ctx.resumeCallback(ApiStatus::INVALID_ARGUMENT, {}, {});
      ctx.resumeCallback = {};
      return EventResult::NOT_SUPPORTED;
    }


    PluginContainer *sendContainer =
        ctx.manager.getCore().getChannel(sendChannelId);
    if (sendContainer == nullptr) {
      helper::logError(logPrefix + "Failed to get channel with id " +
                       sendChannelId);
      ctx.resumeCallback(ApiStatus::CHANNEL_INVALID, {}, {});
      ctx.resumeCallback = {};
      return EventResult::NOT_SUPPORTED;
    }

    PluginContainer *recvContainer =
        ctx.manager.getCore().getChannel(recvChannelId);
    if (recvContainer == nullptr) {
      helper::logError(logPrefix + "Failed to get channel with id " +
                       recvChannelId);
      ctx.resumeCallback(ApiStatus::CHANNEL_INVALID, {}, {});
      ctx.resumeCallback = {};
      return EventResult::NOT_SUPPORTED;
    }


    helper::logDebug(logPrefix + "Input PackageID: " + ctx.opts.package_id);
    std::string decoded_package_id = ctx.opts.package_id;
    try {
      std::vector<uint8_t> byte_vector = base64::decode(ctx.opts.package_id);
      decoded_package_id = std::string(byte_vector.begin(), byte_vector.end());
    } catch (const std::invalid_argument exception) {
      helper::logInfo(logPrefix + " could not decode resume package_id argument from base64, assuming raw value is correct");
    }
    std::string packageIdStr =
      json(std::vector<uint8_t>(decoded_package_id.begin(), decoded_package_id.end())).dump();
    // std::vector<int> int_vector{decoded_package_id.begin(), decoded_package_id.end()};
    helper::logDebug(logPrefix + "Setting PackageId To: " + packageIdStr);

    ctx.packageId = decoded_package_id;

    ctx.sendConnSMHandle = ctx.manager.startConnStateMachine(
                                                             ctx.handle, sendChannelId, sendRole, sendLinkAddress, false, true);

    if (ctx.sendConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.recvConnSMHandle = ctx.manager.startConnStateMachine(
                                                             ctx.handle, recvChannelId, recvRole, recvLinkAddress, true, false);

    if (ctx.recvConnSMHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix + " starting connection state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, ctx.sendConnSMHandle);
    ctx.manager.registerHandle(ctx, ctx.recvConnSMHandle);

    return EventResult::SUCCESS;
  }
};

struct StateResumeWaitingForSecondConnection : public ResumeState {
  explicit StateResumeWaitingForSecondConnection(
      StateType id = STATE_RESUME_WAITING_FOR_SECOND_CONNECTION)
      : ResumeState(id, "StateResumeWaitingForSecondConnection") {}
};

struct StateResumeFinished : public ResumeState {
  explicit StateResumeFinished(StateType id = STATE_RESUME_FINISHED)
      : ResumeState(id, "StateResumeFinished") {}
  virtual bool finalState() { return true; }
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();

    auto &ctx = getContext(context);
    RaceHandle connObjectApiHandle = ctx.manager.getCore().generateHandle();

    RaceHandle connObjectHandle = ctx.manager.startConduitectStateMachine(
        ctx.handle, ctx.recvConnSMHandle, ctx.recvConnId, ctx.sendConnSMHandle,
        ctx.sendConnId, ctx.opts.send_channel, ctx.opts.recv_channel,
        ctx.packageId, {}, connObjectApiHandle);
    if (connObjectHandle == NULL_RACE_HANDLE) {
      helper::logError(logPrefix +
                       " starting connection object state machine failed");
      return EventResult::NOT_SUPPORTED;
    }

    ctx.resumeCallback(ApiStatus::OK, connObjectApiHandle, {});
    ctx.resumeCallback = {};

    ctx.manager.stateMachineFinished(ctx);
    return EventResult::SUCCESS;
  }
};

struct StateResumeFailed : public ResumeState {
  explicit StateResumeFailed(StateType id = STATE_RESUME_FAILED)
      : ResumeState(id, "StateResumeFailed") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.resumeCallback) {
      ctx.resumeCallback(ApiStatus::INTERNAL_ERROR, {}, {});
      ctx.resumeCallback = {};
    }

    ctx.manager.stateMachineFailed(ctx);
    return EventResult::SUCCESS;
  }
};

//-----------------------------------------------------------------------------------------------
// StateEngine
//-----------------------------------------------------------------------------------------------

ResumeStateEngine::ResumeStateEngine() {
  // calls startConnectionStateMachine twice, waits for
  // connStateMachineConnected
  addInitialState<StateResumeInitial>(STATE_RESUME_INITIAL);
  // does nothing, waits for a second connStateMachineConnected call
  addState<StateResumeWaitingForSecondConnection>(
      STATE_RESUME_WAITING_FOR_SECOND_CONNECTION);
  // creates connection object, calls resume callback, calls state machine
  // finished on manager, final state
  addState<StateResumeFinished>(STATE_RESUME_FINISHED);
  // call state machine failed, manager will forward event to connection state
  // machine
  addFailedState<StateResumeFailed>(STATE_RESUME_FAILED);

  // clang-format off
    declareStateTransition(STATE_RESUME_INITIAL,                       EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_RESUME_WAITING_FOR_SECOND_CONNECTION);
    declareStateTransition(STATE_RESUME_WAITING_FOR_SECOND_CONNECTION, EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_RESUME_FINISHED);
  // clang-format on
}

std::string ResumeStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
