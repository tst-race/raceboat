
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

class TestableUnidirectionalSendManager : public ApiManager {
public:
  using ApiManager::ApiManager;
  using ApiManager::impl;
};

class ApiManagerSendTestFixture : public ::testing::Test {
public:
  ApiManagerSendTestFixture()
      : params(), core(std::make_shared<MockCore>()), manager(*core) {
    sendOptions.send_channel = "sendChannel";
    sendOptions.send_address = "sendAddress";
    sendOptions.send_role = "sendRole";
    sendOptions.timeout_ms = 0;

    chanId = "mockChanId";
    linkId = "mockLinkId";
    connId = "mockConnId";

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

    bytes.reserve(0x100);
    for (uint16_t byte = 0; byte < 0x100; ++byte) {
      bytes.push_back(byte);
    }

    linkProps.linkAddress = "linkAddr";
    linkProps.linkType = LT_SEND;
  }

  void expectActivateChannel(
      RaceHandle &handle,
      ActivateChannelStatusCode status = ActivateChannelStatusCode::OK);
  void expectLoadLinkAddress(RaceHandle &handle, std::string send_address = "");
  void expectOpenConnection(RaceHandle &handle, ConnectionID thisConnId);
  void expectSendPackage(RaceHandle &handle, ConnectionID thisConnId);
  void expectCloseConnection(RaceHandle &handle, ConnectionID thisConnId);
  void expectDestroyLink(RaceHandle &handle, LinkID thisLinkId);

  void sendCall(std::function<void(ApiStatus)> callback,
                std::string send_address = "");

  void onChannelStatusChangedCall(RaceHandle handle);
  void onLinkStatusChangedCall(RaceHandle handle, LinkID thisLinkId);
  void onConnectionStatusChangedCall(RaceHandle handle,
                                     ConnectionID thisConnId);
  void onPackageStatusChangedCall(RaceHandle handle);
  void onConnectionStatusChangedClosedCall(RaceHandle handle,
                                           ConnectionID thisConnId);
  void onLinkStatusChangedDestroyedCall(RaceHandle handle, LinkID thisLinkId);

  ChannelParamStore params;
  std::shared_ptr<MockCore> core;
  TestableUnidirectionalSendManager manager;

  LinkProperties linkProps;
  ChannelProperties channelProps;
  SendOptions sendOptions;

  ChannelId chanId;
  LinkID linkId;
  ConnectionID connId;

  std::vector<uint8_t> bytes;

  RaceHandle chanHandle;
  RaceHandle loadLinkHandle;
  RaceHandle openConnHandle;
  RaceHandle pkgHandle;
  RaceHandle closeConnHandle;
  RaceHandle destroyLinkHandle;
};

void ApiManagerSendTestFixture::expectActivateChannel(
    RaceHandle &handle, ActivateChannelStatusCode status) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, sendOptions.send_channel, sendOptions.send_role))
      .WillOnce([&handle, status](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return status;
      });
}

