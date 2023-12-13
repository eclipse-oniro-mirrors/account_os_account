/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#ifndef OS_ACCOUNT_INTERFACES_KITS_NAPI_IAM_INCLUDE_NAPI_IAM_COMMON_H
#define OS_ACCOUNT_INTERFACES_KITS_NAPI_IAM_INCLUDE_NAPI_IAM_COMMON_H

#include "account_error_no.h"
#include "account_iam_client_callback.h"
#include "account_iam_info.h"
#ifdef HAS_PIN_AUTH_PART
#include "i_inputer.h"
#endif
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "napi_account_common.h"

namespace OHOS {
namespace AccountJsKit {
constexpr size_t ARG_SIZE_ONE = 1;
constexpr size_t ARG_SIZE_TWO = 2;
constexpr size_t ARG_SIZE_THREE = 3;
constexpr size_t ARG_SIZE_FOUR = 4;
constexpr size_t ARG_SIZE_FIVE = 5;

constexpr size_t PARAM_ZERO = 0;
constexpr size_t PARAM_ONE = 1;
constexpr size_t PARAM_TWO = 2;
constexpr size_t PARAM_THREE = 3;
constexpr size_t PARAM_FOUR = 4;

enum IAMResultCode : int32_t {
    ERR_IAM_SUCCESS = 0,
    ERR_IAM_FAIL = 1,
    ERR_IAM_GENERAL_ERROR = 2,
    ERR_IAM_CANCELED = 3,
    ERR_IAM_TIMEOUT = 4,
    ERR_IAM_TYPE_NOT_SUPPORT = 5,
    ERR_IAM_TRUST_LEVEL_NOT_SUPPORT = 6,
    ERR_IAM_BUSY = 7,
    ERR_IAM_INVALID_PARAMETERS = 8,
    ERR_IAM_LOCKED = 9,
    ERR_IAM_NOT_ENROLLED = 10,
    ERR_IAM_HARDWARE_NOT_SUPPORTED = 11,
    ERR_IAM_SYSTEM_ERROR_CODE_BEGIN = 1000,
    ERR_IAM_IPC_ERROR = 1001,
    ERR_IAM_INVALID_CONTEXT_ID = 1002,
    ERR_IAM_READ_PARCEL_ERROR = 1003,
    ERR_IAM_WRITE_PARCEL_ERROR = 1004,
    ERR_IAM_CHECK_PERMISSION_FAILED = 1005,
    ERR_IAM_INVALID_HDI_INTERFACE = 1006,
    ERR_IAM_VENDOR_ERROR_CODE_BEGIN = 10000,
    ERR_IAM_TOKEN_TIMEOUT = 10016,
    ERR_IAM_TOKEN_AUTH_FAILED = 10017,
    ERR_IAM_CREDENTIAL_NUMBER_REACH_LIMIT = 10018,
    ERR_IAM_SESSION_TIMEOUT = 10019,
};

int32_t AccountIAMConvertToJSErrCode(int32_t errCode);

struct JsIAMCallback {
    JsIAMCallback(napi_env env) : env(env) {}
    ~JsIAMCallback()
    {
        ReleaseNapiRefArray(env, {onResult, onAcquireInfo});
    }
    bool onResultCalled = false;
    napi_env env;
    napi_ref onResult = nullptr;
    napi_ref onAcquireInfo = nullptr;
};

#ifdef HAS_USER_AUTH_PART
struct IDMCallbackParam : public CommonAsyncContext {
    explicit IDMCallbackParam(napi_env napiEnv) : CommonAsyncContext(napiEnv) {};
    std::shared_ptr<JsIAMCallback> callback;
    int32_t result = 0;
    int32_t module = 0;
    uint32_t acquire = 0;
    uint64_t credentialId = 0;
    std::vector<uint8_t> extraInfo;
};

struct AuthCallbackParam : public CommonAsyncContext {
    explicit AuthCallbackParam(napi_env napiEnv) : CommonAsyncContext(napiEnv) {};
    int32_t module = 0;
    uint32_t acquireInfo = 0;
    std::vector<uint8_t> extraInfo;
    int32_t resultCode = 0;
    int32_t remainTimes = -1;
    int32_t freezingTime = -1;
    std::vector<uint8_t> token;
    std::shared_ptr<JsIAMCallback> callback;
};

struct AuthContext {
    int32_t userId = 0;
    int32_t authType = -1;
    int32_t trustLevel = -1;
    bool throwErr = true;
    std::vector<uint8_t> challenge;
    std::shared_ptr<AccountSA::IDMCallback> callback;
};

struct IDMContext : public CommonAsyncContext {
    IDMContext(napi_env napiEnv) : CommonAsyncContext(napiEnv) {};
    bool throwErr = true;
    std::vector<uint8_t> challenge;
    uint64_t credentialId = 0;
    std::vector<uint8_t> token;
    AccountSA::CredentialParameters addCredInfo;
    std::shared_ptr<JsIAMCallback> callback;
};

struct GetAuthInfoContext : public CommonAsyncContext {
    explicit GetAuthInfoContext(napi_env napiEnv) : CommonAsyncContext(napiEnv) {};
    AccountSA::AuthType authType {0};
    std::vector<AccountSA::CredentialInfo> credInfo;
};

struct GetPropertyContext : public CommonAsyncContext {
    explicit GetPropertyContext(napi_env napiEnv) : CommonAsyncContext(napiEnv) {};
    AccountSA::GetPropertyRequest request;
    int32_t result = 0;
    int32_t authSubType = 0;
    int32_t remainTimes = 0;
    int32_t freezingTime = 0;
    std::string enrollmentProgress;
    std::string sensorInfo;
};

struct SetPropertyContext : public CommonAsyncContext {
    explicit SetPropertyContext(napi_env napiEnv) : CommonAsyncContext(napiEnv) {};
    AccountSA::SetPropertyRequest request;
    int32_t result = 0;
};

class NapiIDMCallback : public AccountSA::IDMCallback {
public:
    NapiIDMCallback(napi_env env, const std::shared_ptr<JsIAMCallback> &callback);
    virtual ~NapiIDMCallback();

