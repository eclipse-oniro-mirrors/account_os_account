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
#include "os_account_etc_file.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <policycoreutils.h>
#include <set>
#include "account_file_operator.h"
#include "account_hisysevent_adapter.h"
#include "account_log_wrapper.h"
#include "os_account_constants.h"

namespace OHOS {
namespace AccountSA {
namespace {
constexpr char ETC_FIELD_DELIMITER = ':';
constexpr char ETC_MEMBER_DELIMITER = ',';
constexpr char ETC_LINE_DELIMITER = '\n';
constexpr size_t PASSWD_FIELD_COUNT = 7;
constexpr size_t GROUP_FIELD_MIN_COUNT = 3;
constexpr size_t GROUP_FIELD_MAX_COUNT = 4;
constexpr size_t PASSWD_FIELD_USERNAME = 0;
constexpr size_t PASSWD_FIELD_PASSWORD = 1;
constexpr size_t PASSWD_FIELD_UID = 2;
constexpr size_t PASSWD_FIELD_GID = 3;
constexpr size_t PASSWD_FIELD_GECOS = 4;
constexpr size_t PASSWD_FIELD_HOME = 5;
constexpr size_t PASSWD_FIELD_SHELL = 6;
constexpr size_t GROUP_FIELD_NAME = 0;
constexpr size_t GROUP_FIELD_PASSWORD = 1;
constexpr size_t GROUP_FIELD_GID = 2;
constexpr size_t GROUP_FIELD_MEMBERS = 3;

void SplitStringToTokens(const std::string &str, char delimiter, std::vector<std::string> &tokens)
{
    tokens.clear();
    std::string token;
    for (char c : str) {
        if (c == delimiter) {
            tokens.push_back(token);
            token.clear();
        } else {
            token.push_back(c);
        }
    }
    tokens.push_back(token);
}

bool ParseInt32(const std::string &str, int32_t &value)
{
    if (str.empty()) {
        return false;
    }
    errno = 0;
    char *endPtr = nullptr;
    long result = strtol(str.c_str(), &endPtr, 10); // NOLINT(cert-err34-c)
    if (endPtr == str.c_str() || *endPtr != '\0' || errno == ERANGE) {
        return false;
    }
    if (result < INT32_MIN || result > INT32_MAX) {
        return false;
    }
    value = static_cast<int32_t>(result);
    return true;
}

bool IsBlankLine(const std::string &line)
{
    return line.empty() || line.find_first_not_of(" \t\r") == std::string::npos;
}

std::string GetEtcFilePath(const std::string &fileName)
{
    return std::string(EtcAccountFileUtils::ETC_FILE_DIR) + fileName;
}
} // namespace

EtcUserEntry EtcUserEntry::FromOsAccountInfo(const OsAccountInfo &info, const std::string &userName)
{
    EtcUserEntry entry;
    entry.userName = userName;
    entry.uid = info.GetLocalId();
    entry.gid = EtcAccountFileUtils::NORMAL_GROUP_GID;
    entry.homeDir = EtcAccountFileUtils::DEFAULT_HOME_DIR;
    entry.shell = EtcAccountFileUtils::DEFAULT_SHELL;
    return entry;
}

std::string EtcUserEntry::ToString() const
{
    return userName + ETC_FIELD_DELIMITER + password + ETC_FIELD_DELIMITER + std::to_string(uid) +
        ETC_FIELD_DELIMITER + std::to_string(gid) + ETC_FIELD_DELIMITER + gecos + ETC_FIELD_DELIMITER +
        homeDir + ETC_FIELD_DELIMITER + shell;
}

ErrCode EtcUserEntry::FromString(const std::string &line, EtcUserEntry &entry)
{
    std::vector<std::string> tokens;
    SplitStringToTokens(line, ETC_FIELD_DELIMITER, tokens);
    if (tokens.size() != PASSWD_FIELD_COUNT || tokens[PASSWD_FIELD_USERNAME].empty()) {
        ACCOUNT_LOGE("Invalid passwd line, field count = %{public}zu", tokens.size());
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    int32_t uid = 0;
    int32_t gid = 0;
    if (!ParseInt32(tokens[PASSWD_FIELD_UID], uid) || !ParseInt32(tokens[PASSWD_FIELD_GID], gid)) {
        ACCOUNT_LOGE("Invalid passwd line, uid or gid is not number");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    entry.userName = tokens[PASSWD_FIELD_USERNAME];
    entry.password = tokens[PASSWD_FIELD_PASSWORD];
    entry.uid = uid;
    entry.gid = gid;
    entry.gecos = tokens[PASSWD_FIELD_GECOS];
    entry.homeDir = tokens[PASSWD_FIELD_HOME];
    entry.shell = tokens[PASSWD_FIELD_SHELL];
    return ERR_OK;
}

std::string EtcGroupEntry::ToString() const
{
    std::string line = groupName + ETC_FIELD_DELIMITER + password + ETC_FIELD_DELIMITER +
        std::to_string(gid) + ETC_FIELD_DELIMITER;
    for (size_t i = 0; i < members.size(); ++i) {
        if (i != 0) {
            line.push_back(ETC_MEMBER_DELIMITER);
        }
        line += members[i];
    }
    return line;
}

ErrCode EtcGroupEntry::FromString(const std::string &line, EtcGroupEntry &entry)
{
    std::vector<std::string> tokens;
    SplitStringToTokens(line, ETC_FIELD_DELIMITER, tokens);
    if ((tokens.size() < GROUP_FIELD_MIN_COUNT || tokens.size() > GROUP_FIELD_MAX_COUNT) ||
        tokens[GROUP_FIELD_NAME].empty()) {
        ACCOUNT_LOGE("Invalid group line, field count = %{public}zu", tokens.size());
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    int32_t gid = 0;
    if (!ParseInt32(tokens[GROUP_FIELD_GID], gid)) {
        ACCOUNT_LOGE("Invalid group line, gid is not number");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    entry.groupName = tokens[GROUP_FIELD_NAME];
    entry.password = tokens[GROUP_FIELD_PASSWORD];
    entry.gid = gid;
    entry.members.clear();
    if (tokens.size() == GROUP_FIELD_MAX_COUNT && !tokens[GROUP_FIELD_MEMBERS].empty()) {
        std::vector<std::string> memberTokens;
        SplitStringToTokens(tokens[GROUP_FIELD_MEMBERS], ETC_MEMBER_DELIMITER, memberTokens);
        for (const auto &member : memberTokens) {
            if (!member.empty()) {
                entry.members.push_back(member);
            }
        }
    }
    return ERR_OK;
}

ErrCode EtcAccountFileUtils::WriteEtcFile(const std::string &fileName, const std::string &content)
{
    AccountFileOperator fileOperator;
    if (!fileOperator.IsExistDir(ETC_FILE_DIR)) {
        ACCOUNT_LOGE("Etc dir not exist, skip write, fileName = %{public}s", fileName.c_str());
        return ERR_ACCOUNT_COMMON_FILE_NOT_EXIST;
    }
    ErrCode ret = fileOperator.InputFileByPathAndContentWithTransaction(GetEtcFilePath(fileName), content,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (ret != ERR_OK) {
        return ret;
    }
    int restoreconRet = Restorecon(GetEtcFilePath(fileName).c_str());
    if (restoreconRet != 0) {
        ACCOUNT_LOGW("Restorecon etc file failed, fileName = %{public}s", fileName.c_str());
        REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_ETC_FILE, restoreconRet,
            "Restorecon etc file failed, fileName = " + fileName);
    }
    return ERR_OK;
}

ErrCode EtcAccountFileUtils::ReadEtcFile(const std::string &fileName, std::string &content)
{
    AccountFileOperator fileOperator;
    return fileOperator.GetFileContentByPath(GetEtcFilePath(fileName), content);
}

ErrCode EtcAccountFileUtils::IsEtcFileExist(const std::string &fileName, bool &isExist)
{
    AccountFileOperator fileOperator;
    ErrCode ret = fileOperator.CheckFileExistence(GetEtcFilePath(fileName));
    if (ret == ERR_ACCOUNT_COMMON_FILE_NOT_EXIST) {
        isExist = false;
        return ERR_OK;
    }
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Check etc file existence failed, ret = %{public}d", ret);
        return ret;
    }
    isExist = true;
    return ERR_OK;
}

ErrCode EtcAccountFileUtils::CreateEtcFaultFlagFile()
{
    AccountFileOperator fileOperator;
    if (!fileOperator.IsExistDir(ETC_FILE_DIR)) {
        ACCOUNT_LOGE("Etc dir not exist, skip create fault flag");
        return ERR_ACCOUNT_COMMON_FILE_NOT_EXIST;
    }
    return fileOperator.InputFileByPathAndContent(GetEtcFilePath(ETC_FAULT_FLAG_NAME), "");
}

ErrCode EtcAccountFileUtils::RemoveEtcFaultFlagFile()
{
    AccountFileOperator fileOperator;
    return fileOperator.DeleteFile(GetEtcFilePath(ETC_FAULT_FLAG_NAME));
}

ErrCode EtcAccountFileUtils::IsEtcFaultFlagFileExist(bool &isExist)
{
    isExist = false;
    AccountFileOperator fileOperator;
    ErrCode ret = fileOperator.CheckFileExistence(GetEtcFilePath(ETC_FAULT_FLAG_NAME));
    if (ret == ERR_ACCOUNT_COMMON_FILE_NOT_EXIST) {
        return ERR_OK;
    }
    if (ret != ERR_OK) {
        ACCOUNT_LOGE("Find etc fault flag file failed, ret = %{public}d", ret);
        return ret;
    }
    isExist = true;
    return ERR_OK;
}

void EtcAccountFileUtils::SplitContentToLines(const std::string &content, std::vector<std::string> &lines)
{
    SplitStringToTokens(content, ETC_LINE_DELIMITER, lines);
    while (!lines.empty() && IsBlankLine(lines.back())) { // drop trailing blank lines
        lines.pop_back();
    }
}

std::string EtcAccountFileUtils::JoinLines(const std::vector<std::string> &lines)
{
    std::string content;
    for (const auto &line : lines) {
        if (IsBlankLine(line)) {
            continue;
        }
        content += line + "\n";
    }
    return content;
}

bool EtcAccountFileUtils::ParsePasswdContent(const std::string &content, std::vector<EtcUserEntry> &entries)
{
    entries.clear();
    std::vector<std::string> lines;
    SplitContentToLines(content, lines);
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string &line = lines[i];
        if (IsBlankLine(line)) {
            continue;
        }
        if (line == U1_FIXED_PASSWD_LINE || line == U0_FIXED_PASSWD_LINE) {
            continue;
        }
        EtcUserEntry entry;
        if (EtcUserEntry::FromString(line, entry) != ERR_OK) {
            ACCOUNT_LOGE("Parse passwd line failed, line index = %{public}zu, length = %{public}zu",
                i, line.size());
            entries.clear();
            return false;
        }
        entries.push_back(entry);
    }
    return true;
}

bool EtcAccountFileUtils::ParseGroupContent(const std::string &content, std::vector<EtcGroupEntry> &entries)
{
    entries.clear();
    std::vector<std::string> lines;
    SplitContentToLines(content, lines);
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string &line = lines[i];
        if (IsBlankLine(line)) {
            continue;
        }
        EtcGroupEntry entry;
        if (EtcGroupEntry::FromString(line, entry) != ERR_OK) {
            ACCOUNT_LOGE("Parse group line failed, line index = %{public}zu, length = %{public}zu",
                i, line.size());
            entries.clear();
            return false;
        }
        entries.push_back(entry);
    }
    return true;
}

bool EtcAccountFileUtils::IsEtcUserNameRepresentable(const std::string &userName)
{
    return userName.find(ETC_LINE_DELIMITER) == std::string::npos &&
        userName.find('\r') == std::string::npos &&
        userName.find(ETC_FIELD_DELIMITER) == std::string::npos;
}

bool EtcAccountFileUtils::IsGroupMemberRepresentable(const std::string &userName)
{
    return IsEtcUserNameRepresentable(userName) && userName.find(ETC_MEMBER_DELIMITER) == std::string::npos;
}

namespace {
void BuildOsAccountEtcEntries(const std::vector<OsAccountInfo> &osAccountInfos,
    std::vector<EtcUserEntry> &osAccountEntries, std::vector<std::string> &normalMembers,
    std::vector<std::string> &adminMembers)
{
    std::vector<OsAccountInfo> scopedInfos;
    for (const auto &info : osAccountInfos) {
        if (info.GetIsCreateCompleted() && !info.GetToBeRemoved()) {
            scopedInfos.push_back(info);
        }
    }
    std::sort(scopedInfos.begin(), scopedInfos.end(),
        [](const OsAccountInfo &a, const OsAccountInfo &b) { return a.GetLocalId() < b.GetLocalId(); });
    std::set<std::string> takenNames = {EtcAccountFileUtils::RESERVED_ETC_USER_NAME};
    for (const auto &info : scopedInfos) {
        std::string userName = info.GetShortName();
        if (userName.empty()) {
            ACCOUNT_LOGI("Skip etc sync for localId %{public}d: empty short name.", info.GetLocalId());
            continue;
        }
        if (takenNames.count(userName) > 0) {
            ACCOUNT_LOGE("Skip etc sync for localId %{public}d: user name already exists.",
                info.GetLocalId());
            REPORT_OS_ACCOUNT_FAIL(info.GetLocalId(), Constants::OPERATION_ETC_FILE,
                ERR_ACCOUNT_COMMON_SHORT_NAME_HAD_EXISTED, "Skip etc sync: user name already exists");
            continue;
        }
        if (!EtcAccountFileUtils::IsEtcUserNameRepresentable(userName)) {
            ACCOUNT_LOGE("Skip etc sync for localId %{public}d: user name is not representable.",
                info.GetLocalId());
            REPORT_OS_ACCOUNT_FAIL(info.GetLocalId(), Constants::OPERATION_ETC_FILE,
                ERR_ACCOUNT_COMMON_INVALID_PARAMETER, "Skip etc sync: user name is not representable");
            continue;
        }
        takenNames.insert(userName);
        osAccountEntries.push_back(EtcUserEntry::FromOsAccountInfo(info, userName));
        if (!EtcAccountFileUtils::IsGroupMemberRepresentable(userName)) {
            ACCOUNT_LOGE("Skip group member %{public}s: contains comma.", userName.c_str());
            continue;
        }
        normalMembers.push_back(userName);
        if (info.GetType() == OsAccountType::ADMIN) {
            adminMembers.push_back(userName);
        }
    }
}

void AppendEtcGroupLine(std::vector<std::string> &groupLines, const std::string &groupName, int32_t gid,
    const std::vector<std::string> &members)
{
    EtcGroupEntry group;
    group.groupName = groupName;
    group.gid = gid;
    group.members = members;
    groupLines.push_back(group.ToString());
}

struct OsAccountEtcEntries {
    std::vector<EtcUserEntry> entries;
    std::vector<std::string> normalMembers;
    std::vector<std::string> adminMembers;
};

void SerializeEtcContent(const OsAccountEtcEntries &osAccount,
    std::string &passwdContent, std::string &groupContent)
{
    std::vector<std::string> passwdLines;
    passwdLines.push_back(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE);
    passwdLines.push_back(EtcAccountFileUtils::U1_FIXED_PASSWD_LINE);
    for (const auto &entry : osAccount.entries) {
        passwdLines.push_back(entry.ToString());
    }
    passwdContent = EtcAccountFileUtils::JoinLines(passwdLines);

    std::vector<std::string> groupLines;
    std::vector<std::string> normalMembers;
    normalMembers.push_back("root");
    normalMembers.insert(normalMembers.end(), osAccount.normalMembers.begin(), osAccount.normalMembers.end());
    std::vector<std::string> adminMembers;
    adminMembers.push_back("root");
    adminMembers.insert(adminMembers.end(), osAccount.adminMembers.begin(), osAccount.adminMembers.end());
    AppendEtcGroupLine(groupLines, EtcAccountFileUtils::NORMAL_GROUP_NAME,
        EtcAccountFileUtils::NORMAL_GROUP_GID, normalMembers);
    AppendEtcGroupLine(groupLines, EtcAccountFileUtils::ADMIN_GROUP_NAME,
        EtcAccountFileUtils::ADMIN_GROUP_GID, adminMembers);
    groupContent = EtcAccountFileUtils::JoinLines(groupLines);
}
} // namespace

ErrCode EtcAccountFileUtils::BuildExpectedEtcContent(const std::vector<OsAccountInfo> &osAccountInfos,
    std::string &passwdContent, std::string &groupContent)
{
    OsAccountEtcEntries osAccount;
    BuildOsAccountEtcEntries(osAccountInfos, osAccount.entries, osAccount.normalMembers, osAccount.adminMembers);

    SerializeEtcContent(osAccount, passwdContent, groupContent);
    return ERR_OK;
}
} // namespace AccountSA
} // namespace OHOS