void ApiManagerSendTestFixture::expectLoadLinkAddress(
    RaceHandle &handle, std::string send_address) {
  send_address = send_address.empty() ? sendOptions.send_address : send_address;
  EXPECT_CALL(*core->plugin,
              loadLinkAddress(_, sendOptions.send_channel, send_address, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerSendTestFixture::expectOpenConnection(RaceHandle &handle,
                                                     ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin,
              openConnection(_, linkProps.linkType, thisConnId, "{}", 0, 0, _))
      .WillOnce(
          [&handle](RaceHandle _handle, auto, auto, auto, auto, auto, auto) {
            handle = _handle;
            return SdkResponse(SDK_OK);
          });
}

void ApiManagerSendTestFixture::expectSendPackage(RaceHandle &handle,
                                                  ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, sendPackage(_, thisConnId, _, _, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerSendTestFixture::expectCloseConnection(RaceHandle &handle,
                                                      ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, closeConnection(_, thisConnId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerSendTestFixture::expectDestroyLink(RaceHandle &handle,
                                                  LinkID thisLinkId) {
  EXPECT_CALL(*core->plugin, destroyLink(_, thisLinkId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerSendTestFixture::sendCall(
    std::function<void(ApiStatus)> callback, std::string send_address) {
  SendOptions sendOptionsCopy = sendOptions;
  sendOptionsCopy.send_address =
      send_address.empty() ? sendOptions.send_address : send_address;
  manager.send(sendOptionsCopy, bytes, callback);
  manager.waitForCallbacks();
}

void ApiManagerSendTestFixture::onChannelStatusChangedCall(RaceHandle handle) {
  manager.onChannelStatusChanged(*core->container, handle,
                                 sendOptions.send_channel, CHANNEL_AVAILABLE,
                                 channelProps);
  manager.waitForCallbacks();
}

void ApiManagerSendTestFixture::onLinkStatusChangedCall(RaceHandle handle,
                                                        LinkID thisLinkId) {
  manager.onLinkStatusChanged(*core->container, handle, thisLinkId, LINK_LOADED,
                              linkProps);
  manager.waitForCallbacks();
}

void ApiManagerSendTestFixture::onConnectionStatusChangedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  manager.onConnectionStatusChanged(*core->container, handle, thisConnId,
                                    CONNECTION_OPEN, linkProps);
  manager.waitForCallbacks();
}

void ApiManagerSendTestFixture::onPackageStatusChangedCall(RaceHandle handle) {
  manager.onPackageStatusChanged(*core->container, handle, PACKAGE_SENT);
  manager.waitForCallbacks();
}

void ApiManagerSendTestFixture::onConnectionStatusChangedClosedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  manager.onConnectionStatusChanged(*core->container, handle, thisConnId,
                                    CONNECTION_CLOSED, linkProps);
  manager.waitForCallbacks();
}

void ApiManagerSendTestFixture::onLinkStatusChangedDestroyedCall(
    RaceHandle handle, LinkID thisLinkId) {
  manager.onLinkStatusChanged(*core->container, handle, thisLinkId,
                              LINK_DESTROYED, linkProps);
  manager.waitForCallbacks();
}

TEST_F(ApiManagerSendTestFixture, no_errors) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);
  expectCloseConnection(closeConnHandle, connId);
  expectDestroyLink(destroyLinkHandle, linkId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);
  onConnectionStatusChangedClosedCall(closeConnHandle, connId);
  onLinkStatusChangedDestroyedCall(destroyLinkHandle, linkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, multiple_sends_no_errors) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);
  expectCloseConnection(closeConnHandle, connId);
  expectDestroyLink(destroyLinkHandle, linkId);

  RaceHandle loadLinkHandle2 = 0;
  RaceHandle openConnHandle2 = 0;
  RaceHandle pkgHandle2 = 0;
  RaceHandle closeConnHandle2 = 0;
  RaceHandle destroyLinkHandle2 = 0;

  LinkID linkId2 = "mockLinkId2";
  ConnectionID connId2 = "mockConnId2";

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);
  onConnectionStatusChangedClosedCall(closeConnHandle, connId);
  onLinkStatusChangedDestroyedCall(destroyLinkHandle, linkId);

  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);

  // There should not be an activate channel call the second time
  expectLoadLinkAddress(loadLinkHandle2);
  expectOpenConnection(openConnHandle2, linkId2);
  expectSendPackage(pkgHandle2, connId2);
  expectCloseConnection(closeConnHandle2, connId2);
  expectDestroyLink(destroyLinkHandle2, linkId2);

  status = ApiStatus::INVALID;
  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onLinkStatusChangedCall(loadLinkHandle2, linkId2);
  onConnectionStatusChangedCall(openConnHandle2, connId2);
  onPackageStatusChangedCall(pkgHandle2);
  onConnectionStatusChangedClosedCall(closeConnHandle2, connId2);
  onLinkStatusChangedDestroyedCall(destroyLinkHandle2, linkId2);

  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, multiple_sends_interleaved_no_errors) {
  // these get set by the expect calls
  RaceHandle chanHandle2 = 0;
  RaceHandle loadLinkHandle2 = 0;
  RaceHandle openConnHandle2 = 0;
  RaceHandle pkgHandle2 = 0;
  RaceHandle closeConnHandle2 = 0;
  RaceHandle destroyLinkHandle2 = 0;

  LinkID linkId2 = "mockLinkId2";
  ConnectionID connId2 = "mockConnId2";
  std::string send_address2 = "sendAddress2";

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  ApiStatus status2 = ApiStatus::INVALID;
  auto sendCallback2 = [&status2](ApiStatus _status) { status2 = _status; };

  expectActivateChannel(chanHandle);
  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  expectActivateChannel(chanHandle2,
                        ActivateChannelStatusCode::ALREADY_ACTIVATED);
  sendCall(sendCallback2, send_address2);
  EXPECT_EQ(manager.impl.activeContexts.size(), 4);

  expectLoadLinkAddress(loadLinkHandle);
  expectLoadLinkAddress(loadLinkHandle2, send_address2);

  // just a single on channelStatusChanged call
  onChannelStatusChangedCall(chanHandle);

  expectOpenConnection(openConnHandle, linkId);
  expectOpenConnection(openConnHandle2, linkId2);
  expectSendPackage(pkgHandle, connId);
  expectSendPackage(pkgHandle2, connId2);
  expectCloseConnection(closeConnHandle, connId);
  expectCloseConnection(closeConnHandle2, connId2);
  expectDestroyLink(destroyLinkHandle, linkId);
  expectDestroyLink(destroyLinkHandle2, linkId2);

  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onLinkStatusChangedCall(loadLinkHandle2, linkId2);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onConnectionStatusChangedCall(openConnHandle2, connId2);
  onPackageStatusChangedCall(pkgHandle);
  onPackageStatusChangedCall(pkgHandle2);
  onConnectionStatusChangedClosedCall(closeConnHandle, connId);
  onConnectionStatusChangedClosedCall(closeConnHandle2, connId2);
  onLinkStatusChangedDestroyedCall(destroyLinkHandle, linkId);
  onLinkStatusChangedDestroyedCall(destroyLinkHandle2, linkId2);

  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, multiple_sends_different_role_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);
  expectCloseConnection(closeConnHandle, connId);
  expectDestroyLink(destroyLinkHandle, linkId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);
  onConnectionStatusChangedClosedCall(closeConnHandle, connId);
  onLinkStatusChangedDestroyedCall(destroyLinkHandle, linkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);

  status = ApiStatus::INVALID;
  sendOptions.send_role = "Some other role";
  manager.send(sendOptions, bytes, sendCallback);
  manager.waitForCallbacks();

  // this should be something else, but this is what currently happens
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, send_bad_channel) {
  core->container.reset();
  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);

  EXPECT_EQ(status, ApiStatus::CHANNEL_INVALID);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, send_empty_channel) {
  sendOptions.send_channel = "";
  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);

  EXPECT_EQ(status, ApiStatus::CHANNEL_INVALID);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, send_empty_role) {
  sendOptions.send_role = "";
  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);

  EXPECT_EQ(status, ApiStatus::INVALID_ARGUMENT);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, send_empty_address) {
  sendOptions.send_address = "";
  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);

  EXPECT_EQ(status, ApiStatus::INVALID_ARGUMENT);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, activateChannel_error) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, sendOptions.send_channel, sendOptions.send_role))
      .WillOnce(Return(ActivateChannelStatusCode::CHANNEL_DOES_NOT_EXIST));

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, onChannelStatusChanged_error) {
  expectActivateChannel(chanHandle);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  manager.onChannelStatusChanged(*core->container, chanHandle,
                                 sendOptions.send_channel, CHANNEL_FAILED, {});

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, onChannelStatusChanged_error_no_handle) {
  expectActivateChannel(chanHandle);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  manager.onChannelStatusChanged(*core->container, NULL_RACE_HANDLE,
                                 sendOptions.send_channel, CHANNEL_FAILED, {});

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, loadLinkAddress_error) {
  expectActivateChannel(chanHandle);
  EXPECT_CALL(*core->plugin, loadLinkAddress(_, sendOptions.send_channel,
                                             sendOptions.send_address, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, onLinkStatusChanged_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  manager.onLinkStatusChanged(*core->container, loadLinkHandle, linkId,
                              LINK_DESTROYED, linkProps);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, openConnection_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  EXPECT_CALL(*core->plugin,
              openConnection(_, linkProps.linkType, linkId, "{}", 0, 0, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID)));

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, onConnectionStatusChanged_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  manager.onConnectionStatusChanged(*core->container, openConnHandle, connId,
                                    CONNECTION_CLOSED, linkProps);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, sendPackage_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  EXPECT_CALL(*core->plugin, sendPackage(_, connId, _, _, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, onPackageStatusChanged_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  manager.onPackageStatusChanged(*core->container, pkgHandle,
                                 PACKAGE_FAILED_GENERIC);

  manager.waitForCallbacks();
  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, closeConnection_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);
  EXPECT_CALL(*core->plugin, closeConnection(_, connId, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);

  manager.waitForCallbacks();

  // We sent the package, so expect ok
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture,
       DISABLED_onConnectionStatusChanged_closed_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);
  expectCloseConnection(closeConnHandle, connId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);

  // How can this call fail?
  // manager.onConnectionStatusChanged(*core->container, closeConnHandle,
  // connId, CONNECTION_CLOSED,
  //                                   linkProps);

  manager.waitForCallbacks();
  // We sent the package, so expect ok
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, destroyLink_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);
  expectCloseConnection(closeConnHandle, connId);
  EXPECT_CALL(*core->plugin, destroyLink(_, linkId, _))
      .WillOnce(Return(SdkResponse(SDK_INVALID_ARGUMENT)));

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);
  onConnectionStatusChangedClosedCall(closeConnHandle, connId);

  manager.waitForCallbacks();
  // We sent the package, so expect ok
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture,
       DISABLED_onLinkStatusChanged_destroyed_error) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);
  expectCloseConnection(closeConnHandle, connId);
  expectDestroyLink(destroyLinkHandle, linkId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };

  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);
  onConnectionStatusChangedClosedCall(closeConnHandle, connId);

  // How can this call fail?
  // manager.onLinkStatusChanged(*core->container, destroyLinkHandle, linkId,
  // LINK_DESTROYED,
  //                             linkProps);

  manager.waitForCallbacks();
  // We sent the package, so expect ok
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, multiple_onChannelStatusChanged_calls) {
  expectActivateChannel(chanHandle);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onChannelStatusChangedCall(chanHandle);

  manager.waitForCallbacks();

  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, multiple_onLinkStatusChanged_calls) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onLinkStatusChangedCall(loadLinkHandle, linkId);

  manager.waitForCallbacks();

  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, multiple_onConnectionStatusChanged_calls) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onConnectionStatusChangedCall(openConnHandle, connId);

  manager.waitForCallbacks();

  EXPECT_EQ(status, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, multiple_onPackageStatusChanged_calls) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);
  onPackageStatusChangedCall(pkgHandle);

  manager.waitForCallbacks();

  // already received package
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture,
       multiple_onConnectionStatusChangedClosed_calls) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);
  expectCloseConnection(closeConnHandle, connId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);
  onConnectionStatusChangedClosedCall(closeConnHandle, connId);
  onConnectionStatusChangedClosedCall(closeConnHandle, connId);

  manager.waitForCallbacks();

  // already received package
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerSendTestFixture, multiple_onLinkStatusChangedDestroyed_calls) {
  expectActivateChannel(chanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openConnHandle, linkId);
  expectSendPackage(pkgHandle, connId);
  expectCloseConnection(closeConnHandle, connId);
  expectDestroyLink(destroyLinkHandle, linkId);

  ApiStatus status = ApiStatus::INVALID;
  auto sendCallback = [&status](ApiStatus _status) { status = _status; };
  sendCall(sendCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle);
  onLinkStatusChangedCall(loadLinkHandle, linkId);
  onConnectionStatusChangedCall(openConnHandle, connId);
  onPackageStatusChangedCall(pkgHandle);
  onConnectionStatusChangedClosedCall(closeConnHandle, connId);
  onLinkStatusChangedDestroyedCall(destroyLinkHandle, linkId);
  onLinkStatusChangedDestroyedCall(destroyLinkHandle, linkId);

  manager.waitForCallbacks();

  // already received package
  EXPECT_EQ(status, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}
