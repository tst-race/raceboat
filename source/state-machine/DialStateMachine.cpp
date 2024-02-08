
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

#include "DialStateMachine.h"

#include "Core.h"
#include "Events.h"
#include "PluginWrapper.h"
#include "States.h"
#include "base64.h"
#include "race/common/EncPkg.h"

namespace RaceLib {

//-----------------------------------------------------------------------------------------------
// Context
//-----------------------------------------------------------------------------------------------

void ApiDialContext::updateDial(const SendOptions &sendOptions, std::vector<uint8_t> &&_data,
                                std::function<void(ApiStatus, RaceHandle)> cb) {
    this->opts = sendOptions;
    this->data = _data;
    this->dialCallback = cb;
}

void ApiDialContext::updateConnStateMachineConnected(RaceHandle contextHandle, ConnectionID connId,
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

struct StateDialInitial : public DialState {
    explicit StateDialInitial(StateType id = STATE_DIAL_INITIAL) :
        DialState(id, "StateDialInitial") {}
    virtual EventResult enter(Context &context) {
        TRACE_METHOD();
        auto &ctx = getContext(context);

        ChannelId sendChannelId = ctx.opts.send_channel;
        ChannelId recvChannelId = ctx.opts.recv_channel;
        std::string sendRole = ctx.opts.send_role;
        std::string recvRole = ctx.opts.recv_role;
        std::string sendLinkAddress = ctx.opts.send_address;
        if (sendChannelId.empty()) {
            helper::logError(logPrefix + "Invalid send channel id passed to sendReceive");
            ctx.dialCallback(ApiStatus::CHANNEL_INVALID, {});
            ctx.dialCallback = {};
            return EventResult::NOT_SUPPORTED;
        } else if (recvChannelId.empty()) {
            helper::logError(logPrefix + "Invalid recv channel id passed to recv");
            ctx.dialCallback(ApiStatus::CHANNEL_INVALID, {});
            ctx.dialCallback = {};
            return EventResult::NOT_SUPPORTED;
        } else if (sendRole.empty()) {
            helper::logError(logPrefix + "Invalid send role passed to sendReceive");
            ctx.dialCallback(ApiStatus::INVALID_ARGUMENT, {});
            ctx.dialCallback = {};
            return EventResult::NOT_SUPPORTED;
        } else if (recvRole.empty()) {
            helper::logError(logPrefix + "Invalid recv role passed to sendReceive");
            ctx.dialCallback(ApiStatus::INVALID_ARGUMENT, {});
            ctx.dialCallback = {};
            return EventResult::NOT_SUPPORTED;
        } else if (sendLinkAddress.empty()) {
            helper::logError(logPrefix + "Invalid send address passed to sendReceive");
            ctx.dialCallback(ApiStatus::INVALID_ARGUMENT, {});
            ctx.dialCallback = {};
            return EventResult::NOT_SUPPORTED;
        }

        PluginContainer *sendContainer = ctx.manager.getCore().getChannel(sendChannelId);
        if (sendContainer == nullptr) {
            helper::logError(logPrefix + "Failed to get channel with id " + sendChannelId);
            ctx.dialCallback(ApiStatus::CHANNEL_INVALID, {});
            ctx.dialCallback = {};
            return EventResult::NOT_SUPPORTED;
        }

        PluginContainer *recvContainer = ctx.manager.getCore().getChannel(recvChannelId);
        if (recvContainer == nullptr) {
            helper::logError(logPrefix + "Failed to get channel with id " + recvChannelId);
            ctx.dialCallback(ApiStatus::CHANNEL_INVALID, {});
            ctx.dialCallback = {};
            return EventResult::NOT_SUPPORTED;
        }

        std::vector<uint8_t> packageIdBytes = ctx.manager.getCore().getEntropy(packageIdLen);
        ctx.packageId = std::string(packageIdBytes.begin(), packageIdBytes.end());

        ctx.sendConnSMHandle = ctx.manager.startConnStateMachine(ctx.handle, sendChannelId,
                                                                 sendRole, sendLinkAddress, true);

        if (ctx.sendConnSMHandle == NULL_RACE_HANDLE) {
            helper::logError(logPrefix + " starting connection state machine failed");
            return EventResult::NOT_SUPPORTED;
        }

        ctx.recvConnSMHandle =
            ctx.manager.startConnStateMachine(ctx.handle, recvChannelId, recvRole, "", false);

        if (ctx.recvConnSMHandle == NULL_RACE_HANDLE) {
            helper::logError(logPrefix + " starting connection state machine failed");
            return EventResult::NOT_SUPPORTED;
        }

        ctx.manager.registerHandle(ctx, ctx.sendConnSMHandle);
        ctx.manager.registerHandle(ctx, ctx.recvConnSMHandle);

        return EventResult::SUCCESS;
    }
};

struct StateDialWaitingForSecondConnection : public DialState {
    explicit StateDialWaitingForSecondConnection(
        StateType id = STATE_DIAL_WAITING_FOR_SECOND_CONNECTION) :
        DialState(id, "StateDialWaitingForSecondConnection") {}
};

struct StateDialConnectionsOpen : public DialState {
    explicit StateDialConnectionsOpen(StateType id = STATE_DIAL_CONNECTIONS_OPEN) :
        DialState(id, "StateDialConnectionsOpen") {}
    virtual EventResult enter(Context &context) {
        TRACE_METHOD();
        auto &ctx = getContext(context);
        PluginWrapper &plugin = getPlugin(ctx, ctx.opts.send_channel);

        RaceHandle connectionHandle = ctx.manager.getCore().generateHandle();

        ctx.manager.registerHandle(ctx, connectionHandle);
        // ctx.manager.registerId(ctx, ctx.recvConnId);

        // TODO: There's better ways to encode than base64 inside json
        nlohmann::json json = {
            {"linkAddress", ctx.recvLinkAddress},
            {"replyChannel", ctx.opts.recv_channel},
            {"packageId",
             base64::encode(std::vector<uint8_t>(ctx.packageId.begin(), ctx.packageId.end()))},
        };

        std::string dataB64 = base64::encode(std::move(ctx.data));
        json["message"] = dataB64;
        ctx.data.clear();

        std::string message = std::string(packageIdLen, '\0') + json.dump();
        std::vector<uint8_t> bytes(message.begin(), message.end());

        EncPkg pkg(0, 0, bytes);
        RaceHandle pkgHandle = ctx.manager.getCore().generateHandle();
        SdkResponse response = plugin.sendPackage(pkgHandle, ctx.sendConnId, pkg, 0, 0);
        ctx.manager.registerHandle(ctx, pkgHandle);

        if (response.status != SdkStatus::SDK_OK) {
            return EventResult::NOT_SUPPORTED;
        }

        return EventResult::SUCCESS;
    }
};

struct StateDialFinished : public DialState {
    explicit StateDialFinished(StateType id = STATE_DIAL_FINISHED) :
        DialState(id, "StateDialFinished") {}
    virtual bool finalState() {
        return true;
    }
    virtual EventResult enter(Context &context) {
        TRACE_METHOD();
        auto &ctx = getContext(context);
        RaceHandle connObjectApiHandle = ctx.manager.getCore().generateHandle();

        RaceHandle connObjectHandle = ctx.manager.startConnObjectStateMachine(
            ctx.handle, ctx.recvConnSMHandle, ctx.recvConnId, ctx.sendConnSMHandle, ctx.sendConnId,
            ctx.opts.send_channel, ctx.opts.recv_channel, ctx.packageId, {}, connObjectApiHandle);
        if (connObjectHandle == NULL_RACE_HANDLE) {
            helper::logError(logPrefix + " starting connection object state machine failed");
            return EventResult::NOT_SUPPORTED;
        }

        ctx.dialCallback(ApiStatus::OK, connObjectApiHandle);
        ctx.dialCallback = {};

        ctx.manager.stateMachineFinished(ctx);
        return EventResult::SUCCESS;
    }
};

struct StateDialFailed : public DialState {
    explicit StateDialFailed(StateType id = STATE_DIAL_FAILED) : DialState(id, "StateDialFailed") {}
    virtual EventResult enter(Context &context) {
        TRACE_METHOD();
        auto &ctx = getContext(context);

        if (ctx.dialCallback) {
            ctx.dialCallback(ApiStatus::INTERNAL_ERROR, {});
            ctx.dialCallback = {};
        }

        ctx.manager.stateMachineFailed(ctx);
        return EventResult::SUCCESS;
    }
};

//-----------------------------------------------------------------------------------------------
// StateEngine
//-----------------------------------------------------------------------------------------------

DialStateEngine::DialStateEngine() {
    // calls startConnectionStateMachine twice, waits for connStateMachineConnected
    addInitialState<StateDialInitial>(STATE_DIAL_INITIAL);
    // does nothing, waits for a second connStateMachineConnected call
    addState<StateDialWaitingForSecondConnection>(STATE_DIAL_WAITING_FOR_SECOND_CONNECTION);
    // sends initial package with recvLinkAddress and message if any, waits for package sent
    addState<StateDialConnectionsOpen>(STATE_DIAL_CONNECTIONS_OPEN);
    // creates connection object, calls dial callback, calls state machine finished on manager,
    // final state
    addState<StateDialFinished>(STATE_DIAL_FINISHED);
    // call state machine failed, manager will forward event to connection state machine
    addFailedState<StateDialFailed>(STATE_DIAL_FAILED);

    // clang-format off
    declareStateTransition(STATE_DIAL_INITIAL,                       EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_DIAL_WAITING_FOR_SECOND_CONNECTION);
    declareStateTransition(STATE_DIAL_WAITING_FOR_SECOND_CONNECTION, EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_DIAL_CONNECTIONS_OPEN);
    declareStateTransition(STATE_DIAL_CONNECTIONS_OPEN,              EVENT_PACKAGE_SENT,                 STATE_DIAL_FINISHED);
    // clang-format on
}

std::string DialStateEngine::eventToString(EventType event) {
    return RaceLib::eventToString(event);
}

}  // namespace RaceLib