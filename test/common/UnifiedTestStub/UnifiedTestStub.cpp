
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

#include "UnifiedTestStub.h"

#include "common/RaceLog.h"

UnifiedTestStub::UnifiedTestStub(IRaceSdkComms *) {}

UnifiedTestStub::~UnifiedTestStub() {}

PluginResponse UnifiedTestStub::init(const PluginConfig &pluginConfig) {
    RaceLog::logDebug("DecomposedStub",
                      "pluginConfig.auxDataDirectory: " + pluginConfig.auxDataDirectory, "");
    RaceLog::logDebug("DecomposedStub", "pluginConfig.etcDirectory: " + pluginConfig.etcDirectory,
                      "");
    RaceLog::logDebug("DecomposedStub",
                      "pluginConfig.loggingDirectory: " + pluginConfig.loggingDirectory, "");
    RaceLog::logDebug("DecomposedStub",
                      "pluginConfig.pluginDirectory: " + pluginConfig.pluginDirectory, "");
    RaceLog::logDebug("DecomposedStub", "pluginConfig.tmpDirectory: " + pluginConfig.tmpDirectory,
                      "");
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::shutdown() {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::sendPackage(RaceHandle, ConnectionID, EncPkg, double, uint64_t) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::openConnection(RaceHandle, LinkType, LinkID, std::string, int32_t) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::closeConnection(RaceHandle, ConnectionID) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::destroyLink(RaceHandle, LinkID) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::createLink(RaceHandle, std::string) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::loadLinkAddress(RaceHandle, std::string, std::string) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::loadLinkAddresses(RaceHandle, std::string,
                                                  std::vector<std::string>) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::createLinkFromAddress(RaceHandle, std::string, std::string) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::activateChannel(RaceHandle, std::string, std::string) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::deactivateChannel(RaceHandle, std::string) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::onUserInputReceived(RaceHandle, bool, const std::string &) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::onUserAcknowledgementReceived(RaceHandle) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::createBootstrapLink(RaceHandle, std::string, std::string) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::serveFiles(LinkID, std::string) {
    return PLUGIN_OK;
}

PluginResponse UnifiedTestStub::flushChannel(RaceHandle, std::string, uint64_t) {
    return PLUGIN_OK;
}

IRacePluginComms *createPluginComms(IRaceSdkComms *sdk) {
    return new UnifiedTestStub(sdk);
}

void destroyPluginComms(IRacePluginComms *plugin) {
    delete static_cast<UnifiedTestStub *>(plugin);
}

const RaceVersionInfo raceVersion = RACE_VERSION;
const char *const racePluginId = "UnifiedTestStub";
const char *const racePluginDescription = "Stub plugin for testing loading of unified plugins";