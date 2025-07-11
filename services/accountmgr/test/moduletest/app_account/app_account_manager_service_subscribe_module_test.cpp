/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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
#include <thread>
#include "account_log_wrapper.h"
#define private public
#include "app_account.h"
#include "app_account_control_manager.h"
#include "app_account_manager_service.h"
#undef private
#include "app_account_subscriber.h"
#include "datetime_ex.h"
#include "singleton.h"

using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;

namespace {
const std::string STRING_NAME = "app_account_subscribe_mt_name";
const std::string STRING_NAME_TWO = "app_account_subscribe_mt_name_two";
const std::string STRING_OWNER = "com.example.owner";
const std::string STRING_BUNDLE_NAME = "com.example.third_party";
const std::string STRING_BUNDLE_NAME_TWO = "com.example.third_party_two";
const std::string STRING_EXTRA_INFO = "extra_info";
std::mutex g_mtx;
const time_t TIME_OUT_SECONDS_LIMIT = 5;

constexpr std::int32_t UID = 10000;
constexpr std::size_t SIZE_ZERO = 0;
constexpr std::size_t SIZE_ONE = 1;
std::int32_t g_counter = 0;
constexpr std::int32_t COUNTER_MAX = 2;
}  // namespace

class AppAccountManagerServiceSubscribeModuleTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void) override;
    void TearDown(void) override;
    AppAccountManagerService* appAccountManagerServicePtr_ = new AppAccountManagerService();
};

void AppAccountManagerServiceSubscribeModuleTest::SetUpTestCase(void)
{
    GTEST_LOG_(INFO) << "SetUpTestCase";
}

void AppAccountManagerServiceSubscribeModuleTest::TearDownTestCase(void)
{
    GTEST_LOG_(INFO) << "TearDownTestCase";
}

void AppAccountManagerServiceSubscribeModuleTest::SetUp(void) __attribute__((no_sanitize("cfi")))
{
    testing::UnitTest *test = testing::UnitTest::GetInstance();
    ASSERT_NE(test, nullptr);
    const testing::TestInfo *testinfo = test->current_test_info();
    ASSERT_NE(testinfo, nullptr);
    string testCaseName = string(testinfo->name());
    ACCOUNT_LOGI("[SetUp] %{public}s start", testCaseName.c_str());
}

void AppAccountManagerServiceSubscribeModuleTest::TearDown(void)
{}

class AppAccountSubscriberTest : public AppAccountSubscriber {
public:
    explicit AppAccountSubscriberTest(const AppAccountSubscribeInfo &subscribeInfo)
        : AppAccountSubscriber(subscribeInfo)
    {
        ACCOUNT_LOGI("enter");
    }

    ~AppAccountSubscriberTest()
    {}

    virtual void OnAccountsChanged(const std::vector<AppAccountInfo> &accounts)
    {
        ACCOUNT_LOGI("enter");

        g_mtx.unlock();

        std::string owner;
        std::string name;
        std::string extraInfo;

        ACCOUNT_LOGI("accounts.size() = %{public}zu", accounts.size());

        for (auto account : accounts) {
            account.GetOwner(owner);
            ACCOUNT_LOGI("owner = %{public}s", owner.c_str());

            account.GetName(name);
            ACCOUNT_LOGI("name = %{public}s", name.c_str());

            account.GetExtraInfo(extraInfo);
            ACCOUNT_LOGI("extraInfo = %{public}s", extraInfo.c_str());
        }
    }
};

class AppAccountSubscriberTestTwo : public AppAccountSubscriber {
public:
    explicit AppAccountSubscriberTestTwo(const AppAccountSubscribeInfo &subscribeInfo)
        : AppAccountSubscriber(subscribeInfo)
    {
        ACCOUNT_LOGI("enter");
    }

    ~AppAccountSubscriberTestTwo()
    {}

    virtual void OnAccountsChanged(const std::vector<AppAccountInfo> &accounts)
    {
        ACCOUNT_LOGI("enter");

        g_mtx.unlock();

        ACCOUNT_LOGI("accounts.size() = %{public}zu", accounts.size());

        EXPECT_EQ(accounts.size(), SIZE_ZERO);
    }
};

