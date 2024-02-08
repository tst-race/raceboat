
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

#include "PluginLoader.h"

#include <string.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "Composition.h"
#include "Core.h"
#include "DecomposedPluginLoader.h"
#include "DynamicLibrary.h"
#include "FileSystem.h"
#include "LoaderWrapper.h"
#include "PluginDef.h"
#include "PluginWrapper.h"
#include "PythonLoaderWrapper.h"
#include "helper.h"

using json = nlohmann::json;

namespace Raceboat {

/**
 * @brief Load a plugin using the provided definition.
 *
 * @param pluginDef A plugin to load.
 * @param core The core.
 * @return std::unique_ptr<PluginContainer> The loaded plugin.
 */
static std::unique_ptr<PluginContainer> loadPlugin(PluginDef pluginDef, Core &core) {
    TRACE_FUNCTION();
    FileSystem &fileSystem = core.getFS();

    auto container = std::make_unique<PluginContainer>();
    container->id = pluginDef.filePath;
    container->sdk = std::make_unique<SdkWrapper>(*container, core);

    switch (pluginDef.fileType) {
        case RaceEnums::PluginFileType::PFT_PYTHON: {
#ifdef PYTHON_PLUGIN_SUPPORT
            const fs::path fullPluginPath =
                fileSystem.makePluginInstallPath(pluginDef.sharedLibraryPath, pluginDef.filePath);
            helper::logDebug(logPrefix + "loading Python plugin: " + fullPluginPath.string());
            try {
                container->plugin =
                    std::make_unique<PythonLoaderWrapper>(*container, core, pluginDef);
            } catch (const std::exception &e) {
                helper::logError(logPrefix + "Exception loading plugin " + fullPluginPath.string() +
                                 ": " + e.what());
                // Don't set container->plugin
            } catch (...) {
                helper::logError(logPrefix + "Unknown exception loading plugin " +
                                 fullPluginPath.string());
                // Don't set container->plugin
            }
#else
            helper::logError(logPrefix + "Python plugin support not compiled");
#endif
            break;
        }
        case RaceEnums::PluginFileType::PFT_SHARED_LIB: {
            const fs::path fullPluginPath =
                fileSystem.makePluginInstallPath(pluginDef.sharedLibraryPath, pluginDef.filePath);
            helper::logDebug(logPrefix +
                             "loading shared library plugin: " + fullPluginPath.string());
            try {
                container->plugin =
                    std::make_unique<LoaderWrapper>(*container, core, fullPluginPath.string());
            } catch (const std::exception &e) {
                helper::logError(logPrefix + "Exception loading plugin " + fullPluginPath.string() +
                                 ": " + e.what());
                // Don't set container->plugin
            } catch (...) {
                helper::logError(logPrefix + "Unknown exception loading plugin " +
                                 fullPluginPath.string());
                // Don't set container->plugin
            }
            break;
        }
        default: {
            helper::logError(logPrefix +
                             "Invalid plugin file type (This should never happen) type: " +
                             std::to_string(static_cast<std::int32_t>(pluginDef.fileType)));
        }
    }

    if (container->plugin != nullptr) {
        return container;
    } else {
        return nullptr;
    }
}

class LoadablePlugin {
public:
    virtual std::unique_ptr<PluginContainer> loadPlugin(Core &core) = 0;
    virtual std::vector<ChannelId> channelIdsForPlugin() = 0;
};

class LoadableUnifiedPlugin : public LoadablePlugin {
public:
    LoadableUnifiedPlugin(PluginDef pluginDef) : pluginDef(pluginDef) {}
    virtual ~LoadableUnifiedPlugin() {}
    virtual std::unique_ptr<PluginContainer> loadPlugin(Core &core) override {
        return Raceboat::loadPlugin(pluginDef, core);
    }
    virtual std::vector<ChannelId> channelIdsForPlugin() override {
        return pluginDef.channels;
    }

private:
    PluginDef pluginDef;
};

class LoadableComposition : public LoadablePlugin {
public:
    LoadableComposition(DecomposedPluginLoader &loader, Composition composition) :
        loader(loader), composition(composition) {}
    virtual ~LoadableComposition() {}
    virtual std::unique_ptr<PluginContainer> loadPlugin(Core &core) override {
        return loader.compose(composition, core);
    }
    virtual std::vector<ChannelId> channelIdsForPlugin() override {
        return {composition.id};
    }

private:
    DecomposedPluginLoader &loader;
    Composition composition;
};

class PluginLoader : public IPluginLoader {
public:
    PluginLoader(Core &core) :
        core(core), decomposedPluginLoader(std::make_unique<DecomposedPluginLoader>(core.getFS())) {
        std::vector<PluginDef> unified_plugin_defs;
        std::vector<PluginDef> decomposed_plugins;
        for (auto &manifest : core.getConfig().manifests) {
            for (auto &pluginDef : manifest.plugins) {
                if (pluginDef.is_unified_plugin()) {
                    unified_plugin_defs.push_back(pluginDef);
                }
                if (pluginDef.is_decomposed_plugin()) {
                    decomposed_plugins.push_back(pluginDef);
                }
            }
        }

        for (auto pluginDef : unified_plugin_defs) {
            auto loader = std::make_shared<LoadableUnifiedPlugin>(pluginDef);
            for (auto &channelId : pluginDef.channels) {
                helper::logDebug("Unified channel: " + channelId + " available");
                channelPluginLoaderMap[channelId] = loader;
            }
        }

        decomposedPluginLoader->loadComponents(decomposed_plugins);
        for (auto &manifest : core.getConfig().manifests) {
            for (auto &composition : manifest.compositions) {
                helper::logDebug("Decomposed channel: " + composition.id + " available");
                auto loader =
                    std::make_shared<LoadableComposition>(*decomposedPluginLoader, composition);
                channelPluginLoaderMap[composition.id] = loader;
            }
        }
        helper::logDebug("channelPluginLoaderMap.size(): " +
                         std::to_string(channelPluginLoaderMap.size()));
    }

