
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

#include <atomic>
#include <memory>

#include "Handler.h"
#include "SdkWrapper.h"
#include "IRacePluginComms.h"

namespace Raceboat {

static const int32_t WAIT_FOREVER = 0;

/*
 * PluginWrapper: A wrapper for a plugin that calls associated methods on a
 * separate plugin thread
 */
class PluginWrapper {
public:
  PluginWrapper(PluginContainer &container,
                std::shared_ptr<IRacePluginComms> plugin,
                std::string description);
  virtual ~PluginWrapper();

  /**
   * @brief Get the id of the wrapped IRacePluginComms
   *
   * @return const std::string & The id of the wrapped plugin
   */
  const std::string &getId() const { return container.id; }

  /**
   * @brief Get the description string of the wrapped IRacePluginComms
   *
   * @return const std::string & The description of the wrapped plugin
   */
  const std::string &getDescription() const { return mDescription; }

  /**
   * stopHandler: stop the plugin thread
   *
   * This stops the internally managed thread on which methods of the wrapped
   * plugin are run. The thread is started when the wrapper is constructed. Any
   * callbacks posted, but not yet completed will be finished. Attempting to
   * post a new callback will fail.
   */
  virtual void stopHandler();

  /**
   * waitForCallbacks: wait for all callbacks to finish
   *
   * Creates a queue and callback with the minimum priority and waits for that
   * to finish. Used for testing.
   */
  virtual void waitForCallbacks();

  /**
   * init: call init on the wrapped plugin
   */
  virtual bool init(const PluginConfig &pluginConfig);

  /**
   * shutdown: call shutdown on the wrapped plugin, timing out if it takes too
   * long.
   *
   * shutdown will be called on the plugin thread. Shutdown may return before
   * the shutdown method of the wrapped plugin is complete.
   *
   * @return bool indicating whether or not the call was successful
   */
  virtual bool shutdown();

  /**
   * @brief Call shutdown on the wrapped plugin, timing out if the call takes
   * longer than the specified timeout.
   *
   * @param timeoutInSeconds Timeout in seconds.
   *
   * @return tuple<bool, double> tuple containing whether the post was
   * successful, and the proportion of the queue utilized
   */
  virtual bool shutdown(std::int32_t timeoutInSeconds);

  /**
   * The following calls all call the appropriate function on the plugin on the
   * work thread with forwarded arguments. The function calls are done
   * asyncronously and may be delayed if there is a currently executing
   * function.
   *
   * See the IRacePluginComms documentation for details about the behavior of
   * each function.
   */
  virtual SdkResponse sendPackage(RaceHandle handle,
                                  const ConnectionID &connectionId,
                                  const EncPkg &pkg, int32_t timeout,
                                  uint64_t batchId);

  virtual SdkResponse openConnection(RaceHandle handle, LinkType linkType,
                                     const LinkID &linkId,
                                     const std::string &linkHints,
                                     int32_t priority, int32_t sendTimeout,
                                     int32_t timeout);

  virtual SdkResponse closeConnection(RaceHandle handle,
                                      const ConnectionID &connectionId,
                                      int32_t timeout);

  virtual SdkResponse createLink(RaceHandle handle,
                                 const std::string &channelGid,
                                 std::int32_t timeout);
  virtual SdkResponse createBootstrapLink(RaceHandle handle,
                                          const std::string &channelGid,
                                          const std::string &passphrase,
                                          std::int32_t timeout);
  virtual SdkResponse loadLinkAddress(RaceHandle handle,
                                      const std::string &channelGid,
                                      const std::string &linkAddress,
                                      std::int32_t timeout);
  virtual SdkResponse loadLinkAddresses(RaceHandle handle,
                                        const std::string &channelGid,
                                        std::vector<std::string> linkAddresses,
                                        std::int32_t timeout);

  virtual SdkResponse createLinkFromAddress(RaceHandle handle,
                                            const std::string &channelGid,
                                            const std::string &linkAddress,
                                            std::int32_t timeout);
  virtual SdkResponse destroyLink(RaceHandle handle, const LinkID &linkId,
                                  std::int32_t timeout);
  virtual SdkResponse deactivateChannel(RaceHandle handle,
                                        const std::string &channelGid,
                                        std::int32_t timeout);
  virtual SdkResponse activateChannel(RaceHandle handle,
                                      const std::string &channelGid,
                                      const std::string &roleName,
                                      std::int32_t timeout);

  virtual SdkResponse serveFiles(LinkID linkId, std::string path,
                                 int32_t timeout);

  virtual SdkResponse flushChannel(RaceHandle handle, std::string channelGid,
                                   uint64_t batchId, int32_t timeout);

  virtual SdkResponse onUserInputReceived(RaceHandle handle, bool answered,
                                          const std::string &response,
                                          int32_t timeout);

  virtual SdkResponse onUserAcknowledgementReceived(RaceHandle handle,
                                                    int32_t timeout);

  /**
   * @brief Notify the thread of a conntection status change
   *
   * This call is used to open or close work queues on the handler. It does not
   * get forwarded to the plugin.
   *
   * @param connId the connection to open/close a queue for
   * @param status whether the connection was opened or closed
   */
  virtual void onConnectionStatusChanged(const ConnectionID &connId,
                                         ConnectionStatus status);

  /**
   * @brief Unblock a queue on the thread handler
   *
   * This call is used to unblock queues on the handler. It does not get
   * forwarded to the plugin.
   *
   * @param connId the connection to unblock
   */
  virtual SdkResponse unblockQueue(const ConnectionID &connId);

protected:
  PluginWrapper(PluginContainer &container);

  IRaceSdkComms *getSdk() { return container.sdk.get(); }

  template <typename T, typename... Args>
  SdkResponse post(const std::string &logPrefix, const std::string &queue,
                   T &&function, Args &&... args);

  template <typename T, typename... Args>
  bool post_lifecycle(const std::string &logPrefix, int32_t timeout,
                      T &&function, Args &&... args);
  SdkResponse makeResponse(const std::string &functionName, bool success,
                           size_t queueUtil, RaceHandle handle);

protected:
  // unique_ptr requires deleter type as template parameter
  std::shared_ptr<IRacePluginComms> mPlugin;
  std::string mDescription;

  PluginContainer &container;
  Handler mThreadHandler;

  // nextPostId is used to identify which post matches with which call/return
  // log.
  std::atomic<uint64_t> nextPostId = 0;
};

} // namespace Raceboat
