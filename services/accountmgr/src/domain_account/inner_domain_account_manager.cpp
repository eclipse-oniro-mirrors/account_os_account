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

#include "inner_domain_account_manager.h"
#include <pthread.h>
#include <thread>
#include <securec.h>
#include "account_info_report.h"
#include "account_log_wrapper.h"
#include "domain_account_plugin_death_recipient.h"
#include "domain_account_callback_service.h"
#include "domain_has_domain_info_callback.h"
#include "idomain_account_callback.h"
#include "iinner_os_account_manager.h"
#include "inner_account_iam_manager.h"
#include "ipc_skeleton.h"
#include "status_listener_manager.h"

namespace OHOS {
namespace AccountSA {
namespace {
const char THREAD_AUTH[] = "auth";
const char THREAD_INNER_AUTH[] = "innerAuth";
const char THREAD_HAS_ACCOUNT[] = "hasAccount";
const char THREAD_GET_ACCOUNT[] = "getAccount";
const char THREAD_BIND_ACCOUNT[] = "bindAccount";
const char THREAD_UNBIND_ACCOUNT[] = "unbindAccount";
const char THREAD_GET_ACCESS_TOKEN[] = "getAccessToken";
const char THREAD_IS_ACCOUNT_VALID[] = "isAccountTokenValid";
}

InnerDomainAccountManager::InnerDomainAccountManager()
{}

InnerDomainAccountManager &InnerDomainAccountManager::GetInstance()
{
    static InnerDomainAccountManager *instance = new (std::nothrow) InnerDomainAccountManager();
    return *instance;
}

InnerDomainAuthCallback::InnerDomainAuthCallback(int32_t userId, const sptr<IDomainAccountCallback> &callback)
    : userId_(userId), callback_(callback)
{}

InnerDomainAuthCallback::~InnerDomainAuthCallback()
{}

void InnerDomainAuthCallback::OnResult(const int32_t errCode, Parcel &parcel)
{
    std::shared_ptr<DomainAuthResult> authResult(DomainAuthResult::Unmarshalling(parcel));
    if (authResult == nullptr) {
        ACCOUNT_LOGE("authResult is nullptr");
        return;
    }
    if ((errCode == ERR_OK) && (userId_ != 0)) {
        InnerDomainAccountManager::GetInstance().InsertTokenToMap(userId_, (*authResult).token);
        DomainAccountInfo domainInfo;
        InnerDomainAccountManager::GetInstance().GetDomainAccountInfoByUserId(userId_, domainInfo);
        InnerDomainAccountManager::GetInstance().NotifyDomainAccountEvent(
            userId_, DomainAccountEvent::LOG_IN, DomainAccountStatus::LOG_END, domainInfo);
        bool isActivated = false;
        (void)IInnerOsAccountManager::GetInstance().IsOsAccountActived(userId_, isActivated);
        DomainAccountStatus status = isActivated ? DomainAccountStatus::LOGIN : DomainAccountStatus::LOGIN_BACKGROUND;
        IInnerOsAccountManager::GetInstance().UpdateAccountStatusForDomain(userId_, status);
    }
    (void)memset_s(authResult->token.data(), authResult->token.size(), 0, authResult->token.size());
    authResult->token.clear();
    Parcel resultParcel;
    if (!(*authResult).Marshalling(resultParcel)) {
        ACCOUNT_LOGE("authResult Marshalling failed");
        return;
    }
    AccountInfoReport::ReportSecurityInfo("", userId_, ReportEvent::EVENT_LOGIN, errCode);
    if (callback_ == nullptr) {
        ACCOUNT_LOGI("callback_ is nullptr");
        return;
    }
    return callback_->OnResult(errCode, resultParcel);
}

ErrCode InnerDomainAccountManager::RegisterPlugin(const sptr<IDomainAccountPlugin> &plugin)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (plugin == nullptr) {
        ACCOUNT_LOGE("the registered plugin is nullptr");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    if (plugin_ != nullptr) {
        ACCOUNT_LOGE("plugin already exists");
        return ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_ALREADY_EXIST;
    }
    auto deathRecipient = GetDeathRecipient();
    if ((plugin->AsObject()->IsProxyObject()) &&
        ((deathRecipient == nullptr) || (!plugin->AsObject()->AddDeathRecipient(deathRecipient)))) {
        ACCOUNT_LOGE("failed to add death recipient for plugin");
        return ERR_ACCOUNT_COMMON_ADD_DEATH_RECIPIENT;
    }
    plugin_ = plugin;
    callingUid_ = IPCSkeleton::GetCallingUid();
    return ERR_OK;
}

void InnerDomainAccountManager::UnregisterPlugin()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if ((plugin_ != nullptr) && (plugin_->AsObject() != nullptr)) {
        plugin_->AsObject()->RemoveDeathRecipient(deathRecipient_);
    }
    plugin_ = nullptr;
    callingUid_ = -1;
    deathRecipient_ = nullptr;
}

ErrCode InnerDomainAccountManager::StartAuth(const sptr<IDomainAccountPlugin> &plugin, const DomainAccountInfo &info,
    const std::vector<uint8_t> &authData, const sptr<IDomainAccountCallback> &callback, AuthMode authMode)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("invalid callback, cannot return result to client");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    Parcel emptyParcel;
    AccountSA::DomainAuthResult emptyResult;
    if (!emptyResult.Marshalling(emptyParcel)) {
        ACCOUNT_LOGE("authResult Marshalling failed");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    if (plugin == nullptr) {
        ACCOUNT_LOGE("plugin not exists");
        callback->OnResult(ConvertToJSErrCode(ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST), emptyParcel);
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    ErrCode errCode = ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    switch (authMode) {
        case AUTH_WITH_CREDENTIAL_MODE:
            errCode = plugin->Auth(info, authData, callback);
            break;
        case AUTH_WITH_POPUP_MODE:
            errCode = plugin->AuthWithPopup(info, callback);
            break;
        case AUTH_WITH_TOKEN_MODE:
            errCode = plugin->AuthWithToken(info, authData, callback);
            break;
        default:
            break;
    }
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("failed to auth domain account, errCode: %{public}d", errCode);
        callback->OnResult(ConvertToJSErrCode(errCode), emptyParcel);
        return errCode;
    }
    return ERR_OK;
}

ErrCode InnerDomainAccountManager::GetDomainAccountInfoByUserId(int32_t userId, DomainAccountInfo &domainInfo)
{
    OsAccountInfo accountInfo;
    ErrCode errCode = IInnerOsAccountManager::GetInstance().QueryOsAccountById(userId, accountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get os account info failed, errCode: %{public}d", errCode);
        return errCode;
    }
    accountInfo.GetDomainInfo(domainInfo);
    if (domainInfo.accountName_.empty()) {
        ACCOUNT_LOGE("the target user is not a domain account");
        return ERR_DOMAIN_ACCOUNT_SERVICE_NOT_DOMAIN_ACCOUNT;
    }
    return ERR_OK;
}

ErrCode InnerDomainAccountManager::Auth(const DomainAccountInfo &info, const std::vector<uint8_t> &password,
    const sptr<IDomainAccountCallback> &callback)
{
    int32_t userId = -1;
    sptr<IDomainAccountCallback> innerCallback = callback;
    IInnerOsAccountManager::GetInstance().GetOsAccountLocalIdFromDomain(info, userId);
    if (userId >= 0) {
        innerCallback = new (std::nothrow) InnerDomainAuthCallback(userId, callback);
        if (innerCallback == nullptr) {
            ACCOUNT_LOGE("failed to create innerCallback");
            innerCallback = callback;
        }
    }
    auto task = std::bind(
        &InnerDomainAccountManager::StartAuth, this, plugin_, info, password, innerCallback, AUTH_WITH_CREDENTIAL_MODE);
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), THREAD_AUTH);
    taskThread.detach();
    return ERR_OK;
}

