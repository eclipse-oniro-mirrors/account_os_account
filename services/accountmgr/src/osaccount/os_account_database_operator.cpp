/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include "os_account_database_operator.h"
#include "account_log_wrapper.h"
#include "os_account_constants.h"
#include "os_account_data_storage.h"

namespace OHOS {
namespace AccountSA {
namespace {
const char OS_ACCOUNT_STORE_ID[] = "os_account_info";
const char APP_ID[] = "os_account_mgr_service";
}

OsAccountDatabaseOperator::OsAccountDatabaseOperator()
{}

OsAccountDatabaseOperator::~OsAccountDatabaseOperator()
{}

bool OsAccountDatabaseOperator::InnerInit()
{
    std::lock_guard<std::mutex> lock(initMutex_);
    if (accountDataStorage_ == nullptr) {
        ACCOUNT_LOGI("Database operator inner init, create accountDataStorage_!");
        accountDataStorage_ = std::make_shared<OsAccountDataStorage>(
            APP_ID, OS_ACCOUNT_STORE_ID, Constants::SYNC_OS_ACCOUNT_DATABASE);
    }
    if (!accountDataStorage_->IsKeyExists(Constants::ACCOUNT_LIST)) {
        ACCOUNT_LOGI("Database operator inner init, create account list.");
        std::vector<std::string> accountListVec;
        auto accountList = CreateJson();
        AddVectorStringToJson(accountList, Constants::ACCOUNT_LIST, accountListVec);
        AddIntToJson(accountList, Constants::COUNT_ACCOUNT_NUM, 0);
        AddIntToJson(accountList, Constants::MAX_ALLOW_CREATE_ACCOUNT_ID, Constants::MAX_USER_ID);
        AddInt64ToJson(accountList, Constants::SERIAL_NUMBER_NUM, Constants::SERIAL_NUMBER_NUM_START);
        std::string strValue = PackJsonToString(accountList);
        ErrCode errCode = accountDataStorage_->PutValueToKvStore(
            Constants::ACCOUNT_LIST, strValue);
        if (errCode != ERR_OK) {
            ACCOUNT_LOGE("Initialize account list failed! errCode %{public}d.", errCode);
            return false;
        }
    }
    return true;
}

ErrCode OsAccountDatabaseOperator::GetOsAccountListFromDatabase(const std::string& storeID,
                                                                std::vector<OsAccountInfo> &osAccountList)
{
    osAccountList.clear();
    if (!InnerInit()) {
        ACCOUNT_LOGE("InnerInit failed!");
        return ERR_ACCOUNT_COMMON_NOT_INIT_ERROR;
    }

    std::map<std::string, std::shared_ptr<IAccountInfo>> osAccountMapInfos;
    ErrCode errCode = ERR_OK;
    if (storeID.empty()) {
        errCode = accountDataStorage_->LoadAllData(osAccountMapInfos);
    } else {
        std::shared_ptr<AccountDataStorage> storagePtr = std::make_shared<OsAccountDataStorage>(
            APP_ID, storeID, Constants::SYNC_OS_ACCOUNT_DATABASE);
        if (storagePtr == nullptr) {
            ACCOUNT_LOGE("StoragePtr is still nullptr, storeID %{public}s.", storeID.c_str());
            return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
        }
        errCode = storagePtr->LoadAllData(osAccountMapInfos);
    }

    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("LoadAllData failed! storeID %{public}s.", storeID.c_str());
        return errCode;
    }

    for (auto it = osAccountMapInfos.begin(); it != osAccountMapInfos.end(); it++) {
        OsAccountInfo curOsInfo = *(std::static_pointer_cast<OsAccountInfo>(it->second));
        if (curOsInfo.GetLocalId() >= Constants::START_USER_ID) {
            osAccountList.push_back(curOsInfo);
        }
    }
    return ERR_OK;
}

void OsAccountDatabaseOperator::InsertOsAccountIntoDataBase(const OsAccountInfo &osAccountInfo)
{
    if (!InnerInit()) {
        ACCOUNT_LOGE("InnerInit failed! target localID %{public}d!", osAccountInfo.GetLocalId());
        return;
    }

    if (osAccountInfo.GetLocalId() < Constants::START_USER_ID && osAccountInfo.GetLocalId() != Constants::U1_ID) {
        ACCOUNT_LOGI("Target os account id %{public}d will not be saved in database!", osAccountInfo.GetLocalId());
        return;
    }

    ErrCode errCode = accountDataStorage_->AddAccountInfo(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("AddAccountInfo failed, error code %{public}d, target id %{public}d.",
            errCode, osAccountInfo.GetLocalId());
        return;
    }
    ACCOUNT_LOGI("Insert account %{public}d to database succeed.", osAccountInfo.GetLocalId());
}

void OsAccountDatabaseOperator::DelOsAccountFromDatabase(const int id)
{
    if (!InnerInit()) {
        ACCOUNT_LOGE("InnerInit failed! id %{public}d!", id);
        return;
    }

    ErrCode errCode = accountDataStorage_->RemoveValueFromKvStore(std::to_string(id));
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Delete os account %{public}d from database failed! error code %{public}d.", id, errCode);
    } else {
        ACCOUNT_LOGI("Delete os account %{public}d from database succeed!", id);
    }
}

