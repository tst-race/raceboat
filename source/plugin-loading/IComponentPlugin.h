
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
#include <memory.h>

#include "race/common/PluginConfig.h"
#include "race/decomposed/IEncodingComponent.h"
#include "race/decomposed/ITransportComponent.h"
#include "race/decomposed/IUserModelComponent.h"

namespace RaceLib {

struct IComponentPlugin {
public:
    virtual ~IComponentPlugin() = default;

    virtual std::shared_ptr<ITransportComponent> createTransport(
        const std::string &name, ITransportSdk *sdk, const std::string &roleName,
        const PluginConfig &pluginConfig) = 0;
    virtual std::shared_ptr<IUserModelComponent> createUserModel(
        const std::string &name, IUserModelSdk *sdk, const std::string &roleName,
        const PluginConfig &pluginConfig) = 0;
    virtual std::shared_ptr<IEncodingComponent> createEncoding(
        const std::string &name, IEncodingSdk *sdk, const std::string &roleName,
        const PluginConfig &pluginConfig) = 0;
    virtual std::string get_path() = 0;
};

}  // namespace RaceLib
