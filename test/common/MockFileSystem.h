
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

#ifndef __MOCK_FILE_SYSTEM_H_
#define __MOCK_FILE_SYSTEM_H_

#include "FileSystem.h"
#include "gmock/gmock.h"

using namespace RaceLib;

class MockFileSystem : public FileSystem {
public:
    explicit MockFileSystem(const std::string pluginsInstallPath, std::unique_ptr<Storage> storage = std::make_unique<Storage>()) :
        FileSystem(pluginsInstallPath, std::move(storage)) {}

    MOCK_METHOD(std::vector<std::uint8_t>, readFile,
                (const fs::path &filePath, const std::string &pluginId), (override));

    MOCK_METHOD(bool, appendFile,
                (const fs::path &filePath, const std::string &pluginId,
                 const std::vector<std::uint8_t> &data),
                (override));

    MOCK_METHOD(bool, makeDir, (const fs::path &directoryPath, const std::string &pluginId),
                (override));

    MOCK_METHOD(bool, removeDir, (const fs::path &directoryPath, const std::string &pluginId),
                (override));

    MOCK_METHOD(std::vector<std::string>, listDir,
                (const fs::path &directoryPath, const std::string &pluginId), (override));

    MOCK_METHOD(bool, copy, (const fs::path &srcPath, const fs::path &destPath), (override));

    MOCK_METHOD(bool, writeFile,
                (const fs::path &filePath, const std::string &pluginId,
                 const std::vector<std::uint8_t> &data),
                (override));

    MOCK_METHOD(fs::path, makePluginFilePath,
                (const fs::path &filePath, const std::string &pluginName), (override));

    MOCK_METHOD(fs::path, makePluginInstallPath,
                (const fs::path &filePath, const std::string &pluginName), (override));

    MOCK_METHOD(std::vector<fs::path>, listInstalledPluginDirs, (), (override));

    MOCK_METHOD(fs::path, makePluginInstallBasePath, (), (override));
    MOCK_METHOD(const char *, getHostArch, (), (override));
    MOCK_METHOD(const char *, getHostOsType, (), (override));
};
#endif  // __MOCK_FILE_SYSTEM_H_