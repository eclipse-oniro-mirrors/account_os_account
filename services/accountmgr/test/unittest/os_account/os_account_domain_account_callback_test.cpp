/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "account_log_wrapper.h"
#define private public
#include "os_account_domain_account_callback.h"
#include "os_account_info.h"
#undef private
#include "parcel.h"
using namespace testing;
using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;

namespace {
} // namespace

class DomainAccountCallbackTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void DomainAccountCallbackTest::SetUpTestCase(void)
{}

void DomainAccountCallbackTest::TearDownTestCase(void)
{}

void DomainAccountCallbackTest::SetUp(void)
{
}

void DomainAccountCallbackTest::TearDown(void)
{}

/**
 * @tc.name: DomainPluginStubModuleTest_OnResult_001
 * @tc.desc: OnResult with callbaci is nullptr.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DomainAccountCallbackTest, DomainAccountCallbackTest_OnResult_001, TestSize.Level0)
{
    DomainAccountInfo info;
    auto callbackPtr = std::make_shared<CheckAndCreateDomainAccountCallback>(OsAccountType::NORMAL, info, nullptr);
    Parcel parcel;
    callbackPtr->OnResult(0, parcel);
    EXPECT_EQ(callbackPtr->innerCallback_, nullptr);
}

/**
 * @tc.name: DomainPluginStubModuleTest_OnResult_002
 * @tc.desc: OnResult with callbaci is nullptr.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DomainAccountCallbackTest, DomainAccountCallbackTest_OnResult_002, TestSize.Level0)
{
    DomainAccountInfo info;
    OsAccountInfo osAccountInfo;
    auto callbackPtr = std::make_shared<BindDomainAccountCallback>(info, osAccountInfo, nullptr);
    Parcel parcel;
    callbackPtr->OnResult(0, parcel);
    EXPECT_EQ(callbackPtr->innerCallback_, nullptr);
}