
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

#ifndef RACE_LINK_TYPE_H
#define RACE_LINK_TYPE_H

#include <string>

enum LinkType {
  LT_UNDEF = 0, // undefined
  LT_SEND = 1,  // send
  LT_RECV = 2,  // receive
  LT_BIDI = 3   // bi-directional
};

/**
 * @brief Convert a LinkType value to a human readable string. This function is
 * strictly for logging and debugging. The output formatting may change without
 * notice. Do NOT use this for any logical comparisons, etc. The functionality
 * of your plugin should in no way rely on the output of this function.
 *
 * @param linkType The value to convert to a string.
 * @return std::string The string representation of the provided value.
 */
std::string linkTypeToString(LinkType linkType);

std::ostream &operator<<(std::ostream &out, LinkType linkType);

#endif
