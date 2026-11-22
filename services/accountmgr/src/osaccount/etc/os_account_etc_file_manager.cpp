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

#include "os_account_etc_file_manager.h"
#include <algorithm>
#include <set>
#include "account_hisysevent_adapter.h"
#include "account_log_wrapper.h"
#include "os_account_etc_file.h"
#include "hitrace_adapter.h"
#include "os_account_constants.h"
#include "os_account_control_file_manager.h"

namespace OHOS {
namespace AccountSA {
#ifdef SUPPORT_ETC_USER_SYNC

ErrCode OsAccountEtcFileManager::SyncEtcForCreate(const OsAccountInfo &osAccountInfo)
{
    bool flagExist = false;
    bool passwdExist = false;
    ErrCode ret = EtcAccountFileUtils::IsEtcFaultFlagFileExist(flagExist);
    if (ret == ERR_OK) {
        ret = EtcAccountFileUtils::IsEtcFileExist(EtcAccountFileUtils::PASSWD_FILE_NAME, passwdExist);
    }
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Check etc file state failed, ret = %{public}d.", ret);
        return ret;
    }
    if (flagExist || !passwdExist) {
        ACCOUNT_LOGI("Etc files need rebuild when creating account %{public}d", osAccountInfo.GetLocalId());
        ret = FlushEtcFilesSync();
        if (ret != ERR_OK) {
            ACCOUNT_LOGE("Rebuild etc files for account %{public}d failed, ret = %{public}d.",
                osAccountInfo.GetLocalId(), ret);
            return ret;
        }
        ACCOUNT_LOGI("Rebuild etc files for account %{public}d success", osAccountInfo.GetLocalId());
    }
    return ModifyEtcUser(osAccountInfo);
}

ErrCode OsAccountEtcFileManager::SyncEtcForRemove(const OsAccountInfo &osAccountInfo)
{
    ErrCode ret = DeleteEtcUser(osAccountInfo);
    if (ret == ERR_OK) {
        ACCOUNT_LOGI("Sync etc files for removed account %{public}d success", osAccountInfo.GetLocalId());
    } else {
        ErrCode flagRet = EtcAccountFileUtils::CreateEtcFaultFlagFile();
        if (flagRet != ERR_OK) {
            ACCOUNT_LOGE("Create etc fault flag file failed after remove sync failed, localId = %{public}d, "
                "removeRet = %{public}d, flagRet = %{public}d", osAccountInfo.GetLocalId(), ret, flagRet);
        }
    }
    return ret;
}

void OsAccountEtcFileManager::CheckAndRecover()
{
    bool etcFaultFlagExist = false;
    bool etcPasswdExist = false;
    bool etcGroupExist = false;
    ErrCode ret = EtcAccountFileUtils::IsEtcFaultFlagFileExist(etcFaultFlagExist);
    if (ret == ERR_OK) {
        ret = EtcAccountFileUtils::IsEtcFileExist(EtcAccountFileUtils::PASSWD_FILE_NAME, etcPasswdExist);
    }
    if (ret == ERR_OK) {
        ret = EtcAccountFileUtils::IsEtcFileExist(EtcAccountFileUtils::GROUP_FILE_NAME, etcGroupExist);
    }
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Check etc file state failed, ret = %{public}d.", ret);
        REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_ETC_FILE, ret, "Check etc file state failed");
        ret = FlushEtcFilesSync();
    }
    if (etcFaultFlagExist || !etcPasswdExist || !etcGroupExist) {
        ACCOUNT_LOGW("Etc file is in fault state or does not exist, need flush etc file.");
        ret = FlushEtcFilesSync();
    }
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Flush etc file failed, ret = %{public}d.", ret);
        REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_ETC_FILE, ret, "Flush etc file failed");
    }
}