ErrCode InnerDomainAccountManager::InnerAuth(int32_t userId, const std::vector<uint8_t> &authData,
    const sptr<IDomainAccountCallback> &callback, AuthMode authMode)
{
    DomainAccountInfo domainInfo;
    ErrCode errCode = GetDomainAccountInfoByUserId(userId, domainInfo);
    if (errCode != ERR_OK) {
        return errCode;
    }
    sptr<InnerDomainAuthCallback> innerCallback = new (std::nothrow) InnerDomainAuthCallback(userId, callback);
    if (innerCallback == nullptr) {
        ACCOUNT_LOGE("failed to create innerCallback");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    auto task =
        std::bind(&InnerDomainAccountManager::StartAuth, this, plugin_, domainInfo, authData, innerCallback, authMode);
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), THREAD_INNER_AUTH);
    taskThread.detach();
    return ERR_OK;
}

ErrCode InnerDomainAccountManager::AuthUser(int32_t userId, const std::vector<uint8_t> &password,
    const sptr<IDomainAccountCallback> &callback)
{
    bool isVerified = false;
    (void) IInnerOsAccountManager::GetInstance().IsOsAccountVerified(userId, isVerified);
    if (isVerified) {
        return InnerAuth(userId, password, callback, AUTH_WITH_CREDENTIAL_MODE);
    }

    uint64_t credentialId = 0;
    (void) IInnerOsAccountManager::GetInstance().GetOsAccountCredentialId(userId, credentialId);
    if (credentialId > 0) {
        ACCOUNT_LOGE("unsupported auth type");
        return ERR_ACCOUNT_IAM_UNSUPPORTED_AUTH_TYPE;
    }
    return InnerAuth(userId, password, callback, AUTH_WITH_CREDENTIAL_MODE);
}

