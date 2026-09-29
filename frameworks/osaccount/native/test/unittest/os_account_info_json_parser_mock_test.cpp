/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#define private public
#include "os_account_info.h"
#include "os_account_info_json_parser.h"
#include "sub_profile_context.h"
#include "json_utils.h"
#undef private
#include "mock_json_util.h"

using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;

namespace {
const std::string JSON_STRING =
    "{\"localId\":12,\"localName\":\"account\",\"shortName\":\"ac\",\"type\":0,"
    "\"constraints\":[\"one\"],\"isVerified\":true,\"photo\":\"\",\"createTime\":100,"
    "\"lastLoginTime\":100,\"serialNumber\":200,\"isActived\":false,"
    "\"isCreateCompleted\":true,\"toBeRemoved\":false,\"credentialId\":0,"
    "\"displayId\":0,\"isForeground\":false,\"isLoggedIn\":true,"
    "\"isDataRemovable\":false,\"creatorType\":0,\"version\":1,"
    "\"domainInfo\":{\"domain\":\"d\",\"accountName\":\"n\",\"accountId\":\"id\","
    "\"domainAccountStatus\":2,\"domainServerConfigId\":\"cfg\",\"additionInfo\":\"add\"}}";
}

class OsAccountInfoJsonParserMockTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void);
    void TearDown(void);
};

void OsAccountInfoJsonParserMockTest::SetUpTestCase(void) {}
void OsAccountInfoJsonParserMockTest::TearDownTestCase(void) {}
void OsAccountInfoJsonParserMockTest::SetUp(void) {}
void OsAccountInfoJsonParserMockTest::TearDown(void) {}

/**
 * @tc.name: OsAccountInfo_ToJson_CreateJsonFail_001
 * @tc.desc: Test ToJson(OsAccountInfo) with CreateJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountInfoJsonParserMockTest, OsAccountInfo_ToJson_CreateJsonFail_001, TestSize.Level4)
{
    auto objJson = CreateJsonFromString(JSON_STRING);
    ASSERT_NE(objJson, nullptr);
    OsAccountInfo info;
    FromJson(objJson.get(), info);

    SetCreateJsonFail(true);
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetCreateJsonFail(false);
}

/**
 * @tc.name: DomainAccountInfo_ToJson_CreateJsonFail_001
 * @tc.desc: Test ToJson(DomainAccountInfo) with CreateJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountInfoJsonParserMockTest, DomainAccountInfo_ToJson_CreateJsonFail_001, TestSize.Level4)
{
    DomainAccountInfo info("domain", "name", "id");

    SetCreateJsonFail(true);
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetCreateJsonFail(false);
}

/**
 * @tc.name: ToJsonSubProfile_CreateJsonFail_001
 * @tc.desc: Test ToJsonSubProfile with CreateJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountInfoJsonParserMockTest, ToJsonSubProfile_CreateJsonFail_001, TestSize.Level4)
{
    SubProfileContext data;
    data.nextSubProfileId = 1;
    data.subProfileIdList = {100};
    data.nextSubProfileIndex = 1;

    SetCreateJsonFail(true);
    auto result = ToJsonSubProfile(data);
    EXPECT_EQ(result, nullptr);
    SetCreateJsonFail(false);
}

/**
 * @tc.name: OsAccountInfo_ToJson_AddIntToJsonFail_001
 * @tc.desc: Test ToJson(OsAccountInfo) with AddIntToJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountInfoJsonParserMockTest, OsAccountInfo_ToJson_AddIntToJsonFail_001, TestSize.Level4)
{
    auto objJson = CreateJsonFromString(JSON_STRING);
    ASSERT_NE(objJson, nullptr);
    OsAccountInfo info;
    FromJson(objJson.get(), info);

    SetTimes(0, 1, "AddIntToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddIntToJson");
}

/**
 * @tc.name: OsAccountInfo_ToJson_AddStringToJsonFail_001
 * @tc.desc: Test ToJson(OsAccountInfo) with AddStringToJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountInfoJsonParserMockTest, OsAccountInfo_ToJson_AddStringToJsonFail_001, TestSize.Level4)
{
    auto objJson = CreateJsonFromString(JSON_STRING);
    ASSERT_NE(objJson, nullptr);
    OsAccountInfo info;
    FromJson(objJson.get(), info);

    SetTimes(0, 1, "AddStringToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddStringToJson");
}

/**
 * @tc.name: OsAccountInfo_ToJson_AddBoolToJsonFail_001
 * @tc.desc: Test ToJson(OsAccountInfo) with AddBoolToJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountInfoJsonParserMockTest, OsAccountInfo_ToJson_AddBoolToJsonFail_001, TestSize.Level4)
{
    auto objJson = CreateJsonFromString(JSON_STRING);
    ASSERT_NE(objJson, nullptr);
    OsAccountInfo info;
    FromJson(objJson.get(), info);

    SetTimes(0, 1, "AddBoolToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddBoolToJson");
}

/**
 * @tc.name: OsAccountInfo_ToJson_AddObjToJsonFail_001
 * @tc.desc: Test ToJson(OsAccountInfo) with AddObjToJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountInfoJsonParserMockTest, OsAccountInfo_ToJson_AddObjToJsonFail_001, TestSize.Level4)
{
    auto objJson = CreateJsonFromString(JSON_STRING);
    ASSERT_NE(objJson, nullptr);
    OsAccountInfo info;
    FromJson(objJson.get(), info);

    SetTimes(0, 1, "AddObjToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddObjToJson");
}

/**
 * @tc.name: DomainAccountInfo_ToJson_AddStringToJsonFail_001
 * @tc.desc: Test ToJson(DomainAccountInfo) with AddStringToJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountInfoJsonParserMockTest, DomainAccountInfo_ToJson_AddStringToJsonFail_001, TestSize.Level4)
{
    DomainAccountInfo info("domain", "name", "id");

    SetTimes(0, 1, "AddStringToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddStringToJson");
}
