
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

#include "race/common/ChannelStatus.h"

std::string channelStatusToString(ChannelStatus channelStatus) {
  switch (channelStatus) {
  case CHANNEL_UNDEF:
    return "CHANNEL_UNDEF";
  case CHANNEL_AVAILABLE:
    return "CHANNEL_AVAILABLE";
  case CHANNEL_UNAVAILABLE:
    return "CHANNEL_UNAVAILABLE";
  case CHANNEL_ENABLED:
    return "CHANNEL_ENABLED";
  case CHANNEL_DISABLED:
    return "CHANNEL_DISABLED";
  case CHANNEL_STARTING:
    return "CHANNEL_STARTING";
  case CHANNEL_FAILED:
    return "CHANNEL_FAILED";
  case CHANNEL_UNSUPPORTED:
    return "CHANNEL_UNSUPPORTED";
  default:
    return "ERROR: INVALID CHANNEL STATUS: " + std::to_string(channelStatus);
  }
}

std::ostream &operator<<(std::ostream &out, ChannelStatus channelStatus) {
  return out << channelStatusToString(channelStatus);
}