ErrCode InnerDomainAccountManager::AuthWithPopup(int32_t userId, const sptr<IDomainAccountCallback> &callback)
{
    if (userId == 0) {
        std::vector<int32_t> userIds;
        (void)IInnerOsAccountManager::GetInstance().QueryActiveOsAccountIds(userIds);
        if (userIds.empty()) {
            ACCOUNT_LOGE("fail to get activated os account ids");
            return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
        }
        userId = userIds[0];
    }
    return InnerAuth(userId, {}, callback, AUTH_WITH_POPUP_MODE);
}

ErrCode InnerDomainAccountManager::AuthWithToken(int32_t userId, const std::vector<uint8_t> &token)
{
    return InnerAuth(userId, token, nullptr, AUTH_WITH_TOKEN_MODE);
}

void InnerDomainAccountManager::InsertTokenToMap(int32_t userId, const std::vector<uint8_t> &token)
{
    std::lock_guard<std::mutex> lock(mutex_);
    userTokenMap_[userId] = token;
}

bool InnerDomainAccountManager::GetTokenFromMap(int32_t userId, std::vector<uint8_t> &token)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = userTokenMap_.find(userId);
    if (it == userTokenMap_.end()) {
        token.clear();
        return false;
    }
    token = it->second;
    return true;
}

void InnerDomainAccountManager::RemoveTokenFromMap(int32_t userId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    userTokenMap_.erase(userId);
    return;
}

void InnerDomainAccountManager::NotifyDomainAccountEvent(
    int32_t userId, DomainAccountEvent event, DomainAccountStatus status, const DomainAccountInfo &info)
{
    if (status == DomainAccountStatus::LOG_END) {
        bool isActivated = false;
        (void)IInnerOsAccountManager::GetInstance().IsOsAccountActived(userId, isActivated);
        status = isActivated ? DomainAccountStatus::LOGIN : DomainAccountStatus::LOGIN_BACKGROUND;
    }

    // There is not need to check userid.
    DomainAccountEventData report;
    report.domainAccountInfo = info;
    report.event = event;
    report.status = status;
    report.userId = userId;
    StatusListenerManager::GetInstance().NotifyEventAsync(report);
}

