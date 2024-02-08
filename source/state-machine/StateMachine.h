
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

#include <stdint.h>

#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "helper.h"

// This implementation provides the basis to traverse states upon internal and external events
// Priorities (descending): readability, ease of use, flexibility/expressiveness
// Given a State and a Context: events are the stimulus to transition to another State
// Users declare States and transitions so the intention can be easily inferred

// Usage notes:
// - Users will likely specialize StateEngine, State, and Context
// - Users must declare an initial state and a failed state
// - Undeclared State transitions result in failed State
// - All States should be able to transition to another State unless State::finalState()
// - StateEngine::validateStateMachine() identifies problems with state transition declarations
//
// - External events are handled via StateEngine::handleEvent()
// - Internal events can be added to Context::pendingEvents by State virtual methods called in
// handleEvent()
//
// - All stateful info is in Context, no stateful info is in State or StateEngine.
//   This allows use of just one State instance per state throughout the lifecycle of the
//   StateEngine as opposed to tasking each State implementation with creating instances of states
//   resulting from events

namespace RaceLib {

enum struct EventResult {
    NOT_SUPPORTED,  // state does not handle event
    SUCCESS         // success
};

using StateType = uint32_t;
typedef enum : StateType {
    STATE_INVALID = 0,
    STATE_FAILED,
    STATE_INIT,
    STATE_FIRST_UNUSED,
} States;

using EventType = uint32_t;
typedef enum : EventType {
    EVENT_INVALID = 0,
    EVENT_FAILED,
    EVENT_FIRST_UNUSED,
} Events;

// ----------------------------------------------
// helper functions
// ----------------------------------------------
std::string EventResultString(const EventResult &result);

struct Context;
template <class T>
T *getDerivedContext(Context *context) {
    T *derived = nullptr;
    if (context) {
        try {
            derived = dynamic_cast<T *>(context);
        } catch (const std::exception &ex) {
            helper::logError("could not derive " + std::string(typeid(T).name()) + " from Context");
        }
    } else {
        helper::logError("null context passed when trying to cast Context to " +
                         std::string(typeid(T).name()));
    }
    return derived;
}

// ----------------------------------------------
// State Context Base
// ----------------------------------------------
struct Context {
    Context() : currentStateId(STATE_INVALID) {}
    virtual ~Context() {}

    StateType currentStateId;
    std::queue<EventType> pendingEvents;
};

// ----------------------------------------------
// State Base
// ----------------------------------------------
struct State {
    explicit State(StateType id, std::string name) : stateId(id), name(name) {}
    virtual ~State() {}

    virtual EventResult enter(Context &) {
        return EventResult::SUCCESS;
    }
    virtual EventResult exit(Context &) {
        return EventResult::SUCCESS;
    }

    EventResult enterWrapper(Context &context) {
        MAKE_LOG_PREFIX();
        try {
            EventResult result = enter(context);
            if (result != EventResult::SUCCESS) {
                helper::logError(logPrefix + " failed with return : " + EventResultString(result));
            }
            return result;
        } catch (std::exception &e) {
            helper::logError(logPrefix + " failed with error: " + std::string(e.what()));
            return EventResult::NOT_SUPPORTED;
        } catch (...) {
            helper::logError(logPrefix + " failed with unknown error");
            return EventResult::NOT_SUPPORTED;
        }
    }

    EventResult exitWrapper(Context &context) {
        MAKE_LOG_PREFIX();
        try {
            EventResult result = exit(context);
            if (result != EventResult::SUCCESS) {
                helper::logError(logPrefix + " failed with return : " + EventResultString(result));
            }
            return result;
        } catch (std::exception &e) {
            helper::logError(logPrefix + " failed with error: " + std::string(e.what()));
            return EventResult::NOT_SUPPORTED;
        } catch (...) {
            helper::logError(logPrefix + " failed with unknown error");
            return EventResult::NOT_SUPPORTED;
        }
    }

    virtual bool prerequisitesSatisfied(Context &) {
        return true;
    }

    virtual StateType nextStateId(Context &context, EventType eventId,
                                  const std::unordered_set<StateType> &allToStates);

    // return true for no transitions
    virtual bool finalState() {
        return false;
    }

    const StateType stateId;
    const std::string name;
};

struct FinalState : public State {
    explicit FinalState(StateType id, std::string name) : State(id, name) {}
    virtual bool finalState() {
        return true;
    }
};

// ----------------------------------------------
// State Engine Base
// ----------------------------------------------
// TODOs
//  - support n links per channel, each with 1 connection
class StateEngine {
public:
    explicit StateEngine();
    virtual ~StateEngine() {}

    // setup methods used before calling start()
    template <class ClassT>
    void addState(StateType stateId) {
        addState(stateId, std::make_shared<ClassT>(stateId));
    }
    template <class ClassT>
    void addInitialState(StateType stateId) {
        std::shared_ptr<ClassT> state = std::make_shared<ClassT>(stateId);
        initStateId = stateId;
        addState(stateId, state);
    }
    template <class ClassT>
    void addFailedState(StateType stateId) {
        std::shared_ptr<ClassT> state = std::make_shared<ClassT>(stateId);
        failedStateId = state->stateId;
        addState(stateId, state);
    }

    void declareInitialTransition(EventType eventId, StateType toStateId);
    void declareStateTransition(StateType fromStateId, EventType eventId, StateType toStateId);

    // events
    EventResult start(Context &context);
    EventResult handleEvent(Context &context, EventType eventId);
    void fail(Context &context);

    std::string stateToString(StateType stateId);
    virtual std::string eventToString(EventType event);

    bool validateStateMachine();  // for test/debug

protected:
    // internal helpers
    void addState(StateType stateId, std::shared_ptr<State> state);
    bool contextValid(Context &context, const std::string &scope);
    bool stateExists(StateType stateId);
    EventResult handleEvents(Context &context);
    EventResult transitionToState(Context &context, State &currState, State &nextState);
    bool stateHandlesEvent(StateType stateId, EventType eventId);
    StateType getNextStateId(Context &context, State &state, EventType eventId);

private:
    // from-state ID -> event IDs -> to-state IDs
    using StateSet = std::unordered_set<StateType>;
    using EventMap = std::unordered_map<EventType, StateSet>;
    using StateTransitionMap = std::unordered_map<StateType, EventMap>;
    StateTransitionMap validTransitions;

    using StateIds = std::unordered_map<StateType, std::shared_ptr<State>>;
    StateIds idToInstanceMap;

    StateType initStateId;
    StateType failedStateId;
};
}  //  namespace RaceLib
