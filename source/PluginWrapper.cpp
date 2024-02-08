
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

#include "PluginWrapper.h"

#include <future>  // std::promise, std::future

#include "helper.h"

namespace Raceboat {

PluginWrapper::PluginWrapper(PluginContainer &container) :
    container(container), mThreadHandler(container.id + "-thread", 0, 0) {
    mThreadHandler.create_queue("lifecycle", std::numeric_limits<int>::max());
    mThreadHandler.create_queue("wait queue", std::numeric_limits<int>::min());
    mThreadHandler.start();
}

PluginWrapper::PluginWrapper(PluginContainer &container, std::shared_ptr<IRacePluginComms> plugin,
                             std::string description) :
    mPlugin(std::move(plugin)),
    mDescription(std::move(description)),
    container(container),
    mThreadHandler(container.id + "-thread", 0, 0) {
    mThreadHandler.create_queue("lifecycle", std::numeric_limits<int>::max());
    mThreadHandler.create_queue("wait queue", std::numeric_limits<int>::min());
    mThreadHandler.start();
}

PluginWrapper::~PluginWrapper() {
    TRACE_METHOD(getId());
    mThreadHandler.stop();
    mPlugin.reset();
}

template <typename T, typename... Args>
SdkResponse PluginWrapper::post(const std::string &logPrefix, const std::string &queue,
                                T &&function, Args &&... args) {
    auto postHandle = nextPostId++;
    std::string postId = std::to_string(postHandle);
    helper::logDebug(logPrefix + "Posting postId: " + postId);

    // we don't want to allow the default queue to be blocked
    bool supportTempError = !queue.empty();

    try {
        auto workFunc = [logPrefix, postId, function, supportTempError,
                         this](const auto &... args) mutable -> std::optional<PluginResponse> {
            helper::logDebug(logPrefix + "Calling postId: " + postId);
            PluginResponse status;
            try {
                // We can't move the arguments that are used for this call because the lambda may be
                // called more than once.
                status = std::mem_fn(function)(mPlugin, args...);
            } catch (std::exception &e) {
                helper::logError(logPrefix + "Threw exception: " + std::string(e.what()));
                status = PLUGIN_FATAL;
            } catch (...) {
                helper::logError(logPrefix + "Threw unknown exception");
                status = PLUGIN_FATAL;
            }

            helper::logDebug(logPrefix + "Returned " + pluginResponseToString(status) +
                             ", postId: " + postId);

            if (supportTempError && status == PLUGIN_TEMP_ERROR) {
                // block queue until unblock_queue is called
                return std::nullopt;
            } else if (status != PLUGIN_OK) {
                helper::logError(logPrefix + "Returned " + pluginResponseToString(status) +
                                 ", postId: " + postId);
                // TODO: how to handle an error?
                // sdk.asyncError(NULL_RACE_HANDLE, errorStatus);
            }

            return std::make_optional(status);
        };
        auto [success, queueSize, future] = mThreadHandler.post(
            queue, 0, -1, std::bind(std::move(workFunc), std::forward<Args>(args)...));

        if (success != Handler::PostStatus::OK) {
            helper::logError(logPrefix + "Post " + postId +
                             " failed with error: " + handlerPostStatusToString(success));
        }

        (void)queueSize;
        (void)future;
        // TODO: how to handle 'handle' return value
        return makeResponse(logPrefix, success == Handler::PostStatus::OK, queueSize, 0);
    } catch (std::out_of_range &e) {
        helper::logError("Queue does not exist. what:" + std::string(e.what()));
        return SDK_INVALID_ARGUMENT;
    }
}

template <typename T, typename... Args>
bool PluginWrapper::post_lifecycle(const std::string &logPrefix, int32_t timeoutInSeconds,
                                   T &&function, Args &&... args) {
    auto postHandle = nextPostId++;
    std::string postId = std::to_string(postHandle);
    helper::logDebug(logPrefix + "Posting postId: " + postId);

    try {
        auto workFunc = [logPrefix, postId, function, this](auto &&... args) mutable {
            helper::logDebug(logPrefix + "Calling postId: " + postId);
            PluginResponse status;
            try {
                status = std::mem_fn(function)(mPlugin, std::move(args)...);
            } catch (std::exception &e) {
                helper::logError(logPrefix + "Threw exception: " + std::string(e.what()));
                status = PLUGIN_FATAL;
            } catch (...) {
                helper::logError(logPrefix + "Threw unknown exception");
                status = PLUGIN_FATAL;
            }

            if (status != PLUGIN_OK) {
                helper::logError(logPrefix + "Returned " + pluginResponseToString(status) +
                                 ", postId: " + postId);
                // TODO: how to handle an error?
                // sdk.asyncError(NULL_RACE_HANDLE, errorStatus);
                status = PLUGIN_FATAL;
            }

            return std::make_optional(status);
        };
        auto [success, queueSize, future] = mThreadHandler.post(
            "lifecycle", 0, -1, std::bind(std::move(workFunc), std::forward<Args>(args)...));

        if (success != Handler::PostStatus::OK) {
            helper::logError(logPrefix + "Post " + postId +
                             " failed with error: " + handlerPostStatusToString(success));
            return false;
        }

        if (timeoutInSeconds == WAIT_FOREVER) {
            future.wait();
        } else {
            std::future_status status = future.wait_for(std::chrono::seconds(timeoutInSeconds));
            if (status != std::future_status::ready) {
                helper::logError(logPrefix + "timed out, took longer than " +
                                 std::to_string(timeoutInSeconds) + " seconds");
                return false;
            }
        }

        return future.get() == PLUGIN_OK;
    } catch (std::out_of_range &e) {
        helper::logError("default queue does not exist. This should never happen. what:" +
                         std::string(e.what()));
        return SDK_INVALID_ARGUMENT;
    }
}

void PluginWrapper::stopHandler() {
    TRACE_METHOD(getId());
    mThreadHandler.stop();
}

void PluginWrapper::waitForCallbacks() {
    auto [success, queueSize, future] =
        mThreadHandler.post("wait queue", 0, -1, [=] { return std::make_optional(true); });
    (void)success;
    (void)queueSize;
    future.wait();
}

bool PluginWrapper::init(const PluginConfig &pluginConfig) {
    TRACE_METHOD(getId());
    return post_lifecycle(logPrefix, WAIT_FOREVER, &IRacePluginComms::init, pluginConfig);
}

bool PluginWrapper::shutdown() {
    // By default, wait with a 30 second timeout for shutdown to complete. Based on internal
    // discussions this _should_ be ample time for the plugins to shutdown. However, this value
    // may be adjusted after testing with actual plugins.
    // Note that if the work queue grows large when this is called (e.g. shutdown gets called in the
    // middle of a large scale stress test) the timeout may get hit before shutdown even gets
    // called. Leaving this as-is for now as this seems to be a corner case, and should be easy to
    // diagnose from the logs.
    const std::int32_t defaultSecondsToWaitForShutdownToComplete = 30;
    return shutdown(defaultSecondsToWaitForShutdownToComplete);
}

bool PluginWrapper::shutdown(std::int32_t timeoutInSeconds) {
    TRACE_METHOD(getId(), timeoutInSeconds);
    return post_lifecycle(logPrefix, timeoutInSeconds, &IRacePluginComms::shutdown);
}

SdkResponse PluginWrapper::sendPackage(RaceHandle handle, const ConnectionID &connectionId,
                                       const EncPkg &pkg, int32_t postTimeout, uint64_t batchId) {
    TRACE_METHOD(getId(), handle, connectionId, postTimeout, batchId);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (connectionId.empty()) {
        helper::logError(logPrefix + "received empty connection id");
        return SDK_INVALID_ARGUMENT;
    }

    std::string queue = connectionId;
    return post(logPrefix, queue, &IRacePluginComms::sendPackage, handle, connectionId, pkg,
                std::numeric_limits<double>::infinity(), batchId);
}

SdkResponse PluginWrapper::openConnection(RaceHandle handle, LinkType linkType,
                                          const LinkID &linkId, const std::string &linkHints,
                                          int32_t priority, int32_t sendTimeout, int32_t) {
    TRACE_METHOD(getId(), handle, linkType, linkId, linkHints, priority, sendTimeout);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (linkType != LT_BIDI && linkType != LT_SEND && linkType != LT_RECV) {
        helper::logError(logPrefix + "received invalid link type");
        return SDK_INVALID_ARGUMENT;
    } else if (linkId.empty()) {
        helper::logError(logPrefix + "received empty link id");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::openConnection, handle, linkType, linkId,
                linkHints, sendTimeout);
}

SdkResponse PluginWrapper::closeConnection(RaceHandle handle, const ConnectionID &connectionId,
                                           int32_t) {
    TRACE_METHOD(getId(), handle, connectionId);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (connectionId.empty()) {
        helper::logError(logPrefix + "received empty connection id");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::closeConnection, handle, connectionId);
}

SdkResponse PluginWrapper::deactivateChannel(RaceHandle handle, const std::string &channelGid,
                                             std::int32_t) {
    TRACE_METHOD(getId(), handle, channelGid);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (channelGid.empty()) {
        // Ideally, this should be more strict and require the channel to actually be available on
        // this plugin, but there's no easy way to do that currently, so just checking like this.
        helper::logError(logPrefix + "received empty channel id");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::deactivateChannel, handle, channelGid);
}

SdkResponse PluginWrapper::activateChannel(RaceHandle handle, const std::string &channelGid,
                                           const std::string &roleName, std::int32_t) {
    TRACE_METHOD(getId(), handle, channelGid, roleName);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (channelGid.empty()) {
        helper::logError(logPrefix + "received empty channel id");
        return SDK_INVALID_ARGUMENT;
    } else if (roleName.empty()) {
        helper::logError(logPrefix + "received empty role name");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::activateChannel, handle, channelGid, roleName);
}

SdkResponse PluginWrapper::destroyLink(RaceHandle handle, const LinkID &linkId, std::int32_t) {
    TRACE_METHOD(getId(), handle, linkId);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (linkId.empty()) {
        helper::logError(logPrefix + "received empty link id");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::destroyLink, handle, linkId);
}

SdkResponse PluginWrapper::createLink(RaceHandle handle, const std::string &channelGid,
                                      std::int32_t) {
    TRACE_METHOD(getId(), handle, channelGid);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (channelGid.empty()) {
        helper::logError(logPrefix + "received empty channel id");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::createLink, handle, channelGid);
}

SdkResponse PluginWrapper::createBootstrapLink(RaceHandle handle, const std::string &channelGid,
                                               const std::string &passphrase, std::int32_t) {
    TRACE_METHOD(getId(), handle, channelGid, passphrase);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (channelGid.empty()) {
        helper::logError(logPrefix + "received empty channel id");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::createBootstrapLink, handle, channelGid,
                passphrase);
}

SdkResponse PluginWrapper::loadLinkAddress(RaceHandle handle, const std::string &channelGid,
                                           const std::string &linkAddress, std::int32_t) {
    TRACE_METHOD(getId(), handle, channelGid, linkAddress);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (channelGid.empty()) {
        helper::logError(logPrefix + "received empty channel id");
        return SDK_INVALID_ARGUMENT;
    } else if (linkAddress.empty()) {
        helper::logError(logPrefix + "received empty link address");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::loadLinkAddress, handle, channelGid, linkAddress);
}
SdkResponse PluginWrapper::loadLinkAddresses(RaceHandle handle, const std::string &channelGid,
                                             std::vector<std::string> linkAddresses, std::int32_t) {
    TRACE_METHOD(getId(), handle, channelGid);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (channelGid.empty()) {
        helper::logError(logPrefix + "received empty channel id");
        return SDK_INVALID_ARGUMENT;
    } else if (linkAddresses.empty()) {
        helper::logError(logPrefix + "received empty link addresses list");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::loadLinkAddresses, handle, channelGid,
                linkAddresses);
}

SdkResponse PluginWrapper::createLinkFromAddress(RaceHandle handle, const std::string &channelGid,
                                                 const std::string &linkAddress, std::int32_t) {
    TRACE_METHOD(getId(), handle, channelGid, linkAddress);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (channelGid.empty()) {
        helper::logError(logPrefix + "received empty channel id");
        return SDK_INVALID_ARGUMENT;
    } else if (linkAddress.empty()) {
        helper::logError(logPrefix + "received empty link address");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::createLinkFromAddress, handle, channelGid,
                linkAddress);
}

SdkResponse PluginWrapper::serveFiles(LinkID linkId, std::string path, int32_t) {
    TRACE_METHOD(getId(), linkId, path);

    if (linkId.empty()) {
        helper::logError(logPrefix + "received empty link id");
        return SDK_INVALID_ARGUMENT;
    } else if (path.empty()) {
        helper::logError(logPrefix + "received empty path");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::serveFiles, linkId, path);
}

SdkResponse PluginWrapper::flushChannel(RaceHandle handle, std::string channelGid, uint64_t batchId,
                                        int32_t) {
    TRACE_METHOD(getId(), handle, channelGid, batchId);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    } else if (channelGid.empty()) {
        helper::logError(logPrefix + "received empty channel id");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::flushChannel, handle, channelGid, batchId);
}