class AppAccountSubscriberTestThree : public AppAccountSubscriber {
public:
    explicit AppAccountSubscriberTestThree(const AppAccountSubscribeInfo &subscribeInfo)
        : AppAccountSubscriber(subscribeInfo)
    {
        ACCOUNT_LOGI("enter");
    }

    ~AppAccountSubscriberTestThree()
    {}

    virtual void OnAccountsChanged(const std::vector<AppAccountInfo> &accounts)
    {
        ACCOUNT_LOGI("enter");

        g_mtx.unlock();

        std::string owner;
        std::string name;
        std::string extraInfo;

        ACCOUNT_LOGI("accounts.size() = %{public}zu", accounts.size());

        EXPECT_EQ(accounts.size(), SIZE_ONE);

        for (auto account : accounts) {
            account.GetOwner(owner);
            EXPECT_EQ(owner, STRING_BUNDLE_NAME);

            account.GetName(name);
            EXPECT_EQ(name, STRING_NAME);

            account.GetExtraInfo(extraInfo);
            EXPECT_EQ(extraInfo, STRING_EXTRA_INFO);
        }
    }
};

class AppAccountSubscriberTestFour : public AppAccountSubscriber {
public:
    explicit AppAccountSubscriberTestFour(const AppAccountSubscribeInfo &subscribeInfo)
        : AppAccountSubscriber(subscribeInfo)
    {
        ACCOUNT_LOGI("enter");
    }

    ~AppAccountSubscriberTestFour()
    {}

    virtual void OnAccountsChanged(const std::vector<AppAccountInfo> &accounts)
    {
        ACCOUNT_LOGI("enter");
        GTEST_LOG_(INFO) << "AppAccountSubscriberTestFour::OnAccountsChanged()";

        g_counter++;

        ACCOUNT_LOGI("g_counter = %{public}d", g_counter);
        GTEST_LOG_(INFO) << "g_counter = " << g_counter;

        if (g_counter == COUNTER_MAX) {
            g_mtx.unlock();
        }

        std::string owner;
        std::string name;
        std::string extraInfo;

        ACCOUNT_LOGI("accounts.size() = %{public}zu", accounts.size());

        EXPECT_EQ(accounts.size(), SIZE_ONE);

        for (auto account : accounts) {
            account.GetOwner(owner);
            EXPECT_EQ(owner, STRING_OWNER);

            account.GetName(name);
            EXPECT_EQ(name, STRING_NAME);

            account.GetExtraInfo(extraInfo);
            EXPECT_EQ(extraInfo, STRING_EXTRA_INFO);
        }
    }
};

static void Wait()
{
    // record start time
    struct tm startTime = {0};
    EXPECT_EQ(GetSystemCurrentTime(&startTime), true);

    // record current time
    struct tm doingTime = {0};

    int64_t seconds = 0;
    while (!g_mtx.try_lock()) {
        // get current time and compare it with the start time
        EXPECT_EQ(GetSystemCurrentTime(&doingTime), true);
        seconds = GetSecondsBetween(startTime, doingTime);
        if (seconds >= TIME_OUT_SECONDS_LIMIT) {
            break;
        }
    }

    // the subscriber should receive the event within 5 seconds
    EXPECT_LT(seconds, TIME_OUT_SECONDS_LIMIT);
}

/**
 * @tc.name: AppAccountManagerServiceSubscribe_SubscribeAppAccount_0100
 * @tc.desc: Subscribe app accounts with valid data.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFT
 */
