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
#include "account_info.h"
#include "json_utils.h"
#define private public
#include "ohos_account_data_deal.h"
#undef private
#include "mock_json_util.h"

using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;

namespace {
const std::string RESOURCE_ROOT_PATH = "/data/test/account/data_deal/";
const int32_t TEST_USER_ID = 100;
}

class OhosAccountDataDealCreateJsonMockTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void);
    void TearDown(void);
};

void OhosAccountDataDealCreateJsonMockTest::SetUpTestCase(void) {}
void OhosAccountDataDealCreateJsonMockTest::TearDownTestCase(void) {}
void OhosAccountDataDealCreateJsonMockTest::SetUp(void) {}
void OhosAccountDataDealCreateJsonMockTest::TearDown(void) {}

/**
 * @tc.name: OhosAccountDataDeal_SaveAccountInfo_CreateJsonFail_001
 * @tc.desc: Test SaveAccountInfo with CreateJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealCreateJsonMockTest, OhosAccountDataDeal_SaveAccountInfo_CreateJsonFail_001, TestSize.Level4)
{
    SetCreateJsonFail(true);
    AccountInfo accountInfo;
    accountInfo.userId_ = TEST_USER_ID;
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
    ErrCode ret = dataDeal.SaveAccountInfo(accountInfo);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_DUMP_JSON_ERROR);
    SetCreateJsonFail(false);
}
