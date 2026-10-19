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

#ifndef OS_ACCOUNT_ACCOUNT_BACKUP_RESTORE_H
#define OS_ACCOUNT_ACCOUNT_BACKUP_RESTORE_H

#include <mutex>
#include <string>
#include "backup_restore_file_ops.h"
#include "json_utils.h"
#include "message_parcel.h"
#include "os_account_info.h"

namespace OHOS {
namespace AccountSA {
class AccountBackupRestoreMgr {
public:
    static AccountBackupRestoreMgr &GetInstance();
    int32_t OnBackup(MessageParcel &data, MessageParcel &reply);
    int32_t OnRestore(MessageParcel &data, MessageParcel &reply);
    ErrCode RollbackFromBackupUnlocked(int32_t localId);

private:
    AccountBackupRestoreMgr() = default;
    ~AccountBackupRestoreMgr() = default;
    AccountBackupRestoreMgr(const AccountBackupRestoreMgr &) = delete;
    AccountBackupRestoreMgr &operator=(const AccountBackupRestoreMgr &) = delete;
    ErrCode ParseLocalIdJson(const std::string &jsonStr, int32_t &localId);
    std::string SetBackupReplyCode(int32_t replyCode);
    ErrCode BuildBackupJson(const OsAccountInfo &osAccountInfo, int32_t localId,
        CJsonUnique &out);
    int32_t CheckRestorePreconditions(int32_t localId, const OsAccountInfo &backupInfo,
        const OsAccountInfo &currentInfo);
    int32_t ValidateBackupName(int32_t localId, const OsAccountInfo &backupInfo);
    int32_t ValidateDomainBinding(int32_t localId, const OsAccountInfo &backupInfo,
        const OsAccountInfo &currentInfo);
    ErrCode DoRollbackOnce(int32_t localId);
    ErrCode ParseBackupForRollback(int32_t localId, OsAccountInfo &backupInfo);
    int32_t DoBackup(int32_t localId, std::string &outBackupData);
    int32_t WriteBackupReply(int32_t localId, MessageParcel &reply, int32_t replyCode,
        const std::string &backupData = "");
    int32_t DoRestore(int32_t localId, int32_t fd);
    ErrCode ReadBackupData(int32_t fd, std::string &backupData);
    int32_t ParseAndCheckBackup(int32_t localId, int32_t fd,
        OsAccountInfo &backupInfo, const OsAccountInfo &currentInfo);
    bool ParseBackupJson(cJSON *jsonRoot, OsAccountInfo &backupInfo);
    ErrCode CheckDomainAccountConsistency(int32_t localId, DomainAccountInfo &domainInfo,
        bool isRestore = false);
    bool ShouldSkipDomainCheck();
    OsAccountInfo BuildRestoredAccountInfo(const OsAccountInfo &backupInfo,
        const OsAccountInfo &currentInfo);
    int32_t ApplyRestoreData(int32_t localId, const OsAccountInfo &backupInfo,
        OsAccountInfo &currentInfo);

private:
    std::recursive_mutex backupMutex_;
};
}  // namespace AccountSA
}  // namespace OHOS
#endif  // OS_ACCOUNT_ACCOUNT_BACKUP_RESTORE_H
