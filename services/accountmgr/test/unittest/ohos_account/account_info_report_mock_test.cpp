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
#include "account_info_report.h"
#include "json_utils.h"
#include "mock_json_util.h"

using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;
namespace OHOS {
namespace AccountSA {
std::string TransformIntoJson(const std::string &user, int32_t id, ReportEvent event, int32_t result);
std::string TransformIntoAccountOperationJson(
    const AccountOperationInfo &accountOperationInfo, AccountOperationType operationType);
} // namespace AccountSA
} // namespace OHOS
class AccountInfoReportCreateJsonMockTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void);
    void TearDown(void);
};

void AccountInfoReportCreateJsonMockTest::SetUpTestCase(void) {}
void AccountInfoReportCreateJsonMockTest::TearDownTestCase(void) {}
void AccountInfoReportCreateJsonMockTest::SetUp(void) {}
void AccountInfoReportCreateJsonMockTest::TearDown(void) {}

/**
 * @tc.name: AccountInfoReport_TransformIntoJson_CreateJsonFail_001
 * @tc.desc: Test TransformIntoJson with CreateJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AccountInfoReportCreateJsonMockTest, AccountInfoReport_TransformIntoJson_CreateJsonFail_001, TestSize.Level4)
{
    SetCreateJsonFail(true);
    std::string result = TransformIntoJson("user", 100, ReportEvent::EVENT_LOGIN, 0);
    EXPECT_TRUE(result.empty());
    SetCreateJsonFail(false);
}

/**
 * @tc.name: AccountInfoReport_TransformIntoAccountOperationJson_CreateJsonFail_001
 * @tc.desc: Test TransformIntoAccountOperationJson with CreateJson failure.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AccountInfoReportCreateJsonMockTest,
    AccountInfoReport_TransformIntoAccountOperationJson_CreateJsonFail_001, TestSize.Level4)
{
    SetCreateJsonFail(true);
    AccountOperationInfo opInfo;
    opInfo.pid = 100;
    opInfo.uid = 100;
    opInfo.sourceUserName = "src";
    opInfo.sourceUserId = 100;
    opInfo.targetUserName = "tgt";
    opInfo.targetUserId = 101;
    std::string result = TransformIntoAccountOperationJson(opInfo, ACCOUNT_OPERATION_TYPE_CREATE);
    EXPECT_TRUE(result.empty());
    SetCreateJsonFail(false);
}
