/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#include "account_iam_callback.h"

#include <mutex>
#include <securec.h>
#include <string>
#include "access_token.h"
#include "accesstoken_kit.h"
#include "account_iam_info.h"
#include "account_info_report.h"
#include "account_log_wrapper.h"
#include "account_hisysevent_adapter.h"
#include "iinner_os_account_manager.h"
#include "inner_account_iam_manager.h"
#ifdef SUPPORT_DOMAIN_ACCOUNTS
#include "inner_domain_account_manager.h"
#endif // SUPPORT_DOMAIN_ACCOUNTS
#include "ipc_skeleton.h"
#include "os_account_delete_user_idm_callback.h"
#include "storage_service_errno.h"
#include "token_setproc.h"
#include "user_auth_client.h"
#include "user_idm_client.h"
#include "os_account_constants.h"

namespace OHOS {
namespace AccountSA {
namespace {
const char OPERATION_REENROLL[] = "re-enroll";
}

using UserIDMClient = UserIam::UserAuth::UserIdmClient;
using UserAuthClient = UserIam::UserAuth::UserAuthClient;

const std::vector<uint8_t> TEMP_PIN = {50, 48, 50, 52, 48, 56};

void AuthCallbackDeathRecipient::SetContextId(uint16_t contextId)
{
    contextId_ = contextId;
}

void AuthCallbackDeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &remote)
{
    ACCOUNT_LOGI("remote callback died, cancel authentication");
    if (contextId_ > 0) {
        UserAuthClient::GetInstance().CancelAuthentication(contextId_);
    }
}

AuthCallback::AuthCallback(
    uint32_t userId, AuthType authType, AuthIntent authIntent, const sptr<IIDMCallback> &callback)
    : userId_(userId), authType_(authType), authIntent_(authIntent), innerCallback_(callback)
{
    // save caller tokenId for pin re-enroll
    if (authType == AuthType::PIN) {
        callerTokenId_ = IPCSkeleton::GetCallingTokenID();
    }
}

AuthCallback::AuthCallback(uint32_t userId, AuthType authType, AuthIntent authIntent,
    bool isRemoteAuth, const sptr<IIDMCallback> &callback)
    : userId_(userId), authType_(authType), authIntent_(authIntent),
    isRemoteAuth_(isRemoteAuth), innerCallback_(callback)
{
    // save caller tokenId for pin re-enroll
    if (authType == AuthType::PIN) {
        callerTokenId_ = IPCSkeleton::GetCallingTokenID();
    }
}

UpdateCredInfo::UpdateCredInfo(const Attributes &extraInfo)
{
    extraInfo.GetUint64Value(Attributes::AttributeKey::ATTR_CREDENTIAL_ID, credentialId);
    extraInfo.GetUint64Value(Attributes::AttributeKey::ATTR_SEC_USER_ID, secureUid);
    extraInfo.GetUint8ArrayValue(Attributes::ATTR_AUTH_TOKEN, token);
    extraInfo.GetUint8ArrayValue(Attributes::ATTR_ROOT_SECRET, newSecret);
    extraInfo.GetUint8ArrayValue(Attributes::ATTR_OLD_ROOT_SECRET, oldSecret);
}

ReEnrollCallback::ReEnrollCallback(const sptr<IIDMCallback> &innerCallback) : innerCallback_(innerCallback)
{}

void ReEnrollCallback::OnResult(int32_t result, const Attributes &extraInfo)
{
    std::unique_lock<std::mutex> lock(mutex_);
    ACCOUNT_LOGE("ReEnroll: UpdateCredential call to ReEnroll OnResult, result is %{public}d", result);
    result_ = result;
    isCalled_ = true;
    onResultCondition_.notify_one();
    return;
}

void ReEnrollCallback::OnAcquireInfo(int32_t module, uint32_t acquireInfo, const AccountSA::Attributes &extraInfo)
{
    std::unique_lock<std::mutex> lock(mutex_);
    ACCOUNT_LOGI("ReEnroll: UpdateCredential call to ReEnroll OnAcquireInfo");
    innerCallback_->OnAcquireInfo(module, acquireInfo, extraInfo);
    return;
}

ErrCode AuthCallback::InnerHandleReEnroll(const std::vector<uint8_t> &token)
{
    //set first caller to sceneboard
    SetFirstCallerTokenID(callerTokenId_);
    sptr<ReEnrollCallback> callback = new (std::nothrow) ReEnrollCallback(innerCallback_);
    if (callback == nullptr) {
        ACCOUNT_LOGE("ReEnroll: failed to allocate callback");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    CredentialParameters credInfo = {
        .authType = AuthType::PIN,
        // `pinType` is unused in iam
        .pinType = PinSubType::PIN_SIX,
        .token = token
    };
    InnerAccountIAMManager::GetInstance().UpdateCredential(userId_, credInfo, callback);
    std::unique_lock<std::mutex> lock(callback->mutex_);
    bool done = callback->onResultCondition_.wait_for(lock, std::chrono::seconds(Constants::REENROLL_WAIT_TIME),
        [cb = callback]() { return cb->isCalled_; });
    ErrCode result = callback->result_;
    if (!done) {
        ACCOUNT_LOGE("ReEnroll: UpdateCredential failed, timeout");
        return ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT;
    }
    return result;
}

void AuthCallback::HandleReEnroll(const Attributes &extraInfo, int32_t accountId, const std::vector<uint8_t> &token)
{
    bool needReEnroll = false;
    extraInfo.GetBoolValue(Attributes::ATTR_RE_ENROLL_FLAG, needReEnroll);
    if (!needReEnroll) {
        return;
    }
    if (authType_ == AuthType::PIN) {
        ACCOUNT_LOGI("ReEnroll: need re-enroll for accountId %{public}d", accountId);
        ErrCode result = InnerHandleReEnroll(token);
        if (result != ERR_OK) {
            ACCOUNT_LOGE("ReEnroll: UpdateCredential failed, result is %{public}d", result);
            ReportOsAccountOperationFail(accountId, "ReEnroll", result, "UpdateCredential failed");
        } else {
            ACCOUNT_LOGI("ReEnroll: UpdateCredential successful");
            ReportOsAccountLifeCycle(accountId, OPERATION_REENROLL);
        }
    } else {
        ACCOUNT_LOGW("ReEnroll: re-enroll flag exist but authType:%{public}d is not pin", authType_);
    }
    // ATTR_RE_ENROLL_FLAG is true means iam opened a session, remeber to close it
    InnerAccountIAMManager::GetInstance().CloseSession(userId_);
}

ErrCode AuthCallback::UnlockAccount(int32_t accountId, const std::vector<uint8_t> &token,
    const std::vector<uint8_t> &secret, bool &isUpdateVerifiedStatus)
{
    ErrCode ret = ERR_OK;
    if (authType_ == AuthType::PIN) {
        if (secret.empty()) {
            ACCOUNT_LOGI("No need to active user.");
            return ERR_OK;
        }
        (void)InnerAccountIAMManager::GetInstance().HandleFileKeyException(accountId, secret, token);
        bool isVerified = false;
        (void)IInnerOsAccountManager::GetInstance().IsOsAccountVerified(accountId, isVerified);
        bool needActivateKey = true;
        if (isVerified) {
            ret = InnerAccountIAMManager::GetInstance().CheckNeedReactivateUserKey(accountId, needActivateKey);
            if (ret != ERR_OK) {
                ReportOsAccountOperationFail(accountId, "auth", ret, "Failed to check need reactivate user key");
                ACCOUNT_LOGE("Failed to check need reactivate key, ret = %{public}d.", ret);
            }
        }

        if (needActivateKey) {
            // el2 file decryption
            ret = InnerAccountIAMManager::GetInstance().ActivateUserKey(accountId, token, secret);
            if (ret != 0 && ret != ErrNo::E_PARAMS_NULLPTR_ERR) {
                ACCOUNT_LOGE("Failed to activate user key");
                ReportOsAccountOperationFail(accountId, "auth", ret, "Failed to activate user key");
                return ret;
            }
            ret = InnerAccountIAMManager::GetInstance().PrepareStartUser(accountId);
            if (ret != 0) {
                ACCOUNT_LOGE("Failed to prepare start user");
                ReportOsAccountOperationFail(accountId, "auth", ret, "Failed to prepare start user");
            }
            isUpdateVerifiedStatus = true;
        }
    }
    ret = UnlockUserScreen(accountId, token, secret, isUpdateVerifiedStatus);
    return ret;
}

ErrCode AuthCallback::UnlockUserScreen(int32_t accountId, const std::vector<uint8_t> &token,
    const std::vector<uint8_t> &secret, bool &isUpdateVerifiedStatus)
{
    ErrCode ret = ERR_OK;
    if (!isUpdateVerifiedStatus) {
        if (authType_ == AuthType::RECOVERY_KEY) {
            ACCOUNT_LOGI("No need to unlock screen.");
            return ERR_OK;
        }
        bool lockScreenStatus = false;
        ret = InnerAccountIAMManager::GetInstance().GetLockScreenStatus(accountId, lockScreenStatus);
        if (ret != 0) {
            ReportOsAccountOperationFail(accountId, "auth", ret, "Failed to get lock status");
        }
        if (!lockScreenStatus) {
            ACCOUNT_LOGI("start unlock user screen");
            // el3\4 file decryption
            ret = InnerAccountIAMManager::GetInstance().UnlockUserScreen(accountId, token, secret);
            if (ret != 0) {
                ReportOsAccountOperationFail(accountId, "auth", ret, "Failed to unlock user");
                return ret;
            }
        }
    }
    return ret;
}

ErrCode AuthCallback::HandleAuthResult(const Attributes &extraInfo, int32_t accountId, bool &isUpdateVerifiedStatus)
{
    // domain account authentication
    if (authType_ == static_cast<AuthType>(IAMAuthType::DOMAIN)) {
        return ERR_OK;
    }
    std::vector<uint8_t> token;
    extraInfo.GetUint8ArrayValue(Attributes::ATTR_SIGNATURE, token);
    std::vector<uint8_t> secret;
    if (!extraInfo.GetUint8ArrayValue(Attributes::ATTR_ROOT_SECRET, secret)) {
        ACCOUNT_LOGW("No secret to support user unlock");
        secret.clear();
    }
    ErrCode ret = UnlockAccount(accountId, token, secret, isUpdateVerifiedStatus);
    if (ret != ERR_OK) {
        return ret;
    }
#ifdef SUPPORT_DOMAIN_ACCOUNTS
    // send msg for domain account offline authentication
    InnerDomainAccountManager::GetInstance().AuthWithToken(accountId, token);
#endif // SUPPORT_DOMAIN_ACCOUNTS
    HandleReEnroll(extraInfo, accountId, token);
    return ret;
}

void AuthCallback::SetDeathRecipient(const sptr<AuthCallbackDeathRecipient> &deathRecipient)
{
    deathRecipient_ = deathRecipient;
}

void AuthCallback::OnResult(int32_t result, const Attributes &extraInfo)
{
    int32_t authedAccountId = 0;
    if (!extraInfo.GetInt32Value(Attributes::AttributeKey::ATTR_USER_ID, authedAccountId)) {
        ACCOUNT_LOGE("Get account id from auth result failed");
        authedAccountId = static_cast<int32_t>(userId_);
    }
    ACCOUNT_LOGI("Auth ret: authType=%{public}d, result=%{public}d, id=%{public}d", authType_, result, authedAccountId);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("innerCallback_ is nullptr");
        return;
    }
    sptr<IRemoteObject> remoteObject = innerCallback_->AsObject();
    if ((deathRecipient_ != nullptr) && (remoteObject != nullptr)) {
        remoteObject->RemoveDeathRecipient(deathRecipient_);
    }
    if (result != 0) {
        innerCallback_->OnResult(result, extraInfo);
        ReportOsAccountOperationFail(authedAccountId, "auth", result,
            "Failed to auth, type:" + std::to_string(authType_));
        return AccountInfoReport::ReportSecurityInfo("", authedAccountId, ReportEvent::EVENT_LOGIN, result);
    }
    // private pin auth
    if ((authType_ == AuthType::PRIVATE_PIN) || (authIntent_ == AuthIntent::QUESTION_AUTH)) {
        ACCOUNT_LOGI("Private pin auth");
        return innerCallback_->OnResult(result, extraInfo);
    }
    if (isRemoteAuth_) {
        ACCOUNT_LOGI("Remote auth");
        return innerCallback_->OnResult(result, extraInfo);
    }
    bool isUpdateVerifiedStatus = false;
    if (HandleAuthResult(extraInfo, authedAccountId, isUpdateVerifiedStatus) != ERR_OK) {
        int32_t remainTimes = 0;
        int32_t freezingTime = 0;
        extraInfo.GetInt32Value(Attributes::AttributeKey::ATTR_REMAIN_TIMES, remainTimes);
        extraInfo.GetInt32Value(Attributes::AttributeKey::ATTR_FREEZING_TIME, freezingTime);
        Attributes errInfo;
        errInfo.SetInt32Value(Attributes::AttributeKey::ATTR_REMAIN_TIMES, remainTimes);
        errInfo.SetInt32Value(Attributes::AttributeKey::ATTR_FREEZING_TIME, freezingTime);
        innerCallback_->OnResult(ResultCode::FAIL, errInfo);
        return AccountInfoReport::ReportSecurityInfo("", authedAccountId, ReportEvent::EVENT_LOGIN, ResultCode::FAIL);
    }
    innerCallback_->OnResult(result, extraInfo);
    if (isUpdateVerifiedStatus) {
        (void)IInnerOsAccountManager::GetInstance().SetOsAccountIsVerified(authedAccountId, true);
    }
    (void)IInnerOsAccountManager::GetInstance().SetOsAccountIsLoggedIn(authedAccountId, true);
    AccountInfoReport::ReportSecurityInfo("", authedAccountId, ReportEvent::EVENT_LOGIN, result);
}

void AuthCallback::OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo)
{
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("innerCallback_ is nullptr");
        return;
    }
    innerCallback_->OnAcquireInfo(module, acquireInfo, extraInfo);
}

void IDMCallbackDeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &remote)
{
    ACCOUNT_LOGW("Remote callback died, cancel cred");
    if (userId_ > 0) {
        UserIDMClient::GetInstance().Cancel(userId_);
    }
}

IDMCallbackDeathRecipient::IDMCallbackDeathRecipient(uint32_t userId) : userId_(userId)
{}

AddCredCallback::AddCredCallback(uint32_t userId, const CredentialParameters &credInfo,
    const sptr<IIDMCallback> &callback)
    : userId_(userId), credInfo_(credInfo), innerCallback_(callback)
{}

void AddCredCallback::SetDeathRecipient(const sptr<IDMCallbackDeathRecipient> &deathRecipient)
{
    deathRecipient_ = deathRecipient;
}

static ErrCode AddUserKey(int32_t userId, uint64_t secureUid, const std::vector<uint8_t> &token,
    const std::vector<uint8_t> &oldSecret, const std::vector<uint8_t> &newSecret)
{
    ErrCode errCode = InnerAccountIAMManager::GetInstance().UpdateStorageUserAuth(
        userId, secureUid, token, oldSecret, newSecret);
    if (errCode != ERR_OK) {
        ReportOsAccountOperationFail(userId, "addCredential", errCode, "Failed to update user auth");
        return errCode;
    }
    errCode = InnerAccountIAMManager::GetInstance().UpdateStorageKeyContext(userId);
    if (errCode != ERR_OK) {
        ReportOsAccountOperationFail(userId, "addCredential", errCode, "Failed to update key context");
    }
    return errCode;
}

