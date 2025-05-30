
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

#include <Composition.h>
#include <DecomposedPluginLoader.h>
#include <PluginDef.h>
#include <gmock/gmock.h>
#include <MockFileSystem.h>
#include <MockEncodingSdk.h>
#include <MockTransportSdk.h>
#include <MockUserModelSdk.h>

#ifndef PLUGIN_PATH
#pragma GCC error "Need Plugin Path from cmake"
#endif

using namespace Raceboat;

class PythonCommsDecompositionTextFixture : public ::testing::Test {
public:

    PythonCommsDecompositionTextFixture() : fs(PLUGIN_PATH), decomposedPluginLoader(fs) {
        RaceLog::setLogLevelStdout(RaceLog::LL_DEBUG);
        PluginDef transportPluginDef, encodingPluginDef, userModelPluginDef;

        transportPluginDef.filePath = "stubs";
        transportPluginDef.fileType = RaceEnums::PluginFileType::PFT_PYTHON;
        transportPluginDef.pythonModule = "TransportStub.TransportStub";
        transportPluginDef.channels = std::vector<std::string>{};
        transportPluginDef.usermodels = std::vector<std::string>{};
        transportPluginDef.transports = std::vector<std::string>{"twoSixStubTransportPy"};
        transportPluginDef.encodings = std::vector<std::string>{};

        encodingPluginDef.filePath = "stubs";
        encodingPluginDef.fileType = RaceEnums::PluginFileType::PFT_PYTHON;
        encodingPluginDef.pythonModule = "EncodingStub.EncodingStub";
        encodingPluginDef.channels = std::vector<std::string>{};
        encodingPluginDef.usermodels = std::vector<std::string>{};
        encodingPluginDef.transports = std::vector<std::string>{};
        encodingPluginDef.encodings = std::vector<std::string>{"twoSixStubEncodingPy"};

        userModelPluginDef.filePath = "stubs";
        userModelPluginDef.fileType = RaceEnums::PluginFileType::PFT_PYTHON;
        userModelPluginDef.pythonModule = "UserModelStub.UserModelStub";
        userModelPluginDef.channels = std::vector<std::string>{};
        userModelPluginDef.usermodels = std::vector<std::string>{"twoSixStubUserModelPy"};
        userModelPluginDef.transports = std::vector<std::string>{};
        userModelPluginDef.encodings = std::vector<std::string>{};

        decomposed_plugins.push_back(transportPluginDef);
        decomposed_plugins.push_back(encodingPluginDef);
        decomposed_plugins.push_back(userModelPluginDef);

    };

    void createComponents() {
        PluginConfig pluginConfig;
        pluginConfig.tmpDirectory = "/tmp";

        decomposedPluginLoader.loadComponents(decomposed_plugins);

        auto encPlugin = decomposedPluginLoader.encodings.at("twoSixStubEncodingPy");
        encoding = std::shared_ptr<IEncodingComponent>(encPlugin->createEncoding(
            "twoSixStubEncodingPy", &encodingSdk, "roleName", pluginConfig));

        auto transportPlugin = decomposedPluginLoader.transports.at("twoSixStubTransportPy");
        transport = std::shared_ptr<ITransportComponent>(transportPlugin->createTransport(
            "twoSixStubTransportPy", &transportSdk, "roleName", pluginConfig));

        auto userModelPlugin = decomposedPluginLoader.usermodels.at("twoSixStubUserModelPy");
        userModel = std::shared_ptr<IUserModelComponent>(userModelPlugin->createUserModel(
            "twoSixStubUserModelPy", &userModelSdk, "roleName", pluginConfig));
    }

    void destroyComponents() {

        if (encoding) {
            encoding.reset();
        }
        if (transport) {
            transport.reset();
        }
        if (userModel) {
            userModel.reset();
        }
    }

    virtual ~PythonCommsDecompositionTextFixture(){};

    virtual void SetUp() override {
        createComponents();
    }

    virtual void TearDown() override {
        destroyComponents();
    }

    MockFileSystem fs;

    MockEncodingSdk encodingSdk;
    MockTransportSdk transportSdk;
    MockUserModelSdk userModelSdk;
    std::vector<PluginDef> decomposed_plugins;
    DecomposedPluginLoader decomposedPluginLoader;

    std::shared_ptr<IEncodingComponent> encoding = nullptr;
    std::shared_ptr<ITransportComponent> transport = nullptr;
    std::shared_ptr<IUserModelComponent> userModel = nullptr;
};

// Comms tests

TEST_F(PythonCommsDecompositionTextFixture, componentLoading) {
    EXPECT_FALSE(decomposedPluginLoader.encodings.empty());
    EXPECT_FALSE(decomposedPluginLoader.usermodels.empty());
    EXPECT_FALSE(decomposedPluginLoader.transports.empty());
}

