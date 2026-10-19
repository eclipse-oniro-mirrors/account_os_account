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

#include <gtest/gtest.h>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <securec.h>
#include "account_error_no.h"
#include "message_parcel.h"
#include "os_account_constants.h"
#include "os_account_info.h"
#include "account_log_wrapper.h"
#define private public
#include "iinner_os_account_manager.h"
#include "account_backup_restore_manager.h"
#include "backup_restore_file_ops.h"
#include "account_file_operator.h"
#ifdef SUPPORT_DOMAIN_ACCOUNTS
#include "inner_domain_account_manager.h"
#include "domain_plugin.h"
#endif
#ifdef SUPPORT_BACKUP_RESTORE
#include "account_mgr_service.h"
#endif
#undef private

using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;

namespace {
const int32_t TEST_LOCAL_ID = 200;
const std::string TEST_LOCAL_NAME = "BackupTestAccount";
const std::string TEST_SHORT_NAME = "BackupShort";
const std::string TEST_PHOTO = "test_photo";
const std::string BACKUP_DIR = "/data/service/el1/public/account";
const int32_t BACKUP_RESTORE_UID = 1089;
}  // namespace

#ifdef SUPPORT_DOMAIN_ACCOUNTS
static int32_t g_a = 0;
static void *g_fakeHandle = &g_a;
static int32_t g_mockPluginErrCode = ERR_OK;
static std::string g_mockDomainName;
static std::string g_mockAccountName;
static std::string g_mockAccountId;
static std::string g_mockServerConfigId;

static void SetMockPluginResult(int32_t errCode, const std::string &domain,
    const std::string &accountName, const std::string &accountId,
    const std::string &serverConfigId)
{
    g_mockPluginErrCode = errCode;
    g_mockDomainName = domain;
    g_mockAccountName = accountName;
    g_mockAccountId = accountId;
    g_mockServerConfigId = serverConfigId;
}

static PluginBusinessError *MockGetAccountInfoByLocalId(
    const int32_t localId, PluginDomainAccountInfo **domainAccountInfo)
{
    auto *error = static_cast<PluginBusinessError *>(malloc(sizeof(PluginBusinessError)));
    if (error == nullptr) {
        return nullptr;
    }
    error->code = g_mockPluginErrCode;
    error->msg.data = nullptr;
    error->msg.length = 0;
    if (g_mockPluginErrCode != ERR_OK || domainAccountInfo == nullptr) {
        *domainAccountInfo = nullptr;
        return error;
    }
    auto *info = static_cast<PluginDomainAccountInfo *>(
        calloc(1, sizeof(PluginDomainAccountInfo)));
    if (info == nullptr) {
        free(error);
        return nullptr;
    }
    auto SetStr = [](const std::string &src, PluginString &dst) {
        dst.length = src.size();
        dst.data = static_cast<char *>(malloc(dst.length + 1));
        if (dst.data != nullptr) {
            errno_t err = memcpy_s(dst.data, dst.length + 1,
                src.c_str(), dst.length + 1);
            if (err != EOK) {
                ACCOUNT_LOGE("memcpy_s failed, err=%{public}d", err);
            }
        }
    };
    SetStr(g_mockDomainName, info->domain);
    SetStr(g_mockAccountName, info->accountName);
    SetStr(g_mockAccountId, info->accountId);
    SetStr(g_mockServerConfigId, info->serverConfigId);
    info->isAuthenticated = 0;
    *domainAccountInfo = info;
    return error;
}

static void LoadMockPlugin()
{
    auto &dm = InnerDomainAccountManager::GetInstance();
    std::lock_guard<std::mutex> lock(dm.libMutex_);
    dm.libHandle_ = g_fakeHandle;
    dm.methodMap_.clear();
    dm.methodMap_[PluginMethodEnum::GET_ACCOUNT_INFO_BY_LOCAL_ID] =
        reinterpret_cast<void *>(MockGetAccountInfoByLocalId);
}

static void UnloadMockPlugin()
{
    auto &dm = InnerDomainAccountManager::GetInstance();
    std::lock_guard<std::mutex> lock(dm.libMutex_);
    dm.libHandle_ = nullptr;
    dm.methodMap_.clear();
}
#endif  // SUPPORT_DOMAIN_ACCOUNTS

class AccountBackupRestoreUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
    static std::string WriteTempFile(const std::string &content);
    static int32_t CreateTestAccount(int32_t localId, const std::string &name);
    static void RemoveTestAccount(int32_t localId);
};

void AccountBackupRestoreUnitTest::SetUpTestCase(void)
{
    mkdir(BACKUP_DIR.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    auto &mgr = IInnerOsAccountManager::GetInstance();
    OsAccountInfo info;
    info.SetLocalId(TEST_LOCAL_ID);
    info.SetLocalName(TEST_LOCAL_NAME);
    info.SetShortName(TEST_SHORT_NAME);
    info.SetType(OsAccountType::NORMAL);
    info.SetPhoto(TEST_PHOTO);
    info.SetIsCreateCompleted(true);
    mgr.CreateOsAccountWithFullInfo(info);
}

void AccountBackupRestoreUnitTest::TearDownTestCase(void)
{
    IInnerOsAccountManager::GetInstance().RemoveOsAccount(TEST_LOCAL_ID);
}

void AccountBackupRestoreUnitTest::SetUp()
{
    auto &mgr = IInnerOsAccountManager::GetInstance();
    mgr.foregroundAccountMap_.EnsureInsert(
        Constants::DEFAULT_DISPLAY_ID, TEST_LOCAL_ID);
}

void AccountBackupRestoreUnitTest::TearDown()
{
    auto &mgr = IInnerOsAccountManager::GetInstance();
    mgr.foregroundAccountMap_.Erase(Constants::DEFAULT_DISPLAY_ID);
}

std::string AccountBackupRestoreUnitTest::WriteTempFile(const std::string &content)
{
    std::string path = "/data/local/tmp/backup_test_" +
        std::to_string(getpid()) + ".json";
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return "";
    }
    ofs << content;
    ofs.close();
    return path;
}

int32_t AccountBackupRestoreUnitTest::CreateTestAccount(
    int32_t localId, const std::string &name)
{
    auto &mgr = IInnerOsAccountManager::GetInstance();
    OsAccountInfo info;
    info.SetLocalId(localId);
    info.SetLocalName(name);
    info.SetType(OsAccountType::NORMAL);
    info.SetIsCreateCompleted(true);
    mgr.CreateOsAccountWithFullInfo(info);
    return localId;
}

void AccountBackupRestoreUnitTest::RemoveTestAccount(int32_t localId)
{
    IInnerOsAccountManager::GetInstance().RemoveOsAccount(localId);
}

