
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

class TestableDialManager : public ApiManager {
public:
  using ApiManager::ApiManager;
  using ApiManager::impl;
};

class ApiManagerDialTestFixture : public ::testing::Test {
public:
  ApiManagerDialTestFixture()
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
    channelProps.creatorsPerLoader = 1;
    channelProps.currentRole.roleName = sendOptions.send_role;
    channelProps.currentRole.linkSide = LS_UNDEF;
    channelProps.duration_s = 0;
    channelProps.intervalEndTime = 0;
    channelProps.isFlushable = true;
    channelProps.linkDirection = LD_LOADER_TO_CREATOR;
    channelProps.loadersPerCreator = 1;

    sendBytes.reserve(0x100);
    recvBytes.reserve(0x100);
    for (uint16_t byte = 0; byte < 0x100; ++byte) {
      sendBytes.push_back(byte);
      recvBytes.push_back(0xFF - byte);
    }

    linkProps.linkAddress = "linkAddr";
    linkProps.linkType = LT_BIDI;

    status1 = ApiStatus::INVALID;
    dialCallback = [this](ApiStatus _status, RaceHandle _handle) {
      status1 = _status;
      dialHandle = _handle;
    };

    status2 = ApiStatus::INVALID;
    readCallback = [this](ApiStatus _status, std::vector<uint8_t> bytes) {
      status2 = _status;
      bytes2 = bytes;
    };

    status3 = ApiStatus::INVALID;
    writeCallback = [this](ApiStatus _status) { status3 = _status; };

    status4 = ApiStatus::INVALID;
    closeCallback = [this](ApiStatus _status) { status4 = _status; };
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

  void dialCall(std::function<void(ApiStatus, RaceHandle)> callback,
                std::string send_address = "");
  void readCall(RaceHandle handle,
                std::function<void(ApiStatus, std::vector<uint8_t>)> callback);
  void writeCall(RaceHandle handle, std::function<void(ApiStatus)> callback);
  void closeCall(RaceHandle handle, std::function<void(ApiStatus)> callback);

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
  TestableDialManager manager;

  LinkProperties linkProps;
  ChannelProperties channelProps;
  SendOptions sendOptions;

  LinkID sendLinkId;
  LinkID recvLinkId;
  ConnectionID sendConnId;
  ConnectionID recvConnId;

  std::vector<uint8_t> sendBytes;
  std::vector<uint8_t> recvBytes;

  RaceHandle sendChanHandle;
  RaceHandle recvChanHandle;
  RaceHandle loadLinkHandle;
  RaceHandle createLinkHandle;
  RaceHandle openSendConnHandle;
  RaceHandle openRecvConnHandle;
  RaceHandle pkgHandle;
  RaceHandle pkgHandle2;
  RaceHandle closeSendConnHandle;
  RaceHandle closeRecvConnHandle;
  RaceHandle destroySendLinkHandle;
  RaceHandle destroyRecvLinkHandle;

  ApiStatus status1;
  RaceHandle dialHandle;
  std::function<void(ApiStatus, RaceHandle)> dialCallback;

  ApiStatus status2;
  std::vector<uint8_t> bytes2;
  std::function<void(ApiStatus, std::vector<uint8_t>)> readCallback;

  ApiStatus status3;
  std::function<void(ApiStatus)> writeCallback;

  ApiStatus status4;
  std::function<void(ApiStatus)> closeCallback;
};

void ApiManagerDialTestFixture::expectActivateSendChannel(
    RaceHandle &handle, ActivateChannelStatusCode thisStatus) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, sendOptions.send_channel, sendOptions.send_role))
      .WillOnce([&handle, thisStatus](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return thisStatus;
      });
}

void ApiManagerDialTestFixture::expectActivateRecvChannel(
    RaceHandle &handle, ActivateChannelStatusCode thisStatus) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, sendOptions.recv_channel, sendOptions.recv_role))
      .WillOnce([&handle, thisStatus](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return thisStatus;
      });
}

