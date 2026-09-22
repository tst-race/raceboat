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

#include "../../source/state-machine/LinkAddressValidation.h"

#include "gtest/gtest.h"

namespace Raceboat::detail {

TEST(LinkAddressValidationTest, AllowsPluginAddedFields) {
  const auto requested = nlohmann::json{
      {"hashtag", "example"}, {"hostname", "whiteboard"}, {"port", 5000}};
  const auto received = nlohmann::json{{"hashtag", "example"},
                                       {"hostname", "whiteboard"},
                                       {"port", 5000},
                                       {"maxTries", 120},
                                       {"timestamp", -1.0}};

  EXPECT_TRUE(containsRequestedAddressFields(requested, received));
}

TEST(LinkAddressValidationTest, RejectsMissingRequestedFields) {
  const auto requested = nlohmann::json{{"hostname", "whiteboard"},
                                        {"port", 5000}};
  const auto received = nlohmann::json{{"hostname", "whiteboard"}};

  EXPECT_FALSE(containsRequestedAddressFields(requested, received));
}

TEST(LinkAddressValidationTest, RejectsChangedRequestedFields) {
  const auto requested = nlohmann::json{{"hostname", "whiteboard"},
                                        {"port", 5000}};
  const auto received = nlohmann::json{{"hostname", "other"},
                                       {"port", 5000}};

  EXPECT_FALSE(containsRequestedAddressFields(requested, received));
}

TEST(LinkAddressValidationTest, ValidatesNestedObjectsAndArrays) {
  const auto requested = nlohmann::json{
      {"transport", { {"host", "whiteboard"}, {"ports", {5000, 5001}} }}};
  const auto received = nlohmann::json{
      {"transport", { {"host", "whiteboard"}, {"ports", {5000, 5001}},
                       {"timeout", 30} }},
       {"version", 2}};

  EXPECT_TRUE(containsRequestedAddressFields(requested, received));
}

} // namespace Raceboat::detail