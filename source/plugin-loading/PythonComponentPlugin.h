
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
#include <memory>

#include "IComponentPlugin.h"
#include "race/common/PluginConfig.h"
#include "race/decomposed/IEncodingComponent.h"
#include "race/decomposed/ITransportComponent.h"
#include "race/decomposed/IUserModelComponent.h"

namespace RaceLib {

static const std::string SDK_TYPE_TRANSPORT = "ITransportSdk*";
static const std::string SDK_TYPE_ENCODING = "IEncodingSdk*";
static const std::string SDK_TYPE_USER_MODEL = "IUserModelSdk*";

static const std::string PLUGIN_TYPE_TRANSPORT = "ITransportComponent*";
static const std::string PLUGIN_TYPE_ENCODING = "IEncodingComponent*";
static const std::string PLUGIN_TYPE_USER_MODEL = "IUserModelComponent*";

static const std::string FUNC_CREATE_TRANSPORT = "createTransport";
static const std::string FUNC_CREATE_USER_MODEL = "createUserModel";
static const std::string FUNC_CREATE_ENCODING = "createEncoding";

static const std::string ARG_PLUGIN_CONFIG = "PluginConfig*";

struct PythonComponentPlugin : public IComponentPlugin {
public:
    explicit PythonComponentPlugin(const std::string &path, const std::string &pythonModule,
                                   const std::string &pythonModulePath,
                                   const std::string &pythonShimsPath);
    virtual ~PythonComponentPlugin() {}

    virtual std::shared_ptr<ITransportComponent> createTransport(
        const std::string &name, ITransportSdk *sdk, const std::string &roleName,
        const PluginConfig &pluginConfig) override;
    virtual std::shared_ptr<IUserModelComponent> createUserModel(
        const std::string &name, IUserModelSdk *sdk, const std::string &roleName,
        const PluginConfig &pluginConfig) override;
    virtual std::shared_ptr<IEncodingComponent> createEncoding(
        const std::string &name, IEncodingSdk *sdk, const std::string &roleName,
        const PluginConfig &pluginConfig) override;

    virtual std::string get_path() override;

private:
    std::string path;
    std::string pythonModule;
    std::string pythonModulePath;
    std::string pythonShimsPath;
};

}  // namespace RaceLib
#endif
