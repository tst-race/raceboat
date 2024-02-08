
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

#include "SdkWrapper.h"

#include "Core.h"
#include "PluginWrapper.h"
#include "helper.h"

namespace RaceLib {

SdkWrapper::SdkWrapper(PluginContainer &container, Core &core) : core(core), container(container) {}

std::vector<uint8_t> SdkWrapper::getEntropy(std::uint32_t numBytes) {
    auto id = container.id;
    TRACE_METHOD(id, numBytes);
    return core.getEntropy(numBytes);
}

std::string SdkWrapper::getActivePersona() {
    auto id = container.id;
    TRACE_METHOD(id);
    return core.getActivePersona(container);
}

SdkResponse SdkWrapper::asyncError(RaceHandle handle, PluginResponse status) {
    auto id = container.id;
    TRACE_METHOD(id, handle, status);
    return core.asyncError(container, handle, status);
}

ChannelProperties SdkWrapper::getChannelProperties(std::string channelGid) {
    auto id = container.id;
    TRACE_METHOD(id, channelGid);
    ChannelProperties response = core.getChannelManager().getChannelProperties(channelGid);
    return response;
}

std::vector<ChannelProperties> SdkWrapper::getAllChannelProperties() {
    auto id = container.id;
    TRACE_METHOD(id);
    std::vector<ChannelProperties> response = core.getChannelManager().getAllChannelProperties();
    return response;
}

SdkResponse SdkWrapper::makeDir(const std::string &directoryPath) {
    auto id = container.id;
    TRACE_METHOD(id, directoryPath);
    if (!core.getFS().makeDir(directoryPath, id)) {
        return SDK_INVALID_ARGUMENT;
    }
    return SDK_OK;
}

SdkResponse SdkWrapper::removeDir(const std::string &directoryPath) {
    auto id = container.id;
    TRACE_METHOD(id, directoryPath);
    if (!core.getFS().removeDir(directoryPath, id)) {
        return SDK_INVALID_ARGUMENT;
    }
    return SDK_OK;
}

std::vector<std::string> SdkWrapper::listDir(const std::string &directoryPath) {
    auto id = container.id;
    TRACE_METHOD(id, directoryPath);
    std::vector<std::string> contents = core.getFS().listDir(directoryPath, id);
    return contents;
}

std::vector<std::uint8_t> SdkWrapper::readFile(const std::string &filename) {
    auto id = container.id;
    TRACE_METHOD(id, filename);
    std::vector<std::uint8_t> data = core.getFS().readFile(filename, id);
    return data;
}

SdkResponse SdkWrapper::appendFile(const std::string &filename,
                                   const std::vector<std::uint8_t> &data) {
    auto id = container.id;
    TRACE_METHOD(id, filename);
    if (!core.getFS().appendFile(filename, id, data)) {
        return SDK_INVALID_ARGUMENT;
    }
    return SDK_OK;
}

SdkResponse SdkWrapper::writeFile(const std::string &filename,
                                  const std::vector<std::uint8_t> &data) {
    auto id = container.id;
    TRACE_METHOD(id, filename);
    if (!core.getFS().writeFile(filename, id, data)) {
        return SDK_INVALID_ARGUMENT;
    }
    return SDK_OK;
}

SdkResponse SdkWrapper::requestPluginUserInput(const std::string &key, const std::string &prompt,
                                               bool cache) {
    auto id = container.id;
    TRACE_METHOD(id, key, prompt, cache);

    auto userInput = core.getUserInput().getPluginUserInput(id, key);
    bool answered = userInput.has_value();
    std::string response = answered ? *userInput : "";

    RaceHandle handle = core.generateHandle();
    SdkResponse resp = container.plugin->onUserInputReceived(handle, answered, response, 0);
    if (resp.status != SDK_OK) {
        container.plugin->onUserInputReceived(handle, false, response, 0);
    }

    SdkResponse ret = SDK_OK;
    ret.handle = handle;
    return ret;
}

SdkResponse SdkWrapper::requestCommonUserInput(const std::string &key) {
    auto id = container.id;
    TRACE_METHOD(id, key);

    auto userInput = core.getUserInput().getCommonUserInput(key);
    bool answered = userInput.has_value();
    std::string response = answered ? *userInput : "";

    RaceHandle handle = core.generateHandle();
    SdkResponse resp = container.plugin->onUserInputReceived(handle, answered, response, 0);
    if (resp.status != SDK_OK) {
        container.plugin->onUserInputReceived(handle, false, response, 0);
    }

    SdkResponse ret = SDK_OK;
    ret.handle = handle;
    return ret;
    return SDK_OK;
}

SdkResponse SdkWrapper::displayInfoToUser(const std::string &, RaceEnums::UserDisplayType) {
    // This is unsupported in the standalone library
    return SDK_INVALID;
}

SdkResponse SdkWrapper::displayBootstrapInfoToUser(const std::string &, RaceEnums::UserDisplayType,
                                                   RaceEnums::BootstrapActionType) {
    // This is unsupported in the standalone library
    return SDK_INVALID;
}

SdkResponse SdkWrapper::unblockQueue(ConnectionID connId) {
    auto id = container.id;
    TRACE_METHOD(id, connId);
    return container.plugin->unblockQueue(connId);
}

SdkResponse SdkWrapper::onPackageStatusChanged(RaceHandle handle, PackageStatus status, int32_t) {
    auto id = container.id;
    TRACE_METHOD(id, handle, status);
    return core.onPackageStatusChanged(container, handle, status);
}

SdkResponse SdkWrapper::onConnectionStatusChanged(RaceHandle handle, ConnectionID connId,
                                                  ConnectionStatus status,
                                                  LinkProperties properties, int32_t) {
    auto id = container.id;
    TRACE_METHOD(id, handle, connId, status);
    container.plugin->onConnectionStatusChanged(connId, status);
    return core.onConnectionStatusChanged(container, handle, connId, status, properties);
}

SdkResponse SdkWrapper::onLinkStatusChanged(RaceHandle handle, LinkID linkId, LinkStatus status,
                                            LinkProperties properties, int32_t) {
    auto id = container.id;
    TRACE_METHOD(id, handle, linkId, status);
    return core.onLinkStatusChanged(container, handle, linkId, status, properties);
}

SdkResponse SdkWrapper::onChannelStatusChanged(RaceHandle handle, std::string channelGid,
                                               ChannelStatus status, ChannelProperties properties,
                                               int32_t) {
    auto id = container.id;
    TRACE_METHOD(id, handle, channelGid, status);
    return core.onChannelStatusChanged(container, handle, channelGid, status, properties);
}

SdkResponse SdkWrapper::updateLinkProperties(LinkID linkId, LinkProperties properties, int32_t) {
    auto id = container.id;
    TRACE_METHOD(id, linkId);
    return core.updateLinkProperties(container, linkId, properties);
}

SdkResponse SdkWrapper::receiveEncPkg(const EncPkg &pkg, const std::vector<ConnectionID> &connIDs,
                                      int32_t) {
    auto id = container.id;
    TRACE_METHOD(id);
    return core.receiveEncPkg(container, pkg, connIDs);
}

ConnectionID SdkWrapper::generateConnectionId(LinkID linkId) {
    auto id = container.id;
    TRACE_METHOD(id, linkId);
    ConnectionID response = core.generateConnectionId(container, linkId);
    helper::logDebug(logPrefix + "returned " + response);
    return response;
}

LinkID SdkWrapper::generateLinkId(std::string channelGid) {
    auto id = container.id;
    TRACE_METHOD(id, channelGid);
    LinkID response = core.generateLinkId(container, channelGid);
    helper::logDebug(logPrefix + "returned " + response);
    return response;
}

}  // namespace RaceLib
