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

#include "ComponentReceivePackageManager.h"

#include "ComponentManager.h"

namespace Raceboat {

using namespace CMTypes;

// reads a value of type T (must be a POD type) from a buffer and updates offset
// accordingly
template <typename T>
T readFromBuffer(const std::vector<uint8_t> &buffer, size_t &offset) {
  if (offset + sizeof(T) > buffer.size()) {
    throw std::out_of_range(
        "Tried to read beyond buffer: offset: " + std::to_string(offset) +
        ", sizeof(T): " + std::to_string(sizeof(T)) +
        ", buffer.size(): " + std::to_string(buffer.size()));
  }

  T value;
  memcpy(&value, buffer.data() + offset, sizeof(value));
  offset += sizeof(value);
  return value;
}

ComponentReceivePackageManager::ComponentReceivePackageManager(
    ComponentManagerInternal &manager)
    : manager(manager) {}

CmInternalStatus ComponentReceivePackageManager::onReceive(
    ComponentWrapperHandle postId, const LinkID &linkId,
    const EncodingParameters &params, std::vector<uint8_t> &&bytes) {
  TRACE_METHOD(postId, linkId, bytes.size());

  // Each encoding is received as a separate message with its own encoding parameters
  auto matchingEncodings = manager.encodingComponentFromEncodingParams(params);
  if (matchingEncodings.empty()) {
    helper::logError(
        logPrefix +
        "Failed to find encoding for params. Encoding type: " + params.type);
    return ERROR;
  } else if (matchingEncodings.size() > 1) {
    helper::logWarning(logPrefix + "Multiple encodings found for encoding type " + params.type + ", using first one - this is dangerous because the sender may have a different first encoding");
  }
  auto encoding = matchingEncodings.front();
  DecodingHandle decodingHandle{++nextDecodingHandle};
  pendingDecodings[decodingHandle] = linkId;
  encoding->decodeBytes(decodingHandle, params, bytes);

  return OK;
}

CmInternalStatus ComponentReceivePackageManager::onBytesDecoded(
    ComponentWrapperHandle postId, DecodingHandle handle,
    std::vector<uint8_t> &&bytes, EncodingStatus status) {
  TRACE_METHOD(postId, handle, bytes.size(), status);

  if (bytes.empty()) {
    // Expected result of decoding cover traffic
    return OK;
  }

  // Each encoding is received as a separate message, so no need to handle multiple
  // encodings per received message. Fragment reassembly happens per encoding stream.
  try {
    auto linkId = pendingDecodings.at(handle);
    pendingDecodings.erase(handle);
    auto link = manager.getLink(linkId);
    auto connSet = link->connections;
    std::vector<std::string> connVec = {connSet.begin(), connSet.end()};

    if (manager.mode == EncodingMode::SINGLE) {
      return receiveSingle(std::move(bytes), std::move(connVec));
    } else if (manager.mode == EncodingMode::BATCH) {
      return receiveBatch(std::move(bytes), std::move(connVec));
    } else if (manager.mode == EncodingMode::FRAGMENT_SINGLE_PRODUCER) {
      return receiveFragmentSingleProducer(link, std::move(bytes),
                                           std::move(connVec));
    } else if (manager.mode == EncodingMode::FRAGMENT_MULTIPLE_PRODUCER) {
      return receiveFragmentMultipleProducer(link, std::move(bytes),
                                             std::move(connVec));
    } else {
      throw std::runtime_error("Not implemented");
    }
  } catch (std::exception &e) {
    helper::logError(logPrefix + "Exception: " + e.what());
    return ERROR;
  }
}

std::vector<uint8_t>
ComponentReceivePackageManager::readFragment(const std::vector<uint8_t> &buffer,
                                             size_t &offset) {
  uint32_t len = readFromBuffer<uint32_t>(buffer, offset);

  if (offset + len > buffer.size()) {
    throw std::out_of_range(
        "Tried to read beyond buffer: offset: " + std::to_string(offset) +
        ", len: " + std::to_string(len) +
        ", buffer.size(): " + std::to_string(buffer.size()));
  }

  offset += len;
  return std::vector<uint8_t>(buffer.data() + offset - len,
                              buffer.data() + offset);
}

CmInternalStatus ComponentReceivePackageManager::receiveSingle(
    std::vector<uint8_t> &&bytes, std::vector<std::string> &&connVec) {
  TRACE_METHOD(bytes.size(), connVec.size());

  EncPkg pkg{std::move(bytes)};
  manager.sdk.receiveEncPkg(pkg, connVec, RACE_BLOCKING);
  return OK;
}

CmInternalStatus ComponentReceivePackageManager::receiveBatch(
    std::vector<uint8_t> &&bytes, std::vector<std::string> &&connVec) {
  TRACE_METHOD(bytes.size(), connVec.size());

  size_t offset = 0;
  while (offset < bytes.size()) {
    auto pkgBytes = readFragment(bytes, offset);
    EncPkg pkg(pkgBytes);
    manager.sdk.receiveEncPkg(pkg, connVec, RACE_BLOCKING);
  }

  return OK;
}

CmInternalStatus ComponentReceivePackageManager::receiveFragmentSingleProducer(
    Link *link, std::vector<uint8_t> &&bytes,
    std::vector<std::string> &&connVec) {
  TRACE_METHOD(bytes.size(), connVec.size());

  return receiveFragmentProducer("", 0, link, std::move(bytes),
                                 std::move(connVec));
}

CmInternalStatus
ComponentReceivePackageManager::receiveFragmentMultipleProducer(
    Link *link, std::vector<uint8_t> &&bytes,
    std::vector<std::string> &&connVec) {
  TRACE_METHOD(bytes.size(), connVec.size());

  size_t offset = 0;
  auto producer = readFromBuffer<std::array<uint8_t, 16>>(bytes, offset);

  std::string producerString(producer.begin(), producer.end());
  return receiveFragmentProducer(producerString, offset, link, std::move(bytes),
                                 std::move(connVec));
}

CmInternalStatus ComponentReceivePackageManager::receiveFragmentProducer(
    const std::string &producer, size_t offset, Link *link,
    std::vector<uint8_t> &&bytes, std::vector<std::string> &&connVec) {
  TRACE_METHOD(offset, link->linkId, bytes.size(), connVec.size());

  uint32_t fragmentId = readFromBuffer<uint32_t>(bytes, offset);
  uint8_t flags = readFromBuffer<uint8_t>(bytes, offset);

  auto fragmentQueue = &link->producerQueues[producer];
  fragmentQueue->lastActivity = std::chrono::steady_clock::now();

  // Read all fragment data
  while (offset < bytes.size()) {
    std::vector<uint8_t> fragmentData;
    auto pkgBytes = readFragment(bytes, offset);
    fragmentData.insert(fragmentData.end(), pkgBytes.begin(), pkgBytes.end());

    // Store the fragment
    StoredFragment storedFrag;
    storedFrag.data = std::move(fragmentData);
    storedFrag.flags = flags;
    storedFrag.timestamp = std::chrono::steady_clock::now();
  
    // fragmentQueue->storedFragments[fragmentId] = std::move(storedFrag);
    fragmentQueue->storedFragments[fragmentId].push_back(std::move(storedFrag));
  
    helper::logDebug(logPrefix + "Stored fragment " + std::to_string(fragmentId) + 
                     " for producer " + producer);
    helper::logDebug(logPrefix + " pieces in fragment: " +
                     std::to_string(fragmentQueue->storedFragments[fragmentId].size()));
  }
  // Process any complete sequences starting from lastFragmentReceived + 1
  processCompleteSequences(fragmentQueue, connVec);
  
  return OK;
}

void ComponentReceivePackageManager::processCompleteSequences(
    ProducerQueue* fragmentQueue, const std::vector<std::string>& connVec) {

  TRACE_METHOD(fragmentQueue->lastFragmentReceived, connVec.size());
  while (true) {
    uint32_t nextExpected = fragmentQueue->lastFragmentReceived + 1;
    helper::logDebug(logPrefix + " looking for fragment " + std::to_string(nextExpected));
    
    // Check if we have the next expected fragment
    auto fragIt = fragmentQueue->storedFragments.find(nextExpected);
    if (fragIt == fragmentQueue->storedFragments.end()) {
      helper::logDebug(logPrefix + "No more consecutive fragments");
      break; // No more consecutive fragments available
    }

    for (auto fragment = fragIt->second.begin();
         fragment != fragIt->second.end(); ++fragment) {
      helper::logDebug(logPrefix + "processing fragment of " + std::to_string(fragIt->first));
      // Process this fragment
      // const auto& fragment = fragIt->second;
    
      // Clear pending bytes if this isn't continuing from previous package
      if (!(fragment->flags & CONTINUE_LAST_PACKAGE) && !fragmentQueue->pendingBytes.empty()) {
        helper::logDebug(logPrefix + "Clearing pending bytes from previous fragment");
        fragmentQueue->pendingBytes.clear();
      }

      // Add fragment data to pending bytes
      fragmentQueue->pendingBytes.insert(fragmentQueue->pendingBytes.end(),
                                         fragment->data.begin(), fragment->data.end());

      bool isPackageEnd = !(fragment->flags & CONTINUE_NEXT_PACKAGE);
    

      if (isPackageEnd) {
        // Package is complete, send it
        if (!fragmentQueue->pendingBytes.empty()) {
          EncPkg pkg(std::move(fragmentQueue->pendingBytes));
          fragmentQueue->pendingBytes.clear();
          helper::logDebug(logPrefix + "calling receiveEncPkg ");
          manager.sdk.receiveEncPkg(pkg, connVec, RACE_BLOCKING);
        
          helper::logDebug(logPrefix + "Sent complete package ending at fragment " + 
                           std::to_string(nextExpected));
        }
      } else {
        helper::logDebug(logPrefix + "Package continues in next fragment");
      }
    }
    // Update state and remove processed fragment
    helper::logDebug(logPrefix + "Package continues in next fragment");
    fragmentQueue->lastFragmentReceived = nextExpected;
    fragmentQueue->storedFragments.erase(fragIt);
  }
}

void ComponentReceivePackageManager::cleanupExpiredFragments(ProducerQueue* fragmentQueue) {
  TRACE_METHOD(fragmentQueue->storedFragments.size());
  
  uint32_t nextExpected = fragmentQueue->lastFragmentReceived + 1;
  
  helper::logWarning(logPrefix + "Timeout reached: skipping missing fragments starting from " + 
                     std::to_string(nextExpected));
  
  // Skip all missing fragments until we find one we have
  skipMissingFragmentsUntilAvailable(fragmentQueue);
}


void ComponentReceivePackageManager::skipMissingFragmentsUntilAvailable(ProducerQueue* fragmentQueue) {
  TRACE_METHOD();
  uint32_t nextExpected = fragmentQueue->lastFragmentReceived + 1;
  
  helper::logDebug(logPrefix + "Starting to skip from fragment " + std::to_string(nextExpected));
  
  // Clear pending bytes since we're breaking the sequence
  if (!fragmentQueue->pendingBytes.empty()) {
    helper::logWarning(logPrefix + "Clearing " + std::to_string(fragmentQueue->pendingBytes.size()) + 
                       " pending bytes due to skipped fragments");
    fragmentQueue->pendingBytes.clear();
  }
  
  // Find the first available fragment
  uint32_t firstAvailable = 0;
  for (const auto& [fragmentId, fragmentList] : fragmentQueue->storedFragments) {
    if (fragmentId >= nextExpected && !fragmentList.empty()) {
      if (firstAvailable == 0 || fragmentId < firstAvailable) {
        firstAvailable = fragmentId;
      }
    }
  }
  
  if (firstAvailable == 0) {
    helper::logWarning(logPrefix + "No available fragments found to skip to");
    return;
  }
  
  helper::logWarning(logPrefix + "Skipping fragments " + std::to_string(nextExpected) + 
                     " through " + std::to_string(firstAvailable - 1) + 
                     ", jumping to " + std::to_string(firstAvailable));
  
  // Remove all fragments between nextExpected and firstAvailable
  for (uint32_t fragmentId = nextExpected; fragmentId < firstAvailable; ++fragmentId) {
    auto it = fragmentQueue->storedFragments.find(fragmentId);
    if (it != fragmentQueue->storedFragments.end()) {
      helper::logDebug(logPrefix + "Removing skipped fragment " + std::to_string(fragmentId));
      fragmentQueue->storedFragments.erase(it);
    }
  }
  
  // Check if the first available fragment has CONTINUE_LAST_PACKAGE
  auto availableIt = fragmentQueue->storedFragments.find(firstAvailable);
  if (availableIt != fragmentQueue->storedFragments.end() && !availableIt->second.empty()) {
    bool hasContinueLastPackage = false;
    for (const auto& fragment : availableIt->second) {
      if (fragment.flags & CONTINUE_LAST_PACKAGE) {
        hasContinueLastPackage = true;
        break;
      }
    }
    
    if (hasContinueLastPackage) {
      helper::logWarning(logPrefix + "First available fragment " + std::to_string(firstAvailable) + 
                         " has CONTINUE_LAST_PACKAGE flag, skipping it too");
      fragmentQueue->storedFragments.erase(availableIt);
      
      // Recursively find the next available fragment that doesn't continue from previous
      fragmentQueue->lastFragmentReceived = firstAvailable;
      skipMissingFragmentsUntilAvailable(fragmentQueue);
      return;
    }
  }
  
  // Jump to just before the first available fragment
  fragmentQueue->lastFragmentReceived = firstAvailable - 1;
  
  helper::logDebug(logPrefix + "Set lastFragmentReceived to " + 
                   std::to_string(fragmentQueue->lastFragmentReceived) + 
                   ", next expected is now " + std::to_string(firstAvailable));
}

// Make the timeout configurable
void ComponentReceivePackageManager::setFragmentTimeout(std::chrono::seconds timeout) {
  TRACE_METHOD();
  fragmentTimeout = timeout;
  helper::logDebug(logPrefix + "Fragment timeout set to " + std::to_string(timeout.count()) + " seconds");
}

void ComponentReceivePackageManager::setCleanupCheckInterval(std::chrono::seconds interval) {
  TRACE_METHOD();
  cleanupCheckInterval = interval;
  helper::logDebug(logPrefix + "Cleanup check interval set to " + std::to_string(interval.count()) + " seconds");
}

std::chrono::steady_clock::time_point 
ComponentReceivePackageManager::findOldestFragmentTime(ProducerQueue* fragmentQueue) {
  TRACE_METHOD();
  auto oldestTime = std::chrono::steady_clock::time_point::max();
  bool foundAny = false;
  
  for (const auto& [fragmentId, fragmentList] : fragmentQueue->storedFragments) {
    if (!fragmentList.empty()) {
      // Use timestamp of first piece in the fragment
      auto fragmentTime = fragmentList[0].timestamp;
      if (fragmentTime < oldestTime) {
        oldestTime = fragmentTime;
        foundAny = true;
      }
    }
  }
  
  return foundAny ? oldestTime : std::chrono::steady_clock::time_point{};
}


void ComponentReceivePackageManager::teardown() {
  TRACE_METHOD();

  shutdownRequested = true;
  if (cleanupThread.joinable()) {
    cleanupThread.join();
  }

  pendingDecodings.clear();
}

void ComponentReceivePackageManager::cleanupWorker() {
  TRACE_METHOD();
  while (!shutdownRequested) {
    std::this_thread::sleep_for(cleanupCheckInterval);
    
    if (shutdownRequested) break;
    
    try {
      runCleanupOnAllQueues();
    } catch (const std::exception& e) {
      helper::logError(logPrefix + "Exception in cleanup worker: " + e.what());
    }
  }
}

void ComponentReceivePackageManager::runCleanupOnAllQueues() {
  TRACE_METHOD();
  helper::logDebug(logPrefix + "Running periodic cleanup check");
  
  bool foundWorkToDo = false;
  
  // Iterate through all links and their producer queues
  for (auto& link : manager.getLinks()) {
    for (auto& [producer, queue] : link->producerQueues) {
      if (shouldRunCleanup(&queue)) {
        foundWorkToDo = true;
        helper::logDebug(logPrefix + "Running cleanup for link " + link->linkId);
        
        cleanupExpiredFragments(&queue);
        
        // After cleanup, try to process any newly available sequences
        std::vector<std::string> connVec(link->connections.begin(), link->connections.end());
        processCompleteSequences(&queue, connVec);
      }
    }
  }
  
  if (foundWorkToDo) {
    helper::logDebug(logPrefix + "Cleanup cycle completed with work done");
  }
}

bool ComponentReceivePackageManager::shouldRunCleanup(ProducerQueue* fragmentQueue) {
  auto now = std::chrono::steady_clock::now();
  uint32_t nextExpected = fragmentQueue->lastFragmentReceived + 1;
  
  // Check condition 1: Do we have fragments with greater IDs?
  bool hasLaterFragments = false;
  for (const auto& [fragmentId, fragmentList] : fragmentQueue->storedFragments) {
    if (fragmentId > nextExpected) {
      hasLaterFragments = true;
      break;
    }
  }
  
  if (!hasLaterFragments) {
    return false; // No point in cleanup if we don't have later fragments
  }
  
  // Check condition 2: Has timeout passed since oldest fragment was received?
  auto oldestFragmentTime = findOldestFragmentTime(fragmentQueue);
  if (oldestFragmentTime == std::chrono::steady_clock::time_point{}) {
    return false; // No fragments stored
  }
  
  return (now - oldestFragmentTime > fragmentTimeout);
}

void ComponentReceivePackageManager::setup() { 
  TRACE_METHOD(); 
  
  shutdownRequested = false;
  cleanupThread = std::thread(&ComponentReceivePackageManager::cleanupWorker, this);
}

static std::ostream &
operator<<(std::ostream &out,
           const std::unordered_map<CMTypes::DecodingHandle, LinkID>
               &pendingDecodings) {
  std::map orderedDecodings{pendingDecodings.begin(), pendingDecodings.end()};
  out << "{";
  for (auto &iter : orderedDecodings) {
    out << iter.first << ":" << iter.second << ", ";
  }
  out << "}";
  return out;
}

std::ostream &operator<<(std::ostream &out,
                         const ComponentReceivePackageManager &manager) {
  return out << "ReceivePackageManager{"
             << "nextDecodingHandle:" << manager.nextDecodingHandle
             << ", pendingDecodings: " << manager.pendingDecodings << "}";
}
} // namespace Raceboat
