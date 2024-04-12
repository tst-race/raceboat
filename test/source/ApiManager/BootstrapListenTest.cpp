
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
#include "base64.h"

using namespace Raceboat;
using namespace testing;
using testing::_;

class TestableBootstrapListenManager : public ApiManager {
public:
  using ApiManager::ApiManager;
  using ApiManager::impl;
};

class ApiManagerBootstrapListenTestFixture : public ::testing::Test {
public:
  ApiManagerBootstrapListenTestFixture()
      : params(), core(std::make_shared<MockCore>()), manager(*core) {
    options.init_send_channel = "sendChannel";
    options.init_send_role = "sendRole";
    options.init_recv_channel = "recvChannel";
    options.init_recv_role = "recvRole";
    options.final_send_channel = "sendChannel";
    options.final_send_role = "sendRole";
    options.final_recv_channel = "recvChannel";
    options.final_recv_role = "recvRole";
    options.timeout_ms = 0;

    sendLinkId = "mockLinkId";
    recvLinkId = "mockLinkId2";
    sendConnId = "mockConnId";
    recvConnId = "mockConnId2";

    expectedInitSendAddress = "initSendAddress";
    expectedFinalRecvAddress = "finalRecvAddress";
    expectedFinalSendAddress = "finalSendAddres";

    channelProps.bootstrap = false;
    channelProps.channelStatus = CHANNEL_UNDEF;
    channelProps.connectionType = CT_UNDEF;
    channelProps.creatorsPerLoader = 1;
    channelProps.currentRole.roleName = options.send_role;
    channelProps.currentRole.linkSide = LS_UNDEF;
    channelProps.duration_s = 0;
    channelProps.intervalEndTime = 0;
    channelProps.isFlushable = true;
    channelProps.linkDirection = LD_LOADER_TO_CREATOR;
    channelProps.loadersPerCreator = 1;

    sendBytes.reserve(0x100);
    recvMsg.reserve(0x100);
    for (uint16_t byte = 0; byte < 0x100; ++byte) {
      sendBytes.push_back(byte);
      recvMsg.push_back(0xFF - byte);
    }
    std::string dataB64 = base64::encode(recvMsg);
    nlohmann::json json = {
      {"initSendChannel", options.init_send_channel},
      {"initSendChannelLinkAddress", expectedInitSendAddress},
      {"finalRecvChannel", options.final_recv_channel},
      {"finalRecvChannelLinkAddress", expectedFinalRecvAddress},
      {"finalSendChannel", options.final_send_channel},
      {"finalSendChannelLinkAddress", expectedFinalSendAddress},
      {"packageId", "0123456789012345678901=="},
      {"message", dataB64},
    };
    std::string message = std::string(16, '\0') + json.dump();
    recvBytes = std::vector<uint8_t>(message.begin(), message.end());

    linkProps.linkAddress = "linkAddr";
    linkProps.linkType = LT_BIDI;

    status1 = ApiStatus::INVALID;
    listenCallback = [this](ApiStatus _status, LinkAddress _linkAddress,
                            RaceHandle _handle) {
      status1 = _status;
      linkAddress = _linkAddress;
      listenHandle = _handle;
    };

    status2 = ApiStatus::INVALID;
    acceptCallback = [this](ApiStatus _status, RaceHandle handle) {
      status2 = _status;
      acceptHandle = handle;
    };

    status3 = ApiStatus::INVALID;
    readCallback = [this](ApiStatus _status, std::vector<uint8_t> bytes) {
      status3 = _status;
      bytes2 = bytes;
    };

    status4 = ApiStatus::INVALID;
    writeCallback = [this](ApiStatus _status) { status4 = _status; };

    status5 = ApiStatus::INVALID;
    closeCallback = [this](ApiStatus _status) { status5 = _status; };
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
  listenCall(std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback);
  void acceptCall(std::function<void(ApiStatus, RaceHandle)> callback);
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
  TestableBootstrapListenManager manager;

  LinkProperties linkProps;
  ChannelProperties channelProps;
  ReceiveOptions options;

  LinkID sendLinkId;
  LinkID recvLinkId;
  ConnectionID sendConnId;
  ConnectionID recvConnId;

  LinkAddress expectedSendAddress;

  std::vector<uint8_t> sendBytes;
  std::vector<uint8_t> recvBytes;
  std::vector<uint8_t> recvMsg;

  RaceHandle sendChanHandle;
  RaceHandle recvChanHandle;
  RaceHandle loadLinkHandle;
  RaceHandle createLinkHandle;
  RaceHandle openSendConnHandle;
  RaceHandle openRecvConnHandle;
  RaceHandle pkgHandle;
  RaceHandle closeSendConnHandle;
  RaceHandle closeRecvConnHandle;
  RaceHandle destroySendLinkHandle;
  RaceHandle destroyRecvLinkHandle;

  ApiStatus status1;
  RaceHandle listenHandle;
  LinkAddress linkAddress;
  std::function<void(ApiStatus, LinkAddress, RaceHandle)> listenCallback;

  ApiStatus status2;
  RaceHandle acceptHandle;
  std::function<void(ApiStatus, RaceHandle)> acceptCallback;

  ApiStatus status3;
  std::vector<uint8_t> bytes2;
  std::function<void(ApiStatus, std::vector<uint8_t>)> readCallback;

  ApiStatus status4;
  std::function<void(ApiStatus)> writeCallback;

  ApiStatus status5;
  std::function<void(ApiStatus)> closeCallback;
};

void ApiManagerBootstrapListenTestFixture::expectActivateSendChannel(
    RaceHandle &handle, ActivateChannelStatusCode thisStatus) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, options.send_channel, options.send_role))
      .WillOnce([&handle, thisStatus](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return thisStatus;
      });
}

