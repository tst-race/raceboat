
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

#include "race/common/ChannelProperties.h"

#include <sstream>
#include <stdexcept>

#include "helper.h"

using namespace Raceboat;

ChannelProperties::ChannelProperties()
    : channelStatus(CHANNEL_UNDEF), linkDirection(LD_UNDEF),
      transmissionType(TT_UNDEF), connectionType(CT_UNDEF), sendType(ST_UNDEF),
      multiAddressable(false), reliable(false), bootstrap(false),
      isFlushable(false), duration_s(-1), period_s(-1), mtu(-1), maxLinks(-1),
      maxCreatorsPerLoader(-1), maxLoadersPerCreator(-1), maxSendsPerInterval(-1),
      secondsPerInterval(-1), intervalEndTime(0), sendsRemainingInInterval(-1) {
}

std::string channelPropertiesToString(const ChannelProperties &props) {
  std::stringstream ss;

  ss << "ChannelProperties {";
  ss << "channelGid: " << props.channelGid << ", ";
  ss << "channelStatus: " << channelStatusToString(props.channelStatus) << ", ";
  ss << "linkDirection: " << linkDirectionToString(props.linkDirection) << ", ";
  ss << "transmissionType: " << transmissionTypeToString(props.transmissionType)
     << ", ";
  ss << "connectionType: " << connectionTypeToString(props.connectionType)
     << ", ";
  ss << "sendType: " << sendTypeToString(props.sendType) << ", ";
  ss << "multiAddressable: " << props.multiAddressable << ", ";
  ss << "reliable: " << props.reliable << ", ";
  ss << "bootstrap: " << props.bootstrap << ", ";
  ss << "isFlushable: " << props.isFlushable << ", ";
  ss << "duration_s: " << props.duration_s << ", ";
  ss << "period_s: " << props.period_s << ", ";
  ss << "mtu: " << props.mtu << ", ";
  ss << "creatorExpected: " << linkPropertyPairToString(props.creatorExpected)
     << ", ";
  ss << "loaderExpected: " << linkPropertyPairToString(props.loaderExpected)
     << ", ";
  ss << "supportedHints: " << stringVectorToString(props.supportedHints)
     << ", ";
  ss << "maxLinks: " << props.maxLinks << ", ";
  ss << "maxCreatorsPerLoader: " << props.maxCreatorsPerLoader << ", ";
  ss << "maxLoadersPerCreator: " << props.maxLoadersPerCreator << ", ";
  ss << "roles: [";
  for (const ChannelRole &role : props.roles) {
    ss << channelRoleToString(role) << ", ";
  }
  ss << "]}, ";
  ss << "currentRole: " << channelRoleToString(props.currentRole) << ", ";
  ss << "maxSendsPerInterval: " << props.maxSendsPerInterval << ", ";
  ss << "secondsPerInterval: " << props.secondsPerInterval << ", ";
  ss << "intervalEndTime: " << props.intervalEndTime << ", ";
  ss << "sendsRemainingInInterval: " << props.sendsRemainingInInterval << "} ";
  return ss.str();
}

std::string linkDirectionToString(LinkDirection linkDirection) {
  switch (linkDirection) {
  case LD_UNDEF:
    return "LD_UNDEF";
  case LD_CREATOR_TO_LOADER:
    return "LD_CREATOR_TO_LOADER";
  case LD_LOADER_TO_CREATOR:
    return "LD_LOADER_TO_CREATOR";
  case LD_BIDI:
    return "LD_BIDI";
  default:
    return "ERROR: INVALID LINK DIRECTION: " + std::to_string(linkDirection);
  }
}

std::ostream &operator<<(std::ostream &out, LinkDirection linkDirection) {
  return out << linkDirectionToString(linkDirection);
}