ErrCode InnerDomainAccountManager::UpdateAccountToken(const DomainAccountInfo &info, const std::vector<uint8_t> &token)
{
    if (plugin_ == nullptr) {
        ACCOUNT_LOGE("plugin is not exit!");
        return ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST;
    }
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    if (callingUid != callingUid_) {
        ACCOUNT_LOGE("callingUid and register callinguid is not same!");
        return ERR_DOMAIN_ACCOUNT_SERVICE_INVALID_CALLING_UID;
    }
    int32_t userId = 0;
    ErrCode result = IInnerOsAccountManager::GetInstance().GetOsAccountLocalIdFromDomain(info, userId);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("get os account localId from domain failed, result: %{public}d", result);
        return result;
    }

    if (token.empty()) {
        RemoveTokenFromMap(userId);
        NotifyDomainAccountEvent(userId, DomainAccountEvent::TOKEN_INVALID, DomainAccountStatus::LOGOUT, info);
        IInnerOsAccountManager::GetInstance().UpdateAccountStatusForDomain(userId, DomainAccountStatus::LOGOUT);
        return ERR_OK;
    }
    InsertTokenToMap(userId, token);
    NotifyDomainAccountEvent(userId, DomainAccountEvent::TOKEN_UPDATED, DomainAccountStatus::LOG_END, info);
    return ERR_OK;
}

static void OnResultForGetAccessToken(const ErrCode errCode, const sptr<IDomainAccountCallback> &callback)
{
    std::vector<uint8_t> token;
    Parcel emptyParcel;
    emptyParcel.WriteUInt8Vector(token);
    callback->OnResult(errCode, emptyParcel);
}

ErrCode InnerDomainAccountManager::StartGetAccessToken(const sptr<IDomainAccountPlugin> &plugin,
    const std::vector<uint8_t> &accountToken, const DomainAccountInfo &info, const GetAccessTokenOptions &option,
    const sptr<IDomainAccountCallback> &callback)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("invalid callback");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    if (plugin == nullptr) {
        ACCOUNT_LOGE("plugin is nullptr");
        OnResultForGetAccessToken(ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST, callback);
        return ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST;
    }
    DomainAccountCallbackFunc callbackFunc = [=](const int32_t errCode, Parcel &parcel) {
        if (callback != nullptr) {
            callback->OnResult(errCode, parcel);
        }
    };
    sptr<DomainAccountCallbackService> callbackService =
        new (std::nothrow) DomainAccountCallbackService(callbackFunc);
    if (callbackService == nullptr) {
        ACCOUNT_LOGE("make shared DomainAccountCallbackService failed");
        OnResultForGetAccessToken(ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST, callback);
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    ErrCode result = plugin->GetAccessToken(info, accountToken, option, callbackService);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("failed to get access token, errCode: %{public}d", result);
        OnResultForGetAccessToken(result, callback);
        return result;
    }
    return ERR_OK;
}

ErrCode InnerDomainAccountManager::GetAccessToken(
    const DomainAccountInfo &info, const AAFwk::WantParams &parameters, const sptr<IDomainAccountCallback> &callback)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("invalid callback");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    int32_t userId = 0;
    ErrCode result = ERR_OK;
    DomainAccountInfo targetInfo = info;
    if (!info.accountName_.empty()) {
        result = IInnerOsAccountManager::GetInstance().GetOsAccountLocalIdFromDomain(info, userId);
        if (result != ERR_OK) {
            ACCOUNT_LOGE("domain account not found");
            return result;
        }
    } else {
        userId = callingUid / UID_TRANSFORM_DIVISOR;
        OsAccountInfo osAccountInfo;
        (void) IInnerOsAccountManager::GetInstance().QueryOsAccountById(userId, osAccountInfo);
        osAccountInfo.GetDomainInfo(targetInfo);
        if (targetInfo.accountName_.empty()) {
            ACCOUNT_LOGE("domain account not found");
            return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
        }
    }
    std::vector<uint8_t> accountToken;
    if (!GetTokenFromMap(userId, accountToken)) {
        ACCOUNT_LOGE("the target domain account has not authenticated");
        return ERR_ACCOUNT_COMMON_NOT_AUTHENTICATED;
    }
    GetAccessTokenOptions option(callingUid, parameters);
    auto task = std::bind(&InnerDomainAccountManager::StartGetAccessToken,
        this, plugin_, accountToken, targetInfo, option, callback);
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), THREAD_GET_ACCESS_TOKEN);
    taskThread.detach();
    return ERR_OK;
}

