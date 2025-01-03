
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

#ifndef I_COMPONENT_BASE_H_
#define I_COMPONENT_BASE_H_

#include "ComponentTypes.h"

class IComponentBase {
public:
  virtual ~IComponentBase() = default;

  virtual ComponentStatus onUserInputReceived(RaceHandle handle, bool answered,
                                              const std::string &response) = 0;
};

#endif
