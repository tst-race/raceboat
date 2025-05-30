
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
#include "../../source/UserInput.h"
#include "gmock/gmock.h"

using namespace Raceboat;

class MockUserInput : public UserInput {
public:
    MockUserInput() : UserInput({}) {}
    MOCK_METHOD(std::optional<std::string>, getPluginUserInput,
                (std::string pluginId, std::string key), (const override));
    MOCK_METHOD(std::optional<std::string>, getCommonUserInput, (std::string key), (const override));
};