    virtual ~PluginLoader() {
        for (auto &plugin : loadedPlugins) {
            if (plugin) {
                plugin->plugin->shutdown();
            }
        }
    }

    virtual PluginContainer *getChannel(ChannelId channelId) {
        TRACE_METHOD(channelId);
        std::unique_lock<std::mutex> lock(pluginListMutex);
        auto it = channelMap.find(channelId);
        if (it != channelMap.end()) {
            return it->second;
        } else {
            // match channel with pluginDef / composition
            // load plugin (store in vec)
            // insert plugin into channel map
            // return reference

            // return nullptr if channel can't be loaded
            auto loader = channelPluginLoaderMap.find(channelId);
            if (loader == channelPluginLoaderMap.end()) {
                helper::logError(logPrefix + "Invalid channel id: " + channelId);
                return nullptr;
            }
            auto plugin = loader->second->loadPlugin(core);
            PluginContainer *pluginPtr = plugin.get();
            loadedPlugins.emplace_back(std::move(plugin));
            for (auto &pluginChannelId : loader->second->channelIdsForPlugin()) {
                channelMap.emplace(pluginChannelId, pluginPtr);
            }
            // pluginPtr is guaranteed to remain valid. We only needed the lock for accessing the
            // channel map, and we don't want to hold it while calling into the plugin. Init is
            // synchronous and blocks until the plugin returns.
            lock.unlock();
            PluginConfig pluginConfig = getPluginConfig(*pluginPtr);
            pluginPtr->plugin->init(pluginConfig);
            return pluginPtr;
        }
    }

protected:
    PluginConfig getPluginConfig(PluginContainer &pluginContainer) {
        PluginConfig pluginConfig;
        pluginConfig.etcDirectory = core.getFS().makeRaceDir("etc", pluginContainer.id);
        pluginConfig.loggingDirectory = core.getFS().makeRaceDir("logging", pluginContainer.id);
        pluginConfig.auxDataDirectory = core.getFS().makeRaceDir("aux", pluginContainer.id);
        pluginConfig.pluginDirectory = core.getFS().makePluginInstallPath("", pluginContainer.id);

        // TODO: use system tmp directory?
        pluginConfig.tmpDirectory = core.getFS().makeRaceDir("tmp", pluginContainer.id);
        return pluginConfig;
    }

protected:
    Core &core;
    std::unique_ptr<DecomposedPluginLoader> decomposedPluginLoader;

    // All access to either the plugin vector or the channel map after initialization should be
    // protected by this mutex
    std::mutex pluginListMutex;
    std::vector<std::unique_ptr<PluginContainer>> loadedPlugins;
    std::unordered_map<ChannelId, PluginContainer *> channelMap;

    std::unordered_map<ChannelId, std::shared_ptr<LoadablePlugin>> channelPluginLoaderMap;
};

std::unique_ptr<IPluginLoader> IPluginLoader::construct(Core &core) {
    return std::make_unique<PluginLoader>(core);
}

}  // namespace Raceboat