static void ErrorOnResult(const ErrCode errCode, const sptr<IDomainAccountCallback> &callback)
{
    Parcel emptyParcel;
    emptyParcel.WriteBool(false);
    callback->OnResult(errCode, emptyParcel);
}

void CheckUserTokenCallback::OnResult(int32_t result, Parcel &parcel)
{
    ACCOUNT_LOGI("enter");
    if (result == ERR_OK) {
        isValid_ = parcel.ReadBool();
    }
    NotifyCallbackEnd();
}

bool CheckUserTokenCallback::GetValidity(void)
{
    return isValid_;
}

void CheckUserTokenCallback::WaitForCallbackResult()
{
    std::unique_lock<std::mutex> lock(lock_);
    condition_.wait(lock, [this] {
        return threadInSleep_ == false;
    });
    ACCOUNT_LOGI("WaitForCallbackResult.");
}

void CheckUserTokenCallback::NotifyCallbackEnd()
{
    std::unique_lock<std::mutex> lock(lock_);
    if (threadInSleep_) {
        ACCOUNT_LOGI("threadInSleep_ set false.");
        threadInSleep_ = false;
        condition_.notify_one();
    }
}

ErrCode InnerDomainAccountManager::CheckUserToken(const std::vector<uint8_t> &token, bool &isValid, int32_t userId)
{
    std::shared_ptr<CheckUserTokenCallback> callback = std::make_shared<CheckUserTokenCallback>();
    sptr<DomainAccountCallbackService> callbackService = new (std::nothrow) DomainAccountCallbackService(callback);
    if (callbackService == nullptr) {
        ACCOUNT_LOGE("make shared DomainAccountCallbackService failed");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }

    ErrCode errCode = ERR_OK;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (plugin_ == nullptr) {
            ACCOUNT_LOGE("plugin not exists");
            return ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST;
        }

        OsAccountInfo osAccountInfo;
        errCode = IInnerOsAccountManager::GetInstance().GetOsAccountInfoById(userId, osAccountInfo);
        if (errCode != ERR_OK) {
            return errCode;
        }
        DomainAccountInfo info;
        osAccountInfo.GetDomainInfo(info);
        errCode = plugin_->IsAccountTokenValid(info, token, callbackService);
    }
    callback->WaitForCallbackResult();
    isValid = callback->GetValidity();
    return errCode;
}

ErrCode InnerDomainAccountManager::GetAccountStatus(const DomainAccountInfo &info, DomainAccountStatus &status)
{
    status = DomainAccountStatus::LOGOUT;

    int32_t userId = 0;
    ErrCode res = IInnerOsAccountManager::GetInstance().GetOsAccountLocalIdFromDomain(info, userId);
    if (res != ERR_OK) {
        return res;
    }
    std::vector<uint8_t> token;
    if (!GetTokenFromMap(userId, token)) {
        ACCOUNT_LOGI("the target domain account has not authenticated");
        return ERR_OK;
    }

    bool isValid = false;
    res = CheckUserToken(token, isValid, userId);
    if (!isValid) {
        ACCOUNT_LOGI("Token is invalid.");
        return res;
    }

    bool isActivated = false;
    res = IInnerOsAccountManager::GetInstance().IsOsAccountActived(userId, isActivated);
    if (isActivated) {
        status = DomainAccountStatus::LOGIN;
    } else {
        status = DomainAccountStatus::LOGIN_BACKGROUND;
    }
    return res;
}

