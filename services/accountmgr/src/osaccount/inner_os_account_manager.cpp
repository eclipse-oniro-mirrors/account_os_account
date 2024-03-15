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
#include "iinner_os_account_manager.h"
#include "account_event_provider.h"
#include <chrono>
#include <unistd.h>
#include "account_info.h"
#include "account_info_report.h"
#include "account_log_wrapper.h"
#include "os_account_info.h"
#ifdef HAS_CES_PART
#include "common_event_support.h"
#endif // HAS_CES_PART
#include "domain_account_callback_service.h"
#include "hitrace_adapter.h"
#include "hisysevent_adapter.h"
#include "ohos_account_kits.h"
#include "os_account_constants.h"
#include "os_account_control_file_manager.h"
#include "os_account_domain_account_callback.h"
#include "os_account_subscribe_manager.h"
#include "parameter.h"
#include "parcel.h"
#include <pthread.h>
#include <thread>
#include <unordered_set>

namespace OHOS {
namespace AccountSA {
namespace {
const std::string CONSTRAINT_CREATE_ACCOUNT_DIRECTLY = "constraint.os.account.create.directly";
const std::string ACCOUNT_READY_EVENT = "bootevent.account.ready";
const char WATCH_START_USER[] = "watch.start.user";
constexpr std::int32_t DELAY_FOR_ACCOUNT_BOOT_EVENT_READY = 5000;
constexpr std::int32_t DELAY_FOR_EXCEPTION = 50;
constexpr std::int32_t MAX_RETRY_TIMES = 50;
const std::string SPECIAL_CHARACTER_ARRAY = "<>|\":*?/\\";
const std::vector<std::string> SHORT_NAME_CANNOT_BE_NAME_ARRAY = {".", ".."};
const int32_t MAX_PRIVATE_TYPE_NUMBER = 1;
}

IInnerOsAccountManager::IInnerOsAccountManager() : subscribeManager_(OsAccountSubscribeManager::GetInstance())
{
    activeAccountId_.clear();
    operatingId_.clear();
    osAccountControl_ = std::make_shared<OsAccountControlFileManager>();
    osAccountControl_->Init();
    osAccountControl_->GetDeviceOwnerId(deviceOwnerId_);
    osAccountControl_->GetDefaultActivatedOsAccount(defaultActivatedId_);
    ACCOUNT_LOGD("OsAccountAccountMgr Init end");
}

IInnerOsAccountManager &IInnerOsAccountManager::GetInstance()
{
    static IInnerOsAccountManager *instance = new (std::nothrow) IInnerOsAccountManager();
    return *instance;
}

void IInnerOsAccountManager::SetOsAccountControl(std::shared_ptr<IOsAccountControl> ptr)
{
    osAccountControl_ = ptr;
}

void IInnerOsAccountManager::CreateBaseAdminAccount()
{
    ACCOUNT_LOGI("start to create admin account");
    bool isExistsAccount = false;
    osAccountControl_->IsOsAccountExists(Constants::ADMIN_LOCAL_ID, isExistsAccount);
    if (!isExistsAccount) {
        int64_t serialNumber =
            Constants::CARRY_NUM * Constants::SERIAL_NUMBER_NUM_START_FOR_ADMIN + Constants::ADMIN_LOCAL_ID;
        OsAccountInfo osAccountInfo(
            Constants::ADMIN_LOCAL_ID, Constants::ADMIN_LOCAL_NAME, OsAccountType::ADMIN, serialNumber);
        int64_t time =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        osAccountInfo.SetCreateTime(time);
        osAccountInfo.SetIsCreateCompleted(true);
        osAccountInfo.SetIsActived(true);  // admin local account is always active
        osAccountControl_->InsertOsAccount(osAccountInfo);
        ReportOsAccountLifeCycle(osAccountInfo.GetLocalId(), Constants::OPERATION_CREATE);
        ACCOUNT_LOGI("OsAccountAccountMgr created admin account end");
    } else {
        ACCOUNT_LOGI("OsAccountAccountMgr admin account already exists");
    }
}

void IInnerOsAccountManager::CreateBaseStandardAccount()
{
    ACCOUNT_LOGI("start to create base account");
    bool isExistsAccount = false;
    osAccountControl_->IsOsAccountExists(Constants::START_USER_ID, isExistsAccount);
    if (!isExistsAccount) {
        int64_t serialNumber = 0;
        osAccountControl_->GetSerialNumber(serialNumber);
#ifdef ENABLE_DEFAULT_ADMIN_NAME
        OsAccountInfo osAccountInfo(Constants::START_USER_ID, Constants::STANDARD_LOCAL_NAME,
            OsAccountType::ADMIN, serialNumber);
#else
        OsAccountInfo osAccountInfo(Constants::START_USER_ID, "", OsAccountType::ADMIN, serialNumber);
#endif //ENABLE_DEFAULT_ADMIN_NAME
        std::vector<std::string> constants;
        ErrCode errCode = osAccountControl_->GetConstraintsByType(OsAccountType::ADMIN, constants);
        if (errCode != ERR_OK) {
            ACCOUNT_LOGE("find first standard type err, errCode %{public}d.", errCode);
            return;
        }
        osAccountInfo.SetConstraints(constants);
        int64_t time =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        osAccountInfo.SetCreateTime(time);
        osAccountInfo.SetIsCreateCompleted(false);
        osAccountControl_->InsertOsAccount(osAccountInfo);
        ReportOsAccountLifeCycle(osAccountInfo.GetLocalId(), Constants::OPERATION_CREATE);
        ACCOUNT_LOGI("OsAccountAccountMgr created base account end");
    } else {
        ACCOUNT_LOGI("base account already exists");
    }
}

void IInnerOsAccountManager::RetryToGetAccount(OsAccountInfo &osAccountInfo)
{
    int32_t retryTimes = 0;
    while (retryTimes < MAX_RETRY_TIMES) {
        std::vector<OsAccountInfo> osAccountInfos;
        QueryAllCreatedOsAccounts(osAccountInfos);
        if (!osAccountInfos.empty()) {
            osAccountInfo = osAccountInfos[0];
            break;
        }
        ACCOUNT_LOGE("fail to query accounts");
        retryTimes++;
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_FOR_EXCEPTION));
    }

#ifdef ENABLE_DEFAULT_ADMIN_NAME
    while (CreateOsAccount(Constants::STANDARD_LOCAL_NAME, ADMIN, osAccountInfo) != ERR_OK) {
#else
    while (CreateOsAccount("", ADMIN, osAccountInfo) != ERR_OK) {
#endif
        ACCOUNT_LOGE("fail to create account");
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_FOR_EXCEPTION));
    }
}

void IInnerOsAccountManager::StartAccount()
{
    ACCOUNT_LOGI("start to activate default account");
    ResetAccountStatus();
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(defaultActivatedId_, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("account not found, localId: %{public}d, error: %{public}d", defaultActivatedId_, errCode);
        RetryToGetAccount(osAccountInfo);
        defaultActivatedId_ = osAccountInfo.GetLocalId();
        osAccountControl_->SetDefaultActivatedOsAccount(defaultActivatedId_);
    }
    auto task = std::bind(&IInnerOsAccountManager::WatchStartUser, this, defaultActivatedId_);
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), WATCH_START_USER);
    taskThread.detach();
    if (!osAccountInfo.GetIsCreateCompleted() && (SendMsgForAccountCreate(osAccountInfo) != ERR_OK)) {
        ACCOUNT_LOGE("account %{public}d not created completely", defaultActivatedId_);
        return;
    }
    // activate
    SendMsgForAccountActivate(osAccountInfo);
}

void IInnerOsAccountManager::RestartActiveAccount()
{
    // query active account to restart and refresh into list
    std::vector<OsAccountInfo> osAccountInfos;
    if (QueryAllCreatedOsAccounts(osAccountInfos) != ERR_OK) {
        return;
    }
    for (size_t i = 0; i < osAccountInfos.size(); ++i) {
        OsAccountInfo osAccountInfo = osAccountInfos[i];
        std::int32_t id = osAccountInfo.GetLocalId();
        if (osAccountInfo.GetIsActived() && id != Constants::START_USER_ID) {
            // reactivate account state
            if (ActivateOsAccount(id) != ERR_OK) {
                ACCOUNT_LOGE("active base account failed");
                return;
            }
        }
    }
}

void IInnerOsAccountManager::ResetAccountStatus(void)
{
    std::vector<OsAccountInfo> osAccountInfos;
    if (QueryAllCreatedOsAccounts(osAccountInfos) != ERR_OK) {
        return;
    }
    for (size_t i = 0; i < osAccountInfos.size(); ++i) {
        DeactivateOsAccount(osAccountInfos[i].GetLocalId());
    }
}

ErrCode IInnerOsAccountManager::PrepareOsAccountInfo(const std::string &name, const OsAccountType &type,
    const DomainAccountInfo &domainInfo, OsAccountInfo &osAccountInfo)
{
    return PrepareOsAccountInfo(name, "", type, domainInfo, osAccountInfo);
}

