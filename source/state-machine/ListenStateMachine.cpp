
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

#include "ListenStateMachine.h"

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

void ApiListenContext::updateListen(const ReceiveOptions &_recvOptions,
                                    std::function<void(ApiStatus, LinkAddress, RaceHandle)> _cb) {
    this->opts = _recvOptions;
    this->listenCb = _cb;
};
void ApiListenContext::updateAccept(RaceHandle /* _handle */,
                                    std::function<void(ApiStatus, RaceHandle)> _cb) {
    this->acceptCb.push_back(_cb);
};
void ApiListenContext::updateClose(RaceHandle /* handle */, std::function<void(ApiStatus)> _cb) {
    this->closeCb = _cb;
}

void ApiListenContext::updateReceiveEncPkg(ConnectionID /* _connId */,
                                           std::shared_ptr<std::vector<uint8_t>> _data) {
    this->data.push(std::move(_data));
};
void ApiListenContext::updateConnStateMachineConnected(RaceHandle /* contextHandle */,
                                                       ConnectionID connId,
                                                       std::string linkAddress) {
    this->recvConnId = connId;
    this->recvLinkAddress = linkAddress;
};

//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StateListenInitial : public ListenState {
    explicit StateListenInitial(StateType id = STATE_LISTEN_INITIAL) :
        ListenState(id, "STATE_LISTEN_INITIAL") {}
    virtual EventResult enter(Context &context) {
        TRACE_METHOD();
        auto &ctx = getContext(context);

        ChannelId channelId = ctx.opts.recv_channel;
        std::string role = ctx.opts.recv_role;
        std::string linkAddress = ctx.opts.recv_address;

        if (channelId.empty()) {
            helper::logError(logPrefix + "Invalid receive channel id passed to getReceiver");
            ctx.listenCb(ApiStatus::CHANNEL_INVALID, "", {});
            ctx.listenCb = {};
            return EventResult::NOT_SUPPORTED;
        } else if (role.empty()) {
            helper::logError(logPrefix + "Invalid receive role passed to getReceiver");
            ctx.listenCb(ApiStatus::INVALID_ARGUMENT, "", {});
            ctx.listenCb = {};
            return EventResult::NOT_SUPPORTED;
        } else if (ctx.opts.send_channel.empty()) {
            helper::logError(logPrefix + "Invalid send channel id passed to getReceiver");
            ctx.listenCb(ApiStatus::INVALID_ARGUMENT, "", {});
            ctx.listenCb = {};
            return EventResult::NOT_SUPPORTED;
        } else if (ctx.opts.send_role.empty()) {
            helper::logError(logPrefix + "Invalid send role passed to getReceiver");
            ctx.listenCb(ApiStatus::INVALID_ARGUMENT, "", {});
            ctx.listenCb = {};
            return EventResult::NOT_SUPPORTED;
        }

        PluginContainer *container = ctx.manager.getCore().getChannel(channelId);
        if (container == nullptr) {
            helper::logError(logPrefix + "Failed to get channel with id " + channelId);
            ctx.listenCb(ApiStatus::CHANNEL_INVALID, "", {});
            ctx.listenCb = {};
            return EventResult::NOT_SUPPORTED;
        }

        ctx.recvConnSMHandle =
            ctx.manager.startConnStateMachine(ctx.handle, channelId, role, linkAddress, false);

        if (ctx.recvConnSMHandle == NULL_RACE_HANDLE) {
            helper::logError(logPrefix + " starting connection state machine failed");
            return EventResult::NOT_SUPPORTED;
        }

        ctx.manager.registerHandle(ctx, ctx.recvConnSMHandle);

        return EventResult::SUCCESS;
    }
};

struct StateListenConnectionOpen : public ListenState {
    explicit StateListenConnectionOpen(StateType id = STATE_LISTEN_CONNECTION_OPEN) :
        ListenState(id, "STATE_LISTEN_CONNECTION_OPEN") {}
    virtual EventResult enter(Context &context) {
        TRACE_METHOD();
        auto &ctx = getContext(context);
        RaceHandle receiverHandle = ctx.manager.getCore().generateHandle();

        ctx.listenCb(ApiStatus::OK, ctx.recvLinkAddress, receiverHandle);
        ctx.listenCb = {};

        ctx.manager.registerHandle(ctx, receiverHandle);

        std::string packageId(packageIdLen, '\0');
        ctx.manager.registerPackageId(ctx, ctx.recvConnId, packageId);

        ctx.pendingEvents.push(EVENT_ALWAYS);

        return EventResult::SUCCESS;
    }
};

