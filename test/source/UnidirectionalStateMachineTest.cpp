
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

#include "../../source/Core.h"
#include "../../source/state-machine/UnidirectionalStateMachine.h"
#include "gtest/gtest.h"

using namespace Raceboat;
using namespace testing;

TEST(UnidirectionalStateMachine, DISABLED_sendEngine) {}

TEST(UnidirectionalStateMachine, DISABLED_recvEngine) {}

TEST(UnidirectionalStateMachine, allStatesFail) {
    UnidirectionalContext context;
    UniSendStateEngine sendEngine;
    UniReceiveStateEngine recvEngine;

    for (StateType state = STATE_PACKAGE_SENDING; state <= STATE_PACKAGE_SENT; ++state) {
        context.currentStateId = state;
        sendEngine.handleEvent(context, STATE_INVALID);
        EXPECT_EQ(context.currentStateId, STATE_FAILED);
    }

    for (StateType state = STATE_PACKAGE_RECEIVING; state <= STATE_PACKAGE_RECEIVED; ++state) {
        context.currentStateId = state;
        recvEngine.handleEvent(context, STATE_INVALID);
        EXPECT_EQ(context.currentStateId, STATE_FAILED);
    }
}
