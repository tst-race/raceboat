
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

class DecomposedTestUserModel : public IUserModelComponent {
public:
    explicit DecomposedTestUserModel(IUserModelSdk * /* sdk */) {}

    virtual ComponentStatus onUserInputReceived(RaceHandle /* handle */, bool /* answered */,
                                                const std::string & /* response */) override {
        return {};
    }

    virtual UserModelProperties getUserModelProperties() override {
        return {};
    }

    virtual ComponentStatus addLink(const LinkID & /* link */,
                                    const LinkParameters & /* params */) override {
        return {};
    }

    virtual ComponentStatus removeLink(const LinkID & /* link */) override {
        return {};
    }

    virtual ActionTimeline getTimeline(Timestamp /* start */, Timestamp /* end */) override {
        return {};
    }

    virtual ComponentStatus onTransportEvent(const Event & /* event */) override {
        return {};
    }
};

IUserModelComponent *createUserModel(const std::string & /* usermodel */, IUserModelSdk *sdk,
                                     const std::string & /* roleName */,
                                     const PluginConfig & /* pluginConfig */) {
    return new DecomposedTestUserModel(sdk);
}

void destroyUserModel(IUserModelComponent *component) {
    delete component;
}
