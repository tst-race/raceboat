
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

#include "ChannelManager.h"

#include "Core.h"
#include "PluginWrapper.h"
#include "helper.h"
#include "plugin-loading/Config.h"

namespace Raceboat {

std::string
activateChannelStatusCodeToString(ActivateChannelStatusCode status) {
  switch (status) {
  case ActivateChannelStatusCode::OK:
    return "OK";
  case ActivateChannelStatusCode::ALREADY_ACTIVATED:
    return "ALREADY_ACTIVATED";
  case ActivateChannelStatusCode::ACTIVATED_WITH_DIFFERENT_ROLE:
    return "ACTIVATED_WITH_DIFFERENT_ROLE";
  case ActivateChannelStatusCode::INVALID_STATE:
    return "INVALID_STATE";
  case ActivateChannelStatusCode::INVALID_ROLE:
    return "INVALID_ROLE";
  case ActivateChannelStatusCode::FAILED_TO_GET_CHANNEL:
    return "FAILED_TO_GET_CHANNEL";
  case ActivateChannelStatusCode::CHANNEL_DOES_NOT_EXIST:
    return "CHANNEL_DOES_NOT_EXIST";
  default:
    return "unknown status: " + std::to_string(static_cast<int>(status));
  }
}

ChannelManager::ChannelManager(Core &core) : core(core) {
  for (auto &manifest : core.getConfig().manifests) {
    channelProps.insert(manifest.channelIdChannelPropsMap.begin(),
                        manifest.channelIdChannelPropsMap.end());
  }

  for (auto &kv : channelProps) {
    kv.second.channelStatus = CHANNEL_ENABLED;
  }
}

ChannelManager::~ChannelManager() {}

ChannelProperties
ChannelManager::getChannelProperties(const ChannelId &channelId) {
  return channelProps[channelId];
}

std::vector<ChannelProperties> ChannelManager::getAllChannelProperties() {
  std::vector<ChannelProperties> props;
  for (auto &kv : channelProps) {
    props.push_back(kv.second);
  }
  return props;
}

void ChannelManager::onChannelStatusChanged(
    RaceHandle /* handle */, const ChannelId &channelGid, ChannelStatus status,
    const ChannelProperties & /* properties */) {
  try {
    std::unique_lock propLock(propsMutex);
    auto &props = channelProps.at(channelGid);
    props.channelStatus = status;
  } catch (std::exception &e) {
    helper::logError("Channel not found: " + channelGid);
  }
}

ActivateChannelStatusCode
ChannelManager::activateChannel(RaceHandle handle, std::string channelGid,
                                std::string roleName) {
  try {
    std::unique_lock propLock(propsMutex);
    auto &props = channelProps.at(channelGid);
    if (props.channelStatus != CHANNEL_ENABLED &&
        props.channelStatus != CHANNEL_STARTING &&
        props.channelStatus != CHANNEL_AVAILABLE) {
      helper::logError("Channel in invalid state: " +
                       channelStatusToString(props.channelStatus));
      return ActivateChannelStatusCode::INVALID_STATE;
    }

    if (props.channelStatus != CHANNEL_ENABLED) {
      if (props.currentRole.roleName == roleName) {
        return ActivateChannelStatusCode::ALREADY_ACTIVATED;
      } else {
        return ActivateChannelStatusCode::ACTIVATED_WITH_DIFFERENT_ROLE;
      }
    } else {
      props.currentRole = {};
      for (auto &role : props.roles) {
        if (role.roleName == roleName) {
          props.currentRole = role;
          break;
        }
      }

      if (props.currentRole.roleName != roleName) {
        return ActivateChannelStatusCode::INVALID_ROLE;
      }

      props.channelStatus = CHANNEL_STARTING;
      propsMutex.unlock();

      PluginContainer *channel = core.getChannel(channelGid);
      if (!channel) {
        return ActivateChannelStatusCode::FAILED_TO_GET_CHANNEL;
      }

      channel->plugin->activateChannel(handle, channelGid, roleName, 0);
      return ActivateChannelStatusCode::OK;
    }

  } catch (std::exception &e) {
    helper::logError("Channel not found: " + channelGid);
    return ActivateChannelStatusCode::CHANNEL_DOES_NOT_EXIST;
  }
}

} // namespace Raceboat