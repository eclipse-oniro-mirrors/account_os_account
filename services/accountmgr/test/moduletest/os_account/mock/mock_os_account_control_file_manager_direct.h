/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef MOCK_OS_ACCOUNT_CONTROL_FILE_MANAGER_DIRECT_H
#define MOCK_OS_ACCOUNT_CONTROL_FILE_MANAGER_DIRECT_H

#include "ios_account_control.h"
#include "sub_profile_context.h"
#include "os_account_database_operator.h"
#include "os_account_file_operator.h"
#include "os_account_photo_operator.h"
#include "os_account_control_file_manager.h"
#include "account_error_no.h"

namespace OHOS {
namespace AccountSA {
class MockOsAccountControlFileManager : public IOsAccountControl {
public:
    MockOsAccountControlFileManager()
    {}
    virtual ~MockOsAccountControlFileManager()
    {}

    std::map<int32_t, OsAccountInfo> accountMap_;
    std::map<uint64_t, int32_t> defaultIds_;
    bool isMultiEnable_ = false;
    ErrCode insertResult_ = ERR_OK;

    void Init()
    {}
    ErrCode GetOsAccountConfig(OsAccountConfig &config)
    {
        return ERR_OK;
    }
    ErrCode GetOsAccountIdList(std::vector<int32_t> &idList)
    {
        return ERR_OK;
    }
    ErrCode GetOsAccountList(std::vector<OsAccountInfo> &osAccountList, bool needPhoto)
    {
        return ERR_OK;
    }
    ErrCode GetConstraintsByType(const OsAccountType type, std::vector<std::string> &constraints)
    {
        return ERR_OK;
    }
    ErrCode GetOsAccountInfoById(const int id, OsAccountInfo &osAccountInfo)
    {
        auto it = accountMap_.find(id);
        if (it != accountMap_.end()) {
            osAccountInfo = it->second;
            return ERR_OK;
        }
        return ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR;
    }
    ErrCode GetSerialNumber(int64_t &serialNumber)
    {
        return ERR_OK;
    }
    ErrCode GetAllowCreateId(int &id)
    {
        return ERR_OK;
    }
    ErrCode InsertOsAccount(OsAccountInfo &osAccountInfo)
    {
        return insertResult_;
    }
    ErrCode UpdateBaseOAConstraints(const std::string& idStr,
        const std::vector<std::string>& ConstraintStr, bool isAdd)
    {
        return ERR_OK;
    }
    ErrCode UpdateOsAccount(OsAccountInfo &osAccountInfo)
    {
        return ERR_OK;
    }
    ErrCode RemoveOAConstraintsInfo(const int32_t id)
    {
        return ERR_OK;
    }
    ErrCode DelOsAccount(const int id)
    {
        return ERR_OK;
    }
    ErrCode GetGlobalOAConstraintsList(std::vector<std::string> &constraintsList)
    {
        return ERR_OK;
    }
    ErrCode GetSpecificOAConstraintsList(const int32_t id, std::vector<std::string> &constraintsList)
    {
        return ERR_OK;
    }
    ErrCode IsFromGlobalOAConstraintsList(const int32_t id, const int32_t deviceOwnerId,
        const std::string constraint, std::vector<ConstraintSourceTypeInfo> &globalSourceList)
    {
        return ERR_OK;
    }
    ErrCode IsFromSpecificOAConstraintsList(const int32_t id, const int32_t deviceOwnerId,
        const std::string constraint, std::vector<ConstraintSourceTypeInfo> &specificSourceList)
    {
        return ERR_OK;
    }
    bool CheckConstraints(const std::vector<std::string> &constraints)
    {
        return true;
    }
    ErrCode GetPhotoById(const int id, std::string &photo)
    {
        return ERR_OK;
    }
    ErrCode GetIsMultiOsAccountEnable(bool &isMultiOsAccountEnable)
    {
        isMultiOsAccountEnable = isMultiEnable_;
        return ERR_OK;
    }
    ErrCode SetPhotoById(const int id, const std::string &photo)
    {
        return ERR_OK;
    }
    ErrCode UpdateDeviceOwnerId(const int32_t deviceOwnerId)
    {
        return ERR_OK;
    }
    ErrCode SetDefaultActivatedOsAccount(const int32_t id)
    {
        return ERR_OK;
    }
    ErrCode SetDefaultActivatedOsAccount(const uint64_t displayId, const int32_t id)
    {
        return ERR_OK;
    }
    ErrCode UpdateGlobalOAConstraints(const std::string& idStr,
        const std::vector<std::string>& ConstraintStr, bool isAdd)
    {
        return ERR_OK;
    }
    ErrCode SetNextLocalId(const int32_t &nextLocalId)
    {
        return ERR_OK;
    }
    ErrCode IsOsAccountExists(const int id, bool &isExists)
    {
        isExists = (accountMap_.find(id) != accountMap_.end());
        return ERR_OK;
    }
    ErrCode IsAllowedCreateAdmin(bool &isAllowedCreateAdmin)
    {
        return ERR_OK;
    }
    ErrCode GetCreatedOsAccountNumFromDatabase(const std::string& storeID,
        int &createdOsAccountNum)
    {
        return ERR_OK;
    }
    ErrCode GetSerialNumberFromDatabase(const std::string& storeID,
        int64_t &serialNumber)
    {
        return ERR_OK;
    }
    ErrCode GetMaxAllowCreateIdFromDatabase(const std::string& storeID, int &id)
    {
        return ERR_OK;
    }
    ErrCode GetOsAccountFromDatabase(const std::string& storeID,
        const int id, OsAccountInfo &osAccountInfo)
    {
        return ERR_OK;
    }
    ErrCode GetOsAccountListFromDatabase(const std::string& storeID,
        std::vector<OsAccountInfo> &osAccountList)
    {
        return ERR_OK;
    }
    ErrCode IsFromBaseOAConstraintsList(const int32_t id, const std::string constraint, bool &isExits)
    {
        isExits = false;
        return ERR_OK;
    }
    ErrCode UpdateSpecificOAConstraints(const std::string& idStr,
        const std::string& targetIdStr, const std::vector<std::string>& ConstraintStr, bool isAdd)
    {
        return ERR_OK;
    }
    ErrCode GetDeviceOwnerId(int32_t &deviceOwnerId)
    {
        return ERR_OK;
    }
    ErrCode GetDefaultActivatedOsAccount(int32_t &id)
    {
        return ERR_OK;
    }
    ErrCode GetDefaultActivatedOsAccount(const uint64_t displayId, int32_t &id)
    {
        return ERR_OK;
    }
    ErrCode GetAllDefaultActivatedOsAccounts(std::map<uint64_t, int32_t> &ids)
    {
        ids = defaultIds_;
        return ERR_OK;
    }
    ErrCode SetDomainBoundFlag(const int32_t localId, const bool isBoundCompleted,
        const DomainAccountInfo domainInfo = {})
    {
        return ERR_OK;
    }
    ErrCode GetDomainBoundFlag(const int32_t localId,
        bool &isBoundCompleted, DomainAccountInfo &domainInfo)
    {
        return ERR_OK;
    }
#ifdef ENABLE_MULTIPLE_OS_ACCOUNT_SUBSPACE
    ErrCode WriteSubProfileContext(int32_t localId, const SubProfileContext &data)
    {
        return ERR_OK;
    }
    ErrCode ReadSubProfileContext(int32_t localId, SubProfileContext &data)
    {
        return ERR_OK;
    }
    ErrCode DeleteSubProfileContextFile(int32_t localId)
    {
        return ERR_OK;
    }
#endif
};
} // namespace AccountSA
} // namespace OHOS

#endif // MOCK_OS_ACCOUNT_CONTROL_FILE_MANAGER_DIRECT_H
