
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
// #include "../../include/race/unified/IRacePluginComms.h"
#include "IRacePluginComms.h"

class UnifiedTestStub : public IRacePluginComms {
public:
    explicit UnifiedTestStub(IRaceSdkComms *raceSdkIn);
    virtual ~UnifiedTestStub();
    virtual PluginResponse init(const PluginConfig &pluginConfig) override;

    virtual PluginResponse shutdown() override;

    virtual PluginResponse sendPackage(RaceHandle handle, ConnectionID connectionId, EncPkg pkg,
                                       double timeoutTimestamp, uint64_t batchId) override;
    virtual PluginResponse openConnection(RaceHandle handle, LinkType linkType, LinkID linkId,
                                          std::string hints, int32_t sendTimeout) override;
    virtual PluginResponse closeConnection(RaceHandle handle, ConnectionID connectionId) override;
    virtual PluginResponse destroyLink(RaceHandle handle, LinkID linkId) override;
    virtual PluginResponse createLink(RaceHandle handle, std::string channelGid) override;
    virtual PluginResponse loadLinkAddress(RaceHandle handle, std::string channelGid,
                                           std::string linkAddress) override;
    virtual PluginResponse loadLinkAddresses(RaceHandle handle, std::string channelGid,
                                             std::vector<std::string> linkAddresses) override;
    virtual PluginResponse createLinkFromAddress(RaceHandle handle, std::string channelGid,
                                                 std::string linkAddress) override;
    virtual PluginResponse activateChannel(RaceHandle handle, std::string channelGid,
                                           std::string roleName) override;
    virtual PluginResponse deactivateChannel(RaceHandle handle, std::string channelGid) override;
    virtual PluginResponse onUserInputReceived(RaceHandle handle, bool answered,
                                               const std::string &response) override;
    PluginResponse onUserAcknowledgementReceived(RaceHandle handle) override;

    virtual PluginResponse createBootstrapLink(RaceHandle handle, std::string channelGid,
                                               std::string passphrase) override;
    virtual PluginResponse serveFiles(LinkID linkId, std::string path) override;

    virtual PluginResponse flushChannel(RaceHandle handle, std::string channelGid,
                                        uint64_t batchId) override;
};
