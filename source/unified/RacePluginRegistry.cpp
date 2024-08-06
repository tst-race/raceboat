#include "race/unified/RacePluginRegistry.h"

RacePluginRegistry &RacePluginRegistry::instance() {
    static RacePluginRegistry _instance;
    return _instance;
}

void RacePluginRegistry::registerPlugin(const RacePluginRegistration &pluginReg) {
    plugins[pluginReg.pluginId] = pluginReg;
}

RacePluginRegistration RacePluginRegistry::getPlugin(const std::string &pluginId) {
    return plugins.at(pluginId);
}
