
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

#include "PluginDef.h"

// #include "RaceExceptions.h"
#include "helper.h"

namespace Raceboat {

bool PluginDef::is_decomposed_plugin() const {
    return !is_unified_plugin();
}

bool PluginDef::is_unified_plugin() const {
    return !channels.empty();
}

void from_json(const nlohmann::json &pluginJson, PluginDef &pluginDef) {
    try {
        pluginDef.fileType = RaceEnums::stringToPluginFileType(pluginJson.at("file_type"));
        pluginDef.filePath = pluginJson.at("file_path");
        if (pluginDef.fileType == RaceEnums::PluginFileType::PFT_PYTHON) {
            pluginDef.pythonModule = pluginJson.at("python_module");
            pluginDef.pythonClass = pluginJson.at("python_class");
        }
        pluginDef.sharedLibraryPath = pluginJson.value("shared_library_path", "");

        pluginDef.channels = pluginJson.value("channels", std::vector<std::string>{});
        pluginDef.transports = pluginJson.value("transports", std::vector<std::string>{});
        pluginDef.usermodels = pluginJson.value("usermodels", std::vector<std::string>{});
        pluginDef.encodings = pluginJson.value("encodings", std::vector<std::string>{});
    } catch (const json::out_of_range &error) {
        throw std::invalid_argument("plugin definition missing required key: " +
                                    std::string(error.what()));
    } catch (const std::invalid_argument &error) {
        throw std::invalid_argument("plugin definition invalid value: " +
                                    std::string(error.what()));
    }
}

}  // namespace Raceboat
