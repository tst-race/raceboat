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

#pragma once

#include <nlohmann/json.hpp>

namespace Raceboat::detail {

inline bool containsRequestedAddressFields(const nlohmann::json &requested,
                                           const nlohmann::json &received) {
  if (requested.is_object()) {
    if (!received.is_object()) {
      return false;
    }

    for (const auto &[key, value] : requested.items()) {
      if (!received.contains(key) ||
          !containsRequestedAddressFields(value, received.at(key))) {
        return false;
      }
    }
    return true;
  }

  if (requested.is_array()) {
    if (!received.is_array() || requested.size() != received.size()) {
      return false;
    }

    for (size_t index = 0; index < requested.size(); ++index) {
      if (!containsRequestedAddressFields(requested.at(index),
                                          received.at(index))) {
        return false;
      }
    }
    return true;
  }

  return requested == received;
}

} // namespace Raceboat::detail
