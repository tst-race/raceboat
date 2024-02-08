
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

#include <memory>
#include <string>

namespace Raceboat {

class PluginWrapper;
class SdkWrapper;

class PluginContainer {
public:
  std::string id;
  std::unique_ptr<PluginWrapper> plugin;
  std::unique_ptr<SdkWrapper> sdk;

  PluginContainer() = default;
  // The destructor must have PluginWrapper and SdkWrapper available when
  // instantiated
  ~PluginContainer();

  // Explicitly delete these. The plugin and sdk assume that the address of the
  // PluginContainer does not change as they maintain references to it that must
  // remain valid.
  PluginContainer(PluginContainer &&other) = delete;
  PluginContainer &operator=(PluginContainer &&other) = delete;
};

} // namespace Raceboat
