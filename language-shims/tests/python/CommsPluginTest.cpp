
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

#ifndef PLUGIN_PATH
#pragma GCC error "Need plugin path from cmake"
#endif

// System
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <PluginContainer.h>
#include <PluginDef.h>
#include <race/common/PluginResponse.h>
#include <PythonLoaderWrapper.h>

// Test Helpers
#include <MockSdkWrapper.h>
#include <MockCore.h>

using ::testing::_;
using ::testing::Return;

using namespace Raceboat;

class CommsPluginTest : public ::testing::Test {
public:
    CommsPluginTest() {
        PluginDef pluginDef;
        pluginDef.filePath = "stubs";
        pluginDef.fileType = RaceEnums::PluginFileType::PFT_PYTHON;
        pluginDef.pythonModule = "CommsStub.CommsStub";
        pluginDef.pythonClass = "PluginCommsTwoSixPy";

        container.id = "PluginCommsTwoSixPy";
        auto sdkWrapper = std::make_unique<MockSdkWrapper>(container, mockCore);
        mockSdkWrapper = sdkWrapper.get();
        container.sdk = std::move(sdkWrapper);
        container.plugin = std::make_unique<PythonLoaderWrapper>(container, mockCore, pluginDef);

        plugin = container.plugin.get();
    }

public:
    PluginContainer container;
    MockCore mockCore;
    MockSdkWrapper* mockSdkWrapper;
    PluginWrapper* plugin;
};

TEST_F(CommsPluginTest, sdkFunctions) {
    RawData entropy = {0x01, 0x02};
    EXPECT_CALL(*mockSdkWrapper, getEntropy(2)).WillOnce(Return(entropy));
    EXPECT_CALL(*mockSdkWrapper, getActivePersona()).WillOnce(Return("expected-persona"));

    SdkResponse response;
    response.status = SDK_OK;
    response.handle = 0x1122334455667788;
    response.queueUtilization = 0.15;

    EXPECT_CALL(*mockSdkWrapper, requestPluginUserInput("expected-user-input-key",
                                                "expected-user-input-prompt", true))
        .WillOnce(Return(response));
    EXPECT_CALL(*mockSdkWrapper, requestCommonUserInput("expected-user-input-key"))
        .WillOnce(Return(response));

    EXPECT_CALL(*mockSdkWrapper, displayInfoToUser("expected-message", RaceEnums::UD_TOAST))
        .WillOnce(Return(response));
    EXPECT_CALL(*mockSdkWrapper, displayBootstrapInfoToUser("expected-message", RaceEnums::UD_QR_CODE,
                                                    RaceEnums::BS_COMPLETE))
        .WillOnce(Return(response));

    EXPECT_CALL(*mockSdkWrapper, onPackageStatusChanged(0x8877665544332211, PACKAGE_SENT, 1))
        .WillOnce(Return(response));
    EXPECT_CALL(*mockSdkWrapper, onConnectionStatusChanged(0x12345678, "expected-conn-id",
                                                   CONNECTION_CLOSED, _, 2))
        .WillOnce(Return(response));

    EXPECT_CALL(*mockSdkWrapper,
                onLinkStatusChanged(0x12345678, "expected-link-id", LINK_DESTROYED, _, 2))
        .WillOnce(Return(response));
    EXPECT_CALL(*mockSdkWrapper, onChannelStatusChanged(0x12345678, "expected-channel-gid",
                                                CHANNEL_AVAILABLE, _, 3))
        .WillOnce(::testing::Invoke(
            [response](auto, const auto &, auto, const auto &props, auto) {
                EXPECT_EQ("expected-channel-gid", props.channelGid);
                EXPECT_EQ(42, props.maxSendsPerInterval);
                EXPECT_EQ(3600, props.secondsPerInterval);
                EXPECT_EQ(8675309, props.intervalEndTime);
                EXPECT_EQ(7, props.sendsRemainingInInterval);
                return response;
            }));

    EXPECT_CALL(*mockSdkWrapper, updateLinkProperties("expected-link-id", _, 4))
        .WillOnce(Return(response));
    EXPECT_CALL(*mockSdkWrapper, generateConnectionId("expected-link-id"))
        .WillOnce(Return("expected-conn-id"));
    EXPECT_CALL(*mockSdkWrapper, generateLinkId("expected-channel-gid"))
        .WillOnce(Return("expected-channel-gid/expected-link-id"));

    EncPkg pkg(0x0011223344556677, 0x2211331144115511, {0x08, 0x67, 0x53, 0x09});
    std::vector<std::string> connIds = {"expected-conn-id-1", "expected-conn-id-2"};
    EXPECT_CALL(*mockSdkWrapper, receiveEncPkg(pkg, connIds, 5)).WillOnce(Return(response));

    PluginConfig pluginConfig;
    pluginConfig.etcDirectory = "/expected/etc/path";
    pluginConfig.loggingDirectory = "/expected/logging/path";
    pluginConfig.auxDataDirectory = "/expected/auxData/path";
    pluginConfig.tmpDirectory = "/expected/tmp/path";
    ASSERT_EQ(PLUGIN_OK, plugin->init(pluginConfig));
}

// // TODO test plugin functions
