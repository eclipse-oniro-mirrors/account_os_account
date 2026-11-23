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
#ifndef OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_ETC_FILE_H
#define OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_ETC_FILE_H

#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "errors.h"
#include "os_account_info.h"

namespace OHOS {
namespace AccountSA {

struct EtcUserEntry {
    std::string userName;
    std::string password = "x";
    int32_t uid = 0;
    int32_t gid = 0;
    std::string gecos;
    std::string homeDir;
    std::string shell;
    static EtcUserEntry FromOsAccountInfo(const OsAccountInfo &info, const std::string &userName);
    std::string ToString() const;
    static ErrCode FromString(const std::string &line, EtcUserEntry &entry);
};

struct EtcGroupEntry {
    std::string groupName;
    std::string password = "x";
    int32_t gid = 0;
    std::vector<std::string> members;
    std::string ToString() const;
    static ErrCode FromString(const std::string &line, EtcGroupEntry &entry);
};

class EtcAccountFileUtils {
public:
#ifdef ACCOUNT_TEST
    static constexpr const char *ETC_FILE_DIR = "/data/service/el1/public/etc/test/";
#else
    static constexpr const char *ETC_FILE_DIR = "/data/service/el1/public/etc/";
#endif
    static constexpr const char *PASSWD_FILE_NAME = "passwd";
    static constexpr const char *GROUP_FILE_NAME = "group";
    static constexpr const char *ETC_FAULT_FLAG_NAME = ".etc_fault_flag";
    static constexpr const char *ETC_MIGRATION_FLAG_NAME = ".etc_migration_done";
    static constexpr const char *RESERVED_USER_NAME = "root";
    static constexpr size_t ETC_NAME_MAX_SIZE = 60;
    static constexpr uint32_t ETC_NAME_SUFFIX_MAX = 999;
    static constexpr size_t ETC_NAME_SUFFIX_RESERVE = 4;
    static constexpr char ETC_NAME_DEFAULT_PREFIX = 'a';
    static constexpr const char *NORMAL_GROUP_NAME = "normal";
    static constexpr const char *ADMIN_GROUP_NAME = "admin";
    static constexpr int32_t NORMAL_GROUP_GID = 20;
    static constexpr int32_t ADMIN_GROUP_GID = 80;
    static constexpr const char *DEFAULT_SHELL = "/bin/bash";
    static constexpr const char *DEFAULT_HOME_DIR = "/storage/Users/currentUser";
    static constexpr const char *U0_FIXED_PASSWD_LINE = "root:x:0:20::/storage/Users/currentUser:/bin/bash";
    static constexpr const char *U1_FIXED_PASSWD_LINE = ":x:1:20::/storage/Users/currentUser:/bin/bash";
    static constexpr const char *RESERVED_ETC_USER_NAME = "root";

    static ErrCode WriteEtcFile(const std::string &fileName, const std::string &content);
    static ErrCode ReadEtcFile(const std::string &fileName, std::string &content);
    static ErrCode IsEtcFileExist(const std::string &fileName, bool &isExist);
    static ErrCode CreateEtcFaultFlagFile();
    static ErrCode RemoveEtcFaultFlagFile();
    static ErrCode IsEtcFaultFlagFileExist(bool &isExist);
    static void SplitContentToLines(const std::string &content, std::vector<std::string> &lines);
    static std::string JoinLines(const std::vector<std::string> &lines);
    static bool ParsePasswdContent(const std::string &content, std::vector<EtcUserEntry> &entries);
    static bool ParseGroupContent(const std::string &content, std::vector<EtcGroupEntry> &entries);
    static bool IsEtcUserNameRepresentable(const std::string &userName);
    static bool IsGroupMemberRepresentable(const std::string &userName);
    static ErrCode CreateEtcMigrationFlagFile();
    static ErrCode IsEtcMigrationFlagExist(bool &isExist);
    static bool IsMigrationNameCompliant(const std::string &userName);
    static bool GenerateCompliantShortName(const std::string &baseName, int32_t localId,
        const std::set<std::string> &takenNames, std::string &newName);
    static void BuildRenamePlan(const std::vector<OsAccountInfo> &osAccountInfos,
        std::vector<OsAccountInfo> &renamePlan);
    static ErrCode BuildExpectedEtcContent(const std::vector<OsAccountInfo> &osAccountInfos,
        std::string &passwdContent, std::string &groupContent);
};
} // namespace AccountSA
} // namespace OHOS
#endif // OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_ETC_FILE_H
