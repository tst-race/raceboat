
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

#include "common/LinkPropertyPair.h"
#include "../../common/race_printers.h"
#include "gtest/gtest.h"

TEST(LinkPropertyPair, equals) {
  LinkPropertySet a1;
  a1.bandwidth_bps = 1;
  a1.latency_ms = 2;
  a1.loss = 3;
  LinkPropertySet a2;
  a2.bandwidth_bps = 4;
  a2.latency_ms = 5;
  a2.loss = 6;
  LinkPropertyPair a;
  a.send = a1;
  a.receive = a2;
  LinkPropertyPair b = a;

  EXPECT_EQ(a, b);
}

TEST(LinkPropertyPair, send_not_equals) {
  LinkPropertySet a1;
  a1.bandwidth_bps = 1;
  a1.latency_ms = 2;
  a1.loss = 3;
  LinkPropertySet a2;
  a2.bandwidth_bps = 4;
  a2.latency_ms = 5;
  a2.loss = 6;
  LinkPropertyPair a;
  a.send = a1;
  a.receive = a2;
  LinkPropertyPair b = a;
  b.send.bandwidth_bps = 7;

  EXPECT_NE(a, b);
}

TEST(LinkPropertyPair, receive_not_equals) {
  LinkPropertySet a1;
  a1.bandwidth_bps = 1;
  a1.latency_ms = 2;
  a1.loss = 3;
  LinkPropertySet a2;
  a2.bandwidth_bps = 4;
  a2.latency_ms = 5;
  a2.loss = 6;
  LinkPropertyPair a;
  a.send = a1;
  a.receive = a2;
  LinkPropertyPair b = a;
  b.receive.bandwidth_bps = 7;

  EXPECT_NE(a, b);
}
