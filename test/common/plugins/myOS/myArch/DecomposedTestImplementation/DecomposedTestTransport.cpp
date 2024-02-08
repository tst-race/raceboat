
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

#include <nlohmann/json.hpp>
#include <mutex>

using nlohmann::json;

#define TRACE_METHOD(...) TRACE_METHOD_BASE(DecomposedTestTransport, ##__VA_ARGS__)

// using a singleton for inter-transport communication because this is a test and there's no good
// way around it
static std::mutex mutex;
static std::unordered_map<int, ITransportSdk *> &sdkMap() {
    static std::unordered_map<int, ITransportSdk *> map;
    return map;
}

LinkProperties createDefaultLinkProperties(const ChannelProperties &channelProperties) {
    LinkProperties linkProperties;

    linkProperties.linkType = LT_BIDI;
    linkProperties.transmissionType = channelProperties.transmissionType;
    linkProperties.connectionType = channelProperties.connectionType;
    linkProperties.sendType = channelProperties.sendType;
    linkProperties.reliable = channelProperties.reliable;
    linkProperties.isFlushable = channelProperties.isFlushable;
    linkProperties.duration_s = channelProperties.duration_s;
    linkProperties.period_s = channelProperties.period_s;
    linkProperties.mtu = channelProperties.mtu;

    LinkPropertySet worstLinkPropertySet;
    worstLinkPropertySet.bandwidth_bps = 277200;
    worstLinkPropertySet.latency_ms = 3190;
    worstLinkPropertySet.loss = 0.1;
    linkProperties.worst.send = worstLinkPropertySet;
    linkProperties.worst.receive = worstLinkPropertySet;

    linkProperties.expected = channelProperties.creatorExpected;

    LinkPropertySet bestLinkPropertySet;
    bestLinkPropertySet.bandwidth_bps = 338800;
    bestLinkPropertySet.latency_ms = 2610;
    bestLinkPropertySet.loss = 0.1;
    linkProperties.best.send = bestLinkPropertySet;
    linkProperties.best.receive = bestLinkPropertySet;

    linkProperties.supported_hints = channelProperties.supported_hints;
    linkProperties.channelGid = channelProperties.channelGid;

    return linkProperties;
}

class DecomposedTestTransport : public ITransportComponent {
public:
    explicit DecomposedTestTransport(ITransportSdk *sdk) :
        sdk(sdk),
        id(instances++),
        channelProperties(sdk->getChannelProperties()),
        linkProperties(createDefaultLinkProperties(channelProperties)) {
        // store sdk for testing purposes
        {
            RaceLog::logInfo("DecomposedTestTransport", "Setting sdk for ID: " + std::to_string(id), "");
            std::lock_guard lock (mutex);
            sdkMap()[id] = sdk;
        }

        // no user input required for the stub. If user input is required, this should be done in
        // onUserInputReceived after receiving responses for all requests
        sdk->updateState(COMPONENT_STATE_STARTED);
    }

    ~DecomposedTestTransport() {
            RaceLog::logInfo("DecomposedTestTransport", "Removing sdk for ID: " + std::to_string(id), "");
            std::lock_guard lock (mutex);
            sdkMap()[id] = nullptr;
    }

    virtual ComponentStatus onUserInputReceived(RaceHandle /* handle */, bool /* answered */,
                                                const std::string & /* response */) override {
        TRACE_METHOD();
        return COMPONENT_OK;
    }

    virtual TransportProperties getTransportProperties() override {
        TRACE_METHOD();
        return {
            // supportedActions
            {
                // action name, type of content associated with action
                {"post", {"*/*"}},
            },
        };
    }

    virtual LinkProperties getLinkProperties(const LinkID &linkId) override {
        TRACE_METHOD();
        LinkProperties props;
        props.linkAddress = json({{"id", id}, {"linkId", linkId}}).dump();
        return props;
    }

    virtual ComponentStatus createLink(RaceHandle handle, const LinkID &linkId) override {
        TRACE_METHOD();
        sdk->onLinkStatusChanged(handle, linkId, LINK_CREATED, {});
        return COMPONENT_OK;
    }

