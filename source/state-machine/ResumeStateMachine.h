
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

#include "ApiContext.h"

namespace Raceboat {

class ApiResumeContext : public ApiContext {
public:
  ApiResumeContext(ApiManagerInternal &manager, StateEngine &engine)
      : ApiContext(manager, engine) {}

  virtual void
  updateResume(const ResumeOptions &resumeOptions,
             std::function<void(ApiStatus, RaceHandle)> cb) override;

  virtual void
  updateConnStateMachineConnected(RaceHandle contextHandle, ConnectionID connId,
                                  std::string linkAddress) override;

public:
  ResumeOptions opts;
  std::function<void(ApiStatus, RaceHandle)> resumeCallback;

  RaceHandle sendConnSMHandle;
  ConnectionID sendConnId;

  RaceHandle recvConnSMHandle;
  ConnectionID recvConnId;
  std::string recvLinkAddress;

  std::string packageId;
};

class ResumeStateEngine : public StateEngine {
public:
  ResumeStateEngine();
  virtual std::string eventToString(EventType event);
};

using ResumeState = BaseApiState<ApiResumeContext>;

} // namespace Raceboat
