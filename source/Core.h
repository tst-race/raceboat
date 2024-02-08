
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

#include "UserInput.h"
#include "api-managers/ApiManager.h"
#include "api-managers/ChannelManager.h"
#include "plugin-loading/Config.h"
#include "plugin-loading/PluginLoader.h"
#include "race/Race.h"
#include "race/common/ConnectionStatus.h"
#include "race/common/LinkStatus.h"
#include "race/common/PackageStatus.h"
#include "race/common/PluginResponse.h"
#include "race/common/RaceHandle.h"

class EncPkg;

namespace Raceboat {

class PluginContainer;

class Core {
public:
  // Unintialized constructor. This won't be functional, but is useful for
  // testing.
  Core();

  // The main constructor
  Core(std::string race_dir, ChannelParamStore params);

  // Explicitly delete these. The plugins assume that the address of Core does
  // not change as they maintain references to it that must remain valid.
  Core(const Core &other) = delete;
  Core(Core &&other) = delete;
  Core &operator=(const Core &other) = delete;
  Core &operator=(Core &&other) = delete;

  void init();

  virtual ~Core();

  virtual FileSystem &getFS();
  virtual const UserInput &getUserInput();
  virtual const Config &getConfig();
  virtual PluginContainer *getChannel(std::string channelId);

  virtual ChannelManager &getChannelManager();
  virtual ApiManager &getApiManager();

  virtual RaceHandle generateHandle();

  // Plugin calls
  virtual std::vector<uint8_t> getEntropy(std::uint32_t numBytes);
  virtual std::string getActivePersona(PluginContainer &plugin);
  virtual SdkResponse asyncError(PluginContainer &plugin, RaceHandle handle,
                                 PluginResponse status);

  virtual SdkResponse onPackageStatusChanged(PluginContainer &plugin,
                                             RaceHandle handle,
                                             PackageStatus status);
  virtual SdkResponse
  onConnectionStatusChanged(PluginContainer &plugin, RaceHandle handle,
                            const ConnectionID &connId, ConnectionStatus status,
                            const LinkProperties &properties);
  virtual SdkResponse onLinkStatusChanged(PluginContainer &plugin,
                                          RaceHandle handle,
                                          const LinkID &linkId,
                                          LinkStatus status,
                                          const LinkProperties &properties);
  virtual SdkResponse
  onChannelStatusChanged(PluginContainer &plugin, RaceHandle handle,
                         const ChannelId &channelGid, ChannelStatus status,
                         const ChannelProperties &properties);
  virtual SdkResponse updateLinkProperties(PluginContainer &plugin,
                                           const LinkID &linkId,
                                           const LinkProperties &properties);
  virtual SdkResponse receiveEncPkg(PluginContainer &plugin, const EncPkg &pkg,
                                    const std::vector<ConnectionID> &connIDs);
  virtual ConnectionID generateConnectionId(PluginContainer &plugin,
                                            const LinkID &linkId);
  virtual LinkID generateLinkId(PluginContainer &plugin,
                                const ChannelId &channelGid);

protected:
  bool shuttingDown = false;

  FileSystem fs;
  UserInput userInput;
  Config config;

  std::unique_ptr<IPluginLoader> pluginLoader;
  std::unique_ptr<ChannelManager> channelManager;
  std::unique_ptr<ApiManager> apiManager;
};

} // namespace Raceboat