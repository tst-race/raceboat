
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
#include "../../source/state-machine/Events.h"
#include "../../source/state-machine/States.h"
#include "../common/MockCore.h"
#include "../common/mocks/MockRacePlugin.h"

using namespace Raceboat;
using namespace testing;
using testing::_;

class TestableSendReceiveManager : public ApiManager {
public:
  using ApiManager::ApiManager;
  using ApiManager::impl;
};

class ApiManagerSendReceiveTestFixture : public ::testing::Test {
public:
  ApiManagerSendReceiveTestFixture()
      : params(), core(std::make_shared<MockCore>()), manager(*core) {
    sendOptions.send_channel = "sendChannel";
    sendOptions.send_address = "sendAddress";
    sendOptions.send_role = "sendRole";
    sendOptions.recv_channel = "recvChannel";
    sendOptions.recv_role = "recvRole";
    sendOptions.timeout_ms = 0;

    sendLinkId = "mockLinkId";
    recvLinkId = "mockLinkId2";
    sendConnId = "mockConnId";
    recvConnId = "mockConnId2";

    channelProps.bootstrap = false;
    channelProps.channelStatus = CHANNEL_UNDEF;
    channelProps.connectionType = CT_UNDEF;
    channelProps.maxCreatorsPerLoader = 1;
    channelProps.currentRole.roleName = sendOptions.send_role;
    channelProps.currentRole.linkSide = LS_UNDEF;
    channelProps.duration_s = 0;
    channelProps.intervalEndTime = 0;
    channelProps.isFlushable = true;
    channelProps.linkDirection = LD_LOADER_TO_CREATOR;
    channelProps.maxLoadersPerCreator = 1;

    sendBytes.reserve(0x100);
    recvBytes.reserve(0x100);
    for (uint16_t byte = 0; byte < 0x100; ++byte) {
      sendBytes.push_back(byte);
      recvBytes.push_back(0xFF - byte);
    }

    linkProps.linkAddress = "linkAddr";
    linkProps.linkType = LT_BIDI;

    status = ApiStatus::INVALID;
    sendReceiveCallback = [this](ApiStatus _status,
                                 std::vector<uint8_t> bytes) {
      status = _status;
      bytes2 = bytes;
    };
  }

  void expectActivateSendChannel(
      RaceHandle &handle,
      ActivateChannelStatusCode status = ActivateChannelStatusCode::OK);

  void expectActivateRecvChannel(
      RaceHandle &handle,
      ActivateChannelStatusCode status = ActivateChannelStatusCode::OK);
  void expectLoadLinkAddress(RaceHandle &handle, std::string send_address = "");
  void expectCreateLink(RaceHandle &handle);
  void expectOpenConnection(RaceHandle &handle, ConnectionID thisConnId);
  void expectSendPackage(RaceHandle &handle, ConnectionID thisConnId);
  void expectCloseConnection(RaceHandle &handle, ConnectionID thisConnId);
  void expectDestroyLink(RaceHandle &handle, LinkID thisLinkId);

  void
  sendReceiveCall(std::function<void(ApiStatus, std::vector<uint8_t>)> callback,
                  std::string send_address = "");

  void onChannelStatusChangedCall(RaceHandle handle, ChannelId channelId);
  void onLinkStatusChangedCall(RaceHandle handle, LinkID thisLinkId);
  void onConnectionStatusChangedCall(RaceHandle handle,
                                     ConnectionID thisConnId);
  void onPackageStatusChangedCall(RaceHandle handle);
  void receiveEncPkgCall(std::vector<uint8_t> &bytes, ConnectionID thisConnId);
  void onConnectionStatusChangedClosedCall(RaceHandle handle,
                                           ConnectionID thisConnId);
  void onLinkStatusChangedDestroyedCall(RaceHandle handle, LinkID thisLinkId);

  ChannelParamStore params;
  std::shared_ptr<MockCore> core;
  TestableSendReceiveManager manager;

  LinkProperties linkProps;
  ChannelProperties channelProps;
  SendOptions sendOptions;

  LinkID sendLinkId;
  LinkID recvLinkId;
  ConnectionID sendConnId;
  ConnectionID recvConnId;

  std::vector<uint8_t> sendBytes;
  std::vector<uint8_t> recvBytes;
  std::vector<uint8_t> bytes2;

  RaceHandle chanHandle;
  RaceHandle loadLinkHandle;
  RaceHandle createLinkHandle;
  RaceHandle openSendConnHandle;
  RaceHandle openRecvConnHandle;
  RaceHandle pkgHandle;
  RaceHandle closeSendConnHandle;
  RaceHandle closeRecvConnHandle;
  RaceHandle destroySendLinkHandle;
  RaceHandle destroyRecvLinkHandle;

  ApiStatus status;
  std::function<void(ApiStatus, std::vector<uint8_t>)> sendReceiveCallback;
};

void ApiManagerSendReceiveTestFixture::expectActivateSendChannel(
    RaceHandle &handle, ActivateChannelStatusCode thisStatus) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, sendOptions.send_channel, sendOptions.send_role))
      .WillOnce([&handle, thisStatus](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return thisStatus;
      });
}

