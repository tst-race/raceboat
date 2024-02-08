
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

#include <fstream>

#include "common/RaceLog.h"
#include "gtest/gtest.h"

TEST(RaceLog, test_redirect) {
    std::string logEntry = "test log entry";
    std::string redirectPath = "/etc/race/logging/core/core.log";
    RaceLog::setLogRedirectPath(redirectPath);
    RaceLog::logError("", logEntry, "");

    const std::streamsize siz = 1024;
    char buffer[siz];
    std::ifstream stream(redirectPath);
    stream.read(buffer, siz);
    std::string output(buffer);
    EXPECT_NE(output.find(logEntry), std::string::npos);

    std::filesystem::remove(std::filesystem::path(redirectPath));
}