LinkDirection linkDirectionFromString(const std::string &linkDirectionString) {
  if (linkDirectionString == "LD_UNDEF") {
    return LD_UNDEF;
  } else if (linkDirectionString == "LD_CREATOR_TO_LOADER") {
    return LD_CREATOR_TO_LOADER;
  } else if (linkDirectionString == "LD_LOADER_TO_CREATOR") {
    return LD_LOADER_TO_CREATOR;
  } else if (linkDirectionString == "LD_BIDI") {
    return LD_BIDI;
  } else {
    throw std::invalid_argument(
        "Invalid argument to linkDirectionFromString: " + linkDirectionString);
  }
}

bool channelStaticPropertiesEqual(const ChannelProperties &a,
                                  const ChannelProperties &b) {
  return (a.channelGid == b.channelGid && a.linkDirection == b.linkDirection &&
          a.transmissionType == b.transmissionType &&
          a.connectionType == b.connectionType && a.sendType == b.sendType &&
          a.multiAddressable == b.multiAddressable &&
          a.reliable == b.reliable && a.bootstrap == b.bootstrap &&
          a.isFlushable == b.isFlushable && a.duration_s == b.duration_s &&
          a.period_s == b.period_s && a.supportedHints == b.supportedHints &&
          a.mtu == b.mtu && a.creatorExpected == b.creatorExpected &&
          a.loaderExpected == b.loaderExpected && a.maxLinks == b.maxLinks &&
          a.maxCreatorsPerLoader == b.maxCreatorsPerLoader &&
          a.maxLoadersPerCreator == b.maxLoadersPerCreator && a.roles == b.roles);
}

template <typename T>
static bool parseField(const nlohmann::json &config, T &dest,
                       const std::string &fieldName,
                       const std::string &channelGid) {
  try {
    dest = config.at(fieldName).get<T>();
    return true;
  } catch (std::exception &e) {
    helper::logDebug("Using default value for " + fieldName + " because it was not found in manifest for channel '" +
                     channelGid + "': " + std::string(e.what()));
    return false;
  }
}

static bool parseLinkPropertySet(const nlohmann::json &propsJson,
                                 const std::string &fieldName,
                                 LinkPropertySet &set,
                                 const std::string &channelGid,
                                 const std::string &pairField) {
  try {
    bool success = true;
    json lpSetJson = propsJson.at(fieldName);
    success &=
        parseField(lpSetJson, set.bandwidth_bps, "bandwidth_bps", channelGid);
    success &= parseField(lpSetJson, set.latency_ms, "latency_ms", channelGid);
    success &= parseField(lpSetJson, set.loss, "loss", channelGid);
    return success;
  } catch (std::exception &e) {
    helper::logDebug("Using default value for " + fieldName + " because it was not found in the manifest for channel '" +
                     channelGid + "', field '" + pairField +
                     "': " + std::string(e.what()));
    return false;
  }
}

static bool parseLinkPropertyPair(const nlohmann::json &propsJson,
                                  const std::string &fieldName,
                                  LinkPropertyPair &pair,
                                  const std::string &channelGid) {
  try {
    bool success = true;
    json lpPairJson = propsJson.at(fieldName);
    success &= parseLinkPropertySet(lpPairJson, "send", pair.send, channelGid,
                                    fieldName);
    success &= parseLinkPropertySet(lpPairJson, "receive", pair.receive,
                                    channelGid, fieldName);
    return success;
  } catch (std::exception &e) {
    helper::logDebug("Using default value for " + fieldName + " because it was not found in the manifest for channel '" +
                     channelGid + 
                     "': " + std::string(e.what()));
    return false;
  }
}

