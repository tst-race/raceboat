
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

#include <stdint.h>

#include <filesystem>
#include <string>
#include <vector>

#include "Storage.h"
namespace fs = std::filesystem;

namespace Raceboat {
/**
 * @brief manages plugin-specific directories for Storage item to write to
 **/
struct FileSystem {
  /**
   * @brief Read the contents of a file in this plugin's storage.
   *
   * @param pluginsInstallPath absolute plugins install directory (eg
   * /opt/race/)
   * @param storage (or derived class) used to perform file IO
   **/
  explicit FileSystem(
      const fs::path &pluginsInstallPath,
      std::unique_ptr<Storage> storage = std::make_unique<Storage>());

  /**
   * @brief Read the contents of a file in this plugin's storage.
   *
   * @param filePath The path of the file to be read.
   * @param pluginId The ID of the plugin for which to read the file.
   * @return std::vector<std::uint8_t> The contents of the file, or an empty
   * vector on error.
   */
  virtual std::vector<std::uint8_t> readFile(const fs::path &filePath,
                                             const std::string &pluginId);

  /**
   * @brief Append the contents of data to filepath in this plugin's storage.
   * @param filePath The string path of the file to be appended to (or written).
   * @param pluginId The ID of the plugin for which to append the file.
   * @param data The string of data to append to the file.
   *
   * @return bool indicator of success or failure of the append.
   */
  virtual bool appendFile(const fs::path &filePath, const std::string &pluginId,
                          const std::vector<std::uint8_t> &data);

  /**
   * @brief Create the directory of directoryPath, including any directories in
   * the path that do not yet exist
   * @param directoryPath the path of the directory to create.
   *
   * @return bool indicator of success or failure of the create
   */
  virtual bool makeDir(const fs::path &directoryPath,
                       const std::string &pluginId);

  /**
   * @brief Recursively remove the directory of directoryPath
   * @param directoryPath the path of the directory to remove.
   * @param pluginId The ID of the plugin for which to remove the directory.
   *
   * @return bool indicator of success or failure of the removal
   */
  virtual bool removeDir(const fs::path &directoryPath,
                         const std::string &pluginId);

  /**
   * @brief List the contents (directories and files) of the directory path
   * @param directoryPath the path of the directory to list.
   *
   * @return std::vector<std::string> list of directories and files
   */
  virtual std::vector<std::string> listDir(const fs::path &directoryPath,
                                           const std::string &pluginId);

  /**
   * @brief Recursively copy the contents of the source directory into the
   * destination
   * @param srcPath Source directory path
   * @param destPath Destination directory path
   *
   * @return bool indicator of success or failure directory copy
   */
  virtual bool copy(const fs::path &srcPath, const fs::path &destPath);

  /**
   * @brief Write the contents of data to filepath in this plugin's storage
   * (overwriting if file exists)
   * @param filePath The string path of the file to be written.
   * @param pluginId The ID of the plugin for which to write the file.
   * @param data The string of data to write to the file.
   *
   * @return bool indicator of success or failure of the write.
   */
  virtual bool writeFile(const fs::path &filePath, const std::string &pluginId,
                         const std::vector<std::uint8_t> &data);

  /**
   * @brief Construct a filepath based on a filepath, pluginId, and data
   * directory path for plugin sandbox directory (eg < \p
   * pluginsInstallPath>/usr/< \p pluginId>/< \p filePath>)
   *
   * @param filePath The string name of the file
   * @param pluginId The string pluginId the file is associated with
   * @return fs::path Full path to the filepath
   */
  virtual fs::path makePluginFilePath(const fs::path &filePath,
                                      const std::string &pluginId);

  /**
   * @brief Construct the anticipated plugin install filepath based on a
   * filepath, pluginId, and data directory path for plugin binary directory (eg
   * < \p pluginsInstallPath>/plugins/<hostOS>/<hostArch>/< \p pluginId>/< \p
   * filePath>)
   *
   * @param filePath The string name of the file
   * @param pluginId The string pluginId the file is associated with
   * @return fs::path Full path to the filepath
   */
  virtual fs::path makePluginInstallPath(const fs::path &filePath,
                                         const std::string &pluginId);

  /**
   * @brief Construct the etc path for a plugin
   * (eg < \p pluginsInstallPath>/< \p prefix>/< \p pluginId>/)
   *
   * @param pluginId The string pluginId the file is associated with
   * @return fs::path Full path to the filepath
   */
  virtual fs::path makeRaceDir(const fs::path &prefix,
                               const std::string &pluginId);

  /**
   * @brief Construct the etc path for language shims binaries
   * (eg < \p pluginsInstallPath>/shims/< \p language>)
   *
   * @param pluginId The string pluginId the file is associated with
   * @return fs::path Path to the directory for shims for the specified language
   */
  virtual fs::path makeShimsPath(const std::string &language);

  /**
   * @brief Construct the anticipated plugin installation path for all plugins
   * @return std::vector<fs::path> all directories in user-specified plugin
   * install path
   */
  virtual std::vector<fs::path> listInstalledPluginDirs();

  virtual fs::path makePluginInstallBasePath();

protected:
  virtual const char *getHostArch();
  virtual const char *getHostOsType();

  bool createDirectories(const fs::path &absPath);

  std::unique_ptr<Storage> storage;

public:
  /**
   * @brief absolute plugins install directory set by constructor
   **/
  const fs::path pluginsInstallPath;
};
} // namespace Raceboat