static inline std::string GetSecretFlagFilePath(const uint32_t userId)
{
    return Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR + std::to_string(userId) +
        Constants::PATH_SEPARATOR + Constants::USER_ADD_SECRET_FLAG_FILE_NAME;
}

void AddCredCallback::OnResult(int32_t result, const Attributes &extraInfo)
{
    std::unique_lock<std::mutex> lock(mutex_);
    ACCOUNT_LOGI("Add cred result, userId:%{public}d, authType:%{public}d, result:%{public}d.",
        userId_, credInfo_.authType, result);
    if (innerCallback_ == nullptr || innerCallback_->AsObject() == nullptr) {
        ACCOUNT_LOGE("innerCallback_ is nullptr");
        isCalled_ = true;
        return onResultCondition_.notify_one();
    }
    innerCallback_->AsObject()->RemoveDeathRecipient(deathRecipient_);
    auto &innerIamMgr = InnerAccountIAMManager::GetInstance();
    if ((result == 0) && (credInfo_.authType == AuthType::PIN)) {
        innerIamMgr.SetState(userId_, AFTER_ADD_CRED);
        uint64_t credentialId = 0;
        extraInfo.GetUint64Value(Attributes::AttributeKey::ATTR_CREDENTIAL_ID, credentialId);
        (void)IInnerOsAccountManager::GetInstance().SetOsAccountCredentialId(userId_, credentialId);
        uint64_t secureUid = 0;
        extraInfo.GetUint64Value(Attributes::AttributeKey::ATTR_SEC_USER_ID, secureUid);
        std::vector<uint8_t> newSecret;
        extraInfo.GetUint8ArrayValue(Attributes::ATTR_ROOT_SECRET, newSecret);
        std::vector<uint8_t> token;
        extraInfo.GetUint8ArrayValue(Attributes::ATTR_AUTH_TOKEN, token);
        std::vector<uint8_t> oldSecret;
        ErrCode code = AddUserKey(userId_, secureUid, token, oldSecret, newSecret);
        if (code == ERR_OK) {
            std::string path = GetSecretFlagFilePath(userId_);
            auto accountFileOperator = std::make_shared<AccountFileOperator>();
            code = accountFileOperator->DeleteDirOrFile(path);
            if (code != ERR_OK) {
                ReportOsAccountOperationFail(userId_, "addCredential", code, "Failed to delete add_secret_flag file");
            }
        }
    }
    if (result != 0) {
        ReportOsAccountOperationFail(userId_, "addCredential", result,
            "Failed to add credential, type: " + std::to_string(credInfo_.authType));
        if (credInfo_.authType == AuthType::PIN) {
            std::string path = GetSecretFlagFilePath(userId_);
            auto accountFileOperator = std::make_shared<AccountFileOperator>();
            accountFileOperator->DeleteDirOrFile(path);
        }
    } else {
        ReportOsAccountLifeCycle(userId_,
            std::string(Constants::OPERATION_ADD_CRED) + "_" + std::to_string(credInfo_.authType));
    }
    innerIamMgr.SetState(userId_, AFTER_OPEN_SESSION);
    innerCallback_->OnResult(result, extraInfo);
    isCalled_ = true;
    onResultCondition_.notify_one();
}

