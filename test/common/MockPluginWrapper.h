
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

#include "PluginWrapper.h"
#include "race/common/EncPkg.h"
#include "gmock/gmock.h"

namespace RaceLib {
class MockPluginWrapper : public PluginWrapper {
public:
    MockPluginWrapper(PluginContainer &container) : PluginWrapper(container) {}
    virtual ~MockPluginWrapper() {}
    MOCK_METHOD(void, stopHandler, (), (override));
    MOCK_METHOD(void, waitForCallbacks, (), (override));
    MOCK_METHOD(bool, init, (const PluginConfig &pluginConfig), (override));
    MOCK_METHOD(bool, shutdown, (), (override));
    MOCK_METHOD(bool, shutdown, (std::int32_t timeoutInSeconds), (override));
    MOCK_METHOD(SdkResponse, sendPackage,
                (RaceHandle handle, const ConnectionID &connectionId, const EncPkg &pkg,
                 int32_t timeout, uint64_t batchId),
                (override));
    MOCK_METHOD(SdkResponse, openConnection,
                (RaceHandle handle, LinkType linkType, const LinkID &linkId,
                 const std::string &linkHints, int32_t priority, int32_t sendTimeout,
                 int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, closeConnection,
                (RaceHandle handle, const ConnectionID &connectionId, int32_t timeout), (override));
    MOCK_METHOD(SdkResponse, createLink,
                (RaceHandle handle, const std::string &channelGid, std::int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, createBootstrapLink,
                (RaceHandle handle, const std::string &channelGid, const std::string &passphrase,
                 std::int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, loadLinkAddress,
                (RaceHandle handle, const std::string &channelGid, const std::string &linkAddress,
                 std::int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, loadLinkAddresses,
                (RaceHandle handle, const std::string &channelGid,
                 std::vector<std::string> linkAddresses, std::int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, createLinkFromAddress,
                (RaceHandle handle, const std::string &channelGid, const std::string &linkAddress,
                 std::int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, destroyLink,
                (RaceHandle handle, const LinkID &linkId, std::int32_t timeout), (override));
    MOCK_METHOD(SdkResponse, deactivateChannel,
                (RaceHandle handle, const std::string &channelGid, std::int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, activateChannel,
                (RaceHandle handle, const std::string &channelGid, const std::string &roleName,
                 std::int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, serveFiles, (LinkID linkId, std::string path, int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, flushChannel,
                (RaceHandle handle, std::string channelGid, uint64_t batchId, int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, onUserInputReceived,
                (RaceHandle handle, bool answered, const std::string &response, int32_t timeout),
                (override));
    MOCK_METHOD(SdkResponse, onUserAcknowledgementReceived, (RaceHandle handle, int32_t timeout),
                (override));
    MOCK_METHOD(void, onConnectionStatusChanged, (const ConnectionID &connId, ConnectionStatus status),
                (override));
    MOCK_METHOD(SdkResponse, unblockQueue, (const ConnectionID& connId),
                (override));
};

}  // namespace RaceLib
