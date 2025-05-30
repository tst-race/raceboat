
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

#include <memory>

#include "DynamicLibrary.h"
#include "IComponentPlugin.h"
#include "PluginConfig.h"
#include "race/decomposed/IEncodingComponent.h"
#include "race/decomposed/ITransportComponent.h"
#include "race/decomposed/IUserModelComponent.h"

namespace fs = std::filesystem;

namespace Raceboat {

struct ComponentPlugin : public IComponentPlugin {
public:
  explicit ComponentPlugin(const std::string &path);
  virtual ~ComponentPlugin() {}
  virtual std::shared_ptr<ITransportComponent>
  createTransport(const std::string &name, ITransportSdk *sdk,
                  const std::string &roleName,
                  PluginConfig &pluginConfig) override;
  virtual std::shared_ptr<IUserModelComponent>
  createUserModel(const std::string &name, IUserModelSdk *sdk,
                  const std::string &roleName,
                  PluginConfig &pluginConfig) override;
  virtual std::shared_ptr<IEncodingComponent>
  createEncoding(const std::string &name, IEncodingSdk *sdk,
                 const std::string &roleName,
                 PluginConfig &pluginConfig) override;

  virtual std::string get_path() override;

private:
  void init();
  void initTransport();
  void initUserModel();
  void initEncoding();

  std::string path;

  DynamicLibrary dl;

  using createTransportImplType = ITransportComponent *(const std::string &,
                                                        ITransportSdk *sdk,
                                                        const std::string &,
                                                        PluginConfig &);
  createTransportImplType *createTransportImpl;

  using destroyTransportImplType = void(ITransportComponent *);
  destroyTransportImplType *destroyTransportImpl;

  using createUserModelImplType = IUserModelComponent *(const std::string &,
                                                        IUserModelSdk *sdk,
                                                        const std::string &,
                                                        PluginConfig &);
  createUserModelImplType *createUserModelImpl;

  using destroyUserModelImplType = void(IUserModelComponent *);
  destroyUserModelImplType *destroyUserModelImpl;

  using createEncodingImplType = IEncodingComponent *(const std::string &,
                                                      IEncodingSdk *sdk,
                                                      const std::string &,
                                                      PluginConfig &);
  createEncodingImplType *createEncodingImpl;

  using destroyEncodingImplType = void(IEncodingComponent *);
  destroyEncodingImplType *destroyEncodingImpl;
};
} // namespace Raceboat
