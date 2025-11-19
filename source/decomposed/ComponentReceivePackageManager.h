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

#include <chrono>
#include <memory>
#include <map>
#include <unordered_map>

#include "ComponentManagerTypes.h"
#include "ComponentWrappers.h"
#include "Composition.h"
#include "SdkWrappers.h"
#include "plugin-loading/ComponentPlugin.h"

namespace Raceboat {

using CMTypes::ProducerQueue;

class ComponentManagerInternal;

struct FragmentInfo {
  std::vector<uint8_t> data;
  uint8_t flags;
  uint32_t fragmentId;
  std::chrono::steady_clock::time_point timestamp;
};

struct PackageAssembly {
  std::map<uint32_t, FragmentInfo> fragments; // fragmentId -> fragment data
  uint32_t expectedNextId = 1;
  std::chrono::steady_clock::time_point lastActivity;
  bool hasLastFragment = false;
  uint32_t lastFragmentId = 0;
};


class ComponentReceivePackageManager {
public:
  explicit ComponentReceivePackageManager(ComponentManagerInternal &manager);
  virtual ~ComponentReceivePackageManager(){};

  virtual CMTypes::CmInternalStatus
  onReceive(CMTypes::ComponentWrapperHandle postId, const LinkID &linkId,
            const EncodingParameters &params, std::vector<uint8_t> &&bytes);

  virtual CMTypes::CmInternalStatus
  onBytesDecoded(CMTypes::ComponentWrapperHandle postId,
                 CMTypes::DecodingHandle handle, std::vector<uint8_t> &&bytes,
                 EncodingStatus status);

  void teardown();
  void setup();

  // Configuration methods
  void setFragmentTimeout(std::chrono::seconds timeout);
  void setCleanupCheckInterval(std::chrono::seconds interval);

protected:
  CMTypes::CmInternalStatus receiveSingle(std::vector<uint8_t> &&bytes,
                                          std::vector<std::string> &&connVec);
  CMTypes::CmInternalStatus receiveBatch(std::vector<uint8_t> &&bytes,
                                         std::vector<std::string> &&connVec);
  CMTypes::CmInternalStatus
  receiveFragmentSingleProducer(CMTypes::Link *link,
                                std::vector<uint8_t> &&bytes,
                                std::vector<std::string> &&connVec);
  CMTypes::CmInternalStatus
  receiveFragmentMultipleProducer(CMTypes::Link *link,
                                  std::vector<uint8_t> &&bytes,
                                  std::vector<std::string> &&connVec);
  CMTypes::CmInternalStatus
  receiveFragmentProducer(const std::string &producer, size_t offset,
                          CMTypes::Link *link, std::vector<uint8_t> &&bytes,
                          std::vector<std::string> &&connVec);

  std::vector<uint8_t> readFragment(const std::vector<uint8_t> &buffer,
                                    size_t &offset);

  friend std::ostream &
  operator<<(std::ostream &out, const ComponentReceivePackageManager &manager);

protected:
  ComponentManagerInternal &manager;
  uint64_t nextDecodingHandle{0};

  std::unordered_map<CMTypes::DecodingHandle, LinkID> pendingDecodings;

private:
  // New members for fragment management
  std::unordered_map<std::string, PackageAssembly> pendingPackages; // producer -> assembly
  std::chrono::seconds fragmentTimeout{10}; // Configurable timeout

  // New helper methods
  CMTypes::CmInternalStatus processOutOfOrderFragment(
      const std::string& producer, uint32_t fragmentId, uint8_t flags,
      std::vector<uint8_t>&& fragmentData, std::vector<std::string>&& connVec);

  bool tryAssemblePackage(const std::string& producer, std::vector<std::string>&& connVec);
  void cleanupExpiredFragments();
  bool isPackageComplete(const PackageAssembly& assembly);

  void processCompleteSequences(ProducerQueue* fragmentQueue, 
                                 const std::vector<std::string>& connVec);
  void cleanupExpiredFragments(ProducerQueue* fragmentQueue);
  std::chrono::steady_clock::time_point findOldestFragmentTime(ProducerQueue* fragmentQueue);
  void skipMissingFragmentsUntilAvailable(ProducerQueue* fragmentQueue);

  // Cleanup thread management
  std::thread cleanupThread;
  std::atomic<bool> shutdownRequested{false};
  std::chrono::seconds cleanupCheckInterval{1}; // Check every 1 second
  void cleanupWorker();
  void runCleanupOnAllQueues();
  bool shouldRunCleanup(ProducerQueue* fragmentQueue);
};
} // namespace Raceboat
