
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

#include "ConnectionStateMachine.h"

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

void ApiConnContext::updateConnStateMachineStart(RaceHandle _contextHandle,
                                                 ChannelId _channelId,
                                                 std::string _role,
                                                 std::string _linkAddress,
                                                 bool _sending) {
  this->dependents.insert(_contextHandle);
  this->newestDependent = _contextHandle;
  this->channelId = _channelId;
  this->channelRole = _role;
  this->linkAddress = _linkAddress;
  this->send = _sending;
}

void ApiConnContext::updateChannelStatusChanged(
    RaceHandle /* _chanHandle */, const ChannelId & /* _channelGid */,
    ChannelStatus /* _status */, const ChannelProperties & /* _properties */) {}
void ApiConnContext::updateLinkStatusChanged(
    RaceHandle /* _linkHandle */, const LinkID &_linkId,
    LinkStatus /* _status */, const LinkProperties &_properties) {
  this->linkId = _linkId;
  this->updatedLinkAddress = _properties.linkAddress;
}
void ApiConnContext::updateConnectionStatusChanged(
    RaceHandle /* _connHandle */, const ConnectionID &_connId,
    ConnectionStatus /* _status */, const LinkProperties & /* _properties */) {
  this->connId = _connId;
}

void ApiConnContext::updateDependent(RaceHandle contextHandle) {
  this->dependents.insert(contextHandle);
  this->newestDependent = contextHandle;
  this->detachedDependent = NULL_RACE_HANDLE;
}

void ApiConnContext::updateDetach(RaceHandle contextHandle) {
  this->dependents.erase(contextHandle);
  this->newestDependent = NULL_RACE_HANDLE;
  this->detachedDependent = contextHandle;
}

void ApiConnContext::updateStateMachineFinished(RaceHandle contextHandle) {
  this->dependents.erase(contextHandle);
}

void ApiConnContext::updateStateMachineFailed(RaceHandle contextHandle) {
  this->dependents.erase(contextHandle);
}

//-----------------------------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------------------------

struct StateConnInitial : public ConnState {
  explicit StateConnInitial(StateType id = STATE_CONN_INITIAL)
      : ConnState(id, "STATE_CONN_INITIAL") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    // listen to events from the parent (a recv/send state machine). This will
    // cause EVENT_CONN_CLOSE, EVENT_STATE_MACHINE_FINISHED,
    // EVENT_STATE_MACHINE_FAILED to be forwarded to us if triggered by the
    // parent
    ctx.manager.registerHandle(ctx, ctx.newestDependent);

    RaceHandle chanHandle = ctx.manager.getCore().generateHandle();
    auto response = ctx.manager.activateChannel(ctx, chanHandle, ctx.channelId,
                                                ctx.channelRole);

    if (response != ActivateChannelStatusCode::OK &&
        response != ActivateChannelStatusCode::ALREADY_ACTIVATED) {
      helper::logError(logPrefix + "Activating channel failed with status: " +
                       activateChannelStatusCodeToString(response));
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, chanHandle);
    ctx.manager.registerId(ctx, ctx.channelId);

    return EventResult::SUCCESS;
  }
};

struct StateConnActivated : public ConnState {
  explicit StateConnActivated(StateType id = STATE_CONN_ACTIVATED)
      : ConnState(id, "STATE_CONN_ACTIVATED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    RaceHandle linkHandle = ctx.manager.getCore().generateHandle();
    PluginWrapper &plugin = getPlugin(ctx, ctx.channelId);

    SdkResponse response = SDK_INVALID;
    if (ctx.send && ctx.linkAddress.empty()) {
      throw std::invalid_argument("missing link address");
    } else if (ctx.send && !ctx.linkAddress.empty()) {
      response =
          plugin.loadLinkAddress(linkHandle, ctx.channelId, ctx.linkAddress, 0);
    } else if (ctx.linkAddress.empty()) {
      response = plugin.createLink(linkHandle, ctx.channelId, 0);
    } else {
      response = plugin.createLinkFromAddress(linkHandle, ctx.channelId,
                                              ctx.linkAddress, 0);
    }

    if (response.status != SDK_OK) {
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, linkHandle);

    return EventResult::SUCCESS;
  }
};

