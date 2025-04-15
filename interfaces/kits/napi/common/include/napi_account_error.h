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

#ifndef OS_ACCOUNT_INTERFACES_KITS_NAPI_COMMON_INCLUDE_NAPI_ACCOUNT_ERROR_H
#define OS_ACCOUNT_INTERFACES_KITS_NAPI_COMMON_INCLUDE_NAPI_ACCOUNT_ERROR_H

#include <cstdint>
#include <string>
#include "account_error_no.h"
#include "napi/native_api.h"
#include "napi/native_common.h"

namespace OHOS {
namespace AccountJsKit {
static const std::unordered_map<uint32_t, std::string> g_errorStringMap = {
    {ERR_JS_PARAMETER_ERROR, "Parameter invalid, please input the correct parameter"},
    {ERR_JS_ACCOUNT_NOT_FOUND, "Account not found, please check whether the account exists"},
    {ERR_JS_IS_NOT_SYSTEM_APP, "This api is system api, Please use the system application to call this api"},
    {ERR_JS_ACCOUNT_ALREADY_EXIST, "Account already exists, please cancel or try other account"},
    {ERR_JS_ACCOUNT_ALREADY_ACTIVATED, "Account already activated, do not activate duplicately"},
    {ERR_JS_ACCOUNT_SERVICE_BUSY, "Account service is busy, please wait for a moment and try again"},
    {ERR_JS_ACCOUNT_NUMBER_REACH_LIMIT, "Account number limit exceeded, please try again after delete other accounts"},
    {ERR_JS_MULTI_USER_NOT_SUPPORT, "Multiple users not supported, please cancel the creation"},
    {ERR_JS_ACCOUNT_TYPE_NOT_SUPPORT, "Account type not supported, please create a non administrator account"},
    {ERR_JS_ACCOUNT_RESTRICTED, "Account is restricted, the operation account ID is the reserved for system"},
    {ERR_JS_LISTENER_ALREADY_REGISTERED,
        "Listener is already registered, please register new listener or delete old listener and try again"},
    {ERR_JS_LISTENER_NOT_REGISTERED, "Listener is not registered, please use the registered listener"},
    {ERR_JS_CREDENTIAL_INPUTER_ALREADY_EXIST, "PIN inputer is already registered, please do not repeat register"},
    {ERR_JS_SYSTEM_SERVICE_EXCEPTION, "System service exception, please try again or reboot your device"},
    {ERR_JS_INVALID_PARAMETER, "Parameter invalid, please input the correct parameter"},
    {ERR_JS_TRUST_LEVEL_NOT_SUPPORTED, "Trust level not supported, please input the correct trust level"},
    {ERR_JS_AUTH_TYPE_NOT_SUPPORTED, "Auth type not supported, please input the correct auth type"},
    {ERR_JS_AUTH_TIMEOUT, "Auth timeout, please try again or check your internet connection"},
    {ERR_JS_AUTH_SERVICE_BUSY, "Auth service is busy, please try again later"},
    {ERR_JS_AUTH_SERVICE_LOCKED,
        "Auth service is locked, auth fail too many times, please try again after freezingTime"},
    {ERR_JS_CREDENTIAL_NOT_EXIST, "Credential not found, please check whether credential exists and try again"},
    {ERR_JS_INVALID_CONTEXT_ID, "Context ID is invalid, please check the context ID and try again"},
    {ERR_JS_AUTH_CREDENTIAL_WRONG_ERROR,
        "Auth credential incorrect, please check the password or credential, and try again"},
    {ERR_JS_APPLICATION_NOT_EXIST, "Application not found, please check the bundle name and try again"},
    {ERR_JS_ACCOUNT_AUTHENTICATOR_NOT_EXIST, "Application account authenticator service is not available, please try "
                                                "again with applications that support authenticator service"},
    {ERR_JS_ACCOUNT_AUTHENTICATOR_SERVICE_EXCEPTION,
        "Application account authenticator service exception, please try again or reboot your device"},
    {ERR_JS_SESSION_NOT_EXIST,
        "Application account session is not exists, please use sessionId that successfully opened"},
    {ERR_JS_AUTHORIZATION_LIST_TOO_LARGE,
        "Application account authorization list is too large, \
        please cancel some existed authentications and try again"},
    {ERR_JS_TOKEN_NUMBER_REACH_LIMIT,
        "Application account token number reach limitations, please delete some existed token and try again"},
    {ERR_JS_CUSTOM_DATA_NUMBER_REACH_LIMIT,
        "Application account customized data number reach limit, please delete some existed custom data and try again"},
    {ERR_JS_CUSTOM_DATA_NOT_FOUND,
        "Application account customized data not found, please use existed customized data and try again"},
    {ERR_JS_AUTH_TYPE_NOT_FOUND, "Application account auth type not found, please use existed auth type"},
    {ERR_JS_PERMISSION_DENIED, "Permission denied"},
    {ERR_JS_PLUGIN_NETWORK_EXCEPTION, "Network exception"},
    {ERR_JS_CAPABILITY_NOT_SUPPORTED, "capability not supported"},
    {ERR_JS_ACCOUNT_LOGGED_IN_ACCOUNTS_OVERSIZE, "The number of the logged in OS accounts reaches upper limit"},
    {ERR_JS_COMPLEXITY_CHECK_FAILED, "The complexity of credential check failed"},
    {ERR_JS_PIN_IS_EXPIRED, "The PIN credential is expired"},
    {ERR_JS_DOMAIN_PLUGIN_ALREADY_REGISTERED, "The domain plugin is already registered"},
    {ERR_JS_SERVER_UNREACHABLE, "The server is unreachable"},
    {ERR_JS_SERVER_CONFIG_NOT_FOUND, "The server config not found"},
    {ERR_JS_SERVER_CONFIG_ALREADY_EXISTS, "Server config already exists"},
    {ERR_JS_SERVER_CONFIG_ASSOCIATED_ACCOUNT, "Server config has been associated with an account"},
    {ERR_JS_SERVER_CONFIG_UPPER_LIMIT, "The number of server config reaches the upper limit"},
};

std::string ConvertToJsErrMsg(int32_t jsErrCode);
napi_value GenerateBusinessSuccess(napi_env env, bool throwErr);
napi_value GenerateBusinessError(napi_env env, int32_t jsErrCode, const std::string &jsErrMsg);
napi_value GenerateBusinessError(napi_env env, int32_t nativeErrCode);
napi_value GenerateBusinessError(napi_env env, int32_t nativeErrCode, bool throwErr);
void GenerateBusinessError(const int32_t nativeErrCode, int32_t &jsErrCode, std::string &errMsg);
void AccountNapiThrow(napi_env env, int32_t jsErrCode, const std::string &jsErrMsg, bool throwErr = true);
void AccountNapiThrow(napi_env env, int32_t nativeErrCode, bool throwErr = true);
void AccountIAMNapiThrow(napi_env env, int32_t jsErrCode, bool throwErr);
bool CheckJsErrorCode(int32_t errCode);
} // namespace AccountJsKit
} // namespace OHOS

#endif // OS_ACCOUNT_INTERFACES_KITS_NAPI_COMMON_INCLUDE_NAPI_ACCOUNT_ERROR_H
