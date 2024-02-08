
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

using namespace Raceboat;
using testing::_;
using testing::Return;

TEST(ApiTest, oneshot) {
  std::promise<LinkAddress> linkAddrPromise;
  auto future = linkAddrPromise.get_future();

  // 'server' thread
  auto thread1 = std::thread([promise = std::move(linkAddrPromise)]() mutable {
    ChannelParamStore params;
    // TODO: determine what the best way to to do things is
    // Currently, we don't have a way to specific keys necessary for a channel
    // We can only get the keys required at the plugin level

    // An alternative is to store the required keys at the channel level in the
    // manifest instead of at the plugin level. This fixes the issue with the
    // user not knowing exactly which keys are required for a specific channel,
    // but has other issues. If multiple channels in a single plugin require the
    // same key, and the user specifies the values at a per channel level, one
    // value could overwrite another. Additionally, we would have to find out
    // which keys are required per channel for existing RACE channels, and can't
    // just create a script to do it.
    params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
    params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
    params.setChannelParam("hostname", "127.0.0.1");

    // The library is normally instantiated by calling this constructor
    // Race race("<plugin path>", params);

    // For testing, mock the core and use a different constructor instead
    auto core = std::make_shared<MockCore>("<plugin path>", params);
    Race race(core);

    ReceiveOptions opt;
    opt.recv_channel = "twoSixDirectCpp";

    EXPECT_CALL(core->mockApiManager, getReceiveObject(_, _))
        .WillOnce([](auto, auto callback) {
          callback(ApiStatus::OK, {}, {});
          return SDK_OK;
        });
    auto [status, link_addr, listener] = race.receive(opt);
    ASSERT_EQ(status, ApiStatus::OK);
    promise.set_value(link_addr);

    EXPECT_CALL(core->mockApiManager, receive(_, _))
        .WillOnce([](auto, auto callback) {
          std::string message = "Hello, World!";
          callback(ApiStatus::OK,
                   std::vector<uint8_t>{message.begin(), message.end()});
          return SDK_OK;
        });
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

    // Race race("<plugin path>", params);
    auto core = std::make_shared<MockCore>("<plugin path>", params);
    Race race(core);

    SendOptions opt;
    opt.send_channel = "twoSixDirectCpp";
    opt.send_address = future.get();

    EXPECT_CALL(core->mockApiManager, send(_, _, _))
        .Times(1)
        .WillOnce([](SendOptions, std::vector<uint8_t>,
                     std::function<void(ApiStatus)> callback) {
          callback(ApiStatus::OK);
          return SDK_OK;
        });
    auto status = race.send_str(opt, "Hello, World!");
    ASSERT_EQ(status, ApiStatus::OK);
  });

  thread1.join();
  thread2.join();
}
/*

TEST(ApiTest, DISABLED_bidi_oneshot) {
    std::promise<LinkAddress> linkAddrPromise;
    auto future = linkAddrPromise.get_future();

    // 'server' thread
    auto thread1 = std::thread([promise = std::move(linkAddrPromise)]() mutable
{ ChannelParamStore params;
        params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
        params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
        params.setChannelParam("hostname", "127.0.0.1");

        // Race race("<plugin path>", params);
        auto core = std::make_shared<MockCore>("<plugin path>", params);
        Race race(core);

        ReceiveOptions opt;
        opt.recv_channel = "twoSixDirectCpp";
        opt.send_channel = "twoSixDirectCpp";

        auto [link_addr, listener] = race.receive_respond(opt);
        promise.set_value(link_addr);

        auto [received_message, reply] = listener.receive_str();

        EXPECT_EQ(received_message, "Hello, World!");

        reply.respond_str("Hello to you too!");
    });

    // 'client' thread
    auto thread2 = std::thread([future = std::move(future)]() mutable {
        ChannelParamStore params;
        params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
        params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
        params.setChannelParam("hostname", "127.0.0.1");

        // Race race("<plugin path>", params);
        auto core = std::make_shared<MockCore>("<plugin path>", params);
        Race race(core);

        SendOptions opt;
        opt.send_channel = "twoSixDirectCpp";
        opt.recv_channel = "twoSixDirectCpp";
        opt.send_address = future.get();
        opt.timeout_ms = 30 * 60 * 1000;

        auto received_message = race.send_receive_str(opt, "Hello, World!");
        EXPECT_EQ(received_message, "Hello to you too!");
    });

    thread1.join();
    thread2.join();
}

TEST(ApiTest, DISABLED_bidi_channel) {
    std::promise<LinkAddress> linkAddrPromise;
    auto future = linkAddrPromise.get_future();

    // 'server' thread
    auto thread1 = std::thread([promise = std::move(linkAddrPromise)]() mutable
{ ChannelParamStore params;
        params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
        params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
        params.setChannelParam("hostname", "127.0.0.1");

        // Race race("<plugin path>", params);
        auto core = std::make_shared<MockCore>("<plugin path>", params);
        Race race(core);

        ReceiveOptions opt;
        opt.recv_channel = "twoSixDirectCpp";
        opt.send_channel = "twoSixDirectCpp";
        opt.alt_channel = "twoSixDirectCpp";
        opt.timeout_ms = 30 * 60 * 1000;

        auto [link_addr, listener] = race.listen(opt);
        promise.set_value(link_addr);

        // TODO: is there a sensible way to accept with a response? Should the
accept call take an
        // argument? If it did, that message could not depend on the received
message. Otherwise, it
        // would be possible to return an reply object that could be used for
the reply. That might
        // be overly complicated though
        auto connection = listener.accept();
        connection.write_str("server message");
        auto received_message = connection.read_str();

        EXPECT_EQ(received_message, "client message 1");
        received_message = connection.read_str();

        EXPECT_EQ(received_message, "client message 2");
    });

    // 'client' thread
    auto thread2 = std::thread([future = std::move(future)]() mutable {
        ChannelParamStore params;
        params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
        params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
        params.setChannelParam("hostname", "127.0.0.1");

        // Race race("<plugin path>", params);
        auto core = std::make_shared<MockCore>("<plugin path>", params);
        Race race(core);

        SendOptions opt;
        opt.send_channel = "twoSixDirectCpp";
        opt.recv_channel = "twoSixDirectCpp";
        opt.alt_channel = "twoSixDirectCpp";
        opt.send_address = future.get();
        opt.timeout_ms = 30 * 60 * 1000;

        auto connection = race.dial_str(opt, "client message 1");
        connection.write_str("client message 2");
        auto received_message = connection.read_str();
        EXPECT_EQ(received_message, "server message");
    });

    thread1.join();
    thread2.join();
}
*/

