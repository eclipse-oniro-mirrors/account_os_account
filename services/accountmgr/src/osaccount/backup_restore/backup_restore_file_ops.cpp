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

#include "backup_restore_file_ops.h"

#include <filesystem>
#include "account_log_wrapper.h"

namespace OHOS {
namespace AccountSA {

std::recursive_mutex BackupRestoreFileOps::fileOpsMutex_;

std::string BackupRestoreFileOps::GetBackupDir(int32_t localId)
{
    return Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(localId) + "_backup";
}

std::string BackupRestoreFileOps::GetUserDir(int32_t localId)
{
    return Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR + std::to_string(localId);
}

ErrCode BackupRestoreFileOps::CreateBackup(int32_t localId)
{
    std::lock_guard<std::recursive_mutex> lock(fileOpsMutex_);
    AccountFileOperator fileOp;
    std::string userDir = GetUserDir(localId);
    std::string backupDir = GetBackupDir(localId);
    if (!fileOp.IsExistDir(userDir)) {
        ACCOUNT_LOGE("CreateBackup: user dir not exist, localId=%{public}d", localId);
        return ERR_OSACCOUNT_SERVICE_CONTROL_UPDATE_FILE_NOT_EXISTS_ERROR;
    }
    DeleteBackup(localId);
    ErrCode errCode = fileOp.CreateDir(backupDir);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("CreateBackup: create dir failed, errCode=%{public}d", errCode);
        return errCode;
    }
    std::error_code ec;
    std::filesystem::copy(userDir, backupDir,
        std::filesystem::copy_options::recursive |
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        ACCOUNT_LOGE("CreateBackup: copy failed, errCode=%{public}d", ec.value());
        DeleteBackup(localId);
        return ERR_ACCOUNT_COMMON_FILE_WRITE_FAILED;
    }
    std::string completePath = backupDir + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_COMPLETE_FILE;
    errCode = fileOp.InputFileByPathAndContentWithTransaction(completePath, "");
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("CreateBackup: write .complete failed, errCode=%{public}d", errCode);
        DeleteBackup(localId);
        return errCode;
    }
    return ERR_OK;
}

ErrCode BackupRestoreFileOps::DeleteBackup(int32_t localId)
{
    std::lock_guard<std::recursive_mutex> lock(fileOpsMutex_);
    AccountFileOperator fileOp;
    std::string backupDir = GetBackupDir(localId);
    if (!fileOp.IsExistDir(backupDir)) {
        return ERR_OK;
    }
    ErrCode errCode = fileOp.DeleteDirOrFile(backupDir);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("DeleteBackup failed, errCode=%{public}d, localId=%{public}d", errCode, localId);
    }
    return errCode;
}

ErrCode BackupRestoreFileOps::IsBackupComplete(int32_t localId)
{
    std::lock_guard<std::recursive_mutex> lock(fileOpsMutex_);
    AccountFileOperator fileOp;
    std::string backupDir = GetBackupDir(localId);
    if (!fileOp.IsExistDir(backupDir)) {
        return ERR_OSACCOUNT_SERVICE_FILE_FIND_FILE_ERROR;
    }
    std::string completePath = backupDir + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_COMPLETE_FILE;
    ErrCode errCode = fileOp.CheckFileExistence(completePath);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("IsBackupComplete: .complete not exist, localId=%{public}d", localId);
    }
    return errCode;
}

ErrCode BackupRestoreFileOps::ReadBackupFile(int32_t localId, const std::string &fileName,
    std::string &content)
{
    std::lock_guard<std::recursive_mutex> lock(fileOpsMutex_);
    AccountFileOperator fileOp;
    std::string backupDir = GetBackupDir(localId);
    std::string filePath = backupDir + Constants::PATH_SEPARATOR + fileName;
    ErrCode errCode = fileOp.GetFileContentByPath(filePath, content);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("ReadBackupFile: read failed, localId=%{public}d, file=%{public}s, "
            "errCode=%{public}d", localId, fileName.c_str(), errCode);
    }
    return errCode;
}

ErrCode BackupRestoreFileOps::IsBackupFileExist(int32_t localId, const std::string &fileName)
{
    std::lock_guard<std::recursive_mutex> lock(fileOpsMutex_);
    AccountFileOperator fileOp;
    std::string backupDir = GetBackupDir(localId);
    std::string filePath = backupDir + Constants::PATH_SEPARATOR + fileName;
    ErrCode errCode = fileOp.CheckFileExistence(filePath);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("IsBackupFileExist: file not exist, localId=%{public}d, file=%{public}s",
            localId, fileName.c_str());
    }
    return errCode;
}
}  // namespace AccountSA
}  // namespace OHOS
