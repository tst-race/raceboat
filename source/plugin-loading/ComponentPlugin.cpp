
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

#include "FileSystem.h"
#include "ComponentPlugin.h"

namespace Raceboat {

ComponentPlugin::ComponentPlugin(const std::string &path)
    : path(path), createTransportImpl(nullptr), destroyTransportImpl(nullptr),
      createUserModelImpl(nullptr), destroyUserModelImpl(nullptr),
      createEncodingImpl(nullptr), destroyEncodingImpl(nullptr) {}

void ComponentPlugin::init() {
  if (dl.is_loaded()) {
    return;
  }
  dl.open(path);
}

void ComponentPlugin::initTransport() {
  if (createTransportImpl) {
    return;
  }
  init();
  dl.get(createTransportImpl, "createTransport");
  dl.get(destroyTransportImpl, "destroyTransport");
}
void ComponentPlugin::initUserModel() {
  if (createUserModelImpl) {
    return;
  }
  init();
  dl.get(createUserModelImpl, "createUserModel");
  dl.get(destroyUserModelImpl, "destroyUserModel");
}
void ComponentPlugin::initEncoding() {
  if (createEncodingImpl) {
    return;
  }
  init();
  dl.get(createEncodingImpl, "createEncoding");
  dl.get(destroyEncodingImpl, "destroyEncoding");
}

std::shared_ptr<ITransportComponent>
ComponentPlugin::createTransport(const std::string &name, ITransportSdk *sdk,
                                 const std::string &roleName,
                                 PluginConfig &pluginConfig) {
  TRACE_METHOD(path, name);
  initTransport();
  fs::path pluginDirectory = path;
  pluginConfig.pluginDirectory = pluginDirectory.remove_filename().c_str();
  ITransportComponent *transport =
      createTransportImpl(name.c_str(), sdk, roleName.c_str(), pluginConfig);
  return std::shared_ptr<ITransportComponent>(transport, destroyTransportImpl);
}

std::shared_ptr<IUserModelComponent>
ComponentPlugin::createUserModel(const std::string &name, IUserModelSdk *sdk,
                                 const std::string &roleName,
                                 PluginConfig &pluginConfig) {
  TRACE_METHOD(path, name);
  initUserModel();
  fs::path pluginDirectory = path;
  pluginConfig.pluginDirectory = pluginDirectory.remove_filename().c_str();
  return std::shared_ptr<IUserModelComponent>(
      createUserModelImpl(name.c_str(), sdk, roleName.c_str(), pluginConfig),
      destroyUserModelImpl);
}

std::shared_ptr<IEncodingComponent>
ComponentPlugin::createEncoding(const std::string &name, IEncodingSdk *sdk,
                                const std::string &roleName,
                                PluginConfig &pluginConfig) {
  TRACE_METHOD(path, name);
  initEncoding();
  fs::path pluginDirectory = path;
  pluginConfig.pluginDirectory = pluginDirectory.remove_filename().c_str();
  return std::shared_ptr<IEncodingComponent>(
      createEncodingImpl(name.c_str(), sdk, roleName.c_str(), pluginConfig),
      destroyEncodingImpl);
}

std::string ComponentPlugin::get_path() { return path; }

} // namespace Raceboat
