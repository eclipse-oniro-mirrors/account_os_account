/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#include "os_account_interface.h"

#include <cerrno>
#include <condition_variable>
#include <thread>

#include "ability_manager_adapter.h"
#include "account_log_wrapper.h"
#include "bundle_manager_adapter.h"
#ifdef HAS_CES_PART
#include "common_event_manager.h"
#include "common_event_support.h"
#endif // HAS_CES_PART
#include "datetime_ex.h"
#include "account_hisysevent_adapter.h"
#include "hitrace_adapter.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#ifdef HAS_STORAGE_PART
#include "istorage_manager.h"
#endif
#include "os_account_constants.h"
#include "os_account_delete_user_idm_callback.h"
#include "os_account_user_callback.h"
#include "os_account_subscribe_manager.h"
#ifdef HAS_STORAGE_PART
#include "storage_manager_proxy.h"
#endif
#include "iinner_os_account_manager.h"
#include "system_ability_definition.h"
#ifdef HAS_USER_IDM_PART
#include "user_idm_client.h"
#endif // HAS_USER_IDM_PART
#ifdef HAS_CES_PART
#include "want.h"
#endif // HAS_CES_PART


namespace OHOS {
namespace AccountSA {
namespace {
#ifdef HAS_STORAGE_PART
constexpr uint32_t CRYPTO_FLAG_EL1 = 1;
constexpr uint32_t CRYPTO_FLAG_EL2 = 2;
constexpr int32_t E_ACTIVE_EL2 = 30;
#endif
// an error code of ipc which means peer end is dead
constexpr int32_t E_IPC_ERROR = 29189;
constexpr int32_t E_IPC_SA_DIED = 32;
constexpr int32_t DELAY_FOR_EXCEPTION = 100;
constexpr int32_t MAX_RETRY_TIMES = 10;
}

ErrCode OsAccountInterface::SendToAMSAccountStart(OsAccountInfo &osAccountInfo)
{
    int32_t localId = osAccountInfo.GetLocalId();
    ACCOUNT_LOGI("Start OS account %{public}d", localId);
    sptr<OsAccountUserCallback> osAccountStartUserCallback = new (std::nothrow) OsAccountUserCallback();
    if (osAccountStartUserCallback == nullptr) {
        ACCOUNT_LOGE("alloc memory for start user callback failed!");
        ReportOsAccountOperationFail(localId, Constants::OPERATION_START,
            ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR, "malloc for OsAccountUserCallback failed!");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    StartTraceAdapter("AbilityManagerAdapter StartUser");

    std::unique_lock<std::mutex> lock(osAccountStartUserCallback->mutex_);
    ErrCode code = AbilityManagerAdapter::GetInstance()->StartUser(osAccountInfo.GetLocalId(),
        osAccountStartUserCallback);
    if (code != ERR_OK) {
        ACCOUNT_LOGE("AbilityManagerAdapter StartUser failed! errcode is %{public}d", code);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_ACTIVATE, code,
            "AbilityManager failed to start user");
        FinishTraceAdapter();
        return code;
    }
    osAccountStartUserCallback->onStartCondition_.wait(lock);
    FinishTraceAdapter();
    if (!osAccountStartUserCallback->isReturnOk_) {
        ACCOUNT_LOGE("failed to AbilityManagerService in call back");
        return ERR_OSACCOUNT_SERVICE_INTERFACE_TO_AM_ACCOUNT_START_ERROR;
    }
    ACCOUNT_LOGI("end, succeed %{public}d", localId);
    return code;
}

ErrCode OsAccountInterface::SendToAMSAccountStop(OsAccountInfo &osAccountInfo)
{
    int32_t localId = osAccountInfo.GetLocalId();
    ACCOUNT_LOGI("Stop OS account %{public}d", localId);
    sptr<OsAccountUserCallback> osAccountStopUserCallback = new (std::nothrow) OsAccountUserCallback();
    if (osAccountStopUserCallback == nullptr) {
        ACCOUNT_LOGE("alloc memory for stop user callback failed!");
        ReportOsAccountOperationFail(localId, Constants::OPERATION_STOP,
            ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR, "malloc for OsAccountUserCallback failed!");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    StartTraceAdapter("AbilityManagerAdapter StopUser");

    std::unique_lock<std::mutex> lock(osAccountStopUserCallback->mutex_);
    ErrCode code = AbilityManagerAdapter::GetInstance()->StopUser(osAccountInfo.GetLocalId(),
        osAccountStopUserCallback);
    if (code != ERR_OK) {
        ACCOUNT_LOGE("failed to AbilityManagerAdapter stop errcode is %{public}d", code);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_STOP, code,
            "AbilityManager failed to stop user");
        FinishTraceAdapter();
        return code;
    }
    osAccountStopUserCallback->onStopCondition_.wait(lock);
    FinishTraceAdapter();
    if (!osAccountStopUserCallback->isReturnOk_) {
        ACCOUNT_LOGE("failed to AbilityManagerService in call back");
        return ERR_OSACCOUNT_SERVICE_INTERFACE_TO_AM_ACCOUNT_START_ERROR;
    }
    ACCOUNT_LOGI("end, succeed %{public}d", localId);
    return code;
}

ErrCode OsAccountInterface::SendToAMSAccountDeactivate(OsAccountInfo &osAccountInfo)
{
    int32_t localId = osAccountInfo.GetLocalId();
    ACCOUNT_LOGI("Deactivate OS account %{public}d", localId);
    StartTraceAdapter("AbilityManagerAdapter LogoutUser");
    ErrCode code = AbilityManagerAdapter::GetInstance()->LogoutUser(osAccountInfo.GetLocalId());
    if (code != ERR_OK) {
        ACCOUNT_LOGE("failed to AbilityManagerAdapter logout errcode is %{public}d", code);
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_STOP, code,
            "AbilityManager failed to logout user");
    }
    FinishTraceAdapter();
    return code;
}

#ifdef HAS_THEME_SERVICE_PART
void OsAccountInterface::InitThemeResource(int32_t localId)
{
    StartTraceAdapter("ThemeManager InitResource");
    bool ret = ThemeManager::ThemeManagerClient::GetInstance().InitResource(localId);
    if (!ret) {
        ACCOUNT_LOGE("Init theme failed, localId=%{public}d.", localId);
        FinishTraceAdapter();
        return;
    }
    ACCOUNT_LOGI("Init theme successful.");
    FinishTraceAdapter();
    return;
}
#endif

ErrCode OsAccountInterface::SendToBMSAccountCreate(
    OsAccountInfo &osAccountInfo, const std::vector<std::string> &disallowedHapList)
{
    ErrCode errCode = ERR_OK;
    int32_t retryTimes = 0;
    while (retryTimes < MAX_RETRY_TIMES) {
        errCode = BundleManagerAdapter::GetInstance()->CreateNewUser(osAccountInfo.GetLocalId(), disallowedHapList);
        if (errCode == ERR_OK) {
            break;
        }
        ACCOUNT_LOGE("Fail to SendToBMSAccountCreate, errCode %{public}d.", errCode);
        retryTimes++;
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_FOR_EXCEPTION));
    }
    return errCode;
}

