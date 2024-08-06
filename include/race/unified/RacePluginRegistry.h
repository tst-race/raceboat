
//
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

#ifndef __RACE_PLUGIN_REGISTRY_H_
#define __RACE_PLUGIN_REGISTRY_H_

#include <map>
#include <string>
#include <functional>

#include "IRacePluginComms.h"
#include "IRaceSdkComms.h"

class RacePluginRegistration;

class RacePluginRegistry {
public:
    /**
     * @brief Obtain instance of the RACE plugin registry
     *
     */
    static RacePluginRegistry &instance();

    /**
     * @brief Register a unified plugin
     *
     * @param pluginReg Plugin registration info
     */
    void registerPlugin(const RacePluginRegistration &pluginReg);

    /**
     * @brief Get plugin registration for a specific plugin ID
     *
     * @param pluginId Plugin ID
     * @return Plugin registration info
     */
    RacePluginRegistration getPlugin(const std::string &pluginId);

private:
    RacePluginRegistry() = default;
    std::map<std::string, RacePluginRegistration> plugins;
};

class RacePluginRegistration {
public:
    /** Plugin ID */
    std::string pluginId;
    /** Plugin description */
    std::string pluginDescription;
    /** RACE version */
    RaceVersionInfo raceVersion;
    /** Plugin create function */
    std::function<IRacePluginComms *(IRaceSdkComms *)> create;
    /** Plugin destroy function */
    std::function<void(IRacePluginComms *)> destroy;

    inline RacePluginRegistration(
        const std::string &pluginId,
	const std::string &pluginDescription,
	const RaceVersionInfo &raceVersion,
	std::function<IRacePluginComms *(IRaceSdkComms *)> create,
	std::function<void(IRacePluginComms *)> destroy
    ) : pluginId(pluginId), pluginDescription(pluginDescription),
	raceVersion(raceVersion), create(create), destroy(destroy) {
        RacePluginRegistry::instance().registerPlugin(*this);
    }

    RacePluginRegistration() = default;
    //RacePluginRegistration(const RacePluginRegistration &) = default;
    //RacePluginRegistration(RacePluginRegistration &&) = default;
};


#endif
