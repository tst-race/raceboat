
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

#include "../../source/UserInput.h"
#include "Race.h"
#include "gtest/gtest.h"

using namespace RaceLib;

TEST(UserInputTest, test) {
    std::string actualStartPort = "26262";
    std::string actualEndPort = "26264";
    std::string actualHostname = "127.0.0.1";
    std::string actualOtherKey = "blah blah";

    ChannelParamStore params;
    params.setChannelParam("PluginCommsTwoSixStub.startPort", actualStartPort);
    params.setChannelParam("PluginCommsTwoSixStub.endPort", actualEndPort);
    params.setChannelParam("hostname", actualHostname);
    params.setChannelParam("other key", actualOtherKey);

    UserInput input(params);
    std::string startPort = *input.getPluginUserInput("PluginCommsTwoSixStub", "startPort");
    std::string endPort = *input.getPluginUserInput("PluginCommsTwoSixStub", "endPort");
    std::string hostname = *input.getCommonUserInput("hostname");
    std::string otherKey = *input.getCommonUserInput("other key");
    std::optional<std::string> invalid1 =
        input.getPluginUserInput("PluginCommsTwoSixStub", "invalid");
    std::optional<std::string> invalid2 = input.getPluginUserInput("invalid", "startPort");
    std::optional<std::string> invalid3 = input.getCommonUserInput("startPort");
    std::optional<std::string> invalid4 = input.getCommonUserInput("invalid");
    std::optional<std::string> invalid5 =
        input.getPluginUserInput("PluginCommsTwoSixStub", "hostname");

    EXPECT_EQ(startPort, actualStartPort);
    EXPECT_EQ(endPort, actualEndPort);
    EXPECT_EQ(hostname, actualHostname);
    EXPECT_EQ(otherKey, actualOtherKey);
    EXPECT_EQ(invalid1, std::nullopt);
    EXPECT_EQ(invalid2, std::nullopt);
    EXPECT_EQ(invalid3, std::nullopt);
    EXPECT_EQ(invalid4, std::nullopt);
    EXPECT_EQ(invalid5, std::nullopt);
}