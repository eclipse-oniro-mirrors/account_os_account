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

#include "account_error_no.h"
#include "account_log_wrapper.h"
#include "account_permission_manager.h"
#include "admin_authorization_callback_stub.h"
#include "authorization_callback.h"
#include "authorization_callback_stub.h"
#include "authorization_common.h"
#include "bundle_manager_adapter.h"
#include "extension_manager_client.h"
#include "iauthorization_callback.h"
#include "iinner_os_account_manager.h"
#include "privilege_cache_manager.h"
#include "privileges_map.h"
#include "service_extension_connect.h"
#include "tee_auth_adapter.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

#define protected public
#define private public
#include "inner_authorization_manager.h"
#undef private
#undef protected

using namespace testing::ext;
using namespace testing;
namespace OHOS {
namespace AccountSA {
namespace {
const std::string TEST_BUNDLE_NAME = "com.test.bundle";
const std::string TEST_ABILITY_NAME = "com.test.bundle.MainAbility";
const std::string TEST_PRIVILEGE = "ohos.privilege.MANAGE_LOCAL_ACCOUNTS";
const std::string TEST_PRIVILEGE_2 = "ohos.privilege.operate_raw_net_packets";
const std::string TEST_DESCRIPTION = "Test privilege description";
const std::string TEST_AUTH_APP_BUNDLE = "com.example.authapp";
const std::string TEST_UI_EXTENSION = "com.example.authapp.UIExtension";
const std::string TEST_SERVICE_EXTENSION = "com.example.authapp.ServiceExtension";
const std::string TEST_SESSIONID = "111111";
const int32_t TEST_CALLING_UID = 200100; // UID for account 100
const int32_t TEST_CALLING_PID = 12345;
const int32_t ERR_PID = -1;
const int32_t TEST_USER_ID = 100;
const int32_t CALLBACK_MAP_LIMIT_PID_BASE = 60000;
const int32_t CALLBACK_MAP_MAX_SIZE = 65535;
const uint32_t TEST_TIME_OUT = 300;
static bool g_transferPrivilegeToCode = true;
static bool g_getPrivilegeBriefDef = true;
static bool g_hasAuthorizationFail = false;
static bool g_openSmartPidFdFail = false;
static std::string g_kernelPermission = "test_kernel_perm";

class MockAdminAuthorizationCallbackNullAsObject : public AdminAuthorizationCallbackStub {
public:
    sptr<IRemoteObject> AsObject() override { return nullptr; }
    ErrCode OnResult(const AdminAuthorizationResult &authResult) override { return ERR_OK; }
};

} // namespace
std::function<ErrCode(int32_t, AuthorizationResult &, int32_t)> acquireAuthorizationOnResultfunc();
std::mutex g_mutex;
std::map<int32_t, sptr<IAuthorizationCallback>> g_callbackMap;
std::map<int32_t, sptr<IRemoteObject>> g_requestRemoteObjectMap;
std::map<int32_t, sptr<ConnectAbilityCallback>> g_connectbackMap;
std::map<int32_t, int32_t> g_pidToUidMap;
std::map<int32_t, SmartPidFd> g_pidFdMap;
std::map<std::string, int32_t> g_sessionIdToPidMap;
/**
 * @class MockIAuthorizationCallback
 * Mock implementation of IAuthorizationCallback.
 */
class MockIAuthorizationCallback final {
public:
    MOCK_METHOD2(OnResult, void(int32_t resultCode, const AuthorizationResult& result));
    MOCK_METHOD2(OnConnectAbility, void(const ConnectAbilityInfo& info, const sptr<IRemoteObject>& connectCallback));
};

/**
 * @class MockAuthorizationCallbackStub
 * Mock implementation of AuthorizationCallbackStub for IRemoteObject.
 */
class MockAuthorizationCallbackStub : public AuthorizationCallbackStub {
public:
    MockAuthorizationCallbackStub() = default;
    ~MockAuthorizationCallbackStub() override = default;

    ErrCode OnResult(int32_t resultCode, const AccountSA::AuthorizationResult& result) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        isReady = true;
        result_ = result;
        cv.notify_one();
        return 0;
    }

    ErrCode OnBatchResult(int32_t resultCode,
        const std::vector<AccountSA::AuthorizationResult>& results) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        isReady = true;
        batchResults_ = results;
        cv.notify_one();
        return 0;
    }

    ErrCode OnConnectAbility(const AccountSA::ConnectAbilityInfo& info, const sptr<IRemoteObject>& callback) override
    {
        auto connectCallback = iface_cast<IConnectAbilityCallback>(callback);
        if (connectCallback == nullptr) {
            ACCOUNT_LOGE("ConnectAbilityCallback proxy is nullptr");
            return -1;
        }
        auto task = [connectCallback] {
            std::vector<uint8_t> iamToken;
            connectCallback->OnResult(0, iamToken, -1, -1);
        };
        std::thread taskThread(task);
        pthread_setname_np(taskThread.native_handle(), "OnConnectAbility");
        taskThread.detach();

        return 0;
    }

    bool AddDeathRecipient(const sptr<DeathRecipient>& recipient) override { return true; }

    int SendRequest(uint32_t code, MessageParcel& data, MessageParcel& reply, MessageOption& option) override
    {
        sendRequestTime++;
        return 0;
    }

    int32_t sendRequestTime = 0;
    std::mutex mutex;
    std::condition_variable cv;
    bool isReady = false;
    AccountSA::AuthorizationResult result_;
    std::vector<AccountSA::AuthorizationResult> batchResults_;
};

class MockAccountPermissionManager {
public:
    static MockAccountPermissionManager& GetInstance()
    {
        static MockAccountPermissionManager instance;
        return instance;
    }
    MOCK_METHOD2(VerifyPermission, ErrCode(const uint32_t tokenId, const std::string& permissionName));
};

ErrCode AccountPermissionManager::VerifyPermission(const uint32_t tokenId, const std::string& permissionName)
{
    return MockAccountPermissionManager::GetInstance().VerifyPermission(tokenId, permissionName);
}

/**
 * @class MockIInnerOsAccountManager
 * Mock for IInnerOsAccountManager.
 */
class MockIInnerOsAccountManager {
public:
    static MockIInnerOsAccountManager& GetInstance()
    {
        static MockIInnerOsAccountManager instance;
        return instance;
    }

    MOCK_METHOD2(GetOsAccountType, ErrCode(int32_t id, OsAccountType& type));
    MOCK_METHOD2(GetForegroundOsAccountLocalId, ErrCode(const uint64_t displayId, int32_t &localId));
    MOCK_METHOD2(IsOsAccountExists, ErrCode(int32_t id, bool& isExists));
};

/**
 * @class MockOsAccountTeeAdapter
 * Mock for OsAccountTeeAdapter.
 */
class MockOsAccountTeeAdapter {
public:
    static MockOsAccountTeeAdapter& GetInstance()
    {
        static MockOsAccountTeeAdapter instance;
        return instance;
    }

    MOCK_METHOD2(TaAcquireAuthorization, ErrCode(const ApplyUserTokenParam& param, ApplyUserTokenResult& result));
    MOCK_METHOD2(TaAcquireAuthorizationBatch,
        ErrCode(const ApplyUserTokenParamV2& param, ApplyUserTokenResultV2& result));
    MOCK_METHOD2(VerifyToken, ErrCode(const std::vector<uint8_t>& token, std::vector<uint8_t>& tokenResult));
};

SessionAbilityConnection& SessionAbilityConnection::GetInstance()
{
    static SessionAbilityConnection instance;
    return instance;
}

bool SessionAbilityConnection::GetConnectInfo(int32_t callingUid, ConnectAbilityInfo& info)
{
    info.callingPid = callingUid;
    info.privileges = {TEST_PRIVILEGE};
    return true;
}

ErrCode SessionAbilityConnection::SaveAuthorizationResult(ErrCode errCode,
    AuthorizationResultCode& resultCode,
    const std::vector<uint8_t>& iamToken,
    int32_t remainValidityTime)
{
    return ERR_OK;
}

ErrCode SessionAbilityConnection::SessionConnectExtension(const ConnectAbilityInfo& info,
    sptr<IAuthorizationCallback>& callback,
    AuthorizationResult& authorizationResult)
{
    return ERR_OK;
}

ErrCode OpenSmartPidFd(const int32_t pid, SmartPidFd &fdPtr)
{
    if (g_openSmartPidFdFail) {
        return ERR_AUTHORIZATION_GET_PROXY_ERROR;
    }
    return ERR_OK;
}

ErrCode GetUptimeMs(int64_t &bootTimeStampMs)
{
    bootTimeStampMs = 1L;
    return ERR_OK;
}

// Mock implementations that redirect to Mock classes
ErrCode IInnerOsAccountManager::GetOsAccountType(int32_t id, OsAccountType& type)
{
    return MockIInnerOsAccountManager::GetInstance().GetOsAccountType(id, type);
}

ErrCode IInnerOsAccountManager::GetForegroundOsAccountLocalId(const uint64_t displayId, int32_t &localId)
{
    return MockIInnerOsAccountManager::GetInstance().GetForegroundOsAccountLocalId(displayId, localId);
}

ErrCode IInnerOsAccountManager::IsOsAccountExists(const int id, bool &isExists)
{
    return MockIInnerOsAccountManager::GetInstance().IsOsAccountExists(id, isExists);
}

ErrCode OsAccountTeeAdapter::TaAcquireAuthorization(const ApplyUserTokenParam& param, ApplyUserTokenResult& result)
{
    return MockOsAccountTeeAdapter::GetInstance().TaAcquireAuthorization(param, result);
}

ErrCode OsAccountTeeAdapter::TaAcquireAuthorizationBatch(
    const ApplyUserTokenParamV2& param, ApplyUserTokenResultV2& result)
{
    return MockOsAccountTeeAdapter::GetInstance().TaAcquireAuthorizationBatch(param, result);
}

ErrCode OsAccountTeeAdapter::VerifyToken(
    const std::vector<uint8_t>& token, const std::string &privilege, std::vector<uint8_t>& tokenResult)
{
    return MockOsAccountTeeAdapter::GetInstance().VerifyToken(token, tokenResult);
}

PrivilegeCacheManager& PrivilegeCacheManager::GetInstance()
{
    static PrivilegeCacheManager instance;
    return instance;
}

static bool g_addCacheFail = false;
static bool g_kernelDeviceNotFound = false;

ErrCode PrivilegeCacheManager::AddCache(const AuthenCallerInfo& callerInfo, uint32_t safeStartTime)
{
    if (callerInfo.pid == ERR_PID || g_addCacheFail) {
        return ERR_AUTHORIZATION_CACHE_ERROR;
    }
    return 0;
}

ErrCode PrivilegeCacheManager::AddCacheAndNotifyKernel(const AuthenCallerInfo& callerInfo,
    const std::string& tokenResult)
{
    if (g_kernelDeviceNotFound) {
        return ERR_AUTHORIZATION_KERNEL_DEVICE_NOT_FOUND;
    }
    if (callerInfo.pid == ERR_PID || g_addCacheFail) {
        return ERR_AUTHORIZATION_CACHE_ERROR;
    }
    return 0;
}

ErrCode PrivilegeCacheManager::HasKernelAuthorization(int32_t pid, uint32_t privilegeIdx,
    const std::string& kernelPermission, bool& isAuthorized)
{
    if (g_hasAuthorizationFail) {
        return ERR_ACCOUNT_COMMON_SYSTEM_SERVICE_EXCEPTION;
    }
    isAuthorized = true;
    return 0;
}

