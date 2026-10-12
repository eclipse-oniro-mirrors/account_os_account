/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "os_account_info_json_parser.h"
#include <string>
#include "json_utils.h"
#include "os_account_constants.h"
#include "os_account_info.h"

namespace OHOS {
namespace AccountSA {

#ifdef ENABLE_MULTIPLE_OS_ACCOUNT_SUBSPACE
namespace {
bool SerializeSubProfileToJson(CJson *jsonObject, const SubProfileContext &data)
{
    bool success = true;
    success &= AddIntToJson(jsonObject, NEXT_SUBPROFILE_ID, data.nextSubProfileId);
    success &= AddVectorIntToJson(jsonObject, SUBPROFILE_ID_LIST, data.subProfileIdList);
    success &= AddIntToJson(jsonObject, NEXT_SUBPROFILE_INDEX, data.nextSubProfileIndex);
    auto indexMapJson = CreateJson();
    for (const auto &[index, subspaceId] : data.subProfileIndexMap) {
        success &= AddIntToJson(indexMapJson, std::to_string(index), subspaceId);
    }
    success &= AddObjToJson(jsonObject, SUBPROFILE_INDEX_MAP, indexMapJson.get());
    return success;
}

SubProfileContext DeserializeSubProfileFromJson(CJson *jsonObject)
{
    SubProfileContext data;
    GetDataByType<int32_t>(jsonObject, NEXT_SUBPROFILE_ID, data.nextSubProfileId);
    GetDataByType<std::vector<int32_t>>(jsonObject, SUBPROFILE_ID_LIST, data.subProfileIdList);
    GetDataByType<int32_t>(jsonObject, NEXT_SUBPROFILE_INDEX, data.nextSubProfileIndex);
    CJson *indexMapJson = nullptr;
    GetDataByType<CJson *>(jsonObject, SUBPROFILE_INDEX_MAP, indexMapJson);
    if (indexMapJson != nullptr) {
        data.subProfileIndexMap.clear();
        auto tempMap = PackJsonToMapUint64Int32(indexMapJson);
        for (const auto &[key, val] : tempMap) {
            if (key <= static_cast<uint64_t>(INT32_MAX)) {
                data.subProfileIndexMap[static_cast<int32_t>(key)] = val;
            }
        }
    }
    return data;
}
}  // anonymous namespace
#endif  // ENABLE_MULTIPLE_OS_ACCOUNT_SUBSPACE

CJsonUnique ToJson(const OsAccountInfo &accountInfo)
{
    auto jsonObject = CreateJson();
    if (jsonObject == nullptr) {
        return nullptr;
    }

    bool success = true;
    success &= AddIntToJson(jsonObject, LOCAL_ID, accountInfo.localId_);
    success &= AddStringToJson(jsonObject, LOCAL_NAME, accountInfo.localName_);
    success &= AddStringToJson(jsonObject, SHORT_NAME, accountInfo.shortName_);
    success &= AddIntToJson(jsonObject, TYPE, static_cast<int32_t>(accountInfo.type_));
    success &= AddVectorStringToJson(jsonObject, CONSTRAINTS, accountInfo.constraints_);
    success &= AddBoolToJson(jsonObject, IS_OS_ACCOUNT_VERIFIED, accountInfo.isVerified_);
    success &= AddStringToJson(jsonObject, PHOTO, accountInfo.photo_);
    success &= AddInt64ToJson(jsonObject, CREATE_TIME, accountInfo.createTime_);
    success &= AddInt64ToJson(jsonObject, LAST_LOGGED_IN_TIME, accountInfo.lastLoginTime_);
    success &= AddInt64ToJson(jsonObject, SERIAL_NUMBER, accountInfo.serialNumber_);

    success &= AddBoolToJson(jsonObject, IS_ACTIVATED, accountInfo.isActivated_);
    success &= AddBoolToJson(jsonObject, IS_ACCOUNT_COMPLETED, accountInfo.isCreateCompleted_);
    success &= AddBoolToJson(jsonObject, TO_BE_REMOVED, accountInfo.toBeRemoved_);
    success &= AddUint64ToJson(jsonObject, CREDENTIAL_ID, accountInfo.credentialId_);
    success &= AddUint64ToJson(jsonObject, DISPLAY_ID, accountInfo.displayId_);
    success &= AddBoolToJson(jsonObject, IS_FOREGROUND, accountInfo.isForeground_);
    success &= AddBoolToJson(jsonObject, IS_LOGGED_IN, accountInfo.isLoggedIn_);
    success &= AddBoolToJson(jsonObject, IS_DATA_REMOVABLE, accountInfo.isDataRemovable_);
    success &= AddIntToJson(jsonObject, CREATOR_TYPE, accountInfo.creatorType_);
    success &= AddIntToJson(jsonObject, VERSION, accountInfo.version_);
    auto domainInfoObject = CreateJson();
    if (domainInfoObject == nullptr) {
        return nullptr;
    }
    success &= AddStringToJson(domainInfoObject, DOMAIN_NAME, accountInfo.domainInfo_.domain_);
    success &= AddStringToJson(domainInfoObject, DOMAIN_ACCOUNT_NAME, accountInfo.domainInfo_.accountName_);
    success &= AddStringToJson(domainInfoObject, DOMAIN_ACCOUNT_ID, accountInfo.domainInfo_.accountId_);
    success &= AddIntToJson(domainInfoObject, DOMAIN_ACCOUNT_STATUS,
        static_cast<int>(accountInfo.domainInfo_.status_));
    success &= AddStringToJson(domainInfoObject, DOMAIN_ACCOUNT_CONFIG, accountInfo.domainInfo_.serverConfigId_);
    success &= AddStringToJson(domainInfoObject, DOMAIN_ACCOUNT_ADDITION, accountInfo.domainInfo_.additionInfo_);
    success &= AddObjToJson(jsonObject, DOMAIN_INFO, domainInfoObject);
#ifdef ENABLE_MULTIPLE_OS_ACCOUNT_SUBSPACE
    success &= AddIntToJson(jsonObject, FOREGROUND_SUBPROFILE_ID, accountInfo.foregroundSubProfileId_);
#endif // ENABLE_MULTIPLE_OS_ACCOUNT_SUBSPACE
    if (!success) {
        return nullptr;
    }
    return jsonObject;
}

bool FromJson(cJSON *jsonObject, OsAccountInfo &accountInfo)
{
    if (jsonObject == nullptr) {
        return false;
    }
    bool parseSuccess = GetDataByType<int>(jsonObject, LOCAL_ID, accountInfo.localId_);
    GetDataByType<std::string>(jsonObject, LOCAL_NAME, accountInfo.localName_);
    GetDataByType<std::string>(jsonObject, SHORT_NAME, accountInfo.shortName_);
    GetDataByType<OsAccountType>(jsonObject, TYPE, accountInfo.type_);
    GetDataByType<std::vector<std::string>>(jsonObject, CONSTRAINTS, accountInfo.constraints_);
    GetDataByType<bool>(jsonObject, IS_OS_ACCOUNT_VERIFIED, accountInfo.isVerified_);
    GetDataByType<std::string>(jsonObject, PHOTO, accountInfo.photo_);
    GetDataByType<int64_t>(jsonObject, CREATE_TIME, accountInfo.createTime_);
    GetDataByType<int64_t>(jsonObject, LAST_LOGGED_IN_TIME, accountInfo.lastLoginTime_);
    GetDataByType<int64_t>(jsonObject, SERIAL_NUMBER, accountInfo.serialNumber_);
    GetDataByType<bool>(jsonObject, IS_ACTIVATED, accountInfo.isActivated_);
    parseSuccess =
        parseSuccess && GetDataByType<bool>(jsonObject, IS_ACCOUNT_COMPLETED, accountInfo.isCreateCompleted_);
    GetDataByType<bool>(jsonObject, TO_BE_REMOVED, accountInfo.toBeRemoved_);
    GetDataByType<uint64_t>(jsonObject, CREDENTIAL_ID, accountInfo.credentialId_);
    GetDataByType<uint64_t>(jsonObject, DISPLAY_ID, accountInfo.displayId_);
    GetDataByType<bool>(jsonObject, IS_FOREGROUND, accountInfo.isForeground_);
    GetDataByType<bool>(jsonObject, IS_LOGGED_IN, accountInfo.isLoggedIn_);
    GetDataByType<bool>(jsonObject, IS_DATA_REMOVABLE, accountInfo.isDataRemovable_);
    GetDataByType<int32_t>(jsonObject, CREATOR_TYPE, accountInfo.creatorType_);
    GetDataByType<int32_t>(jsonObject, VERSION, accountInfo.version_);
    CJson *typeJson = nullptr;
    GetDataByType<CJson *>(jsonObject, DOMAIN_INFO, typeJson);
    if (typeJson != nullptr) {
        GetDataByType<std::string>(typeJson, DOMAIN_NAME, accountInfo.domainInfo_.domain_);
        GetDataByType<std::string>(typeJson, DOMAIN_ACCOUNT_NAME, accountInfo.domainInfo_.accountName_);
        GetDataByType<std::string>(typeJson, DOMAIN_ACCOUNT_ID, accountInfo.domainInfo_.accountId_);
        GetDataByType<DomainAccountStatus>(typeJson, DOMAIN_ACCOUNT_STATUS, accountInfo.domainInfo_.status_);
        GetDataByType<std::string>(typeJson, DOMAIN_ACCOUNT_CONFIG, accountInfo.domainInfo_.serverConfigId_);
        GetDataByType<std::string>(typeJson, DOMAIN_ACCOUNT_ADDITION, accountInfo.domainInfo_.additionInfo_);
    }
#ifdef ENABLE_MULTIPLE_OS_ACCOUNT_SUBSPACE
    GetDataByType<int32_t>(jsonObject, FOREGROUND_SUBPROFILE_ID, accountInfo.foregroundSubProfileId_);
#endif // ENABLE_MULTIPLE_OS_ACCOUNT_SUBSPACE
    return parseSuccess;
}

CJsonUnique ToJson(const DomainAccountInfo &domainInfo)
{
    auto domainInfoObject = CreateJson();
    if (domainInfoObject == nullptr) {
        return nullptr;
    }
    bool success = true;
    success &= AddStringToJson(domainInfoObject, DOMAIN_NAME, domainInfo.domain_);
    success &= AddStringToJson(domainInfoObject, DOMAIN_ACCOUNT_NAME, domainInfo.accountName_);
    success &= AddStringToJson(domainInfoObject, DOMAIN_ACCOUNT_ID, domainInfo.accountId_);
    success &= AddIntToJson(domainInfoObject, DOMAIN_ACCOUNT_STATUS, static_cast<int>(domainInfo.status_));
    success &= AddStringToJson(domainInfoObject, DOMAIN_ACCOUNT_CONFIG, domainInfo.serverConfigId_);
    success &= AddStringToJson(domainInfoObject, DOMAIN_ACCOUNT_ADDITION, domainInfo.additionInfo_);
    if (!success) {
        return nullptr;
    }
    return domainInfoObject;
}

bool FromJson(cJSON *jsonObject, DomainAccountInfo &domainInfo)
{
    if (jsonObject == nullptr) {
        return false;
    }
    bool result = true;
    result &= GetDataByType<std::string>(jsonObject, DOMAIN_NAME, domainInfo.domain_);
    result &= GetDataByType<std::string>(jsonObject, DOMAIN_ACCOUNT_NAME, domainInfo.accountName_);
    result &= GetDataByType<std::string>(jsonObject, DOMAIN_ACCOUNT_ID, domainInfo.accountId_);
    result &= GetDataByType<DomainAccountStatus>(jsonObject, DOMAIN_ACCOUNT_STATUS, domainInfo.status_);
    result &= GetDataByType<std::string>(jsonObject, DOMAIN_ACCOUNT_CONFIG, domainInfo.serverConfigId_);
    result &= GetDataByType<std::string>(jsonObject, DOMAIN_ACCOUNT_ADDITION, domainInfo.additionInfo_);
    return result;
}

#ifdef ENABLE_MULTIPLE_OS_ACCOUNT_SUBSPACE
CJsonUnique ToJsonSubProfile(const SubProfileContext &data)
{
    auto jsonObject = CreateJson();
    if (jsonObject == nullptr) {
        return nullptr;
    }
    if (!SerializeSubProfileToJson(jsonObject.get(), data)) {
        return nullptr;
    }
    return jsonObject;
}

bool FromJsonSubProfile(cJSON *jsonObject, SubProfileContext &data)
{
    if (jsonObject == nullptr) {
        return false;
    }
    data = DeserializeSubProfileFromJson(jsonObject);
    return true;
}
#endif // ENABLE_MULTIPLE_OS_ACCOUNT_SUBSPACE

} // namespace AccountSA
} // namespace OHOS