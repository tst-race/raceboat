
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

#include "Events.h"

namespace Raceboat {

std::string eventToString(EventType event) {
    switch (event) {
        case EVENT_INVALID:
            return "EVENT_INVALID";
        case EVENT_FAILED:
            return "EVENT_FAILED";
        case EVENT_RECEIVE_REQUEST:
            return "EVENT_RECEIVE_REQUEST";
        case EVENT_ACCEPT:
            return "EVENT_ACCEPT";
        case EVENT_LISTEN_ACCEPTED:
            return "EVENT_LISTEN_ACCEPTED";
        case EVENT_CHANNEL_ACTIVATED:
            return "EVENT_CHANNEL_ACTIVATED";
        case EVENT_LINK_ESTABLISHED:
            return "EVENT_LINK_ESTABLISHED";
        case EVENT_LINK_DESTROYED:
            return "EVENT_LINK_DESTROYED";
        case EVENT_CONNECTION_ESTABLISHED:
            return "EVENT_CONNECTION_ESTABLISHED";
        case EVENT_CONNECTION_DESTROYED:
            return "EVENT_CONNECTION_DESTROYED";
        case EVENT_RECEIVE_PACKAGE:
            return "EVENT_RECEIVE_PACKAGE";
        case EVENT_PACKAGE_SENT:
            return "EVENT_PACKAGE_SENT";
        case EVENT_PACKAGE_RECEIVED:
            return "EVENT_PACKAGE_RECEIVED";
        case EVENT_PACKAGE_FAILED:
            return "EVENT_PACKAGE_FAILED";
        case EVENT_STATE_MACHINE_FAILED:
            return "EVENT_STATE_MACHINE_FAILED";
        case EVENT_STATE_MACHINE_FINISHED:
            return "EVENT_STATE_MACHINE_FINISHED";
        case EVENT_ADD_DEPENDENT:
            return "EVENT_ADD_DEPENDENT";
        case EVENT_DETACH_DEPENDENT:
            return "EVENT_DETACH_DEPENDENT";
        case EVENT_CONN_STATE_MACHINE_CONNECTED:
            return "EVENT_CONN_STATE_MACHINE_CONNECTED";
        case EVENT_CONN_CLOSE:
            return "EVENT_CONN_CLOSE";
        case EVENT_READ:
            return "EVENT_READ";
        case EVENT_WRITE:
            return "EVENT_WRITE";
        case EVENT_CLOSE:
            return "EVENT_CLOSE";
        case EVENT_ALWAYS:
            return "EVENT_ALWAYS";
        case EVENT_RECV_NO_PACKAGES_REMAINING:
            return "EVENT_RECV_NO_PACKAGES_REMAINING";
        case EVENT_RECV_PACKAGES_REMAINING:
            return "EVENT_RECV_PACKAGES_REMAINING";
    }
    return "event " + std::to_string(event);
}

}  // namespace Raceboat