ErrCode OsAccountInterface::SendToBMSAccountDelete(OsAccountInfo &osAccountInfo)
{
    return BundleManagerAdapter::GetInstance()->RemoveUser(osAccountInfo.GetLocalId());
}

#ifdef HAS_USER_IDM_PART
ErrCode OsAccountInterface::SendToIDMAccountDelete(OsAccountInfo &osAccountInfo)
{
    std::shared_ptr<OsAccountDeleteUserIdmCallback> callback = std::make_shared<OsAccountDeleteUserIdmCallback>();
    if (callback == nullptr) {
        ACCOUNT_LOGE("get idm callback ptr failed! insufficient memory!");
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_REMOVE,
            ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR,
            "Failed to malloc for OsAccountDeleteUserIdmCallback");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    StartTraceAdapter("UserIDMClient EnforceDelUser");
    int32_t ret = UserIam::UserAuth::UserIdmClient::GetInstance().EraseUser(osAccountInfo.GetLocalId(), callback);
    if (ret != 0) {
        ACCOUNT_LOGE("idm enforce delete user failed! error %{public}d", ret);
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_REMOVE, ret,
            "UserIDM failed to erase user");
        FinishTraceAdapter();
        return ERR_OK;    // do not return fail
    }

    // wait callback
    struct tm startTime = {0};
    struct tm nowTime = {0};
    OHOS::GetSystemCurrentTime(&startTime);
    OHOS::GetSystemCurrentTime(&nowTime);
    while (OHOS::GetSecondsBetween(startTime, nowTime) < Constants::TIME_WAIT_TIME_OUT &&
        !callback->isIdmOnResultCallBack_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(Constants::WAIT_ONE_TIME));
        OHOS::GetSystemCurrentTime(&nowTime);
    }
    if (!callback->isIdmOnResultCallBack_) {
        ACCOUNT_LOGE("idm did not call back! timeout!");
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_REMOVE, -1,
            "UserIDM erase user timeout");
        FinishTraceAdapter();
        return ERR_OK;    // do not return fail
    }
    ACCOUNT_LOGI("send to idm account delete and get callback succeed!");
    FinishTraceAdapter();
    return ERR_OK;
}
#endif // HAS_USER_IDM_PART

