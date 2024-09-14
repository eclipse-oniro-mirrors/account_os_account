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

#ifndef OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_INTERFACE_H
#define OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_INTERFACE_H

#include <chrono>
#include <vector>
#include "account_error_no.h"
#include <mutex>
#include "os_account_info.h"
#include "os_account_user_callback.h"
#ifdef HAS_THEME_SERVICE_PART
#include "theme_manager_client.h"
#endif

namespace OHOS {
namespace AccountSA {
class OsAccountInterface {
public:
    static ErrCode SendToAMSAccountStart(OsAccountInfo &osAccountInfo);
    static ErrCode SendToAMSAccountStop(OsAccountInfo &osAccountInfo);
    static ErrCode SendToAMSAccountDeactivate(OsAccountInfo &osAccountInfo);
    static ErrCode SendToBMSAccountCreate(
        OsAccountInfo &osAccountInfo, const std::vector<std::string> &disallowedHapList = {});
#ifdef HAS_THEME_SERVICE_PART
    static void InitThemeResource(int32_t localId);
#endif
    static ErrCode SendToBMSAccountDelete(OsAccountInfo &osAccountInfo);
#ifdef HAS_USER_IDM_PART
    static ErrCode SendToIDMAccountDelete(OsAccountInfo &osAccountInfo);
#endif // HAS_USER_IDM_PART
    static void SendToCESAccountCreate(OsAccountInfo &osAccountInfo);
    static void SendToCESAccountDelete(OsAccountInfo &osAccountInfo);
    static void SendToCESAccountSwitched(int newId, int oldId);
    static ErrCode SendToStorageAccountCreateComplete(int32_t localId);
    static ErrCode SendToStorageAccountCreate(OsAccountInfo &osAccountInfo);
    static ErrCode SendToStorageAccountRemove(OsAccountInfo &osAccountInfo);
    static ErrCode SendToStorageAccountStart(OsAccountInfo &osAccountInfo);
    static ErrCode SendToStorageAccountStop(OsAccountInfo &osAccountInfo);
    static void PublishCommonEvent(
        const OsAccountInfo &osAccountInfo, const std::string &commonEvent, const std::string &operation);

private:
    static ErrCode InnerSendToStorageAccountCreateComplete(int32_t localId);
    static ErrCode InnerSendToStorageAccountCreate(OsAccountInfo &osAccountInfo);
};
}  // namespace AccountSA
}  // namespace OHOS

#endif  // OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_INTERFACE_H
