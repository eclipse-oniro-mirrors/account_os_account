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

#include <chrono>
#include <condition_variable>
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <set>
#include <unistd.h>
#include <vector>
#include "account_error_no.h"
#include "account_log_wrapper.h"
#include "os_account_event_stub.h"
#include "os_account_state_parcel.h"
#include "os_account_state_reply_callback_service.h"
#include "os_account_subscribe_info.h"
#include "os_account_subscribe_manager.h"
// The handshake-wait tests below seed the static subscriber manager's per-state timeout
// table, which has no public setter; reuse the repo's established private-access pattern
// (see os_account_static_subscriber_manager_test.cpp). No header above includes it, so
// the guard sees the private-opened copy only here.
#define private public
#include "os_account_static_subscriber_manager.h"
#undef private

namespace OHOS {
namespace AccountSA {
using namespace testing;
using namespace testing::ext;

namespace {
constexpr int32_t TEST_ACCOUNT_ID = 100;
constexpr int32_t TEST_TO_ID = 101;
constexpr int32_t TEST_FIFO_FIRST_TO_ID = 111;
constexpr int32_t TEST_FIFO_SECOND_TO_ID = 222;
const int32_t TEST_SUBSCRIBER_UID = static_cast<int32_t>(getuid());
constexpr int32_t TEST_WAIT_SECONDS = 5;
constexpr int32_t TEST_PENDING_COUNT = 1;
// Switching wait used by the handshake-wait tests: long enough to observe that a wait
// actually happened, short enough to keep the suite fast.
constexpr int32_t TEST_CONFIGURED_WAIT_MS = 300;
// A legal static subscriber replies after this delay (within its configured budget).
constexpr int32_t TEST_SLOW_REPLY_MS = 200;
// The caller's own work overlap (SendToStorage/AMS stand-in).
constexpr int32_t TEST_CALLER_WORK_MS = 100;
// usleep takes microseconds; the test delays are authored in milliseconds.
constexpr int32_t MS_TO_US = 1000;

class MockOsAccountEvent : public OsAccountEventStub {
public:
    MOCK_METHOD(ErrCode, OnStateChanged, (const OsAccountStateParcel &parcel), (override));
    MOCK_METHOD(ErrCode, OnAccountsChanged, (int32_t id), (override));
    MOCK_METHOD(ErrCode, OnAccountsSwitch, (int32_t newId, int32_t oldId), (override));
    MOCK_METHOD(int32_t, CallbackEnter, (uint32_t code), (override));
    MOCK_METHOD(int32_t, CallbackExit, (uint32_t code, int32_t result), (override));
};

sptr<OsAccountStateReplyCallbackService> CreateReplyCallback(
    const std::shared_ptr<std::condition_variable> &cvPtr, const std::shared_ptr<SafeQueue<uint8_t>> &safeQueue)
{
    return new (std::nothrow) OsAccountStateReplyCallbackService(
        TEST_ACCOUNT_ID, OsAccountState::SWITCHING, cvPtr, safeQueue, TEST_SUBSCRIBER_UID);
}

OsAccountStateParcel BuildStateParcel(const sptr<IRemoteObject> &callback)
{
    OsAccountStateParcel parcel;
    parcel.state = OsAccountState::SWITCHING;
    parcel.fromId = TEST_ACCOUNT_ID;
    parcel.toId = TEST_TO_ID;
    parcel.callback = callback;
    return parcel;
}

// Stand-in for a legal slow static subscriber: replies after TEST_SLOW_REPLY_MS, within its
// configured budget (TEST_CONFIGURED_WAIT_MS).
int32_t SlowReplyOnStateChanged(const COsAccountStateData *stateData)
{
    usleep(TEST_SLOW_REPLY_MS * MS_TO_US);
    return 0;
}
}  // namespace

class OsAccountSubscribeManagerTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void);
    void TearDown(void);
};

void OsAccountSubscribeManagerTest::SetUp(void) __attribute__((no_sanitize("cfi")))
{
    testing::UnitTest *test = testing::UnitTest::GetInstance();
    ASSERT_NE(test, nullptr);
    const testing::TestInfo *testinfo = test->current_test_info();
    ASSERT_NE(testinfo, nullptr);
    std::string testCaseName = std::string(testinfo->name());
    ACCOUNT_LOGI("[SetUp] %{public}s start", testCaseName.c_str());
}

