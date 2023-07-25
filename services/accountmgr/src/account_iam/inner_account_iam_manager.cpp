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

#include "inner_account_iam_manager.h"

#include "account_iam_callback.h"
#include "account_log_wrapper.h"
#include "domain_account_callback_service.h"
#include "iinner_os_account_manager.h"
#include "inner_domain_account_manager.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "user_auth_client.h"
#include "user_auth_client_impl.h"
#include "user_idm_client.h"

namespace OHOS {
namespace AccountSA {
namespace {
#ifdef HAS_STORAGE_PART
const int32_t ERROR_STORAGE_KEY_NOT_EXIST = -2;
#endif
}
using UserIDMClient = UserIam::UserAuth::UserIdmClient;
using UserAuthClient = UserIam::UserAuth::UserAuthClient;
using UserAuthClientImpl = UserIam::UserAuth::UserAuthClientImpl;

InnerAccountIAMManager::InnerAccountIAMManager()
{
    userStateMap_[0] = IDLE;
}

InnerAccountIAMManager &InnerAccountIAMManager::GetInstance()
{
    static InnerAccountIAMManager instance;
    return instance;
}

void InnerAccountIAMManager::OpenSession(int32_t userId, std::vector<uint8_t> &challenge)
{
    challenge = UserIDMClient::GetInstance().OpenSession(userId);
    std::lock_guard<std::mutex> lock(mutex_);
    userStateMap_[userId] = AFTER_OPEN_SESSION;
    userChallengeMap_[userId] = challenge;
}

void InnerAccountIAMManager::CloseSession(int32_t userId)
{
    UserIDMClient::GetInstance().CloseSession(userId);
    std::lock_guard<std::mutex> lock(mutex_);
    if (userId == 0) {
        userStateMap_[0] = IDLE;
    } else {
        userStateMap_.erase(userId);
    }
    userChallengeMap_.erase(userId);
}

void InnerAccountIAMManager::AddCredential(
    int32_t userId, const CredentialParameters &credInfo, const sptr<IIDMCallback> &callback)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    auto idmCallback = std::make_shared<AddCredCallback>(userId, credInfo, callback);
    UserIDMClient::GetInstance().AddCredential(userId, credInfo, idmCallback);
}

void InnerAccountIAMManager::UpdateCredential(
    int32_t userId, const CredentialParameters &credInfo, const sptr<IIDMCallback> &callback)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    if (credInfo.token.empty()) {
        ACCOUNT_LOGE("token is empty");
        Attributes emptyResult;
        callback->OnResult(ResultCode::INVALID_PARAMETERS, emptyResult);
        return;
    }
    auto idmCallback = std::make_shared<UpdateCredCallback>(userId, credInfo, callback);
    UserIDMClient::GetInstance().UpdateCredential(userId, credInfo, idmCallback);
}

void InnerAccountIAMManager::DelCred(
    int32_t userId, uint64_t credentialId, const std::vector<uint8_t> &authToken, const sptr<IIDMCallback> &callback)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    Attributes emptyResult;
    if (authToken.empty()) {
        ACCOUNT_LOGD("token is empty");
        callback->OnResult(ResultCode::INVALID_PARAMETERS, emptyResult);
        return;
    }
    std::vector<uint8_t> secret;
    ErrCode result = UpdateUserKey(userId, 0, credentialId, authToken, secret);
    if (result != ERR_OK) {
        callback->OnResult(result, emptyResult);
        return;
    }
    auto idmCallback = std::make_shared<DelCredCallback>(userId, credentialId, authToken, callback);
    UserIDMClient::GetInstance().DeleteCredential(userId, credentialId, authToken, idmCallback);
}

void InnerAccountIAMManager::DelUser(
    int32_t userId, const std::vector<uint8_t> &authToken, const sptr<IIDMCallback> &callback)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    Attributes errResult;
    if (authToken.empty()) {
        ACCOUNT_LOGE("token is empty");
        callback->OnResult(ResultCode::INVALID_PARAMETERS, errResult);
        return;
    }
    ErrCode result = RemoveUserKey(userId, authToken);
    if (result != ERR_OK) {
        callback->OnResult(result, errResult);
        return;
    }
    auto idmCallback = std::make_shared<DelCredCallback>(userId, 0, authToken, callback);
    UserIDMClient::GetInstance().DeleteUser(userId, authToken, idmCallback);
}

