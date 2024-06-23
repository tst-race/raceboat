
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
#include "../../source/state-machine/StateMachine.h"
#include "gtest/gtest.h"

using namespace Raceboat;
using namespace testing;

enum TestStates { FIRST_STATE = STATE_INIT, SECOND_STATE, THIRD_STATE, UNUSED_STATE };

enum TestEvents { FIRST_TRANSITION = EVENT_FIRST_UNUSED, SECOND_TRANSITION };

class StateMachineTestFixture : public ::testing::Test {
public:
    StateMachineTestFixture() {
        context = std::make_shared<ConduitContext>();
    }

    struct TestableStateEngine : public StateEngine {
        TestableStateEngine() {
            addInitialState<State>(STATE_INIT);
            addState<State>(FIRST_STATE);
            addState<State>(SECOND_STATE);
            addState<FinalState>(THIRD_STATE);
            addFailedState<State>(STATE_FAILED);

            declareInitialTransition(FIRST_TRANSITION, SECOND_STATE);
            declareStateTransition(SECOND_STATE, SECOND_TRANSITION, THIRD_STATE);
        }
    } engine;
    Core core;
    std::shared_ptr<ConduitContext> context;
};

TEST_F(StateMachineTestFixture, validate) {
    EXPECT_TRUE(engine.validateStateMachine());
}

TEST_F(StateMachineTestFixture, start) {
    EXPECT_EQ(context->currentStateId, STATE_INVALID);
    engine.start(*context);
    EXPECT_EQ(context->currentStateId, STATE_INIT);
}

TEST_F(StateMachineTestFixture, handleEvent) {
    engine.start(*context);
    EXPECT_EQ(context->currentStateId, FIRST_STATE);
    engine.handleEvent(*context, FIRST_TRANSITION);
    EXPECT_EQ(context->currentStateId, SECOND_STATE);
    engine.handleEvent(*context, SECOND_TRANSITION);
    EXPECT_EQ(context->currentStateId, THIRD_STATE);
}

TEST_F(StateMachineTestFixture, handleUnregisteredEvent) {
    engine.start(*context);
    engine.handleEvent(*context, SECOND_TRANSITION);
    EXPECT_EQ(context->currentStateId, STATE_FAILED);
}

TEST_F(StateMachineTestFixture, fail) {
    engine.start(*context);
    engine.handleEvent(*context, 42);
    EXPECT_EQ(context->currentStateId, STATE_FAILED);
}

TEST_F(StateMachineTestFixture, allStatesFail) {
    for (StateType state = FIRST_STATE; state < UNUSED_STATE; ++state) {
        context->currentStateId = state;
        EXPECT_EQ(context->currentStateId, state);
        engine.handleEvent(*context, STATE_INVALID);
        EXPECT_EQ(context->currentStateId, STATE_FAILED);
    }
}