/*
// Example Async interface usage

TEST(ApiTest, DISABLED_async_bidi_oneshot) {
    std::promise<LinkAddress> linkAddrPromise;
    auto future = linkAddrPromise.get_future();
    auto thread1 = std::thread([promise = std::move(linkAddrPromise)]() mutable
{ ChannelParamStore params;
        params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
        params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
        params.setChannelParam("hostname", "127.0.0.1");

        RaceAsync race("<plugin path>", params);
        // auto core = std::make_shared<MockCore>("<plugin path>", params);
        // Race race(core);

        ReceiveOptions opt;
        opt.recv_channel = "twoSixDirectCpp";
        opt.send_channel = "twoSixDirectCpp";

        race.receive_respond(opt, [promise = std::move(promise)](const
LinkAddress addr, RespondObject listener) mutable { promise.set_value(addr);

            listener.receive_str([](auto received_message, auto reply){
                EXPECT_EQ(received_message, "Hello, World!");
                reply.respond_str("Hello to you too!", [](bool success){

                });
            });
        });

        race.waitForCallbacks();
    });

    auto thread2 = std::thread([future = std::move(future)]() mutable {
        ChannelParamStore params;
        params.setChannelParam("PluginCommsTwoSixStub.startPort", "26262");
        params.setChannelParam("PluginCommsTwoSixStub.endPort", "26264");
        params.setChannelParam("hostname", "127.0.0.1");

        // Race race("<plugin path>", params);
        auto core = std::make_shared<MockCore>("<plugin path>", params);
        RaceAsync race(core);

        SendOptions opt;
        opt.send_channel = "twoSixDirectCpp";
        opt.recv_channel = "twoSixDirectCpp";
        opt.send_address = future.get();
        opt.timeout_ms = 30 * 60 * 1000;

        race.send_receive_str(opt, "Hello, World!", [](auto received_message){
            EXPECT_EQ(received_message, "Hello to you too!");
        });

        race.waitForCallbacks();
    });

    thread1.join();
    thread2.join();
}
*/