ErrCode InnerDomainAccountManager::RegisterAccountStatusListener(const sptr<IDomainAccountCallback> &listener)
{
    return StatusListenerManager::GetInstance().InsertListenerToRecords(listener->AsObject());
}

ErrCode InnerDomainAccountManager::UnregisterAccountStatusListener(const sptr<IDomainAccountCallback> &listener)
{
    // userid may be removed already.
    return StatusListenerManager::GetInstance().RemoveListenerByListener(listener->AsObject());
}

ErrCode InnerDomainAccountManager::GetAuthStatusInfo(
    const DomainAccountInfo &info, const std::shared_ptr<DomainAccountCallback> &callback)
{
    sptr<IDomainAccountCallback> callbackService =
        new (std::nothrow) DomainAccountCallbackService(callback);
    if (callbackService == nullptr) {
        ACCOUNT_LOGE("failed to create DomainAccountCallbackService");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (plugin_ == nullptr) {
        ACCOUNT_LOGE("plugin not exists");
        return ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST;
    }
    return plugin_->GetAuthStatusInfo(info, callbackService);
}

sptr<IRemoteObject::DeathRecipient> InnerDomainAccountManager::GetDeathRecipient()
{
    if (deathRecipient_ != nullptr) {
        return deathRecipient_;
    }
    deathRecipient_ = new (std::nothrow) DomainAccountPluginDeathRecipient();
    return deathRecipient_;
}

bool InnerDomainAccountManager::IsPluginAvailable()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return plugin_ != nullptr;
}

ErrCode InnerDomainAccountManager::StartHasDomainAccount(const sptr<IDomainAccountPlugin> &plugin,
    const GetDomainAccountInfoOptions &options, const sptr<IDomainAccountCallback> &callback)
{
    if (callback == nullptr) {
        ACCOUNT_LOGE("invalid callback");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    if (plugin == nullptr) {
        ACCOUNT_LOGE("plugin is nullptr");
        ErrorOnResult(ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST, callback);
        return ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST;
    }
    auto callbackWrapper = std::make_shared<DomainHasDomainInfoCallback>(
        callback, options.accountInfo.domain_, options.accountInfo.accountName_);
    if (callbackWrapper == nullptr) {
        ACCOUNT_LOGE("make shared DomainHasDomainInfoCallback failed");
        ErrorOnResult(ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR, callback);
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    sptr<DomainAccountCallbackService> callbackService =
        new (std::nothrow) DomainAccountCallbackService(callbackWrapper);
    if (callbackService == nullptr) {
        ACCOUNT_LOGE("make shared DomainAccountCallbackService failed");
        ErrorOnResult(ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR, callback);
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    ErrCode result = plugin->GetDomainAccountInfo(options, callbackService);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("failed to get domain account, errCode: %{public}d", result);
        ErrorOnResult(result, callback);
        return result;
    }
    return ERR_OK;
}

ErrCode InnerDomainAccountManager::HasDomainAccount(
    const DomainAccountInfo &info, const sptr<IDomainAccountCallback> &callback)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    GetDomainAccountInfoOptions options;
    options.accountInfo = info;
    options.callingUid = callingUid;
    auto task =
        std::bind(&InnerDomainAccountManager::StartHasDomainAccount, this, plugin_, options, callback);
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), THREAD_HAS_ACCOUNT);
    taskThread.detach();
    return ERR_OK;
}

void InnerDomainAccountManager::StartOnAccountBound(const sptr<IDomainAccountPlugin> &plugin,
    const DomainAccountInfo &info, const int32_t localId, const sptr<IDomainAccountCallback> &callback)
{
    if (plugin == nullptr) {
        ACCOUNT_LOGE("plugin not exists");
        return ErrorOnResult(ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST, callback);
    }
    plugin->OnAccountBound(info, localId, callback);
}

