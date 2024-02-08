
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

#ifdef PYTHON_PLUGIN_SUPPORT
#include "Core.h"
#include "FileSystem.h"
#include "PluginContainer.h"
#include "PluginWrapper.h"

namespace Raceboat {

/**
 * @brief Class for handling loading of a Python plugin. Will initialize the Python interpreter and
 * load a plugin for a given plugin ID.
 *
 * @tparam Parent The Parent wrapper class. This will always be PluginWrapper for the standalone
 * library.
 */
class PythonLoaderWrapper : public PluginWrapper {
private:
    using Interface = IRacePluginComms;
    using SDK = IRaceSdkComms;

public:
    PythonLoaderWrapper(PluginContainer &container, Core &core, const PluginDef &pluginDef);

    // Ensure Parent type has a virtual destructor (required for unique_ptr conversion)
    virtual ~PythonLoaderWrapper();
};

}  // namespace Raceboat
#endif
