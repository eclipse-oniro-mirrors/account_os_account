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
#include "account_data_storage.h"
#include <memory>
#include <unistd.h>
#include "account_log_wrapper.h"
#include "account_hisysevent_adapter.h"

namespace OHOS {
namespace AccountSA {
const int32_t MAX_TIMES = 10;
const int32_t SLEEP_INTERVAL = 100 * 1000;
constexpr char KV_STORE_EL1_BASE_DIR[] = "/data/service/el1/public/database/";

#ifndef SQLITE_DLCLOSE_ENABLE
AccountDataStorage::AccountDataStorage(const std::string &appId, const std::string &storeId,
    const AccountDataStorageOptions &options)
{
    ACCOUNT_LOGI("Constructed");
    appId_.appId = appId;
    storeId_.storeId = storeId;
    options_ = options;
    if (options_.area == DistributedKv::EL1) {
        baseDir_ = KV_STORE_EL1_BASE_DIR + appId;
    } else {
        baseDir_ = options.baseDir;
    }
}
#else
AccountDataStorage::AccountDataStorage(const std::string &appId, const std::string &storeId,
    const DbAdapterOptions &options)
{
    appId_ = appId;
    storeId_ = storeId;
    options_ = options;
    dataManager_ = DatabaseAdapterLoader::GetInstance().GetDataManager();
    if (options_.area == DbAdapterArea::EL1) {
        baseDir_ = KV_STORE_EL1_BASE_DIR + appId;
    } else {
        baseDir_ = options.baseDir;
    }
    options_.baseDir = baseDir_;
}
#endif // SQLITE_DLCLOSE_ENABLE

AccountDataStorage::~AccountDataStorage()
{
    ACCOUNT_LOGI("Destroyed");
    if (kvStorePtr_ != nullptr) {
    #ifndef SQLITE_DLCLOSE_ENABLE
        dataManager_.CloseKvStore(appId_, kvStorePtr_);
    #else
        dataManager_->CloseKvStore(appId_, kvStorePtr_);
    #endif // SQLITE_DLCLOSE_ENABLE
    }
}

#ifndef SQLITE_DLCLOSE_ENABLE
void AccountDataStorage::TryTwice(const std::function<DistributedKv::Status()> &func) const
{
    OHOS::DistributedKv::Status status = func();
    if (status == OHOS::DistributedKv::Status::IPC_ERROR) {
        status = func();
        ACCOUNT_LOGE("distribute database ipc error and try again, status = %{public}d", status);
    }
}
#else
void AccountDataStorage::TryTwice(const std::function<DbAdapterStatus()> &func) const
{
    DbAdapterStatus status = func();
    if (status == DbAdapterStatus::IPC_ERROR) {
        status = func();
        ACCOUNT_LOGE("distribute database ipc error and try again, status = %{public}d", status);
    }
}
#endif // SQLITE_DLCLOSE_ENABLE

#ifndef SQLITE_DLCLOSE_ENABLE
OHOS::DistributedKv::Status AccountDataStorage::GetKvStore()
{
    OHOS::DistributedKv::Options options = {
        .createIfMissing = true,
        .encrypt = options_.encrypt,
        .autoSync = options_.autoSync,
        .syncable = options_.autoSync,
        .securityLevel = options_.securityLevel,
        .area = options_.area,
        .kvStoreType = OHOS::DistributedKv::KvStoreType::SINGLE_VERSION,
        .baseDir = baseDir_,
    };

    OHOS::DistributedKv::Status status = dataManager_.GetSingleKvStore(options, appId_, storeId_, kvStorePtr_);
    if (status != OHOS::DistributedKv::Status::SUCCESS || kvStorePtr_ == nullptr) {
#else
DbAdapterStatus AccountDataStorage::GetKvStore()
{
    DbAdapterStatus status = dataManager_->GetSingleKvStore(options_, appId_, storeId_, kvStorePtr_);
    if (status != DbAdapterStatus::SUCCESS || kvStorePtr_ == nullptr) {
#endif // SQLITE_DLCLOSE_ENABLE
        ACCOUNT_LOGE("GetSingleKvStore failed! status %{public}d, kvStorePtr_ is nullptr", status);
        return status;
    }
    return status;
}

bool AccountDataStorage::CheckKvStore()
{
    std::lock_guard<std::mutex> lock(kvStorePtrMutex_);

    if (kvStorePtr_ != nullptr) {
        return true;
    }
    int32_t tryTimes = MAX_TIMES;
#ifndef SQLITE_DLCLOSE_ENABLE
    OHOS::DistributedKv::Status status = OHOS::DistributedKv::Status::SUCCESS;
#else
    DbAdapterStatus status = DbAdapterStatus::SUCCESS;
#endif // SQLITE_DLCLOSE_ENABLE
    while (tryTimes > 0) {
        status = GetKvStore();
    #ifndef SQLITE_DLCLOSE_ENABLE
        if (status == OHOS::DistributedKv::Status::SUCCESS && kvStorePtr_ != nullptr) {
            break;
        }
    #else
        if (status == DbAdapterStatus::SUCCESS && kvStorePtr_ != nullptr) {
            break;
        }
    #endif // SQLITE_DLCLOSE_ENABLE

        usleep(SLEEP_INTERVAL);
        tryTimes--;
    }

    if (kvStorePtr_ == nullptr) {
        return false;
    }

    return true;
}

ErrCode AccountDataStorage::LoadAllData(std::map<std::string, std::shared_ptr<IAccountInfo>> &infos)
{
    if (!CheckKvStore()) {
        ACCOUNT_LOGE("kvStore is nullptr");
        return OHOS::ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }
#ifndef SQLITE_DLCLOSE_ENABLE
    OHOS::DistributedKv::Status status = DistributedKv::Status::SUCCESS;
    std::vector<OHOS::DistributedKv::Entry> allEntries;
    TryTwice([this, &status, &allEntries] {
        status = GetEntries("", allEntries);
        return status;
    });

    if (status != OHOS::DistributedKv::Status::SUCCESS) {
#else
    DbAdapterStatus status = DbAdapterStatus::SUCCESS;
    std::vector<DbAdapterEntry> allEntries;
    TryTwice([this, &status, &allEntries] {
        status = GetEntries("", allEntries);
        return status;
    });

    if (status != DbAdapterStatus::SUCCESS) {
#endif // SQLITE_DLCLOSE_ENABLE
        ACCOUNT_LOGE("get entries error: %{public}d", status);
        return OHOS::ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }
    infos.clear();
    SaveEntries(allEntries, infos);
    return ERR_OK;
}

ErrCode AccountDataStorage::AddAccountInfo(const IAccountInfo &iAccountInfo)
{
    if (IsKeyExists(iAccountInfo.GetPrimeKey())) {
        ACCOUNT_LOGE("the key already exists.");
        return ERR_OSACCOUNT_SERVICE_DATA_STORAGE_KEY_EXISTED_ERROR;
    }

    std::string accountInfoStr = iAccountInfo.ToString();
    if (accountInfoStr.empty()) {
        ACCOUNT_LOGE("account info str is empty!");
        return ERR_OSACCOUNT_SERVICE_ACCOUNT_INFO_EMPTY_ERROR;
    }
    return PutValueToKvStore(iAccountInfo.GetPrimeKey(), accountInfoStr);
}

ErrCode AccountDataStorage::SaveAccountInfo(const IAccountInfo &iAccountInfo)
{
    if (!IsKeyExists(iAccountInfo.GetPrimeKey())) {
        ACCOUNT_LOGE("the key does not exist");
        return ERR_OSACCOUNT_SERVICE_DATA_STORAGE_KEY_NOT_EXISTS_ERROR;
    }

    std::string accountInfoStr = iAccountInfo.ToString();
    if (accountInfoStr.empty()) {
        ACCOUNT_LOGE("account info str is empty!");
        return ERR_OSACCOUNT_SERVICE_ACCOUNT_INFO_EMPTY_ERROR;
    }
    return PutValueToKvStore(iAccountInfo.GetPrimeKey(), accountInfoStr);
}

#ifndef SQLITE_DLCLOSE_ENABLE
ErrCode AccountDataStorage::RemoveValueFromKvStore(const std::string &keyStr)
{
    if (!CheckKvStore()) {
        ACCOUNT_LOGE("kvStore is nullptr");
        return ERR_ACCOUNT_COMMON_CHECK_KVSTORE_ERROR;
    }

    OHOS::DistributedKv::Key key(keyStr);
    OHOS::DistributedKv::Status status;
    OHOS::DistributedKv::Value value;
    {
        std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
        // check exist
        status = kvStorePtr_->Get(key, value);
        if (status == OHOS::DistributedKv::Status::IPC_ERROR) {
            ACCOUNT_LOGE("kvstore ipc error and try again, status = %{public}d", status);
            status = kvStorePtr_->Get(key, value);
        }
        if (status != OHOS::DistributedKv::Status::SUCCESS) {
            ACCOUNT_LOGI("key does not exist in kvStore.");
            return ERR_OK;
        }

        // delete
        status = kvStorePtr_->Delete(key);
        if (status == OHOS::DistributedKv::Status::IPC_ERROR) {
            status = kvStorePtr_->Delete(key);
            ACCOUNT_LOGE("kvstore ipc error and try to call again, status = %{public}d", status);
        }
    }

    if (status != OHOS::DistributedKv::Status::SUCCESS) {
        ACCOUNT_LOGE("delete key from kvstore failed, status %{public}d.", status);
        return ERR_ACCOUNT_COMMON_DELETE_KEY_FROM_KVSTORE_ERROR;
    }

    ACCOUNT_LOGD("delete key from kvStore succeed!");
    return ERR_OK;
}
#else
ErrCode AccountDataStorage::RemoveValueFromKvStore(const std::string &keyStr)
{
    if (!CheckKvStore()) {
        ACCOUNT_LOGE("kvStore is nullptr");
        return ERR_ACCOUNT_COMMON_CHECK_KVSTORE_ERROR;
    }

    DbAdapterStatus status;
    std::string value;
    {
        std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
        // check exist
        status = kvStorePtr_->Get(keyStr, value);
        if (status == DbAdapterStatus::IPC_ERROR) {
            ACCOUNT_LOGE("kvstore ipc error and try again, status = %{public}d", status);
            status = kvStorePtr_->Get(keyStr, value);
        }
        if (status != DbAdapterStatus::SUCCESS) {
            ACCOUNT_LOGI("key does not exist in kvStore.");
            return ERR_OK;
        }

        // delete
        status = kvStorePtr_->Delete(keyStr);
        if (status == DbAdapterStatus::IPC_ERROR) {
            status = kvStorePtr_->Delete(keyStr);
            ACCOUNT_LOGE("kvstore ipc error and try to call again, status = %{public}d", status);
        }
    }

    if (status != DbAdapterStatus::SUCCESS) {
        ACCOUNT_LOGE("delete key from kvstore failed, status %{public}d.", status);
        return ERR_ACCOUNT_COMMON_DELETE_KEY_FROM_KVSTORE_ERROR;
    }

    ACCOUNT_LOGD("delete key from kvStore succeed!");
    return ERR_OK;
}
#endif // SQLITE_DLCLOSE_ENABLE

#ifndef SQLITE_DLCLOSE_ENABLE
OHOS::DistributedKv::Status AccountDataStorage::GetEntries(
    std::string subId, std::vector<OHOS::DistributedKv::Entry> &allEntries) const
{
    OHOS::DistributedKv::Key allEntryKeyPrefix(subId);
    std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
    OHOS::DistributedKv::Status status = kvStorePtr_->GetEntries(allEntryKeyPrefix, allEntries);

    return status;
}
#else
DbAdapterStatus AccountDataStorage::GetEntries(
    std::string subId, std::vector<DbAdapterEntry> &allEntries) const
{
    std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
    DbAdapterStatus status = kvStorePtr_->GetEntries(subId, allEntries);

    return status;
}
#endif // SQLITE_DLCLOSE_ENABLE

ErrCode AccountDataStorage::Close()
{
    std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
#ifndef SQLITE_DLCLOSE_ENABLE
    ErrCode errCode = dataManager_.CloseKvStore(appId_, kvStorePtr_);
#else
    ErrCode errCode = dataManager_->CloseKvStore(appId_, kvStorePtr_);
#endif // SQLITE_DLCLOSE_ENABLE
    kvStorePtr_ = nullptr;
    return errCode;
}

ErrCode AccountDataStorage::DeleteKvStore()
{
#ifndef SQLITE_DLCLOSE_ENABLE
    if (!CheckKvStore()) {
        ACCOUNT_LOGE("kvStore is nullptr");
        return OHOS::ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }

    OHOS::DistributedKv::Status status;
    {
        std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
        dataManager_.CloseKvStore(this->appId_, this->storeId_);
        status = dataManager_.DeleteKvStore(this->appId_, this->storeId_, baseDir_);
    }
    if (status != OHOS::DistributedKv::Status::SUCCESS) {
        ACCOUNT_LOGE("error, status = %{public}d", status);
        return OHOS::ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }
#else
    ACCOUNT_LOGI("DeleteKvStore not enabled.");
#endif // SQLITE_DLCLOSE_ENABLE
    return ERR_OK;
}

ErrCode AccountDataStorage::GetAccountInfoById(const std::string id, IAccountInfo &iAccountInfo)
{
    std::string valueStr;
    ErrCode ret = GetValueFromKvStore(id, valueStr);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("get value from kvstore failed! id %{public}s.", id.c_str());
        return ret;
    }

    nlohmann::json jsonObject = nlohmann::json::parse(valueStr, nullptr, false);
    if (jsonObject.is_discarded() || !jsonObject.is_structured()) {  // check format
        ACCOUNT_LOGE("bad format of value from kvstore! id %{public}s.", id.c_str());
        return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
    }
    iAccountInfo.FromJson(jsonObject);
    return ERR_OK;
}

ErrCode AccountDataStorage::LoadDataByLocalFuzzyQuery(
    std::string subId, std::map<std::string, std::shared_ptr<IAccountInfo>> &infos)
{
    if (!CheckKvStore()) {
        ACCOUNT_LOGE("kvStore is nullptr");
        return OHOS::ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }
#ifndef SQLITE_DLCLOSE_ENABLE
    OHOS::DistributedKv::Status status = OHOS::DistributedKv::Status::SUCCESS;
    std::vector<OHOS::DistributedKv::Entry> allEntries;
#else
    DbAdapterStatus status = DbAdapterStatus::SUCCESS;
    std::vector<DbAdapterEntry> allEntries;
#endif // SQLITE_DLCLOSE_ENABLE
    TryTwice([this, &status, &allEntries, subId] {
        status = GetEntries(subId, allEntries);
        return status;
    });
#ifndef SQLITE_DLCLOSE_ENABLE
    if (status != OHOS::DistributedKv::Status::SUCCESS) {
#else
    if (status != DbAdapterStatus::SUCCESS) {
#endif // SQLITE_DLCLOSE_ENABLE
        ACCOUNT_LOGE("get entries error: %{public}d", status);
        return OHOS::ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }
    infos.clear();
    SaveEntries(allEntries, infos);
    return ERR_OK;
}

ErrCode AccountDataStorage::PutValueToKvStore(const std::string &keyStr, const std::string &valueStr)
{
    if (!CheckKvStore()) {
        ACCOUNT_LOGE("kvStore is nullptr");
        return ERR_ACCOUNT_COMMON_CHECK_KVSTORE_ERROR;
    }
#ifndef SQLITE_DLCLOSE_ENABLE
    OHOS::DistributedKv::Key key(keyStr);
    OHOS::DistributedKv::Value value(valueStr);
    OHOS::DistributedKv::Status status;

    {
        std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
        status = kvStorePtr_->Put(key, value);
        if (status == OHOS::DistributedKv::Status::IPC_ERROR) {
            status = kvStorePtr_->Put(key, value);
        }
    }

    if (status != OHOS::DistributedKv::Status::SUCCESS) {
#else
    DbAdapterStatus status = DbAdapterStatus::SUCCESS;
    {
        std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
        status = kvStorePtr_->Put(keyStr, valueStr);
        if (status == DbAdapterStatus::IPC_ERROR) {
            status = kvStorePtr_->Put(keyStr, valueStr);
        }
    }

    if (status != DbAdapterStatus::SUCCESS) {
#endif // SQLITE_DLCLOSE_ENABLE
        ACCOUNT_LOGE("put value to kvStore error, status = %{public}d", status);
        return OHOS::ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }

    return ERR_OK;
}

ErrCode AccountDataStorage::GetValueFromKvStore(const std::string &keyStr, std::string &valueStr)
{
    if (!CheckKvStore()) {
        ACCOUNT_LOGE("kvStore is nullptr");
        return ERR_ACCOUNT_COMMON_CHECK_KVSTORE_ERROR;
    }
#ifndef SQLITE_DLCLOSE_ENABLE
    OHOS::DistributedKv::Key key(keyStr);
    OHOS::DistributedKv::Value value;
    OHOS::DistributedKv::Status status;

    {
        std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
        status = kvStorePtr_->Get(key, value);
        if (status == OHOS::DistributedKv::Status::IPC_ERROR) {
            ACCOUNT_LOGE("kvstore ipc error and try again, status = %{public}d", status);
            status = kvStorePtr_->Get(key, value);
        }
    }

    if (status != OHOS::DistributedKv::Status::SUCCESS) {
#else
    DbAdapterStatus status;

    {
        std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
        status = kvStorePtr_->Get(keyStr, valueStr);
        if (status == DbAdapterStatus::IPC_ERROR) {
            ACCOUNT_LOGE("kvstore ipc error and try again, status = %{public}d", status);
            status = kvStorePtr_->Get(keyStr, valueStr);
        }
    }

    if (status != DbAdapterStatus::SUCCESS) {
#endif // SQLITE_DLCLOSE_ENABLE
        ACCOUNT_LOGE("get value from kvstore error, status %{public}d.", status);
        return ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }
#ifndef SQLITE_DLCLOSE_ENABLE
    valueStr = value.ToString();
#endif
    return ERR_OK;
}

bool AccountDataStorage::IsKeyExists(const std::string keyStr)
{
    std::string valueStr;
    if (GetValueFromKvStore(keyStr, valueStr) != ERR_OK) {
        return false;
    }
    return true;
}

ErrCode AccountDataStorage::MoveData(const std::shared_ptr<AccountDataStorage> &ptr)
{
#ifndef SQLITE_DLCLOSE_ENABLE
    if (ptr == nullptr || !ptr->CheckKvStore() || !CheckKvStore()) {
        ACCOUNT_LOGE("AccountDataStorage is nullptr");
        return ERR_ACCOUNT_COMMON_CHECK_KVSTORE_ERROR;
    }
    std::vector<OHOS::DistributedKv::Entry> entries;
    OHOS::DistributedKv::Status status = ptr->GetEntries("", entries);
    if (status != OHOS::DistributedKv::Status::SUCCESS) {
        ACCOUNT_LOGE("GetEntries failed, result=%{public}u", status);
        return ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }
    std::lock_guard<std::mutex> lock(kvStorePtrMutex_);
    status = kvStorePtr_->PutBatch(entries);
    if (status != OHOS::DistributedKv::Status::SUCCESS) {
        ACCOUNT_LOGE("PutBatch failed, result=%{public}u", status);
        return ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR;
    }
#else
    ACCOUNT_LOGI("MoveData not enabled.");
#endif
    return ERR_OK;
}
}  // namespace AccountSA
}  // namespace OHOS
