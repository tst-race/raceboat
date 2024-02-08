
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
#include "common/ChannelRole.h"
#include "gtest/gtest.h"

TEST(ChannelRole, equals) {
    ChannelRole a;
    a.roleName = "some role";
    a.mechanicalTags = {"test_tag_1", "test_tag_2"};
    a.behavioralTags = {"test_behavioral_tag_1", "test_behavioral_tag_2"};
    a.linkSide = LS_BOTH;

    ChannelRole b = a;

    EXPECT_EQ(a, b);
}

TEST(ChannelRole, name_not_equals) {
    ChannelRole a;
    a.roleName = "some role";
    a.mechanicalTags = {"test_tag_1", "test_tag_2"};
    a.behavioralTags = {"test_behavioral_tag_1", "test_behavioral_tag_2"};
    a.linkSide = LS_BOTH;

    ChannelRole b = a;
    b.roleName = "some other role";

    EXPECT_NE(a, b);
}

TEST(ChannelRole, mechanical_tags_not_equals) {
    ChannelRole a;
    a.roleName = "some role";
    a.mechanicalTags = {"test_tag_1", "test_tag_2"};
    a.behavioralTags = {"test_behavioral_tag_1", "test_behavioral_tag_2"};
    a.linkSide = LS_BOTH;

    ChannelRole b = a;
    b.mechanicalTags = {"test_tag_1", "test_tag_3"};

    EXPECT_NE(a, b);
}

TEST(ChannelRole, behavioral_tags_not_equals) {
    ChannelRole a;
    a.roleName = "some role";
    a.mechanicalTags = {"test_tag_1", "test_tag_2"};
    a.behavioralTags = {"test_behavioral_tag_1", "test_behavioral_tag_2"};
    a.linkSide = LS_BOTH;

    ChannelRole b = a;
    b.behavioralTags = {"test_behavioral_tag_1", "test_behavioral_tag_3"};

    EXPECT_NE(a, b);
}

TEST(ChannelRole, link_side_not_equals) {
    ChannelRole a;
    a.roleName = "some role";
    a.mechanicalTags = {"test_tag_1", "test_tag_2"};
    a.behavioralTags = {"test_behavioral_tag_1", "test_behavioral_tag_2"};
    a.linkSide = LS_BOTH;

    ChannelRole b = a;
    a.linkSide = LS_CREATOR;

    EXPECT_NE(a, b);
}