ErrCode IInnerOsAccountManager::PrepareOsAccountInfo(const std::string &localName, const std::string &shortName,
    const OsAccountType &type, const DomainAccountInfo &domainInfo, OsAccountInfo &osAccountInfo)
{
    ErrCode errCode = FillOsAccountInfo(localName, shortName, type, domainInfo, osAccountInfo);
    if (errCode != ERR_OK) {
        return errCode;
    }

    errCode = ValidateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("account name already exist, errCode %{public}d.", errCode);
        return errCode;
    }
    errCode = CheckTypeNumber(type);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Check type number failed.");
        return errCode;
    }

    errCode = osAccountControl_->InsertOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("insert os account info err, errCode %{public}d.", errCode);
        return errCode;
    }
    // private type account not write index to index file, don't check name in ValidateOsAccount
    if (type != OsAccountType::PRIVATE) {
        osAccountControl_->UpdateAccountIndex(osAccountInfo, false);
    }

    errCode = osAccountControl_->UpdateBaseOAConstraints(std::to_string(osAccountInfo.GetLocalId()),
        osAccountInfo.GetConstraints(), true);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("UpdateBaseOAConstraints err");
        return errCode;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::FillOsAccountInfo(const std::string &localName, const std::string &shortName,
    const OsAccountType &type, const DomainAccountInfo &domainInfo, OsAccountInfo &osAccountInfo)
{
    int64_t serialNumber;
    ErrCode errCode = osAccountControl_->GetSerialNumber(serialNumber);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("failed to GetSerialNumber, errCode %{public}d.", errCode);
        return errCode;
    }
    int id = 0;
    errCode = osAccountControl_->GetAllowCreateId(id);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("failed to GetAllowCreateId, errCode %{public}d.", errCode);
        return errCode;
    }
    std::vector<std::string> constraints;
    constraints.clear();
    errCode = osAccountControl_->GetConstraintsByType(type, constraints);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("failed to GetConstraintsByType, errCode %{public}d.", errCode);
        return errCode;
    }

    osAccountInfo = OsAccountInfo(id, localName, shortName, type, serialNumber);
    osAccountInfo.SetConstraints(constraints);
    int64_t time =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    osAccountInfo.SetCreateTime(time);
    if (!osAccountInfo.SetDomainInfo(domainInfo)) {
        ACCOUNT_LOGE("failed to SetDomainInfo");
        return ERR_OSACCOUNT_KIT_CREATE_OS_ACCOUNT_FOR_DOMAIN_ERROR;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::PrepareOsAccountInfoWithFullInfo(OsAccountInfo &osAccountInfo)
{
    int64_t serialNumber;
    ErrCode errCode = osAccountControl_->GetSerialNumber(serialNumber);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("failed to GetSerialNumber, errCode %{public}d.", errCode);
        return errCode;
    }
    osAccountInfo.SetSerialNumber(serialNumber);
    errCode = osAccountControl_->InsertOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("insert os account info err, errCode %{public}d.", errCode);
        return errCode;
    }

    std::vector<std::string> constraints;
    constraints.clear();
    OsAccountType type = static_cast<OsAccountType>(osAccountInfo.GetType());
    errCode = osAccountControl_->GetConstraintsByType(type, constraints);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("failed to GetConstraintsByType, errCode %{public}d.", errCode);
        return errCode;
    }
    std::vector<std::string> constraintsExists = osAccountInfo.GetConstraints();
    std::vector<std::string> constraintsResult;
    std::merge(constraints.begin(), constraints.end(), constraintsExists.begin(), constraintsExists.end(),
        std::back_inserter(constraintsResult));
    osAccountInfo.SetConstraints(constraintsResult);
    errCode = osAccountControl_->UpdateBaseOAConstraints(
        std::to_string(osAccountInfo.GetLocalId()), constraintsResult, true);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("UpdateBaseOAConstraints err");
        return errCode;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SendMsgForAccountCreate(
    OsAccountInfo &osAccountInfo, const CreateOsAccountOptions &options)
{
    ErrCode errCode = OsAccountInterface::SendToStorageAccountCreate(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("create os account SendToStorageAccountCreate failed, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
#ifdef HAS_THEME_SERVICE_PART
    auto task = std::bind(&OsAccountInterface::InitThemeResource, osAccountInfo.GetLocalId());
    std::thread theme_thread(task);
    pthread_setname_np(theme_thread.native_handle(), "InitTheme");
#endif
    errCode = OsAccountInterface::SendToBMSAccountCreate(osAccountInfo, options.disallowedHapList);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("create os account SendToBMSAccountCreate failed, errCode %{public}d.", errCode);
        (void)OsAccountInterface::SendToStorageAccountRemove(osAccountInfo);
        return errCode;
    }
#ifdef HAS_THEME_SERVICE_PART
    if (theme_thread.joinable()) {
        theme_thread.join();
    }
#endif
    osAccountInfo.SetIsCreateCompleted(true);
    errCode = osAccountControl_->UpdateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("create os account when update isCreateCompleted");
        ReportOsAccountOperationFail(
            osAccountInfo.GetLocalId(), Constants::OPERATION_CREATE, errCode, "UpdateOsAccount failed!");
        (void)OsAccountInterface::SendToStorageAccountRemove(osAccountInfo);
        (void)OsAccountInterface::SendToBMSAccountDelete(osAccountInfo);
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }
    ReportOsAccountLifeCycle(osAccountInfo.GetLocalId(), Constants::OPERATION_CREATE);
    OsAccountInterface::SendToCESAccountCreate(osAccountInfo);
    subscribeManager_.Publish(osAccountInfo.GetLocalId(), OS_ACCOUNT_SUBSCRIBE_TYPE::CREATED);
    ACCOUNT_LOGI("OsAccountAccountMgr send to storage and bm for start success");
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::CreateOsAccount(
    const std::string &name, const OsAccountType &type, OsAccountInfo &osAccountInfo)
{
    DomainAccountInfo domainInfo;  // default empty domain info
    ErrCode errCode = PrepareOsAccountInfo(name, type, domainInfo, osAccountInfo);
    if (errCode != ERR_OK) {
        return errCode;
    }
    errCode = SendMsgForAccountCreate(osAccountInfo);
    if (errCode != ERR_OK) {
        (void)osAccountControl_->DelOsAccount(osAccountInfo.GetLocalId());
    }
    return errCode;
}

ErrCode IInnerOsAccountManager::CreateOsAccount(const std::string &localName, const std::string &shortName,
    const OsAccountType &type, OsAccountInfo &osAccountInfo, const CreateOsAccountOptions &options)
{
    osAccountInfo.SetLocalName(localName);
#ifdef ENABLE_ACCOUNT_SHORT_NAME
    OsAccountInfo accountInfoOld;
    ErrCode code = QueryOsAccountById(Constants::START_USER_ID, accountInfoOld);
    if (code != ERR_OK) {
        ACCOUNT_LOGE("QueryOsAccountById error, errCode %{public}d.", code);
        return code;
    }
    if (accountInfoOld.GetShortName().empty()) {
        accountInfoOld.SetType(type);
        accountInfoOld.SetLocalName(localName);
        accountInfoOld.SetShortName(shortName);
        code = osAccountControl_->UpdateOsAccount(accountInfoOld);
        osAccountControl_->UpdateAccountIndex(accountInfoOld, false);
        osAccountInfo = accountInfoOld;
        return code;
    }
#endif // ENABLE_ACCOUNT_SHORT_NAME
    DomainAccountInfo domainInfo;  // default empty domain info
    ErrCode errCode = PrepareOsAccountInfo(localName, shortName, type, domainInfo, osAccountInfo);
    if (errCode != ERR_OK) {
        return errCode;
    }
    errCode = SendMsgForAccountCreate(osAccountInfo, options);
    if (errCode != ERR_OK) {
        (void)osAccountControl_->DelOsAccount(osAccountInfo.GetLocalId());
    }
    return errCode;
}

ErrCode IInnerOsAccountManager::ValidateShortName(const std::string &shortName)
{
    size_t shortNameSize = shortName.size();
    if (shortNameSize == 0 || shortNameSize > Constants::SHORT_NAME_MAX_SIZE) {
        ACCOUNT_LOGE("CreateOsAccount short name length %{public}zu is invalid!", shortNameSize);
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }

    if (shortName.find_first_of(SPECIAL_CHARACTER_ARRAY) != std::string::npos) {
        ACCOUNT_LOGE("CreateOsAccount short name is invalidate, short name is %{public}s !", shortName.c_str());
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }

    for (size_t i = 0; i < SHORT_NAME_CANNOT_BE_NAME_ARRAY.size(); i++) {
        if (shortName == SHORT_NAME_CANNOT_BE_NAME_ARRAY[i]) {
            ACCOUNT_LOGE("CreateOsAccount short name is invalidate, short name is %{public}s !", shortName.c_str());
            return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
        }
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::CreateOsAccountWithFullInfo(OsAccountInfo &osAccountInfo)
{
    ErrCode errCode = PrepareOsAccountInfoWithFullInfo(osAccountInfo);
    if (errCode != ERR_OK) {
        return errCode;
    }
    errCode = SendMsgForAccountCreate(osAccountInfo);
    if (errCode != ERR_OK) {
        (void)osAccountControl_->DelOsAccount(osAccountInfo.GetLocalId());
    }
    return errCode;
}

ErrCode IInnerOsAccountManager::UpdateAccountStatusForDomain(const int id, DomainAccountStatus status)
{
    OsAccountInfo accountInfo;
    DomainAccountInfo domainInfo;
    ErrCode errCode = GetOsAccountInfoById(id, accountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get os account info failed, errCode: %{public}d", errCode);
        return errCode;
    }
    accountInfo.GetDomainInfo(domainInfo);
    domainInfo.status_ = status;
    accountInfo.SetDomainInfo(domainInfo);

    errCode = osAccountControl_->UpdateOsAccount(accountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("update osaccount info error %{public}d, id: %{public}d",
            errCode, accountInfo.GetLocalId());
        return errCode;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::UpdateOsAccountWithFullInfo(OsAccountInfo &newInfo)
{
    int32_t localId = newInfo.GetLocalId();
    if (!CheckAndAddLocalIdOperating(localId)) {
        ACCOUNT_LOGE("the %{public}d already in operating", localId);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_OPERATING_ERROR;
    }
    OsAccountInfo oldInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(localId, oldInfo);
    if (errCode != ERR_OK) {
        RemoveLocalIdToOperating(localId);
        return errCode;
    }
    oldInfo.SetLocalName(newInfo.GetLocalName());
    oldInfo.SetType(newInfo.GetType());
    oldInfo.SetPhoto(newInfo.GetPhoto());
    oldInfo.SetConstraints(newInfo.GetConstraints());
    errCode = osAccountControl_->UpdateOsAccount(oldInfo);
    osAccountControl_->UpdateAccountIndex(oldInfo, false);
    newInfo = oldInfo;
    if (errCode != ERR_OK) {
        ReportOsAccountOperationFail(
            localId, Constants::OPERATION_UPDATE, errCode, "UpdateOsAccount failed!");
    } else {
        OsAccountInterface::PublishCommonEvent(oldInfo,
            OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_INFO_UPDATED, Constants::OPERATION_UPDATE);
    }
    RemoveLocalIdToOperating(localId);
    return errCode;
}

bool IInnerOsAccountManager::CheckDomainAccountBound(
    const std::vector<OsAccountInfo> &osAccountInfos, const DomainAccountInfo &info)
{
    for (size_t i = 0; i < osAccountInfos.size(); ++i) {
        DomainAccountInfo curInfo;
        osAccountInfos[i].GetDomainInfo(curInfo);
        if ((!info.accountId_.empty() && curInfo.accountId_ == info.accountId_) ||
            ((curInfo.accountName_ == info.accountName_) && (curInfo.domain_ == info.domain_))) {
            return true;
        }
    }
    return false;
}

ErrCode IInnerOsAccountManager::BindDomainAccount(const OsAccountType &type, const DomainAccountInfo &domainAccountInfo,
    const sptr<IDomainAccountCallback> &callback)
{
    std::vector<OsAccountInfo> osAccountInfos;
    (void)QueryAllCreatedOsAccounts(osAccountInfos);
    if (CheckDomainAccountBound(osAccountInfos, domainAccountInfo)) {
        ACCOUNT_LOGE("the domain account is already bound");
        return ERR_OSACCOUNT_SERVICE_INNER_DOMAIN_ALREADY_BIND_ERROR;
    }
#ifdef ENABLE_MULTIPLE_OS_ACCOUNTS
    bool isEnabled = false;
    (void)IsOsAccountConstraintEnable(Constants::START_USER_ID, CONSTRAINT_CREATE_ACCOUNT_DIRECTLY, isEnabled);
#else
    bool isEnabled = true;
#endif // ENABLE_MULTIPLE_OS_ACCOUNTS
    std::string osAccountName = domainAccountInfo.domain_ + "/" + domainAccountInfo.accountName_;
    OsAccountInfo osAccountInfo;
    if (isEnabled && (osAccountInfos.size() == 1) && (osAccountInfos[0].GetLocalId() == Constants::START_USER_ID)) {
        DomainAccountInfo curDomainInfo;
        osAccountInfos[0].GetDomainInfo(curDomainInfo);
        if (curDomainInfo.domain_.empty()) {
            osAccountInfos[0].SetLocalName(osAccountName);
            osAccountInfos[0].SetDomainInfo(domainAccountInfo);
            osAccountInfo = osAccountInfos[0];
        }
    }
    if (osAccountInfo.GetLocalId() != Constants::START_USER_ID) {
#ifdef ENABLE_MULTIPLE_OS_ACCOUNTS
        ErrCode errCode = PrepareOsAccountInfo(osAccountName, domainAccountInfo.accountName_,
            type, domainAccountInfo, osAccountInfo);
        if (errCode != ERR_OK) {
            return errCode;
        }
#else
        ACCOUNT_LOGW("multiple os accounts feature not enabled");
        return ERR_OSACCOUNT_SERVICE_MANAGER_NOT_ENABLE_MULTI_ERROR;
#endif // ENABLE_MULTIPLE_OS_ACCOUNTS
    }
    auto callbackWrapper =
        std::make_shared<BindDomainAccountCallback>(osAccountControl_, domainAccountInfo, osAccountInfo, callback);
    if (callbackWrapper == nullptr) {
        ACCOUNT_LOGE("create BindDomainAccountCallback failed");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    return InnerDomainAccountManager::GetInstance().OnAccountBound(
        domainAccountInfo, osAccountInfo.GetLocalId(), callbackWrapper);
}

ErrCode IInnerOsAccountManager::CreateOsAccountForDomain(
    const OsAccountType &type, const DomainAccountInfo &domainInfo, const sptr<IDomainAccountCallback> &callback)
{
    std::vector<OsAccountInfo> osAccountInfos;
    (void)QueryAllCreatedOsAccounts(osAccountInfos);
    if (CheckDomainAccountBound(osAccountInfos, domainInfo)) {
        ACCOUNT_LOGE("the domain account is already bound");
        return ERR_OSACCOUNT_SERVICE_INNER_DOMAIN_ALREADY_BIND_ERROR;
    }
    if (!InnerDomainAccountManager::GetInstance().IsPluginAvailable()) {
        ACCOUNT_LOGE("plugin is not available");
        return ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST;
    }
    sptr<CheckAndCreateDomainAccountCallback> callbackWrapper =
        new (std::nothrow) CheckAndCreateDomainAccountCallback(type, domainInfo, callback);
    if (callbackWrapper == nullptr) {
        ACCOUNT_LOGE("new DomainCreateDomainCallback failed");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    return InnerDomainAccountManager::GetInstance().GetDomainAccountInfo(domainInfo, callbackWrapper);
}

void IInnerOsAccountManager::CheckAndRefreshLocalIdRecord(const int id)
{
    if (id == defaultActivatedId_) {
        ACCOUNT_LOGI("remove default activated id %{public}d", id);
        osAccountControl_->SetDefaultActivatedOsAccount(Constants::START_USER_ID);
        defaultActivatedId_ = Constants::START_USER_ID;
    }
    if (id == deviceOwnerId_) {
        osAccountControl_->UpdateDeviceOwnerId(-1);
    }
    return;
}

ErrCode IInnerOsAccountManager::RemoveOsAccountOperate(const int id, OsAccountInfo &osAccountInfo,
    const DomainAccountInfo &domainAccountInfo)
{
    AccountInfo ohosInfo;
    (void)OhosAccountManager::GetInstance().GetAccountInfoByUserId(id, ohosInfo);
    ErrCode errCode = SendMsgForAccountRemove(osAccountInfo);
    if (errCode != ERR_OK) {
        RemoveLocalIdToOperating(id);
        return errCode;
    }
    RemoveLocalIdToOperating(id);

    errCode = osAccountControl_->RemoveOAConstraintsInfo(id);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("RemoveOsAccount failed to remove os account constraints info");
        return errCode;
    }
    CheckAndRefreshLocalIdRecord(id);
    if (!domainAccountInfo.accountId_.empty()) {
        InnerDomainAccountManager::GetInstance().NotifyDomainAccountEvent(
            id, DomainAccountEvent::LOG_OUT, DomainAccountStatus::LOGOUT, domainAccountInfo);
    }
    if (ohosInfo.ohosAccountInfo_.name_ != DEFAULT_OHOS_ACCOUNT_NAME) {
#ifdef HAS_CES_PART
        AccountEventProvider::EventPublish(EventFwk::CommonEventSupport::COMMON_EVENT_HWID_LOGOUT, id, nullptr);
        AccountEventProvider::EventPublish(
            EventFwk::CommonEventSupport::COMMON_EVENT_DISTRIBUTED_ACCOUNT_LOGOUT, id, nullptr);
#else  // HAS_CES_PART
        ACCOUNT_LOGI("No common event part! Publish nothing!");
#endif // HAS_CES_PART
    }
    subscribeManager_.Publish(id, OS_ACCOUNT_SUBSCRIBE_TYPE::REMOVED);
    return errCode;
}

ErrCode IInnerOsAccountManager::RemoveOsAccount(const int id)
{
    ACCOUNT_LOGI("RemoveOsAccount delete id is %{public}d", id);
    if (!CheckAndAddLocalIdOperating(id)) {
        ACCOUNT_LOGE("the %{public}d already in operating", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_OPERATING_ERROR;
    }

    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGE("RemoveOsAccount cannot find os account info, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    if (osAccountInfo.GetIsForeground()) {
        ACCOUNT_LOGI("Remove foreground account id=%{public}d.", id);
        if (ActivateOsAccount(Constants::START_USER_ID) != ERR_OK) {
            RemoveLocalIdToOperating(id);
            ACCOUNT_LOGE("RemoveOsAccount active base account failed");
            return ERR_OSACCOUNT_SERVICE_INNER_REMOVE_ACCOUNT_ACTIVED_ERROR;
        }
    }

    DomainAccountInfo curDomainInfo;
    osAccountInfo.GetDomainInfo(curDomainInfo);
    if (!curDomainInfo.accountId_.empty()) {
        InnerDomainAccountManager::GetInstance().OnAccountUnBound(curDomainInfo, nullptr);
        InnerDomainAccountManager::GetInstance().RemoveTokenFromMap(id);
    }
    // set remove flag first
    osAccountInfo.SetToBeRemoved(true);
    osAccountControl_->UpdateOsAccount(osAccountInfo);

    // stop account first
    errCode = SendMsgForAccountStop(osAccountInfo);
    if (errCode != ERR_OK) {
        RemoveLocalIdToOperating(id);
        ReportOsAccountOperationFail(id, "stop", errCode, "stop os account failed");
        return errCode;
    }

    // then remove account
    return RemoveOsAccountOperate(id, osAccountInfo, curDomainInfo);
}

ErrCode IInnerOsAccountManager::SendMsgForAccountStop(OsAccountInfo &osAccountInfo)
{
    ErrCode errCode = OsAccountInterface::SendToAMSAccountStop(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("SendToAMSAccountStop failed, id %{public}d, errCode %{public}d",
            osAccountInfo.GetLocalId(), errCode);
        return errCode;
    }
    errCode = OsAccountInterface::SendToStorageAccountStop(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("SendToStorageAccountStop failed, id %{public}d, errCode %{public}d",
            osAccountInfo.GetLocalId(), errCode);
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    return DeactivateOsAccountById(osAccountInfo.GetLocalId());
}

ErrCode IInnerOsAccountManager::SendMsgForAccountDeactivate(OsAccountInfo &osAccountInfo)
{
    ErrCode errCode = OsAccountInterface::SendToAMSAccountDeactivate(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("SendToAMSAccountDeactivate failed, id %{public}d, errCode %{public}d",
            osAccountInfo.GetLocalId(), errCode);
        return errCode;
    }
    errCode = OsAccountInterface::SendToStorageAccountStop(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("SendToStorageAccountStop failed, id %{public}d, errCode %{public}d",
            osAccountInfo.GetLocalId(), errCode);
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    return DeactivateOsAccountByInfo(osAccountInfo);
}

ErrCode IInnerOsAccountManager::ValidateOsAccount(const OsAccountInfo &osAccountInfo)
{
    if (osAccountInfo.GetType() == OsAccountType::PRIVATE) {
        return ERR_OK;
    }
    Json accountIndexJson;
    ErrCode result = osAccountControl_->GetAccountIndexFromFile(accountIndexJson);
    if (result != ERR_OK) {
        return result;
    }
    std::string localIdStr = std::to_string(osAccountInfo.GetLocalId());
    for (const auto& element : accountIndexJson.items()) {
        std::string localIdKey = element.key();
        auto value = element.value();
        std::string localName = value[Constants::LOCAL_NAME].get<std::string>();
        if ((osAccountInfo.GetLocalName() == localName) && (localIdKey != localIdStr)) {
            return ERR_ACCOUNT_COMMON_NAME_HAD_EXISTED;
        }
        if (!osAccountInfo.GetShortName().empty() && value.contains(Constants::SHORT_NAME)) {
            std::string shortName = value[Constants::SHORT_NAME].get<std::string>();
            if ((osAccountInfo.GetShortName() == shortName) && (localIdKey != localIdStr)) {
                return ERR_ACCOUNT_COMMON_SHORT_NAME_HAD_EXISTED;
            }
        }
    }

    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetTypeNumber(const OsAccountType& type, int32_t& typeNumber)
{
    typeNumber = 0;
    std::vector<OsAccountInfo> osAccountList;
    ErrCode result = osAccountControl_->GetOsAccountList(osAccountList);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("Get os account list failed.");
        return result;
    }
    for (const auto &info : osAccountList) {
        if (info.GetType() == type) {
            typeNumber++;
        }
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::CheckTypeNumber(const OsAccountType& type)
{
    if (type != OsAccountType::PRIVATE) {
        return ERR_OK;
    }
    int32_t typeNumber = 0;
    ErrCode result = GetTypeNumber(type, typeNumber);
    if (result != ERR_OK) {
        ACCOUNT_LOGE("Count type number failed.");
        return result;
    }
    if (type == OsAccountType::PRIVATE && typeNumber >= MAX_PRIVATE_TYPE_NUMBER) {
        ACCOUNT_LOGE("Check type number failed, private type number=%{public}d", typeNumber);
        return ERR_OSACCOUNT_SERVICE_CONTROL_MAX_CAN_CREATE_ERROR;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SendMsgForAccountRemove(OsAccountInfo &osAccountInfo)
{
    ErrCode errCode = OsAccountInterface::SendToBMSAccountDelete(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("SendToBMSAccountDelete failed, id %{public}d, errCode %{public}d",
            osAccountInfo.GetLocalId(), errCode);
        return errCode;
    }
    errCode = OsAccountInterface::SendToStorageAccountRemove(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("SendToStorageAccountRemove failed, id %{public}d, errCode %{public}d",
            osAccountInfo.GetLocalId(), errCode);
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
#ifdef HAS_USER_IDM_PART
    errCode = OsAccountInterface::SendToIDMAccountDelete(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("SendToIDMAccountDelete failed, id %{public}d, errCode %{public}d",
            osAccountInfo.GetLocalId(), errCode);
        return errCode;
    }
#endif // HAS_USER_IDM_PART
    errCode = osAccountControl_->DelOsAccount(osAccountInfo.GetLocalId());
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("remove osaccount info failed, id: %{public}d, errCode %{public}d",
            osAccountInfo.GetLocalId(), errCode);
        return errCode;
    }
    OsAccountInterface::SendToCESAccountDelete(osAccountInfo);
    ReportOsAccountLifeCycle(osAccountInfo.GetLocalId(), Constants::OPERATION_DELETE);
    return errCode;
}

void IInnerOsAccountManager::Init()
{
    CreateBaseAdminAccount();
    CreateBaseStandardAccount();
    StartAccount();
    CleanGarbageAccounts();
}

ErrCode IInnerOsAccountManager::IsOsAccountExists(const int id, bool &isOsAccountExits)
{
    isOsAccountExits = false;
    osAccountControl_->IsOsAccountExists(id, isOsAccountExits);
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::IsOsAccountActived(const int id, bool &isOsAccountActived)
{
    isOsAccountActived = false;

    // check if os account exists
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    if (id == Constants::ADMIN_LOCAL_ID) {
        isOsAccountActived = true;
        return ERR_OK;
    }
    isOsAccountActived = IsOsAccountIDInActiveList(id);
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::IsOsAccountConstraintEnable(
    const int id, const std::string &constraint, bool &isOsAccountConstraintEnable)
{
    isOsAccountConstraintEnable = false;
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    std::vector<std::string> constraints;
    constraints = osAccountInfo.GetConstraints();
    if (std::find(constraints.begin(), constraints.end(), constraint) != constraints.end()) {
        isOsAccountConstraintEnable = true;
        return ERR_OK;
    }
    constraints.clear();
    if (osAccountControl_->GetGlobalOAConstraintsList(constraints) == ERR_OK) {
        if (std::find(constraints.begin(), constraints.end(), constraint) != constraints.end()) {
            isOsAccountConstraintEnable = true;
            return ERR_OK;
        }
    }
    constraints.clear();
    if (osAccountControl_->GetSpecificOAConstraintsList(id, constraints) == ERR_OK) {
        if (std::find(constraints.begin(), constraints.end(), constraint) != constraints.end()) {
            isOsAccountConstraintEnable = true;
            return ERR_OK;
        }
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::IsOsAccountVerified(const int id, bool &isVerified)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Get osaccount fail, errCode=%{public}d, id=%{public}d", errCode, id);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    isVerified = osAccountInfo.GetIsVerified();
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetCreatedOsAccountsCount(unsigned int &createdOsAccountCount)
{
    std::vector<OsAccountInfo> osAccountInfos;
    ErrCode errCode = osAccountControl_->GetOsAccountList(osAccountInfos);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info list error, errCode %{public}d.", errCode);
        return errCode;
    }
    createdOsAccountCount = osAccountInfos.size();
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::QueryMaxOsAccountNumber(int &maxOsAccountNumber)
{
#ifdef ENABLE_MULTIPLE_OS_ACCOUNTS
    ErrCode errCode = osAccountControl_->GetMaxCreatedOsAccountNum(maxOsAccountNumber);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get max created osaccount num error, errCode %{public}d.", errCode);
        return errCode;
    }
#else
    maxOsAccountNumber = 0;
#endif // ENABLE_MULTIPLE_OS_ACCOUNTS
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetOsAccountAllConstraints(const int id, std::vector<std::string> &constraints)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    constraints = osAccountInfo.GetConstraints();
    std::vector<std::string> globalConstraints;
    errCode = osAccountControl_->GetGlobalOAConstraintsList(globalConstraints);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get globalConstraints info error");
        return errCode;
    }
    for (auto it = globalConstraints.begin(); it != globalConstraints.end(); it++) {
        if (std::find(constraints.begin(), constraints.end(), *it) == constraints.end()) {
            constraints.push_back(*it);
        }
    }
    std::vector<std::string> specificConstraints;
    errCode = osAccountControl_->GetSpecificOAConstraintsList(id, specificConstraints);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get specificConstraints info error");
        return errCode;
    }
    for (auto it = specificConstraints.begin(); it != specificConstraints.end(); it++) {
        if (std::find(constraints.begin(), constraints.end(), *it) == constraints.end()) {
            constraints.push_back(*it);
        }
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::QueryOsAccountConstraintSourceTypes(const int32_t id,
    const std::string &constraint, std::vector<ConstraintSourceTypeInfo> &constraintSourceTypeInfos)
{
    ACCOUNT_LOGD("enter.");
    bool isOsAccountConstraintEnable = false;
    ErrCode errCode = IsOsAccountConstraintEnable(id, constraint, isOsAccountConstraintEnable);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get os account constraint enable info error");
        return errCode;
    }
    if (!isOsAccountConstraintEnable) {
        ACCOUNT_LOGI("constraint not exist");
        ConstraintSourceTypeInfo constraintSourceTypeInfo;
        constraintSourceTypeInfo.localId = -1;
        constraintSourceTypeInfo.typeInfo = ConstraintSourceType::CONSTRAINT_NOT_EXIST;
        constraintSourceTypeInfos.push_back(constraintSourceTypeInfo);
        return ERR_OK;
    }

    bool isExits;
    if (osAccountControl_->IsFromBaseOAConstraintsList(id, constraint, isExits) == ERR_OK) {
        if (isExits) {
            ACCOUNT_LOGI("constraint is exist in base os account constraints list");
            ConstraintSourceTypeInfo constraintSourceTypeInfo;
            constraintSourceTypeInfo.localId = -1;
            constraintSourceTypeInfo.typeInfo = ConstraintSourceType::CONSTRAINT_TYPE_BASE;
            constraintSourceTypeInfos.push_back(constraintSourceTypeInfo);
        }
    }
    std::vector<ConstraintSourceTypeInfo> globalSourceList;
    errCode = osAccountControl_->IsFromGlobalOAConstraintsList(id, deviceOwnerId_, constraint, globalSourceList);
    if (errCode == ERR_OK && globalSourceList.size() != 0) {
        ACCOUNT_LOGI("constraint is exist in global os account constraints list");
        constraintSourceTypeInfos.insert(
            constraintSourceTypeInfos.end(), globalSourceList.begin(), globalSourceList.end());
    }
    std::vector<ConstraintSourceTypeInfo> specificSourceList;
    errCode = osAccountControl_->IsFromSpecificOAConstraintsList(id, deviceOwnerId_, constraint, specificSourceList);
    if (errCode == ERR_OK && specificSourceList.size() != 0) {
        ACCOUNT_LOGI("constraint is exist in specific os account constraints list");
        constraintSourceTypeInfos.insert(
            constraintSourceTypeInfos.end(), specificSourceList.begin(), specificSourceList.end());
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SetBaseOsAccountConstraints(const int32_t id,
    const std::vector<std::string> &constraints, const bool enable)
{
    ErrCode errCode = SetOsAccountConstraints(id, constraints, enable);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("set os account %{public}d constraints failed! errCode %{public}d.", id, errCode);
        return errCode;
    }

    errCode = osAccountControl_->UpdateBaseOAConstraints(std::to_string(id), constraints, enable);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("update base os account %{public}d constraints failed! errCode %{public}d.", id, errCode);
        return errCode;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SetGlobalOsAccountConstraints(const std::vector<std::string> &constraints,
    const bool enable, const int32_t enforcerId, const bool isDeviceOwner)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(enforcerId, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error %{public}d", enforcerId);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    if (osAccountInfo.GetToBeRemoved()) {
        ACCOUNT_LOGE("account %{public}d will be removed, cannot change constraints!", enforcerId);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }

    bool isExists = false;
    bool isOverSize = false;
    errCode = osAccountControl_->CheckConstraintsList(constraints, isExists, isOverSize);
    if (errCode != ERR_OK || !isExists || isOverSize) {
        ACCOUNT_LOGE("input constraints not in constraints list or is oversize!");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }

    osAccountControl_->UpdateGlobalOAConstraints(std::to_string(enforcerId), constraints, enable);

    errCode = DealWithDeviceOwnerId(isDeviceOwner, enforcerId);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("deal with device owner id error");
        return errCode;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SetSpecificOsAccountConstraints(const std::vector<std::string> &constraints,
    const bool enable, const int32_t targetId, const int32_t enforcerId, const bool isDeviceOwner)
{
    OsAccountInfo enforcerOsAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(enforcerId, enforcerOsAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error");
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

    OsAccountInfo targetOsAccountInfo;
    errCode = osAccountControl_->GetOsAccountInfoById(targetId, targetOsAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error");
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    if (targetOsAccountInfo.GetToBeRemoved() || enforcerOsAccountInfo.GetToBeRemoved()) {
        ACCOUNT_LOGE("account %{public}d or %{public}d will be removed, cannot change constraints!",
            enforcerId, targetId);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }

    bool isExists = false;
    bool isOverSize = false;
    errCode = osAccountControl_->CheckConstraintsList(constraints, isExists, isOverSize);
    if (errCode != ERR_OK || !isExists || isOverSize) {
        ACCOUNT_LOGE("input constraints not in constraints list or is oversize!");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }

    osAccountControl_->UpdateSpecificOAConstraints(
        std::to_string(enforcerId), std::to_string(targetId), constraints, enable);

    errCode = DealWithDeviceOwnerId(isDeviceOwner, enforcerId);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("deal with device owner id error");
        return errCode;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::QueryAllCreatedOsAccounts(std::vector<OsAccountInfo> &osAccountInfos)
{
    ErrCode errCode = osAccountControl_->GetOsAccountList(osAccountInfos);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info list error, errCode %{public}d.", errCode);
        return errCode;
    }
#ifndef ENABLE_MULTIPLE_ACTIVE_ACCOUNTS
    for (auto osAccountInfosPtr = osAccountInfos.begin(); osAccountInfosPtr != osAccountInfos.end();
         ++osAccountInfosPtr) {
        if (IsOsAccountIDInActiveList(osAccountInfosPtr->GetLocalId())) {
            osAccountInfosPtr->SetIsActived(true);
        } else {
            osAccountInfosPtr->SetIsActived(false);
        }
    }
#endif // ENABLE_MULTIPLE_ACTIVE_ACCOUNTS
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::DealWithDeviceOwnerId(const bool isDeviceOwner, const int32_t localId)
{
    ACCOUNT_LOGD("enter.");
    if (isDeviceOwner && localId != deviceOwnerId_) {
        ACCOUNT_LOGI("this device owner os account id is changed!");
        deviceOwnerId_ = localId;
        return osAccountControl_->UpdateDeviceOwnerId(localId);
    }
    if (isDeviceOwner == false && localId == deviceOwnerId_) {
        deviceOwnerId_ = -1;
        return osAccountControl_->UpdateDeviceOwnerId(-1);
    }
    return ERR_OK;
}

void IInnerOsAccountManager::CleanGarbageAccounts()
{
    ACCOUNT_LOGD("enter.");
    std::vector<OsAccountInfo> osAccountInfos;
    if (QueryAllCreatedOsAccounts(osAccountInfos) != ERR_OK) {
        ACCOUNT_LOGI("QueryAllCreatedOsAccounts failed.");
        return;
    }

    // check status and remove garbage accounts data
    for (size_t i = 0; i < osAccountInfos.size(); ++i) {
        if (!osAccountInfos[i].GetToBeRemoved()) {
            continue;
        }

        if (osAccountInfos[i].GetLocalId() == Constants::START_USER_ID ||
            osAccountInfos[i].GetLocalId() == Constants::ADMIN_LOCAL_ID) {
            continue;
        }

        ErrCode errCode = SendMsgForAccountRemove(osAccountInfos[i]);
        if (errCode != ERR_OK) {
            ACCOUNT_LOGE("remove account %{public}d failed! errCode %{public}d.",
                osAccountInfos[i].GetLocalId(), errCode);
        } else {
            ACCOUNT_LOGI("remove account %{public}d succeed!", osAccountInfos[i].GetLocalId());
        }
    }
    ACCOUNT_LOGI("finished.");
}

ErrCode IInnerOsAccountManager::GetOsAccountLocalIdFromDomain(const DomainAccountInfo &domainInfo, int &id)
{
    if (domainInfo.domain_.size() > Constants::DOMAIN_NAME_MAX_SIZE) {
        ACCOUNT_LOGE("invalid domain name length %{public}zu.", domainInfo.domain_.size());
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }

    if (domainInfo.accountName_.size() > Constants::DOMAIN_ACCOUNT_NAME_MAX_SIZE) {
        ACCOUNT_LOGE("invalid domain account name length %{public}zu.", domainInfo.accountName_.size());
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }

    id = -1;
    std::vector<OsAccountInfo> osAccountInfos;
    ErrCode errCode = osAccountControl_->GetOsAccountList(osAccountInfos);
    if (errCode != ERR_OK) {
        return errCode;
    }

    DomainAccountInfo curDomainInfo;
    for (auto osAccountInfosPtr = osAccountInfos.begin(); osAccountInfosPtr != osAccountInfos.end();
         ++osAccountInfosPtr) {
        osAccountInfosPtr->GetDomainInfo(curDomainInfo);
        if (((!domainInfo.accountId_.empty()) && (domainInfo.accountId_ == curDomainInfo.accountId_)) ||
            ((!domainInfo.accountName_.empty()) && (curDomainInfo.accountName_ == domainInfo.accountName_) &&
            (!domainInfo.domain_.empty()) && (curDomainInfo.domain_ == domainInfo.domain_))) {
            id = osAccountInfosPtr->GetLocalId();
            return ERR_OK;
        }
    }
    return ERR_DOMAIN_ACCOUNT_SERVICE_NOT_DOMAIN_ACCOUNT;
}

ErrCode IInnerOsAccountManager::GetOsAccountShortName(const int id, std::string &shortName)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    shortName = osAccountInfo.GetShortName();
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::QueryOsAccountById(const int id, OsAccountInfo &osAccountInfo)
{
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

    if (IsOsAccountIDInActiveList(id)) {
        osAccountInfo.SetIsActived(true);
    } else {
        osAccountInfo.SetIsActived(false);
    }

    if (osAccountInfo.GetPhoto() != "") {
        std::string photo = osAccountInfo.GetPhoto();
        errCode = osAccountControl_->GetPhotoById(osAccountInfo.GetLocalId(), photo);
        if (errCode != ERR_OK) {
            ACCOUNT_LOGE("get osaccount photo error, errCode %{public}d.", errCode);
            return errCode;
        }
        osAccountInfo.SetPhoto(photo);
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetOsAccountType(const int id, OsAccountType &type)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    type = osAccountInfo.GetType();
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetOsAccountProfilePhoto(const int id, std::string &photo)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = QueryOsAccountById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("QueryOsAccountById return error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    photo = osAccountInfo.GetPhoto();
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::IsMultiOsAccountEnable(bool &isMultiOsAccountEnable)
{
#ifdef ENABLE_MULTIPLE_OS_ACCOUNTS
    ErrCode errCode = osAccountControl_->GetIsMultiOsAccountEnable(isMultiOsAccountEnable);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("GetIsMultiOsAccountEnable error, errCode %{public}d.", errCode);
        return errCode;
    }
#else
    isMultiOsAccountEnable = false;
#endif // ENABLE_MULTIPLE_OS_ACCOUNTS
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SetOsAccountName(const int id, const std::string &name)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

    // to be removed, cannot change any thing
    if (osAccountInfo.GetToBeRemoved()) {
        ACCOUNT_LOGE("account %{public}d will be removed, cannot change name!", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }

    std::string localName = osAccountInfo.GetLocalName();
    if (localName == name) {
        return ERR_OK;
    }

    osAccountInfo.SetLocalName(name);
    errCode = ValidateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("account name already exist, errCode %{public}d.", errCode);
        return errCode;
    }

    errCode = osAccountControl_->UpdateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("update osaccount info error %{public}d, id: %{public}d", errCode, osAccountInfo.GetLocalId());
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }
    osAccountControl_->UpdateAccountIndex(osAccountInfo, false);
    OsAccountInterface::PublishCommonEvent(
        osAccountInfo, OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_INFO_UPDATED, Constants::OPERATION_UPDATE);
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SetOsAccountConstraints(
    const int id, const std::vector<std::string> &constraints, const bool enable)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

    // to be removed, cannot change any thing
    if (osAccountInfo.GetToBeRemoved()) {
        ACCOUNT_LOGE("account %{public}d will be removed, cannot change constraints!", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }

    bool isExists = false;
    bool isOverSize = false;
    errCode = osAccountControl_->CheckConstraintsList(constraints, isExists, isOverSize);
    if (errCode != ERR_OK || !isExists || isOverSize) {
        ACCOUNT_LOGE("input constraints not in constraints list or is oversize!");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    std::vector<std::string> oldConstraints = osAccountInfo.GetConstraints();
    for (auto it = constraints.begin(); it != constraints.end(); it++) {
        if (enable) {
            if (std::find(oldConstraints.begin(), oldConstraints.end(), *it) == oldConstraints.end()) {
                oldConstraints.push_back(*it);
            }
        } else {
            oldConstraints.erase(
                std::remove(oldConstraints.begin(), oldConstraints.end(), *it), oldConstraints.end());
        }
    }
    osAccountInfo.SetConstraints(oldConstraints);
    errCode = osAccountControl_->UpdateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("update osaccount info error %{public}d, id: %{public}d", errCode, osAccountInfo.GetLocalId());
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SetOsAccountProfilePhoto(const int id, const std::string &photo)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

    // to be removed, cannot change any thing
    if (osAccountInfo.GetToBeRemoved()) {
        ACCOUNT_LOGE("account %{public}d will be removed, cannot change photo!", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }

    if (osAccountInfo.GetPhoto() == photo) {
        return ERR_OK;
    }
    errCode = osAccountControl_->SetPhotoById(id, photo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("set photo by id error, errCode %{public}d.", errCode);
        return errCode;
    }
    auto sizeType = photo.find(Constants::USER_PHOTO_BASE_JPG_HEAD);
    if (sizeType == std::string::npos) {
        osAccountInfo.SetPhoto(Constants::USER_PHOTO_FILE_PNG_NAME);
    } else {
        osAccountInfo.SetPhoto(Constants::USER_PHOTO_FILE_JPG_NAME);
    }
    errCode = osAccountControl_->UpdateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("update osaccount info error %{public}d, id: %{public}d", errCode, osAccountInfo.GetLocalId());
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }
    OsAccountInterface::PublishCommonEvent(
        osAccountInfo, OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_INFO_UPDATED, Constants::OPERATION_UPDATE);
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::DeactivateOsAccountByInfo(OsAccountInfo &osAccountInfo)
{
    int localId = osAccountInfo.GetLocalId();
    if (localId == Constants::ADMIN_LOCAL_ID) {
        ACCOUNT_LOGI("this osaccount can't deactive, id: %{public}d", Constants::ADMIN_LOCAL_ID);
        return ERR_OK;
    }

    osAccountInfo.SetIsActived(false);
    osAccountInfo.SetDisplayId(Constants::INVALID_DISPALY_ID);
    osAccountInfo.SetIsForeground(false);
    ErrCode errCode = osAccountControl_->UpdateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Update account failed, id=%{public}d, errCode=%{public}d.", localId, errCode);
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }
    int32_t foregroundId = -1;
    if (foregroundAccountMap_.Find(Constants::DEFAULT_DISPALY_ID, foregroundId) && foregroundId == localId) {
        foregroundAccountMap_.Erase(Constants::DEFAULT_DISPALY_ID);
    }

    EraseIdFromActiveList(localId);

    AccountInfoReport::ReportSecurityInfo(osAccountInfo.GetLocalName(), localId,
                                          ReportEvent::EVENT_LOGOUT, 0);
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::DeactivateOsAccountById(const int id)
{
    if (id == Constants::ADMIN_LOCAL_ID) {
        ACCOUNT_LOGI("this osaccount can't deactive, id: %{public}d", Constants::ADMIN_LOCAL_ID);
        return ERR_OK;
    }

#ifndef SUPPROT_STOP_MAIN_OS_ACCOUNT
    if (id == Constants::START_USER_ID) {
        ACCOUNT_LOGI("this osaccount can't deactive, id: %{public}d", Constants::START_USER_ID);
        return ERR_OK;
    }
#endif // SUPPORT_STOP_OS_ACCOUNT

    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("cannot get os account %{public}d info. error %{public}d.",
            id, errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    osAccountInfo.SetIsActived(false);
    osAccountInfo.SetIsForeground(false);
    osAccountInfo.SetDisplayId(Constants::INVALID_DISPALY_ID);
    errCode = osAccountControl_->UpdateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("update %{public}d account info failed, errCode %{public}d.",
            osAccountInfo.GetLocalId(), errCode);
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }

    EraseIdFromActiveList(osAccountInfo.GetLocalId());

    AccountInfoReport::ReportSecurityInfo(osAccountInfo.GetLocalName(), id, ReportEvent::EVENT_LOGOUT, 0);
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::ActivateOsAccount(const int id, const uint64_t displayId)
{
    if (!CheckAndAddLocalIdOperating(id)) {
        ACCOUNT_LOGE("the %{public}d already in operating", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_OPERATING_ERROR;
    }
    int32_t foregroundId = -1;
    if (foregroundAccountMap_.Find(displayId, foregroundId) && foregroundId == id) {
        ACCOUNT_LOGI("Account id=%{public}d already is foreground", id);
        RemoveLocalIdToOperating(id);
        return ERR_OK;
    }

    // get information
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGE("cannot find os account info by id:%{public}d, errCode %{public}d.", id, errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

    // check complete
    if (!osAccountInfo.GetIsCreateCompleted()) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGE("account %{public}d is not completed", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_IS_UNCOMPLETED_ERROR;
    }

    // check to be removed
    if (osAccountInfo.GetToBeRemoved()) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGE("account %{public}d will be removed, cannot be activated!", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }

    subscribeManager_.Publish(id, OS_ACCOUNT_SUBSCRIBE_TYPE::ACTIVATING);
    errCode = SendMsgForAccountActivate(osAccountInfo);
    RemoveLocalIdToOperating(id);
    if (errCode != ERR_OK) {
        return errCode;
    }

    DomainAccountInfo domainInfo;
    osAccountInfo.GetDomainInfo(domainInfo);
    if (domainInfo.accountId_.empty() && (osAccountInfo.GetCredentialId() == 0)) {
        AccountInfoReport::ReportSecurityInfo(
            osAccountInfo.GetLocalName(), osAccountInfo.GetLocalId(), ReportEvent::EVENT_LOGIN, 0);
    }
    ACCOUNT_LOGI("IInnerOsAccountManager ActivateOsAccount end");
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::DeactivateOsAccount(const int id)
{
    if (!CheckAndAddLocalIdOperating(id)) {
        ACCOUNT_LOGW("the %{public}d already in operating", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_OPERATING_ERROR;
    }

    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGW("cannot find os account info by id:%{public}d, errCode %{public}d.", id, errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

    if ((!IsOsAccountIDInActiveList(id)) && (!osAccountInfo.GetIsVerified())) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGW("account %{public}d is neither active nor verified, don't need to deactivate!", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_IS_UNVERIFIED_ERROR;
    }
    if (!osAccountInfo.GetIsCreateCompleted()) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGW("account %{public}d is not completed", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_IS_UNCOMPLETED_ERROR;
    }
    if (osAccountInfo.GetToBeRemoved()) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGW("account %{public}d will be removed, don't need to deactivate!", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }

    OsAccountInterface::PublishCommonEvent(
        osAccountInfo, OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_STOPPING, Constants::OPERATION_STOP);
    subscribeManager_.Publish(id, OS_ACCOUNT_SUBSCRIBE_TYPE::STOPPING);

    errCode = SendMsgForAccountDeactivate(osAccountInfo);
    if (errCode != ERR_OK) {
        RemoveLocalIdToOperating(id);
        ReportOsAccountOperationFail(id, "deactivate", errCode, "deactivate os account failed");
        return errCode;
    }

    OsAccountInterface::PublishCommonEvent(osAccountInfo, OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_STOPPED,
                                           Constants::OPERATION_STOP);
    subscribeManager_.Publish(id, OS_ACCOUNT_SUBSCRIBE_TYPE::STOPPED);

    RemoveLocalIdToOperating(id);
    ACCOUNT_LOGI("IInnerOsAccountManager DeactivateOsAccount end");
    return ERR_OK;
}

void IInnerOsAccountManager::WatchStartUser(std::int32_t id)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_FOR_ACCOUNT_BOOT_EVENT_READY));
    OsAccountInfo osAccountInfo;
    osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (!osAccountInfo.GetIsActived()) {
        ReportOsAccountOperationFail(
            id, Constants::OPERATION_ACTIVATE, ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT, "account activation timed out!");
    }
    SetParameter(ACCOUNT_READY_EVENT.c_str(), "true");
}

ErrCode IInnerOsAccountManager::SendMsgForAccountActivate(OsAccountInfo &osAccountInfo, const uint64_t displayId)
{
    // activate
    int32_t oldId = -1;
    bool oldIdExist = foregroundAccountMap_.Find(displayId, oldId);
    int localId = osAccountInfo.GetLocalId();
    subscribeManager_.Publish(localId, oldId, OS_ACCOUNT_SUBSCRIBE_TYPE::SWITCHING);
    ErrCode errCode = OsAccountInterface::SendToStorageAccountStart(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("account %{public}d call storage active failed, errCode %{public}d.",
            localId, errCode);
        return ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER;
    }
    errCode = OsAccountInterface::SendToAMSAccountStart(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("account %{public}d call ams active failed, errCode %{public}d.",
            localId, errCode);
        return errCode;
    }
    if (oldIdExist) {
        OsAccountInfo oldOsAccountInfo;
        errCode = osAccountControl_->GetOsAccountInfoById(oldId, oldOsAccountInfo);
        if (errCode != ERR_OK) {
            ACCOUNT_LOGE("Get osaccount info failed, errCode=%{public}d.", errCode);
            return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
        }
        errCode = UpdateAccountToBackground(oldOsAccountInfo);
        if (errCode != ERR_OK) {
            return errCode;
        }
    }

    errCode = UpdateAccountToForeground(displayId, osAccountInfo);
    if (errCode != ERR_OK) {
        return errCode;
    }
    ReportOsAccountSwitch(localId, oldId);
#ifndef ENABLE_MULTIPLE_ACTIVE_ACCOUNTS
    if (oldId >= Constants::START_USER_ID) {
        DeactivateOsAccountById(oldId);
    }
#endif // ENABLE_MULTIPLE_ACTIVE_ACCOUNTS
    PushIdIntoActiveList(localId);
    SetParameter(ACCOUNT_READY_EVENT.c_str(), "true");
    OsAccountInterface::SendToCESAccountSwitched(localId, oldId);
    subscribeManager_.Publish(localId, OS_ACCOUNT_SUBSCRIBE_TYPE::ACTIVED);
    subscribeManager_.Publish(localId, oldId, OS_ACCOUNT_SUBSCRIBE_TYPE::SWITCHED);
    ACCOUNT_LOGI("SendMsgForAccountActivate ok");
    return errCode;
}

ErrCode IInnerOsAccountManager::StartOsAccount(const int id)
{
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::StopOsAccount(const int id)
{
    if (id == Constants::START_USER_ID) {
        ACCOUNT_LOGW("Account id=%{public}d can't stop", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_STOP_ACTIVE_ERROR;
    }

    if (!CheckAndAddLocalIdOperating(id)) {
        ACCOUNT_LOGW("Account id=%{public}d already in operating", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_OPERATING_ERROR;
    }
    if (!IsOsAccountIDInActiveList(id)) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGI("Account id=%{public}d already stop", id);
        return ERR_OK;
    }
    // get information
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGW("Cannot find os account info by id:%{public}d, errCode %{public}d.", id, errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

     // check complete
    if (!osAccountInfo.GetIsCreateCompleted()) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGW("Account id=%{public}d is not completed", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_IS_UNCOMPLETED_ERROR;
    }

     // check to be removed
    if (osAccountInfo.GetToBeRemoved()) {
        RemoveLocalIdToOperating(id);
        ACCOUNT_LOGW("Account id=%{public}d will be removed, don't need to stop!", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }

    // stop
    errCode = SendMsgForAccountStop(osAccountInfo);
    if (errCode != ERR_OK) {
        RemoveLocalIdToOperating(id);
        ReportOsAccountOperationFail(id, "stop", errCode, "stop os account failed");
        ACCOUNT_LOGE("Update account failed, id=%{public}d, errCode %{public}d.", id, errCode);
        return errCode;
    }
    RemoveLocalIdToOperating(id);
    ACCOUNT_LOGI("IInnerOsAccountManager StopOsAccount end");
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetOsAccountLocalIdBySerialNumber(const int64_t serialNumber, int &id)
{
    if (serialNumber ==
        Constants::CARRY_NUM * Constants::SERIAL_NUMBER_NUM_START_FOR_ADMIN + Constants::ADMIN_LOCAL_ID) {
        id = Constants::ADMIN_LOCAL_ID;
        return ERR_OK;
    }
    std::vector<OsAccountInfo> osAccountInfos;
    id = -1;
    ErrCode errCode = osAccountControl_->GetOsAccountList(osAccountInfos);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info list error");
        return errCode;
    }
    for (auto it = osAccountInfos.begin(); it != osAccountInfos.end(); it++) {
        if (serialNumber == it->GetSerialNumber()) {
            id = it->GetLocalId();
            break;
        }
    }
    if (id == -1) {
        ACCOUNT_LOGE("cannot find id by serialNumber");
        return ERR_OSACCOUNT_SERVICE_INNER_SELECT_ERROR;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetOsAccountInfoById(const int id, OsAccountInfo &osAccountInfo)
{
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetSerialNumberByOsAccountLocalId(const int &id, int64_t &serialNumber)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    serialNumber = osAccountInfo.GetSerialNumber();
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SubscribeOsAccount(
    const OsAccountSubscribeInfo &subscribeInfo, const sptr<IRemoteObject> &eventListener)
{
    auto subscribeInfoPtr = std::make_shared<OsAccountSubscribeInfo>(subscribeInfo);
    if (subscribeInfoPtr == nullptr) {
        ACCOUNT_LOGE("subscribeInfoPtr is nullptr");
    }
    return subscribeManager_.SubscribeOsAccount(subscribeInfoPtr, eventListener);
}

ErrCode IInnerOsAccountManager::UnsubscribeOsAccount(const sptr<IRemoteObject> &eventListener)
{
    return subscribeManager_.UnsubscribeOsAccount(eventListener);
}

const std::shared_ptr<OsAccountSubscribeInfo> IInnerOsAccountManager::GetSubscribeRecordInfo(
    const sptr<IRemoteObject> &eventListener)
{
    return subscribeManager_.GetSubscribeRecordInfo(eventListener);
}

OS_ACCOUNT_SWITCH_MOD IInnerOsAccountManager::GetOsAccountSwitchMod()
{
    return Constants::NOW_OS_ACCOUNT_SWITCH_MOD;
}

ErrCode IInnerOsAccountManager::IsOsAccountCompleted(const int id, bool &isOsAccountCompleted)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error,id %{public}d, errCode %{public}d.", id, errCode);
        return errCode;
    }
    isOsAccountCompleted = osAccountInfo.GetIsCreateCompleted();
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SetOsAccountIsVerified(const int id, const bool isVerified)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

    // to be removed, cannot change any thing
    if (osAccountInfo.GetToBeRemoved()) {
        ACCOUNT_LOGE("account %{public}d will be removed, cannot change verify state!", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }

    if (isVerified && !osAccountInfo.GetIsVerified()) {
        OsAccountInterface::PublishCommonEvent(osAccountInfo,
            OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_UNLOCKED, Constants::OPERATION_UNLOCK);
        subscribeManager_.Publish(id, OS_ACCOUNT_SUBSCRIBE_TYPE::UNLOCKED);
    }

    osAccountInfo.SetIsVerified(isVerified);
    errCode = osAccountControl_->UpdateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("update osaccount info error %{public}d, id: %{public}d",
            errCode, osAccountInfo.GetLocalId());
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetOsAccountCredentialId(const int id, uint64_t &credentialId)
{
    credentialId = 0;
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode == ERR_OK) {
        credentialId = osAccountInfo.GetCredentialId();
    }
    return errCode;
}

ErrCode IInnerOsAccountManager::SetOsAccountCredentialId(const int id, uint64_t credentialId)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }

    osAccountInfo.SetCredentialId(credentialId);
    errCode = osAccountControl_->UpdateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("update osaccount info error %{public}d, id: %{public}d",
            errCode, osAccountInfo.GetLocalId());
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::SetDefaultActivatedOsAccount(const int32_t id)
{
    std::lock_guard<std::mutex> lock(operatingMutex_);
    if (id == defaultActivatedId_) {
        ACCOUNT_LOGW("no need to repeat set initial start id %{public}d", id);
        return ERR_OK;
    }
    OsAccountInfo osAccountInfo;
    ErrCode errCode = osAccountControl_->GetOsAccountInfoById(id, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("get osaccount info error, errCode %{public}d.", errCode);
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    // to be removed, cannot change any thing
    if (osAccountInfo.GetToBeRemoved()) {
        ACCOUNT_LOGE("account %{public}d will be removed, cannot change verify state!", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR;
    }
    if (!osAccountInfo.GetIsCreateCompleted()) {
        ACCOUNT_LOGE("account %{public}d is not completed", id);
        return ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_IS_UNCOMPLETED_ERROR;
    }
    errCode = osAccountControl_->SetDefaultActivatedOsAccount(id);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("set default activated account id error %{public}d, id: %{public}d", errCode, id);
        return errCode;
    }
    defaultActivatedId_ = id;
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::IsOsAccountForeground(const int32_t localId, const uint64_t displayId,
                                                      bool &isForeground)
{
    int32_t id;
    if (!foregroundAccountMap_.Find(displayId, id)) {
        ACCOUNT_LOGE("No foreground account in displayId %{public}llu.", static_cast<unsigned long long>(displayId));
        return ERR_ACCOUNT_COMMON_ACCOUNT_IN_DISPLAY_ID_NOT_FOUND_ERROR;
    }
    ACCOUNT_LOGI("Get foreground account %{public}d in displayId %{public}llu, qurey localId=%{public}d.", id,
                 static_cast<unsigned long long>(displayId), localId);
    isForeground = (id == localId);
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetForegroundOsAccountLocalId(const uint64_t displayId, int32_t &localId)
{
    if (!foregroundAccountMap_.Find(displayId, localId)) {
        ACCOUNT_LOGE("No foreground account in displayId %{public}llu.", static_cast<unsigned long long>(displayId));
        return ERR_ACCOUNT_COMMON_ACCOUNT_IN_DISPLAY_ID_NOT_FOUND_ERROR;
    }
    ACCOUNT_LOGI("Get foreground account %{public}d in displayId %{public}llu.", localId,
                 static_cast<unsigned long long>(displayId));
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetForegroundOsAccounts(std::vector<ForegroundOsAccount> &accounts)
{
    accounts.clear();
    auto it = [&](uint64_t displayId, int32_t localId) {
        ForegroundOsAccount foregroundOsAccount;
        foregroundOsAccount.displayId = displayId;
        foregroundOsAccount.localId = localId;
        accounts.emplace_back(foregroundOsAccount);
    };
    foregroundAccountMap_.Iterate(it);
    ACCOUNT_LOGI("Get foreground list successful, total=%{public}zu.", accounts.size());
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetBackgroundOsAccountLocalIds(std::vector<int32_t> &localIds)
{
    localIds.clear();
    std::vector<int32_t> activatedIds;
    CopyFromActiveList(activatedIds);

    std::vector<int32_t> foregroundIds;
    auto it = [&](uint64_t displayId, int32_t localId) {
        foregroundIds.emplace_back(localId);
    };
    foregroundAccountMap_.Iterate(it);
    std::unordered_set<int32_t> foregroundSet(foregroundIds.begin(), foregroundIds.end());
    for (const auto &id : activatedIds) {
        if (foregroundSet.find(id) == foregroundSet.end()) {
            localIds.emplace_back(id);
        }
    }
    ACCOUNT_LOGI("Get background list successful, total=%{public}zu.", localIds.size());
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::GetDefaultActivatedOsAccount(int32_t &id)
{
    std::lock_guard<std::mutex> lock(operatingMutex_);
    id = defaultActivatedId_;
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::IsAllowedCreateAdmin(bool &isAllowedCreateAdmin)
{
    return osAccountControl_->IsAllowedCreateAdmin(isAllowedCreateAdmin);
}

ErrCode IInnerOsAccountManager::GetCreatedOsAccountNumFromDatabase(const std::string& storeID,
    int &createdOsAccountNum)
{
    return osAccountControl_->GetCreatedOsAccountNumFromDatabase(storeID, createdOsAccountNum);
}

ErrCode IInnerOsAccountManager::GetSerialNumberFromDatabase(const std::string& storeID,
    int64_t &serialNumber)
{
    return osAccountControl_->GetSerialNumberFromDatabase(storeID, serialNumber);
}

ErrCode IInnerOsAccountManager::GetMaxAllowCreateIdFromDatabase(const std::string& storeID, int &id)
{
#ifdef ENABLE_MULTIPLE_OS_ACCOUNTS
    return osAccountControl_->GetMaxAllowCreateIdFromDatabase(storeID, id);
#else
    id = Constants::START_USER_ID;
    return ERR_OK;
#endif // ENABLE_MULTIPLE_OS_ACCOUNTS
}

ErrCode IInnerOsAccountManager::GetOsAccountFromDatabase(const std::string& storeID, const int id,
    OsAccountInfo &osAccountInfo)
{
    return osAccountControl_->GetOsAccountFromDatabase(storeID, id, osAccountInfo);
}

ErrCode IInnerOsAccountManager::GetOsAccountListFromDatabase(const std::string& storeID,
    std::vector<OsAccountInfo> &osAccountList)
{
    return osAccountControl_->GetOsAccountListFromDatabase(storeID, osAccountList);
}

void IInnerOsAccountManager::RemoveLocalIdToOperating(int32_t localId)
{
    std::lock_guard<std::mutex> lock(operatingMutex_);
    auto it = std::find(operatingId_.begin(), operatingId_.end(), localId);
    if (it != operatingId_.end()) {
        operatingId_.erase(it);
    }
}

bool IInnerOsAccountManager::CheckAndAddLocalIdOperating(int32_t localId)
{
    std::lock_guard<std::mutex> lock(operatingMutex_);
    if (std::find(operatingId_.begin(), operatingId_.end(), localId) != operatingId_.end()) {
        return false;
    }
    operatingId_.push_back(localId);
    return true;
}

ErrCode IInnerOsAccountManager::QueryActiveOsAccountIds(std::vector<int32_t>& ids)
{
    CopyFromActiveList(ids);
    return ERR_OK;
}

void IInnerOsAccountManager::PushIdIntoActiveList(int32_t id)
{
    std::lock_guard<std::mutex> lock(ativeMutex_);
    if (std::find(activeAccountId_.begin(), activeAccountId_.end(), id) == activeAccountId_.end()) {
        CountTraceAdapter("activeId", (int64_t)id);
    }
#ifndef ENABLE_MULTIPLE_ACTIVE_ACCOUNTS
    activeAccountId_.clear();
#else
    activeAccountId_.erase(std::remove(activeAccountId_.begin(), activeAccountId_.end(), id), activeAccountId_.end());
#endif
    //Compatible with the QueryActiveOsAccountIds
    activeAccountId_.insert(activeAccountId_.begin(), id);
    return;
}

void IInnerOsAccountManager::EraseIdFromActiveList(int32_t id)
{
    std::lock_guard<std::mutex> lock(ativeMutex_);
    if (std::find(activeAccountId_.begin(), activeAccountId_.end(), id) != activeAccountId_.end()) {
        ACCOUNT_LOGE("EraseIdFromActiveList enter0");
        activeAccountId_.erase(
            std::remove(activeAccountId_.begin(), activeAccountId_.end(), id), activeAccountId_.end());
    } else {
        ACCOUNT_LOGI("os account is not in active list, no need to erase!");
    }
    CountTraceAdapter("deActiveId", (int64_t)id);
}

bool IInnerOsAccountManager::IsOsAccountIDInActiveList(int32_t id)
{
    std::lock_guard<std::mutex> lock(ativeMutex_);
    auto it = std::find(activeAccountId_.begin(), activeAccountId_.end(), id);
    return (it != activeAccountId_.end());
}

void IInnerOsAccountManager::CopyFromActiveList(std::vector<int32_t>& idList)
{
    idList.clear();
    std::lock_guard<std::mutex> lock(ativeMutex_);
    for (auto it = activeAccountId_.begin(); it != activeAccountId_.end(); it++) {
        idList.push_back(*it);
    }
}

ErrCode IInnerOsAccountManager::UpdateAccountToForeground(const uint64_t displayId, OsAccountInfo &osAccountInfo)
{
    int32_t localId = osAccountInfo.GetLocalId();
    osAccountInfo.SetIsActived(true);
    osAccountInfo.SetDisplayId(displayId);
    osAccountInfo.SetIsForeground(true);
    int64_t time =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    osAccountInfo.SetLastLoginTime(time);
    ErrCode errCode = osAccountControl_->UpdateOsAccount(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Update account failed, localId=%{public}d, errCode=%{public}d.",
            localId, errCode);
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }
    foregroundAccountMap_.EnsureInsert(displayId, localId);
    OsAccountInterface::PublishCommonEvent(osAccountInfo,
        OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_FOREGROUND, Constants::OPERATION_SWITCH);
    return ERR_OK;
}

ErrCode IInnerOsAccountManager::UpdateAccountToBackground(OsAccountInfo &oldOsAccountInfo)
{
    oldOsAccountInfo.SetIsForeground(false);
    oldOsAccountInfo.SetDisplayId(Constants::INVALID_DISPALY_ID);
    ErrCode errCode = osAccountControl_->UpdateOsAccount(oldOsAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Update osaccount failed, errCode=%{public}d, id=%{public}d",
            errCode, oldOsAccountInfo.GetLocalId());
        return ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR;
    }
    OsAccountInterface::PublishCommonEvent(oldOsAccountInfo,
        OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_BACKGROUND, Constants::OPERATION_SWITCH);
    return errCode;
}
}  // namespace AccountSA
}  // namespace OHOS