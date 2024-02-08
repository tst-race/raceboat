
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

#include <nlohmann/json.hpp>

#include "../../source/FileSystem.h"
#include "../../source/Storage.h"
#include "gtest/gtest.h"

#ifndef INPUT_FILE_DIR
#define INPUT_FILE_DIR "Unknown"
#endif

using namespace RaceLib;
using namespace testing;

struct FileSystemTestable : public FileSystem {
    FileSystemTestable(const fs::path &path) : FileSystem(path) {}
    virtual const char *getHostArch() {
        return "myArch";
    }
    virtual const char *getHostOsType() {
        return "myOS";
    }
};

class FileSystemTestFixture : public ::testing::Test {
public:
    FileSystemTestFixture() :
        pluginId("examplePlugin1"),
        fs(INPUT_FILE_DIR, std::make_unique<Storage>()),
        fileName("testFile"),
        testPath(fs::path("test") / "path"),
        fileContents("hello") {
        bytes = std::vector<uint8_t>(fileContents.begin(), fileContents.end());
        fullTestPath = fs.makePluginFilePath(testPath, pluginId);
        fullFilePath = fs.makePluginFilePath(fileName, pluginId);
    }

    const std::string pluginId;
    FileSystem fs;
    fs::path fileName;
    fs::path testPath;
    fs::path fullTestPath;
    fs::path fullFilePath;
    std::string fileContents;
    std::vector<uint8_t> bytes;
};

TEST_F(FileSystemTestFixture, readWriteAppendFile) {
    std::string fileContents2("world");
    std::vector<uint8_t> bytes2(fileContents2.begin(), fileContents2.end());

    EXPECT_TRUE(fs.writeFile(fileName, pluginId, bytes));
    std::vector<std::uint8_t> readBytes = fs.readFile(fileName, pluginId);
    std::string readString(readBytes.begin(), readBytes.end());
    EXPECT_STREQ(readString.c_str(), fileContents.c_str());

    EXPECT_TRUE(fs.appendFile(fileName, pluginId, bytes2));
    readBytes = fs.readFile(fileName, pluginId);
    std::string readString2(readBytes.begin(), readBytes.end());
    EXPECT_STREQ(readString2.c_str(), (fileContents + fileContents2).c_str());

    fs::remove(fullFilePath);
    readBytes = fs.readFile(fileName, pluginId);
    EXPECT_TRUE(readBytes.empty());
}

TEST_F(FileSystemTestFixture, binaryFile) {
    std::vector<uint8_t> binaryFileData;
    binaryFileData.reserve(0x100);
    for (uint16_t byte = 0x00; byte < 0x100; byte++) {
        binaryFileData.push_back(static_cast<uint8_t>(byte));
    }

    EXPECT_TRUE(fs.writeFile(fileName, pluginId, binaryFileData));
    std::vector<std::uint8_t> readBytes = fs.readFile(fileName, pluginId);
    EXPECT_EQ(readBytes.size(), binaryFileData.size());
    EXPECT_TRUE(std::equal(binaryFileData.begin(), binaryFileData.end(), readBytes.begin()));

    fs::remove(fullFilePath);
}

TEST_F(FileSystemTestFixture, dirs) {
    // makeDir/listDir/copyDir/removeDir
    fs::path fullTestPathCopy(fullTestPath.string() + "Copy");
    fs::path fullParentPath(fullTestPath.parent_path());

    EXPECT_TRUE(fs.makeDir(testPath, pluginId));
    EXPECT_TRUE(fs::exists(fullTestPath));
    EXPECT_TRUE(fs.writeFile(testPath / fileName, pluginId, bytes));
    std::vector<std::string> paths = fs.listDir(testPath, pluginId);
    EXPECT_EQ(paths.size(), 1);
    EXPECT_STREQ(paths[0].c_str(), (fullTestPath / fileName).c_str());

    EXPECT_TRUE(fs.copy(fullTestPath, fullTestPathCopy));
    EXPECT_TRUE(fs::exists(fullTestPathCopy));
    paths = fs.listDir(fullParentPath, pluginId);
    EXPECT_EQ(paths.size(), 2);

    EXPECT_TRUE(fs.removeDir(fullParentPath, pluginId));
    paths = fs.listDir(fullParentPath, pluginId);
    EXPECT_EQ(paths.size(), 0);
}

TEST_F(FileSystemTestFixture, makePluginFilePath) {
    fs::path expectedPath = fs::path(INPUT_FILE_DIR) / "usr" / pluginId / testPath;
    EXPECT_STREQ(fullTestPath.c_str(), expectedPath.c_str());
}

TEST_F(FileSystemTestFixture, makePluginInstallPath) {
    FileSystemTestable fst(INPUT_FILE_DIR);
    fs::path expectedInstallPath =
        fs::path(INPUT_FILE_DIR) / "plugins" / "myOS" / "myArch" / pluginId / fileName;
    fs::path installPath = fst.makePluginInstallPath(fileName, pluginId);
    EXPECT_STREQ(installPath.c_str(), expectedInstallPath.c_str());
}
