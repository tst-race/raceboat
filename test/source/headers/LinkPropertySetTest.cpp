
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


#include "../../common/race_printers.h"
#include "common/LinkPropertySet.h"
#include "gtest/gtest.h"

TEST(LinkPropertySet, equals) {
    LinkPropertySet a;
    a.bandwidth_bps = 1;
    a.latency_ms = 2;
    a.loss = 3;
    LinkPropertySet b = a;

    EXPECT_EQ(a, b);
}

TEST(LinkPropertySet, bandwidth_bps_not_equals) {
    LinkPropertySet a;
    a.bandwidth_bps = 1;
    a.latency_ms = 2;
    a.loss = 3;
    LinkPropertySet b = a;
    b.bandwidth_bps = 4;

    EXPECT_NE(a, b);
}

TEST(LinkPropertySet, latency_ms_not_equals) {
    LinkPropertySet a;
    a.bandwidth_bps = 1;
    a.latency_ms = 2;
    a.loss = 3;
    LinkPropertySet b = a;
    b.latency_ms = 4;

    EXPECT_NE(a, b);
}

TEST(LinkPropertySet, loss_not_equals) {
    LinkPropertySet a;
    a.bandwidth_bps = 1;
    a.latency_ms = 2;
    a.loss = 3;
    LinkPropertySet b = a;
    b.loss = 4;

    EXPECT_NE(a, b);
}