void AddCredCallback::OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo)
{
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("innerCallback_ is nullptr");
        return;
    }
    innerCallback_->OnAcquireInfo(module, acquireInfo, extraInfo);
}

UpdateCredCallback::UpdateCredCallback(
    uint32_t userId, const CredentialParameters &credInfo, const sptr<IIDMCallback> &callback)
    : userId_(userId), credInfo_(credInfo), innerCallback_(callback)
{}

void UpdateCredCallback::SetDeathRecipient(const sptr<IDMCallbackDeathRecipient> &deathRecipient)
{
    deathRecipient_ = deathRecipient;
}

static void DeleteCredential(uint32_t userId, uint64_t credentialId, const std::vector<uint8_t> &token)
{
    auto idmCallback = std::make_shared<DelCredCallback>(userId, true, token, nullptr);
    UserIDMClient::GetInstance().DeleteCredential(userId, credentialId, token, idmCallback);
}

void UpdateCredCallback::InnerOnResult(int32_t result, const Attributes &extraInfo)
{
    ACCOUNT_LOGI("UpdateCredCallback, userId=%{public}d, result=%{public}d.", userId_, result);
    if (innerCallback_ == nullptr || innerCallback_->AsObject() == nullptr) {
        ACCOUNT_LOGE("inner callback is nullptr");
        return;
    }
    innerCallback_->AsObject()->RemoveDeathRecipient(deathRecipient_);
    auto &innerIamMgr = InnerAccountIAMManager::GetInstance();
    if (result != 0) {
        ReportOsAccountOperationFail(userId_, "updateCredential", result,
            "Failed to update credential, type: " + std::to_string(credInfo_.authType));
    } else {
        ReportOsAccountLifeCycle(userId_,
            std::string(Constants::OPERATION_UPDATE_CRED) + "_" + std::to_string(credInfo_.authType));
    }
    if ((result != 0) || (credInfo_.authType != AuthType::PIN)) {
        ACCOUNT_LOGE("UpdateCredCallback fail code=%{public}d, authType=%{public}d", result, credInfo_.authType);
        return innerCallback_->OnResult(result, extraInfo);
    }
    innerIamMgr.SetState(userId_, AFTER_OPEN_SESSION);
    UpdateCredInfo updateCredInfo(extraInfo);
    if (updateCredInfo.oldSecret.empty()) {
        ErrCode code = innerIamMgr.UpdateUserAuthWithRecoveryKey(credInfo_.token,
            updateCredInfo.newSecret, updateCredInfo.secureUid, userId_);
        if (code != ERR_OK) {
            DeleteCredential(userId_, updateCredInfo.credentialId, credInfo_.token);
            ReportOsAccountOperationFail(userId_, "updateCredential", code,
                "Failed to update user auth with recovery key");
            return innerCallback_->OnResult(code, extraInfo);
        }
    } else {
        ErrCode code = innerIamMgr.UpdateStorageUserAuth(userId_, updateCredInfo.secureUid,
            updateCredInfo.token, updateCredInfo.oldSecret, {});
        if (code != ERR_OK) {
            DeleteCredential(userId_, updateCredInfo.credentialId, credInfo_.token);
            ReportOsAccountOperationFail(userId_, "updateCredential", code, "Failed to update user auth");
            return innerCallback_->OnResult(code, extraInfo);
        }
    }
    uint64_t oldCredentialId = 0;
    extraInfo.GetUint64Value(Attributes::AttributeKey::ATTR_OLD_CREDENTIAL_ID, oldCredentialId);
    auto idmCallback = std::make_shared<CommitCredUpdateCallback>(userId_, updateCredInfo, innerCallback_);
    Security::AccessToken::AccessTokenID selfToken = IPCSkeleton::GetSelfTokenID();
    result = SetFirstCallerTokenID(selfToken);
    ACCOUNT_LOGI("Set first caller info result: %{public}d", result);
    UserIDMClient::GetInstance().DeleteCredential(userId_, oldCredentialId, credInfo_.token, idmCallback);
    std::unique_lock<std::mutex> delLock(idmCallback->mutex_);
    idmCallback->onResultCondition_.wait(delLock, [idmCallback] { return idmCallback->isCalled_; });
}

