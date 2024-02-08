
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

#include "../common/mocks/MockRacePlugin.h"
#include "Core.h"
#include "PluginWrapper.h"
#include "SdkWrapper.h"
#include "gtest/gtest.h"

using ::testing::Invoke;
using ::testing::Return;
using ::testing::Unused;

using namespace RaceLib;
using namespace std::chrono_literals;

TEST(PluginWrapperTest, test_constructor) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");
}

TEST(PluginWrapperTest, test_getters) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    EXPECT_EQ(wrapper.getId(), "MockPlugin");
    EXPECT_EQ(wrapper.getDescription(), "Mock Plugin Testing");
}

TEST(PluginWrapperTest, init) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    PluginConfig pluginConfig;
    pluginConfig.etcDirectory = "bloop";
    pluginConfig.loggingDirectory = "foo";
    pluginConfig.auxDataDirectory = "bar";

    EXPECT_CALL(*mockPlugin, init(pluginConfig)).Times(1).WillOnce(Return(PLUGIN_OK));
    wrapper.init(pluginConfig);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, shutdown) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    EXPECT_CALL(*mockPlugin, shutdown()).WillOnce(Return(PLUGIN_OK));
    EXPECT_EQ(wrapper.shutdown(), true);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, sendPackage) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    LinkID linkId = "LinkID";
    std::string linkHints = "{}";
    ConnectionID connId = "MockPlugin/ConnectionID";
    const std::string cipherText = "my cipher text";
    EncPkg sentPkg({cipherText.begin(), cipherText.end()});
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin,
                sendPackage(handle, connId, sentPkg, std::numeric_limits<double>::infinity(), 0))
        .WillOnce(Return(PLUGIN_OK));

    wrapper.onConnectionStatusChanged(connId, CONNECTION_OPEN);
    wrapper.sendPackage(handle, connId, sentPkg, 0, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, openConnection) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    LinkType linkType = LT_SEND;
    LinkID linkId = "LinkID";
    std::string linkHints = "{}";
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, openConnection(handle, linkType, linkId, linkHints, 0))
        .WillOnce(Return(PLUGIN_OK));

    wrapper.openConnection(handle, linkType, linkId, linkHints, 0, 0, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, closeConnection) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    LinkID linkId = "LinkID";
    std::string linkHints = "{}";
    ConnectionID connId = "MockPlugin/ConnectionID";
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, closeConnection(handle, connId)).Times(1);

    wrapper.closeConnection(handle, connId, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, deactivateChannel) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    std::string channelGid = "channel1";
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, deactivateChannel(handle, channelGid)).Times(1);

    wrapper.deactivateChannel(handle, channelGid, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, activateChannel) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    std::string channelGid = "channel1";
    std::string roleName = "roleName";
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, activateChannel(handle, channelGid, roleName)).Times(1);

    wrapper.activateChannel(handle, channelGid, roleName, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, destroyLink) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    LinkID linkId = "LinkId";
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, destroyLink(handle, linkId)).Times(1);

    wrapper.destroyLink(handle, linkId, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, createLink) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    std::string channelGid = "channel1";
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, createLink(handle, channelGid)).Times(1);

    wrapper.createLink(handle, channelGid, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, loadLinkAddress) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    std::string channelGid = "channel1";
    std::string linkAddress = "{}";
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, loadLinkAddress(handle, channelGid, linkAddress)).Times(1);

    wrapper.loadLinkAddress(handle, channelGid, linkAddress, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, loadLinkAddressses) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    std::string channelGid = "channel1";
    std::vector<std::string> linkAddresses = {"{}", "{}"};
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, loadLinkAddresses(handle, channelGid, linkAddresses)).Times(1);

    wrapper.loadLinkAddresses(handle, channelGid, linkAddresses, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, createLinkFromAddress) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    std::string channelGid = "channel1";
    std::string linkAddress = "{}";
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, createLinkFromAddress(handle, channelGid, linkAddress)).Times(1);

    wrapper.createLinkFromAddress(handle, channelGid, linkAddress, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, flushChannel) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    std::string channelGid = "channel1";
    uint64_t batchId = 2;
    RaceHandle handle = 42;

    EXPECT_CALL(*mockPlugin, flushChannel(handle, channelGid, batchId)).Times(1);

    wrapper.flushChannel(handle, channelGid, batchId, 0);
    wrapper.stopHandler();
}

TEST(PluginWrapperTest, onUserInputReceived) {
    auto mockPlugin = std::make_shared<MockRacePlugin>();
    Core core;
    PluginContainer container;
    container.id = "MockPlugin";
    container.sdk = std::make_unique<SdkWrapper>(container, core);
    PluginWrapper wrapper(container, mockPlugin, "Mock Plugin Testing");

    EXPECT_CALL(*mockPlugin, onUserInputReceived(0x11223344l, true, "expected-response")).Times(1);

    wrapper.onUserInputReceived(0x11223344l, true, "expected-response", 0);
    wrapper.stopHandler();
}