    virtual ComponentStatus loadLinkAddress(RaceHandle handle, const LinkID &linkId,
                                            const std::string &linkAddress) override {
        TRACE_METHOD();
        try {
            json address = json::parse(linkAddress);
            int targetId = address.at("id");
            LinkID recvLinkId = address.at("linkId");

            linkTargetLinkMap[linkId] = recvLinkId;
            {
                RaceLog::logInfo("DecomposedTestTransport", "Getting sdk for ID: " + std::to_string(targetId), "");
                RaceLog::logInfo("DecomposedTestTransport", "Storing sdk for Link: " + linkId, "");
                RaceLog::logInfo("DecomposedTestTransport", "Storing linkId " + recvLinkId + " for Link: " + linkId, "");
                std::lock_guard lock (mutex);
                linkTargetSdkMap[linkId] = sdkMap().at(targetId);
            }
            sdk->onLinkStatusChanged(handle, linkId, LINK_LOADED, {});
            return COMPONENT_OK;
        } catch (std::exception &e) {
            sdk->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
            return COMPONENT_ERROR;
        }
    }

    virtual ComponentStatus loadLinkAddresses(
        RaceHandle handle, const LinkID &linkId,
        const std::vector<std::string> & /* linkAddress */) override {
        TRACE_METHOD();
        sdk->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
        return COMPONENT_ERROR;
    }

    virtual ComponentStatus createLinkFromAddress(RaceHandle handle, const LinkID &linkId,
                                                  const std::string &linkAddress) override {
        TRACE_METHOD();
        if (id == std::stoi(linkAddress)) {
            sdk->onLinkStatusChanged(handle, linkId, LINK_CREATED, {});
            return COMPONENT_OK;
        } else {
            return COMPONENT_ERROR;
        }
    }

    virtual ComponentStatus destroyLink(RaceHandle handle, const LinkID &linkId) override {
        TRACE_METHOD();
        linkTargetLinkMap.erase(linkId);
        linkTargetSdkMap.erase(linkId);
        sdk->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
        return COMPONENT_OK;
    }

    virtual std::vector<EncodingParameters> getActionParams(const Action &action) override {
        // TRACE_METHOD();
        return {{action.json, "*/*", true, {}}};
    }

    virtual ComponentStatus enqueueContent(const EncodingParameters & /* params */,
                                           const Action &action,
                                           const std::vector<uint8_t> &content) override {
        TRACE_METHOD();
        contentQueue[action.actionId] = content;
        return COMPONENT_OK;
    }

    virtual ComponentStatus dequeueContent(const Action &action) override {
        TRACE_METHOD();
        contentQueue.erase(action.actionId);
        return COMPONENT_OK;
    }

    virtual ComponentStatus doAction(const std::vector<RaceHandle> &handles,
                                     const Action &action) override {
        TRACE_METHOD();
        std::vector<uint8_t> message = std::move(contentQueue[action.actionId]);
        LinkID linkId = action.json;
        ITransportSdk *targetSdk = linkTargetSdkMap[linkId];
        LinkID recvLinkId = linkTargetLinkMap[linkId];

        for (auto &handle : handles) {
            sdk->onPackageStatusChanged(handle, PACKAGE_SENT);
        }
        RaceLog::logInfo("DecomposedTestTransport", "Using sdk for ID: " + linkId, "");
        if (targetSdk && !message.empty()) {
            RaceLog::logInfo("DecomposedTestTransport", "Sending message: " + linkId, "");
            targetSdk->onReceive(recvLinkId, {"", "*/*", false, {}}, message);
        }
        return COMPONENT_OK;
    }

private:
    static int instances;
    ITransportSdk *sdk;
    int id;
    ChannelProperties channelProperties;
    LinkProperties linkProperties;

    // map link to the destination sdk
    std::unordered_map<LinkID, LinkID> linkTargetLinkMap;
    std::unordered_map<LinkID, ITransportSdk *> linkTargetSdkMap;

    // content queue
    std::unordered_map<uint64_t, std::vector<uint8_t>> contentQueue;
};

int DecomposedTestTransport::instances = 0;

ITransportComponent *createTransport(const std::string & /* transport */, ITransportSdk *sdk,
                                     const std::string & /* roleName */,
                                     const PluginConfig & /* pluginConfig */) {
    return new DecomposedTestTransport(sdk);
}

void destroyTransport(ITransportComponent *component) {
    delete component;
}
