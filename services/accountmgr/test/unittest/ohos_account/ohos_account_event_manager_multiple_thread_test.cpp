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
#include <gtest/hwext/gtest-multithread.h>
#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>
#include "account_log_wrapper.h"
#include "distributed_account_event_service.h"
#include "distributed_account_subscribe_manager.h"

namespace OHOS {
namespace AccountSA {
using namespace testing;
using namespace testing::mt;
using namespace testing::ext;
namespace {
constexpr int32_t TEST_COUNT = 100;
constexpr int32_t TEST_ID = 100;
constexpr int32_t TEST_THREAD_COUNT = 10;
constexpr int32_t TEST_PUBLISH_THREAD_COUNT = 4;
constexpr DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE TEST_TYPE = DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN;
DistributedAccountSubscribeManager &mgr = DistributedAccountSubscribeManager::GetInstance();
}  // namespace
class OhosAccountEventManagerTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp();
    void TearDown() {}
};

void OhosAccountEventManagerTest::SetUp(void) __attribute__((no_sanitize("cfi")))
{
    testing::UnitTest *test = testing::UnitTest::GetInstance();
    ASSERT_NE(test, nullptr);
    const testing::TestInfo *testinfo = test->current_test_info();
    ASSERT_NE(testinfo, nullptr);
    string testCaseName = string(testinfo->name());
    ACCOUNT_LOGI("[SetUp] %{public}s start", testCaseName.c_str());
}

void TestPulishOhosAccountEvent()
{
    for (int32_t i = 0; i < TEST_COUNT; i++) {
        mgr.Publish(TEST_ID, TEST_TYPE);
    }
}

void TestSubscribeOhosAccountEvent()
{
    auto listener = sptr<IRemoteObject>(new (std::nothrow) DistributedAccountEventService());
    ASSERT_NE(listener, nullptr);
    EXPECT_EQ(ERR_OK, mgr.SubscribeDistributedAccountEvent(TEST_TYPE, listener));
    std::vector<std::thread> publishThreads;
    for (int32_t i = 0; i < TEST_PUBLISH_THREAD_COUNT; i++) {
        publishThreads.emplace_back(TestPulishOhosAccountEvent);
    }
    EXPECT_EQ(ERR_OK, mgr.UnsubscribeDistributedAccountEvent(listener));
    for (auto &t : publishThreads) {
        t.join();
    }
}

/**
 * @tc.name: OsAccountEventManagerTestTest001
 * @tc.desc: Test concurrent publish vs unsubscribe with single record per process
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountEventManagerTest, OhosAccountEventManagerMulTest001, TestSize.Level1)
{
    TestSubscribeOhosAccountEvent();
}

/**
 * @tc.name: OnAccountsChanged_ReentrantNoDeadlock_001
 * @tc.desc: Callback re-enters AddType (acquires mapLock_) during OnAccountsChanged.
 *           Before fix: OnAccountsChanged held mapLock_ during callback dispatch,
 *           AddType re-acquires non-recursive mutex → self-deadlock.
 *           After fix: snapshot taken under lock, lock released, callback dispatched
 *           in detached thread → re-entry is safe.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountEventManagerTest, OnAccountsChanged_ReentrantNoDeadlock_001, TestSize.Level1)
{
    auto svc = sptr<DistributedAccountEventService>(new (std::nothrow) DistributedAccountEventService());
    ASSERT_NE(svc, nullptr);

    class DummyCallback final : public DistributedAccountSubscribeCallback {
    public:
        void OnAccountsChanged(const DistributedAccountEventData &eventData) override {}
    };

    class ReentrantCallback final : public DistributedAccountSubscribeCallback {
    public:
        DistributedAccountEventService *svc_;
        std::shared_ptr<std::promise<void>> promise_;

        void OnAccountsChanged(const DistributedAccountEventData &eventData) override
        {
            svc_->AddType(DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGOUT,
                std::make_shared<DummyCallback>());
            promise_->set_value();
        }
    };

    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();
    auto cb = std::make_shared<ReentrantCallback>();
    cb->svc_ = svc.GetRefPtr();
    cb->promise_ = promise;
    svc->AddType(DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, cb);
    EXPECT_TRUE(svc->IsTypeExist(DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, cb));

    DistributedAccountEventData eventData;
    eventData.type_ = DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN;
    EXPECT_EQ(svc->OnAccountsChanged(eventData), ERR_OK);

    auto status = future.wait_for(std::chrono::seconds(3));
    EXPECT_EQ(status, std::future_status::ready);
}

/**
 * @tc.name: OnAccountsChanged_MultiSubscriberConcurrentDispatch_001
 * @tc.desc: Multiple subscribers for the same type all receive callbacks when
 *           OnAccountsChanged dispatches. Each callback runs in its own detached
 *           thread (snapshot + thread dispatch pattern). Verify all N callbacks
 *           are invoked within a timeout.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountEventManagerTest, OnAccountsChanged_MultiSubscriberConcurrentDispatch_001, TestSize.Level1)
{
    auto svc = sptr<DistributedAccountEventService>(new (std::nothrow) DistributedAccountEventService());
    ASSERT_NE(svc, nullptr);

    constexpr int callbackCount = 10;
    std::atomic<int> callCount(0);

    class CountingCallback final : public DistributedAccountSubscribeCallback {
    public:
        std::atomic<int> *count_;

        void OnAccountsChanged(const DistributedAccountEventData &eventData) override
        {
            count_->fetch_add(1);
        }
    };

    for (int i = 0; i < callbackCount; i++) {
        auto cb = std::make_shared<CountingCallback>();
        cb->count_ = &callCount;
        svc->AddType(DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN, cb);
    }
    EXPECT_EQ(svc->GetCallbackSize(), callbackCount);

    DistributedAccountEventData eventData;
    eventData.type_ = DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE::LOGIN;
    EXPECT_EQ(svc->OnAccountsChanged(eventData), ERR_OK);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (callCount.load() < callbackCount && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(callCount.load(), callbackCount);
}

/**
 * @tc.name: ConcurrentSubscribeSameListener_001
 * @tc.desc: 10 threads concurrently subscribe with the same listener; final map has exactly 1 record.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountEventManagerTest, ConcurrentSubscribeSameListener_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto listener = sptr<IRemoteObject>(new (std::nothrow) DistributedAccountEventService());
    ASSERT_NE(listener, nullptr);

    std::vector<std::thread> threads;
    for (int32_t i = 0; i < TEST_THREAD_COUNT; i++) {
        threads.emplace_back([&manager, &listener]() {
            manager.SubscribeDistributedAccountEvent(TEST_TYPE, listener);
        });
    }
    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(ERR_OK, manager.UnsubscribeDistributedAccountEvent(listener));
    EXPECT_EQ(ERR_OHOSACCOUNT_KIT_NO_SPECIFIED_CALLBACK_HAS_BEEN_REGISTERED,
        manager.UnsubscribeDistributedAccountEvent(TEST_TYPE, listener));
}

/**
 * @tc.name: ConcurrentSubscribeUnsubscribeMixed_001
 * @tc.desc: 10 threads concurrently subscribe and unsubscribe the same listener;
 *           verify no crash and map stays consistent (no dangling entries).
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountEventManagerTest, ConcurrentSubscribeUnsubscribeMixed_001, TestSize.Level1)
{
    auto &manager = DistributedAccountSubscribeManager::GetInstance();
    auto listener = sptr<IRemoteObject>(new (std::nothrow) DistributedAccountEventService());
    ASSERT_NE(listener, nullptr);

    EXPECT_EQ(ERR_OK, manager.SubscribeDistributedAccountEvent(TEST_TYPE, listener));

    std::vector<std::thread> threads;
    for (int32_t i = 0; i < TEST_THREAD_COUNT; i++) {
        threads.emplace_back([&manager, &listener, i]() {
            if (i % 2 == 0) {
                manager.SubscribeDistributedAccountEvent(TEST_TYPE, listener);
            } else {
                manager.UnsubscribeDistributedAccountEvent(listener);
            }
        });
    }
    for (auto &t : threads) {
        t.join();
    }
    EXPECT_EQ(ERR_OK, manager.UnsubscribeDistributedAccountEvent(listener));
    EXPECT_EQ(ERR_OHOSACCOUNT_KIT_NO_SPECIFIED_CALLBACK_HAS_BEEN_REGISTERED,
        manager.UnsubscribeDistributedAccountEvent(TEST_TYPE, listener));
}
}  // namespace AccountSA
}  // namespace OHOS