void UpdateCredCallback::OnResult(int32_t result, const Attributes &extraInfo)
{
    std::unique_lock<std::mutex> updateLock(mutex_);
    InnerOnResult(result, extraInfo);
    isCalled_ = true;
    onResultCondition_.notify_one();
}

void UpdateCredCallback::OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo)
{
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("innerCallback_ is nullptr");
        return;
    }
    innerCallback_->OnAcquireInfo(module, acquireInfo, extraInfo);
}

VerifyTokenCallbackWrapper::VerifyTokenCallbackWrapper(uint32_t userId, const std::vector<uint8_t> &token,
    Security::AccessToken::AccessTokenID callerTokenId, const sptr<IIDMCallback> &callback)
    : userId_(userId), token_(token), callerTokenId_(callerTokenId), innerCallback_(callback)
{}

void VerifyTokenCallbackWrapper::OnResult(int32_t result, const Attributes &extraInfo)
{
    std::unique_lock<std::mutex> verifyLock(mutex_);
    InnerOnResult(result, extraInfo);
    isCalled_ = true;
    onResultCondition_.notify_one();
}

void VerifyTokenCallbackWrapper::InnerOnResult(int32_t result, const Attributes &extraInfo)
{
    ACCOUNT_LOGI("Verify token result = %{public}d, userId = %{public}d", result, userId_);
    if (result != ERR_OK) {
        ReportOsAccountOperationFail(userId_, "deleteCredential", result, "Failed to verify token");
        return innerCallback_->OnResult(result, extraInfo);
    }
    uint64_t secureUid = 0;
    extraInfo.GetUint64Value(Attributes::AttributeKey::ATTR_SEC_USER_ID, secureUid);
    std::vector<uint8_t> rootSecret;
    extraInfo.GetUint8ArrayValue(Attributes::ATTR_ROOT_SECRET, rootSecret);
    auto &innerIamMgr = InnerAccountIAMManager::GetInstance();
    ErrCode errCode = innerIamMgr.UpdateStorageUserAuth(userId_, secureUid, token_, rootSecret, {});
    if (errCode != ERR_OK) {
        ReportOsAccountOperationFail(userId_, "deleteCredential", errCode, "Failed to update user auth");
        Attributes emptyExtraInfo;
        return innerCallback_->OnResult(ResultCode::FAIL, emptyExtraInfo);
    }
    result = SetFirstCallerTokenID(callerTokenId_);
    ACCOUNT_LOGI("Set first caller info result: %{public}d", result);
    auto deleteUserCallback = std::make_shared<CommitDelCredCallback>(userId_, innerCallback_);
    UserIDMClient::GetInstance().DeleteUser(userId_, token_, deleteUserCallback);
    std::unique_lock<std::mutex> lock(deleteUserCallback->mutex_);
    deleteUserCallback->onResultCondition_.wait(lock, [deleteUserCallback] { return deleteUserCallback->isCalled_; });
}

