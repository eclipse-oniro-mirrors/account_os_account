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
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <set>
#include "account_error_no.h"
#include "os_account_event_listener.h"
#include "os_account_state_parcel.h"
#include "os_account_subscriber.h"
#include "os_account_subscribe_info.h"

namespace OHOS {
namespace AccountSA {
using namespace testing;
using namespace testing::ext;

namespace {
constexpr int32_t TEST_FROM_ID = 100;
constexpr int32_t TEST_TO_ID = 101;
constexpr int32_t TEST_WAIT_SECONDS = 5;
}  // namespace

class TestStateSubscriber : public OsAccountSubscriber {
public:
    explicit TestStateSubscriber(const OsAccountSubscribeInfo &subscribeInfo)
        : OsAccountSubscriber(subscribeInfo)
    {}
    ~TestStateSubscriber() override = default;

    void OnStateChanged(const OsAccountStateData &data) override
    {
        state_ = data.state;
        hasCallback_ = data.callback != nullptr;
        notified_.set_value();
    }

    bool hasCallback_ = false;
    OsAccountState state_ = OsAccountState::INVALID_TYPE;
    std::promise<void> notified_;
};

class OsAccountEventListenerTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

/**
 * @tc.name: OnStateChangedAttachesCallbackForSwitching001
 * @tc.desc: A subscriber registered with handshake receives SWITCHING state data carrying a local
 *           reply callback, which is the client-side attach step of the switch handshake.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountEventListenerTest, OnStateChangedAttachesCallbackForSwitching001, TestSize.Level1)
{
    std::set<OsAccountState> states = { OsAccountState::SWITCHING };
    OsAccountSubscribeInfo subscribeInfo(states, true);
    auto subscriber = std::make_shared<TestStateSubscriber>(subscribeInfo);
    OsAccountEventListener listener;
    ASSERT_EQ(listener.InsertRecord(subscriber), ERR_OK);

    OsAccountStateParcel parcel;
    parcel.state = OsAccountState::SWITCHING;
    parcel.fromId = TEST_FROM_ID;
    parcel.toId = TEST_TO_ID;
    auto notifiedFuture = subscriber->notified_.get_future();
    ASSERT_EQ(listener.OnStateChanged(parcel), ERR_OK);
    ASSERT_EQ(notifiedFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    EXPECT_EQ(subscriber->state_, OsAccountState::SWITCHING);
    EXPECT_TRUE(subscriber->hasCallback_);
    ASSERT_EQ(listener.RemoveRecord(subscriber), ERR_OK);
}

/**
 * @tc.name: OnStateChangedNoCallbackWithoutHandshake002
 * @tc.desc: A subscriber registered without handshake receives SWITCHING state data without a local
 *           reply callback, so no reply is expected from it.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountEventListenerTest, OnStateChangedNoCallbackWithoutHandshake002, TestSize.Level1)
{
    std::set<OsAccountState> states = { OsAccountState::SWITCHING };
    OsAccountSubscribeInfo subscribeInfo(states, false);
    auto subscriber = std::make_shared<TestStateSubscriber>(subscribeInfo);
    OsAccountEventListener listener;
    ASSERT_EQ(listener.InsertRecord(subscriber), ERR_OK);

    OsAccountStateParcel parcel;
    parcel.state = OsAccountState::SWITCHING;
    parcel.fromId = TEST_FROM_ID;
    parcel.toId = TEST_TO_ID;
    auto notifiedFuture = subscriber->notified_.get_future();
    ASSERT_EQ(listener.OnStateChanged(parcel), ERR_OK);
    ASSERT_EQ(notifiedFuture.wait_for(std::chrono::seconds(TEST_WAIT_SECONDS)), std::future_status::ready);
    EXPECT_EQ(subscriber->state_, OsAccountState::SWITCHING);
    EXPECT_FALSE(subscriber->hasCallback_);
    ASSERT_EQ(listener.RemoveRecord(subscriber), ERR_OK);
}

/**
 * @tc.name: IsStateNeedHandshake001
 * @tc.desc: Only the states whose publisher waits for subscriber replies require the handshake.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountEventListenerTest, IsStateNeedHandshake001, TestSize.Level1)
{
    EXPECT_TRUE(IsStateNeedHandshake(OsAccountState::STOPPING));
    EXPECT_TRUE(IsStateNeedHandshake(OsAccountState::LOCKING));
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
    EXPECT_TRUE(IsStateNeedHandshake(OsAccountState::SWITCHING));
#else
    EXPECT_FALSE(IsStateNeedHandshake(OsAccountState::SWITCHING));
#endif
    EXPECT_FALSE(IsStateNeedHandshake(OsAccountState::ACTIVATED));
    EXPECT_FALSE(IsStateNeedHandshake(OsAccountState::UNLOCKED));
    EXPECT_FALSE(IsStateNeedHandshake(OsAccountState::INVALID_TYPE));
}
}  // namespace AccountSA
}  // namespace OHOS