void OsAccountDatabaseOperator::UpdateOsAccountInDatabase(const OsAccountInfo &osAccountInfo)
{
    if (!InnerInit()) {
        ACCOUNT_LOGE("InnerInit failed! local id %{public}d!", osAccountInfo.GetLocalId());
        return;
    }

    ErrCode errCode = accountDataStorage_->SaveAccountInfo(osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Update os account info in database failed, localId =  %{public}d, errCode = %{public}d.",
            osAccountInfo.GetLocalId(), errCode);
    } else {
        ACCOUNT_LOGI("Update os account info in database for account %{public}d succeed!", osAccountInfo.GetLocalId());
    }
}

ErrCode OsAccountDatabaseOperator::GetOsAccountFromDatabase(const std::string& storeID,
                                                            const int id,
                                                            OsAccountInfo &osAccountInfo)
{
    if (!InnerInit()) {
        ACCOUNT_LOGE("InnerInit failed! storeID %{public}s id %{public}d!", storeID.c_str(), id);
        return ERR_ACCOUNT_COMMON_NOT_INIT_ERROR;
    }

    if (storeID.empty()) {
        return accountDataStorage_->GetAccountInfoById(std::to_string(id), osAccountInfo);
    }

    std::shared_ptr<AccountDataStorage> storagePtr = std::make_shared<OsAccountDataStorage>(
        APP_ID, storeID, Constants::SYNC_OS_ACCOUNT_DATABASE);
    if (storagePtr == nullptr) {
        ACCOUNT_LOGE("StoragePtr is nullptr, for other storeID %{public}s, id %{public}d.", storeID.c_str(), id);
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    return storagePtr->GetAccountInfoById(std::to_string(id), osAccountInfo);
}

ErrCode OsAccountDatabaseOperator::GetCreatedOsAccountNumFromDatabase(
    const std::string& storeID, int &createdOsAccountNum)
{
    CJsonUnique accountListJson = nullptr;
    ErrCode ret = GetAccountListFromStoreID(storeID, accountListJson);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Get account list from database failed, storeID %{public}s.", storeID.c_str());
        return ret;
    };
    GetDataByType<int>(accountListJson, Constants::COUNT_ACCOUNT_NUM, createdOsAccountNum);
    return ERR_OK;
}

void OsAccountDatabaseOperator::UpdateOsAccountIDListInDatabase(const CJsonUnique &accountListJson)
{
    if (!InnerInit()) {
        ACCOUNT_LOGE("InnerInit failed!");
        return;
    }
    std::string strValue = PackJsonToString(accountListJson);
    ErrCode errCode = accountDataStorage_->PutValueToKvStore(Constants::ACCOUNT_LIST, strValue);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Update os account id list to database failed! errCode %{public}d.", errCode);
        return;
    }
    ACCOUNT_LOGD("Update os account id list to database succeed.");
}

ErrCode OsAccountDatabaseOperator::GetSerialNumberFromDatabase(
    const std::string& storeID, int64_t &serialNumber)
{
    CJsonUnique accountListJson = nullptr;
    ErrCode ret = GetAccountListFromStoreID(storeID, accountListJson);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Get serial number from database failed! err %{public}d, storeID %{public}s.",
            ret, storeID.c_str());
        return ret;
    }
    GetDataByType<int64_t>(accountListJson, Constants::SERIAL_NUMBER_NUM, serialNumber);
    return ERR_OK;
}

ErrCode OsAccountDatabaseOperator::GetMaxAllowCreateIdFromDatabase(const std::string& storeID, int &id)
{
    CJsonUnique accountListJson = nullptr;
    ErrCode ret = GetAccountListFromStoreID(storeID, accountListJson);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Get max allow created id from database failed. err %{public}d, storeID %{public}s.",
            ret, storeID.c_str());
        return ret;
    }
    GetDataByType<int>(accountListJson, Constants::MAX_ALLOW_CREATE_ACCOUNT_ID, id);

    return ERR_OK;
}

ErrCode OsAccountDatabaseOperator::GetAccountListFromStoreID(
    const std::string& storeID, CJsonUnique &accountListJson)
{
    if (!InnerInit()) {
        ACCOUNT_LOGE("InnerInit failed! storeID %{public}s!", storeID.c_str());
        return ERR_ACCOUNT_COMMON_NOT_INIT_ERROR;
    }

    std::string accountList;
    ErrCode errCode = ERR_OK;
    if (storeID.empty()) {
        errCode = accountDataStorage_->GetValueFromKvStore(Constants::ACCOUNT_LIST, accountList);
    } else {
        std::shared_ptr<AccountDataStorage> storagePtr = std::make_shared<OsAccountDataStorage>(
            APP_ID, storeID, Constants::SYNC_OS_ACCOUNT_DATABASE);
        if (storagePtr == nullptr) {
            ACCOUNT_LOGE("StoragePtr is nullptr, for other storeID %{public}s.", storeID.c_str());
            return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
        }
        errCode = storagePtr->GetValueFromKvStore(Constants::ACCOUNT_LIST, accountList);
    }

    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Get account list info from database failed! storeID %{public}s.", storeID.c_str());
        return errCode;
    }
    accountListJson = CreateJsonFromString(accountList);
    if (accountListJson == nullptr) {
        ACCOUNT_LOGE("AccountListFile does not comply with the json format.");
        return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
    }
    return ERR_OK;
}
}  // namespace AccountSA
}  // namespace OHOS