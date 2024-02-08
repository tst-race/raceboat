
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


#include "../../source/api-managers/ApiManager.h"
#include "../common/MockCore.h"
#include "../common/mocks/MockRacePlugin.h"

using namespace Raceboat;
using namespace testing;
using testing::_;

class TestableUnidirectionalRecvManager : public ApiManager {
public:
    using ApiManager::ApiManager;
    using ApiManager::impl;
};

class ApiManagerRecvTestFixture : public ::testing::Test {
public:
    ApiManagerRecvTestFixture() : params(), core(std::make_shared<MockCore>()), manager(*core) {
        recvOptions.recv_channel = "recvChannel";
        recvOptions.recv_address = "recvAddress";
        recvOptions.recv_role = "recvRole";
        recvOptions.timeout_ms = 0;

        chanId = "mockChanId";
        linkId = "mockLinkId";
        connId = "mockConnId";

        channelProps.bootstrap = false;
        channelProps.channelStatus = CHANNEL_UNDEF;
        channelProps.connectionType = CT_UNDEF;
        channelProps.creatorsPerLoader = 1;
        channelProps.currentRole.linkSide = LS_UNDEF;
        channelProps.duration_s = 0;
        channelProps.intervalEndTime = 0;
        channelProps.isFlushable = true;
        channelProps.linkDirection = LD_LOADER_TO_CREATOR;
        channelProps.loadersPerCreator = 1;

        // 0 and 1 are context handles created by the manager
        chanHandle = core->handle + 2;
        createLinkHandle = core->handle + 3;
        openConnHandle = core->handle + 4;
        pkgHandle = core->handle + 5;
        closeConnHandle = core->handle + 6;
        destroyLinkHandle = core->handle + 7;

        bytes.reserve(0x100);
        for (uint16_t byte = 0; byte < 0x100; ++byte) {
            bytes.push_back(byte);
        }

        linkProps.linkAddress = "recvAddress";
        linkProps.linkType = LT_RECV;

        getReceiverCallback = [this](ApiStatus _status, LinkAddress _address, RaceHandle _handle) {
            status = _status;
            address = _address;
            receiverHandle = _handle;
        };

        receiveCallback = [this](ApiStatus _status, std::vector<uint8_t> _bytes) {
            status2 = _status;
            bytes2 = _bytes;
        };

        closeCallback = [this](ApiStatus _status) { status3 = _status; };
    }

    void expectActivateChannel();
    void expectCreateLinkFromAddress();
    void expectCreateLink();
    void expectOpenConnection();
    void expectCloseConnection();
    void expectDestroyLink();

    void expectAll();
    void expectedStatus();

    void getReceiverCall(std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback);
    void onChannelStatusChangedCall();
    void onLinkStatusChangedCall();
    void onConnectionStatusChangedCall();
    void receiveCall(OpHandle handle,
                     std::function<void(ApiStatus, std::vector<uint8_t>)> callback);
    void closeCall(OpHandle handle, std::function<void(ApiStatus)> callback);
    void receiveEncPkgCall();
    void onConnectionStatusChangedClosedCall();
    void onLinkStatusChangedDestroyedCall();

    void openAll();
    void closeAll();

    ChannelParamStore params;
    std::shared_ptr<MockCore> core;
    TestableUnidirectionalRecvManager manager;

    LinkProperties linkProps;
    ChannelProperties channelProps;
    ReceiveOptions recvOptions;

    ChannelId chanId;
    LinkID linkId;
    ConnectionID connId;

    std::vector<uint8_t> bytes;

    RaceHandle chanHandle;
    RaceHandle createLinkHandle;
    RaceHandle openConnHandle;
    RaceHandle pkgHandle;
    RaceHandle closeConnHandle;
    RaceHandle destroyLinkHandle;

    std::function<void(ApiStatus, LinkAddress, RaceHandle)> getReceiverCallback;
    std::function<void(ApiStatus, std::vector<uint8_t>)> receiveCallback;
    std::function<void(ApiStatus)> closeCallback;

