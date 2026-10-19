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

#ifndef OS_ACCOUNT_BACKUP_RESTORE_FILE_OPS_H
#define OS_ACCOUNT_BACKUP_RESTORE_FILE_OPS_H

#include <mutex>
#include <string>
#include "account_error_no.h"
#include "account_file_operator.h"
#include "os_account_constants.h"

namespace OHOS {
namespace AccountSA {
class BackupRestoreFileOps {
public:
    static ErrCode CreateBackup(int32_t localId);
    static ErrCode DeleteBackup(int32_t localId);
    static ErrCode IsBackupComplete(int32_t localId);
    static ErrCode ReadBackupFile(int32_t localId, const std::string &fileName,
        std::string &content);
    static ErrCode IsBackupFileExist(int32_t localId, const std::string &fileName);

private:
    static std::string GetBackupDir(int32_t localId);
    static std::string GetUserDir(int32_t localId);
    static std::recursive_mutex fileOpsMutex_;
};
}  // namespace AccountSA
}  // namespace OHOS
#endif  // OS_ACCOUNT_BACKUP_RESTORE_FILE_OPS_H
