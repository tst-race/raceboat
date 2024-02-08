
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

#include "helper.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>  // memcpy
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

#include "nlohmann/json.hpp"
#include "race/common/LinkStatus.h"
#include "race/common/RaceLog.h"

namespace RaceLib {

using json = nlohmann::json;

static const std::string pluginNameForLogging = "Raceboat";

void helper::logDebug(const std::string &message, const std::string &stackTrace) {
    RaceLog::logDebug(pluginNameForLogging, message, stackTrace);
}

void helper::logInfo(const std::string &message, const std::string &stackTrace) {
    RaceLog::logInfo(pluginNameForLogging, message, stackTrace);
}

void helper::logWarning(const std::string &message, const std::string &stackTrace) {
    RaceLog::logWarning(pluginNameForLogging, message, stackTrace);
}

void helper::logError(const std::string &message, const std::string &stackTrace) {
    RaceLog::logError(pluginNameForLogging, message, stackTrace);
}

double helper::currentTime() {
    std::chrono::duration<double> sinceEpoch =
        std::chrono::high_resolution_clock::now().time_since_epoch();
    return sinceEpoch.count();
}

thread_local std::string thread_name;
void helper::set_thread_name(const std::string &name) {
    thread_name = name;
}

std::string helper::get_thread_name() {
    return thread_name;
}

std::string stringVectorToString(const std::vector<std::string> &vec) {
    std::string s = "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) {
            s += ", ";
        }
        s += vec[i];
    }

    s += "]";
    return s;
}

}  // namespace RaceLib