    ApiStatus status = ApiStatus::INVALID;
    LinkAddress address;
    OpHandle receiverHandle;

    ApiStatus status2 = ApiStatus::INVALID;
    std::vector<uint8_t> bytes2;

    ApiStatus status3 = ApiStatus::INVALID;
};

void ApiManagerRecvTestFixture::expectActivateChannel() {
    EXPECT_CALL(core->mockChannelManager,
                activateChannel(chanHandle, recvOptions.recv_channel, recvOptions.recv_role))
        .WillOnce(Return(ActivateChannelStatusCode::OK));
}

void ApiManagerRecvTestFixture::expectCreateLinkFromAddress() {
    EXPECT_CALL(*core->plugin, createLinkFromAddress(createLinkHandle, recvOptions.recv_channel,
                                                     recvOptions.recv_address, _))
        .WillOnce(Return(SdkResponse(SDK_OK)));
}

void ApiManagerRecvTestFixture::expectCreateLink() {
    EXPECT_CALL(*core->plugin, createLink(createLinkHandle, recvOptions.recv_channel, _))
        .WillOnce(Return(SdkResponse(SDK_OK)));
}

void ApiManagerRecvTestFixture::expectOpenConnection() {
    EXPECT_CALL(*core->plugin,
                openConnection(openConnHandle, linkProps.linkType, linkId, "{}", 0, 0, _))
        .WillOnce(Return(SdkResponse(SDK_OK)));
}

void ApiManagerRecvTestFixture::expectCloseConnection() {
    EXPECT_CALL(*core->plugin, closeConnection(closeConnHandle, connId, _))
        .WillOnce(Return(SdkResponse(SDK_OK)));
}

void ApiManagerRecvTestFixture::expectDestroyLink() {
    EXPECT_CALL(*core->plugin, destroyLink(destroyLinkHandle, linkId, _))
        .WillOnce(Return(SdkResponse(SDK_OK)));
}

void ApiManagerRecvTestFixture::expectAll() {
    recvOptions.recv_address = "";
    expectActivateChannel();
    expectCreateLink();
    expectOpenConnection();
    expectCloseConnection();
    expectDestroyLink();
}

void ApiManagerRecvTestFixture::expectedStatus() {
    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(address, linkProps.linkAddress);
    EXPECT_EQ(status2, ApiStatus::OK);
    EXPECT_EQ(bytes2, bytes);
    EXPECT_EQ(status3, ApiStatus::OK);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

void ApiManagerRecvTestFixture::getReceiverCall(
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback) {
    manager.getReceiveObject(recvOptions, callback);
    manager.waitForCallbacks();
}

void ApiManagerRecvTestFixture::onChannelStatusChangedCall() {
    manager.onChannelStatusChanged(*core->container, chanHandle, recvOptions.recv_channel,
                                   CHANNEL_AVAILABLE, channelProps);
    manager.waitForCallbacks();
}

void ApiManagerRecvTestFixture::onLinkStatusChangedCall() {
    manager.onLinkStatusChanged(*core->container, createLinkHandle, linkId, LINK_LOADED, linkProps);
    manager.waitForCallbacks();
}

void ApiManagerRecvTestFixture::onConnectionStatusChangedCall() {
    manager.onConnectionStatusChanged(*core->container, openConnHandle, connId, CONNECTION_OPEN,
                                      linkProps);
    manager.waitForCallbacks();
}

void ApiManagerRecvTestFixture::receiveCall(
    OpHandle handle, std::function<void(ApiStatus, std::vector<uint8_t>)> callback) {
    manager.receive(handle, callback);
    manager.waitForCallbacks();
}

void ApiManagerRecvTestFixture::closeCall(OpHandle handle,
                                          std::function<void(ApiStatus)> callback) {
    manager.close(handle, callback);
    manager.waitForCallbacks();
}

void ApiManagerRecvTestFixture::receiveEncPkgCall() {
    EncPkg pkg(0, 0, bytes);
    manager.receiveEncPkg(*core->container, pkg, {connId});
    manager.waitForCallbacks();
}

void ApiManagerRecvTestFixture::onConnectionStatusChangedClosedCall() {
    manager.onConnectionStatusChanged(*core->container, closeConnHandle, connId, CONNECTION_CLOSED,
                                      linkProps);
    manager.waitForCallbacks();
}

void ApiManagerRecvTestFixture::onLinkStatusChangedDestroyedCall() {
    manager.onLinkStatusChanged(*core->container, destroyLinkHandle, linkId, LINK_DESTROYED,
                                linkProps);
    manager.waitForCallbacks();
}

void ApiManagerRecvTestFixture::openAll() {
    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    onChannelStatusChangedCall();
    onLinkStatusChangedCall();
    onConnectionStatusChangedCall();
}

void ApiManagerRecvTestFixture::closeAll() {
    closeCall(receiverHandle, closeCallback);
    onConnectionStatusChangedClosedCall();
    onLinkStatusChangedDestroyedCall();
}

TEST_F(ApiManagerRecvTestFixture, createLink_no_errors) {
    expectAll();

    openAll();
    receiveCall(receiverHandle, receiveCallback);
    receiveEncPkgCall();
    closeAll();
    expectedStatus();
}

TEST_F(ApiManagerRecvTestFixture, createLinkFromAddress_no_errors) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    expectOpenConnection();
    expectCloseConnection();
    expectDestroyLink();

    openAll();
    receiveCall(receiverHandle, receiveCallback);
    receiveEncPkgCall();
    closeAll();
    expectedStatus();
}