void OsAccountInterface::SendToCESAccountCreate(OsAccountInfo &osAccountInfo)
{
    int osAccountID = osAccountInfo.GetLocalId();
#ifdef HAS_CES_PART
    StartTraceAdapter("PublishCommonEvent account create");
    OHOS::AAFwk::Want want;
    want.SetAction(OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_ADDED);
    OHOS::EventFwk::CommonEventData data;
    data.SetCode(osAccountID);
    data.SetWant(want);
    if (!OHOS::EventFwk::CommonEventManager::PublishCommonEvent(data)) {
        ACCOUNT_LOGE("PublishCommonEvent for create account %{public}d failed!", osAccountID);
        ReportOsAccountOperationFail(osAccountID, Constants::OPERATION_CREATE, -1, "PublishCommonEvent failed!");
    } else {
        ACCOUNT_LOGI("PublishCommonEvent for create account %{public}d succeed!", osAccountID);
    }
    FinishTraceAdapter();
#else // HAS_CES_PART
    ACCOUNT_LOGI("No common event part, do not publish for account %{public}d create!", osAccountID);
#endif // HAS_CES_PART
}

void OsAccountInterface::SendToCESAccountDelete(OsAccountInfo &osAccountInfo)
{
    int osAccountID = osAccountInfo.GetLocalId();
#ifdef HAS_CES_PART
    StartTraceAdapter("PublishCommonEvent account delete");
    OHOS::AAFwk::Want want;
    want.SetAction(OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_REMOVED);
    OHOS::EventFwk::CommonEventData data;
    data.SetCode(osAccountID);
    data.SetWant(want);
    if (!OHOS::EventFwk::CommonEventManager::PublishCommonEvent(data)) {
        ACCOUNT_LOGE("PublishCommonEvent for delete account %{public}d failed!", osAccountID);
        ReportOsAccountOperationFail(osAccountID, Constants::OPERATION_REMOVE, -1, "Failed to publish common event");
    } else {
        ACCOUNT_LOGI("PublishCommonEvent for delete account %{public}d succeed!", osAccountID);
    }
    FinishTraceAdapter();
#else // HAS_CES_PART
    ACCOUNT_LOGI("No common event part, do not publish for account %{public}d delete!", osAccountID);
#endif // HAS_CES_PART
}

