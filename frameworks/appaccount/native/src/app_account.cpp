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

#include "app_account.h"

#include "account_error_no.h"
#include "account_log_wrapper.h"
#include "account_proxy.h"
#include "app_account_common.h"
#include "app_account_constants.h"
#include "app_account_death_recipient.h"
#include "ohos_account_kits_impl.h"
#include "system_ability_definition.h"
#include <string>

namespace OHOS {
namespace AccountSA {
namespace {
ErrCode ConvertToAccountErrCode(ErrCode idlErrCode, int32_t funcResult)
{
    if (idlErrCode == ERR_OK) {
        return funcResult;
    }
    if (idlErrCode == ERR_INVALID_VALUE) {
        return ERR_ACCOUNT_COMMON_WRITE_DESCRIPTOR_ERROR;
    } else if (idlErrCode == ERR_INVALID_DATA) {
        return ERR_ACCOUNT_COMMON_WRITE_PARCEL_ERROR;
    } else {
        return ERR_APPACCOUNT_KIT_SEND_REQUEST;
    }
}
} // namespace
#define RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(str)         \
    if (CheckSpecialCharacters(str) != ERR_OK) {            \
        ACCOUNT_LOGE("failed to check special characters"); \
        NativeErrMsg() = "Invalid name. The name cannot contain space characters";                          \
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;        \
    }                                                       \

#define RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(str, maxSize, msg)                                                \
    if ((str).empty() || ((str).size() > (maxSize))) {                                                          \
        ACCOUNT_LOGE("%{public}s, input size: %{public}zu, max size: %{public}zu", msg, (str).size(), maxSize); \
        NativeErrMsg() = msg;                                                                                   \
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;                                                            \
    }

#define RETURN_IF_STRING_IS_OVERSIZE(str, maxSize, msg)                                                         \
    if ((str).size() > (maxSize)) {                                                                             \
        ACCOUNT_LOGE("%{public}s, input size: %{public}zu, max size: %{public}zu", msg, (str).size(), maxSize); \
        NativeErrMsg() = msg;                                                                                   \
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;                                                            \
    }                                                                                                           \

AppAccount &AppAccount::GetInstance()
{
    static AppAccount *instance = new (std::nothrow) AppAccount();
    return *instance;
}

AppAccount::AppAccount()
{
    auto callbackFunc = [] (int32_t systemAbilityId, const std::string &deviceId) {
        if (systemAbilityId == SUBSYS_ACCOUNT_SYS_ABILITY_ID_BEGIN) {
            AppAccount::GetInstance().RestoreListenerRecords();
        }
    };
    OhosAccountKitsImpl::GetInstance().SubscribeSystemAbility(callbackFunc);
}

void AppAccount::RestoreListenerRecords()
{
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(eventListenersMutex_);
    AppAccountSubscribeInfo subscribeInfo;
    bool flag = AppAccountEventListener::GetInstance()->GetRestoreData(subscribeInfo);
    if (!flag) {
        return;
    }
    int32_t funcResult = 0;
    ErrCode result = proxy->SubscribeAppAccount(
        subscribeInfo, AppAccountEventListener::GetInstance()->AsObject(), funcResult);
    result = ConvertToAccountErrCode(result, funcResult);
    if (result != ERR_OK) {
        std::vector<std::string> owners;
        subscribeInfo.GetOwners(owners);
        ACCOUNT_LOGE("SubscribeAppAccount owners size=%{public}d failed, errCode=%{public}d",
            static_cast<uint32_t>(owners.size()), result);
    }

    ACCOUNT_LOGI("The data recovery was successful.");
}

ErrCode AppAccount::AddAccount(const std::string &name, const std::string &extraInfo)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    RETURN_IF_STRING_IS_OVERSIZE(extraInfo, Constants::EXTRA_INFO_MAX_SIZE,
        "Invalid extraInfo. The length of the extraInfo must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->AddAccount(name, extraInfo, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::AddAccountImplicitly(const std::string &owner, const std::string &authType,
    const AAFwk::Want &options, const sptr<IAppAccountAuthenticatorCallback> &callback)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->AddAccountImplicitly(owner, authType, options, callback, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::CreateAccount(const std::string &name, const CreateAccountOptions &options)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    if (options.customData.size() > Constants::MAX_CUSTOM_DATA_SIZE) {
        NativeErrMsg() = "Invalid options.customData."
            "The length of the options.customData must be greater than 0 and less than 513";
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    for (auto it : options.customData) {
        RETURN_IF_STRING_IS_OVERSIZE(it.first, Constants::ASSOCIATED_KEY_MAX_SIZE,
            "Invalid options.customData key."
            "The length of the options.customData key must be less than 1025");
        RETURN_IF_STRING_IS_OVERSIZE(it.second, Constants::ASSOCIATED_VALUE_MAX_SIZE,
            "Invalid options.customData value."
            "The length of the options.customData value must be less than 1025");
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->CreateAccount(name, options, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::CreateAccountImplicitly(const std::string &owner, const CreateAccountImplicitlyOptions &options,
    const sptr<IAppAccountAuthenticatorCallback> &callback)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(options.authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid options.authType. The length of the options.authType must be less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(options.requiredLabels, Constants::MAX_ALLOWED_ARRAY_SIZE_INPUT,
        "Invalid options.requiredLabels. The length of the options.requiredLabels must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->CreateAccountImplicitly(owner, options, callback, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::DeleteAccount(const std::string &name)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->DeleteAccount(name, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAccountExtraInfo(const std::string &name, std::string &extraInfo)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    RETURN_IF_STRING_IS_OVERSIZE(extraInfo, Constants::EXTRA_INFO_MAX_SIZE,
        "Invalid extraInfo. The length of the extraInfo must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAccountExtraInfo(name, extraInfo, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SetAccountExtraInfo(const std::string &name, const std::string &extraInfo)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    RETURN_IF_STRING_IS_OVERSIZE(extraInfo, Constants::EXTRA_INFO_MAX_SIZE,
        "Invalid extraInfo. The length of the extraInfo must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SetAccountExtraInfo(name, extraInfo, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::EnableAppAccess(const std::string &name, const std::string &bundleName)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(bundleName, Constants::BUNDLE_NAME_MAX_SIZE,
        "Invalid bundleName. The length of the bundleName must be greater than 0 and less than 513");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->EnableAppAccess(name, bundleName, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::DisableAppAccess(const std::string &name, const std::string &bundleName)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(bundleName, Constants::BUNDLE_NAME_MAX_SIZE,
        "Invalid bundleName. The length of the bundleName must be greater than 0 and less than 513");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->DisableAppAccess(name, bundleName, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SetAppAccess(const std::string &name, const std::string &authorizedApp, bool isAccessible)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(authorizedApp, Constants::BUNDLE_NAME_MAX_SIZE,
        "Invalid bundleName. The length of the bundleName must be greater than 0 and less than 513");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SetAppAccess(name, authorizedApp, isAccessible, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::CheckAppAccountSyncEnable(const std::string &name, bool &syncEnable)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->CheckAppAccountSyncEnable(name, syncEnable, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SetAppAccountSyncEnable(const std::string &name, const bool &syncEnable)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SetAppAccountSyncEnable(name, syncEnable, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAssociatedData(const std::string &name, const std::string &key, std::string &value)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(key, Constants::ASSOCIATED_KEY_MAX_SIZE,
        "Invalid key. The length of the key must be greater than 0 and less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAssociatedData(name, key, value, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SetAssociatedData(const std::string &name, const std::string &key, const std::string &value)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(key, Constants::ASSOCIATED_KEY_MAX_SIZE,
        "Invalid key. The length of the key must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(value, Constants::ASSOCIATED_VALUE_MAX_SIZE,
        "Invalid value. The length of the value must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SetAssociatedData(name, key, value, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAccountCredential(
    const std::string &name, const std::string &credentialType, std::string &credential)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(credentialType, Constants::CREDENTIAL_TYPE_MAX_SIZE,
        "Invalid credentialType. The length of the credentialType must be greater than 0 and less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAccountCredential(name, credentialType, credential, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SetAccountCredential(
    const std::string &name, const std::string &credentialType, const std::string &credential)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(credentialType, Constants::CREDENTIAL_TYPE_MAX_SIZE,
        "Invalid credentialType. The length of the credentialType must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(credential, Constants::CREDENTIAL_MAX_SIZE,
        "Invalid credential. The length of the credential must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SetAccountCredential(name, credentialType, credential, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::Authenticate(const std::string &name, const std::string &owner, const std::string &authType,
    const AAFwk::Want &options, const sptr<IAppAccountAuthenticatorCallback> &callback)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    AppAccountStringInfo appAccountStringInfo;
    appAccountStringInfo.name = name;
    appAccountStringInfo.owner = owner;
    appAccountStringInfo.authType = authType;

    auto idlErrCode = proxy->Authenticate(appAccountStringInfo, options, callback, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetOAuthToken(
    const std::string &name, const std::string &owner, const std::string &authType, std::string &token)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetOAuthToken(name, owner, authType, token, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAuthToken(
    const std::string &name, const std::string &owner, const std::string &authType, std::string &token)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAuthToken(name, owner, authType, token, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SetOAuthToken(const std::string &name, const std::string &authType, const std::string &token)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(token, Constants::TOKEN_MAX_SIZE,
        "Invalid token. The length of the token must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SetOAuthToken(name, authType, token, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::DeleteOAuthToken(
    const std::string &name, const std::string &owner, const std::string &authType, const std::string &token)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(token, Constants::TOKEN_MAX_SIZE,
        "Invalid token. The length of the token must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->DeleteOAuthToken(name, owner, authType, token, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::DeleteAuthToken(
    const std::string &name, const std::string &owner, const std::string &authType, const std::string &token)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(token, Constants::TOKEN_MAX_SIZE,
        "Invalid token. The length of the token must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->DeleteAuthToken(name, owner, authType, token, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::CheckTokenVisibilityParam(
    const std::string &name, const std::string &authType, const std::string &bundleName)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(bundleName, Constants::BUNDLE_NAME_MAX_SIZE,
        "Invalid bundleName. The length of the bundleName must be greater than 0 and less than 513");
    return ERR_OK;
}

ErrCode AppAccount::SetAuthTokenVisibility(
    const std::string &name, const std::string &authType, const std::string &bundleName, bool isVisible)
{
    ErrCode ret = CheckTokenVisibilityParam(name, authType, bundleName);
    if (ret != ERR_OK) {
        return ret;
    }
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SetAuthTokenVisibility(name, authType, bundleName, isVisible, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SetOAuthTokenVisibility(
    const std::string &name, const std::string &authType, const std::string &bundleName, bool isVisible)
{
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    ErrCode ret = CheckTokenVisibilityParam(name, authType, bundleName);
    if (ret != ERR_OK) {
        return ret;
    }
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SetOAuthTokenVisibility(name, authType, bundleName, isVisible, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::CheckAuthTokenVisibility(
    const std::string &name, const std::string &authType, const std::string &bundleName, bool &isVisible)
{
    ErrCode ret = CheckTokenVisibilityParam(name, authType, bundleName);
    if (ret != ERR_OK) {
        return ret;
    }
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->CheckAuthTokenVisibility(name, authType, bundleName, isVisible, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::CheckOAuthTokenVisibility(
    const std::string &name, const std::string &authType, const std::string &bundleName, bool &isVisible)
{
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    ErrCode ret = CheckTokenVisibilityParam(name, authType, bundleName);
    if (ret != ERR_OK) {
        return ret;
    }
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->CheckOAuthTokenVisibility(name, authType, bundleName, isVisible, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAuthenticatorInfo(const std::string &owner, AuthenticatorInfo &info)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAuthenticatorInfo(owner, info, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAllOAuthTokens(
    const std::string &name, const std::string &owner, std::vector<OAuthTokenInfo> &tokenInfos)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    tokenInfos.clear();
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAllOAuthTokens(name, owner, tokenInfos, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetOAuthList(
    const std::string &name, const std::string &authType, std::set<std::string> &oauthList)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_CONTAINS_SPECIAL_CHAR(name);
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetOAuthList(name, authType, oauthList, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAuthList(
    const std::string &name, const std::string &authType, std::set<std::string> &oauthList)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_OVERSIZE(authType, Constants::AUTH_TYPE_MAX_SIZE,
        "Invalid authType. The length of the authType must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAuthList(name, authType, oauthList, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAuthenticatorCallback(const std::string &sessionId, sptr<IRemoteObject> &callback)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(sessionId, Constants::SESSION_ID_MAX_SIZE,
        "Invalid sessionId. The length of the sessionId must be greater than 0 and less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAuthenticatorCallback(sessionId, funcResult, callback);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAllAccounts(const std::string &owner, std::vector<AppAccountInfo> &appAccounts)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    appAccounts.clear();
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAllAccounts(owner, appAccounts, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::CheckAppAccess(const std::string &name, const std::string &authorizedApp, bool &isAccessible)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(authorizedApp, Constants::BUNDLE_NAME_MAX_SIZE,
        "Invalid bundleName. The length of the bundleName must be greater than 0 and less than 513");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->CheckAppAccess(name, authorizedApp, isAccessible, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::DeleteAccountCredential(const std::string &name, const std::string &credentialType)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(credentialType, Constants::CREDENTIAL_TYPE_MAX_SIZE,
        "Invalid credentialType. The length of the credentialType must be greater than 0 and less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->DeleteAccountCredential(name, credentialType, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SelectAccountsByOptions(
    const SelectAccountsOptions &options, const sptr<IAppAccountAuthenticatorCallback> &callback)
{
    RETURN_IF_STRING_IS_OVERSIZE(options.allowedAccounts, Constants::MAX_ALLOWED_ARRAY_SIZE_INPUT,
        "Invalid options.allowedAccounts."
        "The length of the options.allowedAccounts must be less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(options.allowedOwners, Constants::MAX_ALLOWED_ARRAY_SIZE_INPUT,
        "Invalid options.allowedOwners."
        "The length of the options.allowedOwners must be less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(options.requiredLabels, Constants::MAX_ALLOWED_ARRAY_SIZE_INPUT,
        "Invalid options.requiredLabels."
        "The length of the options.requiredLabels must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SelectAccountsByOptions(options, callback, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::VerifyCredential(const std::string &name, const std::string &owner,
    const VerifyCredentialOptions &options, const sptr<IAppAccountAuthenticatorCallback> &callback)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(options.credentialType, Constants::CREDENTIAL_TYPE_MAX_SIZE,
        "Invalid options.credentialType. The length of the options.credentialType must be less than 1025");
    RETURN_IF_STRING_IS_OVERSIZE(options.credential, Constants::CREDENTIAL_MAX_SIZE,
        "Invalid options.credential. The length of the options.credential must be less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->VerifyCredential(name, owner, options, callback, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::CheckAccountLabels(const std::string &name, const std::string &owner,
    const std::vector<std::string> &labels, const sptr<IAppAccountAuthenticatorCallback> &callback)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(name, Constants::NAME_MAX_SIZE,
        "Invalid name. The length of the name must be greater than 0 and less than 513");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(labels, Constants::MAX_ALLOWED_ARRAY_SIZE_INPUT,
        "Invalid labels. The length of the labels must be greater than 0 and less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->CheckAccountLabels(name, owner, labels, callback, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SetAuthenticatorProperties(const std::string &owner,
    const SetPropertiesOptions &options, const sptr<IAppAccountAuthenticatorCallback> &callback)
{
    RETURN_IF_STRING_IS_EMPTY_OR_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be greater than 0 and less than 1025");
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    int32_t funcResult = 0;
    auto idlErrCode = proxy->SetAuthenticatorProperties(owner, options, callback, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::GetAllAccessibleAccounts(std::vector<AppAccountInfo> &appAccounts)
{
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    appAccounts.clear();
    int32_t funcResult = 0;
    auto idlErrCode = proxy->GetAllAccessibleAccounts(appAccounts, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::QueryAllAccessibleAccounts(
    const std::string &owner, std::vector<AppAccountInfo> &appAccounts)
{
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    RETURN_IF_STRING_IS_OVERSIZE(owner, Constants::OWNER_MAX_SIZE,
        "Invalid owner. The length of the owner must be less than 1025");
    appAccounts.clear();
    int32_t funcResult = 0;
    auto idlErrCode = proxy->QueryAllAccessibleAccounts(owner, appAccounts, funcResult);
    return ConvertToAccountErrCode(idlErrCode, funcResult);
}

ErrCode AppAccount::SubscribeAppAccount(const std::shared_ptr<AppAccountSubscriber> &subscriber)
{
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }

    if (subscriber == nullptr) {
        ACCOUNT_LOGE("subscriber is nullptr");
        return ERR_APPACCOUNT_KIT_SUBSCRIBER_IS_NULLPTR;
    }

    sptr<IRemoteObject> appAccountEventListener = nullptr;
    std::lock_guard<std::mutex> lock(eventListenersMutex_);
    CreateAppAccountEventListener(subscriber, appAccountEventListener);

    bool needNotifyService = false;
    ErrCode result = AppAccountEventListener::GetInstance()->SubscribeAppAccount(subscriber, needNotifyService);
    if (result != ERR_OK) {
        return result;
    }
    if (!needNotifyService) {
        return ERR_OK;
    }
    // Refresh to full owners
    AppAccountSubscribeInfo subscribeInfo;
    AppAccountEventListener::GetInstance()->GetRestoreData(subscribeInfo);
    int32_t funcResult = 0;
    result = proxy->SubscribeAppAccount(subscribeInfo, appAccountEventListener, funcResult);
    result =  ConvertToAccountErrCode(result, funcResult);
    if (result != ERR_OK) {
        std::vector<std::string> deleteOwners;
        AppAccountEventListener::GetInstance()->UnsubscribeAppAccount(subscriber, needNotifyService, deleteOwners);
    }
    return result;
}

ErrCode AppAccount::UnsubscribeAppAccount(const std::shared_ptr<AppAccountSubscriber> &subscriber)
{
    auto proxy = GetAppAccountProxy();
    if (proxy == nullptr) {
        return ERR_ACCOUNT_COMMON_GET_PROXY;
    }
    if (subscriber == nullptr) {
        ACCOUNT_LOGE("subscriber is nullptr");
        return ERR_APPACCOUNT_KIT_SUBSCRIBER_IS_NULLPTR;
    }

    std::lock_guard<std::mutex> lock(eventListenersMutex_);
    bool needNotifyService = false;
    std::vector<std::string> deleteOwners;
    ErrCode result = AppAccountEventListener::GetInstance()->UnsubscribeAppAccount(
        subscriber, needNotifyService, deleteOwners);
    if (result != ERR_OK) {
        return result;
    }
    if (!needNotifyService) {
        return ERR_OK;
    }
    int32_t funcResult = 0;
    result = proxy->UnsubscribeAppAccount(AppAccountEventListener::GetInstance()->AsObject(), deleteOwners, funcResult);
    result = ConvertToAccountErrCode(result, funcResult);
    if (result != ERR_OK) {
        AppAccountEventListener::GetInstance()->SubscribeAppAccount(subscriber, needNotifyService);
    }

    return result;
}

ErrCode AppAccount::ResetAppAccountProxy()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if ((proxy_ != nullptr) && (proxy_->AsObject() != nullptr)) {
        proxy_->AsObject()->RemoveDeathRecipient(deathRecipient_);
    }
    proxy_ = nullptr;
    return ERR_OK;
}

ErrCode AppAccount::CheckSpecialCharacters(const std::string &name)
{
    for (auto specialCharacter : Constants::SPECIAL_CHARACTERS) {
        std::size_t found = name.find(specialCharacter);
        if (found != std::string::npos) {
            ACCOUNT_LOGE("found a special character, specialCharacter = %{public}c", specialCharacter);
            return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
        }
    }

    return ERR_OK;
}

sptr<IAppAccount> AppAccount::GetAppAccountProxy()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (proxy_ != nullptr) {
        return proxy_;
    }
    sptr<IRemoteObject> object = OhosAccountKitsImpl::GetInstance().GetAppAccountService();
    if (object == nullptr) {
        ACCOUNT_LOGE("failed to get app account service");
        return nullptr;
    }
    deathRecipient_ = new (std::nothrow) AppAccountDeathRecipient();
    if (deathRecipient_ == nullptr) {
        ACCOUNT_LOGE("failed to create app account death recipient");
        return nullptr;
    }
    if (!object->AddDeathRecipient(deathRecipient_)) {
        ACCOUNT_LOGE("failed to add app account death recipient");
        deathRecipient_ = nullptr;
        return nullptr;
    }
    proxy_ = iface_cast<IAppAccount>(object);
    return proxy_;
}

ErrCode AppAccount::CreateAppAccountEventListener(
    const std::shared_ptr<AppAccountSubscriber> &subscriber, sptr<IRemoteObject> &appAccountEventListener)
{
    if (subscriber == nullptr) {
        ACCOUNT_LOGE("subscriber is nullptr");
        return SUBSCRIBE_FAILED;
    }

    appAccountEventListener = AppAccountEventListener::GetInstance()->AsObject();
    return INITIAL_SUBSCRIPTION;
}
}  // namespace AccountSA
}  // namespace OHOS
