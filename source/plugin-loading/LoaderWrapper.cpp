
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

#include "LoaderWrapper.h"

namespace Raceboat {

LoaderWrapper::LoaderWrapper(PluginContainer &container, Core &, const fs::path &path) :
    PluginWrapper(container), dl(path) {
    TRACE_METHOD(path);
    auto create = dl.get<Interface *(SDK *)>(createFuncName);
    auto destroy = dl.get<void(Interface *)>(destroyFuncName);
    auto version = dl.get<const RaceVersionInfo>("raceVersion");
    auto pluginId = dl.get<const char *const>("racePluginId");
    auto pluginDesc = dl.get<const char *const>("racePluginDescription");

    std::stringstream debugMessage;
    debugMessage << logPrefix + "Loading plugin: " << path
                 << ". Version: " + versionToString(version) << ". ID: " << pluginId
                 << ". Description: " << pluginDesc;
    helper::logDebug(debugMessage.str());

    if (version != RACE_VERSION) {
        const std::string errorMessage = logPrefix + "Mismatched RACE version number. Expected " +
                                         versionToString(RACE_VERSION) +
                                         ". Found: " + versionToString(version);
        helper::logError(errorMessage);
        throw std::runtime_error(errorMessage);
    }
    if (pluginId == nullptr || pluginId[0] == '\0') {
        const std::string errorMessage = logPrefix + "Invalid plugin ID: null or emptry string.";
        helper::logError(errorMessage);
        throw std::runtime_error(errorMessage);
    }
    for (const char *c = pluginId; *c != '\0'; ++c) {
        if (!std::isalnum(*c) && *c != '-' && *c != '_') {
            const std::string errorMessage =
                logPrefix + "Invalid character in plugin ID: " + std::string(c);
            helper::logError(errorMessage);
            throw std::runtime_error(errorMessage);
        }
    }
    if (pluginDesc == nullptr || pluginDesc[0] == '\0') {
        const std::string errorMessage =
            logPrefix + "Invalid plugin description: null or emptry string.";
        helper::logError(errorMessage);
        throw std::runtime_error(errorMessage);
    }
    if (pluginId != container.id) {
        const std::string errorMessage =
            logPrefix + "Plugin Id does not match expected value. Expected: " + container.id +
            " Got: " + pluginId;
        helper::logError(errorMessage);
        throw std::runtime_error(errorMessage);
    }

    auto plugin = std::shared_ptr<Interface>(create(this->getSdk()), destroy);
    if (!plugin) {
        const std::string errorMessage = logPrefix + "plugin is null.";
        helper::logError(errorMessage);
        throw std::runtime_error(errorMessage);
    }

    this->mPlugin = plugin;
    this->mDescription = pluginDesc;
}

LoaderWrapper::~LoaderWrapper() {
    TRACE_METHOD();
    this->mPlugin.reset();
}

std::string LoaderWrapper::versionToString(const RaceVersionInfo &version) {
    std::stringstream versionString;
    versionString << version.major << "." << version.minor << "." << version.compatibility;
    return versionString.str();
}

}  // namespace Raceboat