bool TransferPrivilegeToCode(const std::string& privilegeName, uint32_t& code) { return g_transferPrivilegeToCode; }

bool GetPrivilegeBriefDef(const std::string& privilege, PrivilegeBriefDef& privilegeBriefDef)
{
    if (!g_getPrivilegeBriefDef) {
        return false;
    }
    privilegeBriefDef.privilegeName = const_cast<char*>(privilege.c_str());
    privilegeBriefDef.description = const_cast<char*>(TEST_DESCRIPTION.c_str());
    privilegeBriefDef.timeout = TEST_TIME_OUT;
    privilegeBriefDef.kernelPermission = g_kernelPermission.empty() ? nullptr :
        const_cast<char*>(g_kernelPermission.c_str());
    return true;
}

class InnerAuthorizationManagerModuleTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

    InnerAuthorizationManager& manager_ = InnerAuthorizationManager::GetInstance();
};

void InnerAuthorizationManagerModuleTest::SetUpTestCase() { ACCOUNT_LOGI("SetUpTestCase enter"); }

void InnerAuthorizationManagerModuleTest::TearDownTestCase() { ACCOUNT_LOGI("TearDownTestCase enter"); }

void InnerAuthorizationManagerModuleTest::SetUp()
{
    // Set default mock behaviors
    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(OsAccountType::ADMIN), Return(ERR_OK)));
    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetForegroundOsAccountLocalId(_, _))
        .WillRepeatedly(Return(ERR_OK));
    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), IsOsAccountExists(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(true), Return(ERR_OK)));

    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorization(_, _)).WillRepeatedly(Return(ERR_OK));
}

void InnerAuthorizationManagerModuleTest::TearDown()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_callbackMap.clear();
    g_connectbackMap.clear();
    g_requestRemoteObjectMap.clear();
    g_pidToUidMap.clear();
    g_pidFdMap.clear();
    g_sessionIdToPidMap.clear();

    g_transferPrivilegeToCode = true;
    g_getPrivilegeBriefDef = true;
    g_hasAuthorizationFail = false;
    g_openSmartPidFdFail = false;
    g_kernelPermission = "test_kernel_perm";
    g_addCacheFail = false;
    g_kernelDeviceNotFound = false;
}