namespace {
std::vector<std::string> BuildNormalGroupMembers(const std::vector<EtcUserEntry> &userEntries)
{
    std::vector<EtcUserEntry> osAccountEntries = userEntries;
    std::sort(osAccountEntries.begin(), osAccountEntries.end(),
        [](const EtcUserEntry &a, const EtcUserEntry &b) { return a.uid < b.uid; });
    std::vector<std::string> members;
    for (const auto &entry : osAccountEntries) {
        if (EtcAccountFileUtils::IsGroupMemberRepresentable(entry.userName)) {
            members.push_back(entry.userName);
        }
    }
    return members;
}

EtcGroupEntry *FindEtcGroup(std::vector<EtcGroupEntry> &groupEntries, const std::string &groupName)
{
    auto it = std::find_if(groupEntries.begin(), groupEntries.end(),
        [&groupName](const EtcGroupEntry &group) { return group.groupName == groupName; });
    return it == groupEntries.end() ? nullptr : &(*it);
}

void RebuildNormalGroup(std::vector<EtcGroupEntry> &groupEntries,
    const std::vector<EtcUserEntry> &userEntries)
{
    std::vector<std::string> normalMembers = BuildNormalGroupMembers(userEntries);
    normalMembers.insert(normalMembers.begin(), EtcAccountFileUtils::RESERVED_ETC_USER_NAME);
    EtcGroupEntry *normalGroup = FindEtcGroup(groupEntries, EtcAccountFileUtils::NORMAL_GROUP_NAME);
    if (normalGroup == nullptr) {
        EtcGroupEntry newGroup;
        newGroup.groupName = EtcAccountFileUtils::NORMAL_GROUP_NAME;
        newGroup.gid = EtcAccountFileUtils::NORMAL_GROUP_GID;
        newGroup.members = normalMembers;
        groupEntries.push_back(std::move(newGroup));
        return;
    }
    normalGroup->members = normalMembers;
}

void UpdateAdminGroup(std::vector<EtcGroupEntry> &groupEntries, const std::string &oldUserName,
    const std::string &newUserName, bool isAdmin)
{
    EtcGroupEntry *adminGroup = FindEtcGroup(groupEntries, EtcAccountFileUtils::ADMIN_GROUP_NAME);
    if (adminGroup == nullptr) {
        EtcGroupEntry newGroup;
        newGroup.groupName = EtcAccountFileUtils::ADMIN_GROUP_NAME;
        newGroup.gid = EtcAccountFileUtils::ADMIN_GROUP_GID;
        adminGroup = &groupEntries.emplace_back(std::move(newGroup));
    }
    if (!oldUserName.empty()) {
        adminGroup->members.erase(std::remove(adminGroup->members.begin(), adminGroup->members.end(),
            oldUserName), adminGroup->members.end());
    }
    adminGroup->members.erase(std::remove(adminGroup->members.begin(), adminGroup->members.end(),
        newUserName), adminGroup->members.end());
    if (isAdmin && EtcAccountFileUtils::IsGroupMemberRepresentable(newUserName)) {
        adminGroup->members.push_back(newUserName);
    }
    if (std::find(adminGroup->members.begin(), adminGroup->members.end(),
        EtcAccountFileUtils::RESERVED_ETC_USER_NAME) == adminGroup->members.end()) {
        adminGroup->members.insert(adminGroup->members.begin(), EtcAccountFileUtils::RESERVED_ETC_USER_NAME);
    }
}

std::string SerializeEtcGroups(const std::vector<EtcGroupEntry> &groupEntries)
{
    std::vector<std::string> groupLines;
    for (const auto &group : groupEntries) {
        groupLines.push_back(group.ToString());
    }
    return EtcAccountFileUtils::JoinLines(groupLines);
}

ErrCode LoadEtcEntries(std::vector<EtcUserEntry> &userEntries, std::vector<EtcGroupEntry> &groupEntries)
{
    std::string passwdContent;
    ErrCode ret = EtcAccountFileUtils::ReadEtcFile(EtcAccountFileUtils::PASSWD_FILE_NAME, passwdContent);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Read etc passwd file failed, ret = %{public}d.", ret);
        return ret;
    }
    std::string groupContent;
    ret = EtcAccountFileUtils::ReadEtcFile(EtcAccountFileUtils::GROUP_FILE_NAME, groupContent);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Read etc group file failed, ret = %{public}d.", ret);
        return ret;
    }
    if (!EtcAccountFileUtils::ParsePasswdContent(passwdContent, userEntries)) {
        ACCOUNT_LOGE("Parse etc passwd file failed, need rebuild.");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    if (!EtcAccountFileUtils::ParseGroupContent(groupContent, groupEntries)) {
        ACCOUNT_LOGE("Parse etc group file failed, need rebuild.");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    return ERR_OK;
}

ErrCode WriteEtcContents(const std::string &passwdContent, const std::string &groupContent)
{
    ErrCode ret = EtcAccountFileUtils::CreateEtcFaultFlagFile();
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Create etc fault flag file failed, ret = %{public}d.", ret);
        return ret;
    }
    ret = EtcAccountFileUtils::WriteEtcFile(EtcAccountFileUtils::PASSWD_FILE_NAME, passwdContent);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Write etc passwd file failed, ret = %{public}d.", ret);
        return ret;
    }
    ret = EtcAccountFileUtils::WriteEtcFile(EtcAccountFileUtils::GROUP_FILE_NAME, groupContent);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Write etc group file failed, ret = %{public}d.", ret);
        return ret;
    }
    ErrCode flagRet = EtcAccountFileUtils::RemoveEtcFaultFlagFile();
    if (flagRet != ERR_OK) {
        ACCOUNT_LOGE("Remove etc fault flag file failed, ret = %{public}d.", flagRet);
    }
    return ERR_OK;
}

