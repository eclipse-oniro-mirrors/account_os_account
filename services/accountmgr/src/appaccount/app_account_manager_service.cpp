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

#include "app_account_manager_service.h"
#include "accesstoken_kit.h"
#include "account_log_wrapper.h"
#include "account_permission_manager.h"
#include "bundle_manager_adapter.h"
#include "account_hisysevent_adapter.h"
#include "inner_app_account_manager.h"
#include "ipc_skeleton.h"

namespace OHOS {
namespace AccountSA {
namespace {
constexpr int32_t UID_TRANSFORM_DIVISOR = 200000;  // local account id = uid / UID_TRANSFORM_DIVISOR
const char DISTRIBUTED_DATASYNC[] = "ohos.permission.DISTRIBUTED_DATASYNC";
const char GET_ALL_APP_ACCOUNTS[] = "ohos.permission.GET_ALL_APP_ACCOUNTS";
}

AppAccountManagerService::AppAccountManagerService()
#ifdef HAS_CES_PART
    : observer_(AppAccountCommonEventObserver::GetInstance())
#endif // HAS_CES_PART
{
    ACCOUNT_LOGI("Constructed");
    innerManager_ = std::make_shared<InnerAppAccountManager>();
}

AppAccountManagerService::~AppAccountManagerService()
{
    ACCOUNT_LOGI("Destroyed");
}

ErrCode AppAccountManagerService::AddAccount(const std::string &name, const std::string &extraInfo, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetCallingInfo(callingUid, bundleName, appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->AddAccount(name, extraInfo, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::AddAccountImplicitly(const std::string &owner, const std::string &authType,
    const AAFwk::Want &options, const sptr<IAppAccountAuthenticatorCallback> &callback, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    request.callerPid = IPCSkeleton::GetCallingRealPid();
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.owner = owner;
    request.authType = authType;
    request.options = options;
    request.callback = callback;
    request.options.SetParam(Constants::KEY_CALLER_PID, request.callerPid);
    request.options.SetParam(Constants::KEY_CALLER_UID, request.callerUid);
    funcResult = innerManager_->AddAccountImplicitly(request);
    return ERR_OK;
}

ErrCode AppAccountManagerService::CreateAccount(
    const std::string &name, const CreateAccountOptions &options, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetCallingInfo(callingUid, bundleName, appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->CreateAccount(name, options, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::CreateAccountImplicitly(const std::string &owner,
    const CreateAccountImplicitlyOptions &options, const sptr<IAppAccountAuthenticatorCallback> &callback,
    int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    request.callerPid = IPCSkeleton::GetCallingRealPid();
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.owner = owner;
    request.callback = callback;
    request.createOptions = options;
    request.createOptions.parameters.SetParam(Constants::KEY_CALLER_BUNDLE_NAME, request.callerBundleName);
    funcResult = innerManager_->CreateAccountImplicitly(request);
    return ERR_OK;
}

ErrCode AppAccountManagerService::DeleteAccount(const std::string &name, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetCallingInfo(callingUid, bundleName, appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->DeleteAccount(name, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAccountExtraInfo(
    const std::string &name, std::string &extraInfo, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetCallingInfo(callingUid, bundleName, appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->GetAccountExtraInfo(name, extraInfo, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SetAccountExtraInfo(
    const std::string &name, const std::string &extraInfo, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetCallingInfo(callingUid, bundleName, appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->SetAccountExtraInfo(name, extraInfo, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::EnableAppAccess(
    const std::string &name, const std::string &authorizedApp, int32_t &funcResult)
{
    AppAccountCallingInfo appAccountCallingInfo;
    ErrCode result = GetCallingInfo(
        appAccountCallingInfo.callingUid, appAccountCallingInfo.bundleName, appAccountCallingInfo.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }

    if (authorizedApp == appAccountCallingInfo.bundleName) {
        ACCOUNT_LOGE("AuthorizedApp is the same to owner.");
        funcResult = ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
        return ERR_OK;
    }

    funcResult = innerManager_->EnableAppAccess(name, authorizedApp, appAccountCallingInfo);
    return ERR_OK;
}

ErrCode AppAccountManagerService::DisableAppAccess(
    const std::string &name, const std::string &authorizedApp, int32_t &funcResult)
{
    AppAccountCallingInfo appAccountCallingInfo;
    ErrCode ret = GetCallingInfo(
        appAccountCallingInfo.callingUid, appAccountCallingInfo.bundleName, appAccountCallingInfo.appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    if (authorizedApp == appAccountCallingInfo.bundleName) {
        ACCOUNT_LOGE("AuthorizedApp is the same to owner.");
        funcResult = ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
        return ERR_OK;
    }
    funcResult = innerManager_->DisableAppAccess(name, authorizedApp, appAccountCallingInfo);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SetAppAccess(
    const std::string &name, const std::string &authorizedApp, bool isAccessible, int32_t &funcResult)
{
    AppAccountCallingInfo appAccountCallingInfo;
    ErrCode ret = GetCallingInfo(
        appAccountCallingInfo.callingUid, appAccountCallingInfo.bundleName, appAccountCallingInfo.appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }

    if (authorizedApp == appAccountCallingInfo.bundleName) {
        if (isAccessible) {
            ACCOUNT_LOGI("AuthorizedApp name is the self, invalid operate.");
            funcResult = ERR_OK;
            return ERR_OK;
        } else {
            ACCOUNT_LOGE("AuthorizedApp is the same to owner.");
            funcResult = ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
            return ERR_OK;
        }
    }
    if (isAccessible) {
        funcResult = innerManager_->EnableAppAccess(
            name, authorizedApp, appAccountCallingInfo, Constants::API_VERSION9);
        return ERR_OK;
    }

    funcResult = innerManager_->DisableAppAccess(name, authorizedApp, appAccountCallingInfo, Constants::API_VERSION9);
    return ERR_OK;
}

ErrCode AppAccountManagerService::CheckAppAccountSyncEnable(
    const std::string &name, bool &syncEnable, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetBundleNameAndCheckPerm(callingUid, bundleName, DISTRIBUTED_DATASYNC);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    ret = GetCallingTokenInfoAndAppIndex(appIndex);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("failed to get app index");
        funcResult = ret;
        return ERR_OK;
    }

    funcResult = innerManager_->CheckAppAccountSyncEnable(name, syncEnable, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SetAppAccountSyncEnable(
    const std::string &name, bool syncEnable, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetBundleNameAndCheckPerm(callingUid, bundleName, DISTRIBUTED_DATASYNC);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    ret = GetCallingTokenInfoAndAppIndex(appIndex);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("failed to get app index");
        funcResult = ret;
        return ERR_OK;
    }

    funcResult = innerManager_->SetAppAccountSyncEnable(name, syncEnable, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAssociatedData(
    const std::string &name, const std::string &key, std::string &value, int32_t &funcResult)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    funcResult = innerManager_->GetAssociatedData(name, key, value, callingUid);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SetAssociatedData(
    const std::string &name, const std::string &key, const std::string &value, int32_t &funcResult)
{
    AppAccountCallingInfo appAccountCallingInfo;
    ErrCode ret = GetCallingInfo(appAccountCallingInfo.callingUid, appAccountCallingInfo.bundleName,
        appAccountCallingInfo.appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->SetAssociatedData(name, key, value, appAccountCallingInfo);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAccountCredential(
    const std::string &name, const std::string &credentialType, std::string &credential, int32_t &funcResult)
{
    AppAccountCallingInfo appAccountCallingInfo;
    ErrCode ret = GetCallingInfo(appAccountCallingInfo.callingUid, appAccountCallingInfo.bundleName,
        appAccountCallingInfo.appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->GetAccountCredential(name, credentialType, credential, appAccountCallingInfo);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SetAccountCredential(
    const std::string &name, const std::string &credentialType, const std::string &credential, int32_t &funcResult)
{
    AppAccountCallingInfo appAccountCallingInfo;
    ErrCode ret = GetCallingInfo(appAccountCallingInfo.callingUid, appAccountCallingInfo.bundleName,
        appAccountCallingInfo.appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->SetAccountCredential(name, credentialType, credential, appAccountCallingInfo);
    return ERR_OK;
}

ErrCode AppAccountManagerService::Authenticate(const AppAccountStringInfo &appAccountStringInfo,
    const AAFwk::Want &options, const sptr<IAppAccountAuthenticatorCallback> &callback, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    request.callerPid = IPCSkeleton::GetCallingRealPid();
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.name = appAccountStringInfo.name;
    request.owner = appAccountStringInfo.owner;
    request.authType = appAccountStringInfo.authType;
    request.options = options;
    request.callback = callback;
    request.options.SetParam(Constants::KEY_CALLER_BUNDLE_NAME, request.callerBundleName);
    request.options.SetParam(Constants::KEY_CALLER_UID, request.callerUid);
    funcResult = innerManager_->Authenticate(request);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetOAuthToken(
    const std::string &name, const std::string &owner, const std::string &authType, std::string &token,
    int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.name = name;
    request.owner = owner;
    request.authType = authType;
    funcResult = innerManager_->GetOAuthToken(request, token);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAuthToken(
    const std::string &name, const std::string &owner, const std::string &authType, std::string &token,
    int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.name = name;
    request.owner = owner;
    request.authType = authType;
    funcResult = innerManager_->GetOAuthToken(request, token, Constants::API_VERSION9);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SetOAuthToken(
    const std::string &name, const std::string &authType, const std::string &token, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.name = name;
    request.owner = request.callerBundleName;
    request.authType = authType;
    request.token = token;
    funcResult = innerManager_->SetOAuthToken(request);
    return ERR_OK;
}

ErrCode AppAccountManagerService::DeleteOAuthToken(
    const std::string &name, const std::string &owner, const std::string &authType, const std::string &token,
    int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode ret = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    request.name = name;
    request.owner = owner;
    request.authType = authType;
    request.token = token;
    funcResult = innerManager_->DeleteOAuthToken(request);
    return ERR_OK;
}

ErrCode AppAccountManagerService::DeleteAuthToken(
    const std::string &name, const std::string &owner, const std::string &authType, const std::string &token,
    int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.name = name;
    request.owner = owner;
    request.authType = authType;
    request.token = token;
    funcResult = innerManager_->DeleteOAuthToken(request, Constants::API_VERSION9);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetTokenVisibilityParam(const std::string &name,
    const std::string &authType, const std::string &bundleName, AuthenticatorSessionRequest &request)
{
    ErrCode ret = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (ret != ERR_OK) {
        return ret;
    }
    request.name = name;
    request.owner = request.callerBundleName;
    request.authType = authType;
    request.bundleName = bundleName;
    return ret;
}

ErrCode AppAccountManagerService::SetOAuthTokenVisibility(
    const std::string &name, const std::string &authType, const std::string &bundleName, bool isVisible,
    int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode ret = GetTokenVisibilityParam(name, authType, bundleName, request);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    request.isTokenVisible = isVisible;
    funcResult = innerManager_->SetOAuthTokenVisibility(request);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SetAuthTokenVisibility(
    const std::string &name, const std::string &authType, const std::string &bundleName, bool isVisible,
    int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetTokenVisibilityParam(name, authType, bundleName, request);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    if (request.bundleName == request.owner) {
        if (isVisible) {
            ACCOUNT_LOGI("authorizedApp name is the self, invalid operate.");
            funcResult = ERR_OK;
            return ERR_OK;
        } else {
            ACCOUNT_LOGE("authorizedApp is the same to owner.");
            funcResult = ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
            return ERR_OK;
        }
    }
    request.isTokenVisible = isVisible;
    funcResult = innerManager_->SetOAuthTokenVisibility(request, Constants::API_VERSION9);
    return ERR_OK;
}

ErrCode AppAccountManagerService::CheckOAuthTokenVisibility(
    const std::string &name, const std::string &authType, const std::string &bundleName, bool &isVisible,
    int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode ret = GetTokenVisibilityParam(name, authType, bundleName, request);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->CheckOAuthTokenVisibility(request, isVisible);
    return ERR_OK;
}

ErrCode AppAccountManagerService::CheckAuthTokenVisibility(
    const std::string &name, const std::string &authType, const std::string &bundleName, bool &isVisible,
    int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode ret = GetTokenVisibilityParam(name, authType, bundleName, request);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->CheckOAuthTokenVisibility(request, isVisible, Constants::API_VERSION9);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAuthenticatorInfo(
    const std::string &owner, AuthenticatorInfo &info, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    request.callerUid = IPCSkeleton::GetCallingUid();
    ErrCode result = GetCallingTokenInfoAndAppIndex(request.appIndex);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("failed to get app index");
        funcResult = result;
        return ERR_OK;
    }
    request.owner = owner;
    funcResult = innerManager_->GetAuthenticatorInfo(request, info);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAllOAuthTokens(
    const std::string &name, const std::string &owner, std::vector<OAuthTokenInfo> &tokenInfos, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.name = name;
    request.owner = owner;
    funcResult = innerManager_->GetAllOAuthTokens(request, tokenInfos);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetOAuthList(
    const std::string &name, const std::string &authType, std::set<std::string> &oauthList, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.name = name;
    request.authType = authType;
    funcResult = innerManager_->GetOAuthList(request, oauthList);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAuthList(
    const std::string &name, const std::string &authType, std::set<std::string> &oauthList, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode ret = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    request.name = name;
    request.authType = authType;
    funcResult = innerManager_->GetOAuthList(request, oauthList, Constants::API_VERSION9);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAuthenticatorCallback(
    const std::string &sessionId, int32_t &funcResult, sptr<IRemoteObject> &callback)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.sessionId = sessionId;
    funcResult = innerManager_->GetAuthenticatorCallback(request, callback);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAllAccounts(
    const std::string &owner, std::vector<AppAccountInfo> &appAccounts, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetCallingInfo(callingUid, bundleName, appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    if ((owner != bundleName) &&
        (AccountPermissionManager::VerifyPermission(GET_ALL_APP_ACCOUNTS) != ERR_OK)) {
        ACCOUNT_LOGE("failed to verify permission for %{public}s", GET_ALL_APP_ACCOUNTS);
        REPORT_PERMISSION_FAIL();
        funcResult = ERR_ACCOUNT_COMMON_PERMISSION_DENIED;
        return ERR_OK;
    }

    AppExecFwk::BundleInfo bundleInfo;
    int32_t userId = callingUid / UID_TRANSFORM_DIVISOR;
    bool result = BundleManagerAdapter::GetInstance()->GetBundleInfo(
        owner, AppExecFwk::BundleFlag::GET_BUNDLE_DEFAULT, bundleInfo, userId);
    if (!result) {
        funcResult = ERR_APPACCOUNT_SERVICE_GET_BUNDLE_INFO;
        return ERR_OK;
    }

    funcResult = innerManager_->GetAllAccounts(owner, appAccounts, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetAllAccessibleAccounts(
    std::vector<AppAccountInfo> &appAccounts, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetBundleNameAndCheckPerm(callingUid, bundleName, GET_ALL_APP_ACCOUNTS);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    ret = GetCallingTokenInfoAndAppIndex(appIndex);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("failed to get app index");
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->GetAllAccessibleAccounts(appAccounts, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::QueryAllAccessibleAccounts(
    const std::string &owner, std::vector<AppAccountInfo> &appAccounts, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode result = GetCallingInfo(callingUid, bundleName, appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    if (owner.empty()) {
        funcResult = innerManager_->GetAllAccessibleAccounts(appAccounts, callingUid, bundleName, appIndex);
        return ERR_OK;
    }
    AppExecFwk::BundleInfo bundleInfo;
    int32_t userId = callingUid / UID_TRANSFORM_DIVISOR;
    bool ret = BundleManagerAdapter::GetInstance()->GetBundleInfo(
        owner, AppExecFwk::BundleFlag::GET_BUNDLE_DEFAULT, bundleInfo, userId);
    if (!ret) {
        funcResult = ERR_OK;
        return ERR_OK;
    }
    funcResult = innerManager_->GetAllAccounts(owner, appAccounts, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::CheckAppAccess(
    const std::string &name, const std::string &authorizedApp, bool &isAccessible, int32_t &funcResult)
{
    AppAccountCallingInfo appAccountCallingInfo;
    ErrCode result = GetCallingInfo(appAccountCallingInfo.callingUid, appAccountCallingInfo.bundleName,
        appAccountCallingInfo.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    if (authorizedApp == appAccountCallingInfo.bundleName) {
        isAccessible = true;
        funcResult = ERR_OK;
        return ERR_OK;
    }
    funcResult = innerManager_->CheckAppAccess(name, authorizedApp, isAccessible, appAccountCallingInfo);
    return ERR_OK;
}

ErrCode AppAccountManagerService::DeleteAccountCredential(
    const std::string &name, const std::string &credentialType, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetCallingInfo(callingUid, bundleName, appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->DeleteAccountCredential(name, credentialType, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SelectAccountsByOptions(
    const SelectAccountsOptions &options, const sptr<IAppAccountAuthenticatorCallback> &callback, int32_t &funcResult)
{
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetCallingInfo(callingUid, bundleName, appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }
    funcResult = innerManager_->SelectAccountsByOptions(options, callback, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::VerifyCredential(const std::string &name, const std::string &owner,
    const VerifyCredentialOptions &options, const sptr<IAppAccountAuthenticatorCallback> &callback, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.name = name;
    request.owner = owner;
    request.verifyCredOptions = options;
    request.callback = callback;
    funcResult = innerManager_->VerifyCredential(request);
    return ERR_OK;
}

ErrCode AppAccountManagerService::CheckAccountLabels(const std::string &name, const std::string &owner,
    const std::vector<std::string> &labels, const sptr<IAppAccountAuthenticatorCallback> &callback, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.labels = labels;
    request.callback = callback;
    request.name = name;
    request.owner = owner;
    funcResult = innerManager_->CheckAccountLabels(request);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SetAuthenticatorProperties(const std::string &owner,
    const SetPropertiesOptions &options, const sptr<IAppAccountAuthenticatorCallback> &callback, int32_t &funcResult)
{
    AuthenticatorSessionRequest request;
    ErrCode result = GetCallingInfo(request.callerUid, request.callerBundleName, request.appIndex);
    if (result != ERR_OK) {
        funcResult = result;
        return ERR_OK;
    }
    request.owner = owner;
    request.setPropOptions = options;
    request.callback = callback;
    funcResult = innerManager_->SetAuthenticatorProperties(request);
    return ERR_OK;
}

ErrCode AppAccountManagerService::SubscribeAppAccount(
    const AppAccountSubscribeInfo &subscribeInfo, const sptr<IRemoteObject> &eventListener, int32_t &funcResult)
{
    auto subscribeInfoCopy = subscribeInfo;
    int32_t callingUid = -1;
    std::string bundleName;
    uint32_t appIndex;
    ErrCode ret = GetCallingInfo(callingUid, bundleName, appIndex);
    if (ret != ERR_OK) {
        funcResult = ret;
        return ERR_OK;
    }

    std::vector<std::string> owners;
    subscribeInfoCopy.GetOwners(owners);
    if (owners.size() == 0) {
        ACCOUNT_LOGE("owners size is 0");
        funcResult = ERR_APPACCOUNT_SERVICE_OWNERS_SIZE_IS_ZERO;
        return ERR_OK;
    }

    int32_t userId = callingUid / UID_TRANSFORM_DIVISOR;
    std::vector<std::string> existOwners;
    for (auto owner : owners) {
        AppExecFwk::BundleInfo bundleInfo;
        bool bundleRet = BundleManagerAdapter::GetInstance()->GetBundleInfo(owner,
            AppExecFwk::BundleFlag::GET_BUNDLE_DEFAULT, bundleInfo, userId);
        if (!bundleRet) {
            ACCOUNT_LOGE("Failed to get bundle info, name=%{public}s", owner.c_str());
            continue;
        }
        existOwners.push_back(owner);
    }
    if (existOwners.size() == 0) {
        ACCOUNT_LOGI("ExistOwners is empty.");
        funcResult = ERR_OK;
        return ERR_OK;
    }
    subscribeInfoCopy.SetOwners(existOwners);
    funcResult = innerManager_->SubscribeAppAccount(subscribeInfoCopy, eventListener, callingUid, bundleName, appIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::UnsubscribeAppAccount(const sptr<IRemoteObject> &eventListener,
    const std::vector<std::string> &owners, int32_t &funcResult)
{
    std::vector<std::string> ownerList = owners;
    funcResult = innerManager_->UnsubscribeAppAccount(eventListener, ownerList);
    return ERR_OK;
}

ErrCode AppAccountManagerService::OnPackageRemoved(
    const uid_t &uid, const std::string &bundleName, const uint32_t &appIndex)
{
    return innerManager_->OnPackageRemoved(uid, bundleName, appIndex);
}

ErrCode AppAccountManagerService::OnUserRemoved(int32_t userId)
{
    return innerManager_->OnUserRemoved(userId);
}

ErrCode AppAccountManagerService::GetBundleNameAndCheckPerm(int32_t &callingUid,
    std::string &bundleName, const std::string &permName)
{
    ErrCode result = GetBundleNameAndCallingUid(callingUid, bundleName);
    if (result != ERR_OK) {
        return result;
    }

    result = AccountPermissionManager::VerifyPermission(permName);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("failed to verify permission for %{public}s, result = %{public}d",
            permName.c_str(), result);
        ReportPermissionFail(callingUid, IPCSkeleton::GetCallingRealPid(), permName);
        return result;
    }
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetBundleNameAndCallingUid(int32_t &callingUid, std::string &bundleName)
{
    callingUid = IPCSkeleton::GetCallingUid();
    ErrCode bundleRet = BundleManagerAdapter::GetInstance()->GetNameForUid(callingUid, bundleName);
    if (bundleRet != ERR_OK) {
        ACCOUNT_LOGE("failed to get bundle name");
        return ERR_APPACCOUNT_SERVICE_GET_BUNDLE_NAME;
    }
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetCallingTokenInfoAndAppIndex(uint32_t &appIndex)
{
    uint32_t callingTokenId = IPCSkeleton::GetCallingTokenID();
    Security::AccessToken::HapTokenInfo hapTokenInfo;
    int result = Security::AccessToken::AccessTokenKit::GetHapTokenInfo(callingTokenId, hapTokenInfo);
    if (result) {
        ACCOUNT_LOGE("failed to get hap token info, result = %{public}d", result);
        return ERR_APPACCOUNT_SERVICE_GET_APP_INDEX;
    }
    if (hapTokenInfo.instIndex < 0) {
        ACCOUNT_LOGE("get invalid app index from hap token info, index = %{public}d", hapTokenInfo.instIndex);
        return ERR_APPACCOUNT_SERVICE_GET_APP_INDEX;
    }
    appIndex = static_cast<uint32_t>(hapTokenInfo.instIndex);
    return ERR_OK;
}

ErrCode AppAccountManagerService::GetCallingInfo(int32_t &callingUid, std::string &bundleName, uint32_t &appIndex)
{
    ErrCode result = GetBundleNameAndCallingUid(callingUid, bundleName);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("Failed to get bundle name");
        return result;
    }
    result = GetCallingTokenInfoAndAppIndex(appIndex);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("failed to get app index");
        return result;
    }
    return result;
}
}  // namespace AccountSA
}  // namespace OHOS
