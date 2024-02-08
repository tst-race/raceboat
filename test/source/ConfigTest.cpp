
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

#include <nlohmann/json.hpp>

#include "../../source/plugin-loading/Config.h"
#include "MockFileSystem.h"
#include "gtest/gtest.h"

#ifndef INPUT_FILE_DIR
#define INPUT_FILE_DIR "Unknown"
#endif

using namespace Raceboat;
using namespace testing;
using ::testing::_;

class ConfigTestable : public Config {
public:
  virtual bool parseJson(const fs::path &path, nlohmann::json &json) {
    return Config::parseJson(path, json);
  }
};

TEST(Config, manifest_parsing) {
  MockFileSystem fs(INPUT_FILE_DIR, std::make_unique<Storage>(Storage()));
  Config config;
  fs::path pluginsDir =
      fs::path(INPUT_FILE_DIR) / "plugins" / "myOS" / "myArch";
  std::vector<fs::path> pluginPaths = {pluginsDir / "examplePlugin1",
                                       pluginsDir / "examplePlugin2"};

  printf("plugin path: %s\n", pluginPaths[0].string().c_str());

  EXPECT_CALL(fs, listInstalledPluginDirs())
      .WillOnce(::testing::Return(pluginPaths));
  EXPECT_CALL(fs, makePluginInstallPath(fs::path("manifest.json"), _))
      .Times(2)
      .WillOnce(::testing::Return(pluginPaths[0] / "manifest.json"))
      .WillOnce(::testing::Return(pluginPaths[1] / "manifest.json"));

  config.parsePluginManifests(fs);

  EXPECT_EQ(config.manifests.size(), 2);
  std::vector<std::string> pluginNums = {"1", "2"};
  size_t manifestIx = 0;
  for (Config::PluginManifest &manifest : config.manifests) {
    std::string pluginName = "examplePlugin" + pluginNums[manifestIx];

    // verify top level, then drill down
    ASSERT_EQ(manifest.channelIdChannelPropsMap.size(), 5);
    ASSERT_EQ(manifest.compositions.size(), 1);
    ASSERT_EQ(manifest.plugins.size(), 2);
    ASSERT_EQ(manifest.channelParameters.size(), 6);

    // verify first plugin
    PluginDef &pluginDef = manifest.plugins.at(0);
    EXPECT_STREQ(pluginDef.filePath.c_str(), pluginName.c_str());
    EXPECT_EQ(pluginDef.fileType, RaceEnums::PFT_SHARED_LIB);
    EXPECT_STREQ(pluginDef.sharedLibraryPath.c_str(),
                 "libPluginCommsTwoSixStub.so");
    EXPECT_STREQ(pluginDef.pythonModule.c_str(), "");
    EXPECT_STREQ(pluginDef.pythonClass.c_str(), "");

    const char *expectedPluginChannels[] = {
        "twoSixBootstrapCpp", "twoSixDirectCpp", "twoSixIndirectCpp",
        "twoSixIndirectBootstrapCpp"};
    EXPECT_EQ(pluginDef.channels.size(), 4);
    for (size_t ix = 0; ix < 4; ix++) {
      EXPECT_STREQ(pluginDef.channels.at(ix).c_str(),
                   expectedPluginChannels[ix]);
    }

    // verify second plugin
    pluginDef = manifest.plugins.at(1);
    EXPECT_STRCASEEQ(pluginDef.filePath.c_str(), pluginName.c_str());
    EXPECT_EQ(pluginDef.fileType, RaceEnums::PFT_PYTHON);
    EXPECT_STREQ(pluginDef.sharedLibraryPath.c_str(), "");
    EXPECT_STREQ(pluginDef.pythonModule.c_str(),
                 "PluginDecomposedConjecture.doStuff");
    EXPECT_STREQ(pluginDef.pythonClass.c_str(), "doStuff.py");

    EXPECT_EQ(pluginDef.transports.size(), 2);
    EXPECT_STREQ(pluginDef.transports.begin()->c_str(), "twoSixIndirect");
    EXPECT_STREQ(pluginDef.transports[1].c_str(), "twoSixDirect");

    EXPECT_EQ(pluginDef.usermodels.size(), 2);
    EXPECT_STREQ(pluginDef.usermodels.begin()->c_str(), "model");
    EXPECT_STREQ(pluginDef.usermodels[1].c_str(), "model2");

    EXPECT_EQ(pluginDef.encodings.size(), 2);
    EXPECT_STREQ(pluginDef.encodings.begin()->c_str(), "base64");
    EXPECT_STREQ(pluginDef.encodings[1].c_str(), "base65");

    // verify channelGid
    const char *channelGids[] = {
        "twoSixBootstrapCpp", "twoSixDirectCpp", "twoSixIndirectBootstrapCpp",
        "twoSixIndirectComposition", "twoSixIndirectCpp"};
    size_t ix = 0;
    for (auto propsMap : manifest.channelIdChannelPropsMap) {
      EXPECT_STREQ(propsMap.first.c_str(), channelGids[ix++]);
    }

    // verify of twoSixDirectCpp channel props
    ChannelProperties &props =
        manifest.channelIdChannelPropsMap[channelGids[1]];
    EXPECT_EQ(props.bootstrap, false);
    EXPECT_STREQ(props.channelGid.c_str(), "twoSixDirectCpp");
    EXPECT_EQ(props.connectionType, CT_DIRECT);

    EXPECT_EQ(props.creatorExpected.send.bandwidth_bps, -1);
    EXPECT_EQ(props.creatorExpected.send.latency_ms, -1);
    EXPECT_FLOAT_EQ(props.creatorExpected.send.loss, -1.0);
    EXPECT_EQ(props.creatorExpected.receive.bandwidth_bps, 25700000);
    EXPECT_EQ(props.creatorExpected.receive.latency_ms, 16);
    EXPECT_FLOAT_EQ(props.creatorExpected.receive.loss, -1.0);

    EXPECT_EQ(props.duration_s, -1);
    EXPECT_EQ(props.linkDirection, LD_LOADER_TO_CREATOR);

    EXPECT_EQ(props.loaderExpected.send.bandwidth_bps, 25700000);
    EXPECT_EQ(props.loaderExpected.send.latency_ms, 16);
    EXPECT_FLOAT_EQ(props.loaderExpected.send.loss, -1.0);
    EXPECT_EQ(props.loaderExpected.receive.bandwidth_bps, -1);
    EXPECT_EQ(props.loaderExpected.receive.latency_ms, -1);
    EXPECT_FLOAT_EQ(props.loaderExpected.receive.loss, -1.0);

    EXPECT_EQ(props.mtu, -1);
    EXPECT_EQ(props.multiAddressable, false);
    EXPECT_EQ(props.period_s, -1);
    EXPECT_EQ(props.reliable, false);
    EXPECT_EQ(props.isFlushable, false);
    EXPECT_EQ(props.sendType, ST_EPHEM_SYNC);
    EXPECT_EQ(props.supported_hints.size(), 1);
    EXPECT_STREQ(props.supported_hints.at(0).c_str(), "hint");
    EXPECT_EQ(props.transmissionType, TT_UNICAST);
    EXPECT_EQ(props.maxLinks, 2000);
    EXPECT_EQ(props.creatorsPerLoader, -1);
    EXPECT_EQ(props.loadersPerCreator, -1);

    EXPECT_EQ(props.roles.size(), 1);
    EXPECT_EQ(props.roles[0].roleName, "default");
    EXPECT_EQ(props.roles[0].mechanicalTags.size(), 2);
    EXPECT_STREQ(props.roles[0].mechanicalTags[0].c_str(), "mechTag");
    EXPECT_STREQ(props.roles[0].mechanicalTags[1].c_str(), "mechTag2");
    EXPECT_EQ(props.roles[0].behavioralTags.size(), 2);
    EXPECT_STREQ(props.roles[0].behavioralTags[0].c_str(), "behaveTag");
    EXPECT_STREQ(props.roles[0].behavioralTags[1].c_str(), "behaveTag2");
    EXPECT_EQ(props.roles[0].linkSide, LS_BOTH);

    EXPECT_EQ(props.maxSendsPerInterval, -1);
    EXPECT_EQ(props.secondsPerInterval, -1);
    EXPECT_EQ(props.intervalEndTime, 0);
    EXPECT_EQ(props.sendsRemainingInInterval, -1);

    // verify compositions
    Composition &comp = manifest.compositions.at(0);
    EXPECT_STREQ(comp.id.c_str(), "twoSixIndirectComposition");
    EXPECT_STREQ(comp.transport.c_str(), "twoSixIndirect");
    EXPECT_STREQ(comp.usermodel.c_str(), "model");
    EXPECT_EQ(comp.encodings.size(), 2);
    EXPECT_STREQ(comp.encodings[0].c_str(), "base64");
    EXPECT_STREQ(comp.encodings[1].c_str(), "base65");

    // verify user response keys
    std::vector<std::string> expectedKeys = {
        "hostname" + pluginNums[manifestIx],
        "env" + pluginNums[manifestIx],
        "startPort" + pluginNums[manifestIx],
        "endPort" + pluginNums[manifestIx],
        "intVal" + pluginNums[manifestIx],
        "boolVal" + pluginNums[manifestIx],
        "floatVal" + pluginNums[manifestIx],
        "listSelectVal" + pluginNums[manifestIx],
    };

    // hostname
    size_t keyIx = 0;
    EXPECT_STREQ(expectedKeys[keyIx].c_str(),
                 manifest.channelParameters[keyIx].key.c_str());
    EXPECT_TRUE(manifest.channelParameters[keyIx].required);
    EXPECT_EQ(manifest.channelParameters[keyIx].valueType,
              Config::ChannelParameter::STRING);
    EXPECT_STREQ(manifest.channelParameters[keyIx].plugin.c_str(), "");

    // env
    keyIx++;
    EXPECT_STREQ(expectedKeys[keyIx].c_str(),
                 manifest.channelParameters[keyIx].key.c_str());
    EXPECT_EQ(manifest.channelParameters[keyIx].valueType,
              Config::ChannelParameter::STRING);
    EXPECT_FALSE(manifest.channelParameters[keyIx].required);

    // startPort
    keyIx++;
    EXPECT_STREQ(expectedKeys[keyIx].c_str(),
                 manifest.channelParameters[keyIx].key.c_str());
    EXPECT_EQ(manifest.channelParameters[keyIx].valueType,
              Config::ChannelParameter::STRING);
    EXPECT_FALSE(manifest.channelParameters[keyIx].required);

    // endPort
    keyIx++;
    EXPECT_STREQ(expectedKeys[keyIx].c_str(),
                 manifest.channelParameters[keyIx].key.c_str());
    EXPECT_FALSE(manifest.channelParameters[keyIx].required);
    EXPECT_STREQ("12345",
                 manifest.channelParameters[keyIx].defaultValue.c_str());
    EXPECT_EQ(manifest.channelParameters[keyIx].valueType,
              Config::ChannelParameter::INTEGER);

    // intVal
    keyIx++;
    EXPECT_STREQ(expectedKeys[keyIx].c_str(),
                 manifest.channelParameters[keyIx].key.c_str());
    EXPECT_TRUE(manifest.channelParameters[keyIx].required);
    EXPECT_EQ(manifest.channelParameters[keyIx].valueType,
              Config::ChannelParameter::INTEGER);

    // boolVal
    keyIx++;
    EXPECT_STREQ(expectedKeys[keyIx].c_str(),
                 manifest.channelParameters[keyIx].key.c_str());
    EXPECT_FALSE(manifest.channelParameters[keyIx].required);
    EXPECT_EQ(manifest.channelParameters[keyIx].valueType,
              Config::ChannelParameter::BOOLEAN);
    EXPECT_STREQ(manifest.channelParameters[keyIx].defaultValue.c_str(), "0");
    EXPECT_STREQ(manifest.channelParameters[keyIx].plugin.c_str(),
                 pluginName.c_str());

    manifestIx++;
  }
}
