/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "account_backup_restore_manager.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include "account_log_wrapper.h"
#include "account_hisysevent_adapter.h"
#include "iinner_os_account_manager.h"
#include "json_utils.h"
#include "os_account_constants.h"
#include "os_account_control_file_manager.h"
#include "os_account_info_json_parser.h"
#include "os_account_manager_service.h"
#include "unique_fd.h"
#include "common_event_support.h"
#ifdef SUPPORT_DOMAIN_ACCOUNTS
#include "domain_account_common.h"
#include "inner_domain_account_manager.h"
#endif

namespace OHOS {
namespace AccountSA {
namespace {
constexpr const char *JSON_KEY_TYPE = "type";
constexpr const char *JSON_KEY_DETAIL = "detail";
constexpr const char *JSON_VALUE_USERID = "userId";
constexpr const char *JSON_KEY_RESULT_INFO = "resultInfo";
constexpr const char *JSON_KEY_ERROR_CODE = "errorCode";
constexpr const char *JSON_KEY_ERROR_INFO = "errorInfo";
constexpr const char *JSON_VALUE_ERROR_INFO = "ErrorInfo";
constexpr const char *BACKUP_TEMP_FILE = "backup_tmp.json";
}

AccountBackupRestoreMgr &AccountBackupRestoreMgr::GetInstance()
{
    static AccountBackupRestoreMgr instance;
    return instance;
}

ErrCode AccountBackupRestoreMgr::ParseLocalIdJson(const std::string &jsonStr, int32_t &localId)
{
    CJsonUnique root = CreateJsonFromString(jsonStr);
    if (root == nullptr || !cJSON_IsArray(root.get())) {
        ACCOUNT_LOGE("ParseLocalIdJson: invalid json");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    int nSize = cJSON_GetArraySize(root.get());
    for (int i = 0; i < nSize; i++) {
        cJSON *item = cJSON_GetArrayItem(root.get(), i);
        if (item == nullptr || !cJSON_IsObject(item)) {
            continue;
        }
        cJSON *typeItem = cJSON_GetObjectItem(item, JSON_KEY_TYPE);
        if (typeItem == nullptr || !cJSON_IsString(typeItem)) {
            continue;
        }
        if (strcmp(typeItem->valuestring, JSON_VALUE_USERID) != 0) {
            continue;
        }
        cJSON *detailItem = cJSON_GetObjectItem(item, JSON_KEY_DETAIL);
        if (detailItem == nullptr || !cJSON_IsString(detailItem)) {
            continue;
        }
        std::istringstream iss(detailItem->valuestring);
        int32_t parsedId = 0;
        if (!(iss >> parsedId) || !iss.eof()) {
            ACCOUNT_LOGE("ParseLocalIdJson: detail is not a valid number, value=%{public}s",
                detailItem->valuestring);
            return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
        }
        if (parsedId < Constants::START_USER_ID || parsedId > Constants::MAX_CREATABLE_USER_ID) {
            ACCOUNT_LOGE("ParseLocalIdJson: localId out of range, value=%{public}d", parsedId);
            return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
        }
        localId = parsedId;
        return ERR_OK;
    }
    ACCOUNT_LOGE("ParseLocalIdJson: userId not found");
    return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
}

std::string AccountBackupRestoreMgr::SetBackupReplyCode(int32_t replyCode)
{
    CJsonUnique root = CreateJson();
    if (root == nullptr) {
        return "";
    }
    CJsonUnique resultInfo = CreateJsonArray();
    if (resultInfo == nullptr) {
        return "";
    }
    CJsonUnique errorInfo = CreateJson();
    if (errorInfo == nullptr) {
        return "";
    }
    cJSON_AddStringToObject(errorInfo.get(), JSON_KEY_TYPE, JSON_VALUE_ERROR_INFO);
    cJSON_AddStringToObject(errorInfo.get(), JSON_KEY_ERROR_CODE, std::to_string(replyCode).c_str());
    cJSON_AddStringToObject(errorInfo.get(), JSON_KEY_ERROR_INFO, "");
    cJSON_AddItemToArray(resultInfo.get(), errorInfo.release());
    cJSON_AddItemToObject(root.get(), JSON_KEY_RESULT_INFO, resultInfo.release());
    return PackJsonToString(root);
}

int32_t AccountBackupRestoreMgr::ValidateBackupName(int32_t localId, const OsAccountInfo &backupInfo)
{
    if (backupInfo.GetLocalName().empty() ||
        backupInfo.GetLocalName().size() >= Constants::LOCAL_NAME_MAX_SIZE) {
        ACCOUNT_LOGE("ValidateBackupName: localName format invalid, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_INVALID_PARAMETER, "localName format invalid");
        return ERR_JS_INVALID_PARAMETER;
    }
    if (OsAccountManagerService::ValidateShortName(backupInfo.GetShortName()) != ERR_OK) {
        ACCOUNT_LOGE("ValidateBackupName: shortName format invalid, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_INVALID_PARAMETER, "shortName format invalid");
        return ERR_JS_INVALID_PARAMETER;
    }
    ErrCode errCode = IInnerOsAccountManager::GetInstance().ValidateOsAccount(backupInfo);
    if (errCode == ERR_ACCOUNT_COMMON_NAME_HAD_EXISTED) {
        ACCOUNT_LOGE("ValidateBackupName: localName already exists, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_NAME_HAD_EXISTED, "localName already exists");
        return ERR_JS_ACCOUNT_ALREADY_EXIST;
    }
    if (errCode == ERR_ACCOUNT_COMMON_SHORT_NAME_HAD_EXISTED) {
        ACCOUNT_LOGE("ValidateBackupName: shortName already exists, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_SHORT_NAME_HAD_EXISTED, "shortName already exists");
        return ERR_JS_ACCOUNT_SHORT_NAME_ALREADY_EXIST;
    }
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("ValidateBackupName: validate failed, errCode=%{public}d, localId=%{public}d", errCode, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            errCode, "validate os account failed");
        return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
    }
    return ERR_OK;
}

int32_t AccountBackupRestoreMgr::ValidateDomainBinding(int32_t localId,
    const OsAccountInfo &backupInfo, const OsAccountInfo &currentInfo)
{
    DomainAccountInfo backupDomainInfo;
    backupInfo.GetDomainInfo(backupDomainInfo);
    DomainAccountInfo curDomainInfo;
    currentInfo.GetDomainInfo(curDomainInfo);
    // backupDomainInfo comes from plugin sync (CheckDomainAccountConsistency).
    // Only reject when: current account already has a domain binding, the plugin is
    // available, and the plugin's domain differs from the current binding (including
    // plugin returning empty, which would mean unbinding — not allowed).
    // When the plugin is unavailable (ShouldSkipDomainCheck), domain data is left
    // untouched for the JS domain plugin to handle.
    if (!curDomainInfo.domain_.empty() && !ShouldSkipDomainCheck() &&
        !IInnerOsAccountManager::GetInstance().IsSameAccount(backupDomainInfo, curDomainInfo)) {
        ACCOUNT_LOGE("ValidateDomainBinding: domain account binding conflict, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_OSACCOUNT_SERVICE_INNER_OS_ACCOUNT_ALREADY_BOUND, "domain account binding conflict");
        return ERR_JS_OS_ACCOUNT_ALREADY_BOUND;
    }
    std::vector<OsAccountInfo> osAccountList;
    ErrCode errCode = IInnerOsAccountManager::GetInstance().QueryAllCreatedOsAccounts(osAccountList);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("query account list failed, errCode=%{public}d, localId=%{public}d", errCode, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            errCode, "query account list failed");
        return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
    }
    DomainAccountInfo otherDomainInfo;
    for (const auto &info : osAccountList) {
        if (info.GetLocalId() == localId) {
            continue;
        }
        info.GetDomainInfo(otherDomainInfo);
        if (!backupDomainInfo.domain_.empty() && !otherDomainInfo.domain_.empty() &&
            IInnerOsAccountManager::GetInstance().IsSameAccount(backupDomainInfo, otherDomainInfo)) {
            ACCOUNT_LOGE("ValidateDomainBinding: domain already bound, localId=%{public}d", localId);
            ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
                ERR_OSACCOUNT_SERVICE_INNER_DOMAIN_ACCOUNT_ALREADY_BOUND,
                "domain account already bound by other account");
            return ERR_JS_DOMAIN_ACCOUNT_ALREADY_BOUND;
        }
    }
    return ERR_OK;
}

int32_t AccountBackupRestoreMgr::CheckRestorePreconditions(int32_t localId,
    const OsAccountInfo &backupInfo, const OsAccountInfo &currentInfo)
{
    int32_t ret = ValidateBackupName(localId, backupInfo);
    if (ret != ERR_OK) {
        return ret;
    }
    ret = ValidateDomainBinding(localId, backupInfo, currentInfo);
    if (ret != ERR_OK) {
        return ret;
    }
    if (backupInfo.GetType() != currentInfo.GetType()) {
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_INVALID_PARAMETER,
            "type mismatch: backup and current type differ, type will not be updated");
        ACCOUNT_LOGW("Type mismatch, backup=%{public}d, current=%{public}d, "
            "type will not be updated, localId=%{public}d",
            static_cast<int32_t>(backupInfo.GetType()),
            static_cast<int32_t>(currentInfo.GetType()), localId);
    }
    return ERR_OK;
}

int32_t AccountBackupRestoreMgr::OnBackup(MessageParcel &data, MessageParcel &reply)
{
    std::string localIdJson = data.ReadString();
    if (localIdJson.empty()) {
        ACCOUNT_LOGE("OnBackup: ReadString localIdJson failed");
        ReportOsAccountOperationFail(0, Constants::OPERATION_BACKUP,
            ERR_ACCOUNT_COMMON_READ_PARCEL_ERROR, "ReadString localIdJson failed");
        return WriteBackupReply(0, reply, ERR_JS_INVALID_PARAMETER);
    }
    int32_t localId = 0;
    if (ParseLocalIdJson(localIdJson, localId) != ERR_OK) {
        return WriteBackupReply(localId, reply, ERR_JS_INVALID_PARAMETER);
    }
    ErrCode recoverRet = IInnerOsAccountManager::GetInstance().CheckAndRecoverRestore();
    if (recoverRet != ERR_OK) {
        ACCOUNT_LOGE("OnBackup: CheckAndRecoverRestore failed, errCode=%{public}d", recoverRet);
        return WriteBackupReply(localId, reply, ERR_JS_SYSTEM_SERVICE_EXCEPTION);
    }
    if (!IInnerOsAccountManager::GetInstance().CheckAndAddLocalIdOperating(localId)) {
        return WriteBackupReply(localId, reply, ERR_JS_ACCOUNT_SERVICE_BUSY);
    }
    std::lock_guard<std::recursive_mutex> lock(backupMutex_);
    std::string backupData;
    int32_t ret = DoBackup(localId, backupData);
    IInnerOsAccountManager::GetInstance().RemoveLocalIdToOperating(localId);
    ret = WriteBackupReply(localId, reply, ret, backupData);
    if (ret == ERR_OK) {
        ReportOsAccountLifeCycle(localId, Constants::OPERATION_BACKUP);
    }
    return ret;
}

ErrCode AccountBackupRestoreMgr::BuildBackupJson(const OsAccountInfo &osAccountInfo,
    int32_t localId, CJsonUnique &out)
{
    out = CreateJson();
    if (out == nullptr) {
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    bool ok = true;
    ok &= (cJSON_AddNumberToObject(out.get(), "localId",
        osAccountInfo.GetLocalId()) != nullptr);
    ok &= (cJSON_AddStringToObject(out.get(), "localName",
        osAccountInfo.GetLocalName().c_str()) != nullptr);
    ok &= (cJSON_AddStringToObject(out.get(), "shortName",
        osAccountInfo.GetShortName().c_str()) != nullptr);
    ok &= (cJSON_AddNumberToObject(out.get(), "type",
        static_cast<int32_t>(osAccountInfo.GetType())) != nullptr);
    std::string photo = osAccountInfo.GetPhoto();
    ErrCode photoErr = IInnerOsAccountManager::GetInstance().GetFileController().GetPhotoById(
        osAccountInfo.GetLocalId(), photo);
    if (photoErr != ERR_OK) {
        ACCOUNT_LOGE("GetPhotoById failed, errCode=%{public}d", photoErr);
        return photoErr;
    }
    ok &= (cJSON_AddStringToObject(out.get(), "photo", photo.c_str()) != nullptr);
    ok &= (cJSON_AddNumberToObject(out.get(), "version",
        osAccountInfo.GetVersion()) != nullptr);
    ok &= (cJSON_AddNumberToObject(out.get(), "creatorType",
        osAccountInfo.GetCreatorType()) != nullptr);
    if (!ok) {
        ACCOUNT_LOGE("BuildBackupJson partial failure");
        return ERR_ACCOUNT_COMMON_FILE_READ_FAILED;
    }
    return ERR_OK;
}

int32_t AccountBackupRestoreMgr::OnRestore(MessageParcel &data, MessageParcel &reply)
{
    UniqueFd fd(data.ReadFileDescriptor());
    if (fd < 0) {
        ACCOUNT_LOGE("OnRestore: ReadFileDescriptor failed");
        ReportOsAccountOperationFail(0, Constants::OPERATION_BACKUP,
            ERR_ACCOUNT_COMMON_READ_PARCEL_ERROR, "OnRestore read fd failed");
        reply.WriteString(SetBackupReplyCode(ERR_JS_INVALID_PARAMETER));
        return ERR_JS_INVALID_PARAMETER;
    }
    std::string localIdJson = data.ReadString();
    if (localIdJson.empty()) {
        ACCOUNT_LOGE("OnRestore: ReadString localIdJson failed");
        ReportOsAccountOperationFail(0, Constants::OPERATION_BACKUP,
            ERR_ACCOUNT_COMMON_READ_PARCEL_ERROR, "OnRestore read localIdJson failed");
        reply.WriteString(SetBackupReplyCode(ERR_JS_INVALID_PARAMETER));
        return ERR_JS_INVALID_PARAMETER;
    }
    int32_t localId = 0;
    if (ParseLocalIdJson(localIdJson, localId) != ERR_OK) {
        ReportOsAccountOperationFail(localId, Constants::OPERATION_BACKUP,
            ERR_ACCOUNT_COMMON_READ_PARCEL_ERROR, "OnRestore parse localIdJson failed");
        reply.WriteString(SetBackupReplyCode(ERR_JS_INVALID_PARAMETER));
        return ERR_JS_INVALID_PARAMETER;
    }
    ErrCode recoverRet = IInnerOsAccountManager::GetInstance().CheckAndRecoverRestore();
    if (recoverRet != ERR_OK) {
        ACCOUNT_LOGE("OnRestore: CheckAndRecoverRestore failed, errCode=%{public}d", recoverRet);
        reply.WriteString(SetBackupReplyCode(ERR_JS_SYSTEM_SERVICE_EXCEPTION));
        return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
    }
    if (!IInnerOsAccountManager::GetInstance().CheckAndAddLocalIdOperating(localId)) {
        reply.WriteString(SetBackupReplyCode(ERR_JS_ACCOUNT_SERVICE_BUSY));
        return ERR_JS_ACCOUNT_SERVICE_BUSY;
    }
    std::lock_guard<std::recursive_mutex> lock(backupMutex_);
    int32_t ret = DoRestore(localId, fd);
    IInnerOsAccountManager::GetInstance().RemoveLocalIdToOperating(localId);
    if (ret == ERR_OK) {
        ReportOsAccountLifeCycle(localId, Constants::OPERATION_RESTORE);
    }
    reply.WriteString(SetBackupReplyCode(ret));
    return ret;
}

int32_t AccountBackupRestoreMgr::DoBackup(int32_t localId, std::string &outBackupData)
{
    OsAccountInfo osAccountInfo;
    auto &ctrl = IInnerOsAccountManager::GetInstance().GetFileController();
    ErrCode errCode = ctrl.GetOsAccountInfoById(localId, osAccountInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("DoBackup: account not found, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_BACKUP,
            ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR, "account not found");
        return ERR_JS_ACCOUNT_NOT_FOUND;
    }
    if (osAccountInfo.GetToBeRemoved() || !osAccountInfo.GetIsCreateCompleted()) {
        ACCOUNT_LOGE("DoBackup: account %{public}d is removed or not completed", localId);
        return ERR_JS_ACCOUNT_NOT_FOUND;
    }
    CJsonUnique jsonInfo;
    ErrCode buildErr = BuildBackupJson(osAccountInfo, localId, jsonInfo);
    if (buildErr != ERR_OK || jsonInfo == nullptr) {
        ACCOUNT_LOGE("DoBackup: BuildBackupJson failed, errCode=%{public}d, localId=%{public}d", buildErr, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_BACKUP,
            ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR, "BuildBackupJson failed");
        return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
    }
    outBackupData = PackJsonToString(jsonInfo);
    if (outBackupData.empty()) {
        ACCOUNT_LOGE("DoBackup: PackJsonToString failed, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_BACKUP,
            ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR, "PackJsonToString failed");
        return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
    }
    return ERR_OK;
}

int32_t AccountBackupRestoreMgr::WriteBackupReply(int32_t localId, MessageParcel &reply,
    int32_t replyCode, const std::string &backupData)
{
    int32_t actualReplyCode = replyCode;
    std::string userDir = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        std::to_string(localId);
    mkdir(userDir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    std::string tempPath = userDir + Constants::PATH_SEPARATOR + BACKUP_TEMP_FILE;
    UniqueFd writeFd(open(tempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
    if (writeFd >= 0 && !backupData.empty()) {
        ssize_t written = write(writeFd, backupData.data(), backupData.size());
        if (written != static_cast<ssize_t>(backupData.size())) {
            ACCOUNT_LOGE("WriteBackupReply: write file failed, written=%{public}zd", written);
            (void)remove(tempPath.c_str());
            return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
        }
        writeFd = UniqueFd(open(tempPath.c_str(), O_RDONLY));
    }
    if (writeFd < 0 || !reply.WriteFileDescriptor(writeFd)) {
        ACCOUNT_LOGE("WriteBackupReply: WriteFileDescriptor failed, fd=%{public}d", writeFd.Get());
        actualReplyCode = ERR_JS_SYSTEM_SERVICE_EXCEPTION;
        if (writeFd < 0) {
            ReportOsAccountOperationFail(localId, Constants::OPERATION_BACKUP,
                ERR_ACCOUNT_COMMON_FILE_OPEN_FAILED, "open backup temp file failed");
        }
    }
    (void)remove(tempPath.c_str());
    reply.WriteString(SetBackupReplyCode(actualReplyCode));
    return actualReplyCode;
}

int32_t AccountBackupRestoreMgr::DoRestore(int32_t localId, int32_t fd)
{
    OsAccountInfo currentInfo;
    auto &ctrl = IInnerOsAccountManager::GetInstance().GetFileController();
    ErrCode errCode = ctrl.GetOsAccountInfoById(localId, currentInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("DoRestore: account not found, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR, "account not found");
        return ERR_JS_ACCOUNT_NOT_FOUND;
    }
    if (currentInfo.GetToBeRemoved() || !currentInfo.GetIsCreateCompleted()) {
        ACCOUNT_LOGE("DoRestore: account %{public}d is removed or not completed", localId);
        return ERR_JS_ACCOUNT_NOT_FOUND;
    }
    OsAccountInfo backupInfo;
    int32_t parseRet = ParseAndCheckBackup(localId, fd, backupInfo, currentInfo);
    if (parseRet != ERR_OK) {
        return parseRet;
    }
    ErrCode backupErr = BackupRestoreFileOps::CreateBackup(localId);
    if (backupErr != ERR_OK) {
        ACCOUNT_LOGE("DoRestore: CreateBackup failed, errCode=%{public}d, id=%{public}d",
            backupErr, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE, backupErr,
            "CreateBackup failed");
        return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
    }
    int32_t ret = ApplyRestoreData(localId, backupInfo, currentInfo);
    if (ret != ERR_OK) {
        RollbackFromBackupUnlocked(localId);
        return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
    }
    (void) BackupRestoreFileOps::DeleteBackup(localId);
    return ret;
}

int32_t AccountBackupRestoreMgr::ParseAndCheckBackup(int32_t localId, int32_t fd,
    OsAccountInfo &backupInfo, const OsAccountInfo &currentInfo)
{
    std::string backupData;
    ErrCode errRead = ReadBackupData(fd, backupData);
    if (errRead != ERR_OK) {
        ACCOUNT_LOGE("ParseAndCheckBackup: read data failed, errCode=%{public}d, localId=%{public}d", errRead, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_FILE_READ_FAILED, "read backup data failed");
        return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
    }
    CJsonUnique jsonRoot = CreateJsonFromString(backupData);
    if (jsonRoot == nullptr) {
        ACCOUNT_LOGE("ParseAndCheckBackup: parse backup json failed, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR, "parse backup json failed");
        return ERR_JS_INVALID_PARAMETER;
    }
    if (!ParseBackupJson(jsonRoot.get(), backupInfo)) {
        ACCOUNT_LOGE("ParseAndCheckBackup: ParseBackupJson failed, localId=%{public}d", localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR, "ParseBackupJson failed");
        return ERR_JS_INVALID_PARAMETER;
    }
    if (backupInfo.GetLocalId() != localId) {
        ACCOUNT_LOGE("ParseAndCheckBackup: localId mismatch, backup=%{public}d, data=%{public}d",
            backupInfo.GetLocalId(), localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            ERR_ACCOUNT_COMMON_INVALID_PARAMETER, "localId mismatch between data and backup");
        return ERR_JS_INVALID_PARAMETER;
    }
    DomainAccountInfo backupDomainInfo;
    if (CheckDomainAccountConsistency(localId, backupDomainInfo, true) != ERR_OK) {
        return ERR_JS_SYSTEM_SERVICE_EXCEPTION;
    }
    backupInfo.SetDomainInfo(backupDomainInfo);
    int32_t ret = CheckRestorePreconditions(localId, backupInfo, currentInfo);
    if (ret != ERR_OK) {
        return ret;
    }
    return ERR_OK;
}

ErrCode AccountBackupRestoreMgr::ReadBackupData(int32_t fd, std::string &backupData)
{
    struct stat st = {};
    if (fstat(fd, &st) != 0 || st.st_size < 0) {
        ACCOUNT_LOGE("ReadBackupData: fstat failed or invalid size");
        return ERR_ACCOUNT_COMMON_FILE_READ_FAILED;
    }
    if (st.st_size > Constants::MAX_FILE_SIZE) {
        ACCOUNT_LOGE("ReadBackupData: file too large, size=%{public}lld", static_cast<long long>(st.st_size));
        return ERR_ACCOUNT_COMMON_FILE_READ_FAILED;
    }
    lseek(fd, 0, SEEK_SET);
    backupData.resize(st.st_size);
    ssize_t total = 0;
    while (total < st.st_size) {
        ssize_t n = read(fd, &backupData[total], st.st_size - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue; // Retry on interrupt
            }
            ACCOUNT_LOGE("ReadBackupData: read failed, total=%{public}zd, errno=%{public}d", total, errno);
            return ERR_ACCOUNT_COMMON_FILE_READ_FAILED;
        }
        if (n == 0) {
            break; // EOF
        }
        total += n;
    }
    backupData.resize(total);
    return ERR_OK;
}

OsAccountInfo AccountBackupRestoreMgr::BuildRestoredAccountInfo(const OsAccountInfo &backupInfo,
    const OsAccountInfo &currentInfo)
{
    OsAccountInfo result = currentInfo;
    result.SetLocalName(backupInfo.GetLocalName());
    if (result.GetShortName().empty()) {
        result.SetShortName(backupInfo.GetShortName());
    }
    result.SetCreatorType(backupInfo.GetCreatorType());
    DomainAccountInfo backupDomainInfo;
    backupInfo.GetDomainInfo(backupDomainInfo);
    if (!backupDomainInfo.IsEmpty()) {
        result.SetDomainInfo(backupDomainInfo);
    }
    return result;
}

int32_t AccountBackupRestoreMgr::ApplyRestoreData(int32_t localId, const OsAccountInfo &backupInfo,
    OsAccountInfo &currentInfo)
{
    currentInfo = BuildRestoredAccountInfo(backupInfo, currentInfo);
    auto &ctrl = IInnerOsAccountManager::GetInstance().GetFileController();
    std::vector<std::string> defaultConstraints;
    ErrCode errCode = ctrl.GetConstraintsByType(currentInfo.GetType(), defaultConstraints);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("GetConstraintsByType failed, errCode=%{public}d, id=%{public}d", errCode, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE, errCode,
            "GetConstraintsByType failed");
        return errCode;
    }
    errCode = ctrl.SetPhotoById(localId, backupInfo.GetPhoto());
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("ApplyRestoreData: SetPhotoById failed, errCode=%{public}d, id=%{public}d", errCode, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            errCode, "SetPhotoById failed");
        return errCode;
    }
    currentInfo.SetPhoto(Constants::USER_PHOTO_FILE_TXT_NAME);
    currentInfo.SetConstraints(defaultConstraints);
    errCode = ctrl.UpdateOsAccount(currentInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("ApplyRestoreData: UpdateOsAccount failed, errCode=%{public}d, id=%{public}d", errCode, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE, errCode,
            "UpdateOsAccount failed");
        return errCode;
    }
    errCode = ctrl.RemoveOABaseConstraintsInfo(localId);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("RemoveOABaseConstraintsInfo failed, errCode=%{public}d, id=%{public}d", errCode, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            errCode, "RemoveOABaseConstraintsInfo failed");
        return errCode;
    }
    errCode = ctrl.UpdateBaseOAConstraints(std::to_string(localId), defaultConstraints, true);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("set constraints failed, errCode=%{public}d, id=%{public}d", errCode, localId);
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE,
            errCode, "SetBaseOsAccountConstraints failed");
        return errCode;
    }
    
    OsAccountInterface::PublishCommonEvent(currentInfo,
        OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_USER_INFO_UPDATED, Constants::OPERATION_RESTORE);
    return ERR_OK;
}

bool AccountBackupRestoreMgr::ParseBackupJson(cJSON *jsonRoot, OsAccountInfo &backupInfo)
{
    if (jsonRoot == nullptr) {
        return false;
    }
    bool parseSuccess = GetDataByType<int>(jsonRoot, "localId", backupInfo.localId_);
    parseSuccess &= GetDataByType<std::string>(jsonRoot, "localName", backupInfo.localName_);
    parseSuccess &= GetDataByType<std::string>(jsonRoot, "shortName", backupInfo.shortName_);
    parseSuccess &= GetDataByType<OsAccountType>(jsonRoot, "type", backupInfo.type_);
    parseSuccess &= GetDataByType<std::string>(jsonRoot, "photo", backupInfo.photo_);
    parseSuccess &= GetDataByType<int32_t>(jsonRoot, "version", backupInfo.version_);
    parseSuccess &= GetDataByType<int32_t>(jsonRoot, "creatorType", backupInfo.creatorType_);
    return parseSuccess;
}

bool AccountBackupRestoreMgr::ShouldSkipDomainCheck()
{
#ifdef SUPPORT_DOMAIN_ACCOUNTS
    if (InnerDomainAccountManager::GetInstance().IsJsPluginRegistered()) {
        ACCOUNT_LOGW("JS plugin is registered, skip domain consistency check");
        return true;
    }
    if (!InnerDomainAccountManager::GetInstance().IsSoPluginLoaded()) {
        ACCOUNT_LOGW("SO plugin is not loaded, skip domain consistency check");
        return true;
    }
    return false;
#else
    return true;
#endif
}

ErrCode AccountBackupRestoreMgr::CheckDomainAccountConsistency(
    int32_t localId, DomainAccountInfo &domainInfo, bool isRestore)
{
#ifdef SUPPORT_DOMAIN_ACCOUNTS
    if (ShouldSkipDomainCheck()) {
        return ERR_OK;
    }
    domainInfo.Clear();
    const char *opStr = isRestore ? Constants::OPERATION_RESTORE
        : Constants::OPERATION_BACKUP;
    DomainAccountInfo pluginDomainInfo;
    ErrCode errCode =
        InnerDomainAccountManager::GetInstance().PluginGetAccountInfoByLocalId(localId, pluginDomainInfo);
    if (errCode != ERR_OK && errCode != ERR_JS_ACCOUNT_NOT_FOUND) {
        ACCOUNT_LOGE("CheckConsistency: plugin query failed, errCode=%{public}d, id=%{public}d", errCode, localId);
        ReportOsAccountOperationFail(localId, opStr,
            errCode, "PluginGetAccountInfoByLocalId failed");
        return errCode;
    }
    if (errCode == ERR_JS_ACCOUNT_NOT_FOUND) {
        ACCOUNT_LOGW("CheckConsistency: plugin account not found, id=%{public}d", localId);
        return ERR_OK;
    }
    domainInfo = pluginDomainInfo;
    return ERR_OK;
#else
    return ERR_OK;
#endif
}

ErrCode AccountBackupRestoreMgr::ParseBackupForRollback(int32_t localId, OsAccountInfo &backupInfo)
{
    std::string backupInfoStr;
    ErrCode errCode = BackupRestoreFileOps::ReadBackupFile(localId,
        Constants::USER_INFO_FILE_NAME, backupInfoStr);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("ParseBackupForRollback: read failed, errCode=%{public}d", errCode);
        return errCode;
    }
    if (backupInfoStr.empty()) {
        ACCOUNT_LOGE("ParseBackupForRollback: backup account_info is empty");
        return ERR_ACCOUNT_COMMON_FILE_READ_FAILED;
    }
    CJsonUnique jsonRoot = CreateJsonFromString(backupInfoStr);
    if (jsonRoot == nullptr) {
        ACCOUNT_LOGE("ParseBackupForRollback: parse json failed");
        return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
    }
    if (!FromJson(jsonRoot.get(), backupInfo)) {
        ACCOUNT_LOGE("ParseBackupForRollback: FromJson failed");
        return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
    }
    if (backupInfo.GetLocalId() != localId) {
        ACCOUNT_LOGE("ParseBackupForRollback: localId mismatch, backup=%{public}d, expected=%{public}d",
            backupInfo.GetLocalId(), localId);
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    return ERR_OK;
}

ErrCode AccountBackupRestoreMgr::DoRollbackOnce(int32_t localId)
{
    auto &ctrl = IInnerOsAccountManager::GetInstance().GetFileController();
    OsAccountInfo backupInfo;
    ErrCode errCode = ParseBackupForRollback(localId, backupInfo);
    if (errCode != ERR_OK) {
        return errCode;
    }
    DomainAccountInfo domainInfo;
    backupInfo.GetDomainInfo(domainInfo);
    ErrCode domainErr = CheckDomainAccountConsistency(localId, domainInfo, true);
    if (domainErr != ERR_OK) {
        ACCOUNT_LOGW("DoRollbackOnce: domain calibration failed, errCode=%{public}d", domainErr);
        return domainErr;
    }
    backupInfo.SetDomainInfo(domainInfo);
    errCode = ctrl.UpdateOsAccount(backupInfo);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("DoRollbackOnce: UpdateOsAccount failed, errCode=%{public}d", errCode);
        return errCode;
    }
    errCode = ctrl.RemoveOABaseConstraintsInfo(localId);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("DoRollbackOnce: RemoveOABaseConstraintsInfo failed, errCode=%{public}d", errCode);
        return errCode;
    }
    errCode = ctrl.UpdateBaseOAConstraints(std::to_string(localId),
        backupInfo.GetConstraints(), true);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("DoRollbackOnce: UpdateBaseOAConstraints failed, errCode=%{public}d", errCode);
        return errCode;
    }
    if (BackupRestoreFileOps::IsBackupFileExist(localId,
        Constants::USER_PHOTO_FILE_TXT_NAME) == ERR_OK) {
        std::string photoContent;
        errCode = BackupRestoreFileOps::ReadBackupFile(localId,
            Constants::USER_PHOTO_FILE_TXT_NAME, photoContent);
        if (errCode != ERR_OK) {
            ACCOUNT_LOGE("DoRollbackOnce: ReadBackupFile failed, errCode=%{public}d", errCode);
            return errCode;
        }
        errCode = ctrl.SetPhotoById(localId, photoContent);
        if (errCode != ERR_OK) {
            ACCOUNT_LOGE("DoRollbackOnce: SetPhotoById failed, errCode=%{public}d", errCode);
            return errCode;
        }
    }
    return ERR_OK;
}

ErrCode AccountBackupRestoreMgr::RollbackFromBackupUnlocked(int32_t localId)
{
    std::lock_guard<std::recursive_mutex> lock(backupMutex_);
    if (BackupRestoreFileOps::IsBackupComplete(localId) != ERR_OK) {
        (void) BackupRestoreFileOps::DeleteBackup(localId);
        return ERR_OK;
    }
    ErrCode ret = ERR_ACCOUNT_COMMON_FILE_READ_FAILED;
    for (int32_t retry = 0; retry < Constants::RESTORE_BACKUP_MAX_RETRY; retry++) {
        if (retry > 0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(Constants::RESTORE_BACKUP_RETRY_DELAY_MS));
        }
        ret = DoRollbackOnce(localId);
        if (ret == ERR_OK || ret == ERR_ACCOUNT_COMMON_INVALID_PARAMETER ||
            ret == ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR) {
            ErrCode delErr = BackupRestoreFileOps::DeleteBackup(localId);
            if (delErr != ERR_OK) {
                ACCOUNT_LOGE("RollbackFromBackup: delete backup failed, errCode=%{public}d", delErr);
            }
            break;
        }
    }
    if (ret != ERR_OK) {
        ReportOsAccountOperationFail(localId, Constants::OPERATION_RESTORE, ret,
            "RollbackFromBackup failed after retries");
    }
    return ret;
}
}  // namespace AccountSA
}  // namespace OHOS
