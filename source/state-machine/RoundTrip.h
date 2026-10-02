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

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "base64.h"

namespace Raceboat {

// The wire framing shared by every bootstrap hello/response exchange: a
// fixed-size raw packageId prefix (consumed by the SDK's package-routing
// layer before state-machine code ever sees it) followed by a JSON body
// carrying arbitrary fields plus an optional base64-encoded payload. This
// exact framing used to be hand-built/hand-parsed independently in
// BootstrapDialStateMachine.cpp (hello, response) and
// BootstrapListenStateMachine.cpp/BootstrapPreConduitStateMachine.cpp
// (hello, response) - RoundTrip centralizes it into one instance of the
// logic instead of three copies.
class RoundTrip {
public:
  // Builds the raw bytes for a request/response message: `routingPrefix`
  // (a packageIdLen-byte raw tag used by the SDK's package-routing layer,
  // all-zero for the well-known "any hello" tag or a real packageId for a
  // reply) followed by `fields.dump()`. If `payload` is non-empty it is
  // base64-encoded into fields["message"] first.
  static std::vector<uint8_t> buildMessage(const std::string &routingPrefix,
                                           nlohmann::json fields,
                                           const std::vector<uint8_t> &payload = {}) {
    if (!payload.empty()) {
      fields["message"] = base64::encode(payload);
    }
    std::string message = routingPrefix + fields.dump();
    return std::vector<uint8_t>(message.begin(), message.end());
  }

  // Parses an already-routing-prefix-stripped message body into JSON
  // fields. Returns false if the bytes aren't valid JSON.
  static bool parseMessage(const std::vector<uint8_t> &data,
                           nlohmann::json &fieldsOut) {
    try {
      std::string str{data.begin(), data.end()};
      fieldsOut = nlohmann::json::parse(str);
      return true;
    } catch (std::exception &) {
      return false;
    }
  }

  // Extracts and base64-decodes fields["message"] into raw payload bytes,
  // if present; empty vector otherwise.
  static std::vector<uint8_t> extractPayload(const nlohmann::json &fields) {
    if (!fields.contains("message")) {
      return {};
    }
    std::string messageB64 = fields.at("message");
    return base64::decode(messageB64);
  }

  // Encodes a raw packageId (fixed-length binary routing tag) using the
  // base64 JSON-field convention used to carry a *reply* routing tag inside
  // a request's body (distinct from the request's own raw routing prefix).
  static std::string encodePackageIdField(const std::string &packageId) {
    return base64::encode(std::vector<uint8_t>(packageId.begin(), packageId.end()));
  }

  // Decodes the "packageId" JSON field back into a raw binary routing tag,
  // validating its length. Returns false if missing or malformed.
  static bool decodePackageIdField(const nlohmann::json &fields, size_t expectedLen,
                                   std::string &packageIdOut) {
    if (!fields.contains("packageId")) {
      return false;
    }
    std::string packageIdB64 = fields.at("packageId");
    std::vector<uint8_t> packageIdBytes = base64::decode(packageIdB64);
    if (packageIdBytes.size() != expectedLen) {
      return false;
    }
    packageIdOut = std::string(packageIdBytes.begin(), packageIdBytes.end());
    return true;
  }
};

} // namespace Raceboat
