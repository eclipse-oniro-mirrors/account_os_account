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
#include "app_account_info.h"
#include "json_utils.h"
#include "app_account_info_json_parser.h"
#undef private
#include "mock_json_util.h"

using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;

namespace {
const std::string STRING_OWNER = "com.example.owner";
const std::string STRING_NAME = "name";
}

class AppAccountInfoJsonParserMockTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void);
    void TearDown(void);
};

void AppAccountInfoJsonParserMockTest::SetUpTestCase(void) {}
void AppAccountInfoJsonParserMockTest::TearDownTestCase(void) {}
void AppAccountInfoJsonParserMockTest::SetUp(void) {}
void AppAccountInfoJsonParserMockTest::TearDown(void) {}

/**
 * @tc.name: AppAccountInfo_ToJson_CreateJsonFail_001
 * @tc.desc: Test ToJson(AppAccountInfo) with CreateJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AppAccountInfoJsonParserMockTest, AppAccountInfo_ToJson_CreateJsonFail_001, TestSize.Level4)
{
    AppAccountInfo info;
    info.owner_ = STRING_OWNER;
    info.name_ = STRING_NAME;

    SetCreateJsonFail(true);
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetCreateJsonFail(false);
}

/**
 * @tc.name: AppAccountInfo_ToJson_AddStringToJsonFail_001
 * @tc.desc: Test ToJson(AppAccountInfo) with AddStringToJson failure on main object.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AppAccountInfoJsonParserMockTest, AppAccountInfo_ToJson_AddStringToJsonFail_001, TestSize.Level4)
{
    AppAccountInfo info;
    info.owner_ = STRING_OWNER;
    info.name_ = STRING_NAME;

    SetTimes(0, 1, "AddStringToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddStringToJson");
}

/**
 * @tc.name: AppAccountInfo_ToJson_AddIntToJsonFail_001
 * @tc.desc: Test ToJson(AppAccountInfo) with AddIntToJson failure on main object.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AppAccountInfoJsonParserMockTest, AppAccountInfo_ToJson_AddIntToJsonFail_001, TestSize.Level4)
{
    AppAccountInfo info;
    info.owner_ = STRING_OWNER;
    info.name_ = STRING_NAME;

    SetTimes(0, 1, "AddIntToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddIntToJson");
}

/**
 * @tc.name: AppAccountInfo_ToJson_AddBoolToJsonFail_001
 * @tc.desc: Test ToJson(AppAccountInfo) with AddBoolToJson failure on main object.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AppAccountInfoJsonParserMockTest, AppAccountInfo_ToJson_AddBoolToJsonFail_001, TestSize.Level4)
{
    AppAccountInfo info;
    info.owner_ = STRING_OWNER;
    info.name_ = STRING_NAME;

    SetTimes(0, 1, "AddBoolToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddBoolToJson");
}

/**
 * @tc.name: AppAccountInfo_ToJson_AddSetStringToJsonFail_001
 * @tc.desc: Test ToJson(AppAccountInfo) with AddSetStringToJson failure on main object.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AppAccountInfoJsonParserMockTest, AppAccountInfo_ToJson_AddSetStringToJsonFail_001, TestSize.Level4)
{
    AppAccountInfo info;
    info.owner_ = STRING_OWNER;
    info.name_ = STRING_NAME;

    SetTimes(0, 1, "AddSetStringToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddSetStringToJson");
}

/**
 * @tc.name: AppAccountInfo_ToJson_AddObjToJsonFail_001
 * @tc.desc: Test ToJson(AppAccountInfo) with AddObjToJson failure on main object.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AppAccountInfoJsonParserMockTest, AppAccountInfo_ToJson_AddObjToJsonFail_001, TestSize.Level4)
{
    AppAccountInfo info;
    info.owner_ = STRING_OWNER;
    info.name_ = STRING_NAME;

    SetTimes(0, 1, "AddObjToJson");
    auto result = ToJson(info);
    EXPECT_EQ(result, nullptr);
    SetTimes(0, 0, "AddObjToJson");
}
