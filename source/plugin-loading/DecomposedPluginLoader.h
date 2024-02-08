
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

#include <vector>

#include "ComponentPlugin.h"
#include "IComponentPlugin.h"
#include "PluginContainer.h"
#include "PluginDef.h"
#include "PluginLoader.h"
#include "PythonComponentPlugin.h"
#include "race/common/RaceEnums.h"

namespace Raceboat {

class Core;
class PluginWrapper;

class DecomposedPluginLoader {
public:
    explicit DecomposedPluginLoader(FileSystem &fs);
    void loadComponents(std::vector<PluginDef> componentPlugins);
    std::unique_ptr<PluginContainer> compose(Composition composition, Core &core);

protected:
    void loadComponentsForPlugin(const PluginDef &pluginToLoad);

public:
    FileSystem &fs;
    std::vector<std::shared_ptr<IComponentPlugin>> plugins;
    std::unordered_map<std::string, IComponentPlugin *> transports;
    std::unordered_map<std::string, IComponentPlugin *> usermodels;
    std::unordered_map<std::string, IComponentPlugin *> encodings;
};

}  // namespace Raceboat
