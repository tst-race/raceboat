
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

#include <future>
#include <thread>

#include "../common/MockCore.h"
#include "Race.h"
#include "gtest/gtest.h"
#include "helper.h"

using namespace Raceboat;
using testing::_;
using testing::Return;

#ifndef DECOMPOSED_IMPLEMENTATION
#define DECOMPOSED_IMPLEMENTATION "Unknown"
#endif

struct IntegrationFileSystem : public FileSystem {
    IntegrationFileSystem(const fs::path &path) : FileSystem(path) {}
    virtual const char *getHostArch() {
        return "myArch";
    }
    virtual const char *getHostOsType() {
        return "myOS";
    }
};

struct IntegrationCore : public Core {
    IntegrationCore(const fs::path &path, const ChannelParamStore &params) :
        Core("", params), integrationFS(IntegrationFileSystem(path)) {
        helper::logDebug("path:" + path.string());
        helper::logDebug("plugin path:" + getFS().pluginsInstallPath.string());
        init();
    }

    virtual FileSystem &getFS() override {
        return integrationFS;
    }

protected:
    IntegrationFileSystem integrationFS;
};

TEST(IntegrationTest, oneshot) {
    std::promise<LinkAddress> linkAddrPromise;
    auto future = linkAddrPromise.get_future();

    // 'server' thread
    auto thread1 = std::thread([promise = std::move(linkAddrPromise)]() mutable {
        ChannelParamStore params;
        params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
        params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
        params.setChannelParam("hostname", "127.0.0.1");

        auto stub_path = std::filesystem::path(DECOMPOSED_IMPLEMENTATION);
        auto plugin_path =
            stub_path.parent_path().parent_path().parent_path().parent_path().parent_path();

        auto core = std::make_shared<IntegrationCore>(plugin_path.string(), params);
        Race race(core);
        // Race race(plugin_path.string(), params);

        ReceiveOptions opt;
        opt.recv_channel = "DecomposedTestImplementation";
        opt.recv_role = "default";

        auto [status, link_addr, listener] = race.receive(opt);
        ASSERT_EQ(status, ApiStatus::OK);
        promise.set_value(link_addr);
        auto [status2, received_message] = listener.receive_str();
        ASSERT_EQ(status2, ApiStatus::OK);
        EXPECT_EQ(received_message, "Hello, World!");
    });

    // 'client' thread
    auto thread2 = std::thread([future = std::move(future)]() mutable {
        ChannelParamStore params;
        params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
        params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
        params.setChannelParam("hostname", "127.0.0.1");

        auto stub_path = std::filesystem::path(DECOMPOSED_IMPLEMENTATION);
        auto plugin_path =
            stub_path.parent_path().parent_path().parent_path().parent_path().parent_path();
        auto core = std::make_shared<IntegrationCore>(plugin_path.string(), params);
        Race race(core);
        // Race race(plugin_path.string(), params);

        SendOptions opt;
        opt.send_channel = "DecomposedTestImplementation";
        opt.send_address = future.get();
        opt.send_role = "default";

        auto status = race.send_str(opt, "Hello, World!");
        ASSERT_EQ(status, ApiStatus::OK);
    });

    thread1.join();
    thread2.join();
}

TEST(IntegrationTest, bidi) {
    std::promise<LinkAddress> linkAddrPromise;
    auto future = linkAddrPromise.get_future();

    std::string message1 = "Hello from client";
    std::string message2 = "Hello from server";

    // 'server' thread
    auto thread1 =
        std::thread([promise = std::move(linkAddrPromise), message1, message2]() mutable {
            ChannelParamStore params;
            params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
            params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
            params.setChannelParam("hostname", "127.0.0.1");

            auto stub_path = std::filesystem::path(DECOMPOSED_IMPLEMENTATION);
            auto plugin_path =
                stub_path.parent_path().parent_path().parent_path().parent_path().parent_path();

            auto core = std::make_shared<IntegrationCore>(plugin_path.string(), params);
            Race race(core);
            // Race race(plugin_path.string(), params);

            ReceiveOptions opt;
            opt.recv_channel = "DecomposedTestImplementation";
            opt.recv_role = "default";
            opt.send_channel = "DecomposedTestImplementation";
            opt.send_role = "default";

            auto [status, link_addr, listener] = race.receive_respond(opt);
            ASSERT_EQ(status, ApiStatus::OK);
            promise.set_value(link_addr);
            auto [status2, received_message, responder] = listener.receive_str();
            ASSERT_EQ(status2, ApiStatus::OK);
            EXPECT_EQ(received_message, message1);
            auto status3 = responder.respond_str(message2);
            ASSERT_EQ(status3, ApiStatus::OK);
        });

    // 'client' thread
    auto thread2 = std::thread([future = std::move(future), message1, message2]() mutable {
        ChannelParamStore params;
        params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
        params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
        params.setChannelParam("hostname", "127.0.0.1");

        auto stub_path = std::filesystem::path(DECOMPOSED_IMPLEMENTATION);
        auto plugin_path =
            stub_path.parent_path().parent_path().parent_path().parent_path().parent_path();
        auto core = std::make_shared<IntegrationCore>(plugin_path.string(), params);
        Race race(core);
        // Race race(plugin_path.string(), params);

        SendOptions opt;
        opt.recv_channel = "DecomposedTestImplementation";
        opt.recv_role = "default";
        opt.send_channel = "DecomposedTestImplementation";
        opt.send_address = future.get();
        opt.send_role = "default";

        auto [status, received_message] = race.send_receive_str(opt, message1);
        ASSERT_EQ(status, ApiStatus::OK);
        EXPECT_EQ(received_message, message2);
    });

    thread1.join();
    thread2.join();
}

