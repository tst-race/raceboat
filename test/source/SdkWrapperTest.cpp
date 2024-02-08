
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

#include "SdkWrapper.h"
#include "../common/MockCore.h"
#include "../common/MockPluginWrapper.h"
#include "../common/race_printers.h"
#include "common/EncPkg.h"
#include "gtest/gtest.h"

using testing::_;
using testing::Return;

TEST(SdkWrapperTest, getEntropy) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::vector<uint8_t> bytes = {16, 17, 18, 19, 20, 21, 22, 23,
                                24, 25, 26, 27, 28, 29, 30, 31};
  EXPECT_CALL(core, getEntropy(16)).WillOnce(Return(bytes));
  auto ret = wrapper.getEntropy(16);
  EXPECT_EQ(ret, bytes);
}

TEST(SdkWrapperTest, getActivePersona) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string persona = "test persona";
  EXPECT_CALL(core, getActivePersona(_)).WillOnce(Return(persona));
  auto ret = wrapper.getActivePersona();
  EXPECT_EQ(ret, persona);
}

TEST(SdkWrapperTest, asyncError) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  RaceHandle handle = 42;
  PluginResponse status = PLUGIN_FATAL;
  EXPECT_CALL(core, asyncError(_, handle, status)).WillOnce(Return(SDK_OK));
  auto ret = wrapper.asyncError(handle, status);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, getChannelProperties) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string channelId = "test channelId";
  ChannelProperties props;
  props.channelGid = channelId;
  EXPECT_CALL(core.mockChannelManager, getChannelProperties(channelId))
      .WillOnce(Return(props));
  auto ret = wrapper.getChannelProperties(channelId);
  EXPECT_EQ(ret, props);
}

TEST(SdkWrapperTest, getAllChannelProperties) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string channelId = "test channelId";
  ChannelProperties props;
  props.channelGid = channelId;
  std::vector<ChannelProperties> allProps = {props};
  EXPECT_CALL(core.mockChannelManager, getAllChannelProperties())
      .WillOnce(Return(allProps));
  auto ret = wrapper.getAllChannelProperties();
  EXPECT_EQ(ret, allProps);
}

TEST(SdkWrapperTest, makeDir) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  EXPECT_CALL(core.mockFS, makeDir(fs::path(path), container.id))
      .WillOnce(Return(true));
  auto ret = wrapper.makeDir(path);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, makeDir_invalid) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  EXPECT_CALL(core.mockFS, makeDir(fs::path(path), container.id))
      .WillOnce(Return(false));
  auto ret = wrapper.makeDir(path);
  EXPECT_EQ(ret.status, SDK_INVALID_ARGUMENT);
}

TEST(SdkWrapperTest, removeDir) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  EXPECT_CALL(core.mockFS, removeDir(fs::path(path), container.id))
      .WillOnce(Return(true));
  auto ret = wrapper.removeDir(path);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, removeDir_invalid) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  EXPECT_CALL(core.mockFS, removeDir(fs::path(path), container.id))
      .WillOnce(Return(false));
  auto ret = wrapper.removeDir(path);
  EXPECT_EQ(ret.status, SDK_INVALID_ARGUMENT);
}

TEST(SdkWrapperTest, listDir) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  auto contents = std::vector<std::string>{"a", "b", "c"};
  EXPECT_CALL(core.mockFS, listDir(fs::path(path), container.id))
      .WillOnce(Return(contents));
  auto ret = wrapper.listDir(path);
  EXPECT_EQ(ret, contents);
}

TEST(SdkWrapperTest, readFile) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  std::vector<uint8_t> bytes = {16, 17, 18, 19, 20, 21, 22, 23,
                                24, 25, 26, 27, 28, 29, 30, 31};
  EXPECT_CALL(core.mockFS, readFile(fs::path(path), container.id))
      .WillOnce(Return(bytes));
  auto ret = wrapper.readFile(path);
  EXPECT_EQ(ret, bytes);
}