SdkResponse PluginWrapper::onUserInputReceived(RaceHandle handle, bool answered,
                                               const std::string &userResponse, int32_t) {
    TRACE_METHOD(getId(), handle, answered, userResponse);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::onUserInputReceived, handle, answered,
                userResponse);
}

SdkResponse PluginWrapper::onUserAcknowledgementReceived(RaceHandle handle, int32_t) {
    TRACE_METHOD(getId(), handle);

    if (handle == NULL_RACE_HANDLE) {
        helper::logError(logPrefix + "received invalid handle");
        return SDK_INVALID_ARGUMENT;
    }

    return post(logPrefix, "", &IRacePluginComms::onUserAcknowledgementReceived, handle);
}

void PluginWrapper::onConnectionStatusChanged(const ConnectionID &connId, ConnectionStatus status) {
    TRACE_METHOD(getId(), connId, status);
    if (status == CONNECTION_OPEN) {
        mThreadHandler.create_queue(connId, 0);
    } else {
        try {
            mThreadHandler.remove_queue(connId);
        } catch (...) {
            // this may happen if the connection fails to open.
        }
    }
}

SdkResponse PluginWrapper::unblockQueue(const ConnectionID &connId) {
    TRACE_METHOD(getId(), connId);
    mThreadHandler.unblock_queue(connId);
    return SDK_OK;
}

SdkResponse PluginWrapper::makeResponse(const std::string &functionName, bool success,
                                        size_t queueSize, RaceHandle handle) {
    double queueUtilization = (static_cast<double>(queueSize) / mThreadHandler.max_queue_size);
    SdkStatus status = SDK_OK;

    if (!success) {
        if (queueUtilization == 0) {
            helper::logWarning(functionName + " returning SDK_INVALID_ARGUMENT");
            status = SDK_INVALID_ARGUMENT;
        } else {
            helper::logWarning(functionName + " returning SDK_QUEUE_FULL");
            status = SDK_QUEUE_FULL;
        }
    }

    return SdkResponse(status, queueUtilization, handle);
}

}  // namespace Raceboat