TEST(IntegrationTest, connect) {
    std::promise<LinkAddress> linkAddrPromise;
    auto future = linkAddrPromise.get_future();

    std::string message1 = "Hello from client";
    std::string message2 = "Hello from server";
    std::string message3 = "Hello from client message 2";

    // 'server' thread
    auto thread1 =
        std::thread([promise = std::move(linkAddrPromise), message1, message2, message3]() mutable {
            ChannelParamStore params;
            params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
            params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
            params.setChannelParam("hostname", "127.0.0.1");

            auto stub_path = std::filesystem::path(DECOMPOSED_IMPLEMENTATION);
            auto plugin_path =
                stub_path.parent_path().parent_path().parent_path().parent_path().parent_path();

            auto core = std::make_shared<IntegrationCore>(plugin_path.string(), params);
            Race race(core);
            // Race race(plugin_path.string(), params);

            ReceiveOptions opt;
            opt.recv_channel = "DecomposedTestImplementation";
            opt.recv_role = "default";
            opt.send_channel = "DecomposedTestImplementation";
            opt.send_role = "default";

            auto [status, link_addr, listener] = race.listen(opt);
            ASSERT_EQ(status, ApiStatus::OK);
            promise.set_value(link_addr);
            auto [status2, connection] = listener.accept();
            ASSERT_EQ(status2, ApiStatus::OK);
            auto status5 = connection.write_str(message2);
            ASSERT_EQ(status5, ApiStatus::OK);
            auto [status3, received_message] = connection.read_str();
            ASSERT_EQ(status3, ApiStatus::OK);
            EXPECT_EQ(received_message, message1);
            auto [status4, received_message2] = connection.read_str();
            ASSERT_EQ(status4, ApiStatus::OK);
            EXPECT_EQ(received_message2, message3);
            auto status6 = connection.close();
            ASSERT_EQ(status6, ApiStatus::OK);
        });

    // 'client' thread
    auto thread2 =
        std::thread([future = std::move(future), message1, message2, message3]() mutable {
            ChannelParamStore params;
            params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
            params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
            params.setChannelParam("hostname", "127.0.0.1");

            SendOptions opt;
            opt.recv_channel = "DecomposedTestImplementation";
            opt.recv_role = "default";
            opt.send_channel = "DecomposedTestImplementation";
            opt.send_address = future.get();
            opt.send_role = "default";

            auto stub_path = std::filesystem::path(DECOMPOSED_IMPLEMENTATION);
            auto plugin_path =
                stub_path.parent_path().parent_path().parent_path().parent_path().parent_path();
            auto core = std::make_shared<IntegrationCore>(plugin_path.string(), params);
            Race race(core);
            // Race race(plugin_path.string(), params);

            auto [status, connection] = race.dial_str(opt, message1);
            // auto [status, connection] = race.dial_str(opt, "");
            ASSERT_EQ(status, ApiStatus::OK);
            auto status2 = connection.write_str(message3);
            ASSERT_EQ(status2, ApiStatus::OK);
            auto [status3, received_message] = connection.read_str();
            ASSERT_EQ(status3, ApiStatus::OK);
            EXPECT_EQ(received_message, message2);
            auto status4 = connection.close();
            ASSERT_EQ(status4, ApiStatus::OK);
        });

    thread1.join();
    thread2.join();
}