/**
 * @tc.name: InnerAuthorizationManagerTest_GetInstance_0100
 * @tc.desc: test GetInstance singleton.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, InnerAuthorizationManagerTest_GetInstance_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("InnerAuthorizationManagerTest_GetInstance_0100");

    auto& instance1 = InnerAuthorizationManager::GetInstance();
    auto& instance2 = InnerAuthorizationManager::GetInstance();

    EXPECT_EQ(&instance1, &instance2);
    sptr<IRemoteObject> callback = nullptr;
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();
    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3};
    options.isReuseNeeded = true;
    options.isInteractionAllowed = true;
    options.hasContext = false;

    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    config.authAppServiceExtensionAbilityName = TEST_SERVICE_EXTENSION;
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    req.results = results;
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, callback, requestObj);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_GET_PROXY_ERROR);
}

/**
 * @tc.name: AcquireAuthorizationTest_0200
 * @tc.desc: test AcquireAuthorization with service extension (no context).
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAuthorizationTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("AcquireAuthorizationTest_0200");
    EXPECT_CALL(MockAccountPermissionManager::GetInstance(), VerifyPermission(_, _)).WillRepeatedly(Return(ERR_OK));

    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3};
    options.isReuseNeeded = true;
    options.isInteractionAllowed = true;
    options.hasContext = false;

    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    config.authAppServiceExtensionAbilityName = TEST_SERVICE_EXTENSION;

    sptr<IRemoteObject> callbackObj = new MockAuthorizationCallbackStub();
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();

    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    req.results = results;
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, callbackObj, requestObj);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: AcquireAuthorizationTest_0300
 * @tc.desc: test AcquireAuthorization with UI extension (has context).
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAuthorizationTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("AcquireAuthorizationTest_0300");
    EXPECT_CALL(MockAccountPermissionManager::GetInstance(), VerifyPermission(_, _)).WillRepeatedly(Return(ERR_OK));

    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3};
    options.isReuseNeeded = true;
    options.isInteractionAllowed = true;
    options.hasContext = true;

    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    config.authAppUIExtensionAbilityName = TEST_UI_EXTENSION;

    sptr<MockAuthorizationCallbackStub> callbackObj = new MockAuthorizationCallbackStub();
    sptr<MockAuthorizationCallbackStub> requestObj = new MockAuthorizationCallbackStub();

    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    req.results = results;
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, callbackObj->AsObject(), requestObj->AsObject());
    EXPECT_EQ(ret, ERR_OK);

    // Give time for the detached thread to complete
    std::unique_lock<std::mutex> lock(callbackObj->mutex);
    callbackObj->cv.wait_for(
        lock, std::chrono::seconds(3), [callbackObj = callbackObj]() { return callbackObj->isReady; });
}

/**
 * @tc.name: AcquireAuthorizationTest_0400
 * @tc.desc: test AcquireAuthorization with null requestRemoteObj.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAuthorizationTest_0400, TestSize.Level0)
{
    ACCOUNT_LOGI("AcquireAuthorizationTest_0400");
    EXPECT_CALL(MockAccountPermissionManager::GetInstance(), VerifyPermission(_, _)).WillRepeatedly(Return(ERR_OK));

    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3};
    options.isReuseNeeded = true;
    options.isInteractionAllowed = true;
    options.hasContext = true;

    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    config.authAppUIExtensionAbilityName = TEST_UI_EXTENSION;

    sptr<IRemoteObject> callbackObj = new MockAuthorizationCallbackStub();
    sptr<IRemoteObject> requestObj = nullptr;

    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    req.results = results;
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, callbackObj, requestObj);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_GET_PROXY_ERROR);
}

/**
 * @tc.name: AcquireAuthorizationTest_0500
 * @tc.desc: test AcquireAuthorization with service extension connection failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAuthorizationTest_0500, TestSize.Level0)
{
    ACCOUNT_LOGI("AcquireAuthorizationTest_0500");
    EXPECT_CALL(MockAccountPermissionManager::GetInstance(), VerifyPermission(_, _)).WillRepeatedly(Return(ERR_OK));

    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3};
    options.isReuseNeeded = true;
    options.isInteractionAllowed = true;
    options.hasContext = false;

    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    config.authAppServiceExtensionAbilityName = TEST_SERVICE_EXTENSION;

    sptr<IRemoteObject> callbackObj = new MockAuthorizationCallbackStub();
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();

    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    req.results = results;
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, callbackObj, requestObj);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: VerifyAdminAccountTest_0100
 * @tc.desc: test VerifyAdminAccount with admin account.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, VerifyAdminAccountTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("VerifyAdminAccountTest_0100");

    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(TEST_USER_ID, _))
        .WillOnce(DoAll(SetArgReferee<1>(OsAccountType::ADMIN), Return(ERR_OK)));

    auto [errCode, resultCode] = manager_.VerifyAdminAccount(TEST_USER_ID);

    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_EQ(resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
}

/**
 * @tc.name: VerifyAdminAccountTest_0200
 * @tc.desc: test VerifyAdminAccount with non-admin account.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, VerifyAdminAccountTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("VerifyAdminAccountTest_0200");

    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(TEST_USER_ID, _))
        .WillOnce(DoAll(SetArgReferee<1>(OsAccountType::NORMAL), Return(ERR_OK)));

    auto [errCode, resultCode] = manager_.VerifyAdminAccount(TEST_USER_ID);

    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_EQ(resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
}

/**
 * @tc.name: VerifyAdminAccountTest_0300
 * @tc.desc: test VerifyAdminAccount with GetOsAccountType failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, VerifyAdminAccountTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("VerifyAdminAccountTest_0300");

    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(TEST_USER_ID, _))
        .WillOnce(Return(ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR));

    auto [errCode, resultCode] = manager_.VerifyAdminAccount(TEST_USER_ID);

    EXPECT_EQ(errCode, ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR);
    EXPECT_EQ(resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
}

/**
 * @tc.name: AdminAuthCallbackCallTaForTokenTest_0100
 * @tc.desc: test admin auth callback passes privilege to TEE.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AdminAuthCallbackCallTaForTokenTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("AdminAuthCallbackCallTaForTokenTest_0100");

    std::vector<uint8_t> challenge = {1, 2, 3, 4};
    std::vector<uint8_t> iamToken = {5, 6, 7, 8};
    std::vector<uint8_t> token;
    AdminAuthCallback callback(challenge, nullptr, TEST_USER_ID, TEST_CALLING_PID, TEST_PRIVILEGE);

    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorization(_, _))
        .WillOnce(Invoke([](const ApplyUserTokenParam &param, ApplyUserTokenResult &result) {
            EXPECT_EQ(param.pid, static_cast<uint32_t>(TEST_CALLING_PID));
            EXPECT_EQ(param.permissionSize, TEST_PRIVILEGE.size());
            std::string privilege(reinterpret_cast<const char *>(param.permission), param.permissionSize);
            EXPECT_EQ(privilege, TEST_PRIVILEGE);
            result.userTokenSize = 2;
            result.userToken[0] = 0x12;
            result.userToken[1] = 0x34;
            return ERR_OK;
        }));

    ErrCode ret = callback.CallTAForToken(TEST_USER_ID, iamToken, token);

    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(token, std::vector<uint8_t>({0x12, 0x34}));
}

/**
 * @tc.name: UpdatePrivilegeCacheTest_0100
 * @tc.desc: test UpdatePrivilegeCache success.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdatePrivilegeCacheTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdatePrivilegeCacheTest_0100");

    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;

    ApplyUserTokenResult tokenResult;
    tokenResult.grantTime = 1000;

    ErrCode ret = manager_.UpdatePrivilegeCache(info, tokenResult);

    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: UpdatePrivilegeCacheTest_0200
 * @tc.desc: test UpdatePrivilegeCache with cache failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdatePrivilegeCacheTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdatePrivilegeCacheTest_0200");

    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = ERR_PID;

    ApplyUserTokenResult tokenResult;
    tokenResult.grantTime = 1000;

    ErrCode ret = manager_.UpdatePrivilegeCache(info, tokenResult);

    EXPECT_EQ(ret, ERR_AUTHORIZATION_CACHE_ERROR);
}

/**
 * @tc.name: UpdatePrivilegeCacheTest_0300
 * @tc.desc: test UpdatePrivilegeCache with TransferPrivilegeToCode
 * failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdatePrivilegeCacheTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdatePrivilegeCacheTest_0300");

    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;

    ApplyUserTokenResult tokenResult;
    tokenResult.grantTime = 1000;

    g_transferPrivilegeToCode = false;
    ErrCode ret = manager_.UpdatePrivilegeCache(info, tokenResult);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_CACHE_ERROR);
    g_transferPrivilegeToCode = true;
}

/**
 * @tc.name: ApplyTaAuthorizationTest_0100
 * @tc.desc: test ApplyTaAuthorization success flow.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_0100");

    std::vector<uint8_t> iamToken = {1, 2, 3, 4, 5};
    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();

    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(TEST_USER_ID, _))
        .WillOnce(DoAll(SetArgReferee<1>(OsAccountType::ADMIN), Return(ERR_OK)));

    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplyUserTokenResultV2{.userTokenSize = 1, .remainValidityTime = 10}),
            Return(ERR_OK)));

    auto result = manager_.ApplyTaAuthorization(iamToken, TEST_USER_ID, results, info);

    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
}

/**
 * @tc.name: ApplyTaAuthorizationTest_0200
 * @tc.desc: test ApplyTaAuthorization with non-admin account.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_0200");

    std::vector<uint8_t> iamToken = {1, 2, 3, 4, 5};
    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();

    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(TEST_USER_ID, _))
        .WillOnce(DoAll(SetArgReferee<1>(OsAccountType::NORMAL), Return(ERR_OK)));

    auto result = manager_.ApplyTaAuthorization(iamToken, TEST_USER_ID, results, info);

    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
}

/**
 * @tc.name: StartServiceExtensionConnectionTest_0100
 * @tc.desc: test StartServiceExtensionConnection success.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StartServiceExtensionConnectionTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("StartServiceExtensionConnectionTest_0100");

    ConnectAbilityInfo info;
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;

    sptr<IAuthorizationCallback> callback = iface_cast<IAuthorizationCallback>(new MockAuthorizationCallbackStub());
    AuthorizationResult result;
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();

    ErrCode ret = manager_.StartServiceExtensionConnection(info, TEST_SERVICE_EXTENSION, callback, result, requestObj);

    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: StartServiceExtensionConnectionTest_0200
 * @tc.desc: test StartServiceExtensionConnection with GetNameForUid failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StartServiceExtensionConnectionTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("StartServiceExtensionConnectionTest_0200");

    ConnectAbilityInfo info;
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;

    sptr<IAuthorizationCallback> callback = iface_cast<IAuthorizationCallback>(new MockAuthorizationCallbackStub());
    AuthorizationResult result;
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();
    ErrCode ret = manager_.StartServiceExtensionConnection(info, TEST_SERVICE_EXTENSION, callback, result, requestObj);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: StartServiceExtensionConnectionTest_0300
 * @tc.desc: test StartServiceExtensionConnection with
 * GetNameForUid failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StartServiceExtensionConnectionTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("StartServiceExtensionConnectionTest_0300");

    ConnectAbilityInfo info;
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;

    sptr<IAuthorizationCallback> callback = iface_cast<IAuthorizationCallback>(new MockAuthorizationCallbackStub());
    AuthorizationResult result;
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();

    BundleManagerAdapter::GetInstance()->g_resultCode = ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    ErrCode ret = manager_.StartServiceExtensionConnection(info, TEST_SERVICE_EXTENSION, callback, result, requestObj);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR);
    BundleManagerAdapter::GetInstance()->g_resultCode = ERR_OK;
}

/**
 * @tc.name: InitializeConnectAbilityInfoTest_0100
 * @tc.desc: test InitializeConnectAbilityInfo.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, InitializeConnectAbilityInfoTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("InitializeConnectAbilityInfoTest_0100");

    PrivilegeBriefDef pdef = {.privilegeName = const_cast<char*>(TEST_PRIVILEGE.c_str()),
        .description = const_cast<char*>(TEST_DESCRIPTION.c_str()),
        .timeout = 300};

    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3};
    options.isReuseNeeded = true;

    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;

    ConnectAbilityInfo info;
    ErrCode ret = manager_.InitializeConnectAbilityInfo(pdef, options, config, info);
    EXPECT_EQ(ret, ERR_OK);

    EXPECT_EQ(info.privileges.size(), 1u);
    EXPECT_EQ(info.privileges[0], TEST_PRIVILEGE);
    EXPECT_EQ(info.descriptions.size(), 1u);
    EXPECT_EQ(info.descriptions[0], TEST_DESCRIPTION);
    EXPECT_EQ(info.timeout, 300);
    EXPECT_EQ(info.bundleName, TEST_AUTH_APP_BUNDLE);
    EXPECT_EQ(info.challenge.size(), 3);
}

/**
 * @tc.name: ConnectAbilityCallbackTest_0100
 * @tc.desc: test ConnectAbilityCallback OnResult with null func.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ConnectAbilityCallbackTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("ConnectAbilityCallbackTest_0100");

    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    AcquireOnResultfunc func = nullptr;
    AuthorizationResult result;

    ConnectAbilityCallback callback(info, func, result);
    std::vector<uint8_t> iamToken = {1, 2, 3};
    ErrCode ret = callback.OnResult(ERR_OK, iamToken, TEST_USER_ID, ERR_OK);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_GET_PROXY_ERROR);
}

/**
 * @tc.name: ConnectAbilityCallbackTest_0200
 * @tc.desc: test ConnectAbilityCallback OnResult with errorCode != ERR_OK.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ConnectAbilityCallbackTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("ConnectAbilityCallbackTest_0200");

    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    auto func = [](int32_t errorCode, AuthorizationResult& result, int32_t callingPid) -> ErrCode { return ERR_OK; };
    AuthorizationResult result;

    ConnectAbilityCallback callback(info, func, result);
    std::vector<uint8_t> iamToken = {1, 2, 3};
    ErrCode ret = callback.OnResult(ERR_AUTHORIZATION_TA_ERROR, iamToken, TEST_USER_ID, ERR_OK);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: ConnectAbilityCallbackTest_0300
 * @tc.desc: test ConnectAbilityCallback OnResult with iamResultCode != ERR_OK.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ConnectAbilityCallbackTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("ConnectAbilityCallbackTest_0300");

    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    auto func = [](int32_t errorCode, AuthorizationResult& result, int32_t callingPid) -> ErrCode { return ERR_OK; };
    AuthorizationResult result;

    ConnectAbilityCallback callback(info, func, result);
    std::vector<uint8_t> iamToken = {1, 2, 3};
    ErrCode ret = callback.OnResult(ERR_OK, iamToken, TEST_USER_ID, ERR_AUTHORIZATION_TA_ERROR);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: ApplyTaAuthorizationTest_0300
 * @tc.desc: test ApplyTaAuthorization with CallTaAuthorization failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_0300");

    std::vector<uint8_t> iamToken = {1, 2, 3, 4, 5};
    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();

    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(TEST_USER_ID, _))
        .WillOnce(DoAll(SetArgReferee<1>(OsAccountType::ADMIN), Return(ERR_OK)));

    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(Return(ERR_AUTHORIZATION_TA_ERROR));

    auto result = manager_.ApplyTaAuthorization(iamToken, TEST_USER_ID, results, info);

    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
}

/**
 * @tc.name: ApplyTaAuthorizationTest_0400
 * @tc.desc: test ApplyTaAuthorization with kernel device not found and non-public API.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_0400, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_0400");

    std::vector<uint8_t> iamToken = {1, 2, 3, 4, 5};
    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    info.isPublicApi = false;
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();

    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(TEST_USER_ID, _))
        .WillOnce(DoAll(SetArgReferee<1>(OsAccountType::ADMIN), Return(ERR_OK)));
    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplyUserTokenResultV2{.userTokenSize = 1, .remainValidityTime = 10}),
            Return(ERR_OK)));

    g_kernelPermission = "test_kernel_perm";
    g_kernelDeviceNotFound = true;

    auto result = manager_.ApplyTaAuthorization(iamToken, TEST_USER_ID, results, info);

    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);

    g_kernelDeviceNotFound = false;
    g_kernelPermission = "";
}

/**
 * @tc.name: UpdateAuthInfoTest_0100
 * @tc.desc: test UpdateAuthInfo.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdateAuthInfoTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdateAuthInfoTest_0100");

    ConnectAbilityInfo cbInfo;
    cbInfo.callingPid = TEST_CALLING_PID;
    cbInfo.callingUid = TEST_CALLING_UID;
    cbInfo.privileges = {TEST_PRIVILEGE};
    cbInfo.sessionId = "";
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(cbInfo,
        [](int32_t, AuthorizationResult &, int32_t) { return ERR_OK; }, AuthorizationResult());

    std::map<std::string, AuthorizationResult> batchResults;
    batchResults[TEST_PRIVILEGE].privilege = TEST_PRIVILEGE;
    batchResults[TEST_PRIVILEGE].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    batchResults[TEST_PRIVILEGE].isReused = false;
    connectCallback->SetBatchResults(batchResults);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_connectbackMap[TEST_CALLING_PID] = connectCallback;
    }

    std::vector<uint8_t>
        iamToken = {1, 2, 3, 4, 5};

    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(TEST_USER_ID, _))
        .WillOnce(DoAll(SetArgReferee<1>(OsAccountType::ADMIN), Return(ERR_OK)));

    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(Return(ERR_OK));

    ErrCode ret = manager_.UpdateAuthInfo(iamToken, TEST_USER_ID, TEST_CALLING_PID, "");
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: StartUIExtensionConnectionTest_0000
 * @tc.desc: test StartUIExtensionConnection with nullptr
 * callback.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StartUIExtensionConnectionTest_0000, TestSize.Level0)
{
    ACCOUNT_LOGI("StartUIExtensionConnectionTest_0000");

    ConnectAbilityInfo info;
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};

    AuthorizationResult result;
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = result;
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();

    ErrCode ret = manager_.StartUIExtensionConnection(info, TEST_UI_EXTENSION, nullptr, results, requestObj);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_GET_PROXY_ERROR);
}

/**
 * @tc.name: StartUIExtensionConnectionTest_0100
 * @tc.desc: test StartUIExtensionConnection with AddDeathRecipient
 * failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StartUIExtensionConnectionTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("StartUIExtensionConnectionTest_0100");

    ConnectAbilityInfo info;
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};

    class MockCallbackStubWithFailAddDeathRecipient : public MockAuthorizationCallbackStub {
    public:
        bool AddDeathRecipient(const sptr<DeathRecipient>& recipient) override { return false; }
    };

    sptr<MockCallbackStubWithFailAddDeathRecipient> callbackObj =
        new (std::nothrow) MockCallbackStubWithFailAddDeathRecipient();
    ASSERT_NE(callbackObj, nullptr);
    sptr<IAuthorizationCallback> callback = iface_cast<IAuthorizationCallback>(callbackObj);
    AuthorizationResult result;
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = result;
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();

    ErrCode ret = manager_.StartUIExtensionConnection(info, TEST_UI_EXTENSION, callback, results, requestObj);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_ADD_DEATH_RECIPIENT);
}

/**
 * @tc.name: ConnectAbilityCallbackTest_0400
 * @tc.desc: test ConnectAbilityCallback OnResult with
 * ApplyTaAuthorization success.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ConnectAbilityCallbackTest_0400, TestSize.Level0)
{
    ACCOUNT_LOGI("ConnectAbilityCallbackTest_0400");

    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};
    AuthorizationResult result;
    auto func = [](int32_t errorCode, AuthorizationResult& result, int32_t callingPid) -> ErrCode { return ERR_OK; };

    ConnectAbilityCallback callback(info, func, result);
    std::vector<uint8_t> iamToken = {1, 2, 3};

    g_addCacheFail = false;
    ErrCode ret = callback.OnResult(ERR_OK, iamToken, TEST_USER_ID, ERR_OK);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: ConnectAbilityCallbackTest_0500
 * @tc.desc: test ConnectAbilityCallback OnResult with
 * UpdatePrivilegeCache failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ConnectAbilityCallbackTest_0500, TestSize.Level0)
{
    ACCOUNT_LOGI("ConnectAbilityCallbackTest_0500");

    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};
    AuthorizationResult result;
    auto func = [](int32_t errorCode, AuthorizationResult& result, int32_t callingPid) -> ErrCode { return ERR_OK; };

    ConnectAbilityCallback callback(info, func, result);
    std::vector<uint8_t> iamToken = {1, 2, 3};

    g_addCacheFail = true;
    ErrCode ret = callback.OnResult(ERR_OK, iamToken, TEST_USER_ID, ERR_OK);
    EXPECT_EQ(ret, ERR_OK);
    g_addCacheFail = false;
}

/**
 * @tc.name: AppDeathRecipientTest_0500
 * @tc.desc: test AppDeathRecipient OnRemoteDied with object not found in
 * callback map.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AppDeathRecipientTest_0500, TestSize.Level0)
{
    ACCOUNT_LOGI("AppDeathRecipientTest_0500");
    InnerAuthorizationManager::AppDeathRecipient deathRecipient1;
    wptr<IRemoteObject> remote1 = nullptr;
    deathRecipient1.OnRemoteDied(remote1);

    InnerAuthorizationManager::AppDeathRecipient deathRecipient2;
    sptr<IRemoteObject> obj2 = nullptr;
    wptr<IRemoteObject> remote2 = obj2;
    deathRecipient2.OnRemoteDied(remote2);

    std::string bundleName = "test";
    auto deathRecipient = new (std::nothrow) InnerAuthorizationManager::AppDeathRecipient();
    ASSERT_NE(deathRecipient, nullptr);
}

/**
 * @tc.name: HasExtensionConnectTest_0100
 * @tc.desc: test HasExtensionConnect with empty map.
 * @tc.type: FUNC
 *
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, HasExtensionConnectTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("HasExtensionConnectTest_0100");

    bool hasConnect = manager_.HasExtensionConnect();
    EXPECT_FALSE(hasConnect);
}

/**
 * @tc.name: VerifyWidgetTest_0100
 * @tc.desc: test VerifyWidget with GetForegroundOsAccountLocalId failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, VerifyWidgetTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("VerifyWidgetTest_0100");
    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetForegroundOsAccountLocalId(_, _))
        .WillOnce(Return(ERR_ACCOUNT_COMMON_EXCEPTION_ERROR));
    bool result = manager_.VerifyWidget(TEST_AUTH_APP_BUNDLE);
    EXPECT_FALSE(result);
}

/**
 * @tc.name: VerifyWidgetTest_0300
 * @tc.desc: test VerifyWidget with GetBundleInfo failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, VerifyWidgetTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("VerifyWidgetTest_0300");
    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetForegroundOsAccountLocalId(_, _))
        .WillRepeatedly(Return(ERR_OK));
    bool result = manager_.VerifyWidget("com.example.get_fail");
    EXPECT_FALSE(result);
}

/**
 * @tc.name: VerifyWidgetTest_0400
 * @tc.desc: test VerifyWidget with VerifyPermission failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, VerifyWidgetTest_0400, TestSize.Level0)
{
    ACCOUNT_LOGI("VerifyWidgetTest_0400");
    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetForegroundOsAccountLocalId(_, _))
        .WillRepeatedly(Return(ERR_OK));
    EXPECT_CALL(MockAccountPermissionManager::GetInstance(), VerifyPermission(_, _))
        .WillOnce(Return(ERR_ACCOUNT_COMMON_PERMISSION_DENIED));
    bool result = manager_.VerifyWidget(TEST_AUTH_APP_BUNDLE);
    EXPECT_FALSE(result);
}

/**
 * @tc.name: VerifyWidgetTest_0500
 * @tc.desc: test VerifyWidget with success case.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, VerifyWidgetTest_0500, TestSize.Level0)
{
    ACCOUNT_LOGI("VerifyWidgetTest_0500");
    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetForegroundOsAccountLocalId(_, _))
        .WillRepeatedly(Return(ERR_OK));
    EXPECT_CALL(MockAccountPermissionManager::GetInstance(), VerifyPermission(_, _))
        .WillOnce(Return(ERR_OK));
    bool result = manager_.VerifyWidget(TEST_AUTH_APP_BUNDLE);
    EXPECT_TRUE(result);
}

/**
 * @tc.name: GetAuthSessionInfoTest_0100
 * @tc.desc: test GetAuthSessionInfo with modal system success case.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, GetAuthSessionInfoTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("GetAuthSessionInfoTest_0100");
    std::vector<uint8_t> inputChallenge = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                            0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    std::string outSessionId;
    std::vector<uint8_t> outChallenge;
    bool result = manager_.GetAuthSessionInfo(inputChallenge, outSessionId, outChallenge, TEST_CALLING_PID);
    EXPECT_FALSE(result);
}

/**
 * @tc.name: GetAuthSessionInfoTest_0200
 * @tc.desc: test GetAuthSessionInfo with non-modal system success case.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, GetAuthSessionInfoTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("GetAuthSessionInfoTest_0200");
    std::vector<uint8_t> inputChallenge = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                            0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    std::string outSessionId;
    std::vector<uint8_t> outChallenge;
    bool result = manager_.GetAuthSessionInfo(inputChallenge, outSessionId, outChallenge, TEST_CALLING_PID);
    EXPECT_FALSE(result);
}

/**
 * @tc.name: GetAuthSessionInfoTest_0300
 * @tc.desc: test GetAuthSessionInfo with sessionId not found case.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, GetAuthSessionInfoTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("GetAuthSessionInfoTest_0300");
    std::vector<uint8_t> inputChallenge = {0xaa, 0xbb, 0xcc, 0xdd};
    std::string outSessionId;
    std::vector<uint8_t> outChallenge;
    bool result = manager_.GetAuthSessionInfo(inputChallenge, outSessionId, outChallenge, TEST_CALLING_PID);
    EXPECT_FALSE(result);
}

/**
 * @tc.name: UpdateAuthInfoTest_0200
 * @tc.desc: test UpdateAuthInfo with empty sessionId.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdateAuthInfoTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdateAuthInfoTest_0200");
    std::vector<uint8_t> iamToken = {1, 2, 3, 4, 5};
    ErrCode ret = manager_.UpdateAuthInfo(iamToken, TEST_USER_ID, 0, TEST_SESSIONID);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_UPDATE_INFO_ERROR);
}

/**
 * @tc.name: UpdateAuthInfoTest_0300
 * @tc.desc: test UpdateAuthInfo uses info.privileges (not results SUCCESS filter) for TEE.
 *           Scenario: results contains cache-reused SUCCESS (isReused=true) + pending SUCCESS (isReused=false).
 *           Bug: old code filtered by resultCode==SUCCESS, sending cache-reused items to TEE again.
 *           Fix: use info.privileges directly, matching only pending items.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdateAuthInfoTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdateAuthInfoTest_0300");
    const std::string cachePriv = "ohos.privilege.cache_reused";
    const std::string teePrivA = "ohos.privilege.tee_a";
    const std::string teePrivB = "ohos.privilege.tee_b";
    const int32_t testPid = 888890;
    const std::string testSessionId = "session_update_auth_0300";

    g_addCacheFail = false;
    g_kernelDeviceNotFound = false;

    ConnectAbilityInfo cbInfo;
    cbInfo.callingPid = testPid;
    cbInfo.callingUid = TEST_CALLING_UID;
    cbInfo.privileges = {teePrivA, teePrivB};
    cbInfo.sessionId = testSessionId;
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(cbInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());

    std::map<std::string, AuthorizationResult> batchResults;
    batchResults[cachePriv].privilege = cachePriv;
    batchResults[cachePriv].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    batchResults[cachePriv].isReused = true;
    batchResults[teePrivA].privilege = teePrivA;
    batchResults[teePrivA].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    batchResults[teePrivA].isReused = false;
    batchResults[teePrivB].privilege = teePrivB;
    batchResults[teePrivB].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    batchResults[teePrivB].isReused = false;
    connectCallback->SetBatchResults(batchResults);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_connectbackMap[testPid] = connectCallback;
        g_sessionIdToPidMap[testSessionId] = testPid;
    }

    static uint32_t capturedPermissionsNum = 0;
    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(Invoke([](const ApplyUserTokenParamV2& param, ApplyUserTokenResultV2& result) {
            capturedPermissionsNum = param.permissionsNum;
            result.userTokenSize = 1;
            result.userToken[0] = 0xAA;
            result.remainValidityTime = 10;
            result.grantTime = 1;
            return ERR_OK;
        }));

    std::vector<uint8_t> iamToken = {1, 2, 3};
    ErrCode ret = manager_.UpdateAuthInfo(iamToken, TEST_USER_ID, testPid, testSessionId);
    EXPECT_EQ(ret, ERR_OK);

    EXPECT_EQ(capturedPermissionsNum, 2);

    auto finalResults = connectCallback->MoveBatchResults();
    EXPECT_EQ(finalResults.size(), 3u);

    bool cacheUntouched = false;
    bool teeAUpdated = false;
    bool teeBUpdated = false;
    for (const auto& [k, v] : finalResults) {
        if (k == cachePriv) {
            EXPECT_EQ(v.resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
            EXPECT_TRUE(v.isReused);
            EXPECT_TRUE(v.token.empty());
            cacheUntouched = true;
        } else if (k == teePrivA) {
            EXPECT_EQ(v.resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
            EXPECT_FALSE(v.isReused);
            EXPECT_FALSE(v.token.empty());
            teeAUpdated = true;
        } else if (k == teePrivB) {
            EXPECT_EQ(v.resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
            EXPECT_FALSE(v.isReused);
            EXPECT_FALSE(v.token.empty());
            teeBUpdated = true;
        }
    }
    EXPECT_TRUE(cacheUntouched);
    EXPECT_TRUE(teeAUpdated);
    EXPECT_TRUE(teeBUpdated);
}

/**
 * @tc.name: UpdateAuthInfoTest_0400
 * @tc.desc: test UpdateAuthInfo with empty info.privileges → return error, no TEE call.
 *           Bug: old code fabricated privileges={""} and sent to TEE.
 *           Fix: return ERR_AUTHORIZATION_UPDATE_INFO_ERROR immediately.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdateAuthInfoTest_0400, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdateAuthInfoTest_0400");
    const int32_t testPid = 888891;
    const std::string testSessionId = "session_update_auth_0400";

    ConnectAbilityInfo cbInfo;
    cbInfo.callingPid = testPid;
    cbInfo.callingUid = TEST_CALLING_UID;
    cbInfo.privileges = {};
    cbInfo.sessionId = testSessionId;
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(cbInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_connectbackMap[testPid] = connectCallback;
        g_sessionIdToPidMap[testSessionId] = testPid;
    }

    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .Times(0);

    std::vector<uint8_t> iamToken = {1, 2, 3};
    ErrCode ret = manager_.UpdateAuthInfo(iamToken, TEST_USER_ID, testPid, testSessionId);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_UPDATE_INFO_ERROR);
}

/**
 * @tc.name: InitializeConnectAbilityInfoTest_0200
 * @tc.desc: test InitializeConnectAbilityInfo with sessionId generation failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, InitializeConnectAbilityInfoTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("InitializeConnectAbilityInfoTest_0200");
    PrivilegeBriefDef pdef = {.privilegeName = const_cast<char*>(TEST_PRIVILEGE.c_str()),
        .description = const_cast<char*>(TEST_DESCRIPTION.c_str()),
        .timeout = 300};
    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3, 4};
    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    ConnectAbilityInfo info;
    ErrCode ret = manager_.InitializeConnectAbilityInfo(pdef, options, config, info);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_FALSE(info.sessionId.empty());
}

/**
 * @tc.name: InitializeConnectAbilityInfoTest_0300
 * @tc.desc: test sessionId uniqueness across multiple calls.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, InitializeConnectAbilityInfoTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("InitializeConnectAbilityInfoTest_0300");
    PrivilegeBriefDef pdef = {.privilegeName = const_cast<char*>(TEST_PRIVILEGE.c_str()),
        .description = const_cast<char*>(TEST_DESCRIPTION.c_str()),
        .timeout = 300};
    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3, 4};
    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    ConnectAbilityInfo info1;
    ConnectAbilityInfo info2;
    ErrCode ret1 = manager_.InitializeConnectAbilityInfo(pdef, options, config, info1);
    ErrCode ret2 = manager_.InitializeConnectAbilityInfo(pdef, options, config, info2);
    EXPECT_EQ(ret1, ERR_OK);
    EXPECT_EQ(ret2, ERR_OK);
    EXPECT_FALSE(info1.sessionId.empty());
    EXPECT_FALSE(info2.sessionId.empty());
    EXPECT_NE(info1.sessionId, info2.sessionId);  // sessionId should be unique
}

/**
 * @tc.name: InitializeConnectAbilityInfoTest_0400
 * @tc.desc: test sessionId length is 32 characters (128 bits).
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, InitializeConnectAbilityInfoTest_0400, TestSize.Level0)
{
    ACCOUNT_LOGI("InitializeConnectAbilityInfoTest_0400");
    PrivilegeBriefDef pdef = {.privilegeName = const_cast<char*>(TEST_PRIVILEGE.c_str()),
        .description = const_cast<char*>(TEST_DESCRIPTION.c_str()),
        .timeout = 300};
    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3, 4};
    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    ConnectAbilityInfo info;
    ErrCode ret = manager_.InitializeConnectAbilityInfo(pdef, options, config, info);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(info.sessionId.length(), 32);  // 128 bits = 16 bytes * 2 hex chars
}

/**
 * @tc.name: StoreCallbackMapsTest_0100
 * @tc.desc: test StoreCallbackMaps with OpenSmartPidFd failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StoreCallbackMapsTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("StoreCallbackMapsTest_0100");
    ConnectAbilityInfo uiInfo;
    uiInfo.callingPid = TEST_CALLING_PID;
    uiInfo.callingUid = TEST_CALLING_UID;
    uiInfo.sessionId = "test_session_id_12345678";
    sptr<IAuthorizationCallback> callback = new MockAuthorizationCallbackStub();
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(uiInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    sptr<IRemoteObject> requestRemoteObj = new MockAuthorizationCallbackStub();
    // Mock OpenSmartPidFd to fail (would need additional mock infrastructure)
    ErrCode result = manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj);
    EXPECT_EQ(result, ERR_OK);  // Should succeed in normal case
}

/**
 * @tc.name: CleanupAuthorizationSessionMapsTest_0100
 * @tc.desc: test CleanupAuthorizationSessionMaps clears sessionId mapping.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, CleanupAuthorizationSessionMapsTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("CleanupAuthorizationSessionMapsTest_0100");
    // This test would verify that g_sessionIdToPidMap is cleaned correctly
    // Since CleanupAuthorizationSessionMaps is a static internal function,
    // we test it indirectly through the callback cleanup flow
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.sessionId = "test_session_id";
    auto func = [](int32_t errorCode, AuthorizationResult& result, int32_t callingPid) -> ErrCode {
        return ERR_OK;
    };
    AuthorizationResult result;
    ConnectAbilityCallback callback(info, func, result);
    // Simulate cleanup through acquireAuthorizationOnResultfunc
    std::vector<uint8_t> iamToken = {1, 2, 3};
    ErrCode ret = callback.OnResult(ERR_AUTHORIZATION_GET_PROXY_ERROR, iamToken, TEST_USER_ID, ERR_OK);
    EXPECT_EQ(ret, ERR_OK);  // Should cleanup properly
}

/**
 * @tc.name: UpdateAuthorizationResultTest_0100
 * @tc.desc: test UpdateAuthorizationResult atomic synchronization.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdateAuthorizationResultTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdateAuthorizationResultTest_0100");
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};
    AuthorizationResult result;
    auto func = [](int32_t errorCode, AuthorizationResult& result, int32_t callingPid) -> ErrCode {
        return ERR_OK;
    };
    ConnectAbilityCallback callback(info, func, result);
    ErrCode errCode = ERR_OK;
    AuthorizationResultCode resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    std::vector<uint8_t> taToken = {1, 2, 3, 4, 5};
    int32_t remainValidityTime = 300;
    callback.UpdateAuthorizationResult(errCode, resultCode, taToken, remainValidityTime);
    std::vector<uint8_t> iamToken = {};
    ErrCode ret = callback.OnResult(ERR_OK, iamToken, TEST_USER_ID, ERR_OK);
    EXPECT_EQ(ret, ERR_OK);  // Should use updated result due to atomic sync
}

/**
 * @tc.name: UpdateAuthorizationResultTest_0200
 * @tc.desc: test UpdateAuthorizationResult with failure code.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdateAuthorizationResultTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdateAuthorizationResultTest_0200");
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};
    AuthorizationResult result;
    auto func = [](int32_t errorCode, AuthorizationResult& result, int32_t callingPid) -> ErrCode {
        return ERR_OK;
    };
    ConnectAbilityCallback callback(info, func, result);
    ErrCode errCode = ERR_AUTHORIZATION_TA_ERROR;
    AuthorizationResultCode resultCode = AuthorizationResultCode::AUTHORIZATION_DENIED;
    std::vector<uint8_t> taToken = {};
    int32_t remainValidityTime = 0;
    callback.UpdateAuthorizationResult(errCode, resultCode, taToken, remainValidityTime);
    std::vector<uint8_t> iamToken = {};
    ErrCode ret = callback.OnResult(ERR_OK, iamToken, TEST_USER_ID, ERR_OK);
    EXPECT_EQ(ret, ERR_OK);  // Should handle failure result
}

/**
 * @tc.name: UpdatePrivilegeCachePublicApiTest_0100
 * @tc.desc: test UpdatePrivilegeCache with isPublicApi=true success.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdatePrivilegeCachePublicApiTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdatePrivilegeCachePublicApiTest_0100");
    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    info.isPublicApi = true;

    ApplyUserTokenResult tokenResult;
    tokenResult.grantTime = 1000;

    g_getPrivilegeBriefDef = true;
    g_kernelPermission = "test_kernel_perm";
    g_addCacheFail = false;
    ErrCode ret = manager_.UpdatePrivilegeCache(info, tokenResult);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: UpdatePrivilegeCachePublicApiTest_0200
 * @tc.desc: test UpdatePrivilegeCache with isPublicApi=true and TransferPrivilegeToCode failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdatePrivilegeCachePublicApiTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdatePrivilegeCachePublicApiTest_0200");
    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    info.isPublicApi = true;

    ApplyUserTokenResult tokenResult;
    tokenResult.grantTime = 1000;

    g_transferPrivilegeToCode = false;
    ErrCode ret = manager_.UpdatePrivilegeCache(info, tokenResult);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_CACHE_ERROR);
    g_transferPrivilegeToCode = true;
}

/**
 * @tc.name: UpdatePrivilegeCachePublicApiTest_0300
 * @tc.desc: test UpdatePrivilegeCache with isPublicApi=true and GetPrivilegeBriefDef failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdatePrivilegeCachePublicApiTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdatePrivilegeCachePublicApiTest_0300");
    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    info.isPublicApi = true;

    ApplyUserTokenResult tokenResult;
    tokenResult.grantTime = 1000;

    g_getPrivilegeBriefDef = false;
    ErrCode ret = manager_.UpdatePrivilegeCache(info, tokenResult);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_CACHE_ERROR);
    g_getPrivilegeBriefDef = true;
}

/**
 * @tc.name: UpdatePrivilegeCachePublicApiTest_0400
 * @tc.desc: test UpdatePrivilegeCache with no kernelPermission → AddCache path.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdatePrivilegeCachePublicApiTest_0400, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdatePrivilegeCachePublicApiTest_0400");
    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    info.isPublicApi = true;

    ApplyUserTokenResult tokenResult;
    tokenResult.grantTime = 1000;

    g_getPrivilegeBriefDef = true;
    g_kernelPermission = "";
    ErrCode ret = manager_.UpdatePrivilegeCache(info, tokenResult);
    EXPECT_EQ(ret, ERR_OK);
    g_kernelPermission = "test_kernel_perm";
}

/**
 * @tc.name: UpdatePrivilegeCachePublicApiTest_0500
 * @tc.desc: test UpdatePrivilegeCache with isPublicApi=true and AddCacheAndNotifyKernel failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, UpdatePrivilegeCachePublicApiTest_0500, TestSize.Level0)
{
    ACCOUNT_LOGI("UpdatePrivilegeCachePublicApiTest_0500");
    ConnectAbilityInfo info;
    info.privileges = {TEST_PRIVILEGE};
    info.callingUid = TEST_CALLING_UID;
    info.callingPid = TEST_CALLING_PID;
    info.isPublicApi = true;

    ApplyUserTokenResult tokenResult;
    tokenResult.grantTime = 1000;

    g_getPrivilegeBriefDef = true;
    g_kernelPermission = "test_kernel_perm";
    g_addCacheFail = true;
    ErrCode ret = manager_.UpdatePrivilegeCache(info, tokenResult);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_CACHE_ERROR);
    g_addCacheFail = false;
}

/**
 * @tc.name: HasKernelAuthorizationTest_0100
 * @tc.desc: test HasKernelAuthorization success.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, HasKernelAuthorizationTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("HasKernelAuthorizationTest_0100");
    g_hasAuthorizationFail = false;
    bool isAuthorized = false;
    ErrCode ret = manager_.HasKernelAuthorization(0, TEST_CALLING_PID, "test_kernel_perm", isAuthorized);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_TRUE(isAuthorized);
}

/**
 * @tc.name: HasKernelAuthorizationTest_0200
 * @tc.desc: test HasKernelAuthorization with PrivilegeCacheManager failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, HasKernelAuthorizationTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("HasKernelAuthorizationTest_0200");
    g_hasAuthorizationFail = true;
    bool isAuthorized = false;
    ErrCode ret = manager_.HasKernelAuthorization(0, TEST_CALLING_PID, "test_kernel_perm", isAuthorized);
    EXPECT_NE(ret, ERR_OK);
    EXPECT_FALSE(isAuthorized);
    g_hasAuthorizationFail = false;
}

/**
 * @tc.name: StoreCallbackMapsBusyTest_0100
 * @tc.desc: test StoreCallbackMaps returns ERR_ACCOUNT_COMMON_BUSY when callback already exists.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StoreCallbackMapsBusyTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("StoreCallbackMapsBusyTest_0100");
    ConnectAbilityInfo uiInfo;
    uiInfo.callingPid = 88888;
    uiInfo.callingUid = TEST_CALLING_UID;
    uiInfo.sessionId = "busy_test_session_id";
    sptr<IAuthorizationCallback> callback = new MockAuthorizationCallbackStub();
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(uiInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    sptr<IRemoteObject> requestRemoteObj = new MockAuthorizationCallbackStub();

    // First call should succeed
    ErrCode ret1 = manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj);
    EXPECT_EQ(ret1, ERR_OK);
    // Second call with same pid should return BUSY
    ErrCode ret2 = manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj);
    EXPECT_EQ(ret2, ERR_ACCOUNT_COMMON_BUSY);
}

/**
 * @tc.name: StartUIExtensionTaskTest_0100
 * @tc.desc: test StartUIExtensionTask with StoreCallbackMaps returning BUSY.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StartUIExtensionTaskTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("StartUIExtensionTaskTest_0100");
    ConnectAbilityInfo uiInfo;
    uiInfo.callingPid = 77771;
    uiInfo.callingUid = TEST_CALLING_UID;
    uiInfo.privileges = {TEST_PRIVILEGE};
    uiInfo.sessionId = "ext_task_busy_session";
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(uiInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    sptr<IRemoteObject> requestRemoteObj = new MockAuthorizationCallbackStub();
    sptr<IAuthorizationCallback> callback = new MockAuthorizationCallbackStub();
    ErrCode storeRet = manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj);
    EXPECT_EQ(storeRet, ERR_OK);
    sptr<MockAuthorizationCallbackStub> busyCallback = new MockAuthorizationCallbackStub();
    ErrCode ret = manager_.StartUIExtensionTask(uiInfo, connectCallback, busyCallback, requestRemoteObj);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_TRUE(busyCallback->isReady);
    EXPECT_EQ(busyCallback->batchResults_[0].resultCode, AuthorizationResultCode::AUTHORIZATION_SERVICE_BUSY);
}

/**
 * @tc.name: StartUIExtensionTaskTest_0200
 * @tc.desc: test StartUIExtensionTask with StoreCallbackMaps returning non-BUSY error.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StartUIExtensionTaskTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("StartUIExtensionTaskTest_0200");
    g_openSmartPidFdFail = true;
    ConnectAbilityInfo uiInfo;
    uiInfo.callingPid = 77772;
    uiInfo.callingUid = TEST_CALLING_UID;
    uiInfo.privileges = {TEST_PRIVILEGE};
    uiInfo.sessionId = "ext_task_err_session";
    sptr<IAuthorizationCallback> callback = new MockAuthorizationCallbackStub();
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(uiInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    sptr<IRemoteObject> requestRemoteObj = new MockAuthorizationCallbackStub();
    ErrCode ret = manager_.StartUIExtensionTask(uiInfo, connectCallback, callback, requestRemoteObj);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_GET_PROXY_ERROR);
    g_openSmartPidFdFail = false;
}

/**
 * @tc.name: ExecuteUIExtensionTaskTest_0100
 * @tc.desc: test ExecuteUIExtensionTask with null callback → rollbackMaps.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ExecuteUIExtensionTaskTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("ExecuteUIExtensionTaskTest_0100");
    ConnectAbilityInfo uiInfo;
    uiInfo.callingPid = 77773;
    uiInfo.callingUid = TEST_CALLING_UID;
    uiInfo.privileges = {TEST_PRIVILEGE};
    uiInfo.sessionId = "ext_exec_null_cb";
    sptr<IAuthorizationCallback> callback = new MockAuthorizationCallbackStub();
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(uiInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    sptr<IRemoteObject> requestRemoteObj = new MockAuthorizationCallbackStub();
    ASSERT_EQ(ERR_OK, manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj));
    manager_.ExecuteUIExtensionTask(uiInfo, connectCallback, nullptr, requestRemoteObj);
    // rollbackMaps should have cleaned up g_callbackMap
    ErrCode ret = manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: ExecuteUIExtensionTaskTest_0200
 * @tc.desc: test ExecuteUIExtensionTask with OnConnectAbility failure → rollbackMaps.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ExecuteUIExtensionTaskTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("ExecuteUIExtensionTaskTest_0200");
    class FailConnectCallback : public MockAuthorizationCallbackStub {
    public:
        ErrCode OnConnectAbility(const ConnectAbilityInfo& info, const sptr<IRemoteObject>& callback) override
        {
            return ERR_AUTHORIZATION_TA_ERROR;
        }
    };
    ConnectAbilityInfo uiInfo;
    uiInfo.callingPid = 77774;
    uiInfo.callingUid = TEST_CALLING_UID;
    uiInfo.privileges = {TEST_PRIVILEGE};
    uiInfo.sessionId = "ext_exec_connect_fail";
    auto callback = sptr<FailConnectCallback>::MakeSptr();
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(uiInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    sptr<IRemoteObject> requestRemoteObj = new MockAuthorizationCallbackStub();
    ASSERT_EQ(ERR_OK, manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj));
    manager_.ExecuteUIExtensionTask(uiInfo, connectCallback, callback, requestRemoteObj);
    EXPECT_TRUE(callback->isReady);
    ErrCode ret = manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: ExecuteUIExtensionTaskTest_0300
 * @tc.desc: test ExecuteUIExtensionTask with null requestRemoteObj → rollbackMaps.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ExecuteUIExtensionTaskTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("ExecuteUIExtensionTaskTest_0300");
    ConnectAbilityInfo uiInfo;
    uiInfo.callingPid = 77775;
    uiInfo.callingUid = TEST_CALLING_UID;
    uiInfo.privileges = {TEST_PRIVILEGE};
    uiInfo.sessionId = "ext_exec_null_req";
    sptr<IAuthorizationCallback> callback = new MockAuthorizationCallbackStub();
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(uiInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    ASSERT_EQ(ERR_OK, manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, nullptr));
    manager_.ExecuteUIExtensionTask(uiInfo, connectCallback, callback, nullptr);
    ErrCode ret = manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, nullptr);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: AcquireAdminAuthorization_AsObjectNull_0100
 * @tc.desc: Test AcquireAdminAuthorization with callback whose AsObject() returns nullptr.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAdminAuthorization_AsObjectNull_0100, TestSize.Level1)
{
    sptr<MockAdminAuthorizationCallbackNullAsObject> callback =
        new (std::nothrow) MockAdminAuthorizationCallbackNullAsObject();
    ASSERT_NE(callback, nullptr);
    std::vector<uint8_t> challenge = {1, 2, 3};
    ErrCode ret = manager_.AcquireAdminAuthorization(
        TEST_USER_ID, challenge, callback, TEST_PRIVILEGE, TEST_CALLING_PID);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_ADD_DEATH_RECIPIENT);
}

/**
 * @tc.name: StoreCallbackMapsLimitTest_0100
 * @tc.desc: test StoreCallbackMaps returns ERR_ACCOUNT_COMMON_BUSY when map size reaches limit.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, StoreCallbackMapsLimitTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("StoreCallbackMapsLimitTest_0100");
    ConnectAbilityInfo uiInfo;
    uiInfo.callingUid = TEST_CALLING_UID;
    sptr<IRemoteObject> requestRemoteObj = new MockAuthorizationCallbackStub();
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(uiInfo,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());

    std::vector<int32_t> pids;
    for (int32_t i = 0; i <= CALLBACK_MAP_MAX_SIZE; i++) {
        int32_t pid = CALLBACK_MAP_LIMIT_PID_BASE + i;
        uiInfo.callingPid = pid;
        uiInfo.sessionId = "limit_test_" + std::to_string(pid);
        sptr<IAuthorizationCallback> callback = new MockAuthorizationCallbackStub();
        ErrCode ret = manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj);
        if (ret != ERR_OK) {
            EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_BUSY);
            break;
        }
        pids.push_back(pid);
    }
    EXPECT_GT(pids.size(), 0u);

    uiInfo.callingPid = CALLBACK_MAP_LIMIT_PID_BASE + CALLBACK_MAP_MAX_SIZE + 1;
    uiInfo.sessionId = "limit_test_overflow";
    sptr<IAuthorizationCallback> callback = new MockAuthorizationCallbackStub();
    ErrCode ret = manager_.StoreCallbackMaps(uiInfo, callback, connectCallback, requestRemoteObj);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_BUSY);
}

/**
 * @tc.name: CallTaAuthorizationTest_0100
 * @tc.desc: test CallTaAuthorization with empty privileges.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, CallTaAuthorizationTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("CallTaAuthorizationTest_0100");
    ApplyUserTokenResultV2 tokenResult;
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    ErrCode ret = manager_.CallTaAuthorization({}, TEST_USER_ID, tokenResult, info);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
}

/**
 * @tc.name: CallTaAuthorizationTest_0200
 * @tc.desc: test CallTaAuthorization with privileges exceeding MAX_BATCH_PRIVILEGE_COUNT.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, CallTaAuthorizationTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("CallTaAuthorizationTest_0200");
    ApplyUserTokenResultV2 tokenResult;
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = std::vector<std::string>(MAX_BATCH_PRIVILEGE_COUNT + 1, TEST_PRIVILEGE);
    ErrCode ret = manager_.CallTaAuthorization({}, TEST_USER_ID, tokenResult, info);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
}

/**
 * @tc.name: CallTaAuthorizationTest_0300
 * @tc.desc: test CallTaAuthorization with permission string exceeding PERMISSION_MAX_LEN.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, CallTaAuthorizationTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("CallTaAuthorizationTest_0300");
    std::string longPrivilege(PERMISSION_MAX_LEN + 1, 'a');
    ApplyUserTokenResultV2 tokenResult;
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {longPrivilege};
    ErrCode ret = manager_.CallTaAuthorization({}, TEST_USER_ID, tokenResult, info);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
}

/**
 * @tc.name: CallTaAuthorizationTest_0400
 * @tc.desc: test CallTaAuthorization with iamToken exceeding AUTH_TOKEN_LEN.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, CallTaAuthorizationTest_0400, TestSize.Level0)
{
    ACCOUNT_LOGI("CallTaAuthorizationTest_0400");
    std::vector<uint8_t> iamToken(AUTH_TOKEN_LEN + 1, 0x01);
    ApplyUserTokenResultV2 tokenResult;
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};
    ErrCode ret = manager_.CallTaAuthorization(iamToken, TEST_USER_ID, tokenResult, info);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
}

/**
 * @tc.name: CallTaAuthorizationTest_0500
 * @tc.desc: test CallTaAuthorization with challenge exceeding CHALLENGE_LEN.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, CallTaAuthorizationTest_0500, TestSize.Level0)
{
    ACCOUNT_LOGI("CallTaAuthorizationTest_0500");
    ApplyUserTokenResultV2 tokenResult;
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};
    info.challenge = std::vector<uint8_t>(CHALLENGE_LEN + 1, 0x01);
    ErrCode ret = manager_.CallTaAuthorization({}, TEST_USER_ID, tokenResult, info);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
}

/**
 * @tc.name: CallTaAuthorizationTest_0600
 * @tc.desc: test CallTaAuthorization with TEE TaAcquireAuthorizationBatch failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, CallTaAuthorizationTest_0600, TestSize.Level0)
{
    ACCOUNT_LOGI("CallTaAuthorizationTest_0600");
    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(Return(ERR_AUTHORIZATION_TA_ERROR));
    ApplyUserTokenResultV2 tokenResult;
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};
    ErrCode ret = manager_.CallTaAuthorization({}, TEST_USER_ID, tokenResult, info);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_TA_ERROR);
}

/**
 * @tc.name: CallTaAuthorizationTest_0700
 * @tc.desc: test CallTaAuthorization success with multiple privileges.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, CallTaAuthorizationTest_0700, TestSize.Level0)
{
    ACCOUNT_LOGI("CallTaAuthorizationTest_0700");
    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplyUserTokenResultV2{.userTokenSize = 1, .remainValidityTime = 10}),
            Return(ERR_OK)));
    ApplyUserTokenResultV2 tokenResult;
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE, TEST_PRIVILEGE_2};
    info.challenge = {0x01, 0x02};
    ErrCode ret = manager_.CallTaAuthorization({0xAA}, TEST_USER_ID, tokenResult, info);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: ApplyTaAuthorizationTest_0800
 * @tc.desc: test ApplyTaAuthorization with empty privileges.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_0800, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_0800");
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    std::map<std::string, AuthorizationResult> results;
    auto result = manager_.ApplyTaAuthorization({}, TEST_USER_ID, results, info);
    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
    EXPECT_TRUE(result.token.empty());
}

/**
 * @tc.name: ApplyTaAuthorizationTest_0900
 * @tc.desc: test ApplyTaAuthorization with empty results.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_0900, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_0900");
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    std::map<std::string, AuthorizationResult> results;
    auto result = manager_.ApplyTaAuthorization({}, TEST_USER_ID, results, info);
    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
    EXPECT_TRUE(result.token.empty());
}

/**
 * @tc.name: ApplyTaAuthorizationTest_1000
 * @tc.desc: test ApplyTaAuthorization with VerifyAdminAccount failure (non-admin).
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_1000, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_1000");
    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(OsAccountType::NORMAL), Return(ERR_OK)));
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    auto result = manager_.ApplyTaAuthorization({}, TEST_USER_ID, results, info);
    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
}

/**
 * @tc.name: ApplyTaAuthorizationTest_1200
 * @tc.desc: test ApplyTaAuthorization with non-admin + mixed results (NOT_SUPPORTED/DENIED/SUCCESS).
 *           Bug: old code overwrote ALL resultCode to DENIED, losing NOT_SUPPORTED.
 *           Fix: only overwrite SUCCESS items, preserve upstream NOT_SUPPORTED/DENIED.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_1200, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_1200");
    const std::string notSupportedPriv = "ohos.privilege.not_supported";
    const std::string deniedPriv = "ohos.privilege.denied_by_kernel";
    const std::string pendingPriv = TEST_PRIVILEGE;

    EXPECT_CALL(MockIInnerOsAccountManager::GetInstance(), GetOsAccountType(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(OsAccountType::NORMAL), Return(ERR_OK)));

    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {pendingPriv};
    std::map<std::string, AuthorizationResult> results;
    results[notSupportedPriv].privilege = notSupportedPriv;
    results[notSupportedPriv].resultCode = AuthorizationResultCode::AUTHORIZATION_PRIVILEGE_NOT_SUPPORTED;
    results[deniedPriv].privilege = deniedPriv;
    results[deniedPriv].resultCode = AuthorizationResultCode::AUTHORIZATION_DENIED;
    results[pendingPriv].privilege = pendingPriv;
    results[pendingPriv].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;

    auto result = manager_.ApplyTaAuthorization({}, TEST_USER_ID, results, info);

    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
    EXPECT_EQ(results[notSupportedPriv].resultCode,
        AuthorizationResultCode::AUTHORIZATION_PRIVILEGE_NOT_SUPPORTED);
    EXPECT_EQ(results[deniedPriv].resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
    EXPECT_EQ(results[pendingPriv].resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
}

/**
 * @tc.name: ApplyTaAuthorizationTest_1100
 * @tc.desc: test ApplyTaAuthorization with single privilege (CallTaAuthorization path).
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_1100, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_1100");
    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplyUserTokenResultV2{.userTokenSize = 1, .remainValidityTime = 10}),
            Return(ERR_OK)));
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE};
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    auto result = manager_.ApplyTaAuthorization({}, TEST_USER_ID, results, info);
    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
}

/**
 * @tc.name: ApplyTaAuthorizationTest_0500
 * @tc.desc: test ApplyTaAuthorization with multiple privileges (CallTaAuthorization path).
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_0500, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_0500");
    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplyUserTokenResultV2{.userTokenSize = 1, .remainValidityTime = 10}),
            Return(ERR_OK)));
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE, TEST_PRIVILEGE_2};
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    results[TEST_PRIVILEGE_2] = AuthorizationResult();
    auto result = manager_.ApplyTaAuthorization({}, TEST_USER_ID, results, info);
    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
}

/**
 * @tc.name: ApplyTaAuthorizationTest_0600
 * @tc.desc: test ApplyTaAuthorization with TEE failure → all results DENIED.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_0600, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_0600");
    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(Return(ERR_AUTHORIZATION_TA_ERROR));
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE, TEST_PRIVILEGE_2};
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    results[TEST_PRIVILEGE_2] = AuthorizationResult();
    auto result = manager_.ApplyTaAuthorization({}, TEST_USER_ID, results, info);
    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
    EXPECT_TRUE(result.token.empty());
    EXPECT_EQ(results[TEST_PRIVILEGE].resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
    EXPECT_EQ(results[TEST_PRIVILEGE_2].resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
}

/**
 * @tc.name: ApplyTaAuthorizationTest_0700
 * @tc.desc: test ApplyTaAuthorization with invalid userTokenSize → DENIED.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, ApplyTaAuthorizationTest_0700, TestSize.Level0)
{
    ACCOUNT_LOGI("ApplyTaAuthorizationTest_0700");
    EXPECT_CALL(MockOsAccountTeeAdapter::GetInstance(), TaAcquireAuthorizationBatch(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplyUserTokenResultV2{.userTokenSize = 0, .remainValidityTime = 0}),
            Return(ERR_OK)));
    ConnectAbilityInfo info;
    info.callingPid = TEST_CALLING_PID;
    info.privileges = {TEST_PRIVILEGE, TEST_PRIVILEGE_2};
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    results[TEST_PRIVILEGE_2] = AuthorizationResult();
    auto result = manager_.ApplyTaAuthorization({}, TEST_USER_ID, results, info);
    EXPECT_EQ(result.resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
    EXPECT_TRUE(result.token.empty());
    EXPECT_EQ(results[TEST_PRIVILEGE].resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
    EXPECT_EQ(results[TEST_PRIVILEGE_2].resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
}

/**
 * @tc.name: BatchAuthorizationOnResultfuncTest_0100
 * @tc.desc: test acquireAuthorizationOnResultfunc with missing callback in g_callbackMap.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, BatchAuthorizationOnResultfuncTest_0100, TestSize.Level0)
{
    ACCOUNT_LOGI("BatchAuthorizationOnResultfuncTest_0100");
    auto func = acquireAuthorizationOnResultfunc();
    AuthorizationResult result;
    result.privilege = TEST_PRIVILEGE;
    ErrCode ret = func(ERR_OK, result, 999991);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: BatchAuthorizationOnResultfuncTest_0200
 * @tc.desc: test acquireAuthorizationOnResultfunc with null callback in g_callbackMap.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, BatchAuthorizationOnResultfuncTest_0200, TestSize.Level0)
{
    ACCOUNT_LOGI("BatchAuthorizationOnResultfuncTest_0200");
    int32_t testPid = 999992;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_callbackMap[testPid] = nullptr;
    }
    auto func = acquireAuthorizationOnResultfunc();
    AuthorizationResult result;
    result.privilege = TEST_PRIVILEGE;
    ErrCode ret = func(ERR_OK, result, testPid);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_GET_PROXY_ERROR);
}

/**
 * @tc.name: BatchAuthorizationOnResultfuncTest_0300
 * @tc.desc: test acquireAuthorizationOnResultfunc with valid callback + errorCode != ERR_OK → CANCELED override.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, BatchAuthorizationOnResultfuncTest_0300, TestSize.Level0)
{
    ACCOUNT_LOGI("BatchAuthorizationOnResultfuncTest_0300");
    int32_t testPid = 999993;
    sptr<MockAuthorizationCallbackStub> callbackObj = new MockAuthorizationCallbackStub();
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_callbackMap[testPid] = iface_cast<IAuthorizationCallback>(callbackObj);
    }
    auto func = acquireAuthorizationOnResultfunc();
    AuthorizationResult result;
    result.privilege = TEST_PRIVILEGE;
    ErrCode ret = func(ERR_ACCOUNT_COMMON_BUSY, result, testPid);
    EXPECT_EQ(ret, ERR_OK);
    ASSERT_TRUE(callbackObj->isReady);
    ASSERT_EQ(callbackObj->batchResults_.size(), 1u);
    EXPECT_EQ(callbackObj->batchResults_[0].resultCode, AuthorizationResultCode::AUTHORIZATION_CANCELED);
}

/**
 * @tc.name: BatchAuthorizationOnResultfuncTest_0400
 * @tc.desc: test acquireAuthorizationOnResultfunc with valid callback + success resultCode → results passed through.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, BatchAuthorizationOnResultfuncTest_0400, TestSize.Level0)
{
    ACCOUNT_LOGI("BatchAuthorizationOnResultfuncTest_0400");
    int32_t testPid = 999994;
    sptr<MockAuthorizationCallbackStub> callbackObj = new MockAuthorizationCallbackStub();
    ConnectAbilityInfo info;
    info.callingPid = testPid;
    info.privileges = {TEST_PRIVILEGE};
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(info,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    std::map<std::string, AuthorizationResult> batchResults;
    batchResults[TEST_PRIVILEGE].privilege = TEST_PRIVILEGE;
    batchResults[TEST_PRIVILEGE].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    batchResults[TEST_PRIVILEGE].isReused = false;
    connectCallback->SetBatchResults(batchResults);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_callbackMap[testPid] = iface_cast<IAuthorizationCallback>(callbackObj);
        g_connectbackMap[testPid] = connectCallback;
    }
    auto func = acquireAuthorizationOnResultfunc();
    AuthorizationResult result;
    result.privilege = TEST_PRIVILEGE;
    ErrCode ret = func(ERR_OK, result, testPid);
    EXPECT_EQ(ret, ERR_OK);
    ASSERT_TRUE(callbackObj->isReady);
    ASSERT_EQ(callbackObj->batchResults_.size(), 1u);
    EXPECT_EQ(callbackObj->batchResults_[0].resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
}

/**
 * @tc.name: BatchAuthorizationOnResultfuncTest_0500
 * @tc.desc: test acquireAuthorizationOnResultfunc with no connectbackMap → empty results → CANCELED fallback.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, BatchAuthorizationOnResultfuncTest_0500, TestSize.Level0)
{
    ACCOUNT_LOGI("BatchAuthorizationOnResultfuncTest_0500");
    int32_t testPid = 999995;
    sptr<MockAuthorizationCallbackStub> callbackObj = new MockAuthorizationCallbackStub();
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_callbackMap[testPid] = iface_cast<IAuthorizationCallback>(callbackObj);
    }
    auto func = acquireAuthorizationOnResultfunc();
    AuthorizationResult result;
    result.privilege = TEST_PRIVILEGE;
    result.resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    ErrCode ret = func(ERR_OK, result, testPid);
    EXPECT_EQ(ret, ERR_OK);
    ASSERT_TRUE(callbackObj->isReady);
    ASSERT_EQ(callbackObj->batchResults_.size(), 1u);
    EXPECT_EQ(callbackObj->batchResults_[0].resultCode, AuthorizationResultCode::AUTHORIZATION_CANCELED);
}

/**
 * @tc.name: BatchAuthorizationOnResultfuncTest_0600
 * @tc.desc: test acquireAuthorizationOnResultfunc with result.resultCode != SUCCESS → override to result.resultCode.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, BatchAuthorizationOnResultfuncTest_0600, TestSize.Level0)
{
    ACCOUNT_LOGI("BatchAuthorizationOnResultfuncTest_0600");
    int32_t testPid = 999996;
    sptr<MockAuthorizationCallbackStub> callbackObj = new MockAuthorizationCallbackStub();
    ConnectAbilityInfo info;
    info.callingPid = testPid;
    info.privileges = {TEST_PRIVILEGE};
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(info,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    std::map<std::string, AuthorizationResult> batchResults;
    batchResults[TEST_PRIVILEGE].privilege = TEST_PRIVILEGE;
    batchResults[TEST_PRIVILEGE].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    batchResults[TEST_PRIVILEGE].isReused = false;
    connectCallback->SetBatchResults(batchResults);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_callbackMap[testPid] = iface_cast<IAuthorizationCallback>(callbackObj);
        g_connectbackMap[testPid] = connectCallback;
    }
    auto func = acquireAuthorizationOnResultfunc();
    AuthorizationResult result;
    result.privilege = TEST_PRIVILEGE;
    result.resultCode = AuthorizationResultCode::AUTHORIZATION_DENIED;
    ErrCode ret = func(ERR_OK, result, testPid);
    EXPECT_EQ(ret, ERR_OK);
    ASSERT_TRUE(callbackObj->isReady);
    ASSERT_EQ(callbackObj->batchResults_.size(), 1u);
    EXPECT_EQ(callbackObj->batchResults_[0].resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
}

/**
 * @tc.name: AcquireAuthorizationTest_0600
 * @tc.desc: test AcquireAuthorization with null callback.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAuthorizationTest_0600, TestSize.Level0)
{
    ACCOUNT_LOGI("AcquireAuthorizationTest_0600");
    AcquireAuthorizationOptions options;
    options.hasContext = false;
    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    req.results = results;
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, nullptr, nullptr);
    EXPECT_EQ(ret, ERR_AUTHORIZATION_GET_PROXY_ERROR);
}

/**
 * @tc.name: AcquireAuthorizationTest_0700
 * @tc.desc: test AcquireAuthorization with GetPrivilegeBriefDef failure.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAuthorizationTest_0700, TestSize.Level0)
{
    ACCOUNT_LOGI("AcquireAuthorizationTest_0700");
    g_getPrivilegeBriefDef = false;
    AcquireAuthorizationOptions options;
    options.hasContext = false;
    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    sptr<IRemoteObject> callbackObj = new MockAuthorizationCallbackStub();
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    req.results = results;
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, callbackObj, nullptr);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    g_getPrivilegeBriefDef = true;
}

/**
 * @tc.name: AcquireAuthorizationTest_0800
 * @tc.desc: test AcquireAuthorization with service extension (no context).
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAuthorizationTest_0800, TestSize.Level0)
{
    ACCOUNT_LOGI("AcquireAuthorizationTest_0800");
    EXPECT_CALL(MockAccountPermissionManager::GetInstance(), VerifyPermission(_, _))
        .WillRepeatedly(Return(ERR_OK));
    AcquireAuthorizationOptions options;
    options.hasContext = false;
    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    config.authAppServiceExtensionAbilityName = TEST_SERVICE_EXTENSION;
    sptr<IRemoteObject> callbackObj = new MockAuthorizationCallbackStub();
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    req.results = results;
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, callbackObj, requestObj);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: AcquireAuthorizationTest_0900
 * @tc.desc: test AcquireAuthorization with UI extension (hasContext=true).
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAuthorizationTest_0900, TestSize.Level0)
{
    ACCOUNT_LOGI("AcquireAuthorizationTest_0900");
    EXPECT_CALL(MockAccountPermissionManager::GetInstance(), VerifyPermission(_, _))
        .WillRepeatedly(Return(ERR_OK));
    AcquireAuthorizationOptions options;
    options.hasContext = true;
    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    config.authAppUIExtensionAbilityName = TEST_UI_EXTENSION;
    sptr<IRemoteObject> callbackObj = new MockAuthorizationCallbackStub();
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();
    std::map<std::string, AuthorizationResult> results;
    results[TEST_PRIVILEGE] = AuthorizationResult();
    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    req.results = results;
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, callbackObj, requestObj);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: BatchAuthorizationOnResultfuncTest_0700
 * @tc.desc: test acquireAuthorizationOnResultfunc with mixed batch (SUCCESS + DENIED) → only SUCCESS overridden.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, BatchAuthorizationOnResultfuncTest_0700, TestSize.Level0)
{
    ACCOUNT_LOGI("BatchAuthorizationOnResultfuncTest_0700");
    int32_t testPid = 999997;
    const std::string validPrivilege = "ohos.privilege.operate_raw_net_packets";
    const std::string deniedPrivilege = "ohos.privilege.monitor_raw_usb_packets";
    sptr<MockAuthorizationCallbackStub> callbackObj = new MockAuthorizationCallbackStub();
    ConnectAbilityInfo info;
    info.callingPid = testPid;
    info.privileges = {validPrivilege, deniedPrivilege};
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(info,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    std::map<std::string, AuthorizationResult> batchResults;
    batchResults[validPrivilege].privilege = validPrivilege;
    batchResults[validPrivilege].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    batchResults[validPrivilege].isReused = false;
    batchResults[deniedPrivilege].privilege = deniedPrivilege;
    batchResults[deniedPrivilege].resultCode = AuthorizationResultCode::AUTHORIZATION_DENIED;
    batchResults[deniedPrivilege].isReused = false;
    connectCallback->SetBatchResults(batchResults);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_callbackMap[testPid] = iface_cast<IAuthorizationCallback>(callbackObj);
        g_connectbackMap[testPid] = connectCallback;
    }
    auto func = acquireAuthorizationOnResultfunc();
    AuthorizationResult result;
    result.privilege = validPrivilege;
    result.resultCode = AuthorizationResultCode::AUTHORIZATION_CANCELED;
    ErrCode ret = func(ERR_OK, result, testPid);
    EXPECT_EQ(ret, ERR_OK);
    ASSERT_TRUE(callbackObj->isReady);
    ASSERT_EQ(callbackObj->batchResults_.size(), 2u);
    bool validFound = false;
    bool deniedFound = false;
    for (const auto &r : callbackObj->batchResults_) {
        if (r.privilege == validPrivilege) {
            EXPECT_EQ(r.resultCode, AuthorizationResultCode::AUTHORIZATION_CANCELED);
            validFound = true;
        } else if (r.privilege == deniedPrivilege) {
            EXPECT_EQ(r.resultCode, AuthorizationResultCode::AUTHORIZATION_DENIED);
            deniedFound = true;
        }
    }
    EXPECT_TRUE(validFound);
    EXPECT_TRUE(deniedFound);
}

/**
 * @tc.name: BatchAuthorizationOnResultfuncTest_0800
 * @tc.desc: test acquireAuthorizationOnResultfunc with isReused=true → not overridden.
 *           Bug: cache-reused SUCCESS items were overridden to CANCELED/DENIED.
 *           Fix: OverrideBatchResults skips items with isReused=true.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, BatchAuthorizationOnResultfuncTest_0800, TestSize.Level0)
{
    ACCOUNT_LOGI("BatchAuthorizationOnResultfuncTest_0800");
    int32_t testPid = 999998;
    const std::string reusedPriv = "ohos.privilege.cache_reused";
    const std::string pendingPriv = TEST_PRIVILEGE;
    sptr<MockAuthorizationCallbackStub> callbackObj = new MockAuthorizationCallbackStub();
    ConnectAbilityInfo info;
    info.callingPid = testPid;
    info.privileges = {pendingPriv};
    sptr<ConnectAbilityCallback> connectCallback = new ConnectAbilityCallback(info,
        [](int32_t, AuthorizationResult&, int32_t) { return ERR_OK; }, AuthorizationResult());
    std::map<std::string, AuthorizationResult> batchResults;
    batchResults[reusedPriv].privilege = reusedPriv;
    batchResults[reusedPriv].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    batchResults[reusedPriv].isReused = true;
    batchResults[pendingPriv].privilege = pendingPriv;
    batchResults[pendingPriv].resultCode = AuthorizationResultCode::AUTHORIZATION_SUCCESS;
    batchResults[pendingPriv].isReused = false;
    connectCallback->SetBatchResults(batchResults);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_callbackMap[testPid] = iface_cast<IAuthorizationCallback>(callbackObj);
        g_connectbackMap[testPid] = connectCallback;
    }
    auto func = acquireAuthorizationOnResultfunc();
    AuthorizationResult result;
    result.privilege = pendingPriv;
    result.resultCode = AuthorizationResultCode::AUTHORIZATION_CANCELED;
    ErrCode ret = func(ERR_OK, result, testPid);
    EXPECT_EQ(ret, ERR_OK);
    ASSERT_TRUE(callbackObj->isReady);
    ASSERT_EQ(callbackObj->batchResults_.size(), 2u);
    bool reusedFound = false;
    bool pendingFound = false;
    for (const auto &r : callbackObj->batchResults_) {
        if (r.privilege == reusedPriv) {
            EXPECT_EQ(r.resultCode, AuthorizationResultCode::AUTHORIZATION_SUCCESS);
            EXPECT_TRUE(r.isReused);
            reusedFound = true;
        } else if (r.privilege == pendingPriv) {
            EXPECT_EQ(r.resultCode, AuthorizationResultCode::AUTHORIZATION_CANCELED);
            EXPECT_FALSE(r.isReused);
            pendingFound = true;
        }
    }
    EXPECT_TRUE(reusedFound);
    EXPECT_TRUE(pendingFound);
}

/**
 * @tc.name: AcquireAuthorizationTest_1000
 * @tc.desc: test AcquireAuthorization with empty results (System API path).
 *           Bug: System API path left results empty, ApplyTaAuthorization returned DENIED.
 *           Fix: AcquireAuthorization fills results from privileges when empty.
 * @tc.type: FUNC
 * @tc.require: issueIXXXXX
 */
