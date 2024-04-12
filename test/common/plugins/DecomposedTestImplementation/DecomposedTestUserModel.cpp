
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

#include <common/RaceLog.h>
#include <decomposed/IUserModelComponent.h>

#include <unordered_set>

#define TRACE_METHOD(...) TRACE_METHOD_BASE(DecomposedTestUserModel, ##__VA_ARGS__)

class DecomposedTestUserModel : public IUserModelComponent {
public:
    explicit DecomposedTestUserModel(IUserModelSdk *sdk) : sdk(sdk), actionId(0) {
        TRACE_METHOD();
        sdk->updateState(COMPONENT_STATE_STARTED);
    }

    virtual ComponentStatus onUserInputReceived(RaceHandle /* handle */, bool /* answered */,
                                                const std::string & /* response */) override {
        TRACE_METHOD();
        return COMPONENT_OK;
    }

    virtual UserModelProperties getUserModelProperties() override {
        // TRACE_METHOD();

        return UserModelProperties{10, 5};
    }

    virtual ComponentStatus addLink(const LinkID &link,
                                    const LinkParameters & /* params */) override {
        TRACE_METHOD(link);
        links.insert(link);
        sdk->onTimelineUpdated();
        return COMPONENT_OK;
    }

    virtual ComponentStatus removeLink(const LinkID &link) override {
        TRACE_METHOD(link);
        links.erase(link);
        sdk->onTimelineUpdated();
        return COMPONENT_OK;
    }

    virtual ActionTimeline getTimeline(Timestamp start, Timestamp end) override {
        TRACE_METHOD(start, end);
        return {};
    }

    virtual ActionTimeline onSendPackage(const LinkID &linkId, int /* bytes */)override {
        Action action;
        action.actionId = actionId++;
        action.timestamp = 0;
        action.json = linkId;
        return {action};
    };

    virtual ComponentStatus onTransportEvent(const Event & /* event */) override {
        TRACE_METHOD();
        return COMPONENT_OK;
    }

private:
    IUserModelSdk *sdk;

    std::unordered_set<LinkID> links;
    uint64_t actionId;
};

IUserModelComponent *createUserModel(const std::string & /* usermodel */, IUserModelSdk *sdk,
                                     const std::string & /* roleName */,
                                     const PluginConfig & /* pluginConfig */) {
    return new DecomposedTestUserModel(sdk);
}

void destroyUserModel(IUserModelComponent *component) {
    delete component;
}
