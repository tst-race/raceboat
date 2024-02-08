
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

#include "api-managers/ApiManager.h"
#include "gmock/gmock.h"

namespace Raceboat {

class MockApiManager : public ApiManager {
public:
    explicit MockApiManager(Core &core) : ApiManager(core) {}
    
    MOCK_METHOD(SdkResponse, send, (SendOptions sendOptions, std::vector<uint8_t>data, std::function<void(ApiStatus)> callback), (override));
    MOCK_METHOD(SdkResponse, getReceiveObject,
                (ReceiveOptions opts, std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback),
                (override));
    MOCK_METHOD(SdkResponse, receive,
                (OpHandle handle, std::function<void(ApiStatus, std::vector<uint8_t>)> callback), (override));
};

class MockApiManagerInternal : public ApiManagerInternal {
public:
    explicit MockApiManagerInternal(Core &core, ApiManager& manager): ApiManagerInternal(core, manager) {}
    
    MOCK_METHOD(void, send, (uint64_t postId, SendOptions sendOptions, std::vector<uint8_t>data, std::function<void(ApiStatus)> callback), (override));
    MOCK_METHOD(void, getReceiveObject,
                (uint64_t postId, ReceiveOptions opts, std::function<void(ApiStatus, LinkAddress, RaceHandle)> callback),
                (override));
    MOCK_METHOD(void, receive,
                (uint64_t postId, OpHandle handle, std::function<void(ApiStatus, std::vector<uint8_t>)> callback), (override));
};

}  // namespace Raceboat