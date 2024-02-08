
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
#include <decomposed/ITransportComponent.h>

class DecomposedTestTransport : public ITransportComponent {
public:
    explicit DecomposedTestTransport(ITransportSdk * /* sdk */) {}

    virtual ComponentStatus onUserInputReceived(RaceHandle /* handle */, bool /* answered */,
                                                const std::string & /* response */) override {
        return {};
    }

    virtual TransportProperties getTransportProperties() override {
        return {};
    }

    virtual LinkProperties getLinkProperties(const LinkID & /* linkId */) override {
        return {};
    }

    virtual ComponentStatus createLink(RaceHandle /* handle */,
                                       const LinkID & /* linkId */) override {
        return {};
    }

    virtual ComponentStatus loadLinkAddress(RaceHandle /* handle */, const LinkID & /* linkId */,
                                            const std::string & /* linkAddress */) override {
        return {};
    }

    virtual ComponentStatus loadLinkAddresses(
        RaceHandle /* handle */, const LinkID & /* linkId */,
        const std::vector<std::string> & /* linkAddress */) override {
        return {};
    }

    virtual ComponentStatus createLinkFromAddress(RaceHandle /* handle */,
                                                  const LinkID & /* linkId */,
                                                  const std::string & /* linkAddress */) override {
        return {};
    }

    virtual ComponentStatus destroyLink(RaceHandle /* handle */,
                                        const LinkID & /* linkId */) override {
        return {};
    }

    virtual std::vector<EncodingParameters> getActionParams(const Action & /* action */) override {
        return {};
    }

    virtual ComponentStatus enqueueContent(const EncodingParameters & /* params */,
                                           const Action & /* action */,
                                           const std::vector<uint8_t> & /* content */) override {
        return {};
    }

    virtual ComponentStatus dequeueContent(const Action & /* action */) override {
        return {};
    }

    virtual ComponentStatus doAction(const std::vector<RaceHandle> & /* handles */,
                                     const Action & /* action */) override {
        return {};
    }
};

ITransportComponent *createTransport(const std::string & /* transport */, ITransportSdk *sdk,
                                     const std::string & /* roleName */,
                                     const PluginConfig & /* pluginConfig */) {
    return new DecomposedTestTransport(sdk);
}

void destroyTransport(ITransportComponent *component) {
    delete component;
}
