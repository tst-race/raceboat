
//
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

#include "SdkResponse.h"
#include "ChannelProperties.h"
#include "LinkProperties.h"
#include "PluginConfig.h"
#include "EncPkg.h"
#include "nlohmann/json.hpp"

using nlohmann::json;

// valgrind complains about the built-in gtest generic object printer, so we need our own
[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 const SdkResponse & /*response*/) {
    os << "<SdkResponse>"
       << "\n";
    return os;
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 const ChannelProperties & /*props*/) {
    os << "<ChannelProperties>"
       << "\n";

    return os;
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 const LinkProperties &properties) {
    os << "linkType: " << static_cast<unsigned int>(properties.linkType) << ", ";
    os << "transmissionType: " << static_cast<unsigned int>(properties.transmissionType) << ", ";
    os << "reliable: " << static_cast<unsigned int>(properties.reliable) << std::endl;

    return os;
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 const PluginConfig &pluginConfig) {
    os << "{" << pluginConfig.etcDirectory << ", " << pluginConfig.loggingDirectory << ", "
       << pluginConfig.auxDataDirectory << ", " << pluginConfig.tmpDirectory << ", "
       << pluginConfig.pluginDirectory << "}" << std::endl;

    return os;
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os, const EncPkg & /*pkg*/) {
    os << "<EncPkg>" << std::endl;
    return os;
}
