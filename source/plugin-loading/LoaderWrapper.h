
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

#include "DynamicLibrary.h"
#include "FileSystem.h"
#include "PluginContainer.h"
#include "PluginWrapper.h"
#include "SdkWrapper.h"
#include "helper.h"
#include "race/common/RacePluginExports.h"

namespace Raceboat {

class Core;

class LoaderWrapper : public PluginWrapper {
private:
  DynamicLibrary dl;
  using Interface = IRacePluginComms;
  using SDK = IRaceSdkComms;

  // These are the names of symbols in the dynamic library
  static constexpr const char *createFuncName = "createPluginComms";
  static constexpr const char *destroyFuncName = "destroyPluginComms";

public:
  LoaderWrapper(PluginContainer &container, Core &core, const fs::path &path);

  // Ensure PluginWrapper type has a virtual destructor (required for unique_ptr
  // conversion)
  virtual ~LoaderWrapper() override;

protected:
  static std::string versionToString(const RaceVersionInfo &version);
};

} // namespace Raceboat
