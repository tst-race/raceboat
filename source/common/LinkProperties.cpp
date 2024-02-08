
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

#include "race/common/LinkProperties.h"

#include <numeric>
#include <sstream>

LinkProperties::LinkProperties() :
    linkType(LT_UNDEF),
    transmissionType(TT_UNDEF),
    connectionType(CT_UNDEF),
    sendType(ST_UNDEF),
    reliable(false),
    isFlushable(false),
    duration_s(-1),
    period_s(-1),
    mtu(-1) {}

std::string linkPropertiesToString(const LinkProperties &props) {
    std::stringstream ss;

    ss << "LinkProperties {";
    ss << " LinkType = " + linkTypeToString(props.linkType);
    ss << " TransmissionType = " + transmissionTypeToString(props.transmissionType);
    ss << " ConnectionType = " + connectionTypeToString(props.connectionType);
    ss << " SendType = " + sendTypeToString(props.sendType);
    ss << " reliable = " + std::to_string(props.reliable);
    ss << " isFlushable = " + std::to_string(props.isFlushable);
    ss << " worse = " + linkPropertyPairToString(props.worst);
    ss << " expected = " + linkPropertyPairToString(props.expected);
    ss << " best = " + linkPropertyPairToString(props.best);
    ss << " channelGid = " + props.channelGid;
    ss << " linkAddress = " + props.linkAddress;
    ss << " supported_hints = " + std::accumulate(props.supported_hints.begin(),
                                                  props.supported_hints.end(), std::string{});
    ss << "}";

    return ss.str();
}