HWTEST_F(AppAccountManagerServiceSubscribeModuleTest, AppAccountManagerServiceSubscribe_SubscribeAppAccount_0100,
    TestSize.Level1)
{
    ACCOUNT_LOGI("AppAccountManagerServiceSubscribe_SubscribeAppAccount_0100");

    // make owners
    std::vector<std::string> owners;
    owners.emplace_back(STRING_OWNER);

    // make subscribe info
    AppAccountSubscribeInfo subscribeInfo(owners);

    // make a subscriber
    auto subscriberTestPtr = std::make_shared<AppAccountSubscriberTest>(subscribeInfo);

    // make an event listener
    sptr<IRemoteObject> appAccountEventListener = nullptr;

    ErrCode subscribeState = AppAccount::GetInstance().CreateAppAccountEventListener(
        subscriberTestPtr, appAccountEventListener);
    EXPECT_EQ(subscribeState, AppAccount::INITIAL_SUBSCRIPTION);

    subscribeState =
        AppAccount::GetInstance().CreateAppAccountEventListener(nullptr, appAccountEventListener);
    EXPECT_EQ(subscribeState, AppAccount::SUBSCRIBE_FAILED);

    bool needNotifyService = false;
    AppAccountEventListener::GetInstance()->SubscribeAppAccount(subscriberTestPtr, needNotifyService);
    EXPECT_TRUE(needNotifyService);

    // subscribe app account
    int32_t funcResult = -1;
    ErrCode result = appAccountManagerServicePtr_->SubscribeAppAccount(subscribeInfo, appAccountEventListener,
        funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // lock the mutex
    g_mtx.lock();

    // add app account
    result = appAccountManagerServicePtr_->AddAccount(STRING_NAME, STRING_EXTRA_INFO, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // set app account extra info
    result = appAccountManagerServicePtr_->SetAccountExtraInfo(STRING_NAME, STRING_EXTRA_INFO, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    Wait();

    AppAccountEventListener::GetInstance()->UnsubscribeAppAccount(subscriberTestPtr, needNotifyService, owners);
    EXPECT_TRUE(needNotifyService);

    // unsubscribe app account
    result = appAccountManagerServicePtr_->UnsubscribeAppAccount(appAccountEventListener, owners, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // delete account
    result = appAccountManagerServicePtr_->DeleteAccount(STRING_NAME, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // unlock the mutex
    g_mtx.unlock();
}

/**
 * @tc.name: AppAccountManagerServiceSubscribe_SubscribeAppAccount_0300
 * @tc.desc: Subscribe app accounts with invalid data.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFT
 */
HWTEST_F(AppAccountManagerServiceSubscribeModuleTest, AppAccountManagerServiceSubscribe_SubscribeAppAccount_0300,
    TestSize.Level1)
{
    ACCOUNT_LOGI("AppAccountManagerServiceSubscribe_SubscribeAppAccount_0300");

    // make owners
    std::vector<std::string> owners;
    owners.emplace_back(STRING_BUNDLE_NAME);

    // make subscribe info
    AppAccountSubscribeInfo subscribeInfo(owners);

    // make a subscriber
    auto subscriberTestPtr = std::make_shared<AppAccountSubscriberTest>(subscribeInfo);

    // make an event listener
    sptr<IRemoteObject> appAccountEventListener = nullptr;

    ErrCode subscribeState = AppAccount::GetInstance().CreateAppAccountEventListener(
        subscriberTestPtr, appAccountEventListener);
    EXPECT_EQ(subscribeState, AppAccount::INITIAL_SUBSCRIPTION);

    // subscribe app account
    int32_t funcResult = -1;
    ErrCode result = appAccountManagerServicePtr_->SubscribeAppAccount(subscribeInfo, appAccountEventListener,
        funcResult);
    EXPECT_EQ(result, ERR_OK);
    EXPECT_EQ(funcResult, ERR_ACCOUNT_COMMON_PERMISSION_DENIED);
}

/**
 * @tc.name: AppAccountManagerServiceSubscribe_SubscribeAppAccount_0500
 * @tc.desc: Subscribe app accounts with invalid data.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFT
 */
HWTEST_F(AppAccountManagerServiceSubscribeModuleTest, AppAccountManagerServiceSubscribe_SubscribeAppAccount_0500,
    TestSize.Level1)
{
    ACCOUNT_LOGI("AppAccountManagerServiceSubscribe_SubscribeAppAccount_0500");

    // add an account
    ErrCode result = appAccountManagerServicePtr_->innerManager_->AddAccount(STRING_NAME,
        STRING_EXTRA_INFO, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);

    // enable app access
    AppAccountCallingInfo appAccountCallingInfo;
    appAccountCallingInfo.callingUid = UID;
    appAccountCallingInfo.bundleName = STRING_BUNDLE_NAME;
    result = appAccountManagerServicePtr_->innerManager_->EnableAppAccess(STRING_NAME,
        STRING_OWNER, appAccountCallingInfo);
    EXPECT_EQ(result, ERR_OK);

    // make owners
    std::vector<std::string> owners;
    owners.emplace_back(STRING_BUNDLE_NAME);

    // make subscribe info
    AppAccountSubscribeInfo subscribeInfo(owners);

    // make a subscriber
    auto subscriberTestPtr = std::make_shared<AppAccountSubscriberTestTwo>(subscribeInfo);

    // make an event listener
    sptr<IRemoteObject> appAccountEventListener = nullptr;

    ErrCode subscribeState = AppAccount::GetInstance().CreateAppAccountEventListener(
        subscriberTestPtr, appAccountEventListener);
    EXPECT_EQ(subscribeState, AppAccount::INITIAL_SUBSCRIPTION);

    bool needNotifyService = false;
    AppAccountEventListener::GetInstance()->SubscribeAppAccount(subscriberTestPtr, needNotifyService);
    EXPECT_TRUE(needNotifyService);

    // subscribe app account
    int32_t funcResult = -1;
    result = appAccountManagerServicePtr_->SubscribeAppAccount(subscribeInfo, appAccountEventListener, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // lock the mutex
    g_mtx.lock();

    // disable app access
    appAccountCallingInfo.callingUid = UID;
    appAccountCallingInfo.bundleName = STRING_BUNDLE_NAME;
    result = appAccountManagerServicePtr_->innerManager_->DisableAppAccess(STRING_NAME,
        STRING_OWNER, appAccountCallingInfo);
    EXPECT_EQ(result, ERR_OK);

    // set extra info
    result = appAccountManagerServicePtr_->innerManager_->SetAccountExtraInfo(STRING_NAME,
        STRING_EXTRA_INFO, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);

    Wait();

    AppAccountEventListener::GetInstance()->UnsubscribeAppAccount(subscriberTestPtr, needNotifyService, owners);
    EXPECT_TRUE(needNotifyService);

    // unsubscribe app account
    result = appAccountManagerServicePtr_->UnsubscribeAppAccount(appAccountEventListener, owners, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // delete account
    result = appAccountManagerServicePtr_->innerManager_->DeleteAccount(STRING_NAME, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);

    // unlock the mutex
    g_mtx.unlock();
}

/**
 * @tc.name: AppAccountManagerServiceSubscribe_SubscribeAppAccount_0600
 * @tc.desc: Subscribe app accounts with valid data.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFT
 */
HWTEST_F(AppAccountManagerServiceSubscribeModuleTest, AppAccountManagerServiceSubscribe_SubscribeAppAccount_0600,
    TestSize.Level1)
{
    ACCOUNT_LOGI("AppAccountManagerServiceSubscribe_SubscribeAppAccount_0600");
    auto accountInnerManager = appAccountManagerServicePtr_->innerManager_;
    // add an account
    ErrCode result = accountInnerManager->AddAccount(STRING_NAME, STRING_EXTRA_INFO, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);
    EXPECT_EQ(accountInnerManager->AddAccount(STRING_NAME_TWO, STRING_EXTRA_INFO, UID, STRING_BUNDLE_NAME, 0), ERR_OK);

    // enable app access
    AppAccountCallingInfo appAccountCallingInfo;
    appAccountCallingInfo.callingUid = UID;
    appAccountCallingInfo.bundleName = STRING_BUNDLE_NAME;
    result = accountInnerManager->EnableAppAccess(STRING_NAME, STRING_OWNER, appAccountCallingInfo);
    EXPECT_EQ(result, ERR_OK);

    // make owners
    std::vector<std::string> owners;
    owners.emplace_back(STRING_BUNDLE_NAME);

    // make subscribe info
    AppAccountSubscribeInfo subscribeInfo(owners);

    // make a subscriber
    auto subscriberTestPtr = std::make_shared<AppAccountSubscriberTestTwo>(subscribeInfo);

    // make an event listener
    sptr<IRemoteObject> appAccountEventListener = nullptr;

    ErrCode subscribeState = AppAccount::GetInstance().CreateAppAccountEventListener(
        subscriberTestPtr, appAccountEventListener);
    EXPECT_EQ(subscribeState, AppAccount::INITIAL_SUBSCRIPTION);

    bool needNotifyService = false;
    AppAccountEventListener::GetInstance()->SubscribeAppAccount(subscriberTestPtr, needNotifyService);
    EXPECT_TRUE(needNotifyService);

    // subscribe app account
    int32_t funcResult = -1;
    result = appAccountManagerServicePtr_->SubscribeAppAccount(subscribeInfo, appAccountEventListener, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // lock the mutex
    g_mtx.lock();

    // disable app access
    appAccountCallingInfo.callingUid = UID;
    appAccountCallingInfo.bundleName = STRING_BUNDLE_NAME;
    result = accountInnerManager->DisableAppAccess(STRING_NAME,
        STRING_OWNER, appAccountCallingInfo);
    EXPECT_EQ(result, ERR_OK);

    // set extra info
    result = accountInnerManager->SetAccountExtraInfo(STRING_NAME,
        STRING_EXTRA_INFO, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);

    Wait();

    AppAccountEventListener::GetInstance()->UnsubscribeAppAccount(subscriberTestPtr, needNotifyService, owners);
    EXPECT_TRUE(needNotifyService);

    // unsubscribe app account
    result = appAccountManagerServicePtr_->UnsubscribeAppAccount(appAccountEventListener, owners, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // delete account
    EXPECT_EQ(accountInnerManager->DeleteAccount(STRING_NAME, UID, STRING_BUNDLE_NAME, 0), ERR_OK);
    EXPECT_EQ(accountInnerManager->DeleteAccount(STRING_NAME_TWO, UID, STRING_BUNDLE_NAME, 0), ERR_OK);

    // unlock the mutex
    g_mtx.unlock();
}

/**
 * @tc.name: AppAccountManagerServiceSubscribe_SubscribeAppAccount_0700
 * @tc.desc: Subscribe app accounts with valid data.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFT
 */
HWTEST_F(AppAccountManagerServiceSubscribeModuleTest, AppAccountManagerServiceSubscribe_SubscribeAppAccount_0700,
    TestSize.Level1)
{
    ACCOUNT_LOGI("AppAccountManagerServiceSubscribe_SubscribeAppAccount_0700");

    // add an account
    ErrCode result = appAccountManagerServicePtr_->innerManager_->AddAccount(STRING_NAME,
        STRING_EXTRA_INFO, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);
    result = appAccountManagerServicePtr_->innerManager_->AddAccount(STRING_NAME_TWO,
        STRING_EXTRA_INFO, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);

    // enable app access
    AppAccountCallingInfo appAccountCallingInfo;
    appAccountCallingInfo.callingUid = UID;
    appAccountCallingInfo.bundleName = STRING_BUNDLE_NAME;
    result = appAccountManagerServicePtr_->innerManager_->EnableAppAccess(STRING_NAME,
        STRING_OWNER, appAccountCallingInfo);
    EXPECT_EQ(result, ERR_OK);

    // make owners
    std::vector<std::string> owners;
    owners.emplace_back(STRING_BUNDLE_NAME);

    // make subscribe info
    AppAccountSubscribeInfo subscribeInfo(owners);

    // make a subscriber
    auto subscriberTestPtr = std::make_shared<AppAccountSubscriberTestThree>(subscribeInfo);

    // make an event listener
    sptr<IRemoteObject> appAccountEventListener = nullptr;

    ErrCode subscribeState = AppAccount::GetInstance().CreateAppAccountEventListener(
        subscriberTestPtr, appAccountEventListener);
    EXPECT_EQ(subscribeState, AppAccount::INITIAL_SUBSCRIPTION);

    bool needNotifyService = false;
    AppAccountEventListener::GetInstance()->SubscribeAppAccount(subscriberTestPtr, needNotifyService);
    EXPECT_TRUE(needNotifyService);

    // subscribe app account
    int32_t funcResult = -1;
    result = appAccountManagerServicePtr_->SubscribeAppAccount(subscribeInfo, appAccountEventListener, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // lock the mutex
    g_mtx.lock();

    // set extra info
    result = appAccountManagerServicePtr_->innerManager_->SetAccountExtraInfo(STRING_NAME,
        STRING_EXTRA_INFO, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);

    Wait();

    AppAccountEventListener::GetInstance()->UnsubscribeAppAccount(subscriberTestPtr, needNotifyService, owners);
    EXPECT_TRUE(needNotifyService);

    // unsubscribe app account
    result = appAccountManagerServicePtr_->UnsubscribeAppAccount(appAccountEventListener, owners, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // delete account
    result = appAccountManagerServicePtr_->innerManager_->DeleteAccount(STRING_NAME, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);
    result = appAccountManagerServicePtr_->innerManager_->DeleteAccount(STRING_NAME_TWO, UID, STRING_BUNDLE_NAME, 0);
    EXPECT_EQ(result, ERR_OK);

    // unlock the mutex
    g_mtx.unlock();
}

/**
 * @tc.name: AppAccountManagerServiceSubscribe_SubscribeAppAccount_0800
 * @tc.desc: Subscribe app accounts with valid data.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFT
 */
HWTEST_F(AppAccountManagerServiceSubscribeModuleTest, AppAccountManagerServiceSubscribe_SubscribeAppAccount_0800,
    TestSize.Level1)
{
    ACCOUNT_LOGI("AppAccountManagerServiceSubscribe_SubscribeAppAccount_0800");

    // add app account
    int32_t funcResult = -1;
    ErrCode result = appAccountManagerServicePtr_->AddAccount(STRING_NAME, STRING_EXTRA_INFO, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // make owners
    std::vector<std::string> owners{STRING_OWNER};

    // make subscribe info
    AppAccountSubscribeInfo subscribeInfo(owners);

    // make a subscriber
    auto subscriberTestPtr = std::make_shared<AppAccountSubscriberTestFour>(subscribeInfo);

    // make an event listener
    sptr<IRemoteObject> appAccountEventListener = nullptr;

    ErrCode subscribeState = AppAccount::GetInstance().CreateAppAccountEventListener(
        subscriberTestPtr, appAccountEventListener);
    EXPECT_EQ(subscribeState, AppAccount::INITIAL_SUBSCRIPTION);

    bool needNotifyService = false;
    result = AppAccountEventListener::GetInstance()->SubscribeAppAccount(subscriberTestPtr, needNotifyService);
    EXPECT_EQ(result, ERR_OK);
    EXPECT_TRUE(needNotifyService);

    // subscribe app account
    result = appAccountManagerServicePtr_->SubscribeAppAccount(subscribeInfo, appAccountEventListener, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // make a subscriber
    auto subscriberTestPtrTwo = std::make_shared<AppAccountSubscriberTestFour>(subscribeInfo);

    // make an event listener
    sptr<IRemoteObject> appAccountEventListenerTwo = nullptr;

    subscribeState = AppAccount::GetInstance().CreateAppAccountEventListener(
        subscriberTestPtrTwo, appAccountEventListenerTwo);
    EXPECT_EQ(subscribeState, AppAccount::INITIAL_SUBSCRIPTION);

    needNotifyService = false;
    result = AppAccountEventListener::GetInstance()->SubscribeAppAccount(subscriberTestPtrTwo, needNotifyService);
    EXPECT_EQ(result, ERR_OK);
    EXPECT_FALSE(needNotifyService);
    // subscribe app account
    result = appAccountManagerServicePtr_->SubscribeAppAccount(subscribeInfo, appAccountEventListenerTwo, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // lock the mutex
    g_mtx.lock();

    // set extra info
    result = appAccountManagerServicePtr_->SetAccountExtraInfo(STRING_NAME, STRING_EXTRA_INFO, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    Wait();

    result =
        AppAccountEventListener::GetInstance()->UnsubscribeAppAccount(subscriberTestPtr, needNotifyService, owners);
    EXPECT_EQ(result, ERR_OK);
    EXPECT_FALSE(needNotifyService);

    // unsubscribe app account
    result = appAccountManagerServicePtr_->UnsubscribeAppAccount(appAccountEventListener, owners, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    result =
        AppAccountEventListener::GetInstance()->UnsubscribeAppAccount(subscriberTestPtrTwo, needNotifyService, owners);
    EXPECT_EQ(result, ERR_OK);
    EXPECT_TRUE(needNotifyService);
    // unsubscribe app account
    result = appAccountManagerServicePtr_->UnsubscribeAppAccount(appAccountEventListenerTwo, owners, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // delete app account
    result = appAccountManagerServicePtr_->DeleteAccount(STRING_NAME, funcResult);
    EXPECT_EQ(funcResult, ERR_OK);

    // unlock the mutex
    g_mtx.unlock();
}
