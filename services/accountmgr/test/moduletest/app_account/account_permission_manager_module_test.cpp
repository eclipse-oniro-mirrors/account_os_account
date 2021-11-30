/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "account_error_no.h"
#include "account_permission_manager.h"

using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;

namespace {
const std::string STRING_OWNER = "com.example.owner";

constexpr std::int32_t UID = 10000;
}  // namespace

class AccountPermissionManagerModuleTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void) override;
    void TearDown(void) override;

    std::shared_ptr<AccountPermissionManager> permissionManagerPtr_;
};

void AccountPermissionManagerModuleTest::SetUpTestCase(void)
{}

void AccountPermissionManagerModuleTest::TearDownTestCase(void)
{}

void AccountPermissionManagerModuleTest::SetUp(void)
{
    permissionManagerPtr_ = DelayedSingleton<AccountPermissionManager>::GetInstance();
}

void AccountPermissionManagerModuleTest::TearDown(void)
{}

/**
 * @tc.number: AccountPermissionManager_VerifyPermission_0100
 * @tc.name: VerifyPermission
 * @tc.desc: Verify permission with valid data.
 */
HWTEST_F(
    AccountPermissionManagerModuleTest, AccountPermissionManager_VerifyPermission_0100, Function | MediumTest | Level1)
{
    ASSERT_NE(permissionManagerPtr_, nullptr);

    ErrCode result =
        permissionManagerPtr_->VerifyPermission(UID, AccountPermissionManager::DISTRIBUTED_DATASYNC, STRING_OWNER);
    EXPECT_EQ(result, ERR_APPACCOUNT_SERVICE_PERMISSION_DENIED);
}