bool IsEtcUserSkippable(const std::string &userName, const std::set<std::string> &takenNames,
    int32_t localId)
{
    if (takenNames.count(userName) > 0) {
        ACCOUNT_LOGE("Skip etc sync for localId %{public}d: user name already exists.", localId);
        REPORT_OS_ACCOUNT_FAIL(localId, Constants::OPERATION_ETC_FILE,
            ERR_ACCOUNT_COMMON_SHORT_NAME_HAD_EXISTED, "Skip etc sync: user name already exists");
        return true;
    }
    if (!EtcAccountFileUtils::IsEtcUserNameRepresentable(userName)) {
        ACCOUNT_LOGE("Skip etc sync for localId %{public}d: user name is not representable.", localId);
        REPORT_OS_ACCOUNT_FAIL(localId, Constants::OPERATION_ETC_FILE,
            ERR_ACCOUNT_COMMON_INVALID_PARAMETER, "Skip etc sync: user name is not representable");
        return true;
    }
    return false;
}

bool IsEtcRowUpToDate(const std::vector<EtcUserEntry> &userEntries,
    const std::vector<EtcGroupEntry> &groupEntries, const OsAccountInfo &osAccountInfo)
{
    int32_t localId = osAccountInfo.GetLocalId();
    const std::string &userName = osAccountInfo.GetShortName();
    auto rowIt = std::find_if(userEntries.begin(), userEntries.end(),
        [localId](const EtcUserEntry &entry) { return entry.uid == localId; });
    if (rowIt == userEntries.end() || rowIt->userName != userName) {
        return false;
    }
    bool isAdminMember = false;
    for (const auto &group : groupEntries) {
        if (group.groupName != EtcAccountFileUtils::ADMIN_GROUP_NAME) {
            continue;
        }
        isAdminMember = std::find(group.members.begin(), group.members.end(), userName) !=
            group.members.end();
        break;
    }
    return isAdminMember == (osAccountInfo.GetType() == OsAccountType::ADMIN);
}

ErrCode WriteEtcUserEntries(const std::vector<EtcUserEntry> &userEntries,
    std::vector<EtcGroupEntry> &groupEntries, const std::string &oldUserName,
    const std::string &newUserName, bool isAdmin)
{
    std::vector<std::string> passwdLines;
    passwdLines.push_back(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE);
    passwdLines.push_back(EtcAccountFileUtils::U1_FIXED_PASSWD_LINE);
    for (const auto &entry : userEntries) {
        passwdLines.push_back(entry.ToString());
    }
    RebuildNormalGroup(groupEntries, userEntries);
    UpdateAdminGroup(groupEntries, oldUserName, newUserName, isAdmin);
    return WriteEtcContents(EtcAccountFileUtils::JoinLines(passwdLines), SerializeEtcGroups(groupEntries));
}

ErrCode RemoveEtcUserRow(std::vector<EtcUserEntry> &userEntries,
    std::vector<EtcGroupEntry> &groupEntries, int32_t localId, const std::string &oldUserName)
{
    if (oldUserName.empty()) {
        ACCOUNT_LOGI("Skip etc sync for localId %{public}d: empty short name.", localId);
        return ERR_OK;
    }
    auto rowIt = std::find_if(userEntries.begin(), userEntries.end(),
        [localId](const EtcUserEntry &entry) { return entry.uid == localId; });
    if (rowIt != userEntries.end()) {
        userEntries.erase(rowIt);
    }
    ErrCode ret = WriteEtcUserEntries(userEntries, groupEntries, oldUserName, "", false);
    if (ret == ERR_OK) {
        ACCOUNT_LOGI("Remove etc entry for localId %{public}d success", localId);
    }
    return ret;
}