void OsAccountInterface::PublishCommonEvent(
    const OsAccountInfo &osAccountInfo, const std::string &commonEvent, const std::string &operation)
{
    int osAccountID = osAccountInfo.GetLocalId();
#ifdef HAS_CES_PART
    StartTraceAdapter("PublishCommonEvent account");
    OHOS::AAFwk::Want want;
    want.SetAction(commonEvent);
    OHOS::EventFwk::CommonEventData data;
    data.SetCode(osAccountID);
    data.SetWant(want);
    if (!OHOS::EventFwk::CommonEventManager::PublishCommonEvent(data)) {
        ACCOUNT_LOGE("PublishCommonEvent %{public}d failed!", osAccountID);
        ReportOsAccountOperationFail(osAccountID, operation, -1, "PublishCommonEvent failed!");
    } else {
        ACCOUNT_LOGI("PublishCommonEvent %{public}d succeed!", osAccountID);
    }
    FinishTraceAdapter();
#else  // HAS_CES_PART
    ACCOUNT_LOGI("No common event part, do not publish for account %{public}d!", osAccountID);
#endif // HAS_CES_PART
}

void OsAccountInterface::SendToCESAccountSwitched(int newId, int oldId)
{
#ifdef HAS_CES_PART
    StartTraceAdapter("PublishCommonEvent account switched");
    OHOS::AAFwk::Want want;
    want.SetAction(OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_SWITCHED);
    want.SetParam("oldId", std::to_string(oldId));
    OHOS::EventFwk::CommonEventData data;
    data.SetCode(newId);
    data.SetWant(want);
    if (!OHOS::EventFwk::CommonEventManager::PublishCommonEvent(data)) {
        ACCOUNT_LOGE("PublishCommonEvent failed, account switched:%{public}d->%{public}d", oldId, newId);
        ReportOsAccountOperationFail(newId, Constants::OPERATION_SWITCH, -1, "PublishCommonEvent switched failed!");
    } else {
        ACCOUNT_LOGI("PublishCommonEvent successful, account switched:%{public}d->%{public}d", oldId, newId);
    }
    FinishTraceAdapter();
#else // HAS_CES_PART
    ACCOUNT_LOGI("No common event part, do not publish for account switched:%{public}d->%{public}d", oldId, newId);
#endif // HAS_CES_PART
}

ErrCode OsAccountInterface::SendToStorageAccountCreate(OsAccountInfo &osAccountInfo)
{
    ErrCode errCode = ERR_OK;
    int32_t retryTimes = 0;
    while (retryTimes < MAX_RETRY_TIMES) {
        errCode = InnerSendToStorageAccountCreate(osAccountInfo);
        if (errCode != E_IPC_ERROR && errCode != E_IPC_SA_DIED) {
            break;
        }
        ACCOUNT_LOGE("Fail to SendToStorageAccountCreate,id=%{public}d, errCode %{public}d.",
            osAccountInfo.GetLocalId(), errCode);
        retryTimes++;
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_FOR_EXCEPTION));
    }
    return errCode;
}

#ifdef HAS_STORAGE_PART
static ErrCode PrepareAddUser(const sptr<StorageManager::IStorageManager> &proxy, int32_t userId)
{
    ErrCode err = proxy->PrepareAddUser(userId, CRYPTO_FLAG_EL1 | CRYPTO_FLAG_EL2);
    if (err == 0) {
        return ERR_OK;
    }
    ReportOsAccountOperationFail(userId, Constants::OPERATION_CREATE, err, "StorageManager failed to add user");
    if (err == -EEXIST) {
        return ERR_OK;
    }
    return err;
}
#endif