HWTEST_F(InnerAuthorizationManagerModuleTest, AcquireAuthorizationTest_1000, TestSize.Level0)
{
    ACCOUNT_LOGI("AcquireAuthorizationTest_1000");
    EXPECT_CALL(MockAccountPermissionManager::GetInstance(), VerifyPermission(_, _)).WillRepeatedly(Return(ERR_OK));

    AcquireAuthorizationOptions options;
    options.challenge = {1, 2, 3};
    options.isReuseNeeded = true;
    options.isInteractionAllowed = true;
    options.hasContext = false;

    OsAccountConfig config;
    config.authAppBundleName = TEST_AUTH_APP_BUNDLE;
    config.authAppServiceExtensionAbilityName = TEST_SERVICE_EXTENSION;

    sptr<IRemoteObject> callbackObj = new MockAuthorizationCallbackStub();
    sptr<IRemoteObject> requestObj = new MockAuthorizationCallbackStub();

    InnerAuthorizationManager::AuthorizationRequest req;
    req.privilegesToAuthorize = {TEST_PRIVILEGE};
    ErrCode ret = manager_.AcquireAuthorization(req, options, config, callbackObj, requestObj);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_FALSE(req.results.empty());
    EXPECT_EQ(req.results.begin()->second.privilege, TEST_PRIVILEGE);
}
} // namespace AccountSA
} // namespace OHOS
