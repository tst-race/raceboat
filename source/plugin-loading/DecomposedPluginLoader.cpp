
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

#include "DecomposedPluginLoader.h"

#include "CompositeWrapper.h"
#include "Core.h"
#include "decomposed/ComponentManager.h"
#include "helper.h"

namespace RaceLib {

DecomposedPluginLoader::DecomposedPluginLoader(FileSystem &fs) : fs(fs) {}

void DecomposedPluginLoader::loadComponentsForPlugin(const PluginDef &pluginToLoad) {
    TRACE_METHOD(pluginToLoad.filePath, pluginToLoad.sharedLibraryPath);

    std::shared_ptr<IComponentPlugin> plugin = nullptr;
    std::string fullPluginPath{};

    // TODO FileSystem in place of fullPluginPath
    if (pluginToLoad.fileType == RaceEnums::PFT_SHARED_LIB) {
        fullPluginPath =
            fs.makePluginInstallPath(pluginToLoad.sharedLibraryPath, pluginToLoad.filePath);
        helper::logDebug(logPrefix + "Loading component shared library plugin from " +
                         fullPluginPath);
        plugin = std::make_unique<ComponentPlugin>(const_cast<std::string &>(fullPluginPath));
    } else if (pluginToLoad.fileType == RaceEnums::PFT_PYTHON) {
#ifdef PYTHON_PLUGIN_SUPPORT
        fullPluginPath = fs.makePluginInstallPath("", pluginToLoad.filePath);
        plugin = std::make_unique<PythonComponentPlugin>(
            const_cast<std::string &>(fullPluginPath), pluginToLoad.pythonModule,
            fs.makePluginInstallBasePath().string(), fs.makeShimsPath("python").string());
        helper::logDebug(logPrefix + "Loading component python plugin from " + fullPluginPath);
#else
        helper::logError(logPrefix + "Python plugin support not compilied");
#endif
    } else {
        throw std::runtime_error("Unknown plugin type ");
    }

    for (auto &transport : pluginToLoad.transports) {
        auto it = transports.find(transport);
        if (it != transports.end()) {
            helper::logError(logPrefix + "transport " + transport +
                             " already exists. Previous transport supplied by " +
                             it->second->get_path() + ". New transport supplied by " +
                             fullPluginPath);

            throw std::runtime_error("Multiple definitions of transport" + transport);
        }
        transports[transport] = plugin.get();
    }
    for (auto &usermodel : pluginToLoad.usermodels) {
        auto it = usermodels.find(usermodel);
        if (it != usermodels.end()) {
            helper::logError(logPrefix + "usermodel " + usermodel +
                             " already exists. Previous usermodel supplied by " +
                             it->second->get_path() + ". New usermodel supplied by " +
                             fullPluginPath);

            throw std::runtime_error("Multiple definitions of usermodel" + usermodel);
        }
        usermodels[usermodel] = plugin.get();
    }
    for (auto &encoding : pluginToLoad.encodings) {
        auto it = encodings.find(encoding);
        if (it != encodings.end()) {
            helper::logError(logPrefix + "encoding " + encoding +
                             " already exists. Previous encoding supplied by " +
                             it->second->get_path() + ". New encoding supplied by " +
                             fullPluginPath);

            throw std::runtime_error("Multiple definitions of encoding" + encoding);
        }
        encodings[encoding] = plugin.get();
    }
    plugins.emplace_back(std::move(plugin));
}

void DecomposedPluginLoader::loadComponents(std::vector<PluginDef> componentPlugins) {
    TRACE_METHOD();
    for (auto &plugin : componentPlugins) {
        loadComponentsForPlugin(plugin);
    }

    helper::logDebug(logPrefix + "Loaded plugins containing:");
    helper::logDebug(logPrefix + "Transports:");
    for (auto &kv : transports) {
        helper::logDebug(logPrefix + "    " + kv.first);
    }
    helper::logDebug(logPrefix + "User Models:");
    for (auto &kv : usermodels) {
        helper::logDebug(logPrefix + "    " + kv.first);
    }
    helper::logDebug(logPrefix + "Encodings:");
    for (auto &kv : encodings) {
        helper::logDebug(logPrefix + "    " + kv.first);
    }
}

std::unique_ptr<PluginContainer> DecomposedPluginLoader::compose(Composition composition,
                                                                 Core &core) {
    TRACE_METHOD();
    auto description = composition.description();
    helper::logDebug(logPrefix + "Creating composition: " + description);

    auto &transport = *transports.at(composition.transport);
    auto &usermodel = *usermodels.at(composition.usermodel);
    std::unordered_map<std::string, IComponentPlugin *> compositeEncodings;
    for (auto &encoding : composition.encodings) {
        compositeEncodings[encoding] = encodings.at(encoding);
    }

    auto container = std::make_unique<PluginContainer>();
    container->id = composition.id;
    container->sdk = std::make_unique<SdkWrapper>(*container, core);
    container->plugin = std::make_unique<CompositeWrapper>(
        *container, core, composition, description, transport, usermodel, compositeEncodings);

    return container;
}
}  // namespace RaceLib
