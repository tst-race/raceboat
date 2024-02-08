
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

#include "Core.h"
#include "MockChannelManager.h"
#include "MockFileSystem.h"
#include "MockPluginWrapper.h"
#include "MockApiManager.h"
#include "MockUserInput.h"
#include "race/common/EncPkg.h"
#include "gmock/gmock.h"

namespace RaceLib {

class MockCore : public Core {
public:
    MockCore() : MockCore("") {}
    MockCore(std::string race_dir, ChannelParamStore /* params */) : MockCore(race_dir) {}
    MockCore(std::string race_dir, Config mockConfig = Config()) :
        Core(),
        mockApiManager(*this),
        mockFS(MockFileSystem(race_dir)),
        mockConfig(mockConfig),
        mockChannelManager(*this) {
        container = std::make_unique<PluginContainer>();
        container->id = "mock-plugin";
        auto mockPlugin = std::make_unique<MockPluginWrapper>(*container);
        plugin = mockPlugin.get();
        container->plugin = std::move(mockPlugin);
        container->sdk = std::make_unique<SdkWrapper>(*container, *this);
    }

    FileSystem &getFS() override {
        return mockFS;
    }
    const UserInput &getUserInput() override {
        return mockUserInput;
    }
    const Config &getConfig() override {
        return mockConfig;
    }

    PluginContainer *getChannel(std::string) override {
        return container.get();
    }

    ChannelManager &getChannelManager() override {
        return mockChannelManager;
    }

    RaceHandle generateHandle() override {
        return handle++;
    }

    MOCK_METHOD(std::vector<uint8_t>, getEntropy, (std::uint32_t numBytes), (override));
    MOCK_METHOD(std::string, getActivePersona, (PluginContainer & plugin), (override));
    MOCK_METHOD(SdkResponse, asyncError,
                (PluginContainer & plugin, RaceHandle handle, PluginResponse status), (override));
    MOCK_METHOD(SdkResponse, onPackageStatusChanged,
                (PluginContainer & plugin, RaceHandle handle, PackageStatus status), (override));
    MOCK_METHOD(SdkResponse, onConnectionStatusChanged,
                (PluginContainer & plugin, RaceHandle handle, const ConnectionID &connId,
                 ConnectionStatus status, const LinkProperties &properties),
                (override));
    MOCK_METHOD(SdkResponse, onLinkStatusChanged,
                (PluginContainer & plugin, RaceHandle handle, const LinkID &linkId,
                 LinkStatus status, const LinkProperties &properties),
                (override));
    MOCK_METHOD(SdkResponse, onChannelStatusChanged,
                (PluginContainer & plugin, RaceHandle handle, const ChannelId &channelGid,
                 ChannelStatus status, const ChannelProperties &properties),
                (override));
    MOCK_METHOD(SdkResponse, updateLinkProperties,
                (PluginContainer & plugin, const LinkID &linkId, const LinkProperties &properties),
                (override));
    MOCK_METHOD(SdkResponse, receiveEncPkg,
                (PluginContainer & plugin, const EncPkg &pkg,
                 const std::vector<ConnectionID> &connIDs),
                (override));
    MOCK_METHOD(ConnectionID, generateConnectionId,
                (PluginContainer & plugin, const LinkID &linkId), (override));
    MOCK_METHOD(LinkID, generateLinkId, (PluginContainer & plugin, const ChannelId &channelGid),
                (override));

    ApiManager &getApiManager() override {
        return mockApiManager;
    }

public:
    MockApiManager mockApiManager;
    MockFileSystem mockFS;
    MockUserInput mockUserInput;
    Config mockConfig;
    MockChannelManager mockChannelManager;
    std::unique_ptr<PluginContainer> container;
    MockPluginWrapper *plugin;

    RaceHandle handle = 1;
};

}  // namespace RaceLib