TEST_F(ApiManagerRecvTestFixture, pkg_before_app_no_errors) {
    expectAll();

    openAll();
    receiveEncPkgCall();
    receiveCall(receiverHandle, receiveCallback);
    closeAll();
    expectedStatus();
}

TEST_F(ApiManagerRecvTestFixture, multiple_packages_no_errors) {
    expectAll();

    openAll();
    receiveCall(receiverHandle, receiveCallback);
    receiveEncPkgCall();
    receiveCall(receiverHandle, receiveCallback);
    receiveEncPkgCall();
    closeAll();
    expectedStatus();
}

TEST_F(ApiManagerRecvTestFixture, multiple_packages_order_2_no_errors) {
    expectAll();

    openAll();
    receiveEncPkgCall();
    receiveEncPkgCall();
    receiveCall(receiverHandle, receiveCallback);
    receiveCall(receiverHandle, receiveCallback);
    closeAll();
    expectedStatus();
}

TEST_F(ApiManagerRecvTestFixture, multiple_packages_order_3_no_errors) {
    expectAll();

    openAll();
    receiveEncPkgCall();
    receiveCall(receiverHandle, receiveCallback);
    receiveCall(receiverHandle, receiveCallback);
    receiveEncPkgCall();
    closeAll();
    expectedStatus();
}

TEST_F(ApiManagerRecvTestFixture, multiple_packages_order_4_no_errors) {
    expectAll();

    openAll();
    receiveCall(receiverHandle, receiveCallback);
    receiveEncPkgCall();
    receiveEncPkgCall();
    receiveCall(receiverHandle, receiveCallback);
    closeAll();
    expectedStatus();
}

TEST_F(ApiManagerRecvTestFixture, multiple_packages_order_5_no_errors) {
    expectAll();

    openAll();
    receiveEncPkgCall();
    receiveCall(receiverHandle, receiveCallback);
    receiveEncPkgCall();
    receiveCall(receiverHandle, receiveCallback);
    closeAll();
    expectedStatus();
}

