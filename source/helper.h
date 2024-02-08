
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
#include <string>
#include <vector>

#include "race/common/PluginResponse.h"
#include "race/common/RaceLog.h"

namespace Raceboat {
namespace helper {

/**
 * @brief Convenience function for calling RaceLog::logDebug. Provides a common,
 * default plugin name so that logging is consistent throughout the code base.
 *
 * @param message The message to log.
 * @param stackTrace An optional stack trace to log. Defaults to empty string.
 */
void logDebug(const std::string &message, const std::string &stackTrace = "");

/**
 * @brief Convenience function for calling RaceLog::logInfo. Provides a common,
 * default plugin name so that logging is consistent throughout the code base.
 *
 * @param message The message to log.
 * @param stackTrace An optional stack trace to log. Defaults to empty string.
 */
void logInfo(const std::string &message, const std::string &stackTrace = "");

/**
 * @brief Convenience function for calling RaceLog::logWarning. Provides a
 * common, default plugin name so that logging is consistent throughout the code
 * base.
 *
 * @param message The message to log.
 * @param stackTrace An optional stack trace to log. Defaults to empty string.
 */
void logWarning(const std::string &message, const std::string &stackTrace = "");

/**
 * @brief Convenience function for calling RaceLog::logError. Provides a common,
 * default plugin name so that logging is consistent throughout the code base.
 *
 * @param message The message to log.
 * @param stackTrace An optional stack trace to log. Defaults to empty string.
 */
void logError(const std::string &message, const std::string &stackTrace = "");

/**
 * @brief Return the current time in seconds since epoch. The seconds may be
 * fractional e.g. xxxxxxxxx.xxx.
 *
 * @return double The current time
 */
double currentTime();

/**
 * @brief Set a name for the thread that can be retrieved via get_thread_name.
 *
 * @param name The name of the thread
 */
void set_thread_name(const std::string &name);

/**
 * @brief Get a name for the current thread that was previously set with
 * set_thread_name. If the name for the thread was not previously set, returns
 * an empty string
 *
 * @return std::string the name of the thread
 */
std::string get_thread_name();
} // namespace helper

/**
 * @brief return stringified vector in [a, b, c] format
 **/
std::string stringVectorToString(const std::vector<std::string> &vec);

#define TRACE_FUNCTION(...) TRACE_FUNCTION_BASE(Raceboat, ##__VA_ARGS__)
#define TRACE_METHOD(...) TRACE_METHOD_BASE(Raceboat, ##__VA_ARGS__)

} // namespace Raceboat