void InnerAccountIAMManager::GetCredentialInfo(
    int32_t userId, AuthType authType, const sptr<IGetCredInfoCallback> &callback)
{
    if (static_cast<int32_t>(authType) == static_cast<int32_t>(IAMAuthType::DOMAIN)) {
        std::vector<CredentialInfo> infoList;
        if (CheckDomainAuthAvailable(userId)) {
            CredentialInfo info;
            info.authType = static_cast<AuthType>(IAMAuthType::DOMAIN);
            info.pinType = static_cast<PinSubType>(IAMAuthSubType::DOMAIN_MIXED);
            infoList.emplace_back(info);
        }
        return callback->OnCredentialInfo(infoList);
    }
    auto getCallback = std::make_shared<GetCredInfoCallbackWrapper>(userId, static_cast<int32_t>(authType), callback);
    UserIDMClient::GetInstance().GetCredentialInfo(userId, authType, getCallback);
}

int32_t InnerAccountIAMManager::Cancel(int32_t userId)
{
    return UserIDMClient::GetInstance().Cancel(userId);
}

int32_t InnerAccountIAMManager::AuthUser(
    int32_t userId, const AuthParam &authParam, const sptr<IIDMCallback> &callback, uint64_t &contextId)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    auto userAuthCallback = std::make_shared<AuthCallback>(userId, authParam.authType, callback);
    contextId = UserAuthClient::GetInstance().BeginAuthentication(
        userId, authParam.challenge, authParam.authType, authParam.authTrustLevel, userAuthCallback);
    return ERR_OK;
}

int32_t InnerAccountIAMManager::CancelAuth(uint64_t contextId)
{
    return UserAuthClient::GetInstance().CancelAuthentication(contextId);
}

int32_t InnerAccountIAMManager::GetAvailableStatus(
    AuthType authType, AuthTrustLevel authTrustLevel, int32_t &status)
{
    if (static_cast<int32_t>(authType) != static_cast<int32_t>(IAMAuthType::DOMAIN)) {
        status = UserAuthClientImpl::Instance().GetAvailableStatus(authType, authTrustLevel);
        return ERR_OK;
    }
    bool isPluginAvailable = InnerDomainAccountManager::GetInstance().IsPluginAvailable();
    if (isPluginAvailable) {
        status = ERR_JS_SUCCESS;
    } else {
        status = ERR_JS_AUTH_TYPE_NOT_SUPPORTED;
    }
    return ERR_OK;
}

ErrCode InnerAccountIAMManager::GetDomainAuthStatusInfo(
    int32_t userId, const GetPropertyRequest &request, const sptr<IGetSetPropCallback> &callback)
{
    OsAccountInfo osAccountInfo;
    ErrCode result = IInnerOsAccountManager::GetInstance().QueryOsAccountById(userId, osAccountInfo);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("failed to get account info");
        return result;
    }
    DomainAccountInfo domainAccountInfo;
    osAccountInfo.GetDomainInfo(domainAccountInfo);
    if (domainAccountInfo.accountName_.empty()) {
        ACCOUNT_LOGE("the target user is not a domain account");
        return ERR_ACCOUNT_IAM_UNSUPPORTED_AUTH_TYPE;
    }
    std::shared_ptr<DomainAccountCallback> statusCallback =
        std::make_shared<GetDomainAuthStatusInfoCallback>(request, callback);
    if (statusCallback == nullptr) {
        ACCOUNT_LOGE("failed to create GetDomainAuthStatusInfoCallback");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    return InnerDomainAccountManager::GetInstance().GetAuthStatusInfo(domainAccountInfo, statusCallback);
}

bool InnerAccountIAMManager::CheckDomainAuthAvailable(int32_t userId)
{
    OsAccountInfo osAccountInfo;
    if (IInnerOsAccountManager::GetInstance().QueryOsAccountById(userId, osAccountInfo) != ERR_OK) {
        ACCOUNT_LOGE("failed to get account info");
        return false;
    }
    DomainAccountInfo domainAccountInfo;
    osAccountInfo.GetDomainInfo(domainAccountInfo);
    bool isAvailable = InnerDomainAccountManager::GetInstance().IsPluginAvailable();
    return !domainAccountInfo.accountName_.empty() && isAvailable;
}

