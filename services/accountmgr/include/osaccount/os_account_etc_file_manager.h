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

#ifndef OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_ETC_FILE_MANAGER_H
#define OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_ETC_FILE_MANAGER_H

#include <memory>
#include <mutex>
#include "errors.h"
#include "os_account_info.h"

namespace OHOS {
namespace AccountSA {
#ifdef SUPPORT_ETC_USER_SYNC
class OsAccountControlFileManager;

class OsAccountEtcFileManager {
public:
    OsAccountEtcFileManager(const std::weak_ptr<OsAccountControlFileManager> &weakPtr) : weakPtr_(weakPtr) {}
    ~OsAccountEtcFileManager() = default;
    ErrCode FlushEtcFilesSync();
    ErrCode SyncEtcForCreate(const OsAccountInfo &osAccountInfo);
    ErrCode SyncEtcForRemove(const OsAccountInfo &osAccountInfo);
    void CheckAndRecover();
private:
    ErrCode ModifyEtcUser(const OsAccountInfo &osAccountInfo);
    ErrCode DeleteEtcUser(const OsAccountInfo &osAccountInfo);
    ErrCode FlushEtcFilesSyncLocked();
    std::mutex etcFileLock_;
    std::weak_ptr<OsAccountControlFileManager> weakPtr_;
};
#endif // SUPPORT_ETC_USER_SYNC
} // namespace AccountSA
} // namespace OHOS
#endif // OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_ETC_FILE_MANAGER_H