struct StateListenWaiting : public ListenState {
    explicit StateListenWaiting(StateType id = STATE_LISTEN_WAITING) :
        ListenState(id, "STATE_LISTEN_WAITING") {}
    virtual EventResult enter(Context &context) {
        TRACE_METHOD();
        auto &ctx = getContext(context);

        while (!ctx.data.empty()) {
            auto data = std::move(ctx.data.front());
            ctx.data.pop();

            try {
                std::string str{data->begin(), data->end()};
                nlohmann::json json = nlohmann::json::parse(str);
                LinkAddress linkAddress = json.at("linkAddress");
                std::string replyChannel = json.at("replyChannel");
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

                if (replyChannel != ctx.opts.send_channel) {
                    helper::logError(logPrefix +
                                     "Mismatch between expected reply channel and requested reply "
                                     "channel. Expected: " +
                                     ctx.opts.send_channel + ", Requested: " + replyChannel);
                    continue;
                }

                std::vector<uint8_t> dialMessage = base64::decode(messageB64);

                RaceHandle preConnSMHandle = ctx.manager.startPreConnObjStateMachine(
                    ctx.handle, ctx.recvConnSMHandle, ctx.recvConnId, ctx.opts.recv_channel,
                    ctx.opts.send_channel, ctx.opts.send_role, linkAddress, replyPackageId,
                    {std::move(dialMessage)});

                if (preConnSMHandle == NULL_RACE_HANDLE) {
                    helper::logError(logPrefix + " starting connection state machine failed");
                    return EventResult::NOT_SUPPORTED;
                }

                ctx.preConnObjSM.push(preConnSMHandle);

                break;
            } catch (std::exception &e) {
                helper::logError(logPrefix + "Failed to process received message: " + e.what());
            }
        }

        while (!ctx.acceptCb.empty() && !ctx.preConnObjSM.empty()) {
            auto cb = std::move(ctx.acceptCb.front());
            ctx.acceptCb.pop_front();
            RaceHandle preConnSMHandle = std::move(ctx.preConnObjSM.front());
            ctx.preConnObjSM.pop();
            if (!ctx.manager.onListenAccept(preConnSMHandle, cb)) {
                cb(ApiStatus::INTERNAL_ERROR, {});
            };
        }

        return EventResult::SUCCESS;
    }
};

struct StateListenFinished : public ListenState {
    explicit StateListenFinished(StateType id = STATE_LISTEN_FINISHED) :
        ListenState(id, "STATE_LISTEN_FINISHED") {}
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
    virtual bool finalState() {
        return true;
    }
};

struct StateListenFailed : public ListenState {
    explicit StateListenFailed(StateType id = STATE_LISTEN_FAILED) :
        ListenState(id, "STATE_LISTEN_FAILED") {}
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

ListenStateEngine::ListenStateEngine() {
    // calls startConnectionStateMachine on manager, waits for connStateMachineConnected
    addInitialState<StateListenInitial>(STATE_LISTEN_INITIAL);
    // calls user supplied callback to return the receiver object, always transitions to next state
    addState<StateListenConnectionOpen>(STATE_LISTEN_CONNECTION_OPEN);
    // do nothing, wait for a package to be received, accept to be called, or listener to be closed
    addState<StateListenWaiting>(STATE_LISTEN_WAITING);
    // calls state machine finished on manager, final state
    addState<StateListenFinished>(STATE_LISTEN_FINISHED);
    addFailedState<StateListenFailed>(STATE_LISTEN_FAILED);

    // clang-format off
    declareStateTransition(STATE_LISTEN_INITIAL,                       EVENT_CONN_STATE_MACHINE_CONNECTED, STATE_LISTEN_CONNECTION_OPEN);
    declareStateTransition(STATE_LISTEN_CONNECTION_OPEN,               EVENT_ALWAYS,                       STATE_LISTEN_WAITING);
    declareStateTransition(STATE_LISTEN_WAITING,                       EVENT_RECEIVE_PACKAGE,              STATE_LISTEN_WAITING);
    declareStateTransition(STATE_LISTEN_WAITING,                       EVENT_ACCEPT,                       STATE_LISTEN_WAITING);
    declareStateTransition(STATE_LISTEN_WAITING,                       EVENT_CLOSE,                        STATE_LISTEN_FINISHED);
    // clang-format on
}

std::string ListenStateEngine::eventToString(EventType event) {
    return Raceboat::eventToString(event);
}

}  // namespace Raceboat
