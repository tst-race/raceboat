
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

#include "StateMachine.h"

#include "Events.h"
#include "States.h"
#include "helper.h"

namespace RaceLib {

std::string EventResultString(const EventResult &result) {
    switch (result) {
        case EventResult::NOT_SUPPORTED:
            return "NOT_SUPPORTED";
        case EventResult::SUCCESS:
            return "SUCCESS";
    }
    return "unknown result";
}

// ----------------------------------------------
// State
// ----------------------------------------------
StateType State::nextStateId(Context &, EventType eventId,
                             const std::unordered_set<StateType> &allToStates) {
    TRACE_METHOD(eventId);

    // return state if there's exactly one state to transition to
    // otherwise specialized logic required to choose
    StateType toStateId = STATE_INVALID;
    if (allToStates.size() == 1) {
        toStateId = *allToStates.begin();
    } else {
        helper::logError(logPrefix + "event " + std::to_string(eventId) + " can transition to " +
                         std::to_string(allToStates.size()) + " states for state " + name);
    }
    return toStateId;
}

// ----------------------------------------------
// State Engine
// ----------------------------------------------
StateEngine::StateEngine() : initStateId(STATE_INVALID), failedStateId(STATE_INVALID) {}

void StateEngine::declareInitialTransition(EventType eventId, StateType toStateId) {
    declareStateTransition(initStateId, eventId, toStateId);
}

void StateEngine::declareStateTransition(StateType fromStateId, EventType eventId,
                                         StateType toStateId) {
    validTransitions[fromStateId][eventId].insert(toStateId);
}

EventResult StateEngine::start(Context &context) {
    TRACE_METHOD();
    validateStateMachine();  // TODO: debug only
    EventResult result = EventResult::NOT_SUPPORTED;

    context.pendingEvents = {};

    if (not stateExists(initStateId)) {
        return EventResult::NOT_SUPPORTED;
    }

    State *initState = idToInstanceMap[initStateId].get();
    if (initState->prerequisitesSatisfied(context)) {
        result = initState->enterWrapper(context);

        // check for internal events
        if (result == EventResult::SUCCESS) {
            context.currentStateId = initStateId;
            result = handleEvents(context);
        } else {
            fail(context);
        }
    }
    return result;
}

EventResult StateEngine::handleEvent(Context &context, EventType eventId) {
    TRACE_METHOD(stateToString(context.currentStateId), eventToString(eventId));
    EventResult result = EventResult::NOT_SUPPORTED;

    if (not contextValid(context, logPrefix)) {
        return result;
    }

    context.pendingEvents.push(eventId);
    return handleEvents(context);
}

void StateEngine::fail(Context &context) {
    TRACE_METHOD();
    State &failedState = *idToInstanceMap[failedStateId];
    if (contextValid(context, logPrefix)) {
        State &currentState = *idToInstanceMap[context.currentStateId];
        currentState.exit(context);
    }
    failedState.enterWrapper(context);
    context.currentStateId = failedStateId;
}

// protected members
void StateEngine::addState(StateType stateId, std::shared_ptr<State> state) {
    if (idToInstanceMap.find(stateId) != idToInstanceMap.end()) {
        MAKE_LOG_PREFIX();
        helper::logError(logPrefix + "over-writing pre-existing state " + stateToString(stateId));
    }
    idToInstanceMap[stateId] = state;
}

bool StateEngine::contextValid(Context &context, const std::string &logPrefix) {
    if (idToInstanceMap.find(context.currentStateId) == idToInstanceMap.end()) {
        helper::logError(logPrefix + " invalid state " + stateToString(context.currentStateId));
        return false;
    }
    return true;
}

bool StateEngine::stateExists(StateType stateId) {
    MAKE_LOG_PREFIX();

    auto stateIdsIt = idToInstanceMap.find(stateId);
    if (stateIdsIt == idToInstanceMap.end()) {
        helper::logDebug(logPrefix + " no state for state " + std::to_string(stateId));
        return false;
    } else if (stateIdsIt->second.get() == nullptr) {
        helper::logDebug(logPrefix + " null state for state " + std::to_string(stateId));
        return false;
    }
    return true;
}

EventResult StateEngine::handleEvents(Context &context) {
    MAKE_LOG_PREFIX();

    // We need to return success if there are no events
    EventResult result = EventResult::SUCCESS;
    while (!context.pendingEvents.empty()) {
        EventType eventId = context.pendingEvents.front();
        context.pendingEvents.pop();
        helper::logDebug(logPrefix + " handling event " + eventToString(eventId));

        if (stateHandlesEvent(context.currentStateId, eventId)) {
            // State exists for id and handles eventId
            State &currentState = *idToInstanceMap[context.currentStateId];
            StateType nextStateId = getNextStateId(context, currentState, eventId);

            if (stateExists(nextStateId)) {
                State &nextState = *idToInstanceMap[nextStateId];
                result = transitionToState(context, currentState, nextState);
            }
        } else {
            result = EventResult::NOT_SUPPORTED;  // not supported if not in transition map
            helper::logError(logPrefix + " state " + stateToString(context.currentStateId) +
                             " doesn't handle event " + eventToString(eventId));
        }

        if (result != EventResult::SUCCESS) {
            fail(context);
            break;
        }
    }

    return result;
}

bool StateEngine::stateHandlesEvent(StateType stateId, EventType eventId) {
    MAKE_LOG_PREFIX();
    bool success = false;

    if (stateExists(stateId)) {
        if (validTransitions[stateId].find(eventId) != validTransitions[stateId].end()) {
            success = true;
        } else {
            helper::logDebug(logPrefix + " event " + eventToString(eventId) +
                             " not registered for state " + stateToString(stateId));
        }
    }
    return success;
}

EventResult StateEngine::transitionToState(Context &context, State &curr, State &next) {
    TRACE_METHOD(curr.stateId, next.stateId);
    // exit current state, then enter next state
    EventResult result = curr.exitWrapper(context);

    if (result == EventResult::NOT_SUPPORTED) {
        helper::logError(logPrefix + " state " + stateToString(curr.stateId) +
                         " exit returned not-supported");
        return result;
    }

    if (not next.prerequisitesSatisfied(context)) {
        helper::logError(logPrefix + " next state " + stateToString(next.stateId) + " not ready");
        return EventResult::NOT_SUPPORTED;
    }

    result = next.enterWrapper(context);

    if (result == EventResult::NOT_SUPPORTED) {
        helper::logError(logPrefix + " failed to enter next state " + stateToString(next.stateId));
    } else {
        context.currentStateId = next.stateId;
    }
    return result;
}

StateType StateEngine::getNextStateId(Context &context, State &currentState, EventType eventId) {
    StateType nextStateId = STATE_INVALID;
    auto stateEventsPair = validTransitions.find(currentState.stateId);
    if (stateEventsPair != validTransitions.end()) {
        auto eventToStatesPair = stateEventsPair->second.find(eventId);
        // TODO this can return 1+ to-states, delegate decision to state
        if (eventToStatesPair != stateEventsPair->second.end()) {
            nextStateId = currentState.nextStateId(context, eventId, eventToStatesPair->second);
        }
    }
    return nextStateId;
}

std::string StateEngine::stateToString(StateType stateId) {
    if (stateExists(stateId)) {
        State &state = *idToInstanceMap[stateId];
        return state.name;
    } else {
        return "unknown state " + std::to_string(stateId);
    }
}

std::string StateEngine::eventToString(EventType event) {
    // this is expected to be overridden in a subclass
    return "event" + std::to_string(event);
}

bool StateEngine::validateStateMachine() {
    TRACE_METHOD();
    bool success = true;
    std::unordered_map<StateType, bool> toStateTransitions;

    if (failedStateId == STATE_INVALID) {
        success = false;
        helper::logError(logPrefix + " invalid failed state");
    }
    if (initStateId == STATE_INVALID) {
        success = false;
        helper::logError(logPrefix + "invalid init state");
    }

    for (auto idInstPair : idToInstanceMap) {
        // every from-state must be registered in validTransitions except FAILED and final states
        if (idInstPair.first != failedStateId && not idInstPair.second->finalState() &&
            validTransitions.find(idInstPair.first) == validTransitions.end()) {
            success = false;
            helper::logError(logPrefix + " state " + stateToString(idInstPair.first) +
                             " not in valid state transitions");
        }

        // every state must have a valid instance
        if (idInstPair.second.get() == nullptr) {
            success = false;
            helper::logError(logPrefix + " invalid state instance for state " +
                             stateToString(idInstPair.first));
        }

        if (idInstPair.first != initStateId) {
            toStateTransitions[idInstPair.first] = false;
        }
    }

    // validTransitions from-stateId -> eventIds -> to-stateIds
    for (auto stateEventsMapPair : validTransitions) {
        // invalid from-stateId check
        if (stateEventsMapPair.first == STATE_INVALID ||
            stateEventsMapPair.first == failedStateId) {
            success = false;
            helper::logError(logPrefix + "invalid from-state " +
                             stateToString(stateEventsMapPair.first));
        }

        // every valid transition must have an entry
        auto idInstPair = idToInstanceMap.find(stateEventsMapPair.first);
        if (idInstPair == idToInstanceMap.end()) {
            success = false;
            helper::logError(logPrefix + " state " + stateToString(stateEventsMapPair.first) +
                             " has no corresponding entry");
        }

        // every state must declare a transition except the final states
        if (stateEventsMapPair.first != failedStateId) {
            if (stateEventsMapPair.second.empty()) {
                success = false;
                helper::logError(logPrefix + " state " + stateToString(stateEventsMapPair.first) +
                                 " has no events registered");
            }

            // every event must transition to at least one state
            for (auto eventToStateSetPair : stateEventsMapPair.second) {
                if (eventToStateSetPair.second.empty()) {
                    success = false;
                    helper::logError(logPrefix + "event Id " +
                                     eventToString(eventToStateSetPair.first) + " for state " +
                                     stateToString(stateEventsMapPair.first) +
                                     " transitions to no states");
                }

                for (auto toState : eventToStateSetPair.second) {
                    toStateTransitions[toState] = true;
                    // invalid to-stateId check
                    if (toState == STATE_INVALID) {
                        success = false;
                        helper::logError(logPrefix + "invalid to-state " + stateToString(toState));
                    }
                }
            }
        }
    }

    // every state must be able to be transitioned to except init
    for (auto idEnteredpair : toStateTransitions) {
        if (idEnteredpair.second == false && idEnteredpair.first != initStateId &&
            idEnteredpair.first != failedStateId) {
            helper::logError(logPrefix + " Non-init state (" + stateToString(idEnteredpair.first) +
                             ") must have transition to it");
        }
    }
    return success;
}
}  // namespace RaceLib
