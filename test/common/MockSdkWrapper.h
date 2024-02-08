
//
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

#include "SdkWrapper.h"
#include "gmock/gmock.h"
#include "race_printers.h"

namespace Raceboat {

class MockSdkWrapper : public SdkWrapper {
public:
    MockSdkWrapper(PluginContainer &container, Core &core): SdkWrapper(container, core) {}
    MOCK_METHOD(RawData, getEntropy, (std::uint32_t numBytes), (override));
    MOCK_METHOD(std::string, getActivePersona, (), (override));
    MOCK_METHOD(ChannelProperties, getChannelProperties, (std::string channelGid), (override));
    MOCK_METHOD(std::vector<ChannelProperties>, getAllChannelProperties, (), (override));
    MOCK_METHOD(SdkResponse, asyncError, (RaceHandle handle, PluginResponse status), (override));
    MOCK_METHOD(SdkResponse, makeDir, (const std::string &directoryPath), (override));
    MOCK_METHOD(SdkResponse, removeDir, (const std::string &directoryPath), (override));
    MOCK_METHOD(std::vector<std::string>, listDir, (const std::string &directoryPath), (override));
    MOCK_METHOD(std::vector<std::uint8_t>, readFile, (const std::string &filepath), (override));
    MOCK_METHOD(SdkResponse, appendFile,
                (const std::string &filepath, const std::vector<std::uint8_t> &data), (override));
    MOCK_METHOD(SdkResponse, writeFile,
                (const std::string &filepath, const std::vector<std::uint8_t> &data), (override));

    MOCK_METHOD(SdkResponse, onPackageStatusChanged,
                (RaceHandle handle, PackageStatus status, int32_t timeout), (override));
    MOCK_METHOD(SdkResponse, onConnectionStatusChanged,
                (RaceHandle handle, ConnectionID connId, ConnectionStatus status,
                 LinkProperties properties, int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, onChannelStatusChanged,
                (RaceHandle handle, std::string channelGid, ChannelStatus status,
                 ChannelProperties properties, int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, onLinkStatusChanged,
                (RaceHandle handle, LinkID linkId, LinkStatus status, LinkProperties properties,
                 int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, updateLinkProperties,
                (LinkID linkId, LinkProperties properties, int32_t timeout), (override));
    MOCK_METHOD(ConnectionID, generateConnectionId, (LinkID linkId), (override));
    MOCK_METHOD(LinkID, generateLinkId, (std::string channelGid), (override));
    MOCK_METHOD(SdkResponse, receiveEncPkg,
                (const EncPkg &pkg, const std::vector<ConnectionID> &connIDs, int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, requestPluginUserInput,
                (const std::string &key, const std::string &prompt, bool cache), (override));
    MOCK_METHOD(SdkResponse, requestCommonUserInput, (const std::string &key), (override));
    MOCK_METHOD(SdkResponse, displayInfoToUser,
                (const std::string &data, RaceEnums::UserDisplayType displayType), (override));
    MOCK_METHOD(SdkResponse, displayBootstrapInfoToUser,
                (const std::string &data, RaceEnums::UserDisplayType displayType,
                 RaceEnums::BootstrapActionType actionType),
                (override));
    MOCK_METHOD(SdkResponse, unblockQueue, (ConnectionID connId), (override));
};
}  // namespace Raceboat