void InnerAccountIAMManager::GetProperty(
    int32_t userId, const GetPropertyRequest &request, const sptr<IGetSetPropCallback> &callback)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    if (static_cast<int32_t>(request.authType) != static_cast<int32_t>(IAMAuthType::DOMAIN)) {
        auto getCallback = std::make_shared<GetPropCallbackWrapper>(callback);
        UserAuthClient::GetInstance().GetProperty(userId, request, getCallback);
        return;
    }
    ErrCode result = GetDomainAuthStatusInfo(userId, request, callback);
    if (result != ERR_OK) {
        Attributes attributes;
        callback->OnResult(result, attributes);
    }
}

void InnerAccountIAMManager::SetProperty(
    int32_t userId, const SetPropertyRequest &request, const sptr<IGetSetPropCallback> &callback)
{
    if (static_cast<int32_t>(request.authType) == static_cast<int32_t>(IAMAuthType::DOMAIN)) {
        Attributes result;
        callback->OnResult(ERR_ACCOUNT_IAM_UNSUPPORTED_AUTH_TYPE, result);
        return;
    }
    auto setCallback = std::make_shared<SetPropCallbackWrapper>(callback);
    UserAuthClient::GetInstance().SetProperty(userId, request, setCallback);
}

IAMState InnerAccountIAMManager::GetState(int32_t userId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = userStateMap_.find(userId);
    if (it != userStateMap_.end()) {
        return it->second;
    }
    return userStateMap_[0];
}

void InnerAccountIAMManager::SetState(int32_t userId, IAMState state)
{
    std::lock_guard<std::mutex> lock(mutex_);
    userStateMap_[userId] = state;
}

void InnerAccountIAMManager::GetChallenge(int32_t userId, std::vector<uint8_t> &challenge)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = userChallengeMap_.find(userId);
    if (it != userChallengeMap_.end()) {
        challenge = it->second;
    } else {
        challenge = userChallengeMap_[0];
    }
}

#ifdef FILE_ENCRYPTION_EL1_FEATURE
static bool IsDomainAccount(int32_t userId)
{
    OsAccountInfo accountInfo;
    ErrCode errCode = IInnerOsAccountManager::GetInstance().QueryOsAccountById(userId, accountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get os account info failed, errCode: %{public}d", errCode);
        return false;
    }
    DomainAccountInfo domainInfo;
    accountInfo.GetDomainInfo(domainInfo);
    if (domainInfo.accountName_.empty()) {
        ACCOUNT_LOGE("the target user is not a domain account");
        return false;
    }
    return true;
}
#endif // FILE_ENCRYPTION_EL1_FEATURE

ErrCode InnerAccountIAMManager::UpdateStorageKey(
    int32_t userId, uint64_t secureUid, const std::vector<uint8_t> &token,
    const std::vector<uint8_t> &oldSecret, const std::vector<uint8_t> &newSecret)
{
#ifdef HAS_STORAGE_PART
    ErrCode result = GetStorageManagerProxy();
    if (result != ERR_OK) {
        ACCOUNT_LOGE("fail to get storage proxy");
        return result;
    }
#ifdef FILE_ENCRYPTION_EL1_FEATURE
    if (IsDomainAccount(userId)) {
        result = storageMgrProxy_->UpdateUserAuth(userId, secureUid, {}, {}, {});
    } else {
        result = storageMgrProxy_->UpdateUserAuth(userId, secureUid, token, oldSecret, newSecret);
    }
#else
    result = storageMgrProxy_->UpdateUserAuth(userId, secureUid, token, oldSecret, newSecret);
#endif // FILE_ENCRYPTION_EL1_FEATURE
    if ((result != ERR_OK) && (result != ERROR_STORAGE_KEY_NOT_EXIST)) {
        ACCOUNT_LOGE("fail to update user auth");
        return result;
    }
    return storageMgrProxy_->UpdateKeyContext(userId);
#else
    return ERR_OK;
#endif
}