void ApiManagerBootstrapListenTestFixture::expectActivateRecvChannel(
    RaceHandle &handle, ActivateChannelStatusCode thisStatus) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, options.recv_channel, options.recv_role))
      .WillOnce([&handle, thisStatus](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return thisStatus;
      });
}

void ApiManagerBootstrapListenTestFixture::expectLoadLinkAddress(
    RaceHandle &handle, std::string send_address) {
  EXPECT_CALL(*core->plugin,
              loadLinkAddress(_, options.send_channel, send_address, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerBootstrapListenTestFixture::expectCreateLink(RaceHandle &handle) {
  EXPECT_CALL(*core->plugin, createLink(_, options.recv_channel, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerBootstrapListenTestFixture::expectOpenConnection(
    RaceHandle &handle, ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, openConnection(_, _, thisConnId, "{}", 0, 0, _))
      .WillOnce(
          [&handle](RaceHandle _handle, auto, auto, auto, auto, auto, auto) {
            handle = _handle;
            return SdkResponse(SDK_OK);
          });
}

void ApiManagerBootstrapListenTestFixture::expectSendPackage(RaceHandle &handle,
                                                    ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, sendPackage(_, thisConnId, _, _, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      })
      .RetiresOnSaturation(); // this allows multiple packages to be sent if we
                              // call expectSendPackage multiple times.
}

void ApiManagerBootstrapListenTestFixture::expectCloseConnection(
    RaceHandle &handle, ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, closeConnection(_, thisConnId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerBootstrapListenTestFixture::expectDestroyLink(RaceHandle &handle,
                                                    LinkID thisLinkId) {
  EXPECT_CALL(*core->plugin, destroyLink(_, thisLinkId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerBootstrapListenTestFixture::bootstrapListenCall(
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback) {
  manager.bootstrapListen(options, callback);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::acceptCall(
    std::function<void(ApiStatus, RaceHandle)> callback) {
  manager.accept(listenHandle, callback);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::readCall(
    RaceHandle handle,
    std::function<void(ApiStatus, std::vector<uint8_t>)> callback) {
  manager.read(handle, callback);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::writeCall(
    RaceHandle handle, std::function<void(ApiStatus)> callback) {
  manager.write(handle, sendBytes, callback);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::closeCall(
    RaceHandle handle, std::function<void(ApiStatus)> callback) {
  manager.close(handle, callback);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::onChannelStatusChangedCall(
    RaceHandle handle, ChannelId channelId) {
  manager.onChannelStatusChanged(*core->container, handle, channelId,
                                 CHANNEL_AVAILABLE, channelProps);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::onLinkStatusChangedCall(RaceHandle handle,
                                                          LinkID thisLinkId) {
  manager.onLinkStatusChanged(*core->container, handle, thisLinkId, LINK_LOADED,
                              linkProps);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::onConnectionStatusChangedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  manager.onConnectionStatusChanged(*core->container, handle, thisConnId,
                                    CONNECTION_OPEN, linkProps);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::onPackageStatusChangedCall(
    RaceHandle handle) {
  manager.onPackageStatusChanged(*core->container, handle, PACKAGE_SENT);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::receiveEncPkgCall(std::vector<uint8_t> &bytes,
                                                    ConnectionID thisConnId) {
  EncPkg pkg(0, 0, bytes);
  manager.receiveEncPkg(*core->container, pkg, {thisConnId});
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::onConnectionStatusChangedClosedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  manager.onConnectionStatusChanged(*core->container, handle, thisConnId,
                                    CONNECTION_CLOSED, linkProps);
  manager.waitForCallbacks();
}

void ApiManagerBootstrapListenTestFixture::onLinkStatusChangedDestroyedCall(
    RaceHandle handle, LinkID thisLinkId) {
  manager.onLinkStatusChanged(*core->container, handle, thisLinkId,
                              LINK_DESTROYED, linkProps);
  manager.waitForCallbacks();
}

TEST_F(ApiManagerBootstrapListenTestFixture, no_errors) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  expectActivateSendChannel(sendChanHandle);
  expectLoadLinkAddress(loadLinkHandle, expectedSendAddress);
  expectOpenConnection(openSendConnHandle, sendLinkId);

  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  EXPECT_EQ(status1, ApiStatus::OK);

  receiveEncPkgCall(recvBytes, recvConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            3); // recv conn, listen, pre conn obj
  acceptCall(acceptCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, pre conn obj, send conn

  onChannelStatusChangedCall(sendChanHandle, options.send_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, send conn, conn obj
  EXPECT_EQ(status2, ApiStatus::OK);

  receiveEncPkgCall(recvMsg, recvConnId);
  readCall(acceptHandle, readCallback);

  writeCall(acceptHandle, writeCallback);
  onPackageStatusChangedCall(pkgHandle);

  closeCall(listenHandle, closeCallback);
  EXPECT_EQ(status5, ApiStatus::OK);
  status5 = ApiStatus::INVALID;

  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  closeCall(acceptHandle, closeCallback);

  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(status4, ApiStatus::OK);
  EXPECT_EQ(status5, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvMsg);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerBootstrapListenTestFixture, no_errors_accept_before_recv) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  expectActivateSendChannel(sendChanHandle);
  expectLoadLinkAddress(loadLinkHandle, expectedSendAddress);
  expectOpenConnection(openSendConnHandle, sendLinkId);

  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  EXPECT_EQ(status1, ApiStatus::OK);

  acceptCall(acceptCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  receiveEncPkgCall(recvBytes, recvConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, send conn, pre conn obj

  onChannelStatusChangedCall(sendChanHandle, options.send_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, send conn, conn obj
  EXPECT_EQ(status2, ApiStatus::OK);

  receiveEncPkgCall(recvMsg, recvConnId);
  readCall(acceptHandle, readCallback);

  writeCall(acceptHandle, writeCallback);
  onPackageStatusChangedCall(pkgHandle);

  closeCall(listenHandle, closeCallback);
  EXPECT_EQ(status5, ApiStatus::OK);
  status5 = ApiStatus::INVALID;

  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  closeCall(acceptHandle, closeCallback);

  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(status4, ApiStatus::OK);
  EXPECT_EQ(status5, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvMsg);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerBootstrapListenTestFixture, no_errors_recv2_before_accept) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  expectActivateSendChannel(sendChanHandle);
  expectLoadLinkAddress(loadLinkHandle, expectedSendAddress);
  expectOpenConnection(openSendConnHandle, sendLinkId);

  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  EXPECT_EQ(status1, ApiStatus::OK);

  receiveEncPkgCall(recvBytes, recvConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            3); // recv conn, listen, pre conn obj

  receiveEncPkgCall(recvMsg, recvConnId);

  acceptCall(acceptCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, pre conn obj, send conn

  onChannelStatusChangedCall(sendChanHandle, options.send_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, send conn, conn obj
  EXPECT_EQ(status2, ApiStatus::OK);

  readCall(acceptHandle, readCallback);

  writeCall(acceptHandle, writeCallback);
  onPackageStatusChangedCall(pkgHandle);

  closeCall(listenHandle, closeCallback);
  EXPECT_EQ(status5, ApiStatus::OK);
  status5 = ApiStatus::INVALID;

  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  closeCall(acceptHandle, closeCallback);

  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(status4, ApiStatus::OK);
  EXPECT_EQ(status5, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvMsg);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerBootstrapListenTestFixture, no_errors_no_accept) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  EXPECT_EQ(status1, ApiStatus::OK);

  closeCall(listenHandle, closeCallback);
  EXPECT_EQ(status5, ApiStatus::OK);
  status5 = ApiStatus::INVALID;

  EXPECT_EQ(manager.impl.activeContexts.size(), 1); // recv conn

  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerBootstrapListenTestFixture, no_errors_recv_no_accept) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  EXPECT_EQ(status1, ApiStatus::OK);

  receiveEncPkgCall(recvBytes, recvConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            3); // recv conn, listen, pre conn obj
  closeCall(listenHandle, closeCallback);
  EXPECT_EQ(status5, ApiStatus::OK);
  status5 = ApiStatus::INVALID;

  EXPECT_EQ(manager.impl.activeContexts.size(), 1); // recv conn

  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerBootstrapListenTestFixture, no_errors_listener_close_before_read) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  expectActivateSendChannel(sendChanHandle);
  expectLoadLinkAddress(loadLinkHandle, expectedSendAddress);
  expectOpenConnection(openSendConnHandle, sendLinkId);

  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  EXPECT_EQ(status1, ApiStatus::OK);

  receiveEncPkgCall(recvBytes, recvConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            3); // recv conn, listen, pre conn obj
  acceptCall(acceptCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, pre conn obj, send conn

  onChannelStatusChangedCall(sendChanHandle, options.send_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, send conn, conn obj
  EXPECT_EQ(status2, ApiStatus::OK);

  closeCall(listenHandle, closeCallback);
  EXPECT_EQ(status5, ApiStatus::OK);
  status5 = ApiStatus::INVALID;

  receiveEncPkgCall(recvMsg, recvConnId);
  readCall(acceptHandle, readCallback);

  writeCall(acceptHandle, writeCallback);
  onPackageStatusChangedCall(pkgHandle);

  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  closeCall(acceptHandle, closeCallback);

  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(status4, ApiStatus::OK);
  EXPECT_EQ(status5, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvMsg);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerBootstrapListenTestFixture, write_fail) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  expectActivateSendChannel(sendChanHandle);
  expectLoadLinkAddress(loadLinkHandle, expectedSendAddress);
  expectOpenConnection(openSendConnHandle, sendLinkId);

  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  EXPECT_EQ(status1, ApiStatus::OK);

  receiveEncPkgCall(recvBytes, recvConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            3); // recv conn, listen, pre conn obj
  acceptCall(acceptCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, pre conn obj, send conn

  onChannelStatusChangedCall(sendChanHandle, options.send_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, send conn, conn obj
  EXPECT_EQ(status2, ApiStatus::OK);

  closeCall(listenHandle, closeCallback);
  EXPECT_EQ(status5, ApiStatus::OK);
  status5 = ApiStatus::INVALID;

  receiveEncPkgCall(recvMsg, recvConnId);
  readCall(acceptHandle, readCallback);

  writeCall(acceptHandle, writeCallback);
  manager.onPackageStatusChanged(*core->container, acceptHandle,
                                 PACKAGE_FAILED_GENERIC);
  manager.waitForCallbacks();

  EXPECT_EQ(manager.impl.activeContexts.size(), 3);

  closeCall(acceptHandle, closeCallback);

  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(status4, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(status5, ApiStatus::OK);
  EXPECT_EQ(bytes2, recvMsg);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerBootstrapListenTestFixture, open_send_conn_fail) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  expectActivateSendChannel(sendChanHandle);
  expectLoadLinkAddress(loadLinkHandle, expectedSendAddress);
  expectOpenConnection(openSendConnHandle, sendLinkId);

  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  EXPECT_EQ(status1, ApiStatus::OK);

  receiveEncPkgCall(recvBytes, recvConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            3); // recv conn, listen, pre conn obj
  acceptCall(acceptCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, pre conn obj, send conn

  onChannelStatusChangedCall(sendChanHandle, options.send_channel);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);

  manager.onConnectionStatusChanged(*core->container, openSendConnHandle,
                                    sendConnId, CONNECTION_CLOSED, linkProps);

  closeCall(listenHandle, closeCallback);

  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status5, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerBootstrapListenTestFixture, open_send_link_fail) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  expectActivateSendChannel(sendChanHandle);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);
  EXPECT_EQ(status1, ApiStatus::OK);

  receiveEncPkgCall(recvBytes, recvConnId);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            3); // recv conn, listen, pre conn obj
  acceptCall(acceptCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(),
            4); // recv conn, listen, pre conn obj, send conn

  manager.onChannelStatusChanged(*core->container, sendChanHandle,
                                 options.send_channel, CHANNEL_FAILED,
                                 channelProps);

  closeCall(listenHandle, closeCallback);

  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  EXPECT_EQ(status5, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerBootstrapListenTestFixture, open_recv_conn_fail) {
  expectActivateRecvChannel(recvChanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  listenCall(listenCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2); // recv conn, listen

  onChannelStatusChangedCall(recvChanHandle, options.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);

  manager.onConnectionStatusChanged(*core->container, openRecvConnHandle,
                                    recvConnId, CONNECTION_CLOSED, linkProps);
  manager.waitForCallbacks();
  EXPECT_EQ(status1, ApiStatus::INTERNAL_ERROR);

  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}
