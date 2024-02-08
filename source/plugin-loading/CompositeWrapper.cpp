
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

#include "CompositeWrapper.h"

namespace Raceboat {

CompositeWrapper::CompositeWrapper(
    PluginContainer &container, Core &, Composition composition, const std::string &description,
    IComponentPlugin &transport, IComponentPlugin &usermodel,
    const std::unordered_map<std::string, IComponentPlugin *> &encodings) :
    PluginWrapper(container) {
    TRACE_METHOD();

    auto componentManager = std::make_shared<ComponentManager>(*this->getSdk(), composition,
                                                               transport, usermodel, encodings);

    this->mPlugin = componentManager;
    this->mDescription = description;
}

CompositeWrapper::~CompositeWrapper() {
    TRACE_METHOD();
}
}  // namespace Raceboat
