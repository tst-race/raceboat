
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

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>
#include <vector>

#include "race/common/RaceEnums.h"

namespace Raceboat {

using json = nlohmann::json;

/**
 * @brief Definition used to load a plugin.
 *
 */

class PluginDef {
public:
  std::string filePath;
  RaceEnums::PluginFileType fileType;
  std::string sharedLibraryPath;
  std::string pythonModule;
  std::string pythonClass;
  std::vector<std::string> channels;
  std::vector<std::string> usermodels;
  std::vector<std::string> transports;
  std::vector<std::string> encodings;

  bool is_unified_plugin() const;
  bool is_decomposed_plugin() const;
};

/**
 * @brief Convert plugin json to a plugin definition in the nlohmann way
 * If the plugin json is invalid then an exception will be thrown.
 *
 * @param pluginJson The plugin json.
 * @return PluginDef The plugin definition.
 */
void from_json(const nlohmann::json &pluginJson, PluginDef &pluginDef);

inline std::ostream &operator<<(std::ostream &os, const PluginDef &pluginDef) {
  os << "{ "
     << "filePath: " << pluginDef.filePath << ", "
     << "fileType: " << static_cast<std::int32_t>(pluginDef.fileType) << ", "
     << "pythonModule: " << pluginDef.pythonModule << ", "
     << "pythonClass: " << pluginDef.pythonClass << "}";

  return os;
}

} // namespace Raceboat
