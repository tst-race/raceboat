
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

#include "race/common/PackageType.h"

std::string packageTypeToString(PackageType packageType) {
  switch (packageType) {
  case PKG_TYPE_UNDEF:
    return "PKG_TYPE_UNDEF";
  case PKG_TYPE_NETWORK_MANAGER:
    return "PKG_TYPE_NETWORK_MANAGER";
  case PKG_TYPE_TEST_HARNESS:
    return "PKG_TYPE_TEST_HARNESS";
  case PKG_TYPE_SDK:
    return "PKG_TYPE_SDK";
  default:
    return "ERROR: INVALID PACKAGE TYPE: " + std::to_string(packageType);
  }
}

std::ostream &operator<<(std::ostream &out, PackageType packageType) {
  return out << packageTypeToString(packageType);
}
