
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

#include <mutex>

#include "ChannelId.h"
#include "ChannelProperties.h"
#include "RaceHandle.h"

namespace Raceboat {
class Core;

enum class ActivateChannelStatusCode {
  OK,
  ALREADY_ACTIVATED,
  ACTIVATED_WITH_DIFFERENT_ROLE,
  INVALID_STATE,
  INVALID_ROLE,
  FAILED_TO_GET_CHANNEL,
  CHANNEL_DOES_NOT_EXIST
};

std::string activateChannelStatusCodeToString(ActivateChannelStatusCode status);

class ChannelManager {
public:
  ChannelManager(Core &core);
  virtual ~ChannelManager();
  virtual ChannelProperties getChannelProperties(const ChannelId &channelGid);
  virtual std::vector<ChannelProperties> getAllChannelProperties();
  virtual void onChannelStatusChanged(RaceHandle handle,
                                      const ChannelId &channelGid,
                                      ChannelStatus status,
                                      const ChannelProperties &properties);
  virtual ActivateChannelStatusCode activateChannel(RaceHandle handle,
                                                    std::string channelGid,
                                                    std::string roleName);

protected:
  Core &core;

  std::mutex propsMutex;
  std::unordered_map<ChannelId, ChannelProperties> channelProps;
};

} // namespace Raceboat