CommitDelCredCallback::CommitDelCredCallback(uint32_t userId, const sptr<IIDMCallback> callback)
    : userId_(userId), innerCallback_(callback)
{}

void CommitDelCredCallback::OnResult(int32_t result, const UserIam::UserAuth::Attributes &extraInfo)
{
    std::unique_lock<std::mutex> lock(mutex_);
    ACCOUNT_LOGI("Commit delete user result = %{public}d, userId = %{public}d", result, userId_);
    isCalled_ = true;
    if (result != ERR_OK) {
        ReportOsAccountOperationFail(userId_, "deleteCredential", result, "Failed to delete user's credential");
    } else {
        ReportOsAccountLifeCycle(userId_, std::string(Constants::OPERATION_DELETE_CRED) + "_0" + "_commit");
        ErrCode errCode = InnerAccountIAMManager::GetInstance().UpdateStorageKeyContext(userId_);
        if (errCode != ERR_OK) {
            ReportOsAccountOperationFail(userId_, "deleteCredential", errCode, "Failed to update key context");
        }
        (void)IInnerOsAccountManager::GetInstance().SetOsAccountCredentialId(userId_, 0);
    }
    innerCallback_->OnResult(result, extraInfo);
    onResultCondition_.notify_one();
}

void CommitDelCredCallback::OnAcquireInfo(int32_t module, uint32_t acquireInfo,
    const UserIam::UserAuth::Attributes &extraInfo)
{
    ACCOUNT_LOGI("IAM OnAcquireInfo callback! module %{public}d, acquire %{public}u.", module, acquireInfo);
}

CommitCredUpdateCallback::CommitCredUpdateCallback(int32_t userId,
    const UpdateCredInfo &extraUpdateInfo, const sptr<IIDMCallback> &callback)
    : userId_(userId), extraUpdateInfo_(extraUpdateInfo), innerCallback_(callback)
{}

void CommitCredUpdateCallback::InnerOnResult(int32_t result, const Attributes &extraInfo)
{
    ACCOUNT_LOGI("CommitCredUpdateCallback, result=%{public}d.", result);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("innerCallback_ is nullptr");
        return;
    }

    auto &innerIamMgr = InnerAccountIAMManager::GetInstance();
    if (result != 0) {
        ACCOUNT_LOGE("CommitCredUpdateCallback fail code=%{public}d", result);
        ReportOsAccountOperationFail(userId_, std::string(Constants::OPERATION_UPDATE_CRED) + "_commit",
            result, "Failed to commit credential update");
        innerCallback_->OnResult(result, extraInfo);
        innerIamMgr.SetState(userId_, AFTER_OPEN_SESSION);
        return;
    } else {
        ReportOsAccountLifeCycle(userId_,
            std::string(Constants::OPERATION_UPDATE_CRED) + "_" + std::to_string(AuthType::PIN) + "_commit");
    }
    if (!extraUpdateInfo_.oldSecret.empty()) {
        ErrCode code = innerIamMgr.UpdateStorageUserAuth(
            userId_, extraUpdateInfo_.secureUid, extraUpdateInfo_.token, {}, extraUpdateInfo_.newSecret);
        if (code != ERR_OK) {
            ACCOUNT_LOGE("Fail to update user auth, userId=%{public}d, code=%{public}d", userId_, code);
            innerIamMgr.SetState(userId_, AFTER_OPEN_SESSION);
            ReportOsAccountOperationFail(userId_, std::string(Constants::OPERATION_UPDATE_CRED) + "_commit",
                code, "Failed to update user auth");
            innerCallback_->OnResult(code, extraInfo);
            return;
        }
    }
    (void)IInnerOsAccountManager::GetInstance().SetOsAccountCredentialId(userId_, extraUpdateInfo_.credentialId);
    Attributes extraInfoResult;
    extraInfoResult.SetUint64Value(Attributes::AttributeKey::ATTR_CREDENTIAL_ID, extraUpdateInfo_.credentialId);
    innerIamMgr.SetState(userId_, AFTER_UPDATE_CRED);
    innerCallback_->OnResult(result, extraInfoResult);
    ErrCode updateRet = innerIamMgr.UpdateStorageKeyContext(userId_);
    if (updateRet != ERR_OK) {
        ReportOsAccountOperationFail(userId_, std::string(Constants::OPERATION_UPDATE_CRED) + "_commit",
            updateRet, "Failed to update key context");
    }
}

