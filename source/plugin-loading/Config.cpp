
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

#include "Config.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "helper.h"

namespace Raceboat {

bool Config::parsePluginManifests(Raceboat::FileSystem &fs) {
  TRACE_METHOD();
  bool success = false;
  nlohmann::json json;
  std::vector<fs::path> pluginPaths = fs.listInstalledPluginDirs();
  for (fs::path &pluginPath : pluginPaths) {
    std::string pluginName = pluginPath.filename().string();
    helper::logInfo(logPrefix + " parsing plugin path " + pluginPath.string());
    fs::path path = fs.makePluginInstallPath("manifest.json", pluginName);

    if (parseJson(path, json)) {
      try {
        helper::logInfo(logPrefix + " " + "Parsing to a PluginManifest");
        manifests.push_back(json.get<Raceboat::Config::PluginManifest>());
        helper::logInfo(logPrefix + " " + "Parsed to a PluginManifest");
        success = true;
      } catch (nlohmann::json::exception &ex) {
        helper::logWarning(logPrefix + " " + path.string() +
                           " json parse error: " + std::string(ex.what()));
      } catch (...) {
        helper::logWarning(logPrefix + " " + path.string() +
                           " json parse error: unknown");
      }
    }
  }
  return success;
}

bool Config::parseJson(const fs::path &path, nlohmann::json &json) {
  TRACE_METHOD();
  bool success = false;
  std::ifstream fileStream(path);

  if (fileStream.fail()) {
    helper::logWarning(path.string() + " does not exist");
  } else {
    try {
      json.clear();
      json = nlohmann::json::parse(fileStream);
      success = true;
    } catch (nlohmann::json::exception &ex) {
      helper::logWarning(logPrefix + " " + path.string() +
                         " json parse error: " + std::string(ex.what()));
    } catch (...) {
      helper::logWarning(logPrefix + " " + path.string() +
                         " json parse error: unknown");
    }
  }
  return success;
}

void from_json(const nlohmann::json &j, Config::ChannelParameter &response) {
  // defaults
  std::string type = "string";
  response.valueType = Config::ChannelParameter::STRING;
  response.required = true;
  response.plugin = "";
  response.defaultValue = "";

  response.key = j.at("key");

  // optional params
  response.plugin = j.value("plugin", response.plugin);
  response.required = j.value("required", response.required);
  type = j.value("type", type);
  if (type == "string") {
    response.valueType = Config::ChannelParameter::STRING;
  } else if (type == "int") {
    response.valueType = Config::ChannelParameter::INTEGER;
  } else if (type == "bool") {
    response.valueType = Config::ChannelParameter::BOOLEAN;
  } else {
    throw std::invalid_argument("invalid \'type\' " + type +
                                ". Supported types: [string, int, bool]");
  }

  if (j.contains("default")) {
    if (response.valueType == Config::ChannelParameter::STRING) {
      response.defaultValue = j.at("default").get<std::string>();
    } else if (response.valueType == Config::ChannelParameter::INTEGER) {
      response.defaultValue = std::to_string(j.at("default").get<int>());
    } else if (response.valueType == Config::ChannelParameter::BOOLEAN) {
      response.defaultValue = std::to_string(j.at("default").get<bool>());
    }
  }
}

void from_json(const nlohmann::json &j,
               Raceboat::Config::PluginManifest &manifest) {
  manifest.plugins = j.at("plugins").get<std::vector<PluginDef>>();
  manifest.channelIdChannelPropsMap =
      j.at("channel_properties")
          .get<std::map<std::string, ChannelProperties>>();
  manifest.compositions = j.value("compositions", manifest.compositions);
  manifest.channelParameters =
      j.value("channel_parameters", manifest.channelParameters);

  // ensure channelParameter.plugin matches plugin/composition id
  std::unordered_set<std::string> ids;
  ids.insert(""); // allow for unspecified plugin
  for (auto plugin : manifest.plugins) {
    ids.insert(plugin.filePath);
  }
  for (auto composition : manifest.compositions) {
    ids.insert(composition.id);
  }

  for (auto channelParam : manifest.channelParameters) {
    if (ids.find(channelParam.plugin) == ids.end()) {
      throw std::invalid_argument(
          "no corresponding plugins.file_path in channel_parameters.plugin " +
          channelParam.plugin);
    }
  }
}
} // namespace Raceboat
