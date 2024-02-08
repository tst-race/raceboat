
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

#include "../../source/Core.h"
#include "../../source/PluginWrapper.h"
#include "../../source/SdkWrapper.h"
#include "gtest/gtest.h"

using namespace Raceboat;

TEST(CoreTest, getEntropy) {
  Core core;
  auto v = core.getEntropy(12);
  EXPECT_EQ(v.size(), 12);
  auto v2 = core.getEntropy(32);
  EXPECT_EQ(v2.size(), 32);
}

TEST(CoreTest, generateLinkId) {
  Core core;
  PluginContainer plugin;
  plugin.id = "Mock Plugin";

  LinkID linkId1 = core.generateLinkId(plugin, "Mock Channel");
  LinkID linkId2 = core.generateLinkId(plugin, "Mock Channel");
  EXPECT_NE(linkId1, linkId2);
}

TEST(CoreTest, generateConnectionId) {
  Core core;
  PluginContainer plugin;
  plugin.id = "Mock Plugin";

  LinkID linkId1 = "Mock Plugin/Mock Channel/LinkID_0";
  LinkID linkId2 = "Mock Plugin/Mock Channel/LinkID_1";

  ConnectionID connId1 = core.generateConnectionId(plugin, linkId1);
  ConnectionID connId2 = core.generateConnectionId(plugin, linkId2);
  EXPECT_NE(connId1, connId2);
}

TEST(CoreTest, generateHandle) {
  Core core;
  PluginContainer plugin;
  plugin.id = "Mock Plugin";

  auto raceHandle1 = core.generateHandle();
  auto raceHandle2 = core.generateHandle();
  EXPECT_NE(raceHandle1, raceHandle2);
}
