
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

#include "helper.h"

namespace Raceboat {

FileSystem::FileSystem(const fs::path &pluginsPath,
                       std::unique_ptr<Storage> _storage)
    : pluginsInstallPath(pluginsPath) {
  storage = std::move(_storage);
}

std::vector<std::uint8_t> FileSystem::readFile(const fs::path &filePath,
                                               const std::string &pluginId) {
  TRACE_METHOD(filePath, pluginId);
  fs::path path = makePluginFilePath(filePath, pluginId);
  return storage->read(path);
}

bool FileSystem::appendFile(const fs::path &filePath,
                            const std::string &pluginId,
                            const std::vector<std::uint8_t> &data) {
  TRACE_METHOD(filePath, pluginId);
  fs::path path = makePluginFilePath(filePath, pluginId);
  return storage->append(path, data);
}

bool FileSystem::makeDir(const fs::path &directoryPath,
                         const std::string &pluginId) {
  TRACE_METHOD(directoryPath, pluginId);
  fs::path fullPath = makePluginFilePath(directoryPath, pluginId);
  return createDirectories(fullPath);
}

bool FileSystem::removeDir(const fs::path &directoryPath,
                           const std::string &pluginId) {
  TRACE_METHOD(directoryPath, pluginId);
  bool success = false;
  fs::path fullPath = makePluginFilePath(directoryPath, pluginId);
  std::error_code ec;
  success = fs::remove_all(fullPath, ec);
  if (fs::exists(fullPath)) {
    helper::logError(logPrefix + " failed to remove " + fullPath.string() +
                     " error: " + ec.message());
  }
  return success;
}

std::vector<std::string> FileSystem::listDir(const fs::path &directoryPath,
                                             const std::string &pluginId) {
  TRACE_METHOD(directoryPath, pluginId);
  std::vector<std::string> dirs;
  fs::path fullPath = makePluginFilePath(directoryPath, pluginId);
  if (!fs::exists(fullPath)) {
    helper::logInfo(logPrefix + " path does not exist: " + fullPath.string());
  } else if (!fs::is_directory(fullPath)) {
    helper::logInfo(logPrefix + " path is not directory: " + fullPath.string());
  } else {
    std::error_code ec;
    auto dirIter = fs::directory_iterator(fullPath, ec);
    if (ec.value()) {
      helper::logWarning(logPrefix + " error: " + ec.message());
    }
    for (const auto &entry : dirIter) {
      dirs.push_back(entry.path().string());
    }
  }
  return dirs;
}

bool FileSystem::copy(const fs::path &srcPath, const fs::path &destPath) {
  TRACE_METHOD(srcPath, destPath);
  bool success = true;
  if (!fs::exists(srcPath)) {
    success = false;
    helper::logInfo(logPrefix +
                    " source path does not exist: " + srcPath.string());
  } else {
    try {
      fs::copy(srcPath, destPath,
               fs::copy_options::recursive | fs::copy_options::skip_existing);
    } catch (const std::exception &ex) {
      helper::logWarning(logPrefix + " exception: " + std::string(ex.what()));
    }
    success = fs::exists(destPath);
  }
  return success;
}

bool FileSystem::FileSystem::writeFile(const fs::path &filePath,
                                       const std::string &pluginId,
                                       const std::vector<std::uint8_t> &data) {
  TRACE_METHOD(filePath, pluginId);
  // create the parent directory for the plugin if it doesn't exist
  fs::path path = makePluginFilePath(filePath, pluginId);
  if (!path.has_parent_path()) {
    // should never be the case when using makePluginFilePath()
    helper::logWarning(logPrefix +
                       " path has no parent path: " + path.string());
  } else if (!fs::exists(path.parent_path())) {
    helper::logDebug(logPrefix +
                     " parent path does not exist.  Creating parent path: " +
                     path.parent_path().string());
    createDirectories(path.parent_path());
  }

  return storage->write(path, data);
}

fs::path FileSystem::makePluginFilePath(const fs::path &filePath,
                                        const std::string &pluginId) {
  fs::path path = pluginsInstallPath / "usr" /
                  pluginId; // throws exception if pluginId empty
  if (!filePath.empty()) {  // would throw exception otherwise
    path = path / filePath;
  }
  return path;
}

fs::path FileSystem::makePluginInstallPath(const fs::path &filePath,
                                           const std::string &pluginId) {
  fs::path path = makePluginInstallBasePath() /
                  pluginId; // throws exception if pluginId empty
  if (!filePath.empty()) {  // would throw exception otherwise
    path = path / filePath;
  }
  return path;
}

fs::path FileSystem::makeShimsPath(const std::string &language) {
  // throws exception if language is empty
  fs::path path = pluginsInstallPath / "shims" / language;
  createDirectories(path);
  return path;
}

std::vector<fs::path> FileSystem::listInstalledPluginDirs() {
  TRACE_METHOD();
  fs::path pluginBasePath = makePluginInstallBasePath();
  std::vector<fs::path> dirs;
  if (fs::exists(pluginBasePath) && fs::is_directory(pluginBasePath)) {
    for (const auto &entry : fs::directory_iterator(pluginBasePath)) {
      if (entry.is_directory()) {
        dirs.push_back(entry.path());
      }
    }
  }

  if (dirs.empty()) {
    helper::logError("No directories found in " + pluginBasePath.string());
  }

  return dirs;
}

fs::path FileSystem::makePluginInstallBasePath() {
  fs::path basePath =
      pluginsInstallPath / "plugins" / getHostOsType() / getHostArch();
  createDirectories(basePath);
  return basePath;
}

fs::path FileSystem::makeRaceDir(const fs::path &prefix,
                                 const std::string &pluginId) {
  auto path = pluginsInstallPath / prefix / pluginId;
  createDirectories(path);
  return path;
}

const char *FileSystem::getHostArch() {
  MAKE_LOG_PREFIX();
#if defined(__x86_64__) || defined(_M_X64) || defined(i386) ||                 \
    defined(__i386__) || defined(__i386) || defined(_M_IX86)
  static const char *hostArch = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
  static const char *hostArch = "arm64-v8a";
#else
  helper::logWarning(logPrefix + " unsupported host architecture");
  static const char *hostArch = "unsupported-host-architecture";
#endif
  return hostArch;
}

const char *FileSystem::getHostOsType() {
  MAKE_LOG_PREFIX();
#if defined(__unix__)
  static const char *hostOS = "unix"; // unix-based OSs
#elif defined(__APPLE__)
  static const char *hostOS = "mac";
#elif defined(__ANDROID__)
  static const char *hostOS = "android";
#else
  helper::logWarning(logPrefix + " unsupported host OS");
  static const char *hostOS = "unsupported-host-OS";
#endif
  return hostOS;
}

bool FileSystem::createDirectories(const fs::path &absPath) {
  MAKE_LOG_PREFIX();
  std::error_code ec;
  fs::create_directories(absPath, ec);
  if (!fs::exists(absPath)) {
    helper::logError(logPrefix + " failed to create " + absPath.string() +
                     " error: " + ec.message());
    return false;
  }
  return true;
}

} // namespace Raceboat