/**
 * @tc.name: OnBackup_EmptyLocalIdJson_001
 * @tc.desc: OnBackup with empty localIdJson returns error
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnBackup_EmptyLocalIdJson_001, TestSize.Level1)
{
    MessageParcel data;
    MessageParcel reply;
    data.WriteString("");
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnBackup(data, reply);
    EXPECT_NE(ret, ERR_OK);
}

/**
 * @tc.name: OnBackup_InvalidLocalIdJson_001
 * @tc.desc: OnBackup with invalid localIdJson returns error
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnBackup_InvalidLocalIdJson_001, TestSize.Level1)
{
    MessageParcel data;
    MessageParcel reply;
    data.WriteString("invalid");
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnBackup(data, reply);
    EXPECT_NE(ret, ERR_OK);
}

/**
 * @tc.name: OnBackup_NotExistAccount_001
 * @tc.desc: OnBackup with non-existent account returns error
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnBackup_NotExistAccount_001, TestSize.Level1)
{
    MessageParcel data;
    MessageParcel reply;
    data.WriteString(R"([{"type":"userId", "detail":"99999"}])");
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnBackup(data, reply);
    EXPECT_NE(ret, ERR_OK);
}

/**
 * @tc.name: BuildBackupJson_Success_001
 * @tc.desc: AC-1.2 BuildBackupJson produces JSON with account fields
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, BuildBackupJson_Success_001, TestSize.Level2)
{
    OsAccountInfo osAccountInfo;
    ErrCode errCode = IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        TEST_LOCAL_ID, osAccountInfo);
    ASSERT_EQ(errCode, ERR_OK);
    CJsonUnique jsonInfo;
    ErrCode buildRet = AccountBackupRestoreMgr::GetInstance().BuildBackupJson(
        osAccountInfo, TEST_LOCAL_ID, jsonInfo);
    EXPECT_EQ(buildRet, ERR_OK);
    ASSERT_NE(jsonInfo, nullptr);
    std::string jsonStr = PackJsonToString(jsonInfo);
    EXPECT_FALSE(jsonStr.empty());
    EXPECT_NE(jsonStr.find(TEST_LOCAL_NAME), std::string::npos);
    EXPECT_NE(jsonStr.find(TEST_SHORT_NAME), std::string::npos);
    EXPECT_NE(jsonStr.find("\"localId\""), std::string::npos);
    EXPECT_NE(jsonStr.find("\"type\""), std::string::npos);
    EXPECT_NE(jsonStr.find("\"version\""), std::string::npos);
}

/**
 * @tc.name: OnRestore_InvalidFd_001
 * @tc.desc: OnRestore with invalid fd returns error
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_InvalidFd_001, TestSize.Level1)
{
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(-1);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    EXPECT_NE(ret, ERR_OK);
}

/**
 * @tc.name: OnRestore_EmptyLocalIdJson_001
 * @tc.desc: OnRestore with empty localIdJson returns error
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_EmptyLocalIdJson_001, TestSize.Level1)
{
    int32_t pipefd[2] = {-1, -1};
    ASSERT_EQ(pipe(pipefd), 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(pipefd[0]);
    data.WriteString("");
    close(pipefd[1]);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    EXPECT_NE(ret, ERR_OK);
    close(pipefd[0]);
}

/**
 * @tc.name: OnRestore_InvalidBackupJson_001
 * @tc.desc: AC-2.3 OnRestore with invalid backup JSON
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_InvalidBackupJson_001, TestSize.Level2)
{
    std::string path = WriteTempFile("invalid_json");
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_NE(ret, ERR_OK);
}

/**
 * @tc.name: OnRestore_NotExistAccount_001
 * @tc.desc: AC-2.11 OnRestore with non-existent account
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_NotExistAccount_001, TestSize.Level2)
{
    std::string backupJson = R"({"localId":99998, "localName":"Ghost", )"
        R"("shortName":"g", "type":1, "photo":"", "version":1, "creatorType":0})";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    data.WriteString(R"([{"type":"userId", "detail":"99998"}])");
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_NE(ret, ERR_OK);
}

/**
 * @tc.name: EndToEnd_BuildBackupJson_OnRestore_RoundTrip_001
 * @tc.desc: AC-2.1 End-to-end: BuildBackupJson → write file → OnRestore → verify
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, EndToEnd_BuildBackupJson_OnRestore_RoundTrip_001, TestSize.Level3)
{
    OsAccountInfo osAccountInfo;
    ASSERT_EQ(IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        TEST_LOCAL_ID, osAccountInfo), ERR_OK);
    CJsonUnique jsonInfo;
    ASSERT_EQ(AccountBackupRestoreMgr::GetInstance().BuildBackupJson(
        osAccountInfo, TEST_LOCAL_ID, jsonInfo), ERR_OK);
    ASSERT_NE(jsonInfo, nullptr);
    std::string backupJson = PackJsonToString(jsonInfo);
    ASSERT_FALSE(backupJson.empty());
    ASSERT_NE(backupJson.find(TEST_LOCAL_NAME), std::string::npos);
    auto &mgr = IInnerOsAccountManager::GetInstance();
    osAccountInfo.SetLocalName("ModifiedName");
    mgr.GetFileController().UpdateOsAccount(osAccountInfo);
    std::string tempPath = WriteTempFile(backupJson);
    ASSERT_FALSE(tempPath.empty());
    int32_t fd = open(tempPath.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(tempPath.c_str());
    EXPECT_EQ(ret, ERR_OK);
    std::string replyStr = reply.ReadString();
    EXPECT_NE(replyStr.find("\"errorCode\":\"0\""), std::string::npos);
    OsAccountInfo restoredInfo;
    ASSERT_EQ(mgr.QueryOsAccountById(TEST_LOCAL_ID, restoredInfo), ERR_OK);
    EXPECT_EQ(restoredInfo.GetLocalName(), TEST_LOCAL_NAME);
}

#ifdef SUPPORT_DOMAIN_ACCOUNTS

/**
 * @tc.name: DomainConsistency_PluginNotFound_001
 * @tc.desc: AC-6.2 Plugin returns not found, domainInfo cleared
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, DomainConsistency_PluginNotFound_001, TestSize.Level2)
{
    LoadMockPlugin();
    DomainAccountInfo domainInfo;
    domainInfo.domain_ = "test.example.com";
    domainInfo.accountName_ = "testaccount";
    domainInfo.accountId_ = "testid";
    SetMockPluginResult(ERR_JS_ACCOUNT_NOT_FOUND, "", "", "", "");
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().CheckDomainAccountConsistency(
        TEST_LOCAL_ID, domainInfo, false);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_TRUE(domainInfo.IsEmpty());
    UnloadMockPlugin();
}

/**
 * @tc.name: DomainConsistency_SameAccount_001
 * @tc.desc: AC-6.1 Plugin returns same account, detail fields synced
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, DomainConsistency_SameAccount_001, TestSize.Level2)
{
    LoadMockPlugin();
    DomainAccountInfo domainInfo;
    domainInfo.domain_ = "test.example.com";
    domainInfo.accountName_ = "testaccount";
    domainInfo.accountId_ = "testid";
    domainInfo.serverConfigId_ = "old_config_id";
    SetMockPluginResult(ERR_OK, "test.example.com", "testaccount",
        "testid", "new_config_id");
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().CheckDomainAccountConsistency(
        TEST_LOCAL_ID, domainInfo, false);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(domainInfo.serverConfigId_, "new_config_id");
    EXPECT_EQ(domainInfo.domain_, "test.example.com");
    UnloadMockPlugin();
}

/**
 * @tc.name: DomainConsistency_DifferentAccount_Restore_001
 * @tc.desc: Plugin returns different account during restore, local data overwritten with plugin data (self-healing)
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, DomainConsistency_DifferentAccount_Restore_001, TestSize.Level2)
{
    LoadMockPlugin();
    DomainAccountInfo domainInfo;
    domainInfo.domain_ = "original.example.com";
    domainInfo.accountName_ = "originalaccount";
    domainInfo.accountId_ = "originalid";
    domainInfo.serverConfigId_ = "original_config";
    SetMockPluginResult(ERR_OK, "different.example.com",
        "differentaccount", "differentid", "config1");
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().CheckDomainAccountConsistency(
        TEST_LOCAL_ID, domainInfo, true);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(domainInfo.domain_, "different.example.com");
    EXPECT_EQ(domainInfo.accountName_, "differentaccount");
    EXPECT_EQ(domainInfo.accountId_, "differentid");
    EXPECT_EQ(domainInfo.serverConfigId_, "config1");
    UnloadMockPlugin();
}

/**
 * @tc.name: DomainConsistency_NoPlugin_Skip_001
 * @tc.desc: No plugin loaded, consistency check skipped
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, DomainConsistency_NoPlugin_Skip_001, TestSize.Level2)
{
    DomainAccountInfo domainInfo;
    domainInfo.domain_ = "test.example.com";
    domainInfo.accountName_ = "testaccount";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().CheckDomainAccountConsistency(
        TEST_LOCAL_ID, domainInfo, false);
    EXPECT_EQ(ret, ERR_OK);
}

#endif  // SUPPORT_DOMAIN_ACCOUNTS

/**
 * @tc.name: OnBackup_NonForeground_Succeeds_001
 * @tc.desc: OnBackup succeeds even when account is not in foreground
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnBackup_NonForeground_Succeeds_001, TestSize.Level2)
{
    auto &mgr = IInnerOsAccountManager::GetInstance();
    mgr.foregroundAccountMap_.Erase(Constants::DEFAULT_DISPLAY_ID);
    MessageParcel data;
    MessageParcel reply;
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnBackup(data, reply);
    EXPECT_EQ(ret, ERR_OK);
    mgr.foregroundAccountMap_.EnsureInsert(
        Constants::DEFAULT_DISPLAY_ID, TEST_LOCAL_ID);
}

/**
 * @tc.name: OnRestore_NonForeground_Succeeds_001
 * @tc.desc: OnRestore succeeds even when account is not in foreground
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_NonForeground_Succeeds_001, TestSize.Level2)
{
    auto &mgr = IInnerOsAccountManager::GetInstance();
    mgr.foregroundAccountMap_.Erase(Constants::DEFAULT_DISPLAY_ID);
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    mgr.foregroundAccountMap_.EnsureInsert(
        Constants::DEFAULT_DISPLAY_ID, TEST_LOCAL_ID);
}

/**
 * @tc.name: OnRestore_NameConflict_001
 * @tc.desc: AC-2.7 OnRestore with duplicate localName returns error
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_NameConflict_001, TestSize.Level2)
{
    int32_t conflictId = CreateTestAccount(201, TEST_LOCAL_NAME);
    ASSERT_GT(conflictId, 0);
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_NE(ret, ERR_OK);
    RemoveTestAccount(conflictId);
}

/**
 * @tc.name: OnRestore_ShortNameConflict_001
 * @tc.desc: AC-2.7 OnRestore with duplicate shortName returns error
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_ShortNameConflict_001, TestSize.Level2)
{
    int32_t conflictId = CreateTestAccount(205, "ShortNameHolder");
    ASSERT_GT(conflictId, 0);
    OsAccountInfo conflictInfo;
    conflictInfo.SetLocalId(conflictId);
    conflictInfo.SetLocalName("ShortNameHolder");
    conflictInfo.SetShortName(TEST_SHORT_NAME);
    conflictInfo.SetType(OsAccountType::NORMAL);
    conflictInfo.SetIsCreateCompleted(true);
    IInnerOsAccountManager::GetInstance().GetFileController().UpdateOsAccount(
        conflictInfo);
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_NE(ret, ERR_OK);
    RemoveTestAccount(conflictId);
}

/**
 * @tc.name: OnRestore_TypeMismatch_001
 * @tc.desc: AC-2.8 OnRestore with different type does not block
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_TypeMismatch_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":0, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    OsAccountInfo info;
    IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        TEST_LOCAL_ID, info);
    EXPECT_EQ(static_cast<int32_t>(info.GetType()),
        static_cast<int32_t>(OsAccountType::NORMAL));
}

#ifdef SUPPORT_DOMAIN_ACCOUNTS

/**
 * @tc.name: OnRestore_DomainBindingConflict_SkippedWhenNoPlugin_001
 * @tc.desc: AC-2.5 Restore backup to account with domain binding — no plugin, domain check skipped
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_DomainBindingConflict_SkippedWhenNoPlugin_001, TestSize.Level2)
{
    int32_t domainAccountId = CreateTestAccount(202, "DomainAccount");
    ASSERT_GT(domainAccountId, 0);
    OsAccountInfo domainAccount;
    domainAccount.SetLocalId(domainAccountId);
    domainAccount.SetLocalName("DomainAccount");
    domainAccount.SetType(OsAccountType::NORMAL);
    domainAccount.SetIsCreateCompleted(true);
    DomainAccountInfo domainInfo;
    domainInfo.domain_ = "test.example.com";
    domainInfo.accountName_ = "domainuser";
    domainInfo.accountId_ = "domainid";
    domainAccount.SetDomainInfo(domainInfo);
    IInnerOsAccountManager::GetInstance().GetFileController().UpdateOsAccount(
        domainAccount);
    std::string backupJson = std::string("{\"localId\":") +
        std::to_string(domainAccountId) +
        ", \"localName\":\"DomainAccount\", \"shortName\":\"d\", "
        "\"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(domainAccountId) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    RemoveTestAccount(domainAccountId);
}

/**
 * @tc.name: OnRestore_DomainAlreadyBoundByOther_SkippedWhenNoPlugin_001
 * @tc.desc: AC-2.12 Restore backup — no plugin, domain already bound check skipped
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_DomainAlreadyBoundByOther_SkippedWhenNoPlugin_001, TestSize.Level3)
{
    int32_t accountA = CreateTestAccount(203, "AccountA");
    ASSERT_GT(accountA, 0);
    OsAccountInfo infoA;
    infoA.SetLocalId(accountA);
    infoA.SetLocalName("AccountA");
    infoA.SetType(OsAccountType::NORMAL);
    infoA.SetIsCreateCompleted(true);
    DomainAccountInfo domainX;
    domainX.domain_ = "shared.example.com";
    domainX.accountName_ = "shareduser";
    domainX.accountId_ = "sharedid";
    infoA.SetDomainInfo(domainX);
    IInnerOsAccountManager::GetInstance().GetFileController().UpdateOsAccount(
        infoA);
    int32_t accountB = CreateTestAccount(204, "AccountB");
    ASSERT_GT(accountB, 0);
    std::string backupJson = std::string("{\"localId\":") +
        std::to_string(accountB) +
        ", \"localName\":\"AccountB\", \"shortName\":\"b\", "
        "\"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(accountB) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    RemoveTestAccount(accountA);
    RemoveTestAccount(accountB);
}

#endif  // SUPPORT_DOMAIN_ACCOUNTS

/**
 * @tc.name: OnRestore_OldVersionData_MissingOptionalFields_001
 * @tc.desc: AC-2.2 Restore with missing optional fields uses defaults
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_OldVersionData_MissingOptionalFields_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"OldVersionName\", \"shortName\":\"OldShort\", "
        "\"type\":1, \"photo\":\"\", \"version\":0, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    OsAccountInfo info;
    IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        TEST_LOCAL_ID, info);
    EXPECT_EQ(info.GetLocalName(), "OldVersionName");
    EXPECT_EQ(info.GetShortName(), TEST_SHORT_NAME);
    DomainAccountInfo domainInfo;
    info.GetDomainInfo(domainInfo);
    EXPECT_TRUE(domainInfo.domain_.empty());
}

/**
 * @tc.name: OnRestore_FutureVersionData_UnknownFields_001
 * @tc.desc: AC-2.2 Restore with unknown future fields ignores them
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_FutureVersionData_UnknownFields_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"test\", \"version\":1, \"creatorType\":0, "
        "\"futureField\":\"xxx\", \"extraData\":123}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    OsAccountInfo info;
    IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        TEST_LOCAL_ID, info);
    EXPECT_EQ(info.GetLocalName(), TEST_LOCAL_NAME);
}

/**
 * @tc.name: OnRestore_DoesNotChangeActivationState_001
 * @tc.desc: AC-2.9 Restore does not change isActivated/isVerified
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_DoesNotChangeActivationState_001, TestSize.Level2)
{
    OsAccountInfo beforeInfo;
    IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        TEST_LOCAL_ID, beforeInfo);
    bool beforeActivated = beforeInfo.GetIsActived();
    bool beforeVerified = beforeInfo.GetIsVerified();
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    OsAccountInfo afterInfo;
    IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        TEST_LOCAL_ID, afterInfo);
    EXPECT_EQ(afterInfo.GetIsActived(), beforeActivated);
    EXPECT_EQ(afterInfo.GetIsVerified(), beforeVerified);
}

/**
 * @tc.name: OnRestore_ConstraintsReinitializedByType_001
 * @tc.desc: AC-2.13 Restore reinitializes constraints by type, not from backup
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_ConstraintsReinitializedByType_001, TestSize.Level2)
{
    std::vector<std::string> expectedConstraints;
    IInnerOsAccountManager::GetInstance().GetFileController().GetConstraintsByType(
        OsAccountType::NORMAL, expectedConstraints);
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0, "
        "\"constraints\":[\"fake_constraint_1\", \"fake_constraint_2\"]}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    OsAccountInfo info;
    IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        TEST_LOCAL_ID, info);
    auto actualConstraints = info.GetConstraints();
    EXPECT_EQ(actualConstraints, expectedConstraints);
    auto it = std::find(actualConstraints.begin(), actualConstraints.end(),
        "fake_constraint_1");
    EXPECT_EQ(it, actualConstraints.end());
}

/**
 * @tc.name: OnRestore_NoCrossUserInterference_001
 * @tc.desc: AC-2.4 Restore to account B does not affect account A
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_NoCrossUserInterference_001, TestSize.Level3)
{
    int32_t accountA = CreateTestAccount(206, "AccountA_Original");
    ASSERT_GT(accountA, 0);
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"RestoredName\", \"shortName\":\"RS\", "
        "\"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    OsAccountInfo infoA;
    IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        accountA, infoA);
    EXPECT_EQ(infoA.GetLocalName(), "AccountA_Original");
    OsAccountInfo infoB;
    IInnerOsAccountManager::GetInstance().QueryOsAccountById(
        TEST_LOCAL_ID, infoB);
    EXPECT_EQ(infoB.GetLocalName(), "RestoredName");
    RemoveTestAccount(accountA);
}

/**
 * @tc.name: OnBackup_EndToEnd_ReturnsBackupData_001
 * @tc.desc: AC-1.1 OnBackup returns serialized data in reply (fd + replyCode)
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnBackup_EndToEnd_ReturnsBackupData_001, TestSize.Level2)
{
    MessageParcel data;
    MessageParcel reply;
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnBackup(data, reply);
    EXPECT_EQ(ret, ERR_OK);
    int32_t fd = reply.ReadFileDescriptor();
    EXPECT_GE(fd, 0);
    std::string replyCode = reply.ReadString();
    EXPECT_NE(replyCode.find("\"errorCode\":\"0\""), std::string::npos);
    off_t fileSize = lseek(fd, 0, SEEK_END);
    EXPECT_GT(fileSize, 0);
    lseek(fd, 0, SEEK_SET);
    std::string backupData(fileSize, '\0');
    ssize_t bytesRead = read(fd, &backupData[0], fileSize);
    EXPECT_EQ(bytesRead, fileSize);
    close(fd);
    EXPECT_NE(backupData.find("\"localId\""), std::string::npos);
    EXPECT_NE(backupData.find("\"localName\""), std::string::npos);
    EXPECT_NE(backupData.find("\"shortName\""), std::string::npos);
    EXPECT_NE(backupData.find("\"type\""), std::string::npos);
    EXPECT_NE(backupData.find("\"photo\""), std::string::npos);
    EXPECT_NE(backupData.find("\"version\""), std::string::npos);
    EXPECT_NE(backupData.find("\"creatorType\""), std::string::npos);
    EXPECT_EQ(backupData.find("\"domainInfo\""), std::string::npos);
    EXPECT_EQ(backupData.find("\"constraints\""), std::string::npos);
}

/**
 * @tc.name: OnRestore_LocalIdMismatch_ReturnsError_001
 * @tc.desc: localId consistency check - backup localId != data localId returns 12300002
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_LocalIdMismatch_ReturnsError_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":999, \"") +
        "\"localName\":\"Mismatch\", \"shortName\":\"MS\", "
        "\"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: OnRestore_MissingField_ReturnsError_001
 * @tc.desc: ParseBackupJson strong validation - missing field returns 12300002
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_MissingField_ReturnsError_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"MissingShortName\", "
        "\"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: OnRestore_MissingLocalId_ReturnsError_001
 * @tc.desc: T-02 ParseBackupJson strong validation - missing localId
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_MissingLocalId_ReturnsError_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localName\":\"") + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: OnRestore_MissingLocalName_ReturnsError_001
 * @tc.desc: T-02 ParseBackupJson strong validation - missing localName
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_MissingLocalName_ReturnsError_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: OnRestore_MissingType_ReturnsError_001
 * @tc.desc: T-02 ParseBackupJson strong validation - missing type
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_MissingType_ReturnsError_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: OnRestore_MissingPhoto_ReturnsError_001
 * @tc.desc: T-02 ParseBackupJson strong validation - missing photo
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_MissingPhoto_ReturnsError_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: OnRestore_MissingVersion_ReturnsError_001
 * @tc.desc: T-02 ParseBackupJson strong validation - missing version
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_MissingVersion_ReturnsError_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\"}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: OnRestore_MissingDomainInfo_Succeeds_001
 * @tc.desc: Backup JSON without domainInfo succeeds; domainInfo is sourced from plugin at restore time.
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_MissingDomainInfo_Succeeds_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: OnRestore_MissingCreatorType_ReturnsError_001
 * @tc.desc: T-02 ParseBackupJson strong validation - missing creatorType
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_MissingCreatorType_ReturnsError_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: OnExtension_NonBackupRestoreUid_Rejected_001
 * @tc.desc: T-03 OnExtension rejects non-1089 UID caller
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnExtension_NonBackupRestoreUid_Rejected_001, TestSize.Level2)
{
#ifdef SUPPORT_BACKUP_RESTORE
    MessageParcel data;
    MessageParcel reply;
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountMgrService::GetInstance().OnExtension("backup", data, reply);
    EXPECT_EQ(ret, ERR_JS_PERMISSION_DENIED);
#endif
}

/**
 * @tc.name: OnExtension_UnknownExtension_ReturnsError_001
 * @tc.desc: T-03 OnExtension unknown extension returns error
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnExtension_UnknownExtension_ReturnsError_001, TestSize.Level2)
{
#ifdef SUPPORT_BACKUP_RESTORE
    MessageParcel data;
    MessageParcel reply;
    int32_t ret = AccountMgrService::GetInstance().OnExtension("unknown", data, reply);
    EXPECT_EQ(ret, ERR_JS_PERMISSION_DENIED);
#endif
}

#ifdef SUPPORT_BACKUP_RESTORE
/**
 * @tc.name: OnExtension_Backup_DispatchToHandler_001
 * @tc.desc: OnExtension with UID=1089 dispatches "backup" to AccountBackupRestoreMgr::OnBackup
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnExtension_Backup_DispatchToHandler_001, TestSize.Level2)
{
    uid_t oldUid = getuid();
    setuid(BACKUP_RESTORE_UID);
    MessageParcel data;
    MessageParcel reply;
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountMgrService::GetInstance().OnExtension("backup", data, reply);
    setuid(oldUid);
    EXPECT_EQ(ret, ERR_OK);
    (void)reply.ReadFileDescriptor();
    std::string replyStr = reply.ReadString();
    EXPECT_NE(replyStr.find("\"errorCode\":\"0\""), std::string::npos);
}

/**
 * @tc.name: OnExtension_Restore_DispatchToHandler_001
 * @tc.desc: OnExtension with UID=1089 dispatches "restore" to AccountBackupRestoreMgr::OnRestore
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnExtension_Restore_DispatchToHandler_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"" + TEST_LOCAL_NAME + "\"" +
        ", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    uid_t oldUid = getuid();
    setuid(BACKUP_RESTORE_UID);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountMgrService::GetInstance().OnExtension("restore", data, reply);
    setuid(oldUid);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_OK);
    std::string replyStr = reply.ReadString();
    EXPECT_NE(replyStr.find("\"errorCode\":\"0\""), std::string::npos);
}

/**
 * @tc.name: OnExtension_UnknownExtension_ReturnsInvalidParameter_001
 * @tc.desc: EX-14: OnExtension with UID=1089 and unknown extension returns ERR_JS_INVALID_PARAMETER
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnExtension_UnknownExtension_ReturnsInvalidParameter_001, TestSize.Level2)
{
    uid_t oldUid = getuid();
    setuid(BACKUP_RESTORE_UID);
    MessageParcel data;
    MessageParcel reply;
    int32_t ret = AccountMgrService::GetInstance().OnExtension("unknown", data, reply);
    setuid(oldUid);
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}
#endif

/**
 * @tc.name: OnRestore_EmptyLocalName_ReturnsError_001
 * @tc.desc: T-06 ValidateBackupName - empty localName
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_EmptyLocalName_ReturnsError_001, TestSize.Level2)
{
    std::string backupJson = std::string("{\"localId\":") + std::to_string(TEST_LOCAL_ID) +
        ", \"localName\":\"\", \"shortName\":\"" + TEST_SHORT_NAME + "\"" +
        ", \"type\":1, \"photo\":\"\", \"version\":1, \"creatorType\":0}";
    std::string path = WriteTempFile(backupJson);
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: OnRestore_EmptyBackupFile_ReturnsError_001
 * @tc.desc: T-07 ReadBackupData - empty file content
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, OnRestore_EmptyBackupFile_ReturnsError_001, TestSize.Level2)
{
    std::string path = WriteTempFile("");
    ASSERT_FALSE(path.empty());
    int32_t fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    MessageParcel data;
    MessageParcel reply;
    data.WriteFileDescriptor(fd);
    std::string json = R"([{"type":"userId", "detail":")" +
        std::to_string(TEST_LOCAL_ID) + R"("}])";
    data.WriteString(json);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().OnRestore(data, reply);
    close(fd);
    remove(path.c_str());
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: BuildBackupJson_GetPhotoFailed_ContinuesWithDefault_001
 * @tc.desc: T-08 BuildBackupJson - GetPhotoById fails, photo uses default
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, BuildBackupJson_GetPhotoFailed_ContinuesWithDefault_001, TestSize.Level2)
{
    int32_t testId = CreateTestAccount(301, "NoPhotoAccount");
    ASSERT_GT(testId, 0);
    OsAccountInfo info;
    IInnerOsAccountManager::GetInstance().QueryOsAccountById(testId, info);
    CJsonUnique jsonOut;
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().BuildBackupJson(info, testId, jsonOut);
    EXPECT_EQ(ret, ERR_OK);
    ASSERT_NE(jsonOut, nullptr);
    std::string result = PackJsonToString(jsonOut);
    EXPECT_NE(result.find("\"localName\""), std::string::npos);
    EXPECT_NE(result.find("\"photo\""), std::string::npos);
    RemoveTestAccount(testId);
}

/**
 * @tc.name: BackupRestoreFileOps_CreateBackup_Success_001
 * @tc.desc: AC-5.3 CreateBackup creates .{userId}_backup/ with .complete
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, BackupRestoreFileOps_CreateBackup_Success_001, TestSize.Level2)
{
    AccountFileOperator fileOp;
    std::string backupDir = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup";
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
    ErrCode ret = BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_TRUE(fileOp.IsExistDir(backupDir));
    std::string completePath = backupDir + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_COMPLETE_FILE;
    EXPECT_TRUE(fileOp.IsExistFile(completePath));
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
}

/**
 * @tc.name: BackupRestoreFileOps_IsBackupComplete_True_001
 * @tc.desc: AC-5.3 IsBackupComplete returns ERR_OK when .complete exists
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, BackupRestoreFileOps_IsBackupComplete_True_001, TestSize.Level2)
{
    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);
    ErrCode ret = BackupRestoreFileOps::IsBackupComplete(TEST_LOCAL_ID);
    EXPECT_EQ(ret, ERR_OK);
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
}

/**
 * @tc.name: BackupRestoreFileOps_IsBackupComplete_NoComplete_False_001
 * @tc.desc: AC-5.4 IsBackupComplete fails when .complete missing
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, BackupRestoreFileOps_IsBackupComplete_NoComplete_False_001, TestSize.Level2)
{
    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);
    std::string completePath = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup" +
        Constants::PATH_SEPARATOR + Constants::RESTORE_BACKUP_COMPLETE_FILE;
    remove(completePath.c_str());
    ErrCode ret = BackupRestoreFileOps::IsBackupComplete(TEST_LOCAL_ID);
    EXPECT_NE(ret, ERR_OK);
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
}

/**
 * @tc.name: BackupRestoreFileOps_IsBackupComplete_NoDir_False_001
 * @tc.desc: AC-5.4 IsBackupComplete fails when backup dir not exist
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, BackupRestoreFileOps_IsBackupComplete_NoDir_False_001, TestSize.Level2)
{
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
    ErrCode ret = BackupRestoreFileOps::IsBackupComplete(TEST_LOCAL_ID);
    EXPECT_NE(ret, ERR_OK);
}

/**
 * @tc.name: BackupRestoreFileOps_DeleteBackup_Idempotent_001
 * @tc.desc: AC-5.6 DeleteBackup is idempotent (no error when dir not exist)
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, BackupRestoreFileOps_DeleteBackup_Idempotent_001, TestSize.Level2)
{
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
    ErrCode ret = BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: RollbackFromBackupUnlocked_NoBackup_ReturnsOK_001
 * @tc.desc: AC-5.5 RollbackFromBackupUnlocked returns OK when no backup exists
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, RollbackFromBackupUnlocked_NoBackup_ReturnsOK_001, TestSize.Level2)
{
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().RollbackFromBackupUnlocked(TEST_LOCAL_ID);
    EXPECT_EQ(ret, ERR_OK);
}

/**
 * @tc.name: RollbackFromBackupUnlocked_RestoresAccountInfo_001
 * @tc.desc: AC-5.3 RollbackFromBackupUnlocked restores account_info from backup
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, RollbackFromBackupUnlocked_RestoresAccountInfo_001, TestSize.Level2)
{
    auto &mgr = IInnerOsAccountManager::GetInstance();
    OsAccountInfo original;
    mgr.QueryOsAccountById(TEST_LOCAL_ID, original);
    original.SetLocalName("BeforeRestore");
    mgr.GetFileController().UpdateOsAccount(original);

    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);

    OsAccountInfo modified;
    mgr.QueryOsAccountById(TEST_LOCAL_ID, modified);
    modified.SetLocalName("AfterRestore");
    mgr.GetFileController().UpdateOsAccount(modified);

    ErrCode ret = AccountBackupRestoreMgr::GetInstance().RollbackFromBackupUnlocked(TEST_LOCAL_ID);
    EXPECT_EQ(ret, ERR_OK);

    OsAccountInfo restored;
    mgr.QueryOsAccountById(TEST_LOCAL_ID, restored);
    EXPECT_EQ(restored.GetLocalName(), "BeforeRestore");

    mgr.GetFileController().UpdateOsAccount(original);
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
}

/**
 * @tc.name: RollbackFromBackupUnlocked_DeletesBackupAfterRollback_001
 * @tc.desc: AC-5.3 RollbackFromBackupUnlocked deletes backup after rollback
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, RollbackFromBackupUnlocked_DeletesBackupAfterRollback_001, TestSize.Level2)
{
    AccountFileOperator fileOp;
    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);
    std::string backupDir = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup";
    EXPECT_TRUE(fileOp.IsExistDir(backupDir));

    ErrCode ret = AccountBackupRestoreMgr::GetInstance().RollbackFromBackupUnlocked(TEST_LOCAL_ID);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_FALSE(fileOp.IsExistDir(backupDir));
}

/**
 * @tc.name: RollbackFromBackupUnlocked_CorruptedJson_FailsAndDeletes_001
 * @tc.desc: AC-5.5 RollbackFromBackupUnlocked with corrupted backup JSON fails and deletes backup
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, RollbackFromBackupUnlocked_CorruptedJson_FailsAndDeletes_001, TestSize.Level2)
{
    AccountFileOperator fileOp;
    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);

    std::string backupInfoPath = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup" +
        Constants::PATH_SEPARATOR + Constants::USER_INFO_FILE_NAME;
    fileOp.InputFileByPathAndContent(backupInfoPath, "{corrupted json}");

    ErrCode ret = AccountBackupRestoreMgr::GetInstance().RollbackFromBackupUnlocked(TEST_LOCAL_ID);
    EXPECT_NE(ret, ERR_OK);

    std::string backupDir = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup";
    EXPECT_FALSE(fileOp.IsExistDir(backupDir));
}

/**
 * @tc.name: RollbackFromBackupUnlocked_IncompleteBackup_SkipsAndCleans_001
 * @tc.desc: AC-5.4 RollbackFromBackupUnlocked with incomplete backup (no .complete) skips and cleans
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, RollbackFromBackupUnlocked_IncompleteBackup_SkipsAndCleans_001, TestSize.Level2)
{
    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);
    std::string completePath = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup" +
        Constants::PATH_SEPARATOR + Constants::RESTORE_BACKUP_COMPLETE_FILE;
    remove(completePath.c_str());

    ErrCode ret = AccountBackupRestoreMgr::GetInstance().RollbackFromBackupUnlocked(TEST_LOCAL_ID);
    EXPECT_EQ(ret, ERR_OK);

    ErrCode existRet = BackupRestoreFileOps::IsBackupComplete(TEST_LOCAL_ID);
    EXPECT_NE(existRet, ERR_OK);
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
}

/**
 * @tc.name: RollbackFromBackupUnlocked_PhotoRestored_001
 * @tc.desc: AC-5.3 RollbackFromBackupUnlocked restores photo from backup
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, RollbackFromBackupUnlocked_PhotoRestored_001, TestSize.Level2)
{
    auto &mgr = IInnerOsAccountManager::GetInstance();
    OsAccountInfo original;
    mgr.QueryOsAccountById(TEST_LOCAL_ID, original);

    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);

    mgr.SetOsAccountProfilePhoto(TEST_LOCAL_ID, "modified_photo");

    ErrCode ret = AccountBackupRestoreMgr::GetInstance().RollbackFromBackupUnlocked(TEST_LOCAL_ID);
    EXPECT_EQ(ret, ERR_OK);

    mgr.GetFileController().UpdateOsAccount(original);
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
}

/**
 * @tc.name: CheckAndRecoverRestore_NoBackup_NoOp_001
 * @tc.desc: AC-5.3 CheckAndRecoverRestore does nothing when no backup exists
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, CheckAndRecoverRestore_NoBackup_NoOp_001, TestSize.Level2)
{
    AccountFileOperator fileOp;
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
    IInnerOsAccountManager::GetInstance().CheckAndRecoverRestore();
    EXPECT_FALSE(fileOp.IsExistDir(Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup"));
}

/**
 * @tc.name: ParseLocalIdJson_NotArray_ReturnsError_001
 * @tc.desc: ParseLocalIdJson with valid JSON that is not an array returns error
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_NotArray_ReturnsError_001, TestSize.Level1)
{
    int32_t localId = -1;
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson("{}", localId);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(localId, -1);
}

/**
 * @tc.name: ParseLocalIdJson_EmptyArray_ReturnsError_001
 * @tc.desc: ParseLocalIdJson with empty array returns error (userId not found)
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_EmptyArray_ReturnsError_001, TestSize.Level1)
{
    int32_t localId = -1;
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson("[]", localId);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(localId, -1);
}

/**
 * @tc.name: ParseLocalIdJson_ItemNotObject_Skipped_001
 * @tc.desc: ParseLocalIdJson skips non-object array elements and finds valid userId
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_ItemNotObject_Skipped_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([123, {"type":"userId","detail":"100"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(localId, 100);
}

/**
 * @tc.name: ParseLocalIdJson_TypeMissing_Skipped_001
 * @tc.desc: ParseLocalIdJson skips items missing the type field
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_TypeMissing_Skipped_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([{"detail":"100"}, {"type":"userId","detail":"101"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(localId, 101);
}

/**
 * @tc.name: ParseLocalIdJson_TypeNotString_Skipped_001
 * @tc.desc: ParseLocalIdJson skips items where type is not a string
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_TypeNotString_Skipped_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([{"type":123,"detail":"100"}, {"type":"userId","detail":"102"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(localId, 102);
}

/**
 * @tc.name: ParseLocalIdJson_TypeNotUserId_Skipped_001
 * @tc.desc: ParseLocalIdJson skips items with type != "userId"
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_TypeNotUserId_Skipped_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([{"type":"other","detail":"100"}, {"type":"userId","detail":"103"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(localId, 103);
}

/**
 * @tc.name: ParseLocalIdJson_DetailMissing_Skipped_001
 * @tc.desc: ParseLocalIdJson skips userId items missing the detail field
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_DetailMissing_Skipped_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([{"type":"userId"}, {"type":"userId","detail":"104"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(localId, 104);
}

/**
 * @tc.name: ParseLocalIdJson_DetailNotString_Skipped_001
 * @tc.desc: ParseLocalIdJson skips userId items where detail is not a string
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_DetailNotString_Skipped_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([{"type":"userId","detail":123}, {"type":"userId","detail":"105"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(localId, 105);
}

/**
 * @tc.name: ParseLocalIdJson_DetailNotNumber_ReturnsError_001
 * @tc.desc: ParseLocalIdJson returns error when detail is not a valid number
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_DetailNotNumber_ReturnsError_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([{"type":"userId","detail":"abc"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(localId, -1);
}

/**
 * @tc.name: ParseLocalIdJson_DetailTrailingChars_ReturnsError_001
 * @tc.desc: ParseLocalIdJson returns error when detail has trailing non-numeric chars
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_DetailTrailingChars_ReturnsError_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([{"type":"userId","detail":"100abc"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(localId, -1);
}

/**
 * @tc.name: ParseLocalIdJson_LocalIdBelowRange_ReturnsError_001
 * @tc.desc: ParseLocalIdJson returns error when localId < START_USER_ID
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_LocalIdBelowRange_ReturnsError_001, TestSize.Level1)
{
    int32_t localId = -1;
    int32_t belowRange = Constants::START_USER_ID - 1;
    std::string json = R"([{"type":"userId","detail":")" +
        std::to_string(belowRange) + R"("}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(localId, -1);
}

/**
 * @tc.name: ParseLocalIdJson_LocalIdAboveRange_ReturnsError_001
 * @tc.desc: ParseLocalIdJson returns error when localId > MAX_CREATABLE_USER_ID
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_LocalIdAboveRange_ReturnsError_001, TestSize.Level1)
{
    int32_t localId = -1;
    int32_t aboveRange = Constants::MAX_CREATABLE_USER_ID + 1;
    std::string json = R"([{"type":"userId","detail":")" +
        std::to_string(aboveRange) + R"("}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(localId, -1);
}

/**
 * @tc.name: ParseLocalIdJson_UserIdNotFound_ReturnsError_001
 * @tc.desc: ParseLocalIdJson returns error when no userId entry exists in array
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_UserIdNotFound_ReturnsError_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([{"type":"other","detail":"100"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(localId, -1);
}

/**
 * @tc.name: ParseLocalIdJson_ValidInput_Success_001
 * @tc.desc: ParseLocalIdJson succeeds with valid userId entry
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseLocalIdJson_ValidInput_Success_001, TestSize.Level1)
{
    int32_t localId = -1;
    std::string json = R"([{"type":"userId","detail":"100"}])";
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseLocalIdJson(json, localId);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(localId, 100);
}

/**
 * @tc.name: ValidateBackupName_LocalNameTooLong_ReturnsError_001
 * @tc.desc: ValidateBackupName rejects localName exceeding LOCAL_NAME_MAX_SIZE
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ValidateBackupName_LocalNameTooLong_ReturnsError_001, TestSize.Level1)
{
    OsAccountInfo backupInfo;
    backupInfo.SetLocalName(std::string(Constants::LOCAL_NAME_MAX_SIZE, 'x'));
    backupInfo.SetType(OsAccountType::NORMAL);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().ValidateBackupName(TEST_LOCAL_ID, backupInfo);
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: ValidateBackupName_ShortNameSpecialChar_ReturnsError_001
 * @tc.desc: ValidateBackupName rejects shortName containing special characters
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ValidateBackupName_ShortNameSpecialChar_ReturnsError_001, TestSize.Level1)
{
    OsAccountInfo backupInfo;
    backupInfo.SetLocalName("ValidName");
    backupInfo.SetShortName("bad<name");
    backupInfo.SetType(OsAccountType::NORMAL);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().ValidateBackupName(TEST_LOCAL_ID, backupInfo);
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

/**
 * @tc.name: ValidateBackupName_ShortNameDot_ReturnsError_001
 * @tc.desc: ValidateBackupName rejects shortName equal to "."
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ValidateBackupName_ShortNameDot_ReturnsError_001, TestSize.Level1)
{
    OsAccountInfo backupInfo;
    backupInfo.SetLocalName("ValidName");
    backupInfo.SetShortName(".");
    backupInfo.SetType(OsAccountType::NORMAL);
    int32_t ret = AccountBackupRestoreMgr::GetInstance().ValidateBackupName(TEST_LOCAL_ID, backupInfo);
    EXPECT_EQ(ret, ERR_JS_INVALID_PARAMETER);
}

#ifdef SUPPORT_DOMAIN_ACCOUNTS

#endif // SUPPORT_DOMAIN_ACCOUNTS

/**
 * @tc.name: ParseBackupJson_NullRoot_ReturnsFalse_001
 * @tc.desc: ParseBackupJson returns false when jsonRoot is nullptr
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseBackupJson_NullRoot_ReturnsFalse_001, TestSize.Level1)
{
    OsAccountInfo backupInfo;
    bool ret = AccountBackupRestoreMgr::GetInstance().ParseBackupJson(nullptr, backupInfo);
    EXPECT_FALSE(ret);
}

/**
 * @tc.name: ShouldSkipDomainCheck_NoPlugin_ReturnsTrue_001
 * @tc.desc: ShouldSkipDomainCheck returns true when no plugin is loaded
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ShouldSkipDomainCheck_NoPlugin_ReturnsTrue_001, TestSize.Level1)
{
    bool ret = AccountBackupRestoreMgr::GetInstance().ShouldSkipDomainCheck();
    EXPECT_TRUE(ret);
}

/**
 * @tc.name: ParseBackupForRollback_EmptyContent_ReturnsError_001
 * @tc.desc: ParseBackupForRollback returns error when backup file content is empty
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseBackupForRollback_EmptyContent_ReturnsError_001, TestSize.Level1)
{
    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);
    AccountFileOperator fileOp;
    std::string backupInfoPath = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup" +
        Constants::PATH_SEPARATOR + Constants::USER_INFO_FILE_NAME;
    fileOp.InputFileByPathAndContent(backupInfoPath, "");
    OsAccountInfo backupInfo;
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseBackupForRollback(TEST_LOCAL_ID, backupInfo);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_FILE_READ_FAILED);
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
}

/**
 * @tc.name: ParseBackupForRollback_LocalIdMismatch_ReturnsError_001
 * @tc.desc: ParseBackupForRollback returns error when backup localId differs from expected
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, ParseBackupForRollback_LocalIdMismatch_ReturnsError_001, TestSize.Level1)
{
    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);
    AccountFileOperator fileOp;
    std::string backupInfoPath = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup" +
        Constants::PATH_SEPARATOR + Constants::USER_INFO_FILE_NAME;
    std::string mismatchJson = R"({"localId":99999,"localName":"Mismatch",)"
        R"("isCreateCompleted":true})";
    fileOp.InputFileByPathAndContent(backupInfoPath, mismatchJson);
    OsAccountInfo backupInfo;
    ErrCode ret = AccountBackupRestoreMgr::GetInstance().ParseBackupForRollback(TEST_LOCAL_ID, backupInfo);
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    BackupRestoreFileOps::DeleteBackup(TEST_LOCAL_ID);
}

/**
 * @tc.name: CheckAndRecoverRestore_WithBackup_RollsBack_001
 * @tc.desc: AC-5.3 CheckAndRecoverRestore recovers user with backup
 * @tc.type: FUNC
 */