void ApiManagerSendReceiveTestFixture::expectActivateRecvChannel(
    RaceHandle &handle, ActivateChannelStatusCode thisStatus) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, sendOptions.recv_channel, sendOptions.recv_role))
      .WillOnce([&handle, thisStatus](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return thisStatus;
      });
}

void ApiManagerSendReceiveTestFixture::expectLoadLinkAddress(
    RaceHandle &handle, std::string send_address) {
  send_address = send_address.empty() ? sendOptions.send_address : send_address;
  EXPECT_CALL(*core->plugin,
              loadLinkAddress(_, sendOptions.send_channel, send_address, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerSendReceiveTestFixture::expectCreateLink(RaceHandle &handle) {
  EXPECT_CALL(*core->plugin, createLink(_, sendOptions.recv_channel, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerSendReceiveTestFixture::expectOpenConnection(
    RaceHandle &handle, ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, openConnection(_, _, thisConnId, "{}", 0, 0, _))
      .WillOnce(
          [&handle](RaceHandle _handle, auto, auto, auto, auto, auto, auto) {
            handle = _handle;
            return SdkResponse(SDK_OK);
          });
}

void ApiManagerSendReceiveTestFixture::expectSendPackage(
    RaceHandle &handle, ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, sendPackage(_, thisConnId, _, _, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerSendReceiveTestFixture::expectCloseConnection(
    RaceHandle &handle, ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, closeConnection(_, thisConnId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerSendReceiveTestFixture::expectDestroyLink(RaceHandle &handle,
                                                         LinkID thisLinkId) {
  EXPECT_CALL(*core->plugin, destroyLink(_, thisLinkId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerSendReceiveTestFixture::sendReceiveCall(
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback,
    std::string send_address) {
  SendOptions sendOptionsCopy = sendOptions;
  sendOptionsCopy.send_address =
      send_address.empty() ? sendOptions.send_address : send_address;
  SdkResponse response =
      manager.sendReceive(sendOptionsCopy, sendBytes, callback);
  EXPECT_EQ(response.status, SDK_OK);
  manager.waitForCallbacks();
}

void ApiManagerSendReceiveTestFixture::onChannelStatusChangedCall(
    RaceHandle handle, ChannelId channelId) {
  SdkResponse response = manager.onChannelStatusChanged(
      *core->container, handle, channelId, CHANNEL_AVAILABLE, channelProps);
  EXPECT_EQ(response.status, SDK_OK);
  manager.waitForCallbacks();
}

void ApiManagerSendReceiveTestFixture::onLinkStatusChangedCall(
    RaceHandle handle, LinkID thisLinkId) {
  SdkResponse response = manager.onLinkStatusChanged(
      *core->container, handle, thisLinkId, LINK_LOADED, linkProps);
  EXPECT_EQ(response.status, SDK_OK);
  manager.waitForCallbacks();
}

void ApiManagerSendReceiveTestFixture::onConnectionStatusChangedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  SdkResponse response = manager.onConnectionStatusChanged(
      *core->container, handle, thisConnId, CONNECTION_OPEN, linkProps);
  EXPECT_EQ(response.status, SDK_OK);
  manager.waitForCallbacks();
}

void ApiManagerSendReceiveTestFixture::onPackageStatusChangedCall(
    RaceHandle handle) {
  SdkResponse response =
      manager.onPackageStatusChanged(*core->container, handle, PACKAGE_SENT);
  EXPECT_EQ(response.status, SDK_OK);
  manager.waitForCallbacks();
}

void ApiManagerSendReceiveTestFixture::receiveEncPkgCall(
    std::vector<uint8_t> &bytes, ConnectionID thisConnId) {
  EncPkg pkg(0, 0, bytes);
  SdkResponse response =
      manager.receiveEncPkg(*core->container, pkg, {thisConnId});
  EXPECT_EQ(response.status, SDK_OK);
  manager.waitForCallbacks();
}

void ApiManagerSendReceiveTestFixture::onConnectionStatusChangedClosedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  SdkResponse response = manager.onConnectionStatusChanged(
      *core->container, handle, thisConnId, CONNECTION_CLOSED, linkProps);
  EXPECT_EQ(response.status, SDK_OK);
  manager.waitForCallbacks();
}

void ApiManagerSendReceiveTestFixture::onLinkStatusChangedDestroyedCall(
    RaceHandle handle, LinkID thisLinkId) {
  SdkResponse response = manager.onLinkStatusChanged(
      *core->container, handle, thisLinkId, LINK_DESTROYED, linkProps);
  EXPECT_EQ(response.status, SDK_OK);
  manager.waitForCallbacks();
}

TEST_F(ApiManagerSendReceiveTestFixture, no_errors) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  onPackageStatusChangedCall(pkgHandle);
  receiveEncPkgCall(recvBytes, recvConnId);
  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvBytes);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, sendReceive_bad_callback_error) {
  auto response = manager.sendReceive(sendOptions, sendBytes, {});
  EXPECT_EQ(response.status, SDK_INVALID_ARGUMENT);

  manager.waitForCallbacks();
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, sendReceive_empty_sendChannel_error) {
  sendOptions.send_channel = "";
  sendReceiveCall(sendReceiveCallback);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::CHANNEL_INVALID);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, sendReceive_empty_sendRole_error) {
  sendOptions.send_role = "";
  sendReceiveCall(sendReceiveCallback);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INVALID_ARGUMENT);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, sendReceive_empty_recvChannel_error) {
  sendOptions.recv_channel = "";
  sendReceiveCall(sendReceiveCallback);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::CHANNEL_INVALID);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, sendReceive_empty_recvRole_error) {
  sendOptions.recv_role = "";
  sendReceiveCall(sendReceiveCallback);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INVALID_ARGUMENT);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, sendReceive_empty_send_address_error) {
  sendOptions.send_address = "";
  sendReceiveCall(sendReceiveCallback);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INVALID_ARGUMENT);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, activateChannel_1_error) {
  expectActivateSendChannel(chanHandle,
                            ActivateChannelStatusCode::CHANNEL_DOES_NOT_EXIST);

  sendReceiveCall(sendReceiveCallback);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, activateChannel_2_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::CHANNEL_DOES_NOT_EXIST);

  sendReceiveCall(sendReceiveCallback);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, onChannelStatusChanged_send_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  manager.onChannelStatusChanged(*core->container, NULL_RACE_HANDLE,
                                 sendOptions.send_channel, CHANNEL_FAILED, {});

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, onChannelStatusChanged_recv_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  manager.onChannelStatusChanged(*core->container, NULL_RACE_HANDLE,
                                 sendOptions.recv_channel, CHANNEL_FAILED, {});

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, createLink_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  EXPECT_CALL(*core->plugin, createLink(_, sendOptions.recv_channel, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, loadLinkAddress_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  EXPECT_CALL(*core->plugin, loadLinkAddress(_, sendOptions.send_channel,
                                             sendOptions.send_address, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, onLinkStatusChanged_recv_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  manager.onLinkStatusChanged(*core->container, createLinkHandle, recvLinkId,
                              LINK_DESTROYED, linkProps);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, onLinkStatusChanged_send_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  manager.onLinkStatusChanged(*core->container, loadLinkHandle, sendLinkId,
                              LINK_DESTROYED, linkProps);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, openConnection_recv_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  EXPECT_CALL(*core->plugin, openConnection(_, _, recvLinkId, "{}", 0, 0, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID)));

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, openConnection_send_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  EXPECT_CALL(*core->plugin, openConnection(_, _, sendLinkId, "{}", 0, 0, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID)));

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture,
       onConnectionStatusChanged_open_recv_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  manager.onConnectionStatusChanged(*core->container, openRecvConnHandle,
                                    recvConnId, CONNECTION_CLOSED, linkProps);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture,
       onConnectionStatusChanged_open_send_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  manager.onConnectionStatusChanged(*core->container, openSendConnHandle,
                                    sendConnId, CONNECTION_CLOSED, linkProps);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, sendPackage_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  EXPECT_CALL(*core->plugin, sendPackage(_, sendConnId, _, _, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, onPackageStatusChanged_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectSendPackage(pkgHandle, sendConnId);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  manager.onPackageStatusChanged(*core->container, pkgHandle,
                                 PACKAGE_FAILED_GENERIC);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, closeConnection_recv_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  EXPECT_CALL(*core->plugin, closeConnection(_, recvConnId, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));
  expectDestroyLink(destroySendLinkHandle, sendLinkId);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  onPackageStatusChangedCall(pkgHandle);
  receiveEncPkgCall(recvBytes, recvConnId);
  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvBytes);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, closeConnection_send_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  EXPECT_CALL(*core->plugin, closeConnection(_, sendConnId, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  onPackageStatusChangedCall(pkgHandle);
  receiveEncPkgCall(recvBytes, recvConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvBytes);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, destroyLink_recv_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  EXPECT_CALL(*core->plugin, destroyLink(_, recvLinkId, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  onPackageStatusChangedCall(pkgHandle);
  receiveEncPkgCall(recvBytes, recvConnId);
  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvBytes);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, destroyLink_send_error) {
  expectActivateSendChannel(chanHandle);

  RaceHandle chanHandle2;
  expectActivateRecvChannel(chanHandle2,
                            ActivateChannelStatusCode::ALREADY_ACTIVATED);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);
  EXPECT_CALL(*core->plugin, destroyLink(_, sendLinkId, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

  sendReceiveCall(sendReceiveCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(chanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(chanHandle2, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  onPackageStatusChangedCall(pkgHandle);
  receiveEncPkgCall(recvBytes, recvConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvBytes);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendReceiveTestFixture, empty_callback_error) {
  SendReceiveStateEngine sendReceiveEngine;
  ApiSendReceiveContext context(manager.impl, sendReceiveEngine);
  context.receivedMsg = std::make_shared<std::vector<uint8_t>>();
  context.currentStateId = STATE_SEND_RECEIVE_PACKAGE_SENT;
  sendReceiveEngine.handleEvent(context, EVENT_RECEIVE_PACKAGE);
  EXPECT_EQ(context.currentStateId, STATE_SEND_RECEIVE_FAILED);
}
