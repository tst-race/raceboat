
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

#ifndef __CONFIG_READER_H__
#define __CONFIG_READER_H__

#include <map>
#include <string>

#include "Composition.h"
#include "FileSystem.h"
#include "PluginDef.h"
#include "ChannelProperties.h"

namespace Raceboat {
/**
 * @brief Reads plugin and channel information necessary for \p IPluginLoader
 * it requires at least one of the following files to exist: manifest.json,
 * channel_properties.json
 */
class Config {
public:
  /**
   * @brief Parse plugin manifests to populate manifests, and user response keys
   *
   * @param FileSystem fs used to specify plugins install path and list plugin
   * directories
   * /<plugin id>/ directory and contains manifest.json (eg \p
   * pluginsPath/PluginCommsTwoSixCpp)
   *
   * @return Indication of success
   */
  bool parsePluginManifests(FileSystem &fs);

  struct ChannelParameter {
    typedef enum {
      STRING, // default
      INTEGER,
      BOOLEAN
    } ValueType;

    ValueType valueType;
    std::string key;
    std::string plugin; // considered common to all plugins (vs plugin-specific)
                        // if empty
    bool required;      // default true
    std::string defaultValue; // capable of representing any value type
  };

  struct PluginManifest {
    std::vector<PluginDef> plugins;
    std::map<std::string, ChannelProperties> channelIdChannelPropsMap;
    std::vector<Composition> compositions;
    std::vector<ChannelParameter> channelParameters;
  };
  std::vector<PluginManifest> manifests;

protected:
  virtual bool parseJson(const fs::path &path, nlohmann::json &json);
};

void from_json(const nlohmann::json &j, Config::ChannelParameter &response);
void from_json(const nlohmann::json &j, Config::PluginManifest &manifest);

} // namespace Raceboat

#endif // __CONFIG_READER_H__
