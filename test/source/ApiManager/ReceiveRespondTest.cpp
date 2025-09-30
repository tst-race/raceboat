
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

class TestableReceiveRespondManager : public ApiManager {
public:
  using ApiManager::ApiManager;
  using ApiManager::impl;
};

class ApiManagerReceiveRespondTestFixture : public ::testing::Test {
public:
  ApiManagerReceiveRespondTestFixture()
      : params(), core(std::make_shared<MockCore>()), manager(*core) {
    recvOptions.send_channel = "sendChannel";
    recvOptions.send_role = "sendRole";
    recvOptions.recv_channel = "recvChannel";
    recvOptions.recv_role = "recvRole";
    recvOptions.timeout_ms = 0;

    sendLinkId = "mockLinkId";
    recvLinkId = "mockLinkId2";
    sendConnId = "mockConnId";
    recvConnId = "mockConnId2";

    expectedSendAddress = "sendAddress";

    channelProps.bootstrap = false;
    channelProps.channelStatus = CHANNEL_UNDEF;
    channelProps.connectionType = CT_UNDEF;
    channelProps.maxCreatorsPerLoader = 1;
    channelProps.currentRole.roleName = recvOptions.send_role;
    channelProps.currentRole.linkSide = LS_UNDEF;
    channelProps.duration_s = 0;
    channelProps.intervalEndTime = 0;
    channelProps.isFlushable = true;
    channelProps.linkDirection = LD_LOADER_TO_CREATOR;
    channelProps.maxLoadersPerCreator = 1;

    sendBytes.reserve(0x100);
    recvMsg.reserve(0x100);
    for (uint16_t byte = 0; byte < 0x100; ++byte) {
      sendBytes.push_back(byte);
      recvMsg.push_back(0xFF - byte);
    }

    std::string dataB64 = base64::encode(recvMsg);
    nlohmann::json json = {
        {"linkAddress", expectedSendAddress},
        {"replyChannel", recvOptions.send_channel},
        {"message", dataB64},
    };
    std::string message = json.dump();
    recvBytes = std::vector<uint8_t>(message.begin(), message.end());

    linkProps.linkAddress = "linkAddr";
    linkProps.linkType = LT_BIDI;

    status1 = ApiStatus::INVALID;
    status2 = ApiStatus::INVALID;
    status3 = ApiStatus::INVALID;
    status4 = ApiStatus::INVALID;
    timesCalled = 0;

    getReceiverCallback = [this](ApiStatus _status, LinkAddress address,
                                 RaceHandle _handle) {
      status1 = _status;
      recvAddress = address;
      receiverHandle = _handle;
    };
    receiveCallback = [this](ApiStatus _status, std::vector<uint8_t> _bytes,
                             LinkAddress address) {
      status2 = _status;
      sendAddress = address;
      bytes2 = _bytes;
      timesCalled++;
    };
    closeCallback = [this](ApiStatus _status) { status3 = _status; };
    sendCallback = [this](ApiStatus _status) { status4 = _status; };
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

  void getReceiverCall(
      std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback);
  void receiveRespondCall(
      RaceHandle handle,
      std::function<void(ApiStatus, std::vector<uint8_t>, LinkAddress)>
          callback = {});
  void sendCall(std::function<void(ApiStatus)> callback,
                std::string send_address);

  void onChannelStatusChangedCall(RaceHandle handle, ChannelId channelId);
  void onLinkStatusChangedCall(RaceHandle handle, LinkID thisLinkId);
  void onConnectionStatusChangedCall(RaceHandle handle,
                                     ConnectionID thisConnId);
  void onPackageStatusChangedCall(RaceHandle handle);
  void receiveEncPkgCall(std::vector<uint8_t> &bytes, ConnectionID thisConnId);
  void closeCall(OpHandle handle, std::function<void(ApiStatus)> callback);
  void onConnectionStatusChangedClosedCall(RaceHandle handle,
                                           ConnectionID thisConnId);
  void onLinkStatusChangedDestroyedCall(RaceHandle handle, LinkID thisLinkId);

  ChannelParamStore params;
  std::shared_ptr<MockCore> core;
  TestableReceiveRespondManager manager;

  LinkProperties linkProps;
  ChannelProperties channelProps;
  ReceiveOptions recvOptions;

  LinkID sendLinkId;
  LinkID recvLinkId;
  ConnectionID sendConnId;
  ConnectionID recvConnId;

  LinkAddress expectedSendAddress;

  std::vector<uint8_t> sendBytes;
  std::vector<uint8_t> recvMsg;
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

  std::function<void(ApiStatus, LinkAddress, RaceHandle)> getReceiverCallback;
  std::function<void(ApiStatus, std::vector<uint8_t>, LinkAddress)>
      receiveCallback;
  std::function<void(ApiStatus)> closeCallback;
  std::function<void(ApiStatus)> sendCallback;

  int timesCalled;

  ApiStatus status1;
  LinkAddress recvAddress;
  RaceHandle receiverHandle;

  ApiStatus status2;
  LinkAddress sendAddress;

  ApiStatus status3;
  ApiStatus status4;
};

void ApiManagerReceiveRespondTestFixture::expectActivateSendChannel(
    RaceHandle &handle, ActivateChannelStatusCode thisStatus) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, recvOptions.send_channel, recvOptions.send_role))
      .WillOnce([&handle, thisStatus](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return thisStatus;
      });
}