void OsAccountSubscribeManagerTest::TearDown(void) __attribute__((no_sanitize("cfi")))
{
    // stateTimeouts_ is singleton state shared by the whole suite; clear it so a
    // handshake-wait case cannot leak a configured timeout into later tests.
    OsAccountStaticSubscriberManager::GetInstance().stateTimeouts_.clear();
}

/**
 * @tc.name: SwitchSubscribeInfo_ProductTask_SendFailed
 * @tc.desc: When OnStateChanged send fails, HandleSwitchReplyCallback force-completes the reply callback and
 *           releases the service-side wait.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, SwitchSubscribeInfo_ProductTask_SendFailed, TestSize.Level1)
{
    auto cvPtr = std::make_shared<std::condition_variable>();
    auto safeQueue = std::make_shared<SafeQueue<uint8_t>>();
    safeQueue->Push(TEST_PENDING_COUNT);
    auto callback = CreateReplyCallback(cvPtr, safeQueue);
    ASSERT_NE(callback, nullptr);
    OsAccountStateParcel parcel = BuildStateParcel(callback);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(Return(ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT));

    auto switchInfo = std::make_shared<SwitchSubscribeInfo>();
    EXPECT_TRUE(switchInfo->ProductTask(mockEvent, parcel));

    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    bool result = cvPtr->wait_for(lock, std::chrono::seconds(TEST_WAIT_SECONDS),
        [safeQueue]() { return safeQueue->Size() == 0; });
    EXPECT_TRUE(result);
}

/**
 * @tc.name: SwitchSubscribeInfo_ProductTask_SendOk
 * @tc.desc: When OnStateChanged send succeeds, the reply callback is not force-completed; the pending item
 *           remains until OnComplete is invoked.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, SwitchSubscribeInfo_ProductTask_SendOk, TestSize.Level1)
{
    auto cvPtr = std::make_shared<std::condition_variable>();
    auto safeQueue = std::make_shared<SafeQueue<uint8_t>>();
    safeQueue->Push(TEST_PENDING_COUNT);
    auto callback = CreateReplyCallback(cvPtr, safeQueue);
    ASSERT_NE(callback, nullptr);
    OsAccountStateParcel parcel = BuildStateParcel(callback);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    std::promise<void> sendDone;
    auto sendDoneFuture = sendDone.get_future();
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(
        DoAll(Invoke([&sendDone](const OsAccountStateParcel &) { sendDone.set_value(); }), Return(ERR_OK)));

    auto switchInfo = std::make_shared<SwitchSubscribeInfo>();
    EXPECT_TRUE(switchInfo->ProductTask(mockEvent, parcel));

    EXPECT_EQ(sendDoneFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    EXPECT_EQ(safeQueue->Size(), TEST_PENDING_COUNT);
    EXPECT_EQ(callback->OnComplete(), ERR_OK);
    EXPECT_EQ(safeQueue->Size(), 0);
}

/**
 * @tc.name: SwitchSubscribeInfo_ProductTask_NullCallback
 * @tc.desc: When the parcel carries no reply callback, HandleSwitchReplyCallback returns early and the send
 *           still completes without crash.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, SwitchSubscribeInfo_ProductTask_NullCallback, TestSize.Level1)
{
    OsAccountStateParcel parcel = BuildStateParcel(nullptr);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    std::promise<void> sendDone;
    auto sendDoneFuture = sendDone.get_future();
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(
        DoAll(Invoke([&sendDone](const OsAccountStateParcel &) { sendDone.set_value(); }), Return(ERR_OK)));

    auto switchInfo = std::make_shared<SwitchSubscribeInfo>();
    EXPECT_TRUE(switchInfo->ProductTask(mockEvent, parcel));

    EXPECT_EQ(sendDoneFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    SUCCEED();
}

/**
 * @tc.name: SwitchSubscribeInfo_ProductTask_FifoOrder
 * @tc.desc: Two SWITCHING notifies enqueued back-to-back are consumed in FIFO order by the single
 *           per-uid consumer thread. This is the mechanism the rollback ordering guarantee depends on.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, SwitchSubscribeInfo_ProductTask_FifoOrder, TestSize.Level1)
{
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);

    std::mutex orderMutex;
    std::vector<int32_t> order;
    std::condition_variable orderCv;
    int processed = 0;
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).Times(2).WillRepeatedly(
        Invoke([&orderMutex, &order, &orderCv, &processed](const OsAccountStateParcel &stateParcel) {
            {
                std::lock_guard<std::mutex> lock(orderMutex);
                order.push_back(stateParcel.toId);
                ++processed;
            }
            orderCv.notify_all();
            return ERR_OK;
        }));

    OsAccountStateParcel first = BuildStateParcel(nullptr);
    first.toId = TEST_FIFO_FIRST_TO_ID;
    OsAccountStateParcel second = BuildStateParcel(nullptr);
    second.toId = TEST_FIFO_SECOND_TO_ID;

    auto switchInfo = std::make_shared<SwitchSubscribeInfo>();
    EXPECT_TRUE(switchInfo->ProductTask(mockEvent, first));
    EXPECT_TRUE(switchInfo->ProductTask(mockEvent, second));

    std::unique_lock<std::mutex> lock(orderMutex);
    bool done = orderCv.wait_for(lock, std::chrono::seconds(TEST_WAIT_SECONDS),
        [&processed]() { return processed == 2; });
    EXPECT_TRUE(done);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], TEST_FIFO_FIRST_TO_ID);
    EXPECT_EQ(order[1], TEST_FIFO_SECOND_TO_ID);
}

/**
 * @tc.name: EnqueueStateChange_WaitForStateReplies_NoSubscriber
 * @tc.desc: With no subscribers registered, EnqueueStateChange returns a token whose queue is empty,
 *           so WaitForStateReplies completes immediately with ERR_OK (no handshake to await).
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, EnqueueStateChange_WaitForStateReplies_NoSubscriber, TestSize.Level1)
{
    auto &manager = OsAccountSubscribeManager::GetInstance();
    auto token = manager.EnqueueStateChange(TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_TO_ID, 0u);
    EXPECT_EQ(manager.WaitForStateReplies(token, OsAccountState::SWITCHING), ERR_OK);
}

/**
 * @tc.name: EnqueueStateChange_AttachesCallbackForHandshakeSubscriber
 * @tc.desc: For a SWITCHING subscription registered with handshake, EnqueueStateChange attaches a
 *           reply callback to the state parcel and pushes one pending reply item; replying OnComplete
 *           releases the wait. Covers the service-side attach step of the switch handshake without
 *           depending on how the platform built the service.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, EnqueueStateChange_AttachesCallbackForHandshakeSubscriber,
    TestSize.Level1)
{
    auto &manager = OsAccountSubscribeManager::GetInstance();
    std::set<OsAccountState> states = { OsAccountState::SWITCHING };
    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(states, true);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    sptr<IRemoteObject> eventListener = mockEvent;
    ASSERT_EQ(manager.SubscribeOsAccount(subscribeInfoPtr, eventListener), ERR_OK);

    std::promise<void> notified;
    auto notifiedFuture = notified.get_future();
    sptr<IRemoteObject> capturedCallback = nullptr;
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(
        DoAll(Invoke([&](const OsAccountStateParcel &parcel) {
            capturedCallback = parcel.callback;
            notified.set_value();
        }), Return(ERR_OK)));

    auto token = manager.EnqueueStateChange(TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_TO_ID, 0u);
    ASSERT_EQ(notifiedFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    ASSERT_NE(capturedCallback, nullptr);
    EXPECT_EQ(token.safeQueue->Size(), TEST_PENDING_COUNT);
    auto replyCallback = static_cast<OsAccountStateReplyCallbackService *>(capturedCallback.GetRefPtr());
    ASSERT_NE(replyCallback, nullptr);
    EXPECT_EQ(replyCallback->OnComplete(), ERR_OK);
    EXPECT_EQ(manager.WaitForStateReplies(token, OsAccountState::SWITCHING), ERR_OK);
    EXPECT_EQ(manager.UnsubscribeOsAccount(eventListener), ERR_OK);
}

/**
 * @tc.name: EnqueueStateChange_NoCallbackWithoutHandshake
 * @tc.desc: For a SWITCHING subscription registered without handshake, no reply callback is attached
 *           and no pending reply item is pushed, so WaitForStateReplies completes immediately.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, EnqueueStateChange_NoCallbackWithoutHandshake, TestSize.Level1)
{
    auto &manager = OsAccountSubscribeManager::GetInstance();
    std::set<OsAccountState> states = { OsAccountState::SWITCHING };
    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(states, false);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    sptr<IRemoteObject> eventListener = mockEvent;
    ASSERT_EQ(manager.SubscribeOsAccount(subscribeInfoPtr, eventListener), ERR_OK);

    std::promise<void> notified;
    auto notifiedFuture = notified.get_future();
    bool hasCallback = false;
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(
        DoAll(Invoke([&](const OsAccountStateParcel &parcel) {
            hasCallback = parcel.callback != nullptr;
            notified.set_value();
        }), Return(ERR_OK)));

    auto token = manager.EnqueueStateChange(TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_TO_ID, 0u);
    ASSERT_EQ(notifiedFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    EXPECT_FALSE(hasCallback);
    EXPECT_EQ(token.safeQueue->Size(), 0);
    EXPECT_EQ(manager.WaitForStateReplies(token, OsAccountState::SWITCHING), ERR_OK);
    EXPECT_EQ(manager.UnsubscribeOsAccount(eventListener), ERR_OK);
}

#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
/**
 * @tc.name: AwaitPublish001
 * @tc.desc: Publish the switching notification with no subscriber and no configured timeout, then
 *           await it: the await returns without waiting and the publish future is ready.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, AwaitPublish001, TestSize.Level1)
{
    auto &manager = OsAccountSubscribeManager::GetInstance();
    std::future<ErrCode> notifyFuture = manager.Publish(
        TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_TO_ID, 0u);
    EXPECT_TRUE(notifyFuture.valid());
    manager.AwaitPublish(notifyFuture, OsAccountState::SWITCHING, TEST_ACCOUNT_ID, TEST_TO_ID, 0u);
    EXPECT_EQ(notifyFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
}

/**
 * @tc.name: WaitForStateReplies_TimeoutWithConfiguredWait
 * @tc.desc: With a SWITCHING wait configured and a handshake subscriber that never replies,
 *           WaitForStateReplies blocks for the configured duration and then returns
 *           ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, WaitForStateReplies_TimeoutWithConfiguredWait, TestSize.Level1)
{
    auto &staticManager = OsAccountStaticSubscriberManager::GetInstance();
    staticManager.stateTimeouts_[OsAccountState::SWITCHING] = TEST_CONFIGURED_WAIT_MS;
    auto &manager = OsAccountSubscribeManager::GetInstance();
    std::set<OsAccountState> states = { OsAccountState::SWITCHING };
    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(states, true);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    sptr<IRemoteObject> eventListener = mockEvent;
    ASSERT_EQ(manager.SubscribeOsAccount(subscribeInfoPtr, eventListener), ERR_OK);
    std::promise<void> notified;
    auto notifiedFuture = notified.get_future();
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(
        DoAll(Invoke([&](const OsAccountStateParcel &) { notified.set_value(); }), Return(ERR_OK)));

    auto token = manager.EnqueueStateChange(TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_TO_ID, 0u);
    ASSERT_EQ(notifiedFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    EXPECT_EQ(token.safeQueue->Size(), TEST_PENDING_COUNT); // the reply is still pending

    auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(manager.WaitForStateReplies(token, OsAccountState::SWITCHING),
        ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT);
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    EXPECT_GE(elapsedMs, TEST_CONFIGURED_WAIT_MS); // the wait really lasted the configured budget
    EXPECT_EQ(manager.UnsubscribeOsAccount(eventListener), ERR_OK);
}

/**
 * @tc.name: Publish_PendingFutureWithConfiguredWait
 * @tc.desc: With a SWITCHING wait configured, Publish collects the replies on a background
 *           thread: the returned future is pending right after the call and completes with
 *           ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT once the never-replying subscriber exceeds
 *           the configured wait.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, Publish_PendingFutureWithConfiguredWait, TestSize.Level1)
{
    auto &staticManager = OsAccountStaticSubscriberManager::GetInstance();
    staticManager.stateTimeouts_[OsAccountState::SWITCHING] = TEST_CONFIGURED_WAIT_MS;
    auto &manager = OsAccountSubscribeManager::GetInstance();
    std::set<OsAccountState> states = { OsAccountState::SWITCHING };
    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(states, true);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    sptr<IRemoteObject> eventListener = mockEvent;
    ASSERT_EQ(manager.SubscribeOsAccount(subscribeInfoPtr, eventListener), ERR_OK);
    std::promise<void> notified;
    auto notifiedFuture = notified.get_future();
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(
        DoAll(Invoke([&](const OsAccountStateParcel &) { notified.set_value(); }), Return(ERR_OK)));

    std::future<ErrCode> notifyFuture = manager.Publish(
        TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_TO_ID, 0u);
    EXPECT_EQ(notifyFuture.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);
    EXPECT_EQ(notifyFuture.get(), ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT);
    // The dispatched notification must have been consumed before the mock goes away.
    ASSERT_EQ(notifiedFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    EXPECT_EQ(manager.UnsubscribeOsAccount(eventListener), ERR_OK);
}

/**
 * @tc.name: AwaitPublish_TimeoutWithNeverReadyFuture
 * @tc.desc: AwaitPublish with a configured wait and a future that never becomes ready takes
 *           the timeout branch: it returns after the configured wait instead of blocking
 *           without bound, and returns immediately when no wait is configured.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, AwaitPublish_TimeoutWithNeverReadyFuture, TestSize.Level1)
{
    auto &staticManager = OsAccountStaticSubscriberManager::GetInstance();
    auto &manager = OsAccountSubscribeManager::GetInstance();
    std::promise<ErrCode> neverReadyPromise;
    std::future<ErrCode> pendingFuture = neverReadyPromise.get_future();

    staticManager.stateTimeouts_[OsAccountState::SWITCHING] = TEST_CONFIGURED_WAIT_MS;
    auto start = std::chrono::steady_clock::now();
    manager.AwaitPublish(pendingFuture, OsAccountState::SWITCHING, TEST_ACCOUNT_ID, TEST_TO_ID, 0u);
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    EXPECT_GE(elapsedMs, TEST_CONFIGURED_WAIT_MS); // the timeout branch actually waited

    staticManager.stateTimeouts_.clear(); // unconfigured: early return, even for a pending future
    start = std::chrono::steady_clock::now();
    manager.AwaitPublish(pendingFuture, OsAccountState::SWITCHING, TEST_ACCOUNT_ID, TEST_TO_ID, 0u);
    elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    EXPECT_LT(elapsedMs, TEST_CONFIGURED_WAIT_MS);
}


/**
 * @tc.name: Publish_ConcurrentWaitsSettleInOneWindow
 * @tc.desc: A legal slow static subscriber (replies at 200ms within its 300ms budget) plus a
 *           dynamic subscriber that never replies: the static handshake and the dynamic
 *           reply wait run concurrently, so the future settles by the configured window
 *           (~300ms), not the serial sum (~500ms).
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, Publish_ConcurrentWaitsSettleInOneWindow, TestSize.Level1)
{
    auto &staticManager = OsAccountStaticSubscriberManager::GetInstance();
    staticManager.stateTimeouts_[OsAccountState::SWITCHING] = TEST_CONFIGURED_WAIT_MS;
    auto &manager = OsAccountSubscribeManager::GetInstance();
    // static subscriber: slow but within its configured budget
    auto subscriber = std::make_shared<StaticSubscriber>();
    subscriber->withHandshake = true;
    subscriber->handle = nullptr; // no dlclose on destruction
    subscriber->callback = reinterpret_cast<void *>(SlowReplyOnStateChanged);
    staticManager.state2Subscribers_[OsAccountState::SWITCHING] = { subscriber };
    // dynamic subscriber: never replies, so the dynamic wait runs its full configured budget
    std::set<OsAccountState> states = { OsAccountState::SWITCHING };
    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(states, true);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    sptr<IRemoteObject> eventListener = mockEvent;
    ASSERT_EQ(manager.SubscribeOsAccount(subscribeInfoPtr, eventListener), ERR_OK);
    std::promise<void> notified;
    auto notifiedFuture = notified.get_future();
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(
        DoAll(Invoke([&](const OsAccountStateParcel &) { notified.set_value(); }), Return(ERR_OK)));

    auto start = std::chrono::steady_clock::now();
    std::future<ErrCode> notifyFuture = manager.Publish(
        TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_TO_ID, 0u);
    EXPECT_EQ(notifyFuture.get(), ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT); // the dynamic wait really ran
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    EXPECT_GE(elapsedMs, TEST_CONFIGURED_WAIT_MS - 20); // the dynamic wait lasted its budget
    // concurrent, not serial: serial would take TEST_SLOW_REPLY_MS + TEST_CONFIGURED_WAIT_MS
    EXPECT_LT(elapsedMs, TEST_SLOW_REPLY_MS + TEST_CONFIGURED_WAIT_MS - 50);
    // The dispatched notification must have been consumed before the mock goes away.
    ASSERT_EQ(notifiedFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    EXPECT_EQ(manager.UnsubscribeOsAccount(eventListener), ERR_OK);
    staticManager.state2Subscribers_.clear();
}

/**
 * @tc.name: AwaitPublish_JoinsBudgetedHandshakeWithoutMisreport
 * @tc.desc: L-04 regression: with the caller overlapping its own work (100ms) and both
 *           handshakes legal (static replies at 200ms within its 300ms budget, dynamic never
 *           replies), the future settles inside the configured window, so AwaitPublish joins
 *           it instead of reporting a spurious notify timeout.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, AwaitPublish_JoinsBudgetedHandshakeWithoutMisreport, TestSize.Level1)
{
    auto &staticManager = OsAccountStaticSubscriberManager::GetInstance();
    staticManager.stateTimeouts_[OsAccountState::SWITCHING] = TEST_CONFIGURED_WAIT_MS;
    auto &manager = OsAccountSubscribeManager::GetInstance();
    auto subscriber = std::make_shared<StaticSubscriber>();
    subscriber->withHandshake = true;
    subscriber->handle = nullptr; // no dlclose on destruction
    subscriber->callback = reinterpret_cast<void *>(SlowReplyOnStateChanged);
    staticManager.state2Subscribers_[OsAccountState::SWITCHING] = { subscriber };
    std::set<OsAccountState> states = { OsAccountState::SWITCHING };
    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(states, true);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    sptr<IRemoteObject> eventListener = mockEvent;
    ASSERT_EQ(manager.SubscribeOsAccount(subscribeInfoPtr, eventListener), ERR_OK);
    std::promise<void> notified;
    auto notifiedFuture = notified.get_future();
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(
        DoAll(Invoke([&](const OsAccountStateParcel &) { notified.set_value(); }), Return(ERR_OK)));

    std::future<ErrCode> notifyFuture = manager.Publish(
        TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_TO_ID, 0u);
    usleep(TEST_CALLER_WORK_MS * MS_TO_US); // caller's own work, like SendToStorage/AMS
    manager.AwaitPublish(notifyFuture, OsAccountState::SWITCHING, TEST_ACCOUNT_ID, TEST_TO_ID, 0u);
    // joined in time: the future is ready right after the await, no notify-timeout misreport
    EXPECT_EQ(notifyFuture.wait_for(std::chrono::milliseconds(0)), std::future_status::ready);
    EXPECT_EQ(notifyFuture.get(), ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT); // the dynamic timeout is real
    ASSERT_EQ(notifiedFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    EXPECT_EQ(manager.UnsubscribeOsAccount(eventListener), ERR_OK);
    staticManager.state2Subscribers_.clear();
}

/**
 * @tc.name: WaitForStateReplies_NoWaitWithoutConfiguredTimeout
 * @tc.desc: With the handshake feature enabled but no SWITCHING wait configured, a pending
 *           handshake reply is not awaited: WaitForStateReplies returns ERR_OK immediately
 *           (the advisory-dispatch fork declared at WaitForStateReplies).
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountSubscribeManagerTest, WaitForStateReplies_NoWaitWithoutConfiguredTimeout, TestSize.Level1)
{
    auto &staticManager = OsAccountStaticSubscriberManager::GetInstance();
    staticManager.stateTimeouts_.clear();
    auto &manager = OsAccountSubscribeManager::GetInstance();
    std::set<OsAccountState> states = { OsAccountState::SWITCHING };
    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(states, true);
    auto mockEvent = sptr<MockOsAccountEvent>(new (std::nothrow) MockOsAccountEvent());
    ASSERT_NE(mockEvent, nullptr);
    sptr<IRemoteObject> eventListener = mockEvent;
    ASSERT_EQ(manager.SubscribeOsAccount(subscribeInfoPtr, eventListener), ERR_OK);
    std::promise<void> notified;
    auto notifiedFuture = notified.get_future();
    EXPECT_CALL(*mockEvent, OnStateChanged(_)).WillOnce(
        DoAll(Invoke([&](const OsAccountStateParcel &) { notified.set_value(); }), Return(ERR_OK)));

    auto token = manager.EnqueueStateChange(TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_TO_ID, 0u);
    ASSERT_EQ(notifiedFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    EXPECT_EQ(token.safeQueue->Size(), TEST_PENDING_COUNT); // a reply is still pending ...

    auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(manager.WaitForStateReplies(token, OsAccountState::SWITCHING), ERR_OK);
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    EXPECT_LT(elapsedMs, 1000); // ... yet the unconfigured state does not wait for it
    EXPECT_EQ(manager.UnsubscribeOsAccount(eventListener), ERR_OK);
}
#endif
}  // namespace AccountSA
}  // namespace OHOS