void CommitCredUpdateCallback::OnResult(int32_t result, const Attributes &extraInfo)
{
    std::unique_lock<std::mutex> lock(mutex_);
    InnerOnResult(result, extraInfo);
    isCalled_ = true;
    onResultCondition_.notify_one();
}

void CommitCredUpdateCallback::OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo)
{
    ACCOUNT_LOGE("CommitCredUpdateCallback OnAcquireInfo");
}

DelCredCallback::DelCredCallback(int32_t userId, bool isPIN, std::vector<uint8_t> token,
    const sptr<IIDMCallback> &callback)
    : userId_(userId), isPIN_(isPIN), token_(token), innerCallback_(callback)
{}

void DelCredCallback::OnResult(int32_t result, const Attributes &extraInfo)
{
    ACCOUNT_LOGI("DelCredCallback, result=%{public}d, userId=%{public}d", result, userId_);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("innerCallback_ is nullptr");
        return;
    }
    auto &innerIamMgr = InnerAccountIAMManager::GetInstance();
    if ((result == 0) && isPIN_) {
        (void)IInnerOsAccountManager::GetInstance().SetOsAccountCredentialId(userId_, 0);  // 0-invalid credentialId
        std::vector<uint8_t> newSecret;
        std::vector<uint8_t> oldSecret;
        extraInfo.GetUint8ArrayValue(Attributes::ATTR_OLD_ROOT_SECRET, oldSecret);
        uint64_t secureUid = 0;
        extraInfo.GetUint64Value(Attributes::AttributeKey::ATTR_SEC_USER_ID, secureUid);
        ErrCode updateRet = innerIamMgr.UpdateStorageUserAuth(userId_, secureUid, token_, oldSecret, newSecret);
        if (updateRet != ERR_OK) {
            ReportOsAccountOperationFail(userId_, "deleteCredential", updateRet, "Failed to update user auth");
        }
        updateRet = innerIamMgr.UpdateStorageKeyContext(userId_);
        if (updateRet != ERR_OK) {
            ReportOsAccountOperationFail(userId_, "deleteCredential", updateRet, "Failed to update key context");
        }
    }
    if (result != 0) {
        ACCOUNT_LOGE("DelCredCallback fail code=%{public}d, userId=%{public}d", result, userId_);
        ReportOsAccountOperationFail(userId_, "deleteCredential", result, "Failed to delete credential");
    } else {
        ReportOsAccountLifeCycle(userId_, std::string(Constants::OPERATION_DELETE_CRED));
    }

    innerIamMgr.SetState(userId_, AFTER_OPEN_SESSION);
    innerCallback_->OnResult(result, extraInfo);
}

void DelCredCallback::OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo)
{
    ACCOUNT_LOGI("DelCredCallback, userId=%{public}d", userId_);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("innerCallback_ is nullptr");
        return;
    }
    innerCallback_->OnAcquireInfo(module, acquireInfo, extraInfo);
}

GetCredInfoCallbackWrapper::GetCredInfoCallbackWrapper(
    int32_t userId, int32_t authType, const sptr<IGetCredInfoCallback> &callback)
    : userId_(userId), authType_(authType), innerCallback_(callback)
{}

void GetCredInfoCallbackWrapper::OnCredentialInfo(int32_t result, const std::vector<CredentialInfo> &infoList)
{
    ACCOUNT_LOGI("Get credential info ret:%{public}d, userId:%{public}d, authType:%{public}d",
        result, userId_, authType_);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("InnerCallback_ is nullptr");
        return;
    }
    if (result != 0) {
        REPORT_OS_ACCOUNT_FAIL(userId_, "getCredentialInfo", result,
            "Failed to get credential info, authType:" + std::to_string(authType_));
    }
    if (result == ERR_IAM_NOT_ENROLLED) {
        result = 0;
    }
    if ((result == 0) && (authType_ == 0)) {
        bool isAvailable = InnerAccountIAMManager::GetInstance().CheckDomainAuthAvailable(userId_);
        if (isAvailable) {
            ACCOUNT_LOGI("Domain auth is support");
            std::vector<CredentialInfo> newInfoList = infoList;
            CredentialInfo info;
            info.authType = static_cast<AuthType>(IAMAuthType::DOMAIN);
            info.pinType = static_cast<PinSubType>(IAMAuthSubType::DOMAIN_MIXED);
            newInfoList.emplace_back(info);
            return innerCallback_->OnCredentialInfo(result, newInfoList);
        }
    }
    return innerCallback_->OnCredentialInfo(result, infoList);
}

GetPropCallbackWrapper::GetPropCallbackWrapper(int32_t userId, const sptr<IGetSetPropCallback> &callback)
    : userId_(userId), innerCallback_(callback)
{}