HWTEST_F(AccountBackupRestoreUnitTest, CheckAndRecoverRestore_WithBackup_RollsBack_001, TestSize.Level2)
{
    auto &mgr = IInnerOsAccountManager::GetInstance();
    OsAccountInfo original;
    mgr.QueryOsAccountById(TEST_LOCAL_ID, original);
    original.SetLocalName("CrashRecoveryTest");
    mgr.GetFileController().UpdateOsAccount(original);

    AccountFileOperator fileOp;
    BackupRestoreFileOps::CreateBackup(TEST_LOCAL_ID);

    OsAccountInfo modified;
    mgr.QueryOsAccountById(TEST_LOCAL_ID, modified);
    modified.SetLocalName("ModifiedAfterCrash");
    mgr.GetFileController().UpdateOsAccount(modified);

    IInnerOsAccountManager::GetInstance().isNeedRestoreRecover_.store(true);
    IInnerOsAccountManager::GetInstance().CheckAndRecoverRestore();

    OsAccountInfo restored;
    mgr.QueryOsAccountById(TEST_LOCAL_ID, restored);
    EXPECT_EQ(restored.GetLocalName(), "CrashRecoveryTest");

    std::string backupDir = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR +
        Constants::RESTORE_BACKUP_DIR_PREFIX + std::to_string(TEST_LOCAL_ID) + "_backup";
    EXPECT_FALSE(fileOp.IsExistDir(backupDir));

    mgr.GetFileController().UpdateOsAccount(original);
}