ErrCode OsAccountInterface::InnerSendToStorageAccountCreate(OsAccountInfo &osAccountInfo)
{
#ifdef HAS_STORAGE_PART
    auto systemAbilityManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    int32_t localId = osAccountInfo.GetLocalId();
    if (!systemAbilityManager) {
        ACCOUNT_LOGE("failed to get system ability mgr.");
        ReportOsAccountOperationFail(localId, Constants::OPERATION_CREATE,
            ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER,
            "GetSystemAbilityManager for storage failed!");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    auto remote = systemAbilityManager->CheckSystemAbility(STORAGE_MANAGER_MANAGER_ID);
    if (!remote) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID service.");
        ReportOsAccountOperationFail(localId, Constants::OPERATION_CREATE,
            ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER,
            "CheckSystemAbility for storage failed!");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    auto proxy = iface_cast<StorageManager::IStorageManager>(remote);
    if (!proxy) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID proxy.");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    StartTraceAdapter("StorageManager PrepareAddUser");
    ErrCode err = PrepareAddUser(proxy, localId);
    if (err == ERR_OK) {
        FinishTraceAdapter();
        return ERR_OK;
    }

    ACCOUNT_LOGI("PrepareAddUser Failed, start check and clean accounts.");
    auto &osAccountManager = IInnerOsAccountManager::GetInstance();
    if (osAccountManager.CleanGarbageOsAccounts(localId) <= 0) {
        FinishTraceAdapter();
        return err;
    }
    ACCOUNT_LOGI("Clean garbage account data, Retry Storage PrepareAddUser.");
    err = PrepareAddUser(proxy, localId);
    FinishTraceAdapter();
    return err;
#endif
    return ERR_OK;
}

ErrCode OsAccountInterface::SendToStorageAccountRemove(OsAccountInfo &osAccountInfo)
{
    ACCOUNT_LOGI("start");
#ifdef HAS_STORAGE_PART
    auto systemAbilityManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!systemAbilityManager) {
        ACCOUNT_LOGE("failed to get system ability mgr.");
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_REMOVE,
            ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER,
            "Failed to get SystemAbilityManager");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    auto remote = systemAbilityManager->GetSystemAbility(STORAGE_MANAGER_MANAGER_ID);
    if (!remote) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID service.");
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_REMOVE,
            ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER,
            "Failed to get StorageManager service");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    auto proxy = iface_cast<StorageManager::IStorageManager>(remote);
    if (!proxy) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID proxy.");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }

    StartTraceAdapter("StorageManager RemoveUser");
    int err = proxy->RemoveUser(osAccountInfo.GetLocalId(),
        CRYPTO_FLAG_EL1 | CRYPTO_FLAG_EL2);
    if (err != 0) {
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_REMOVE,
            err, "StorageManager failed to remove user");
        ACCOUNT_LOGE("Storage RemoveUser failed, ret %{public}d", err);
        FinishTraceAdapter();
        return err;
    }

    ACCOUNT_LOGI("end, Storage RemoveUser ret %{public}d.", err);
    FinishTraceAdapter();
#endif
    return ERR_OK;
}

#ifdef HAS_STORAGE_PART
static ErrCode GetStorageProxy(sptr<StorageManager::IStorageManager> &proxy)
{
    auto systemAbilityManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!systemAbilityManager) {
        ACCOUNT_LOGE("failed to get system ability mgr.");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    auto remote = systemAbilityManager->CheckSystemAbility(STORAGE_MANAGER_MANAGER_ID);
    if (!remote) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID service.");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    proxy = iface_cast<StorageManager::IStorageManager>(remote);
    return ERR_OK;
}
#endif

ErrCode OsAccountInterface::SendToStorageAccountStart(OsAccountInfo &osAccountInfo)
{
    ACCOUNT_LOGI("start");
    bool isUserUnlocked = false;
#ifdef HAS_STORAGE_PART
    sptr<StorageManager::IStorageManager> proxy = nullptr;
    if (GetStorageProxy(proxy) != ERR_OK) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID proxy.");
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_ACTIVATE,
            ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER, "Failed to get StorageManager service");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    StartTraceAdapter("StorageManager PrepareStartUser");
    std::vector<uint8_t> emptyData;
    int32_t err = proxy->ActiveUserKey(osAccountInfo.GetLocalId(), emptyData, emptyData);
    ACCOUNT_LOGI("Storage ActiveUserKey end, ret %{public}d.", err);
    if (err == 0) {
        isUserUnlocked = true;
    }
    if (err != E_ACTIVE_EL2) {
        err = proxy->PrepareStartUser(osAccountInfo.GetLocalId());
        if (err != 0) {
            ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_ACTIVATE,
                err, "Storage PrepareStartUser failed!");
        }
    }
    ACCOUNT_LOGI("end, Storage PrepareStartUser ret %{public}d.", err);
    FinishTraceAdapter();
