
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

#pragma once

#include "StateMachine.h"

namespace Raceboat {

enum ApiManagerEvent : EventType {
    // EVENT_INVALID = 0,
    // EVENT_FAILED,
    EVENT_RECEIVE_REQUEST = EVENT_FIRST_UNUSED,
    EVENT_ACCEPT,
    EVENT_LISTEN_ACCEPTED,
    EVENT_CHANNEL_ACTIVATED,
    EVENT_LINK_ESTABLISHED,
    EVENT_LINK_DESTROYED,
    EVENT_CONNECTION_ESTABLISHED,
    EVENT_CONNECTION_DESTROYED,
    EVENT_RECEIVE_PACKAGE,
    EVENT_PACKAGE_SENT,
    EVENT_PACKAGE_RECEIVED,
    EVENT_PACKAGE_FAILED,
    EVENT_STATE_MACHINE_FAILED,
    EVENT_STATE_MACHINE_FINISHED,
    EVENT_ADD_DEPENDENT,
    EVENT_DETACH_DEPENDENT,
    EVENT_CONN_STATE_MACHINE_CONNECTED,
    EVENT_CONN_CLOSE,
    EVENT_READ,
    EVENT_WRITE,
    EVENT_CLOSE,
    EVENT_ALWAYS,
    EVENT_RECV_NO_PACKAGES_REMAINING,
    EVENT_RECV_PACKAGES_REMAINING,
};

std::string eventToString(EventType event);

}  // namespace Raceboat