ErrCode InnerAccountIAMManager::ActivateUserKey(
    int32_t userId, const std::vector<uint8_t> &token, const std::vector<uint8_t> &secret)
{
#ifdef HAS_STORAGE_PART
    ErrCode result = GetStorageManagerProxy();
    if (result != ERR_OK) {
        ACCOUNT_LOGE("fail to get storage proxy");
        return result;
    }
#ifdef FILE_ENCRYPTION_EL1_FEATURE
    if (IsDomainAccount(userId)) {
        result = storageMgrProxy_->ActiveUserKey(userId, {}, {});
    } else {
        result = storageMgrProxy_->ActiveUserKey(userId, token, secret);
    }
#else
    result = storageMgrProxy_->ActiveUserKey(userId, token, secret);
#endif // FILE_ENCRYPTION_EL1_FEATURE
    if (result != ERR_OK && result != ERROR_STORAGE_KEY_NOT_EXIST) {
        ACCOUNT_LOGE("fail to active user key, error code: %{public}d", result);
        return result;
    }
    storageMgrProxy_->PrepareStartUser(userId);
#endif
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = credInfoMap_.find(userId);
    if (it != credInfoMap_.end()) {
        it->second.secret = secret;
    } else {
        credInfoMap_[userId] = {
            .secret = secret
        };
    }
    return ERR_OK;
}

ErrCode InnerAccountIAMManager::UpdateUserKey(int32_t userId, uint64_t secureUid, uint64_t credentialId,
    const std::vector<uint8_t> &token, const std::vector<uint8_t> &newSecret)
{
    ErrCode result = ERR_OK;
    AccountCredentialInfo oldCredInfo;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = credInfoMap_.find(userId);
    if (it != credInfoMap_.end()) {
        oldCredInfo = it->second;
    }
    if (newSecret.empty() && credentialId != oldCredInfo.credentialId) {
        ACCOUNT_LOGE("the key do not need to be removed");
        return ERR_OK;
    }
    result = UpdateStorageKey(userId, secureUid, token, oldCredInfo.secret, newSecret);
    if (result != ERR_OK) {
        return result;
    }
    credInfoMap_[userId] = {
        .credentialId = credentialId,
        .oldSecureUid = oldCredInfo.secureUid,
        .secureUid = secureUid,
        .oldSecret = oldCredInfo.secret,
        .secret = newSecret
    };
    return result;
}

ErrCode InnerAccountIAMManager::RemoveUserKey(int32_t userId, const std::vector<uint8_t> &token)
{
    ErrCode result = ERR_OK;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = credInfoMap_.find(userId);
    if (it == credInfoMap_.end()) {
        return ERR_OK;
    }
    AccountCredentialInfo oldCredInfo = it->second;
    std::vector<uint8_t> newSecret;
    result = UpdateStorageKey(userId, 0, token, oldCredInfo.secret, newSecret);
    if (result != ERR_OK) {
        return result;
    }
    credInfoMap_[userId] = {
        .oldSecureUid = oldCredInfo.secureUid,
        .secureUid = 0,
        .oldSecret = oldCredInfo.secret,
        .secret = newSecret
    };
    return result;
}

ErrCode InnerAccountIAMManager::RestoreUserKey(int32_t userId, uint64_t credentialId,
    const std::vector<uint8_t> &token)
{
    ErrCode result = ERR_OK;
    AccountCredentialInfo credInfo;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = credInfoMap_.find(userId);
    if (it != credInfoMap_.end()) {
        credInfo = it->second;
    }
    if (credentialId != 0 && credInfo.credentialId != credentialId) {
        return ERR_OK;
    }
    result = UpdateStorageKey(userId, credInfo.oldSecureUid, token, credInfo.secret, credInfo.oldSecret);
    if (result != ERR_OK) {
        return result;
    }
    credInfoMap_[userId] = {
        .secureUid = credInfo.oldSecureUid,
        .secret = credInfo.oldSecret
    };
    return result;
}

ErrCode InnerAccountIAMManager::GetStorageManagerProxy()
{
#ifdef HAS_STORAGE_PART
    if (storageMgrProxy_ != nullptr) {
        return ERR_OK;
    }
    auto systemAbilityManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (systemAbilityManager == nullptr) {
        ACCOUNT_LOGE("failed to get system ability mgr");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    auto remote = systemAbilityManager->GetSystemAbility(STORAGE_MANAGER_MANAGER_ID);
    if (remote == nullptr) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID service");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY;
    }
    storageMgrProxy_ = iface_cast<StorageManager::IStorageManager>(remote);
    if (storageMgrProxy_ == nullptr) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID proxy");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY;
    }
    return ERR_OK;
#else
    return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY;
#endif
}
}  // namespace AccountSA
}  // namespace OHOS