void ApiManagerReceiveRespondTestFixture::expectActivateRecvChannel(
    RaceHandle &handle, ActivateChannelStatusCode thisStatus) {
  EXPECT_CALL(
      core->mockChannelManager,
      activateChannel(_, recvOptions.recv_channel, recvOptions.recv_role))
      .WillOnce([&handle, thisStatus](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return thisStatus;
      });
}

void ApiManagerReceiveRespondTestFixture::expectLoadLinkAddress(
    RaceHandle &handle, std::string _sendAddress) {
  // use member variable if argument is not specified
  _sendAddress = _sendAddress.empty() ? sendAddress : _sendAddress;
  EXPECT_CALL(*core->plugin,
              loadLinkAddress(_, recvOptions.send_channel, _sendAddress, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerReceiveRespondTestFixture::expectCreateLink(RaceHandle &handle) {
  EXPECT_CALL(*core->plugin, createLink(_, recvOptions.recv_channel, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerReceiveRespondTestFixture::expectOpenConnection(
    RaceHandle &handle, ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, openConnection(_, _, thisConnId, "{}", 0, 0, _))
      .WillOnce(
          [&handle](RaceHandle _handle, auto, auto, auto, auto, auto, auto) {
            handle = _handle;
            return SdkResponse(SDK_OK);
          });
}

void ApiManagerReceiveRespondTestFixture::expectSendPackage(
    RaceHandle &handle, ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, sendPackage(_, thisConnId, _, _, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerReceiveRespondTestFixture::expectCloseConnection(
    RaceHandle &handle, ConnectionID thisConnId) {
  EXPECT_CALL(*core->plugin, closeConnection(_, thisConnId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerReceiveRespondTestFixture::expectDestroyLink(RaceHandle &handle,
                                                            LinkID thisLinkId) {
  EXPECT_CALL(*core->plugin, destroyLink(_, thisLinkId, _))
      .WillOnce([&handle](RaceHandle _handle, auto, auto) {
        handle = _handle;
        return SdkResponse(SDK_OK);
      });
}

void ApiManagerReceiveRespondTestFixture::getReceiverCall(
    std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback) {
  manager.getReceiveObject(recvOptions, callback);
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::receiveRespondCall(
    RaceHandle handle,
    std::function<void(ApiStatus, std::vector<uint8_t>, LinkAddress)>
        callback) {
  // ReceiveOptions recvOptionsCopy = recvOptions;
  // recvOptionsCopy.recv_address = _recvAddress.empty() ?
  // recvOptionsCopy.recv_address : _recvAddress;
  manager.receiveRespond(handle, callback);
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::sendCall(
    std::function<void(ApiStatus)> callback, std::string send_address) {
  SendOptions sendOptions;
  sendOptions.send_channel = recvOptions.send_channel;
  sendOptions.send_role = recvOptions.send_role;
  sendOptions.send_address = send_address;
  manager.send(sendOptions, sendBytes, callback);
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::onChannelStatusChangedCall(
    RaceHandle handle, ChannelId channelId) {
  manager.onChannelStatusChanged(*core->container, handle, channelId,
                                 CHANNEL_AVAILABLE, channelProps);
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::onLinkStatusChangedCall(
    RaceHandle handle, LinkID thisLinkId) {
  manager.onLinkStatusChanged(*core->container, handle, thisLinkId, LINK_LOADED,
                              linkProps);
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::onConnectionStatusChangedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  manager.onConnectionStatusChanged(*core->container, handle, thisConnId,
                                    CONNECTION_OPEN, linkProps);
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::onPackageStatusChangedCall(
    RaceHandle handle) {
  manager.onPackageStatusChanged(*core->container, handle, PACKAGE_SENT);
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::receiveEncPkgCall(
    std::vector<uint8_t> &bytes, ConnectionID thisConnId) {
  EncPkg pkg(0, 0, bytes);
  manager.receiveEncPkg(*core->container, pkg, {thisConnId});
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::closeCall(
    OpHandle handle, std::function<void(ApiStatus)> callback) {
  manager.close(handle, callback);
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::onConnectionStatusChangedClosedCall(
    RaceHandle handle, ConnectionID thisConnId) {
  manager.onConnectionStatusChanged(*core->container, handle, thisConnId,
                                    CONNECTION_CLOSED, linkProps);
  manager.waitForCallbacks();
}

void ApiManagerReceiveRespondTestFixture::onLinkStatusChangedDestroyedCall(
    RaceHandle handle, LinkID thisLinkId) {
  manager.onLinkStatusChanged(*core->container, handle, thisLinkId,
                              LINK_DESTROYED, linkProps);
  manager.waitForCallbacks();
}

TEST_F(ApiManagerReceiveRespondTestFixture, no_errors) {
  recvOptions.recv_address = "";
  expectActivateRecvChannel(chanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  getReceiverCall(getReceiverCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);
  onChannelStatusChangedCall(chanHandle, recvOptions.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);

  receiveRespondCall(receiverHandle, receiveCallback);
  receiveEncPkgCall(recvBytes, recvConnId);

  closeCall(receiverHandle, closeCallback);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  RaceHandle chanHandle2;
  expectActivateSendChannel(chanHandle2);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);

  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  sendCall(sendCallback, sendAddress);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle2, sendConnId);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onPackageStatusChangedCall(pkgHandle);
  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status1, ApiStatus::OK);
  EXPECT_EQ(status2, ApiStatus::OK);
  EXPECT_EQ(sendAddress, expectedSendAddress);
  EXPECT_EQ(bytes2, recvMsg);
  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(status4, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerReceiveRespondTestFixture, multiple_channels) {
  recvOptions.multi_channel = true;
  recvOptions.recv_channel = "someChannel";
  recvOptions.send_channel = "someOtherChannel";
  recvOptions.recv_address = "";
  expectActivateRecvChannel(chanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  getReceiverCall(getReceiverCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);
  onChannelStatusChangedCall(chanHandle, recvOptions.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);

  receiveRespondCall(receiverHandle, receiveCallback);
  receiveEncPkgCall(recvBytes, recvConnId);

  closeCall(receiverHandle, closeCallback);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  RaceHandle chanHandle2;
  expectActivateSendChannel(chanHandle2);
  expectLoadLinkAddress(loadLinkHandle);
  expectOpenConnection(openSendConnHandle, sendLinkId);
  expectSendPackage(pkgHandle, sendConnId);
  expectCloseConnection(closeSendConnHandle, sendConnId);
  expectDestroyLink(destroySendLinkHandle, sendLinkId);

  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  sendCall(sendCallback, sendAddress);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);

  onChannelStatusChangedCall(chanHandle2, sendConnId);
  onLinkStatusChangedCall(loadLinkHandle, sendLinkId);
  onConnectionStatusChangedCall(openSendConnHandle, sendConnId);
  onPackageStatusChangedCall(pkgHandle);
  onConnectionStatusChangedClosedCall(closeSendConnHandle, sendConnId);
  onLinkStatusChangedDestroyedCall(destroySendLinkHandle, sendLinkId);
}

TEST_F(ApiManagerReceiveRespondTestFixture, wrapper_handles_error) {
  std::string message = "srctybu";
  recvBytes = std::vector<uint8_t>(message.begin(), message.end());

  recvOptions.recv_address = "";
  expectActivateRecvChannel(chanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);

  getReceiverCall(getReceiverCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);
  onChannelStatusChangedCall(chanHandle, recvOptions.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);

  receiveRespondCall(receiverHandle, receiveCallback);
  onConnectionStatusChangedClosedCall(NULL_RACE_HANDLE, recvConnId);

  manager.waitForCallbacks();
  EXPECT_EQ(status1, ApiStatus::OK);
  EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(timesCalled, 1);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerReceiveRespondTestFixture, malformed_received_message_error) {
  std::string message = "srctybu";
  recvBytes = std::vector<uint8_t>(message.begin(), message.end());

  recvOptions.recv_address = "";
  expectActivateRecvChannel(chanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  getReceiverCall(getReceiverCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);
  onChannelStatusChangedCall(chanHandle, recvOptions.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);

  receiveRespondCall(receiverHandle, receiveCallback);
  receiveEncPkgCall(recvBytes, recvConnId);

  closeCall(receiverHandle, closeCallback);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status1, ApiStatus::OK);
  EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerReceiveRespondTestFixture, json_missing_replyChannel_error) {
  std::string dataB64 = base64::encode(recvMsg);
  nlohmann::json json = {
      {"linkAddress", expectedSendAddress},
      {"message", dataB64},
  };
  std::string message = json.dump();
  recvBytes = std::vector<uint8_t>(message.begin(), message.end());

  recvOptions.recv_address = "";
  expectActivateRecvChannel(chanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  getReceiverCall(getReceiverCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);
  onChannelStatusChangedCall(chanHandle, recvOptions.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);

  receiveRespondCall(receiverHandle, receiveCallback);
  receiveEncPkgCall(recvBytes, recvConnId);

  closeCall(receiverHandle, closeCallback);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status1, ApiStatus::OK);
  EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerReceiveRespondTestFixture, json_missing_linkAddress_error) {
  std::string dataB64 = base64::encode(recvMsg);
  nlohmann::json json = {
      {"replyChannel", recvOptions.send_channel},
      {"message", dataB64},
  };
  std::string message = json.dump();
  recvBytes = std::vector<uint8_t>(message.begin(), message.end());

  recvOptions.recv_address = "";
  expectActivateRecvChannel(chanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  getReceiverCall(getReceiverCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);
  onChannelStatusChangedCall(chanHandle, recvOptions.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);

  receiveRespondCall(receiverHandle, receiveCallback);
  receiveEncPkgCall(recvBytes, recvConnId);

  closeCall(receiverHandle, closeCallback);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status1, ApiStatus::OK);
  EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerReceiveRespondTestFixture, json_missing_message_error) {
  nlohmann::json json = {
      {"linkAddress", expectedSendAddress},
      {"replyChannel", recvOptions.send_channel},
  };
  std::string message = json.dump();
  recvBytes = std::vector<uint8_t>(message.begin(), message.end());

  recvOptions.recv_address = "";
  expectActivateRecvChannel(chanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  getReceiverCall(getReceiverCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);
  onChannelStatusChangedCall(chanHandle, recvOptions.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);

  receiveRespondCall(receiverHandle, receiveCallback);
  receiveEncPkgCall(recvBytes, recvConnId);

  closeCall(receiverHandle, closeCallback);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status1, ApiStatus::OK);
  EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}

TEST_F(ApiManagerReceiveRespondTestFixture, invalid_recv_channel_error) {
  std::string dataB64 = base64::encode(recvMsg);
  nlohmann::json json = {
      {"linkAddress", expectedSendAddress},
      {"replyChannel", "invalid channel"},
      {"message", dataB64},
  };
  std::string message = json.dump();
  recvBytes = std::vector<uint8_t>(message.begin(), message.end());

  recvOptions.recv_address = "";
  expectActivateRecvChannel(chanHandle);
  expectCreateLink(createLinkHandle);
  expectOpenConnection(openRecvConnHandle, recvLinkId);
  expectCloseConnection(closeRecvConnHandle, recvConnId);
  expectDestroyLink(destroyRecvLinkHandle, recvLinkId);

  getReceiverCall(getReceiverCallback);
  EXPECT_EQ(manager.impl.activeContexts.size(), 2);
  onChannelStatusChangedCall(chanHandle, recvOptions.recv_channel);
  onLinkStatusChangedCall(createLinkHandle, recvLinkId);
  onConnectionStatusChangedCall(openRecvConnHandle, recvConnId);

  receiveRespondCall(receiverHandle, receiveCallback);
  receiveEncPkgCall(recvBytes, recvConnId);

  closeCall(receiverHandle, closeCallback);
  onConnectionStatusChangedClosedCall(closeRecvConnHandle, recvConnId);
  onLinkStatusChangedDestroyedCall(destroyRecvLinkHandle, recvLinkId);

  manager.waitForCallbacks();
  EXPECT_EQ(status1, ApiStatus::OK);
  EXPECT_EQ(status2, ApiStatus::INTERNAL_ERROR);
  EXPECT_EQ(status3, ApiStatus::OK);
  EXPECT_EQ(manager.impl.activeContexts.size(), 0);
  EXPECT_EQ(manager.impl.idContextMap.size(), 0);
  EXPECT_EQ(manager.impl.handleContextMap.size(), 0);
}
