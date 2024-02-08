
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

#include "race/common/RaceEnums.h"

#include <algorithm>
#include <stdexcept>

/**
 * @brief Convert a string to lowercase.
 *
 * @param input The string to convert.
 * @return std::string The lower case version of the input string.
 */
static std::string stringToLowerCase(const std::string &input) {
  std::string output = input;
  std::transform(output.begin(), output.end(), output.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return output;
}

std::string
RaceEnums::pluginFileTypeToString(RaceEnums::PluginFileType pluginFileType) {
  switch (pluginFileType) {
  case RaceEnums::PFT_SHARED_LIB:
    return "PFT_SHARED_LIB";
  case RaceEnums::PFT_PYTHON:
    return "PFT_PYTHON";
  default:
    return "ERROR: INVALID PLUGIN FILE TYPE: " + std::to_string(pluginFileType);
  }
}

std::ostream &RaceEnums::operator<<(std::ostream &out,
                                    RaceEnums::PluginFileType pluginFileType) {
  return out << RaceEnums::pluginFileTypeToString(pluginFileType);
}

RaceEnums::PluginFileType
RaceEnums::stringToPluginFileType(const std::string &pluginFileTypeString) {
  std::string pluginFileType = stringToLowerCase(pluginFileTypeString);
  if (pluginFileType == "shared_library") {
    return RaceEnums::PluginFileType::PFT_SHARED_LIB;
  } else if (pluginFileType == "python") {
    return RaceEnums::PluginFileType::PFT_PYTHON;
  }

  throw std::invalid_argument(
      "RaceEnums::stringToPluginFileType: invalid plugin file type " +
      pluginFileTypeString);
}
