
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

#include <cctype>
#include <memory>
#include <sstream>

#include "ComponentPlugin.h"
#include "DynamicLibrary.h"
#include "PluginContainer.h"
#include "PluginWrapper.h"
#include "SdkWrapper.h"
#include "decomposed/ComponentManager.h"
#include "helper.h"
#include "race/common/RacePluginExports.h"

namespace Raceboat {

class CompositeWrapper : public PluginWrapper {
public:
    CompositeWrapper(PluginContainer &container, Core &core, Composition composition,
                     const std::string &description, IComponentPlugin &transport,
                     IComponentPlugin &usermodel,
                     const std::unordered_map<std::string, IComponentPlugin *> &encodings);
    virtual ~CompositeWrapper() override;
};

}  // namespace Raceboat
