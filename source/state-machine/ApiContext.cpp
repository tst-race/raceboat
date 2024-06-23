
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

#include "ApiContext.h"

#include "Core.h"
#include "api-managers/ApiManager.h"

namespace Raceboat {

ApiContext::ApiContext(ApiManagerInternal &_manager, StateEngine &_engine)
    : manager(_manager), engine(_engine),
      handle(manager.getCore().generateHandle()) {}

bool ApiContext::shouldCreate(const ChannelId &channelId, bool useForRecv) {
  ChannelProperties props = manager.getCore().getChannelManager().getChannelProperties(channelId);

  // Treat LOADER_TO_CREATOR and BIDIRECTIONAL as the same
  // If sending, we create for CREATOR_TO_LOADER and otherwise load
  bool shouldCreate = props.linkDirection == LD_CREATOR_TO_LOADER;
  // If receiving, we load for CREATOR_TO_LOADER and otherwise create
  if (useForRecv) {
    return not shouldCreate;
  }
  return shouldCreate;
}
bool ApiContext::shouldCreateSender(const ChannelId &channelId) {
  return shouldCreate(channelId, false);
}

bool ApiContext::shouldCreateReceiver(const ChannelId &channelId) {
  return shouldCreate(channelId, true);
}


} // namespace Raceboat