ErrCode InnerDomainAccountManager::OnAccountBound(const DomainAccountInfo &info, const int32_t localId,
    const std::shared_ptr<DomainAccountCallback> &callback)
{
    sptr<DomainAccountCallbackService> callbackService = new (std::nothrow) DomainAccountCallbackService(callback);
    if (callbackService == nullptr) {
        ACCOUNT_LOGE("make shared DomainAccountCallbackService failed");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    auto task =
        std::bind(&InnerDomainAccountManager::StartOnAccountBound, this, plugin_, info, localId, callbackService);
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), THREAD_BIND_ACCOUNT);
    taskThread.detach();
    return ERR_OK;
}

void InnerDomainAccountManager::StartOnAccountUnBound(const sptr<IDomainAccountPlugin> &plugin,
    const DomainAccountInfo &info, const sptr<IDomainAccountCallback> &callback)
{
    if (plugin == nullptr) {
        ACCOUNT_LOGE("plugin not exists");
        return ErrorOnResult(ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST, callback);
    }
    plugin->OnAccountUnBound(info, callback);
}

ErrCode InnerDomainAccountManager::OnAccountUnBound(const DomainAccountInfo &info,
    const std::shared_ptr<DomainAccountCallback> &callback)
{
    sptr<DomainAccountCallbackService> callbackService = new (std::nothrow) DomainAccountCallbackService(callback);
    if (callbackService == nullptr) {
        ACCOUNT_LOGE("make shared DomainAccountCallbackService failed");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    auto task =
        std::bind(&InnerDomainAccountManager::StartOnAccountUnBound, this, plugin_, info, callbackService);
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), THREAD_UNBIND_ACCOUNT);
    taskThread.detach();
    return ERR_OK;
}

void InnerDomainAccountManager::StartGetDomainAccountInfo(const sptr<IDomainAccountPlugin> &plugin,
    const GetDomainAccountInfoOptions &options, const sptr<IDomainAccountCallback> &callback)
{
    if (plugin == nullptr) {
        ACCOUNT_LOGE("plugin not exists");
        return ErrorOnResult(ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST, callback);
    }
    ErrCode errCode = plugin->GetDomainAccountInfo(options, callback);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("failed to get domain account, errCode: %{public}d", errCode);
        ErrorOnResult(errCode, callback);
    }
}

ErrCode InnerDomainAccountManager::GetDomainAccountInfo(
    const DomainAccountInfo &info, const sptr<IDomainAccountCallback> &callback)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    GetDomainAccountInfoOptions options;
    options.accountInfo = info;
    options.callingUid = callingUid;
    auto task = std::bind(
        &InnerDomainAccountManager::StartGetDomainAccountInfo, this, plugin_, options, callback);
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), THREAD_GET_ACCOUNT);
    taskThread.detach();
    return ERR_OK;
}

void InnerDomainAccountManager::StartIsAccountTokenValid(const sptr<IDomainAccountPlugin> &plugin,
    const DomainAccountInfo &info, const std::vector<uint8_t> &token, const sptr<IDomainAccountCallback> &callback)
{
    if (plugin == nullptr) {
        ACCOUNT_LOGE("plugin not exists");
        return ErrorOnResult(ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST, callback);
    }
    ErrCode errCode = plugin->IsAccountTokenValid(info, token, callback);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("failed to get domain account, errCode: %{public}d", errCode);
        ErrorOnResult(errCode, callback);
    }
}

ErrCode InnerDomainAccountManager::IsAccountTokenValid(const DomainAccountInfo &info,
    const std::vector<uint8_t> &token, const std::shared_ptr<DomainAccountCallback> &callback)
{
    sptr<DomainAccountCallbackService> callbackService = new (std::nothrow) DomainAccountCallbackService(callback);
    if (callbackService == nullptr) {
        ACCOUNT_LOGE("make shared DomainAccountCallbackService failed");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    auto task = std::bind(
        &InnerDomainAccountManager::StartIsAccountTokenValid, this, plugin_, info, token, callbackService);
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), THREAD_IS_ACCOUNT_VALID);
    taskThread.detach();
    return ERR_OK;
}
}  // namespace AccountSA
}  // namespace OHOS
