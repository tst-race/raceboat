
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

#include "PluginContainer.h"
#include "race/unified/IRaceSdkComms.h"

namespace Raceboat {
class Core;

/**
 * @brief The SdkWrapper wraps calls into the library core and provides the id of the calling
 * plugin. See the documentation for IRaceSdkCommon and IRaceSdkComms for details about each of the
 * calls.
 *
 */
class SdkWrapper : public IRaceSdkComms {
public:
    SdkWrapper(PluginContainer &container, Core &core);

    // IRaceSdkCommon
    virtual std::vector<uint8_t> getEntropy(std::uint32_t numBytes) override;
    virtual std::string getActivePersona() override;
    virtual SdkResponse asyncError(RaceHandle handle, PluginResponse status) override;
    virtual ChannelProperties getChannelProperties(std::string channelGid) override;
    virtual std::vector<ChannelProperties> getAllChannelProperties() override;

    virtual SdkResponse makeDir(const std::string &directoryPath) override;
    virtual SdkResponse removeDir(const std::string &directoryPath) override;
    virtual std::vector<std::string> listDir(const std::string &directoryPath) override;
    virtual std::vector<std::uint8_t> readFile(const std::string &filename) override;
    virtual SdkResponse appendFile(const std::string &filename,
                                   const std::vector<std::uint8_t> &data) override;
    virtual SdkResponse writeFile(const std::string &filename,
                                  const std::vector<std::uint8_t> &data) override;

    // IRaceSdkComms
    virtual SdkResponse onPackageStatusChanged(RaceHandle handle, PackageStatus status,
                                               int32_t timeout) override;
    virtual SdkResponse onConnectionStatusChanged(RaceHandle handle, ConnectionID connId,
                                                  ConnectionStatus status,
                                                  LinkProperties properties,
                                                  int32_t timeout) override;
    virtual SdkResponse onLinkStatusChanged(RaceHandle handle, LinkID linkId, LinkStatus status,
                                            LinkProperties properties, int32_t timeout) override;
    virtual SdkResponse onChannelStatusChanged(RaceHandle handle, std::string channelGid,
                                               ChannelStatus status, ChannelProperties properties,
                                               int32_t timeout) override;
    virtual SdkResponse updateLinkProperties(LinkID linkId, LinkProperties properties,
                                             int32_t timeout) override;
    virtual SdkResponse receiveEncPkg(const EncPkg &pkg, const std::vector<ConnectionID> &connIDs,
                                      int32_t timeout) override;
    virtual ConnectionID generateConnectionId(LinkID linkId) override;
    virtual LinkID generateLinkId(std::string channelGid) override;
    virtual SdkResponse requestPluginUserInput(const std::string &key, const std::string &prompt,
                                               bool cache) override;
    virtual SdkResponse requestCommonUserInput(const std::string &key) override;
    virtual SdkResponse displayInfoToUser(const std::string &data,
                                          RaceEnums::UserDisplayType displayType) override;
    virtual SdkResponse displayBootstrapInfoToUser(
        const std::string &data, RaceEnums::UserDisplayType displayType,
        RaceEnums::BootstrapActionType actionType) override;

    virtual SdkResponse unblockQueue(ConnectionID connId) override;

protected:
    Core &core;
    PluginContainer &container;
};

}  // namespace Raceboat
