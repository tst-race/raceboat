
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

#include "Core.h"

#include <atomic>
#include <random>

#include "helper.h"

namespace RaceLib {

Core::Core() : fs(""), userInput({}), apiManager(std::make_unique<ApiManager>(*this)) {}
Core::Core(std::string pluginPath, ChannelParamStore params) :
    fs(pluginPath), userInput(params), apiManager(std::make_unique<ApiManager>(*this)) {
    init();
}
Core::~Core() {
    shuttingDown = true;

    apiManager.reset();

    // unload the plugins
    pluginLoader.reset();
}

void Core::init() {
    if (!config.parsePluginManifests(getFS())) {
        std::string message =
            "Unable to parse any plugin manifests in path: " + getFS().pluginsInstallPath.string();
        helper::logError(message);
    }

    // plugin loader and channelManager require config to be populated before initializing
    pluginLoader = IPluginLoader::construct(*this);
    channelManager = std::make_unique<ChannelManager>(*this);
}

FileSystem &Core::getFS() {
    return fs;
}

const UserInput &Core::getUserInput() {
    return userInput;
}

const Config &Core::getConfig() {
    return config;
}

ChannelManager &Core::getChannelManager() {
    return *channelManager;
}

ApiManager &Core::getApiManager() {
    return *apiManager;
}

PluginContainer *Core::getChannel(std::string channelId) {
    return pluginLoader->getChannel(channelId);
}

std::vector<uint8_t> Core::getEntropy(std::uint32_t numBytes) {
    std::random_device rand_dev;
    unsigned int value;
    std::vector<uint8_t> randomness;
    randomness.reserve(numBytes);

    // Use bit shifts to fully utilize the entropy returned by random_device
    for (std::size_t index = 0u; index < numBytes; ++index) {
        if (index % sizeof(unsigned int) == 0) {
            value = rand_dev();
        }
        randomness.push_back(static_cast<std::uint8_t>(value));
        value >>= 8;
    }

    return randomness;
}

std::string Core::getActivePersona(PluginContainer & /* plugin */) {
    return "race-client-00001";
}

SdkResponse Core::asyncError(PluginContainer & /* plugin */, RaceHandle /* handle */,
                             PluginResponse /* status */) {
    // TODO: shutdown plugin on async Error
    return SDK_OK;
}

SdkResponse Core::onPackageStatusChanged(PluginContainer &plugin, RaceHandle handle,
                                         PackageStatus status) {
    if (shuttingDown) {
        return SDK_SHUTTING_DOWN;
    }

    return getApiManager().onPackageStatusChanged(plugin, handle, status);
}

SdkResponse Core::onConnectionStatusChanged(PluginContainer &plugin, RaceHandle handle,
                                            const ConnectionID &connId, ConnectionStatus status,
                                            const LinkProperties &properties) {
    if (shuttingDown) {
        return SDK_SHUTTING_DOWN;
    }

    return getApiManager().onConnectionStatusChanged(plugin, handle, connId, status, properties);
}

SdkResponse Core::onLinkStatusChanged(PluginContainer &plugin, RaceHandle handle,
                                      const LinkID &linkId, LinkStatus status,
                                      const LinkProperties &properties) {
    if (shuttingDown) {
        return SDK_SHUTTING_DOWN;
    }

    return getApiManager().onLinkStatusChanged(plugin, handle, linkId, status, properties);
}

SdkResponse Core::onChannelStatusChanged(PluginContainer &plugin, RaceHandle handle,
                                         const ChannelId &channelGid, ChannelStatus status,
                                         const ChannelProperties &properties) {
    if (shuttingDown) {
        return SDK_SHUTTING_DOWN;
    }

    getChannelManager().onChannelStatusChanged(handle, channelGid, status, properties);
    return getApiManager().onChannelStatusChanged(plugin, handle, channelGid, status, properties);
}

SdkResponse Core::updateLinkProperties(PluginContainer &, const LinkID &, const LinkProperties &) {
    return SDK_OK;
}

SdkResponse Core::receiveEncPkg(PluginContainer &plugin, const EncPkg &pkg,
                                const std::vector<ConnectionID> &connIDs) {
    if (shuttingDown) {
        return SDK_SHUTTING_DOWN;
    }

    return getApiManager().receiveEncPkg(plugin, pkg, connIDs);
}

ConnectionID Core::generateConnectionId(PluginContainer & /* plugin */, const LinkID &linkId) {
    static std::atomic<int> connectionCount = 0;
    return linkId + "/Connection_" + std::to_string(connectionCount++);
}

LinkID Core::generateLinkId(PluginContainer &plugin, const ChannelId &channelGid) {
    static std::atomic<int> linkCount = 0;
    return plugin.id + "/" + channelGid + "/LinkID_" + std::to_string(linkCount++);
}

RaceHandle Core::generateHandle() {
    static std::atomic<uint64_t> handleCount = 1;
    // return value in range [1, (UINT64_MAX / 2) + thread count]
    // 0 reserved for NULL_RACE_HANDLE
    static RaceHandle rollover = (UINT64_MAX / 2);
    // compare and increment are atomic, but the call sequence is not atomic
    handleCount.compare_exchange_strong(rollover, 1ull);
    return handleCount++;
}
}  // namespace RaceLib