struct StateConnLinkEstablished : public ConnState {
  explicit StateConnLinkEstablished(StateType id = STATE_CONN_LINK_ESTABLISHED)
      : ConnState(id, "STATE_CONN_LINK_ESTABLISHED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    PluginWrapper &plugin = getPlugin(ctx, ctx.channelId);
    RaceHandle openConnHandle = ctx.manager.getCore().generateHandle();

    ctx.manager.registerId(ctx, ctx.linkId);

    if (!ctx.send && !ctx.linkAddress.empty() &&
        ctx.updatedLinkAddress != ctx.linkAddress) {
      nlohmann::json updatedJson =
          nlohmann::json::parse(ctx.updatedLinkAddress);
      nlohmann::json originalJson = nlohmann::json::parse(ctx.linkAddress);
      if (updatedJson != originalJson) {
        helper::logError(logPrefix +
                         "received link address does not match requested link "
                         "address supplied "
                         "by user. Requested: " +
                         ctx.linkAddress + " got: " + ctx.updatedLinkAddress);
        return EventResult::NOT_SUPPORTED;
      }
    }

    LinkType linkType = ctx.send ? LT_SEND : LT_RECV;
    SdkResponse response = plugin.openConnection(openConnHandle, linkType,
                                                 ctx.linkId, "{}", 0, 0, 0);

    if (response.status != SdkStatus::SDK_OK) {
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, openConnHandle);

    return EventResult::SUCCESS;
  }
};

struct StateConnConnectionOpen : public ConnState {
  explicit StateConnConnectionOpen(StateType id = STATE_CONN_CONNECTION_OPEN)
      : ConnState(id, "STATE_CONN_CONNECTION_OPEN") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    ctx.manager.registerId(ctx, ctx.connId);
    ctx.manager.connStateMachineConnected(ctx.handle, ctx.connId,
                                          ctx.updatedLinkAddress);

    ctx.pendingEvents.push(EVENT_ALWAYS);

    return EventResult::SUCCESS;
  }
};

struct StateConnConnected : public ConnState {
  explicit StateConnConnected(StateType id = STATE_CONN_CONNECTED)
      : ConnState(id, "STATE_CONN_CONNECTED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);

    if (ctx.newestDependent != NULL_RACE_HANDLE) {
      ctx.manager.registerHandle(ctx, ctx.newestDependent);
      ctx.newestDependent = NULL_RACE_HANDLE;
    }

    if (ctx.detachedDependent != NULL_RACE_HANDLE) {
      ctx.manager.unregisterHandle(ctx, ctx.detachedDependent);
      ctx.detachedDependent = NULL_RACE_HANDLE;
    }

    if (ctx.dependents.empty()) {
      ctx.pendingEvents.push(EVENT_CONN_CLOSE);
    }

    return EventResult::SUCCESS;
  }
};

struct StateConnClosing : public ConnState {
  explicit StateConnClosing(StateType id = STATE_CONN_CLOSING)
      : ConnState(id, "STATE_CONN_CLOSING") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    if (context.currentStateId == stateId) {
      // This can loop back to the same state if a EVENT_PACKAGE_RECEIVED
      // happens In that case, don't do anything.
      return EventResult::SUCCESS;
    }

    auto &ctx = getContext(context);
    PluginWrapper &plugin = getPlugin(ctx, ctx.channelId);
    RaceHandle closeConnHandle = ctx.manager.getCore().generateHandle();

    SdkResponse response =
        plugin.closeConnection(closeConnHandle, ctx.connId, 0);

    if (response.status != SdkStatus::SDK_OK) {
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, closeConnHandle);

    return EventResult::SUCCESS;
  }
};

struct StateConnConnectionClosed : public ConnState {
  explicit StateConnConnectionClosed(
      StateType id = STATE_CONN_CONNECTION_CLOSED)
      : ConnState(id, "STATE_CONN_CONNECTION_CLOSED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    PluginWrapper &plugin = getPlugin(ctx, ctx.channelId);
    RaceHandle destroyLinkHandle = ctx.manager.getCore().generateHandle();

    SdkResponse response = plugin.destroyLink(destroyLinkHandle, ctx.linkId, 0);

    if (response.status != SdkStatus::SDK_OK) {
      return EventResult::NOT_SUPPORTED;
    }

    ctx.manager.registerHandle(ctx, destroyLinkHandle);

    return EventResult::SUCCESS;
  }
};

struct StateConnLinkClosed : public ConnState {
  explicit StateConnLinkClosed(StateType id = STATE_CONN_LINK_CLOSED)
      : ConnState(id, "STATE_CONN_LINK_CLOSED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    ctx.manager.stateMachineFinished(ctx);
    return EventResult::SUCCESS;
  }
  virtual bool finalState() { return true; }
};

