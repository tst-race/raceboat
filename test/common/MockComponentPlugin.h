
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

#pragma once

#include "LogExpect.h"
#include "MockEncoding.h"
#include "MockTransport.h"
#include "MockUserModel.h"
#include "gmock/gmock.h"
#include "helper.h"
#include "plugin-loading/ComponentPlugin.h"
#include "race_printers.h"

class MockComponentPlugin : public Raceboat::ComponentPlugin {
public:
    MockComponentPlugin(std::string id, LogExpect &logger) :
        Raceboat::ComponentPlugin(""), id(id), logger(logger) {
        using ::testing::_;
        ON_CALL(*this, createTransport(_, _, _, _))
            .WillByDefault([this](const std::string &name, ITransportSdk *sdk,
                                  const std::string &roleName, const PluginConfig &pluginConfig) {
                LOG_EXPECT(this->logger, this->id + ".createTransport", name, roleName,
                           pluginConfig);
                transport = std::make_shared<MockTransport>(this->logger, *sdk);
                return transport;
            });
        ON_CALL(*this, createUserModel(_, _, _, _))
            .WillByDefault([this](const std::string &name, IUserModelSdk *sdk,
                                  const std::string &roleName, const PluginConfig &pluginConfig) {
                LOG_EXPECT(this->logger, this->id + ".createUserModel", name, roleName,
                           pluginConfig);
                userModel = std::make_shared<MockUserModel>(this->logger, *sdk);
                return userModel;
            });
        ON_CALL(*this, createEncoding(_, _, _, _))
            .WillByDefault([this](const std::string &name, IEncodingSdk *sdk,
                                  const std::string &roleName, const PluginConfig &pluginConfig) {
                LOG_EXPECT(this->logger, this->id + ".createEncoding", name, roleName,
                           pluginConfig);
                encoding = std::make_shared<MockEncoding>(this->logger, *sdk);
                return encoding;
            });
    }

    MOCK_METHOD(std::shared_ptr<ITransportComponent>, createTransport,
                (const std::string &name, ITransportSdk *sdk, const std::string &roleName,
                 const PluginConfig &pluginConfig),
                (override));
    MOCK_METHOD(std::shared_ptr<IUserModelComponent>, createUserModel,
                (const std::string &name, IUserModelSdk *sdk, const std::string &roleName,
                 const PluginConfig &pluginConfig),
                (override));
    MOCK_METHOD(std::shared_ptr<IEncodingComponent>, createEncoding,
                (const std::string &name, IEncodingSdk *sdk, const std::string &roleName,
                 const PluginConfig &pluginConfig),
                (override));

public:
    std::string id;
    LogExpect &logger;

    std::shared_ptr<MockTransport> transport;
    std::shared_ptr<MockUserModel> userModel;
    std::shared_ptr<MockEncoding> encoding;
};