// Encoding

TEST_F(PythonCommsDecompositionTextFixture, encoding_createEncoding) {
    TRACE_FUNCTION();
    EXPECT_TRUE(encoding != nullptr);
}

TEST_F(PythonCommsDecompositionTextFixture, encoding_getEncodingProperties) {
    TRACE_FUNCTION();

    EncodingProperties prop = encoding->getEncodingProperties();
    EXPECT_EQ(prop.encodingTime, 1010101);
    EXPECT_EQ(prop.type, "text/plain");
}

TEST_F(PythonCommsDecompositionTextFixture, encoding_encodeBytes) {
    TRACE_FUNCTION();

    RaceHandle handle = 1;
    EncodingParameters params;
    const std::vector<uint8_t> bytes = {0x01, 0x02, 0x03};
    params.linkId = "linkID_1";
    params.type = "application/octet-stream";
    params.encodePackage = false;
    params.json = "{}";

    EXPECT_CALL(encodingSdk, onBytesEncoded(0, bytes, EncodingStatus::ENCODE_OK));

    ComponentStatus status = encoding->encodeBytes(handle, params, bytes);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, encoding_decodeBytes) {
    TRACE_FUNCTION();

    RaceHandle handle = 2;
    EncodingParameters params;
    const std::vector<uint8_t> bytes = {0x01, 0x02, 0x03};
    params.linkId = "linkID_2";
    params.type = "application/octet-stream";
    params.encodePackage = false;
    params.json = "{}";

    EXPECT_CALL(encodingSdk, onBytesDecoded(0, bytes, EncodingStatus::ENCODE_OK));

    ComponentStatus status = encoding->decodeBytes(handle, params, bytes);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, encoding_onUserInputReceived) {
    TRACE_FUNCTION();

    RaceHandle handle = 3;
    const std::string response = "response";

    ComponentStatus status = encoding->onUserInputReceived(handle, true, response);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

// Transport

TEST_F(PythonCommsDecompositionTextFixture, transport_createTransport) {
    TRACE_FUNCTION();
    EXPECT_TRUE(transport != nullptr);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_getTransportProperties) {
    TRACE_FUNCTION();

    TransportProperties prop = transport->getTransportProperties();

    EXPECT_TRUE(prop.supportedActions.at("action1").size() == 3 &&
                prop.supportedActions.at("action1").back() == "image/*");
}

TEST_F(PythonCommsDecompositionTextFixture, transport_getLinkProperties) {
    TRACE_FUNCTION();

    EXPECT_CALL(transportSdk, getChannelProperties());

    const LinkID link = "link_1";
    LinkProperties prop = transport->getLinkProperties(link);
    EXPECT_EQ(prop.linkType, LT_BIDI);
    EXPECT_EQ(prop.transmissionType, TT_MULTICAST);
    EXPECT_EQ(prop.connectionType, CT_LOCAL);
    EXPECT_EQ(prop.sendType, ST_EPHEM_SYNC);
    EXPECT_EQ(prop.reliable, true);
    EXPECT_EQ(prop.isFlushable, true);
    EXPECT_EQ(prop.duration_s, 10101);
    EXPECT_EQ(prop.period_s, 20202);
    EXPECT_EQ(prop.mtu, 30303);

    EXPECT_EQ(prop.worst.send.bandwidth_bps, 101);
    EXPECT_EQ(prop.worst.send.latency_ms, 202);
    EXPECT_EQ(prop.worst.send.loss, 0.5);
    EXPECT_EQ(prop.expected.send.bandwidth_bps, 101);
    EXPECT_EQ(prop.expected.send.latency_ms, 202);
    EXPECT_EQ(prop.expected.send.loss, 0.5);
    EXPECT_EQ(prop.best.send.bandwidth_bps, 101);
    EXPECT_EQ(prop.best.send.latency_ms, 202);
    EXPECT_EQ(prop.best.send.loss, 0.5);

    EXPECT_EQ(prop.worst.receive.bandwidth_bps, 101);
    EXPECT_EQ(prop.worst.receive.latency_ms, 202);
    EXPECT_EQ(prop.worst.receive.loss, 0.5);
    EXPECT_EQ(prop.expected.receive.bandwidth_bps, 101);
    EXPECT_EQ(prop.expected.receive.latency_ms, 202);
    EXPECT_EQ(prop.expected.receive.loss, 0.5);
    EXPECT_EQ(prop.best.receive.bandwidth_bps, 101);
    EXPECT_EQ(prop.best.receive.latency_ms, 202);
    EXPECT_EQ(prop.best.receive.loss, 0.5);

    EXPECT_EQ(prop.supported_hints.size(), 1);
    EXPECT_EQ(prop.supported_hints.front(), "hint1");
    EXPECT_EQ(prop.channelGid, "mockChannel");
    EXPECT_EQ(prop.linkAddress, "mockLinkAddress");
}

TEST_F(PythonCommsDecompositionTextFixture, transport_createLink) {
    TRACE_FUNCTION();

    RaceHandle handle = 1;
    const LinkID link = "link_1";

    EXPECT_CALL(transportSdk,
                onLinkStatusChanged(handle, link, LinkStatus::LINK_CREATED, testing::_));

    ComponentStatus status = transport->createLink(handle, link);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_loadLinkAddress) {
    TRACE_FUNCTION();

    RaceHandle handle = 1;
    const LinkID link = "link_1";
    const std::string linkAddr = "linkAddress_1";

    ComponentStatus status = transport->loadLinkAddress(handle, link, linkAddr);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_loadLinkAddresses) {
    TRACE_FUNCTION();

    RaceHandle handle = 1;
    const LinkID link = "link_1";
    const std::vector<std::string> linkAddrs = {"linkAddress_1", "linkAddress_2"};

    ComponentStatus status = transport->loadLinkAddresses(handle, link, linkAddrs);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_createLinkFromAddress) {
    TRACE_FUNCTION();

    RaceHandle handle = 1;
    const LinkID link = "link_1";
    const std::string linkAddr = "linkAddress_1";

    ComponentStatus status = transport->createLinkFromAddress(handle, link, linkAddr);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_destroyLink) {
    TRACE_FUNCTION();

    RaceHandle handle = 1;
    const LinkID link = "link_1";

    ComponentStatus status = transport->destroyLink(handle, link);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_getActionParams) {
    TRACE_FUNCTION();

    const Action action = {1, 0x10, "{}"};
    auto actionParams = transport->getActionParams(action);
    EXPECT_EQ(actionParams.size(), 1);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_enqueueContent) {
    TRACE_FUNCTION();

    EncodingParameters params = {};
    params.linkId = "link_1";
    const Action action = {1, 0x10, "{}"};
    const std::vector<uint8_t> content = {0x01, 0x02, 0x03};

    EXPECT_CALL(transportSdk, onReceive("link_1", testing::_, testing::_));

    ComponentStatus status = transport->enqueueContent(params, action, content);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_dequeueContent) {
    TRACE_FUNCTION();

    EXPECT_CALL(transportSdk, onPackageStatusChanged(1, PackageStatus::PACKAGE_SENT));

    const Action action = {1, 0x10, "{}"};
    ComponentStatus status = transport->dequeueContent(action);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_doAction) {
    TRACE_FUNCTION();

    const std::vector<RaceHandle> handles{1, 2, 3};
    const Action action = {1, 0x10, "{}"};

    ComponentStatus status = transport->doAction(handles, action);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, transport_onUserInputReceived) {
    TRACE_FUNCTION();

    RaceHandle handle = 3;
    const std::string response = "response";

    EXPECT_CALL(transportSdk, onEvent(testing::_));

    ComponentStatus status = transport->onUserInputReceived(handle, true, response);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

// User Model

TEST_F(PythonCommsDecompositionTextFixture, um_getUserModelProperties) {
    TRACE_FUNCTION();

    UserModelProperties expected;

    UserModelProperties prop = userModel->getUserModelProperties();
    EXPECT_TRUE(typeid(prop) == typeid(expected));
}

TEST_F(PythonCommsDecompositionTextFixture, um_addLink) {
    TRACE_FUNCTION();

    EXPECT_CALL(userModelSdk, onTimelineUpdated());

    LinkID linkId = "link_1";
    LinkParameters params = {"{}"};

    ComponentStatus status = userModel->addLink(linkId, params);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, um_removeLink) {
    TRACE_FUNCTION();

    LinkID linkId = "link_1";

    ComponentStatus status = userModel->removeLink(linkId);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, um_getTimeline) {
    TRACE_FUNCTION();

    Timestamp start = 1000000.0;
    Timestamp end = 2000000.0;

    ActionTimeline tl = userModel->getTimeline(start, end);
    EXPECT_EQ(tl.size(), 1);
}

TEST_F(PythonCommsDecompositionTextFixture, um_onTransportEvent) {
    TRACE_FUNCTION();

    const Event ev = {"{}"};
    ComponentStatus status = userModel->onTransportEvent(ev);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}

TEST_F(PythonCommsDecompositionTextFixture, um_onUserInputReceived) {
    TRACE_FUNCTION();

    RaceHandle handle = 3;
    const std::string response = "response";
    ComponentStatus status = userModel->onUserInputReceived(handle, true, response);
    EXPECT_EQ(status, ComponentStatus::COMPONENT_OK);
}
