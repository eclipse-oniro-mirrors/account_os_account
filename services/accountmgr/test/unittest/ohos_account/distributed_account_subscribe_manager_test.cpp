/*
 * Copyright (c) 2024-2026 Huawei Device Co., Ltd.
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
#include <set>
#include "account_log_wrapper.h"
#include "distributed_account_event_service.h"
#include "message_parcel.h"
#define private public
#include "distributed_account_subscribe_manager.h"
#include "distributed_account_subscribe_death_recipient.h"
#undef private

namespace OHOS {
namespace AccountSA {
using namespace testing;
using namespace testing::ext;

class MockDistributedAccountEventListener : public IRemoteStub<IDistributedAccountEvent> {
public:
    int OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override
    {
        return 0;
    }
    ErrCode OnAccountsChanged(const DistributedAccountEventData &eventData) override
    {
        return ERR_OK;
    }
};

class MockRemoteObject : public IRemoteObject {
public:
    MockRemoteObject() : IRemoteObject(u"MockRemoteObject") {}

    bool AddDeathRecipient(const sptr<DeathRecipient> &recipient) override
    {
        deathRecipient_ = recipient;
        return true;
    }

    bool RemoveDeathRecipient(const sptr<DeathRecipient> &recipient) override
    {
        deathRecipient_ = nullptr;
        return true;
    }

    int32_t GetObjectRefCount() override { return 0; }
    int SendRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override
    {
        return 0;
    }
    bool IsProxyObject() const override { return false; }
    bool IsObjectDead() const override { return isDead_; }
    int Dump(int fd, const std::vector<std::u16string> &args) override { return 0; }

    bool isDead_ = false;
    sptr<DeathRecipient> deathRecipient_;
};

class DistributedAccountSubscribeManagerTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void DistributedAccountSubscribeManagerTest::SetUpTestCase()
{}

void DistributedAccountSubscribeManagerTest::TearDownTestCase()
{}

void DistributedAccountSubscribeManagerTest::SetUp() __attribute__((no_sanitize("cfi")))
{
    testing::UnitTest *test = testing::UnitTest::GetInstance();
    ASSERT_NE(test, nullptr);
    const testing::TestInfo *testinfo = test->current_test_info();
    ASSERT_NE(testinfo, nullptr);
    std::string testCaseName = std::string(testinfo->name());
    ACCOUNT_LOGI("[SetUp] %{public}s start", testCaseName.c_str());
}

void DistributedAccountSubscribeManagerTest::TearDown()
{
    DistributedAccountSubscribeManager::GetInstance().subscribeRecords_.clear();
}

/**
 * @tc.name: Subscribe_UnsubscribeWithMultipleTypes_RemainingTypes_001
 * @tc.desc: Unsubscribe one type when subscriber has multiple types (line 100 TRUE branch).
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    Subscribe_UnsubscribeWithMultipleTypes_RemainingTypes_001, TestSize.Level1)
{
    auto listener = sptr<MockDistributedAccountEventListener>(new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener, nullptr);

    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    ErrCode subRet = manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener);
    EXPECT_EQ(subRet, ERR_OK);

    subRet = manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGOUT, listener);
    EXPECT_EQ(subRet, ERR_OK);

    ErrCode unsubRet = manager.UnsubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener);
    EXPECT_EQ(unsubRet, ERR_OK);
    unsubRet = manager.UnsubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGOUT, listener);
    EXPECT_EQ(unsubRet, ERR_OK);
}

/**
 * @tc.name: Publish_WithNullEventListener_0100
 * @tc.desc: Test Publish with a null eventListener_ record. Other valid subscribers
 *          should still receive events (break→continue fix).
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    Publish_WithNullEventListener_0100, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto validListener = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(validListener, nullptr);
    ASSERT_EQ(manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, validListener), ERR_OK);

    auto nullRecord = std::make_shared<DistributedSubscribeRecord>();
    nullRecord->eventListener_ = nullptr;
    nullRecord->types_.insert(DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN);
    manager.subscribeRecords_[0] = nullRecord;

    ErrCode publishRet = manager.Publish(100, DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN);
    EXPECT_EQ(publishRet, ERR_OK);

    manager.subscribeRecords_.clear();
}

/**
 * @tc.name: Subscribe_PerProcessDedup_DifferentListenerRejected_001
 * @tc.desc: Subscribing with a different eventListener from the same process
 *           (same calling PID) is rejected with ALREADY_REGISTERED error.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    Subscribe_PerProcessDedup_DifferentListenerRejected_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto listener1 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener1, nullptr);
    auto listener2 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener2, nullptr);

    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener1));
    EXPECT_EQ(manager.subscribeRecords_.size(), 1u);

    EXPECT_EQ(ERR_OHOSACCOUNT_KIT_CALLBACK_ALREADY_REGISTERED_ERROR,
        manager.SubscribeDistributedAccountEvent(
            DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener2));
    EXPECT_EQ(manager.subscribeRecords_.size(), 1u);
    EXPECT_EQ(manager.subscribeRecords_.begin()->second->eventListener_.GetRefPtr(), listener1.GetRefPtr());

    EXPECT_EQ(ERR_OK, manager.UnsubscribeDistributedAccountEvent(listener1));
    EXPECT_EQ(manager.subscribeRecords_.size(), 0u);
}

/**
 * @tc.name: Subscribe_PerProcessDedup_SameListenerMergeTypes_001
 * @tc.desc: Subscribing with the same eventListener from the same process
 *           merges types into the single per-process record.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    Subscribe_PerProcessDedup_SameListenerMergeTypes_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto listener1 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener1, nullptr);

    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener1));
    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGOUT, listener1));
    EXPECT_EQ(manager.subscribeRecords_.size(), 1u);
    EXPECT_EQ(manager.subscribeRecords_.begin()->second->types_.size(), 2u);

    EXPECT_EQ(ERR_OK, manager.UnsubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener1));
    EXPECT_EQ(ERR_OK, manager.UnsubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGOUT, listener1));
    EXPECT_EQ(manager.subscribeRecords_.size(), 0u);
}

/**
 * @tc.name: Subscribe_StaleRecordDeadListener_CleanupAndResubscribe_001
 * @tc.desc: When the existing record's eventListener is dead (IsObjectDead=true),
 *           the stale record is cleaned and the new subscription succeeds.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    Subscribe_StaleRecordDeadListener_CleanupAndResubscribe_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto oldListener = sptr<MockRemoteObject>(new (std::nothrow) MockRemoteObject());
    ASSERT_NE(oldListener, nullptr);
    auto newListener = sptr<MockRemoteObject>(new (std::nothrow) MockRemoteObject());
    ASSERT_NE(newListener, nullptr);

    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, oldListener));
    EXPECT_EQ(manager.subscribeRecords_.size(), 1u);

    oldListener->isDead_ = true;

    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, newListener));
    EXPECT_EQ(manager.subscribeRecords_.size(), 1u);
    EXPECT_EQ(manager.subscribeRecords_.begin()->second->eventListener_.GetRefPtr(), newListener.GetRefPtr());

    EXPECT_EQ(ERR_OK, manager.UnsubscribeDistributedAccountEvent(newListener));
    EXPECT_EQ(manager.subscribeRecords_.size(), 0u);
}

/**
 * @tc.name: Subscribe_StaleRecordNullListener_CleanupAndResubscribe_001
 * @tc.desc: When the existing record's eventListener is nullptr (partial cleanup),
 *           the stale record is cleaned and the new subscription succeeds.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    Subscribe_StaleRecordNullListener_CleanupAndResubscribe_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto listener1 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener1, nullptr);
    auto listener2 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener2, nullptr);

    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener1));
    ASSERT_EQ(manager.subscribeRecords_.size(), 1u);

    manager.subscribeRecords_.begin()->second->eventListener_ = nullptr;

    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener2));
    EXPECT_EQ(manager.subscribeRecords_.size(), 1u);
    EXPECT_EQ(manager.subscribeRecords_.begin()->second->eventListener_.GetRefPtr(), listener2.GetRefPtr());

    EXPECT_EQ(ERR_OK, manager.UnsubscribeDistributedAccountEvent(listener2));
    EXPECT_EQ(manager.subscribeRecords_.size(), 0u);
}

/**
 * @tc.name: Unsubscribe_Type_ListenerMismatch_NotFound_001
 * @tc.desc: Unsubscribing with a different eventListener from the same PID
 *           returns NOT_FOUND (PID matches but listener does not).
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    Unsubscribe_Type_ListenerMismatch_NotFound_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto listener1 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener1, nullptr);
    auto listener2 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener2, nullptr);

    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener1));

    EXPECT_EQ(ERR_OHOSACCOUNT_KIT_NO_SPECIFIED_CALLBACK_HAS_BEEN_REGISTERED,
        manager.UnsubscribeDistributedAccountEvent(
            DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener2));
    EXPECT_EQ(manager.subscribeRecords_.size(), 1u);

    EXPECT_EQ(ERR_OK, manager.UnsubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener1));
    EXPECT_EQ(manager.subscribeRecords_.size(), 0u);
}

/**
 * @tc.name: Publish_MultiPid_IndependentDispatch_001
 * @tc.desc: Multiple PIDs coexist in the map; Publish dispatches to all
 *           without removing or affecting other PID records.
 *           (multi-PID unsubscribe isolation is covered by
 *           DeathRecipient_OnRemoteDied_ValidRemote_001)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    Publish_MultiPid_IndependentDispatch_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto listener1 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener1, nullptr);
    auto listener2 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener2, nullptr);

    auto record1 = std::make_shared<DistributedSubscribeRecord>(listener1);
    record1->types_.insert(DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN);
    auto record2 = std::make_shared<DistributedSubscribeRecord>(listener2);
    record2->types_.insert(DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN);
    manager.subscribeRecords_[100] = record1;
    manager.subscribeRecords_[200] = record2;
    ASSERT_EQ(manager.subscribeRecords_.size(), 2u);

    EXPECT_EQ(ERR_OK, manager.Publish(100, DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN));
    EXPECT_EQ(manager.subscribeRecords_.size(), 2u);

    manager.subscribeRecords_.clear();
}

// ======================== DeathRecipient Tests ========================

/**
 * @tc.name: DeathRecipient_OnRemoteDied_NullRemote_001
 * @tc.desc: 验证传入空 remote 死亡通知不崩溃，订阅记录保持不变
 *           (covers null-check early-return branch in OnRemoteDied)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    DeathRecipient_OnRemoteDied_NullRemote_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto listener = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener, nullptr);
    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(
        DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, listener));
    ASSERT_EQ(manager.subscribeRecords_.size(), 1u);

    DistributedAccountSubscribeDeathRecipient recipient;
    wptr<IRemoteObject> nullRemote;
    recipient.OnRemoteDied(nullRemote);

    EXPECT_EQ(manager.subscribeRecords_.begin()->second->eventListener_.GetRefPtr(),
        listener.GetRefPtr());

    manager.subscribeRecords_.clear();
}

/**
 * @tc.name: DeathRecipient_OnRemoteDied_PromoteFail_001
 * @tc.desc: 验证 promote 失败（对象已销毁）时死亡通知不崩溃
 *           (covers promote-failure early-return branch in OnRemoteDied)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    DeathRecipient_OnRemoteDied_PromoteFail_001, TestSize.Level1)
{
    sptr<MockRemoteObject> tempObj = new (std::nothrow) MockRemoteObject();
    ASSERT_NE(tempObj, nullptr);
    wptr<IRemoteObject> dangling = tempObj;
    tempObj = nullptr;
    ASSERT_EQ(dangling.promote(), nullptr);

    DistributedAccountSubscribeDeathRecipient recipient;
    recipient.OnRemoteDied(dangling);
}

/**
 * @tc.name: DeathRecipient_OnRemoteDied_ValidRemote_001
 * @tc.desc: 验证有效 remote 死亡通知后对应订阅记录被清理，不影响其他 PID 记录
 *           (covers normal path: promote succeeds → UnsubscribeDistributedAccountEvent)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DistributedAccountSubscribeManagerTest,
    DeathRecipient_OnRemoteDied_ValidRemote_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto listener1 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener1, nullptr);
    auto listener2 = sptr<MockDistributedAccountEventListener>(
        new (std::nothrow) MockDistributedAccountEventListener());
    ASSERT_NE(listener2, nullptr);

    auto record1 = std::make_shared<DistributedSubscribeRecord>(listener1);
    record1->types_.insert(DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN);
    auto record2 = std::make_shared<DistributedSubscribeRecord>(listener2);
    record2->types_.insert(DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN);
    manager.subscribeRecords_[100] = record1;
    manager.subscribeRecords_[200] = record2;
    ASSERT_EQ(manager.subscribeRecords_.size(), 2u);

    DistributedAccountSubscribeDeathRecipient recipient;
    wptr<IRemoteObject> remote(listener1);
    recipient.OnRemoteDied(remote);

    EXPECT_EQ(manager.subscribeRecords_.size(), 1u);
    EXPECT_EQ(manager.subscribeRecords_.begin()->second->eventListener_.GetRefPtr(),
        listener2.GetRefPtr());

    manager.subscribeRecords_.clear();
}
}
}