void ApiManagerDialTestFixture::expectLoadLinkAddress(
    RaceHandle &handle, std::string send_address) {
  send_address = send_address.empty() ? sendOptions.send_address : send_address;
  EXPECT_CALL(*core->plugin,
              loadLinkAddress(_, sendOptions.send_channel, send_address, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerDialTestFixture::expectCreateLink(RaceHandle &handle) {
  EXPECT_CALL(*core->plugin, createLink(_, sendOptions.recv_channel, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerDialTestFixture::expectOpenConnection(RaceHandle &handle,
                                                     ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, openConnection(_, _, thisConnId, "{}", 0, 0, _))
      .WillOnce(
          [&handle](RaceHandle _handle, auto, auto, auto, auto, auto, auto) {
            handle = _handle;
            return SdkResponse(SDK_OK);
          });
}

void ApiManagerDialTestFixture::expectSendPackage(RaceHandle &handle,
                                                  ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, sendPackage(_, thisConnId, _, _, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      })
      .RetiresOnSaturation(); // this allows multiple packages to be sent if we
                              // call expectSendPackage multiple times.
}

void ApiManagerDialTestFixture::expectCloseConnection(RaceHandle &handle,
                                                      ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, closeConnection(_, thisConnId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerDialTestFixture::expectDestroyLink(RaceHandle &handle,
                                                  LinkID thisLinkId) {
  EXPECT_CALL(*core->plugin, destroyLink(_, thisLinkId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerDialTestFixture::dialCall(
    std::function<void(ApiStatus, RaceHandle)> callback,
    std::string send_address) {
  SendOptions sendOptionsCopy = sendOptions;
  sendOptionsCopy.send_address =
      send_address.empty() ? sendOptions.send_address : send_address;
  manager.dial(sendOptionsCopy, sendBytes, callback);
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::readCall(
    RaceHandle handle,
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback) {
  manager.read(handle, callback);
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::writeCall(
    RaceHandle handle, std::function<void(ApiStatus)> callback) {
  manager.write(handle, sendBytes, callback);
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::closeCall(
    RaceHandle handle, std::function<void(ApiStatus)> callback) {
  manager.close(handle, callback);
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::onChannelStatusChangedCall(
    RaceHandle handle, ChannelId channelId) {
  manager.onChannelStatusChanged(*core->container, handle, channelId,
                                 CHANNEL_AVAILABLE, channelProps);
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::onLinkStatusChangedCall(RaceHandle handle,
                                                        LinkID thisLinkId) {
  manager.onLinkStatusChanged(*core->container, handle, thisLinkId, LINK_LOADED,
                              linkProps);
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::onConnectionStatusChangedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  manager.onConnectionStatusChanged(*core->container, handle, thisConnId,
                                    CONNECTION_OPEN, linkProps);
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::onPackageStatusChangedCall(RaceHandle handle) {
  manager.onPackageStatusChanged(*core->container, handle, PACKAGE_SENT);
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::receiveEncPkgCall(std::vector<uint8_t> &bytes,
                                                  ConnectionID thisConnId) {
  std::vector<uint8_t> packageId(16, 1);
  packageId.insert(packageId.end(), bytes.begin(), bytes.end());
  EncPkg pkg(0, 0, packageId);
  manager.receiveEncPkg(*core->container, pkg, {thisConnId});
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::onConnectionStatusChangedClosedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  manager.onConnectionStatusChanged(*core->container, handle, thisConnId,
                                    CONNECTION_CLOSED, linkProps);
  manager.waitForCallbacks();
}

void ApiManagerDialTestFixture::onLinkStatusChangedDestroyedCall(
    RaceHandle handle, LinkID thisLinkId) {
  manager.onLinkStatusChanged(*core->container, handle, thisLinkId,
                              LINK_DESTROYED, linkProps);
  manager.waitForCallbacks();
}

TEST_F(ApiManagerDialTestFixture, no_errors) {
  EXPECT_CALL(*core, getEntropy(_))
      .WillRepeatedly(Return(std::vector<uint8_t>(16, 1)));
  expectActivateSendChannel(sendChanHandle);
  expectActivateRecvChannel(recvChanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectSendPackage(pkgHandle2, sendConnId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  dialCall(dialCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(sendChanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(recvChanHandle, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  onPackageStatusChangedCall(pkgHandle);

  readCall(dialHandle, readCallback);
  receiveEncPkgCall(recvBytes, recvConnId);

  writeCall(dialHandle, writeCallback);
  onPackageStatusChangedCall(pkgHandle2);

  closeCall(dialHandle, closeCallback);

  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status1, ApiStatus::OK);
  EXPECT_EQ(status2, ApiStatus::OK);
  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(status4, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvBytes);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerDialTestFixture, write_fail) {
  EXPECT_CALL(*core, getEntropy(_))
      .WillRepeatedly(Return(std::vector<uint8_t>(16, 1)));
  expectActivateSendChannel(sendChanHandle);
  expectActivateRecvChannel(recvChanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectSendPackage(pkgHandle2, sendConnId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  dialCall(dialCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(sendChanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(recvChanHandle, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  onPackageStatusChangedCall(pkgHandle);

  readCall(dialHandle, readCallback);
  receiveEncPkgCall(recvBytes, recvConnId);

  writeCall(dialHandle, writeCallback);
  manager.onPackageStatusChanged(*core->container, pkgHandle2,
                                 PACKAGE_FAILED_GENERIC);
  manager.waitForCallbacks();

  closeCall(dialHandle, closeCallback);

  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status1, ApiStatus::OK);
  EXPECT_EQ(status2, ApiStatus::OK);
  EXPECT_EQ(status3, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(status4, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvBytes);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerDialTestFixture, initial_package_fail) {
  EXPECT_CALL(*core, getEntropy(_))
      .WillRepeatedly(Return(std::vector<uint8_t>(16, 1)));
  expectActivateSendChannel(sendChanHandle);
  expectActivateRecvChannel(recvChanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  // expectSendPackage(pkgHandle2, sendConnId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  dialCall(dialCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(sendChanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(recvChanHandle, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  manager.onPackageStatusChanged(*core->container, pkgHandle,
                                 PACKAGE_FAILED_GENERIC);
  manager.waitForCallbacks();

  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status1, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerDialTestFixture, send_conn_fail) {
  EXPECT_CALL(*core, getEntropy(_))
      .WillRepeatedly(Return(std::vector<uint8_t>(16, 1)));
  expectActivateSendChannel(sendChanHandle);
  expectActivateRecvChannel(recvChanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  dialCall(dialCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(sendChanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(recvChanHandle, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  manager.onConnectionStatusChanged(*core->container, openSendConnHandle,
                                    sendConnId, CONNECTION_CLOSED, linkProps);
  manager.waitForCallbacks();

  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status1, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerDialTestFixture, recv_conn_fail) {
  EXPECT_CALL(*core, getEntropy(_))
      .WillRepeatedly(Return(std::vector<uint8_t>(16, 1)));
  expectActivateSendChannel(sendChanHandle);
  expectActivateRecvChannel(recvChanHandle);
  expectLoadLinkAddress(loadLinkHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);

  dialCall(dialCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  onChannelStatusChangedCall(sendChanHandle, sendOptions.send_channel);
  onChannelStatusChangedCall(recvChanHandle, sendOptions.recv_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  manager.onConnectionStatusChanged(*core->container, openRecvConnHandle,
                                    recvConnId, CONNECTION_CLOSED, linkProps);
  manager.waitForCallbacks();

  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);

  EXPECT_EQ(status1, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}