    void OnResult(int32_t result, const AccountSA::Attributes &extraInfo) override;
    void OnAcquireInfo(int32_t module, uint32_t acquireInfo, const AccountSA::Attributes &extraInfo) override;

private:
    napi_env env_;
    std::shared_ptr<JsIAMCallback> callback_;
    std::mutex mutex_;
};

class NapiGetInfoCallback : public AccountSA::GetCredInfoCallback {
public:
    explicit NapiGetInfoCallback(napi_env env, napi_ref callbackRef, napi_deferred deferred);
    virtual ~NapiGetInfoCallback();

    void OnCredentialInfo(int32_t result, const std::vector<AccountSA::CredentialInfo> &infoList) override;
private:
    napi_env env_;
    napi_ref callbackRef_;
    napi_deferred deferred_;
};

class NapiUserAuthCallback : public AccountSA::IDMCallback {
public:
    NapiUserAuthCallback(napi_env env, const std::shared_ptr<JsIAMCallback> &callback);
    virtual ~NapiUserAuthCallback();

    void OnResult(int32_t result, const AccountSA::Attributes &extraInfo) override;
    void OnAcquireInfo(int32_t module, uint32_t acquireInfo, const AccountSA::Attributes &extraInfo) override;
private:
    napi_env env_;
    std::shared_ptr<JsIAMCallback> callback_;
    std::mutex mutex_;
};

class NapiGetPropCallback : public AccountSA::GetSetPropCallback {
public:
    explicit NapiGetPropCallback(
        napi_env env, napi_ref callbackRef, napi_deferred deferred, const AccountSA::GetPropertyRequest &request);
    virtual ~NapiGetPropCallback();
    void GetContextParams(const UserIam::UserAuth::Attributes &extraInfo, GetPropertyContext &context);
    void OnResult(int32_t result, const AccountSA::Attributes &extraInfo) override;
private:
    napi_env env_ = nullptr;
    napi_ref callbackRef_ = nullptr;
    napi_deferred deferred_ = nullptr;
    AccountSA::GetPropertyRequest request_;
    std::mutex mutex_;
};

class NapiSetPropCallback : public AccountSA::GetSetPropCallback {
public:
    explicit NapiSetPropCallback(napi_env env, napi_ref callbackRef, napi_deferred deferred);
    virtual ~NapiSetPropCallback();

    void OnResult(int32_t result, const AccountSA::Attributes &extraInfo) override;

private:
    napi_env env_ = nullptr;
    napi_ref callbackRef_ = nullptr;
    napi_deferred deferred_ = nullptr;
    std::mutex mutex_;
};
#endif  // HAS_USER_AUTH_PART

#ifdef HAS_PIN_AUTH_PART
struct InputerContext : public CommonAsyncContext {
    int32_t authSubType = -1;
    std::shared_ptr<AccountSA::IInputerData> inputerData = nullptr;
    std::shared_ptr<NapiCallbackRef> callback;
};

class NapiGetDataCallback : public AccountSA::IInputer {
public:
    NapiGetDataCallback(napi_env env, const std::shared_ptr<NapiCallbackRef> &callback);
    virtual ~NapiGetDataCallback();

    void OnGetData(int32_t authSubType, const std::shared_ptr<AccountSA::IInputerData> inputerData) override;

private:
    napi_env env_;
    std::shared_ptr<NapiCallbackRef> callback_;
    ThreadLockInfo lockInfo_;
};
#endif  // HAS_PIN_AUTH_PART

void CallbackAsyncOrPromise(napi_env env, CommonAsyncContext *context, napi_value errJs, napi_value dataJs);
napi_value CreateErrorObject(napi_env env, int32_t code);
napi_status ParseUInt32Array(napi_env env, napi_value value, std::vector<uint32_t> &data);
napi_status ParseIAMCallback(napi_env env, napi_value object, std::shared_ptr<JsIAMCallback> &callback);
#ifdef HAS_USER_AUTH_PART
napi_status ParseAddCredInfo(napi_env env, napi_value value, AccountSA::CredentialParameters &addCredInfo);
napi_status ParseGetPropRequest(napi_env env, napi_value object, AccountSA::GetPropertyRequest &request);
napi_status ParseSetPropRequest(napi_env env, napi_value object, AccountSA::SetPropertyRequest &request);
napi_value CreateCredInfoArray(napi_env env, const std::vector<AccountSA::CredentialInfo> &info);
napi_value CreateAuthResult(napi_env env, const std::vector<uint8_t> &token, int32_t remainTimes, int32_t freezingTime);
#endif
}  // namespace AccountJsKit
}  // namespace OHOS
#endif  // OS_ACCOUNT_INTERFACES_KITS_NAPI_IAM_INCLUDE_NAPI_IAM_COMMON_H
