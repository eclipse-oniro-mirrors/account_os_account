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
const std::string STRING_KEY = "key";
const std::string STRING_VALUE = "value";
const std::string STRING_CREDENTIAL_TYPE = "password";
const std::string STRING_CREDENTIAL = "1024";
} // namespace

class AppAccountInfoMockTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void);
    void TearDown(void);
};

void AppAccountInfoMockTest::SetUpTestCase(void) {}
void AppAccountInfoMockTest::TearDownTestCase(void) {}
void AppAccountInfoMockTest::SetUp(void) {}
void AppAccountInfoMockTest::TearDown(void) {}

/**
 * @tc.name: AppAccountInfo_SetAssociatedData_AddStringToJsonFail_001
 * @tc.desc: Test SetAssociatedData with AddStringToJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AppAccountInfoMockTest, AppAccountInfo_SetAssociatedData_AddStringToJsonFail_001, TestSize.Level4)
{
    AppAccountInfo appAccountInfo;
    appAccountInfo.name_ = STRING_NAME;
    appAccountInfo.owner_ = STRING_OWNER;
    SetTimes(0, 1, "AddStringToJson");
    ErrCode ret = appAccountInfo.SetAssociatedData(STRING_KEY, STRING_VALUE);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_DUMP_JSON_ERROR);
    SetTimes(0, 0, "AddStringToJson");
}

/**
 * @tc.name: AppAccountInfo_SetAccountCredential_AddStringToJsonFail_001
 * @tc.desc: Test SetAccountCredential with AddStringToJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AppAccountInfoMockTest, AppAccountInfo_SetAccountCredential_AddStringToJsonFail_001, TestSize.Level4)
{
    AppAccountInfo appAccountInfo;
    appAccountInfo.name_ = STRING_NAME;
    appAccountInfo.owner_ = STRING_OWNER;
    SetTimes(0, 1, "AddStringToJson");
    ErrCode ret = appAccountInfo.SetAccountCredential(STRING_CREDENTIAL_TYPE, STRING_CREDENTIAL);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_DUMP_JSON_ERROR);
    SetTimes(0, 0, "AddStringToJson");
}

/**
 * @tc.name: AppAccountInfo_ToJson_CreateJsonFail_001
 * @tc.desc: Test ToJson(AppAccountInfo) with CreateJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AppAccountInfoMockTest, AppAccountInfo_ToJson_CreateJsonFail_001, TestSize.Level4)
{
    SetCreateJsonFail(true);
    AppAccountInfo appAccountInfo;
    appAccountInfo.owner_ = STRING_OWNER;
    appAccountInfo.name_ = STRING_NAME;
    auto result = ToJson(appAccountInfo);
    EXPECT_EQ(result, nullptr);
    SetCreateJsonFail(false);
}
