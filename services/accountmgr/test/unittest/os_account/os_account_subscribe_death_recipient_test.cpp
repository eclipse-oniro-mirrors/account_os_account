/*
 * Copyright (c) 2022-2025 Huawei Device Co., Ltd.
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
#include <map>
#include <thread>
#include "account_error_no.h"
#include "account_log_wrapper.h"
#include "os_account_constants.h"
#define private public
#include "os_account_constraint_subscribe_manager.h"
#undef private
#include "os_account_constraint_manager.h"
#include "os_account_constraint_subscribe_death_recipient.h"
#include "os_account_delete_user_idm_callback.h"
#include "os_account_event_proxy.h"
#include "os_account_subscribe_death_recipient.h"
#define private public
#include "os_account_subscribe_manager.h"
#include "ability_manager_adapter.h"
#undef private
#include "os_account_interface.h"
#include "mock_account_mgr_service.h"
#include "os_account_info.h"

namespace OHOS {
namespace AccountSA {
using namespace testing::ext;
using namespace OHOS::AccountSA;
using namespace OHOS;
using namespace AccountSA;
const int32_t SLEEP_TIME = 100;
const std::string TEST_STR = "test";
const std::string TEST_STR2 = "test2";
class OsAccountCoverageTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};


void OsAccountCoverageTest::SetUpTestCase(void)
{}

void OsAccountCoverageTest::TearDownTestCase(void)
{}

void OsAccountCoverageTest::SetUp(void) __attribute__((no_sanitize("cfi")))
{
    testing::UnitTest *test = testing::UnitTest::GetInstance();
    ASSERT_NE(test, nullptr);
    const testing::TestInfo *testinfo = test->current_test_info();
    ASSERT_NE(testinfo, nullptr);
    string testCaseName = string(testinfo->name());
    ACCOUNT_LOGI("[SetUp] %{public}s start", testCaseName.c_str());
}

void OsAccountCoverageTest::TearDown(void)
{}

class DelayEventProxy : public OsAccountEventProxy {
public:
    DelayEventProxy() :OsAccountEventProxy(nullptr) {}
    ErrCode OnAccountsSwitch(int newId, int oldId) override
    {
        // mock operation
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
        return ERR_OK;
    }

    ErrCode OnAccountsChanged(int32_t id)
    {
        return ERR_OK;
    }
};

/*
 * @tc.name: DestructorCrashWhenThreadRunning_001
 * @tc.desc: test if SwitchSubscribeInfo crash when destructor.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, DestructorCrashWhenThreadRunning_001, TestSize.Level1)
{
    auto obj = std::make_shared<SwitchSubscribeInfo>();
    auto proxy = sptr<IOsAccountEvent>(new DelayEventProxy());
    ASSERT_NE(proxy, nullptr);

    OsAccountStateParcel stateParcel;
    stateParcel.state = OsAccountState::ACTIVATED;
    for (int i = 0; i < 10; ++i) {
        stateParcel.fromId = i - 1;
        stateParcel.toId = i;
        EXPECT_TRUE(obj->ProductTask(proxy, stateParcel));
    }
    obj.reset();
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

/*
 * @tc.name: UseAfterFreeScenario_001
 * @tc.desc: test if SwitchSubscribeInfo use after free.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, UseAfterFreeScenario_001, TestSize.Level1)
{
    auto obj = std::make_shared<SwitchSubscribeInfo>();
    auto proxy = sptr<IOsAccountEvent>(new DelayEventProxy());
    ASSERT_NE(proxy, nullptr);

    OsAccountStateParcel stateParcel;
    stateParcel.state = OsAccountState::ACTIVATED;
    stateParcel.fromId = 0;
    stateParcel.toId = 1;
    EXPECT_TRUE(obj->ProductTask(proxy, stateParcel));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    obj.reset();

    // customer is working
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

/*
 * @tc.name: HighFrequencyLifecycle_001
 * @tc.desc: test if SwitchSubscribeInfo high frequency lifecycle.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, HighFrequencyLifecycle_001, TestSize.Level1)
{
    OsAccountStateParcel stateParcel;
    stateParcel.state = OsAccountState::ACTIVATED;
    stateParcel.fromId = 0;
    stateParcel.toId = 1;
    for (int i = 0; i < 1000; ++i) {
        auto obj = std::make_shared<SwitchSubscribeInfo>();
        EXPECT_TRUE(obj->ProductTask(sptr<IOsAccountEvent>(new DelayEventProxy()), stateParcel));
        obj.reset();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

/*
 * @tc.name: OnRemoteDiedTest_0100
 * @tc.desc: test if OsAccountSubscribeDeathRecipient's OnRemoteDied function executed as expected in normal case.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, OnRemoteDiedTest_0100, TestSize.Level1)
{
    GTEST_LOG_(INFO)
        << "OsAccountCoverageTest, OnRemoteDiedTest_0100, TestSize.Level1";
    OsAccountSubscribeInfo subscribeInfo;
    sptr<MockAccountMgrService> listener = new (std::nothrow) MockAccountMgrService();
    ASSERT_NE(nullptr, listener);

    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(subscribeInfo);

    OsAccountSubscribeManager::GetInstance().SubscribeOsAccount(subscribeInfoPtr, listener);

    int size = OsAccountSubscribeManager::GetInstance().subscribeRecords_.size();
    EXPECT_EQ(size, 1);

    std::shared_ptr<OsAccountSubscribeDeathRecipient> recipient = std::make_shared<OsAccountSubscribeDeathRecipient>();
    ASSERT_NE(nullptr, recipient);
    recipient->OnRemoteDied(listener);
    size = OsAccountSubscribeManager::GetInstance().subscribeRecords_.size();
    EXPECT_EQ(size, 0);
}

/*
 * @tc.name: OnRemoteDiedTest_0200
 * @tc.desc: test if OsAccountSubscribeDeathRecipient's OnRemoteDied function executed
 * as expected when param is nullptr.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, OnRemoteDiedTest_0200, TestSize.Level1)
{
    GTEST_LOG_(INFO)
        << "OsAccountCoverageTest, OnRemoteDiedTest_0200, TestSize.Level1";
    std::shared_ptr<OsAccountSubscribeDeathRecipient> recipient = std::make_shared<OsAccountSubscribeDeathRecipient>();
    ASSERT_NE(nullptr, recipient);
    wptr<IRemoteObject> wptrDeath = nullptr;
    recipient->OnRemoteDied(wptrDeath);
    int size = OsAccountSubscribeManager::GetInstance().subscribeRecords_.size();
    EXPECT_EQ(size, 0);
}

/*
 * @tc.name: OnRemoteDiedTest_0200
 * @tc.desc: test if OsAccountSubscribeDeathRecipient's OnRemoteDied function executed as expected when
 *           sptr param is nullptr.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, OnRemoteDiedTest_0300, TestSize.Level1)
{
    GTEST_LOG_(INFO)
        << "OsAccountCoverageTest, OnRemoteDiedTest_0300, TestSize.Level1";
    std::shared_ptr<OsAccountSubscribeDeathRecipient> recipient = std::make_shared<OsAccountSubscribeDeathRecipient>();
    ASSERT_NE(nullptr, recipient);
    const sptr<IRemoteObject> sptrDeath = nullptr;
    wptr<IRemoteObject> wptrDeath = sptrDeath;
    recipient->OnRemoteDied(wptrDeath);
    int size = OsAccountSubscribeManager::GetInstance().subscribeRecords_.size();
    EXPECT_EQ(size, 0);
}

/*
 * @tc.name: OnRemoteDiedTest_0400
 * @tc.desc: test if AbilityMgrDeathRecipient's OnRemoteDied function executed as expected when
 *           sptr param is nullptr.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, OnRemoteDiedTest_0400, TestSize.Level1)
{
    std::shared_ptr<AbilityManagerAdapter::AbilityMgrDeathRecipient> recipient =
        std::make_shared<AbilityManagerAdapter::AbilityMgrDeathRecipient>();
    ASSERT_NE(nullptr, recipient);
    const sptr<IRemoteObject> sptrDeath = nullptr;
    wptr<IRemoteObject> wptrDeath = sptrDeath;
    recipient->OnRemoteDied(wptrDeath);
    EXPECT_EQ(DelayedSingleton<AbilityManagerAdapter>::GetInstance()->deathRecipient_, nullptr);
}

/**
 * @tc.name: SubscribeOsAccount_0001
 * @tc.desc: Test SubscribeOsAccount with nullptr
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, SubscribeOsAccount_0001, TestSize.Level1)
{
    OsAccountSubscribeInfo subscribeInfo;
    sptr<MockAccountMgrService> listener = new (std::nothrow) MockAccountMgrService();
    ASSERT_NE(nullptr, listener);

    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(subscribeInfo);
    ASSERT_NE(nullptr, subscribeInfoPtr);

    ErrCode result = OsAccountSubscribeManager::GetInstance().SubscribeOsAccount(nullptr, listener);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_NULL_PTR_ERROR);

    result = OsAccountSubscribeManager::GetInstance().SubscribeOsAccount(subscribeInfoPtr, nullptr);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_NULL_PTR_ERROR);
}

/**
 * @tc.name: UnsubscribeOsAccount_0001
 * @tc.desc: Test UnsubscribeOsAccount with nullptr
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, UnsubscribeOsAccount_0001, TestSize.Level1)
{
    ErrCode result = OsAccountSubscribeManager::GetInstance().UnsubscribeOsAccount(nullptr);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_NULL_PTR_ERROR);
}

/**
 * @tc.name: RemoveSubscribeRecord_0001
 * @tc.desc: Test RemoveSubscribeRecord with nullptr
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, RemoveSubscribeRecord_0001, TestSize.Level1)
{
    ErrCode result = OsAccountSubscribeManager::GetInstance().RemoveSubscribeRecord(nullptr);
    EXPECT_EQ(result, ERR_OK);
}

/*
 * @tc.name: OnRemoteDiedTest_0100
 * @tc.desc: test if OsAccountConstraintSubscribeDeathRecipient's OnRemoteDied
 *           function executed as expected in normal case.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, OnRemoteDiedTest_0500, TestSize.Level1)
{
    GTEST_LOG_(INFO)
        << "OsAccountCoverageTest, OnRemoteDiedTest_0500, TestSize.Level1";
    OsAccountSubscribeInfo subscribeInfo;
    sptr<MockAccountMgrService> listener = new (std::nothrow) MockAccountMgrService();
    sptr<MockAccountMgrService> listener2 = new (std::nothrow) MockAccountMgrService();
    ASSERT_NE(nullptr, listener);
    std::set<std::string> constraints = {TEST_STR};
    OsAccountConstraintSubscribeManager::GetInstance().SubscribeOsAccountConstraints(constraints, listener);

    int size = OsAccountConstraintSubscribeManager::GetInstance().constraintRecords_.size();
    EXPECT_EQ(size, 1);

    std::shared_ptr<OsAccountConstraintSubscribeDeathRecipient> recipient =
        std::make_shared<OsAccountConstraintSubscribeDeathRecipient>();
    ASSERT_NE(nullptr, recipient);
    recipient->OnRemoteDied(listener2);
    size = OsAccountConstraintSubscribeManager::GetInstance().constraintRecords_.size();
    EXPECT_EQ(size, 1);
    recipient->OnRemoteDied(listener);
    size = OsAccountConstraintSubscribeManager::GetInstance().constraintRecords_.size();
    EXPECT_EQ(size, 0);
}

/*
 * @tc.name: OnRemoteDiedTest_0600
 * @tc.desc: test if OsAccountConstraintSubscribeDeathRecipient's OnRemoteDied function executed
 * as expected when param is nullptr.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, OnRemoteDiedTest_0600, TestSize.Level1)
{
    GTEST_LOG_(INFO)
        << "OsAccountCoverageTest, OnRemoteDiedTest_0600, TestSize.Level1";
    std::shared_ptr<OsAccountConstraintSubscribeDeathRecipient> recipient =
        std::make_shared<OsAccountConstraintSubscribeDeathRecipient>();
    ASSERT_NE(nullptr, recipient);
    wptr<IRemoteObject> wptrDeath = nullptr;
    recipient->OnRemoteDied(wptrDeath);
    int size = OsAccountSubscribeManager::GetInstance().subscribeRecords_.size();
    EXPECT_EQ(size, 0);
}

/*
 * @tc.name: OnRemoteDiedTest_0700
 * @tc.desc: test if OsAccountConstraintSubscribeDeathRecipient's OnRemoteDied function executed as expected when
 *           sptr param is nullptr.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, OnRemoteDiedTest_0700, TestSize.Level1)
{
    GTEST_LOG_(INFO)
        << "OsAccountCoverageTest, OnRemoteDiedTest_0700, TestSize.Level1";
    std::shared_ptr<OsAccountConstraintSubscribeDeathRecipient> recipient =
        std::make_shared<OsAccountConstraintSubscribeDeathRecipient>();
    ASSERT_NE(nullptr, recipient);
    const sptr<IRemoteObject> sptrDeath = nullptr;
    wptr<IRemoteObject> wptrDeath = sptrDeath;
    recipient->OnRemoteDied(wptrDeath);
    int size = OsAccountSubscribeManager::GetInstance().subscribeRecords_.size();
    EXPECT_EQ(size, 0);
}

/*
 * @tc.name: OsAccountSubscribeManager_001
 * @tc.desc: Test OnStateChangedV0 state.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, OsAccountSubscribeManager_001, TestSize.Level3)
{
    GTEST_LOG_(INFO)
        << "OsAccountCoverageTest, OsAccountSubscribeManager_001, TestSize.Level1";
    auto proxy = sptr<IOsAccountEvent>(new DelayEventProxy());
    int32_t testUid = 1;
    int32_t fromId = 100;
    int32_t toId = 101;
    ASSERT_NE(proxy, nullptr);
    bool result = OsAccountSubscribeManager::GetInstance().OnStateChangedV0(
        proxy, OsAccountState::SWITCHING, fromId, toId, testUid);
    EXPECT_EQ(result, false);
    result = OsAccountSubscribeManager::GetInstance().OnStateChangedV0(
        proxy, OsAccountState::SWITCHED, fromId, toId, testUid);
    EXPECT_EQ(result, false);
    auto obj = std::make_shared<SwitchSubscribeInfo>();
    OsAccountSubscribeManager::GetInstance().switchRecordMap_.emplace(testUid, obj);
    result = OsAccountSubscribeManager::GetInstance().OnStateChangedV0(
        proxy, OsAccountState::SWITCHED, fromId, toId, testUid);
    EXPECT_EQ(result, true);
    result = OsAccountSubscribeManager::GetInstance().OnStateChangedV0(
        proxy, OsAccountState::ACTIVATED, fromId, toId, testUid);
    EXPECT_EQ(result, true);
}

/**
 * @tc.name: SubscribeConstraints_0001
 * @tc.desc: Test SubscribeOsAccountConstraints with nullptr
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, SubscribeConstraints_0001, TestSize.Level1)
{
    std::set<std::string> constraints;
    ErrCode result =
        OsAccountConstraintSubscribeManager::GetInstance().SubscribeOsAccountConstraints(constraints, nullptr);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_NULL_PTR_ERROR);
    result = OsAccountConstraintSubscribeManager::GetInstance().UnsubscribeOsAccountConstraints(constraints, nullptr);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_NULL_PTR_ERROR);
    result = OsAccountConstraintSubscribeManager::GetInstance().UnsubscribeOsAccountConstraints(nullptr);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_NULL_PTR_ERROR);
}

/**
 * @tc.name: SubscribeConstraints_002
 * @tc.desc: Test SubscribeOsAccountConstraints success and ERR_ACCOUNT_COMMON_ACCOUNT_SUBSCRIBE_NOT_FOUND_ERROR
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, SubscribeConstraints_002, TestSize.Level1)
{
    sptr<MockAccountMgrService> listener = new (std::nothrow) MockAccountMgrService();
    ASSERT_NE(nullptr, listener);
    std::set<std::string> constraints = {TEST_STR};
    OsAccountConstraintSubscribeRecordPtr recordPtr =
        std::make_shared<OsAccountConstraintSubscribeRecord>(constraints, nullptr, 0);
    OsAccountConstraintSubscribeManager::GetInstance().PublishToSubscriber(recordPtr, 100, constraints, true);
    ErrCode result =
        OsAccountConstraintSubscribeManager::GetInstance().SubscribeOsAccountConstraints(constraints, listener);
    EXPECT_EQ(result, ERR_OK);
    constraints.emplace(TEST_STR2);
    result = OsAccountConstraintSubscribeManager::GetInstance().SubscribeOsAccountConstraints(constraints, listener);
    EXPECT_EQ(result, ERR_OK);
    sptr<MockAccountMgrService> listener2 = new (std::nothrow) MockAccountMgrService();
    ASSERT_NE(nullptr, listener2);
    result = OsAccountConstraintSubscribeManager::GetInstance().SubscribeOsAccountConstraints(constraints, listener2);
    EXPECT_EQ(result, ERR_OK);
    constraints.erase(TEST_STR2);
    result = OsAccountConstraintSubscribeManager::GetInstance().UnsubscribeOsAccountConstraints(constraints, listener2);
    EXPECT_EQ(result, ERR_OK);
    result = OsAccountConstraintSubscribeManager::GetInstance().UnsubscribeOsAccountConstraints(constraints, listener);
    EXPECT_EQ(result, ERR_OK);
    result = OsAccountConstraintSubscribeManager::GetInstance().UnsubscribeOsAccountConstraints(constraints, listener);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_ACCOUNT_SUBSCRIBE_NOT_FOUND_ERROR);
    result = OsAccountConstraintSubscribeManager::GetInstance().UnsubscribeOsAccountConstraints(listener);
    EXPECT_EQ(result, ERR_OK);
    result = OsAccountConstraintSubscribeManager::GetInstance().UnsubscribeOsAccountConstraints(listener);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_ACCOUNT_SUBSCRIBE_NOT_FOUND_ERROR);
}

/**
 * @tc.name: SubscribeConstraints_002
 * @tc.desc: Test SubscribeOsAccountConstraints ERR_ACCOUNT_COMMON_INVALID_PARAMETER
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountCoverageTest, SubscribeConstraints_003, TestSize.Level1)
{
    std::set<std::string> constraints;
    OsAccountConstraintSubscribeInfo info(constraints);
    constraints = {TEST_STR};
    sptr<MockAccountMgrService> listener = new (std::nothrow) MockAccountMgrService();
    ASSERT_NE(nullptr, listener);
    ErrCode result = OsAccountConstraintManager::GetInstance().SubscribeOsAccountConstraints(info, listener);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    result = OsAccountConstraintManager::GetInstance().UnsubscribeOsAccountConstraints(info, listener);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    OsAccountConstraintSubscribeInfo info2(constraints);
    result = OsAccountConstraintManager::GetInstance().UnsubscribeOsAccountConstraints(info2, listener);
    EXPECT_EQ(result, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
}
}  // namespace AccountSA
}  // namespace OHOS
