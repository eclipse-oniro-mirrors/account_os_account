/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
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

#include <cerrno>
#include <gtest/gtest.h>
#include <thread>
#include "accesstoken_kit.h"
#include "account_log_wrapper.h"
#include "account_test_common.h"
#define private public
#include "account_iam_service.h"
#undef private
#include "token_setproc.h"
#include "parcel.h"
#include "want.h"
using namespace testing;
using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;

namespace {
DECLARE_INTERFACE_DESCRIPTOR(u"ohos.accountfwk.IAccountIAM");
} // namespace

class AccountIAMStubModuleTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
    sptr<AccountIAMService> service_ = nullptr;
};

void AccountIAMStubModuleTest::SetUpTestCase(void)
{
    ASSERT_TRUE(MockTokenId("accountmgr"));
}

void AccountIAMStubModuleTest::TearDownTestCase(void)
{}

void AccountIAMStubModuleTest::SetUp(void) __attribute__((no_sanitize("cfi")))
{
    testing::UnitTest *test = testing::UnitTest::GetInstance();
    ASSERT_NE(test, nullptr);
    const testing::TestInfo *testinfo = test->current_test_info();
    ASSERT_NE(testinfo, nullptr);
    string testCaseName = string(testinfo->name());
    ACCOUNT_LOGI("[SetUp] %{public}s start", testCaseName.c_str());

    service_ = new (std::nothrow) AccountIAMService();
    ASSERT_NE(service_, nullptr);
}

void AccountIAMStubModuleTest::TearDown(void)
{}

/**
 * @tc.name: AccountIAMStubModuleTest_OnRemoteRequest_001
 * @tc.desc: OnRemoteRequest with invalid code.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AccountIAMStubModuleTest, AccountIAMStubModuleTest_OnRemoteRequest_001, TestSize.Level3)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    data.WriteInterfaceToken(GetDescriptor());
    EXPECT_NE(service_->OnRemoteRequest(-1, data, reply, option), ERR_NONE);
}

/**
 * @tc.name: AccountIAMStubModuleTest_OnRemoteRequest_002
 * @tc.desc: OnRemoteRequest with not InterfaceToken.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AccountIAMStubModuleTest, AccountIAMStubModuleTest_OnRemoteRequest_002, TestSize.Level3)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    EXPECT_NE(service_->OnRemoteRequest(-1, data, reply, option), ERR_NONE);
}

/**
 * @tc.name: AccountIAMStubModuleTest_OnRemoteRequest_003
 * @tc.desc: OnRemoteRequest with invalid code.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AccountIAMStubModuleTest, AccountIAMStubModuleTest_OnRemoteRequest_003, TestSize.Level3)
{
    for (uint32_t code = static_cast<uint32_t>(IAccountIAMIpcCode::COMMAND_OPEN_SESSION);
        code <= static_cast<uint32_t>(IAccountIAMIpcCode::COMMAND_GET_ACCOUNT_STATE); code++) {
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;
        data.WriteInterfaceToken(GetDescriptor());

        auto ret = service_->OnRemoteRequest(static_cast<uint32_t>(static_cast<uint32_t>(code)), data, reply, option);
        if (code == static_cast<uint32_t>(IAccountIAMIpcCode::COMMAND_OPEN_SESSION) ||
            code == static_cast<uint32_t>(IAccountIAMIpcCode::COMMAND_CLOSE_SESSION) ||
            code == static_cast<uint32_t>(IAccountIAMIpcCode::COMMAND_CANCEL) ||
            code == static_cast<uint32_t>(IAccountIAMIpcCode::COMMAND_CANCEL_AUTH) ||
            code == static_cast<uint32_t>(IAccountIAMIpcCode::COMMAND_GET_AVAILABLE_STATUS) ||
            code == static_cast<uint32_t>(IAccountIAMIpcCode::COMMAND_GET_ACCOUNT_STATE)) {
            EXPECT_EQ(ret, ERR_NONE);
        } else {
            EXPECT_NE(ret, ERR_NONE);
        }
    }
}