TEST_F(ApiManagerRecvTestFixture, multiple_receive_calls_error) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    expectOpenConnection();

    openAll();
    receiveCall(receiverHandle, receiveCallback);
    receiveCall(receiverHandle, receiveCallback);

    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(address, linkProps.linkAddress);

    // This shouldn't really be internal error, but that's what it manifests as currently
    EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, no_packages_no_errors) {
    expectAll();

    openAll();
    closeAll();

    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(address, linkProps.linkAddress);
    EXPECT_EQ(status3, ApiStatus::OK);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, receive_no_packages_no_errors) {
    expectAll();
    openAll();
    receiveCall(receiverHandle, receiveCallback);
    closeAll();

    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(address, linkProps.linkAddress);
    EXPECT_EQ(status2, ApiStatus::CLOSING);
    EXPECT_EQ(bytes2, std::vector<uint8_t>{});
    EXPECT_EQ(status3, ApiStatus::OK);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, receiveEncPkg_no_receive_no_errors) {
    expectAll();
    openAll();
    receiveEncPkgCall();
    closeAll();

    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(address, linkProps.linkAddress);
    EXPECT_EQ(status3, ApiStatus::OK);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, activateChannel_error) {
    EXPECT_CALL(core->mockChannelManager,
                activateChannel(chanHandle, recvOptions.recv_channel, recvOptions.recv_role))
        .WillOnce(Return(ActivateChannelStatusCode::CHANNEL_DOES_NOT_EXIST));
    getReceiverCall(getReceiverCallback);

    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, createLinkFromAddress_error) {
    expectActivateChannel();
    EXPECT_CALL(*core->plugin, createLinkFromAddress(createLinkHandle, recvOptions.recv_channel,
                                                     recvOptions.recv_address, _))
        .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));
    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    onChannelStatusChangedCall();
    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, createLink_error) {
    recvOptions.recv_address = "";
    expectActivateChannel();
    EXPECT_CALL(*core->plugin, createLink(createLinkHandle, recvOptions.recv_channel, _))
        .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));
    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    onChannelStatusChangedCall();
    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, openConnection_error) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    EXPECT_CALL(*core->plugin,
                openConnection(openConnHandle, linkProps.linkType, linkId, "{}", 0, 0, _))
        .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    onChannelStatusChangedCall();
    onLinkStatusChangedCall();
    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

// This no longer returns INTERNAL_ERROR because close returns before the underlying connection is
// closed
TEST_F(ApiManagerRecvTestFixture, DISABLED_closeConnection_error) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    expectOpenConnection();
    EXPECT_CALL(*core->plugin, closeConnection(closeConnHandle, connId, _))
        .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

    openAll();
    receiveEncPkgCall();
    receiveCall(receiverHandle, receiveCallback);
    closeCall(receiverHandle, closeCallback);
    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(status2, ApiStatus::OK);
    EXPECT_EQ(status3, ApiStatus::INTERNAL_ERROR);

    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

// need close to indicate that something failed
// TEST_F(ApiManagerRecvTestFixture, destroyLink_error)