TEST(SdkWrapperTest, appendFile) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  std::vector<uint8_t> bytes = {16, 17, 18, 19, 20, 21, 22, 23,
                                24, 25, 26, 27, 28, 29, 30, 31};
  EXPECT_CALL(core.mockFS, appendFile(fs::path(path), container.id, bytes))
      .WillOnce(Return(true));
  auto ret = wrapper.appendFile(path, bytes);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, appendFile_invalid) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  std::vector<uint8_t> bytes = {16, 17, 18, 19, 20, 21, 22, 23,
                                24, 25, 26, 27, 28, 29, 30, 31};
  EXPECT_CALL(core.mockFS, appendFile(fs::path(path), container.id, bytes))
      .WillOnce(Return(false));
  auto ret = wrapper.appendFile(path, bytes);
  EXPECT_EQ(ret.status, SDK_INVALID_ARGUMENT);
}

TEST(SdkWrapperTest, writeFile) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  std::vector<uint8_t> bytes = {16, 17, 18, 19, 20, 21, 22, 23,
                                24, 25, 26, 27, 28, 29, 30, 31};
  EXPECT_CALL(core.mockFS, writeFile(fs::path(path), container.id, bytes))
      .WillOnce(Return(true));
  auto ret = wrapper.writeFile(path, bytes);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, writeFile_invalid) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string path = "test path";
  std::vector<uint8_t> bytes = {16, 17, 18, 19, 20, 21, 22, 23,
                                24, 25, 26, 27, 28, 29, 30, 31};
  EXPECT_CALL(core.mockFS, writeFile(fs::path(path), container.id, bytes))
      .WillOnce(Return(false));
  auto ret = wrapper.writeFile(path, bytes);
  EXPECT_EQ(ret.status, SDK_INVALID_ARGUMENT);
}

TEST(SdkWrapperTest, requestPluginUserInput) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  container.sdk = std::make_unique<SdkWrapper>(container, core);
  container.plugin = std::make_unique<MockPluginWrapper>(container);
  MockPluginWrapper *plugin =
      dynamic_cast<MockPluginWrapper *>(container.plugin.get());

  std::string key = "test key";
  std::string prompt = "test prompt";
  bool cache = true;
  std::optional<std::string> response = "test response";
  EXPECT_CALL(core.mockUserInput, getPluginUserInput(container.id, key))
      .WillOnce(Return(response));
  EXPECT_CALL(*plugin, onUserInputReceived(_, true, *response, _))
      .WillOnce(Return(SDK_OK));
  auto ret = container.sdk->requestPluginUserInput(key, prompt, cache);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, requestPluginUserInput_invalid) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  container.sdk = std::make_unique<SdkWrapper>(container, core);
  container.plugin = std::make_unique<MockPluginWrapper>(container);
  MockPluginWrapper *plugin =
      dynamic_cast<MockPluginWrapper *>(container.plugin.get());

  std::string key = "test key";
  std::string prompt = "test prompt";
  bool cache = true;
  std::optional<std::string> response = std::nullopt;
  EXPECT_CALL(core.mockUserInput, getPluginUserInput(container.id, key))
      .WillOnce(Return(response));
  EXPECT_CALL(*plugin, onUserInputReceived(_, false, "", _))
      .WillOnce(Return(SDK_OK));
  auto ret = container.sdk->requestPluginUserInput(key, prompt, cache);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, requestCommonUserInput) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  container.sdk = std::make_unique<SdkWrapper>(container, core);
  container.plugin = std::make_unique<MockPluginWrapper>(container);
  MockPluginWrapper *plugin =
      dynamic_cast<MockPluginWrapper *>(container.plugin.get());

  std::string key = "test key";
  std::optional<std::string> response = "test response";
  EXPECT_CALL(core.mockUserInput, getCommonUserInput(key))
      .WillOnce(Return(response));
  EXPECT_CALL(*plugin, onUserInputReceived(_, true, *response, _))
      .WillOnce(Return(SDK_OK));
  auto ret = container.sdk->requestCommonUserInput(key);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, requestCommonUserInput_invalid) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  container.sdk = std::make_unique<SdkWrapper>(container, core);
  container.plugin = std::make_unique<MockPluginWrapper>(container);
  MockPluginWrapper *plugin =
      dynamic_cast<MockPluginWrapper *>(container.plugin.get());

  std::string key = "test key";
  std::optional<std::string> response = std::nullopt;
  EXPECT_CALL(core.mockUserInput, getCommonUserInput(key))
      .WillOnce(Return(response));
  EXPECT_CALL(*plugin, onUserInputReceived(_, false, "", _))
      .WillOnce(Return(SDK_OK));
  auto ret = container.sdk->requestCommonUserInput(key);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, displayInfoToUser) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);
  auto ret = wrapper.displayInfoToUser({}, {});
  EXPECT_EQ(ret.status, SDK_INVALID);
}

