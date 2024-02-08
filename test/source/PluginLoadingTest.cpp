
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

#include "MockCore.h"
#include "PluginWrapper.h"
#include "gtest/gtest.h"
#include "plugin-loading/PluginLoader.h"

#ifndef EXPECT_LOG_DIR
#define EXPECT_LOG_DIR "Unknown"
#endif

#ifndef UNIFIED_STUB
#define UNIFIED_STUB "Unknown"
#endif

#ifndef DECOMPOSED_ENCODING
#define DECOMPOSED_ENCODING "Unknown"
#endif

#ifndef DECOMPOSED_USERMODEL
#define DECOMPOSED_USERMODEL "Unknown"
#endif

#ifndef DECOMPOSED_TRANSPORT
#define DECOMPOSED_TRANSPORT "Unknown"
#endif

using namespace RaceLib;
using testing::_;
using testing::Return;

TEST(PluginLoading, test_unified) {
    // /code/raceboat/build/LINUX_x86_64/test/common/unified/libunified-test-stub.so
    auto stub_path = std::filesystem::path(UNIFIED_STUB);

    // /code/raceboat/build/LINUX_x86_64/test/common
    auto plugin_loader_path = stub_path.parent_path().parent_path();

    //                                                    unified
    auto stub_dir = stub_path.parent_path().filename();

    //                                                            libunified-test-stub.so
    auto stub_name = stub_path.filename();

    PluginDef def;
    def.fileType = RaceEnums::PFT_SHARED_LIB;
    def.filePath = stub_dir;
    def.sharedLibraryPath = stub_name;
    def.channels = {"stub channel"};

    Config::PluginManifest manifest;
    manifest.plugins.push_back(def);

    Config config;
    config.manifests.push_back(manifest);

    auto core = MockCore(plugin_loader_path, config);
    auto loader = IPluginLoader::construct(core);

    EXPECT_CALL(core.mockFS,
                makePluginInstallPath(fs::path("libunified-test-stub.so"), "UnifiedTestStub"))
        .WillOnce(Return(stub_path));
    EXPECT_CALL(core.mockFS, makePluginInstallPath(fs::path(""), "UnifiedTestStub"))
        .WillOnce(Return(stub_path.parent_path() / ""));
    auto container1 = loader->getChannel("stub channel");
    auto container2 = loader->getChannel("stub channel");

    ASSERT_NE(container1, nullptr);
    EXPECT_EQ(container1, container2);
    EXPECT_EQ(container1->plugin->shutdown(), true);
}

TEST(PluginLoading, test_decomposed) {
    // /code/raceboat/build/LINUX_x86_64/test/common/unified/libunified-test-stub.so
    auto transportPath = std::filesystem::path(DECOMPOSED_TRANSPORT);

    PluginDef transportDef;
    transportDef.fileType = RaceEnums::PFT_SHARED_LIB;
    transportDef.filePath = transportPath.parent_path().filename();
    transportDef.sharedLibraryPath = transportPath.filename();
    transportDef.transports = {"DecomposedTestTransport"};

    auto usermodelPath = std::filesystem::path(DECOMPOSED_USERMODEL);

    PluginDef usermodelDef;
    usermodelDef.fileType = RaceEnums::PFT_SHARED_LIB;
    usermodelDef.filePath = usermodelPath.parent_path().filename();
    usermodelDef.sharedLibraryPath = usermodelPath.filename();
    usermodelDef.usermodels = {"DecomposedTestUserModel"};

    auto encodingPath = std::filesystem::path(DECOMPOSED_ENCODING);

    PluginDef encodingDef;
    encodingDef.fileType = RaceEnums::PFT_SHARED_LIB;
    encodingDef.filePath = encodingPath.parent_path().filename();
    encodingDef.sharedLibraryPath = encodingPath.filename();
    encodingDef.encodings = {"DecomposedTestEncoding"};

    Composition composition;
    composition.id = "DecomposedTestStub";
    composition.encodings = {"DecomposedTestEncoding"};
    composition.usermodel = "DecomposedTestUserModel";
    composition.transport = "DecomposedTestTransport";

    Config::PluginManifest manifest;
    manifest.plugins.push_back(transportDef);
    manifest.plugins.push_back(usermodelDef);
    manifest.plugins.push_back(encodingDef);
    manifest.compositions.push_back(composition);

    Config config;
    config.manifests.push_back(manifest);

    // /code/raceboat/build/LINUX_x86_64/test/common
    auto plugin_loader_path = transportPath.parent_path().parent_path();
    auto core = MockCore(plugin_loader_path, config);
    EXPECT_CALL(core.mockFS, makePluginInstallPath(fs::path(""), "DecomposedTestStub"))
        .WillOnce(Return(transportPath.parent_path() / ""));
    EXPECT_CALL(core.mockFS, makePluginInstallPath(fs::path("libDecomposedTestTransport.so"),
                                                   "DecomposedTestStub"))
        .WillOnce(Return(transportPath.parent_path() / "libDecomposedTestTransport.so"));
    EXPECT_CALL(core.mockFS, makePluginInstallPath(fs::path("libDecomposedTestUserModel.so"),
                                                   "DecomposedTestStub"))
        .WillOnce(Return(transportPath.parent_path() / "libDecomposedTestUserModel.so"));
    EXPECT_CALL(core.mockFS, makePluginInstallPath(fs::path("libDecomposedTestEncoding.so"),
                                                   "DecomposedTestStub"))
        .WillOnce(Return(transportPath.parent_path() / "libDecomposedTestEncoding.so"));

    auto loader = IPluginLoader::construct(core);

    auto container1 = loader->getChannel("DecomposedTestStub");
    auto container2 = loader->getChannel("DecomposedTestStub");

    ASSERT_NE(container1, nullptr);
    EXPECT_EQ(container1, container2);

    // components aren't actually loaded until activateChannel is called
    EXPECT_CALL(core, onChannelStatusChanged(_, _, _, _, _));
    EXPECT_CALL(core, asyncError(_, _, _)).Times(0);
    auto response = container1->plugin->activateChannel(42, "DecomposedTestStub", "default", 0);
    container1->plugin->waitForCallbacks();
    EXPECT_EQ(response.status, SDK_OK);
    EXPECT_EQ(container1->plugin->shutdown(), true);
}