TEST_F(ApiManagerRecvTestFixture, getReceiver_bad_channel) {
    core->container.reset();
    getReceiverCall(getReceiverCallback);

    EXPECT_EQ(status, ApiStatus::CHANNEL_INVALID);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, getReceiver_empty_channel) {
    recvOptions.recv_channel = "";
    getReceiverCall(getReceiverCallback);

    EXPECT_EQ(status, ApiStatus::CHANNEL_INVALID);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, getReceiver_bad_role) {
    recvOptions.recv_role = "";
    getReceiverCall(getReceiverCallback);

    EXPECT_EQ(status, ApiStatus::INVALID_ARGUMENT);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, onChannelStatusChanged_activated_error) {
    expectActivateChannel();
    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    manager.onChannelStatusChanged(*core->container, chanHandle, recvOptions.recv_channel,
                                   CHANNEL_FAILED, channelProps);
    manager.waitForCallbacks();

    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, onChannelStatusChanged_no_handle_error) {
    expectActivateChannel();
    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    manager.onChannelStatusChanged(*core->container, 0, recvOptions.recv_channel, CHANNEL_FAILED,
                                   channelProps);
    manager.waitForCallbacks();

    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, onChannelStatusChanged_no_id_error) {
    expectActivateChannel();
    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    manager.onChannelStatusChanged(*core->container, chanHandle, "", CHANNEL_FAILED, channelProps);
    manager.waitForCallbacks();

    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, onLinkStatusChanged_created_error) {
    expectActivateChannel();
    expectCreateLinkFromAddress();

    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    onChannelStatusChangedCall();
    manager.onLinkStatusChanged(*core->container, createLinkHandle, linkId, LINK_DESTROYED,
                                linkProps);
    manager.waitForCallbacks();

    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, onLinkStatusChanged_created_bad_address_error) {
    linkProps.linkAddress = "other address";
    expectActivateChannel();
    expectCreateLinkFromAddress();

    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    onChannelStatusChangedCall();
    onLinkStatusChangedCall();

    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, onConnectionStatusChanged_opened_error) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    expectOpenConnection();

    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    onChannelStatusChangedCall();
    onLinkStatusChangedCall();
    manager.onConnectionStatusChanged(*core->container, openConnHandle, connId, CONNECTION_CLOSED,
                                      linkProps);
    manager.waitForCallbacks();

    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, receive_invalid_handle_error) {
    receiveCall(NULL_RACE_HANDLE, receiveCallback);
    EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, multiple_onChannelStatusChanged_calls) {
    expectActivateChannel();
    expectCreateLinkFromAddress();

    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    onChannelStatusChangedCall();
    onChannelStatusChangedCall();

    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(address, "");
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, multiple_onLinkStatusChanged_calls) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    expectOpenConnection();

    getReceiverCall(getReceiverCallback);
    EXPECT_EQ(manager.impl.activeContexts.size(), 2);

    onChannelStatusChangedCall();
    onLinkStatusChangedCall();
    onLinkStatusChangedCall();

    EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(address, "");
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, multiple_onConnectionStatusChanged_calls) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    expectOpenConnection();

    openAll();
    onConnectionStatusChangedCall();
    receiveCall(receiverHandle, receiveCallback);

    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(address, linkProps.linkAddress);
    EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(bytes2.empty(), true);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, multiple_receive_calls) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    expectOpenConnection();

    openAll();
    receiveCall(receiverHandle, receiveCallback);
    receiveCall(receiverHandle, receiveCallback);

    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(address, linkProps.linkAddress);

    // TODO: this shouldn't really be internal error, as it's triggered by the user.
    EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
    EXPECT_EQ(bytes2.empty(), true);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, multiple_onConnectionStatusChangedClosed_calls) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    expectOpenConnection();
    expectCloseConnection();

    openAll();
    receiveCall(receiverHandle, receiveCallback);
    receiveEncPkgCall();
    closeCall(receiverHandle, closeCallback);
    onConnectionStatusChangedClosedCall();
    onConnectionStatusChangedClosedCall();

    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(address, linkProps.linkAddress);

    // alrady received
    EXPECT_EQ(status2, ApiStatus::OK);
    EXPECT_EQ(bytes2, bytes);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerRecvTestFixture, multiple_onLinkStatusChangedDestroyed_calls) {
    expectActivateChannel();
    expectCreateLinkFromAddress();
    expectOpenConnection();
    expectCloseConnection();

    openAll();
    receiveCall(receiverHandle, receiveCallback);
    receiveEncPkgCall();
    closeCall(receiverHandle, closeCallback);
    onConnectionStatusChangedClosedCall();
    onLinkStatusChangedDestroyedCall();
    onLinkStatusChangedDestroyedCall();

    EXPECT_EQ(status, ApiStatus::OK);
    EXPECT_EQ(address, linkProps.linkAddress);

    // alrady received
    EXPECT_EQ(status2, ApiStatus::OK);
    EXPECT_EQ(bytes2, bytes);
    EXPECT_EQ(manager.impl.activeContexts.size(), 0);
    EXPECT_EQ(manager.impl.idContextMap.size(), 0);
    EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}