void GetPropCallbackWrapper::OnResult(int32_t result, const Attributes &extraInfo)
{
    ACCOUNT_LOGI("Get property, result:%{public}d, userId:%{public}d", result, userId_);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("inner callback is nullptr");
        return;
    }
    if (result != 0) {
        ReportOsAccountOperationFail(userId_, "getProperty", result, "Failed to get property");
    }
    innerCallback_->OnResult(result, extraInfo);
}

SetPropCallbackWrapper::SetPropCallbackWrapper(int32_t userId, const sptr<IGetSetPropCallback> &callback)
    : userId_(userId), innerCallback_(callback)
{}

void SetPropCallbackWrapper::OnResult(int32_t result, const Attributes &extraInfo)
{
    ACCOUNT_LOGI("Set property, result:%{public}d, userId:%{public}d", result, userId_);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("inner callback is nullptr");
        return;
    }
    if (result != 0) {
        ReportOsAccountOperationFail(userId_, "setProperty", result, "Failed to set property");
    }
    innerCallback_->OnResult(result, extraInfo);
}

GetSecUserInfoCallbackWrapper::GetSecUserInfoCallbackWrapper(
    int32_t userId, AuthType authType, const sptr<IGetEnrolledIdCallback> &callback)
    : userId_(userId), authType_(authType), innerCallback_(callback)
{}

void GetSecUserInfoCallbackWrapper::OnSecUserInfo(int32_t result, const SecUserInfo &info)
{
    ACCOUNT_LOGI("Get sec user info, result:%{public}d, authType_:%{public}d", result, authType_);
    static_cast<void>(result);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("Inner callback is nullptr");
        return;
    }
    if (result != 0) {
        REPORT_OS_ACCOUNT_FAIL(userId_, "getEnrolledId", result,
            "Failed to get sec user info, type = " + std::to_string(authType_));
    }
    auto it = std::find_if(info.enrolledInfo.begin(), info.enrolledInfo.end(), [this](const auto& item) {
        return item.authType == authType_;
    });
    if (it != info.enrolledInfo.end()) {
        return innerCallback_->OnEnrolledId(ERR_OK, it->enrolledId);
    } else {
        return innerCallback_->OnEnrolledId(ERR_IAM_NOT_ENROLLED, 0);
    }
}

GetSecureUidCallback::GetSecureUidCallback(int32_t userId): userId_(userId)
{}

void GetSecureUidCallback::OnSecUserInfo(int32_t result, const SecUserInfo &info)
{
    static_cast<void>(result);
    ACCOUNT_LOGI("SecUserInfo call back userId=%{public}d", userId_);
    std::unique_lock<std::mutex> lck(secureMtx_);
    this->secureUid_ = info.secureUid;
    isCalled_ = true;
    secureCv_.notify_all();
}

PrepareRemoteAuthCallbackWrapper::PrepareRemoteAuthCallbackWrapper(const sptr<IPreRemoteAuthCallback> &callback)
    : innerCallback_(callback)
{}

void PrepareRemoteAuthCallbackWrapper::OnResult(int32_t result)
{
    ACCOUNT_LOGI("Prepare remote auth, result:%{public}d.", result);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("Inner callback is nullptr.");
        return;
    }
    if (result != 0) {
        ACCOUNT_LOGE("PrepareRemoteAuth, result=%{public}d fail to prepare remote auth.", result);
        REPORT_OS_ACCOUNT_FAIL(0, "prepareRemoteAuth", result, "Failed to prepare remote auth");
    }
    innerCallback_->OnResult(result);
}

#ifdef SUPPORT_DOMAIN_ACCOUNTS
GetDomainAuthStatusInfoCallback::GetDomainAuthStatusInfoCallback(
    const GetPropertyRequest &request, const sptr<IGetSetPropCallback> &callback)
    : request_(request), innerCallback_(callback)
{}

void GetDomainAuthStatusInfoCallback::OnResult(int32_t result, Parcel &parcel)
{
    ACCOUNT_LOGI("Get domain auth status info, result=%{public}d.", result);
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("inner callback is nullptr");
        return;
    }
    Attributes attributes;
    std::shared_ptr<AuthStatusInfo> infoPtr(AuthStatusInfo::Unmarshalling(parcel));
    if (infoPtr == nullptr) {
        ACCOUNT_LOGE("Unmarshalling parcel to auth status info failed.");
        innerCallback_->OnResult(ERR_ACCOUNT_COMMON_READ_PARCEL_ERROR, attributes);
        return;
    }
    attributes.SetInt32Value(Attributes::ATTR_PIN_SUB_TYPE, static_cast<int32_t>(IAMAuthSubType::DOMAIN_MIXED));
    attributes.SetInt32Value(Attributes::ATTR_REMAIN_TIMES, infoPtr->remainingTimes);
    attributes.SetInt32Value(Attributes::ATTR_FREEZING_TIME, infoPtr->freezingTime);
    innerCallback_->OnResult(result, attributes);
}
#endif // SUPPORT_DOMAIN_ACCOUNTS
}  // namespace AccountSA
}  // namespace OHOS
