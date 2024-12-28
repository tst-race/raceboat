
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

#ifndef RACE_ENUMS_H
#define RACE_ENUMS_H

#include <string>

namespace RaceEnums {
/**
 * @brief The file type of the plugin. Currently either a shared library or
 * Python code.
 *
 */
enum PluginFileType { PFT_SHARED_LIB, PFT_PYTHON };

/**
 * @brief Convert a PluginFileType value to a human readable string. This
 * function is strictly for logging and debugging. The output formatting may
 * change without notice. Do NOT use this for any logical comparisons, etc. The
 * functionality of your plugin should in no way rely on the output of this
 * function.
 *
 * @param pluginFileType The value to convert to a string.
 * @return std::string The string representation of the provided value.
 */
std::string pluginFileTypeToString(PluginFileType pluginFileType);
std::ostream &operator<<(std::ostream &out, PluginFileType pluginFileType);

/**
 * @brief Convert a plugin file type string to a plugin file type enum. If the
 * plugin file type string is invalid then an exception will be thrown.
 *
 * @param pluginFileTypeString The plugin file type as a string, either "python"
 * or "shared_library".
 * @return PluginDef::PluginType The corresponding plugin file type enum.
 */
PluginFileType stringToPluginFileType(const std::string &pluginFileTypeString);

/**
 * @brief The type of information to display to the User
 *
 */
enum UserDisplayType {
  UD_DIALOG = 0,
  UD_QR_CODE = 1,
  UD_TOAST = 2,
  UD_NOTIFICATION = 3,
  UD_UNDEF = 4
};

/**
 * @brief The type of action the target node's RACE Daemon should take
 *
 */
enum BootstrapActionType {
  BS_DOWNLOAD_BUNDLE = 0,
  BS_NETWORK_CONNECT = 1,
  BS_COMPLETE = 2,
  BS_UNDEF = 3
};

} // namespace RaceEnums

#endif