static bool parseRoles(const nlohmann::json &propsJson,
                       std::vector<ChannelRole> &roles,
                       const std::string &fieldName,
                       const std::string &channelGid) {
  bool success = true;
  try {
    json rolesJson = propsJson.at(fieldName);

    for (auto &roleJson : rolesJson) {
      ChannelRole role;
      success &= parseField(roleJson, role.roleName, "roleName", channelGid);
      success &= parseField(roleJson, role.mechanicalTags, "mechanicalTags",
                            channelGid);
      success &= parseField(roleJson, role.behavioralTags, "behavioralTags",
                            channelGid);

      std::string tmp = linkSideToString(LS_UNDEF);
      success &= parseField(roleJson, tmp, "linkSide", channelGid);
      role.linkSide = linkSideFromString(tmp);

      roles.push_back(role);
    }
  } catch (std::exception &e) {
    helper::logDebug("Using default value for " + fieldName + " because it was not found in the manifest for channel '" +
                     channelGid +
                     "': " + std::string(e.what()));
    success = false;
  } catch (...) {
    helper::logDebug("Using default value for " + fieldName + " because it was not found in the manifest for channel '" +
                     channelGid);
    success = false;
  }

  // Insert default role if none provided
  if (roles.empty()) {
    helper::logDebug("No roles specified in manifest, inserting \"default\" role for " + channelGid);
    ChannelRole role;
    role.roleName = "default";
    role.linkSide = LS_BOTH;
    roles.push_back(role);
  }
  return success;
}

void from_json(const nlohmann::json &j, ChannelProperties &props) {
  bool success = true;
  try {
    std::string tmp;

    // REQUIRED fields: channelGid, linkDirection, transmissionType
    props.channelStatus = CHANNEL_UNSUPPORTED;
    props.channelGid = "<missing channelGid>";
    success &= parseField(j, props.channelGid, "channelGid", props.channelGid);
    
    tmp = linkDirectionToString(LD_UNDEF);
    success &= parseField(j, tmp, "linkDirection", props.channelGid);
    props.linkDirection = linkDirectionFromString(tmp);

    // OPTIONAL fields
    tmp = transmissionTypeToString(TT_UNICAST);
    parseField(j, tmp, "transmissionType", props.channelGid);
    props.transmissionType = transmissionTypeFromString(tmp);

    parseField(j, props.bootstrap, "bootstrap", props.channelGid);
    parseField(j, props.duration_s, "duration_s", props.channelGid);
    parseField(j, props.isFlushable, "isFlushable", props.channelGid);
    parseField(j, props.mtu, "mtu", props.channelGid);
    parseField(j, props.multiAddressable, "multiAddressable",
                          props.channelGid);
    parseField(j, props.period_s, "period_s", props.channelGid);
    parseField(j, props.reliable, "reliable", props.channelGid);
    parseField(j, props.supportedHints, "supportedHints",
                          props.channelGid);
    parseField(j, props.maxLinks, "maxLinks", props.channelGid);
    parseField(j, props.maxCreatorsPerLoader, "maxCreatorsPerLoader",
                          props.channelGid);
    parseField(j, props.maxLoadersPerCreator, "maxLoadersPerCreator",
                          props.channelGid);
    parseLinkPropertyPair(j, "creatorExpected",
                                     props.creatorExpected, props.channelGid);
    parseLinkPropertyPair(j, "loaderExpected", props.loaderExpected,
                                     props.channelGid);


    tmp = connectionTypeToString(CT_UNDEF);
    parseField(j, tmp, "connectionType", props.channelGid);
    props.connectionType = connectionTypeFromString(tmp);


    tmp = sendTypeToString(ST_UNDEF);
    parseField(j, tmp, "sendType", props.channelGid);
    props.sendType = sendTypeFromString(tmp);

    parseRoles(j, props.roles, "roles", props.channelGid);

    parseField(j, props.maxSendsPerInterval, "maxSendsPerInterval",
                          props.channelGid);
    parseField(j, props.secondsPerInterval, "secondsPerInterval",
                          props.channelGid);
    parseField(j, props.intervalEndTime, "intervalEndTime",
                          props.channelGid);
    parseField(j, props.sendsRemainingInInterval,
                          "sendsRemainingInInterval", props.channelGid);
  } catch (std::exception &ex) {
    helper::logError("exception \"" + std::string(ex.what()) +
                     "\" occurred while parsing channelID " + props.channelGid);
    success = false;
  } catch (...) {
    helper::logError("unknown exception parsing channelID " + props.channelGid);
    success = false;
  }
  if (!success) {
    helper::logError("Failed to parse channel '" + props.channelGid + "'");
    helper::logInfo("contents: " + j.dump());
    throw std::domain_error("Failed to parse channel '" + props.channelGid +
                            "'");
  }
}