#else
    isUserUnlocked = true;
#endif
    if (!osAccountInfo.GetIsVerified() && isUserUnlocked) {
        osAccountInfo.SetIsVerified(true);
        bool hasCredential = osAccountInfo.GetCredentialId() > 0;
        if (!hasCredential) {
            osAccountInfo.SetIsLoggedIn(true);
            osAccountInfo.SetLastLoginTime(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
    }
    ACCOUNT_LOGI("end, succeed!");
    return ERR_OK;
}

ErrCode OsAccountInterface::SendToStorageAccountStop(OsAccountInfo &osAccountInfo)
{
    ACCOUNT_LOGI("Stop storage, account id = %{public}d", osAccountInfo.GetLocalId());
#ifdef HAS_STORAGE_PART
    auto systemAbilityManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!systemAbilityManager) {
        ACCOUNT_LOGE("failed to get system ability mgr.");
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_STOP,
            ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER,
            "GetSystemAbilityManager for storage failed!");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    auto remote = systemAbilityManager->GetSystemAbility(STORAGE_MANAGER_MANAGER_ID);
    if (!remote) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID service.");
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_STOP,
            ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER,
            "GetSystemAbility for storage failed!");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    auto proxy = iface_cast<StorageManager::IStorageManager>(remote);
    if (!proxy) {
        ACCOUNT_LOGE("failed to get STORAGE_MANAGER_MANAGER_ID proxy.");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    StartTraceAdapter("StorageManager StopUser");
    int localId = osAccountInfo.GetLocalId();
    int err = proxy->StopUser(localId);
    if (err != 0) {
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_STOP,
            err, "StorageManager failed to stop user");
    }
    err = proxy->InactiveUserKey(localId);
    if (err != 0) {
        ReportOsAccountOperationFail(osAccountInfo.GetLocalId(), Constants::OPERATION_STOP,
            err, "StorageManager failed to inactivate user key");
    }
    FinishTraceAdapter();
#endif
    osAccountInfo.SetIsVerified(false);
    return ERR_OK;
}

ErrCode OsAccountInterface::SendToStorageAccountCreateComplete(int32_t localId)
{
    ErrCode errCode = ERR_OK;
    int32_t retryTimes = 0;
    while (retryTimes < MAX_RETRY_TIMES) {
        errCode = InnerSendToStorageAccountCreateComplete(localId);
        if (errCode == ERR_OK) {
            break;
        }
        ACCOUNT_LOGE("Fail to complete account, localId=%{public}d, errCode=%{public}d.", localId, errCode);
        retryTimes++;
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_FOR_EXCEPTION));
    }
    return errCode;
}

ErrCode OsAccountInterface::InnerSendToStorageAccountCreateComplete(int32_t localId)
{
#ifdef HAS_STORAGE_PART
    sptr<StorageManager::IStorageManager> proxy = nullptr;
    if (GetStorageProxy(proxy) != ERR_OK) {
        ACCOUNT_LOGE("Failed to get STORAGE_MANAGER_MANAGER_ID proxy.");
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    StartTraceAdapter("StorageManager CompleteAddUser");
    int errCode = proxy->CompleteAddUser(localId);
    if (errCode != 0) {
        ACCOUNT_LOGE("Failed to CompleteAddUser, localId=%{public}d, errCode=%{public}d", localId, errCode);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_CREATE, errCode,
            "StorageManager failed to complete add user");
        return errCode;
    }
    FinishTraceAdapter();
#endif
    return ERR_OK;
}
}  // namespace AccountSA
}  // namespace OHOS