struct StateConnFailed : public ConnState {
  explicit StateConnFailed(StateType id = STATE_CONN_FAILED)
      : ConnState(id, "STATE_CONN_FAILED") {}
  virtual EventResult enter(Context &context) {
    TRACE_METHOD();
    auto &ctx = getContext(context);
    ctx.manager.stateMachineFailed(ctx);
    return EventResult::SUCCESS;
  }
};

//-----------------------------------------------------------------------------------------------
// StateEngine
//-----------------------------------------------------------------------------------------------

ConnStateEngine::ConnStateEngine() {
  // calls activateChannel on plugin, waits for
  // onChannelStatusChanged(status=CHANEL_ACTIVATED)
  addInitialState<StateConnInitial>(STATE_CONN_INITIAL);

  // calls load/create link on plugin, waits for
  // onLinkStatusChanged(status=LINK_CREATED/LOADED)
  addState<StateConnActivated>(STATE_CONN_ACTIVATED);

  // calls openConnection link on plugin, waits for
  // onConnectionStatusChanged(status=CONNECTION_OPEN)
  addState<StateConnLinkEstablished>(STATE_CONN_LINK_ESTABLISHED);

  // Tell the parent (e.g. a send or a recv) state machine that we're connected
  // and always transition to the next state
  addState<StateConnConnectionOpen>(STATE_CONN_CONNECTION_OPEN);

  // wait for all state machines that depend on this state machine to stop
  addState<StateConnConnected>(STATE_CONN_CONNECTED);

  // calls closeConnection on the plugin, wait for
  // onConnectionStatusChanged(status=CONNECTION_CLOSED)
  addState<StateConnClosing>(STATE_CONN_CLOSING);

  // calls destroyLink on the plugin, wait for
  // onLinkStatusChanged(status=LINK_DESTROYED)
  addState<StateConnConnectionClosed>(STATE_CONN_CONNECTION_CLOSED);

  // call state machine finished, final state
  addState<StateConnLinkClosed>(STATE_CONN_LINK_CLOSED);

  // call state machine failed
  // TODO: clean up link if it was created
  addFailedState<StateConnFailed>(STATE_CONN_FAILED);

  // clang-format off
    // activating -> activated -> link established -> connected -> closing -> closed -> link destroyed
    declareStateTransition(STATE_CONN_INITIAL,           EVENT_CHANNEL_ACTIVATED,      STATE_CONN_ACTIVATED);
    declareStateTransition(STATE_CONN_ACTIVATED,         EVENT_LINK_ESTABLISHED,       STATE_CONN_LINK_ESTABLISHED);
    declareStateTransition(STATE_CONN_LINK_ESTABLISHED,  EVENT_CONNECTION_ESTABLISHED, STATE_CONN_CONNECTION_OPEN);
    declareStateTransition(STATE_CONN_CONNECTION_OPEN,   EVENT_ALWAYS,                 STATE_CONN_CONNECTED);
    declareStateTransition(STATE_CONN_CONNECTED,         EVENT_ADD_DEPENDENT,          STATE_CONN_CONNECTED);
    declareStateTransition(STATE_CONN_CONNECTED,         EVENT_DETACH_DEPENDENT,       STATE_CONN_CONNECTED);

    // These are the transitions when a state machine that depends on this state machine fails
    declareStateTransition(STATE_CONN_CONNECTED,         EVENT_STATE_MACHINE_FINISHED, STATE_CONN_CONNECTED);
    declareStateTransition(STATE_CONN_CONNECTED,         EVENT_STATE_MACHINE_FAILED,   STATE_CONN_CONNECTED);

    declareStateTransition(STATE_CONN_CONNECTED,         EVENT_CONN_CLOSE,             STATE_CONN_CLOSING);
    declareStateTransition(STATE_CONN_CLOSING,           EVENT_CONNECTION_DESTROYED,   STATE_CONN_CONNECTION_CLOSED);
    declareStateTransition(STATE_CONN_CONNECTION_CLOSED, EVENT_LINK_DESTROYED,         STATE_CONN_LINK_CLOSED);

    // We want to ignore EVENT_RECEIVE_PACKAGE. We're listening to the connId, but we only care
    // about the onConnectionStatusChanged call, not the recvEncPkg
    declareStateTransition(STATE_CONN_CONNECTED,         EVENT_RECEIVE_PACKAGE,        STATE_CONN_CONNECTED);
    declareStateTransition(STATE_CONN_CLOSING,           EVENT_RECEIVE_PACKAGE,        STATE_CONN_CLOSING);
  // clang-format on
}

std::string ConnStateEngine::eventToString(EventType event) {
  return Raceboat::eventToString(event);
}

} // namespace Raceboat