TEST(SdkWrapperTest, displayBootstrapInfoToUser) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);
  auto ret = wrapper.displayBootstrapInfoToUser({}, {}, {});
  EXPECT_EQ(ret.status, SDK_INVALID);
}

TEST(SdkWrapperTest, unblockQueue) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  container.sdk = std::make_unique<SdkWrapper>(container, core);
  container.plugin = std::make_unique<MockPluginWrapper>(container);
  MockPluginWrapper *plugin =
      dynamic_cast<MockPluginWrapper *>(container.plugin.get());

  ConnectionID connId = "conn";
  EXPECT_CALL(*plugin, unblockQueue(connId)).WillOnce(Return(SDK_OK));
  auto ret = container.sdk->unblockQueue(connId);
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, onPackageStatusChanged) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  EXPECT_CALL(core, onPackageStatusChanged(_, _, _)).WillOnce(Return(SDK_OK));
  auto ret = wrapper.onPackageStatusChanged({}, {}, {});
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, onConnectionStatusChanged) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  container.sdk = std::make_unique<SdkWrapper>(container, core);
  container.plugin = std::make_unique<MockPluginWrapper>(container);
  MockPluginWrapper *plugin =
      dynamic_cast<MockPluginWrapper *>(container.plugin.get());

  EXPECT_CALL(*plugin, onConnectionStatusChanged(_, _));
  EXPECT_CALL(core, onConnectionStatusChanged(_, _, _, _, _))
      .WillOnce(Return(SDK_OK));
  auto ret = container.sdk->onConnectionStatusChanged({}, {}, {}, {}, {});
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, onLinkStatusChanged) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  EXPECT_CALL(core, onLinkStatusChanged(_, _, _, _, _))
      .WillOnce(Return(SDK_OK));
  auto ret = wrapper.onLinkStatusChanged({}, {}, {}, {}, {});
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, onChannelStatusChanged) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  EXPECT_CALL(core, onChannelStatusChanged(_, _, _, _, _))
      .WillOnce(Return(SDK_OK));
  auto ret = wrapper.onChannelStatusChanged({}, {}, {}, {}, {});
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, updateLinkProperties) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  EXPECT_CALL(core, updateLinkProperties(_, _, _)).WillOnce(Return(SDK_OK));
  auto ret = wrapper.updateLinkProperties({}, {}, {});
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, receiveEncPkg) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  EXPECT_CALL(core, receiveEncPkg(_, _, _)).WillOnce(Return(SDK_OK));
  auto ret = wrapper.receiveEncPkg(EncPkg{{}}, {}, {});
  EXPECT_EQ(ret.status, SDK_OK);
}

TEST(SdkWrapperTest, generateConnectionId) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string linkId = "test linkId";
  std::string connId = "test connId";
  EXPECT_CALL(core, generateConnectionId(_, linkId)).WillOnce(Return(connId));
  auto ret = wrapper.generateConnectionId(linkId);
  EXPECT_EQ(ret, connId);
}

TEST(SdkWrapperTest, generateLinkId) {
  MockCore core("<plugin path>");

  PluginContainer container;
  container.id = "MockContainer";

  SdkWrapper wrapper(container, core);

  std::string channelId = "test channelId";
  std::string linkId = "test linkId";
  EXPECT_CALL(core, generateLinkId(_, channelId)).WillOnce(Return(linkId));
  auto ret = wrapper.generateLinkId(channelId);
  EXPECT_EQ(ret, linkId);
}