ErrCode UpsertEtcUserRow(const OsAccountInfo &osAccountInfo,
    std::vector<EtcUserEntry> &userEntries, std::vector<EtcGroupEntry> &groupEntries,
    const std::string &oldUserName, const std::set<std::string> &takenNames)
{
    std::string userName = osAccountInfo.GetShortName();
    int32_t localId = osAccountInfo.GetLocalId();
    if (IsEtcUserSkippable(userName, takenNames, localId)) {
        return ERR_OK;
    }
    EtcUserEntry newEntry = EtcUserEntry::FromOsAccountInfo(osAccountInfo, userName);
    auto it = std::find_if(userEntries.begin(), userEntries.end(),
        [localId](const EtcUserEntry &entry) { return entry.uid == localId; });
    if (it != userEntries.end()) {
        *it = newEntry;
    } else {
        userEntries.push_back(newEntry);
    }
    ErrCode ret = WriteEtcUserEntries(userEntries, groupEntries, oldUserName, newEntry.userName,
        osAccountInfo.GetType() == OsAccountType::ADMIN);
    if (ret == ERR_OK) {
        ACCOUNT_LOGI("Sync etc files for account %{public}d success", localId);
    }
    return ret;
}
} // namespace

ErrCode OsAccountEtcFileManager::ModifyEtcUser(const OsAccountInfo &osAccountInfo)
{
    TraceGuard traceGuard("EtcFileModify");
    std::lock_guard<std::mutex> lock(etcFileLock_);
    std::vector<EtcUserEntry> userEntries;
    std::vector<EtcGroupEntry> groupEntries;
    ErrCode ret = LoadEtcEntries(userEntries, groupEntries);
    if (ret != ERR_OK) {
        ACCOUNT_LOGW("Parse etc files failed, rebuild them. ret = %{public}d", ret);
        ret = FlushEtcFilesSyncLocked();
        if (ret != ERR_OK) {
            return ret;
        }
        ret = LoadEtcEntries(userEntries, groupEntries);
        if (ret != ERR_OK) {
            return ret;
        }
    }
    int32_t localId = osAccountInfo.GetLocalId();
    std::string oldUserName;
    std::set<std::string> takenNames = {EtcAccountFileUtils::RESERVED_ETC_USER_NAME};
    for (const auto &entry : userEntries) {
        if (entry.uid == localId) {
            oldUserName = entry.userName;
            continue;
        }
        takenNames.insert(entry.userName);
    }
    if (osAccountInfo.GetShortName().empty()) {
        return RemoveEtcUserRow(userEntries, groupEntries, localId, oldUserName);
    }
    if (IsEtcRowUpToDate(userEntries, groupEntries, osAccountInfo)) {
        return ERR_OK;
    }
    return UpsertEtcUserRow(osAccountInfo, userEntries, groupEntries, oldUserName, takenNames);
}

ErrCode OsAccountEtcFileManager::DeleteEtcUser(const OsAccountInfo &osAccountInfo)
{
    TraceGuard traceGuard("EtcFileDelete");
    OsAccountInfo info = osAccountInfo;
    info.SetShortName(""); // empty short name drives ModifyEtcUser to take the row-remove path
    return ModifyEtcUser(info);
}

ErrCode OsAccountEtcFileManager::FlushEtcFilesSync()
{
    TraceGuard traceGuard("EtcFileFlush");
    std::lock_guard<std::mutex> lock(etcFileLock_);
    return FlushEtcFilesSyncLocked();
}

ErrCode OsAccountEtcFileManager::FlushEtcFilesSyncLocked()
{
    auto control = weakPtr_.lock();
    std::vector<OsAccountInfo> osAccountInfos;
    ErrCode ret = control->GetOsAccountList(osAccountInfos);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Get all account info failed, ret = %{public}d.", ret);
        return ret;
    }
    std::string passwdContent;
    std::string groupContent;
    ret = EtcAccountFileUtils::BuildExpectedEtcContent(osAccountInfos, passwdContent, groupContent);
    if (ret != ERR_OK) {
        return ret;
    }
    ret = WriteEtcContents(passwdContent, groupContent);
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Flush write etc files failed, ret = %{public}d.", ret);
        return ret;
    }
    return ERR_OK;
}
#endif // SUPPORT_ETC_USER_SYNC

}  // namespace AccountSA
}  // namespace OHOS
