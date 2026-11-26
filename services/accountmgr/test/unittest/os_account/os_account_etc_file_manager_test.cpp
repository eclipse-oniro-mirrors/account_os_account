/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "account_error_no.h"
#include "os_account_constants.h"
#define private public
#include "os_account_etc_file_manager.h"
#include "os_account_control_file_manager.h"
#undef private
#include "os_account_etc_file.h"

#ifndef ACCOUNT_TEST
#error "os_account_etc_file_manager_test.cpp must be built with -DACCOUNT_TEST"
#endif

namespace OHOS {
namespace AccountSA {
using namespace testing::ext;

namespace {
const int32_t TEST_ID_ONE = 101;
const int32_t TEST_ID_TWO = 102;
const int32_t TEST_ID_MISSING = 10500;
const std::string TEST_NAME_ONE = "alice";
const std::string TEST_NAME_TWO = "bob";
const std::string TEST_NAME_ADMIN = "adm1";
const std::string TEST_NAME_PINYIN = "zhangsan";
const std::string TEST_NAME_CHINESE = "\xe5\xbc\xa0\xe4\xb8\x89"; // UTF-8 for 张三
const std::string TEST_ETC_DIR = EtcAccountFileUtils::ETC_FILE_DIR;
const int32_t EXPECTED_ETC_RETRY_TIMES = 3;
const int32_t EXPECTED_ETC_SECOND_ATTEMPT_COUNT = 2;
const int32_t ASYNC_TASK_WAIT_TIMEOUT_MS = 5000;
const int32_t ASYNC_POLL_INTERVAL_MS = 10;

std::shared_ptr<OsAccountControlFileManager> g_etcControlManager = std::make_shared<OsAccountControlFileManager>();

std::string EtcFilePath(const std::string &fileName)
{
    return TEST_ETC_DIR + fileName;
}
}  // namespace

class OsAccountEtcFileManagerTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

protected:
    std::shared_ptr<OsAccountEtcFileManager> etcManager_;
    OsAccountInfo MakeAccount(int32_t id, const std::string &shortName,
        OsAccountType type = OsAccountType::NORMAL);
    void SeedEtcFiles();
    void CleanEtcFiles();
    std::string ReadPasswd();
    std::string ReadGroup();
    bool FlagExists();
    bool PasswdExists();
    bool HasUserRow(const std::string &passwdContent, int32_t uid, const std::string &name);
    int CountUserRows(const std::string &passwdContent, const std::string &name);
    bool HasGroupMember(const std::string &groupContent, const std::string &groupName,
        const std::string &member);
    void ResetAccountStore();
    bool WaitForAsyncState(const std::function<bool()> &ready);
};

void OsAccountEtcFileManagerTest::SetUpTestCase()
{
    system(("rm -rf " + std::string(EtcAccountFileUtils::ETC_FILE_DIR)).c_str());
    system(("mkdir -p " + std::string(EtcAccountFileUtils::ETC_FILE_DIR)).c_str());
#ifdef ACCOUNT_TEST
    system(("rm -rf " + Constants::USER_INFO_BASE).c_str());
#endif
    g_etcControlManager->Init();
    bool passwdExist = false;
    EtcAccountFileUtils::IsEtcFileExist(EtcAccountFileUtils::PASSWD_FILE_NAME, passwdExist);
    ASSERT_TRUE(passwdExist);
}

void OsAccountEtcFileManagerTest::TearDownTestCase()
{
    system(("rm -rf " + std::string(EtcAccountFileUtils::ETC_FILE_DIR)).c_str());
#ifdef ACCOUNT_TEST
    system(("rm -rf " + Constants::USER_INFO_BASE).c_str());
#endif
}

void OsAccountEtcFileManagerTest::SetUp()
{
    CleanEtcFiles();
    etcManager_ = std::make_shared<OsAccountEtcFileManager>(g_etcControlManager);
}

void OsAccountEtcFileManagerTest::TearDown()
{
    ResetAccountStore();
}

void OsAccountEtcFileManagerTest::ResetAccountStore()
{
#ifdef ACCOUNT_TEST
    system(("rm -rf " + Constants::USER_INFO_BASE).c_str());
#endif
    g_etcControlManager = std::make_shared<OsAccountControlFileManager>();
    g_etcControlManager->Init();
    etcManager_ = std::make_shared<OsAccountEtcFileManager>(g_etcControlManager);
}

bool OsAccountEtcFileManagerTest::WaitForAsyncState(const std::function<bool()> &ready)
{
    auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(ASYNC_TASK_WAIT_TIMEOUT_MS);
    while (std::chrono::steady_clock::now() < deadline) {
        if (ready()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(ASYNC_POLL_INTERVAL_MS));
    }
    return ready();
}

OsAccountInfo OsAccountEtcFileManagerTest::MakeAccount(int32_t id, const std::string &shortName,
    OsAccountType type)
{
    OsAccountInfo info;
    info.SetLocalId(id);
    info.SetLocalName("name" + std::to_string(id));
    info.SetShortName(shortName);
    info.SetType(type);
    info.SetIsCreateCompleted(true);
    return info;
}

void OsAccountEtcFileManagerTest::SeedEtcFiles()
{
    std::string passwd = std::string(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE) + "\n" +
        std::string(EtcAccountFileUtils::U1_FIXED_PASSWD_LINE) + "\n";
    std::string group = "root:x:0:\nnormal:x:20:\nadmin:x:80:\n";
    EtcAccountFileUtils::WriteEtcFile(EtcAccountFileUtils::PASSWD_FILE_NAME, passwd);
    EtcAccountFileUtils::WriteEtcFile(EtcAccountFileUtils::GROUP_FILE_NAME, group);
}

void OsAccountEtcFileManagerTest::CleanEtcFiles()
{
    system(("rm -rf " + EtcFilePath(EtcAccountFileUtils::PASSWD_FILE_NAME) + " " +
        EtcFilePath(EtcAccountFileUtils::GROUP_FILE_NAME) + " " +
        EtcFilePath(EtcAccountFileUtils::ETC_FAULT_FLAG_NAME) + " " +
        EtcFilePath(EtcAccountFileUtils::ETC_MIGRATION_FLAG_NAME)).c_str());
}

std::string OsAccountEtcFileManagerTest::ReadPasswd()
{
    std::string content;
    EtcAccountFileUtils::ReadEtcFile(EtcAccountFileUtils::PASSWD_FILE_NAME, content);
    return content;
}

std::string OsAccountEtcFileManagerTest::ReadGroup()
{
    std::string content;
    EtcAccountFileUtils::ReadEtcFile(EtcAccountFileUtils::GROUP_FILE_NAME, content);
    return content;
}

bool OsAccountEtcFileManagerTest::FlagExists()
{
    bool isExist = false;
    EtcAccountFileUtils::IsEtcFaultFlagFileExist(isExist);
    return isExist;
}

bool OsAccountEtcFileManagerTest::PasswdExists()
{
    bool isExist = false;
    EtcAccountFileUtils::IsEtcFileExist(EtcAccountFileUtils::PASSWD_FILE_NAME, isExist);
    return isExist;
}

bool OsAccountEtcFileManagerTest::HasUserRow(const std::string &passwdContent, int32_t uid,
    const std::string &name)
{
    std::vector<EtcUserEntry> entries;
    if (!EtcAccountFileUtils::ParsePasswdContent(passwdContent, entries)) {
        return false;
    }
    for (const auto &entry : entries) {
        if (entry.uid == uid && entry.userName == name) {
            return true;
        }
    }
    return false;
}

int OsAccountEtcFileManagerTest::CountUserRows(const std::string &passwdContent, const std::string &name)
{
    std::vector<EtcUserEntry> entries;
    if (!EtcAccountFileUtils::ParsePasswdContent(passwdContent, entries)) {
        return -1;
    }
    int count = 0;
    for (const auto &entry : entries) {
        if (entry.userName == name) {
            ++count;
        }
    }
    return count;
}

bool OsAccountEtcFileManagerTest::HasGroupMember(const std::string &groupContent,
    const std::string &groupName, const std::string &member)
{
    std::vector<EtcGroupEntry> entries;
    if (!EtcAccountFileUtils::ParseGroupContent(groupContent, entries)) {
        return false;
    }
    for (const auto &group : entries) {
        if (group.groupName != groupName) {
            continue;
        }
        return std::find(group.members.begin(), group.members.end(), member) != group.members.end();
    }
    return false;
}

/**
 * @tc.name: ControlInitTriggersRecover001
 * @tc.desc: Control manager Init calls CheckAndRecoverEtcFile, corrupted passwd rebuilt.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ControlInitTriggersRecover001, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    ASSERT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
    CleanEtcFiles();
    ASSERT_FALSE(PasswdExists());
    static std::shared_ptr<OsAccountControlFileManager> controlManager =
        std::make_shared<OsAccountControlFileManager>();
    controlManager->Init();
    ASSERT_TRUE(PasswdExists());
    EXPECT_NE(ReadPasswd().find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: SyncEtcForCreate001
 * @tc.desc: Sync etc files for a new normal account, passwd row added and grouped into normal group.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate001, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    EXPECT_TRUE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    std::string group = ReadGroup();
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ONE));
    EXPECT_FALSE(HasGroupMember(group, EtcAccountFileUtils::ADMIN_GROUP_NAME, TEST_NAME_ONE));
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: SyncEtcForCreate002
 * @tc.desc: Sync etc files for a new admin account, grouped into both normal and admin groups.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate002, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ADMIN,
        OsAccountType::ADMIN)), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_TRUE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ADMIN));
    std::string group = ReadGroup();
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ADMIN));
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::ADMIN_GROUP_NAME, TEST_NAME_ADMIN));
}

/**
 * @tc.name: SyncEtcForCreate003
 * @tc.desc: Sync etc files twice with different short names, old row replaced and group member renamed.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate003, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_TWO)), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_TRUE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_TWO));
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    std::string group = ReadGroup();
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_TWO));
    EXPECT_FALSE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ONE));
}

/**
 * @tc.name: SyncEtcForCreate004
 * @tc.desc: Sync etc files with empty short name while no row exists, no change to files.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate004, TestSize.Level1)
{
    SeedEtcFiles();
    std::string passwdBefore = ReadPasswd();
    std::string groupBefore = ReadGroup();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, "")), ERR_OK);
    EXPECT_EQ(ReadPasswd(), passwdBefore);
    EXPECT_EQ(ReadGroup(), groupBefore);
}

/**
 * @tc.name: SyncEtcForCreate005
 * @tc.desc: Sync etc files with empty short name while row exists, row and group member removed.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate005, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    ASSERT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, "")), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    std::string group = ReadGroup();
    EXPECT_FALSE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ONE));
}

/**
 * @tc.name: SyncEtcForCreate006
 * @tc.desc: Sync etc files with a short name already taken by another account, new row skipped.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate006, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_TWO, TEST_NAME_ONE)), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_EQ(CountUserRows(passwd, TEST_NAME_ONE), 1);
    EXPECT_TRUE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_TWO, TEST_NAME_ONE));
}

/**
 * @tc.name: SyncEtcForCreate007
 * @tc.desc: Sync etc files with a short name containing newline, row skipped as unrepresentable.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate007, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, "bad\nname")), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_ONE, "bad\nname"));
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
}

/**
 * @tc.name: SyncEtcForCreate008
 * @tc.desc: Sync etc files when passwd file is missing, files rebuilt then new account row upserted.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate008, TestSize.Level1)
{
    ASSERT_FALSE(PasswdExists());
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U1_FIXED_PASSWD_LINE), std::string::npos);
    EXPECT_TRUE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    std::string group = ReadGroup();
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ONE));
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: SyncEtcForRemove001
 * @tc.desc: Sync etc files for a removed account, passwd row and group member removed.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForRemove001, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    ASSERT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
    ASSERT_EQ(etcManager_->SyncEtcForRemove(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    std::string group = ReadGroup();
    EXPECT_FALSE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ONE));
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: SyncEtcForRemove002
 * @tc.desc: Sync etc files for a removed account without an etc row, no change and no fault flag.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForRemove002, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    std::string passwdBefore = ReadPasswd();
    ASSERT_EQ(etcManager_->SyncEtcForRemove(MakeAccount(TEST_ID_TWO, TEST_NAME_TWO)), ERR_OK);
    EXPECT_EQ(ReadPasswd(), passwdBefore);
    EXPECT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: SyncEtcForRemoveCorruptedGroup001
 * @tc.desc: Remove sync with a corrupted group file self-heals via inline rebuild: row removed,
 *           group canonicalized, no fault flag left.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForRemoveCorruptedGroup001, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    ASSERT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
    // Corrupt the group file so parsing fails; the unified delete path rebuilds instead of failing.
    ASSERT_EQ(EtcAccountFileUtils::WriteEtcFile(EtcAccountFileUtils::GROUP_FILE_NAME,
        "normal:x:20:root\nbadline\n"), ERR_OK);
    ASSERT_EQ(etcManager_->SyncEtcForRemove(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    std::string group = ReadGroup();
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME,
        EtcAccountFileUtils::RESERVED_ETC_USER_NAME));
    EXPECT_FALSE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ONE));
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: FlushEtcFilesSync001
 * @tc.desc: Flush etc files synchronously, files rebuilt from account list and fault flag cleared.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, FlushEtcFilesSync001, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    ASSERT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
    ASSERT_EQ(etcManager_->FlushEtcFilesSync(), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: CheckAndRecover001
 * @tc.desc: Check and recover etc files when content matches the account list, files stay unchanged.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, CheckAndRecover001, TestSize.Level1)
{
    etcManager_->FlushEtcFilesSync();
    std::string passwdBefore = ReadPasswd();
    std::string groupBefore = ReadGroup();
    ASSERT_NE(passwdBefore.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    ASSERT_FALSE(FlagExists());
    etcManager_->CheckAndRecover();
    EXPECT_EQ(ReadPasswd(), passwdBefore);
    EXPECT_EQ(ReadGroup(), groupBefore);
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: CheckAndRecover002
 * @tc.desc: Check and recover etc files when fault flag exists, files rebuilt synchronously.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, CheckAndRecover002, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    ASSERT_EQ(EtcAccountFileUtils::CreateEtcFaultFlagFile(), ERR_OK);
    etcManager_->CheckAndRecover();
    ASSERT_TRUE(PasswdExists());
    ASSERT_FALSE(FlagExists());
    std::string passwd = ReadPasswd();
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
}

/**
 * @tc.name: CheckAndRecover003
 * @tc.desc: Check and recover etc files when passwd file is missing, files rebuilt synchronously.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, CheckAndRecover003, TestSize.Level1)
{
    ASSERT_EQ(EtcAccountFileUtils::WriteEtcFile(EtcAccountFileUtils::GROUP_FILE_NAME,
        "root:x:0:\nnormal:x:20:\nadmin:x:80:\n"), ERR_OK);
    ASSERT_FALSE(PasswdExists());
    etcManager_->CheckAndRecover();
    ASSERT_TRUE(PasswdExists());
    ASSERT_FALSE(FlagExists());
    std::string passwd = ReadPasswd();
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U1_FIXED_PASSWD_LINE), std::string::npos);
}

/**
 * @tc.name: SyncEtcForCreate009
 * @tc.desc: Sync etc files with a short name containing ':', row skipped and files stay parseable.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate009, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, "bad:name")), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_ONE, "bad:name"));
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_TWO, TEST_NAME_TWO)), ERR_OK);
    EXPECT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_TWO, TEST_NAME_TWO));
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: SyncEtcForCreate010
 * @tc.desc: Sync etc files with the reserved name "root", row skipped and files unchanged.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate010, TestSize.Level1)
{
    SeedEtcFiles();
    std::string passwdBefore = ReadPasswd();
    std::string groupBefore = ReadGroup();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(
        MakeAccount(TEST_ID_ONE, EtcAccountFileUtils::RESERVED_ETC_USER_NAME)), ERR_OK);
    EXPECT_EQ(ReadPasswd(), passwdBefore);
    EXPECT_EQ(ReadGroup(), groupBefore);
    EXPECT_EQ(CountUserRows(ReadPasswd(), EtcAccountFileUtils::RESERVED_ETC_USER_NAME), 0);
}

/**
 * @tc.name: SyncEtcForCreate011
 * @tc.desc: Sync etc files with a corrupted passwd file, files rebuilt then new row upserted.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate011, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(EtcAccountFileUtils::WriteEtcFile(EtcAccountFileUtils::PASSWD_FILE_NAME, "badline\n"),
        ERR_OK);
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    EXPECT_TRUE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    std::string group = ReadGroup();
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ONE));
    EXPECT_FALSE(FlagExists());
}

/**
 * @tc.name: PartialWriteFailure001
 * @tc.desc: Group file write fails, fault flag remains and the next sync recovers via rebuild.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, PartialWriteFailure001, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    // Block the group file with a directory so WriteEtcContents fails after passwd is written.
    system(("rm -f " + EtcFilePath(EtcAccountFileUtils::GROUP_FILE_NAME)).c_str());
    system(("mkdir -p " + EtcFilePath(EtcAccountFileUtils::GROUP_FILE_NAME)).c_str());
    ASSERT_NE(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_TWO, TEST_NAME_TWO)), ERR_OK);
    EXPECT_TRUE(FlagExists());
    system(("rm -rf " + EtcFilePath(EtcAccountFileUtils::GROUP_FILE_NAME)).c_str());
    ASSERT_EQ(etcManager_->FlushEtcFilesSync(), ERR_OK);
    ASSERT_TRUE(PasswdExists());
    ASSERT_FALSE(FlagExists());
    // The rebuild is list-driven, so rows not backed by the account list are gone.
    std::string passwd = ReadPasswd();
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    EXPECT_FALSE(HasUserRow(passwd, TEST_ID_TWO, TEST_NAME_TWO));
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
}

/**
 * @tc.name: FlushEtcFilesSyncDirMissing001
 * @tc.desc: Flush etc file with the etc dir missing, sync flush fails once and no files appear.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, FlushEtcFilesSyncDirMissing001, TestSize.Level1)
{
    SeedEtcFiles();
    system(("rm -rf " + std::string(EtcAccountFileUtils::ETC_FILE_DIR)).c_str());
    EXPECT_NE(etcManager_->FlushEtcFilesSync(), ERR_OK); // no retry: single attempt reports failure
    EXPECT_FALSE(PasswdExists());
    EXPECT_FALSE(FlagExists());
    // Restore the directory so later cases can write again.
    system(("mkdir -p " + std::string(EtcAccountFileUtils::ETC_FILE_DIR)).c_str());
}

/**
 * @tc.name: SyncEtcForCreate012
 * @tc.desc: Fault flag exists with healthy files, SyncEtcForCreate rebuilds then upserts row.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate012, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(EtcAccountFileUtils::CreateEtcFaultFlagFile(), ERR_OK);
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    EXPECT_FALSE(FlagExists());
    EXPECT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
}

/**
 * @tc.name: SyncEtcForCreate013
 * @tc.desc: Etc dir missing, SyncEtcForCreate fails because rebuild cannot write.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreate013, TestSize.Level1)
{
    system(("rm -rf " + std::string(EtcAccountFileUtils::ETC_FILE_DIR)).c_str());
    ErrCode ret = etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE));
    EXPECT_NE(ret, ERR_OK);
    EXPECT_FALSE(PasswdExists());
    system(("mkdir -p " + std::string(EtcAccountFileUtils::ETC_FILE_DIR)).c_str());
}

/**
 * @tc.name: EtcMigrationFlagNotExist001
 * @tc.desc: Migration flag is absent by default, the existence check succeeds with false.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, EtcMigrationFlagNotExist001, TestSize.Level1)
{
    bool isExist = true;
    ASSERT_EQ(EtcAccountFileUtils::IsEtcMigrationFlagExist(isExist), ERR_OK);
    EXPECT_FALSE(isExist);
}

/**
 * @tc.name: EtcMigrationFlagCreateAndExist001
 * @tc.desc: Create the migration flag, then the existence check reports it.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, EtcMigrationFlagCreateAndExist001, TestSize.Level1)
{
    ASSERT_EQ(EtcAccountFileUtils::CreateEtcMigrationFlagFile(), ERR_OK);
    bool isExist = false;
    ASSERT_EQ(EtcAccountFileUtils::IsEtcMigrationFlagExist(isExist), ERR_OK);
    EXPECT_TRUE(isExist);
}

/**
 * @tc.name: EtcMigrationFlagCreateBlockedByDir001
 * @tc.desc: Creating the migration flag fails when the flag path is occupied by a directory.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, EtcMigrationFlagCreateBlockedByDir001, TestSize.Level1)
{
    std::string flagPath = EtcFilePath(EtcAccountFileUtils::ETC_MIGRATION_FLAG_NAME);
    system(("mkdir -p " + flagPath).c_str());
    EXPECT_NE(EtcAccountFileUtils::CreateEtcMigrationFlagFile(), ERR_OK);
    system(("rm -rf " + flagPath).c_str());
}

/**
 * @tc.name: EtcMigrationFlagExistCheckBlockedByDir001
 * @tc.desc: Existence check fails with an error when the flag path is a directory.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, EtcMigrationFlagExistCheckBlockedByDir001, TestSize.Level1)
{
    std::string flagPath = EtcFilePath(EtcAccountFileUtils::ETC_MIGRATION_FLAG_NAME);
    system(("mkdir -p " + flagPath).c_str());
    bool isExist = true;
    EXPECT_NE(EtcAccountFileUtils::IsEtcMigrationFlagExist(isExist), ERR_OK);
    system(("rm -rf " + flagPath).c_str());
}

/**
 * @tc.name: SyncEtcForCreateSkipDuringMigration001
 * @tc.desc: Create sync is skipped with success while migration is in progress, no row appears.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForCreateSkipDuringMigration001, TestSize.Level1)
{
    SeedEtcFiles();
    std::string passwdBefore = ReadPasswd();
    etcManager_->migrationInProgress_.store(true);
    EXPECT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    EXPECT_EQ(ReadPasswd(), passwdBefore);
    EXPECT_FALSE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
}

/**
 * @tc.name: SyncEtcForRemoveSkipDuringMigration001
 * @tc.desc: Remove sync is skipped with success while migration is in progress, row stays.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, SyncEtcForRemoveSkipDuringMigration001, TestSize.Level1)
{
    SeedEtcFiles();
    ASSERT_EQ(etcManager_->SyncEtcForCreate(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    ASSERT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
    etcManager_->migrationInProgress_.store(true);
    EXPECT_EQ(etcManager_->SyncEtcForRemove(MakeAccount(TEST_ID_ONE, TEST_NAME_ONE)), ERR_OK);
    EXPECT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_ONE));
}

/**
 * @tc.name: MigrateAndSyncWeakPtrExpired001
 * @tc.desc: Migration with an expired control-manager weak pointer is a no-op with success.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, MigrateAndSyncWeakPtrExpired001, TestSize.Level1)
{
    std::weak_ptr<OsAccountControlFileManager> expiredWeak;
    {
        auto temp = std::make_shared<OsAccountControlFileManager>();
        expiredWeak = temp;
    } // temp released: the weak pointer is expired
    auto orphanManager = std::make_shared<OsAccountEtcFileManager>(expiredWeak);
    EXPECT_EQ(orphanManager->MigrateAndSync(), ERR_OK);
    EXPECT_FALSE(orphanManager->migrationInProgress_.load());
    bool flagExist = true;
    ASSERT_EQ(EtcAccountFileUtils::IsEtcMigrationFlagExist(flagExist), ERR_OK);
    EXPECT_FALSE(flagExist);
}

/**
 * @tc.name: MigrateAndSyncSuccess001
 * @tc.desc: Migration renames a non-compliant short name, rebuilds etc files and writes the flag.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, MigrateAndSyncSuccess001, TestSize.Level1)
{
    OsAccountInfo account = MakeAccount(TEST_ID_ONE, TEST_NAME_CHINESE);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(account), ERR_OK);
    EXPECT_EQ(etcManager_->MigrateAndSync(), ERR_OK);
    EXPECT_FALSE(etcManager_->migrationInProgress_.load());
    OsAccountInfo migrated;
    ASSERT_EQ(g_etcControlManager->GetOsAccountInfoById(TEST_ID_ONE, migrated), ERR_OK);
    EXPECT_EQ(migrated.GetShortName(), TEST_NAME_PINYIN); // 张三 → zhangsan
    EXPECT_EQ(migrated.GetOldShortName(), TEST_NAME_CHINESE);
    EXPECT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_PINYIN));
    std::string group = ReadGroup();
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_PINYIN));
    bool flagExist = false;
    ASSERT_EQ(EtcAccountFileUtils::IsEtcMigrationFlagExist(flagExist), ERR_OK);
    EXPECT_TRUE(flagExist);
}

/**
 * @tc.name: MigrateAndSyncGetAccountListFailed001
 * @tc.desc: Migration fails and resets progress when the account list file is corrupted.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, MigrateAndSyncGetAccountListFailed001, TestSize.Level1)
{
    system(("echo not_json > " + Constants::ACCOUNT_LIST_FILE_JSON_PATH).c_str());
    EXPECT_EQ(etcManager_->MigrateAndSync(), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_FALSE(etcManager_->migrationInProgress_.load());
    bool flagExist = true;
    ASSERT_EQ(EtcAccountFileUtils::IsEtcMigrationFlagExist(flagExist), ERR_OK);
    EXPECT_FALSE(flagExist);
}

/**
 * @tc.name: MigrateAndSyncApplyRenamePlanFailed001
 * @tc.desc: Migration fails when the rename plan references an account whose file is gone.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, MigrateAndSyncApplyRenamePlanFailed001, TestSize.Level1)
{
    OsAccountInfo holder = MakeAccount(TEST_ID_TWO, TEST_NAME_TWO);
    OsAccountInfo legacy = MakeAccount(TEST_ID_MISSING, TEST_NAME_CHINESE);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(holder), ERR_OK);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(legacy), ERR_OK);
    std::string storeBase = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR;
    std::string infoFile = Constants::PATH_SEPARATOR + Constants::USER_INFO_FILE_NAME;
    // Slot TEST_ID_TWO now serves content whose localId is TEST_ID_MISSING; dropping that
    // account's own file makes the rename plan (keyed by the embedded id) impossible to re-read.
    system(("cp " + storeBase + std::to_string(TEST_ID_MISSING) + infoFile + " " + storeBase +
        std::to_string(TEST_ID_TWO) + infoFile).c_str());
    system(("rm -rf " + storeBase + std::to_string(TEST_ID_MISSING)).c_str());
    EXPECT_EQ(etcManager_->MigrateAndSync(), ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR);
    EXPECT_FALSE(etcManager_->migrationInProgress_.load());
}

/**
 * @tc.name: MigrateAndSyncFlushEtcFilesFailed001
 * @tc.desc: Migration fails when etc files cannot be rebuilt because the etc dir is missing.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, MigrateAndSyncFlushEtcFilesFailed001, TestSize.Level1)
{
    system(("rm -rf " + TEST_ETC_DIR).c_str());
    EXPECT_EQ(etcManager_->MigrateAndSync(), ERR_ACCOUNT_COMMON_FILE_NOT_EXIST);
    EXPECT_FALSE(etcManager_->migrationInProgress_.load());
    bool flagExist = true;
    ASSERT_EQ(EtcAccountFileUtils::IsEtcMigrationFlagExist(flagExist), ERR_OK);
    EXPECT_FALSE(flagExist);
    system(("mkdir -p " + TEST_ETC_DIR).c_str());
}

/**
 * @tc.name: MigrateAndSyncCreateFlagFailed001
 * @tc.desc: Migration fails at the last step when the flag path is blocked by a directory;
 *           rename and rebuild have already been applied.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, MigrateAndSyncCreateFlagFailed001, TestSize.Level1)
{
    OsAccountInfo account = MakeAccount(TEST_ID_ONE, TEST_NAME_CHINESE);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(account), ERR_OK);
    std::string flagPath = EtcFilePath(EtcAccountFileUtils::ETC_MIGRATION_FLAG_NAME);
    system(("mkdir -p " + flagPath).c_str());
    EXPECT_NE(etcManager_->MigrateAndSync(), ERR_OK);
    EXPECT_FALSE(etcManager_->migrationInProgress_.load());
    EXPECT_TRUE(HasUserRow(ReadPasswd(), TEST_ID_ONE, TEST_NAME_PINYIN)); // rebuild happened
    system(("rm -rf " + flagPath).c_str());
}

/**
 * @tc.name: ApplyRenamePlanWeakPtrExpired001
 * @tc.desc: Applying a rename plan with an expired weak pointer succeeds without any work.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ApplyRenamePlanWeakPtrExpired001, TestSize.Level1)
{
    std::weak_ptr<OsAccountControlFileManager> expiredWeak;
    {
        auto temp = std::make_shared<OsAccountControlFileManager>();
        expiredWeak = temp;
    }
    auto orphanManager = std::make_shared<OsAccountEtcFileManager>(expiredWeak);
    // Non-empty plan proves the early return: proceeding would fail re-reading TEST_ID_ONE.
    std::vector<OsAccountInfo> plan = { MakeAccount(TEST_ID_ONE, TEST_NAME_ONE) };
    EXPECT_EQ(orphanManager->ApplyRenamePlan(plan), ERR_OK);
}

/**
 * @tc.name: ApplyRenamePlanRereadFailed001
 * @tc.desc: Applying a rename plan fails when the account no longer exists in the store.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ApplyRenamePlanRereadFailed001, TestSize.Level1)
{
    std::vector<OsAccountInfo> plan = { MakeAccount(TEST_ID_MISSING, TEST_NAME_ONE) };
    EXPECT_EQ(etcManager_->ApplyRenamePlan(plan), ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR);
}

/**
 * @tc.name: ApplyRenamePlanAlreadyCompliantSkipped001
 * @tc.desc: A plan entry whose current short name is already compliant is skipped, store untouched.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ApplyRenamePlanAlreadyCompliantSkipped001, TestSize.Level1)
{
    OsAccountInfo account = MakeAccount(TEST_ID_ONE, TEST_NAME_ONE); // "alice" is compliant
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(account), ERR_OK);
    std::vector<OsAccountInfo> plan = { MakeAccount(TEST_ID_ONE, TEST_NAME_TWO) };
    EXPECT_EQ(etcManager_->ApplyRenamePlan(plan), ERR_OK);
    OsAccountInfo current;
    ASSERT_EQ(g_etcControlManager->GetOsAccountInfoById(TEST_ID_ONE, current), ERR_OK);
    EXPECT_EQ(current.GetShortName(), TEST_NAME_ONE);
    EXPECT_TRUE(current.GetOldShortName().empty());
}

/**
 * @tc.name: ApplyRenamePlanConflictResolved001
 * @tc.desc: A planned name taken by another account meanwhile is regenerated with a free suffix.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ApplyRenamePlanConflictResolved001, TestSize.Level1)
{
    OsAccountInfo legacy = MakeAccount(TEST_ID_ONE, TEST_NAME_CHINESE);
    OsAccountInfo holder = MakeAccount(TEST_ID_TWO, TEST_NAME_ONE);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(legacy), ERR_OK);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(holder), ERR_OK);
    // Plan was built when "alice" was free for account TEST_ID_ONE; TEST_ID_TWO owns it now.
    std::vector<OsAccountInfo> plan = { MakeAccount(TEST_ID_ONE, TEST_NAME_ONE) };
    EXPECT_EQ(etcManager_->ApplyRenamePlan(plan), ERR_OK);
    OsAccountInfo migrated;
    ASSERT_EQ(g_etcControlManager->GetOsAccountInfoById(TEST_ID_ONE, migrated), ERR_OK);
    EXPECT_EQ(migrated.GetShortName(), TEST_NAME_ONE + "1");
    EXPECT_EQ(migrated.GetOldShortName(), TEST_NAME_CHINESE);
}

/**
 * @tc.name: ApplyRenamePlanUpdateOsAccountFailed001
 * @tc.desc: Applying a rename plan fails when persisting targets a missing per-account file.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ApplyRenamePlanUpdateOsAccountFailed001, TestSize.Level1)
{
    OsAccountInfo holder = MakeAccount(TEST_ID_TWO, TEST_NAME_TWO);
    OsAccountInfo legacy = MakeAccount(TEST_ID_MISSING, TEST_NAME_CHINESE);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(holder), ERR_OK);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(legacy), ERR_OK);
    std::string storeBase = Constants::USER_INFO_BASE + Constants::PATH_SEPARATOR;
    std::string infoFile = Constants::PATH_SEPARATOR + Constants::USER_INFO_FILE_NAME;
    // Slot TEST_ID_TWO serves content whose localId is TEST_ID_MISSING and whose own file is
    // removed: re-read succeeds, but UpdateOsAccount writes to the missing TEST_ID_MISSING path.
    system(("cp " + storeBase + std::to_string(TEST_ID_MISSING) + infoFile + " " + storeBase +
        std::to_string(TEST_ID_TWO) + infoFile).c_str());
    system(("rm -rf " + storeBase + std::to_string(TEST_ID_MISSING)).c_str());
    std::vector<OsAccountInfo> plan = { MakeAccount(TEST_ID_TWO, TEST_NAME_ONE) };
    EXPECT_EQ(etcManager_->ApplyRenamePlan(plan),
        ERR_OSACCOUNT_SERVICE_CONTROL_UPDATE_FILE_NOT_EXISTS_ERROR);
}

/**
 * @tc.name: RefreshTakenNamesReadFailed001
 * @tc.desc: Name resolution keeps the planned name when the account store cannot be read.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, RefreshTakenNamesReadFailed001, TestSize.Level1)
{
    system(("echo not_json > " + Constants::ACCOUNT_LIST_FILE_JSON_PATH).c_str());
    std::string name = TEST_NAME_ONE;
    EXPECT_FALSE(etcManager_->RefreshTakenNamesAndResolve(g_etcControlManager, TEST_ID_ONE, name));
    EXPECT_EQ(name, TEST_NAME_ONE); // planned name kept, UpdateOsAccount will surface errors
}

/**
 * @tc.name: RefreshTakenNamesNoConflict001
 * @tc.desc: Name resolution keeps the planned name when no other account took it.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, RefreshTakenNamesNoConflict001, TestSize.Level1)
{
    OsAccountInfo holder = MakeAccount(TEST_ID_TWO, TEST_NAME_TWO);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(holder), ERR_OK);
    std::string name = TEST_NAME_ONE; // free: only "bob" is taken
    EXPECT_FALSE(etcManager_->RefreshTakenNamesAndResolve(g_etcControlManager, TEST_ID_ONE, name));
    EXPECT_EQ(name, TEST_NAME_ONE);
}

/**
 * @tc.name: RefreshTakenNamesConflictRegenerated001
 * @tc.desc: Name resolution regenerates a conflict-free name when the planned one is taken.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, RefreshTakenNamesConflictRegenerated001, TestSize.Level1)
{
    OsAccountInfo holder = MakeAccount(TEST_ID_TWO, TEST_NAME_PINYIN);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(holder), ERR_OK);
    std::string name = TEST_NAME_PINYIN; // taken by TEST_ID_TWO
    EXPECT_TRUE(etcManager_->RefreshTakenNamesAndResolve(g_etcControlManager, TEST_ID_ONE, name));
    EXPECT_EQ(name, TEST_NAME_PINYIN + "1");
}

/**
 * @tc.name: ReconcileEtcFilesWeakPtrExpired001
 * @tc.desc: Reconcile with an expired weak pointer is a no-op with success, no files appear.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ReconcileEtcFilesWeakPtrExpired001, TestSize.Level1)
{
    ASSERT_FALSE(PasswdExists()); // SetUp cleaned the etc files
    std::weak_ptr<OsAccountControlFileManager> expiredWeak;
    {
        auto temp = std::make_shared<OsAccountControlFileManager>();
        expiredWeak = temp;
    }
    auto orphanManager = std::make_shared<OsAccountEtcFileManager>(expiredWeak);
    EXPECT_EQ(orphanManager->ReconcileEtcFiles(), ERR_OK);
    EXPECT_FALSE(PasswdExists()); // early return: no rebuild attempted
}

/**
 * @tc.name: ReconcileEtcFilesGetAccountListFailed001
 * @tc.desc: Reconcile fails without rebuilding when the account list file is corrupted.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ReconcileEtcFilesGetAccountListFailed001, TestSize.Level1)
{
    SeedEtcFiles();
    system(("echo not_json > " + Constants::ACCOUNT_LIST_FILE_JSON_PATH).c_str());
    std::string passwdBefore = ReadPasswd();
    EXPECT_EQ(etcManager_->ReconcileEtcFiles(), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_EQ(ReadPasswd(), passwdBefore); // no rebuild on list failure
}

/**
 * @tc.name: ReconcileEtcFilesMissingFilesRebuilt001
 * @tc.desc: Reconcile rebuilds missing etc files from the account list.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ReconcileEtcFilesMissingFilesRebuilt001, TestSize.Level1)
{
    OsAccountInfo account = MakeAccount(TEST_ID_ONE, TEST_NAME_ONE);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(account), ERR_OK);
    ASSERT_FALSE(PasswdExists()); // SetUp cleaned the etc files
    EXPECT_EQ(etcManager_->ReconcileEtcFiles(), ERR_OK);
    std::string passwd = ReadPasswd();
    EXPECT_TRUE(HasUserRow(passwd, TEST_ID_ONE, TEST_NAME_ONE));
    EXPECT_NE(passwd.find(EtcAccountFileUtils::U0_FIXED_PASSWD_LINE), std::string::npos);
    EXPECT_TRUE(HasGroupMember(ReadGroup(), EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ONE));
}

/**
 * @tc.name: ReconcileEtcFilesExistenceCheckFailed001
 * @tc.desc: Reconcile attempts a rebuild when the group path is blocked by a directory, and the
 *           rebuild itself fails writing there.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ReconcileEtcFilesExistenceCheckFailed001, TestSize.Level1)
{
    SeedEtcFiles();
    std::string groupPath = EtcFilePath(EtcAccountFileUtils::GROUP_FILE_NAME);
    system(("rm -rf " + groupPath).c_str());
    system(("mkdir -p " + groupPath).c_str());
    EXPECT_NE(etcManager_->ReconcileEtcFiles(), ERR_OK); // rebuild cannot write the group file
    system(("rm -rf " + groupPath).c_str());
}

/**
 * @tc.name: ReconcileEtcFilesMatchNoChange001
 * @tc.desc: Reconcile succeeds without rewriting when files already match the account list.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ReconcileEtcFilesMatchNoChange001, TestSize.Level1)
{
    OsAccountInfo account = MakeAccount(TEST_ID_ONE, TEST_NAME_ONE);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(account), ERR_OK);
    ASSERT_EQ(etcManager_->FlushEtcFilesSync(), ERR_OK);
    std::string passwdBefore = ReadPasswd();
    std::string groupBefore = ReadGroup();
    EXPECT_EQ(etcManager_->ReconcileEtcFiles(), ERR_OK);
    EXPECT_EQ(ReadPasswd(), passwdBefore);
    EXPECT_EQ(ReadGroup(), groupBefore);
}

/**
 * @tc.name: ReconcileEtcFilesMismatchRebuilt001
 * @tc.desc: Reconcile rewrites etc files that drifted from the account list.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, ReconcileEtcFilesMismatchRebuilt001, TestSize.Level1)
{
    OsAccountInfo account = MakeAccount(TEST_ID_ONE, TEST_NAME_ONE);
    ASSERT_EQ(g_etcControlManager->InsertOsAccount(account), ERR_OK);
    ASSERT_EQ(etcManager_->FlushEtcFilesSync(), ERR_OK);
    // Simulate drift: strip group members so the content no longer matches the account list.
    ASSERT_EQ(EtcAccountFileUtils::WriteEtcFile(EtcAccountFileUtils::GROUP_FILE_NAME,
        "normal:x:20:\nadmin:x:80:\n"), ERR_OK);
    EXPECT_EQ(etcManager_->ReconcileEtcFiles(), ERR_OK);
    std::string group = ReadGroup();
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::NORMAL_GROUP_NAME, TEST_NAME_ONE));
    EXPECT_TRUE(HasGroupMember(group, EtcAccountFileUtils::ADMIN_GROUP_NAME,
        EtcAccountFileUtils::RESERVED_ETC_USER_NAME));
}

/**
 * @tc.name: PostAsyncTaskRetryUntilSuccess001
 * @tc.desc: A failing async task is retried and stops as soon as it succeeds.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, PostAsyncTaskRetryUntilSuccess001, TestSize.Level1)
{
    auto attempts = std::make_shared<std::atomic<int32_t>>(0);
    auto task = [attempts]() {
        attempts->fetch_add(1);
        return attempts->load() >= EXPECTED_ETC_SECOND_ATTEMPT_COUNT ? ERR_OK
            : ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    };
    etcManager_->PostAsyncTask(task);
    ASSERT_TRUE(WaitForAsyncState([attempts]() {
        return attempts->load() >= EXPECTED_ETC_SECOND_ATTEMPT_COUNT;
    }));
    EXPECT_EQ(attempts->load(), EXPECTED_ETC_SECOND_ATTEMPT_COUNT);
}

/**
 * @tc.name: PostAsyncTaskRetryExhausted001
 * @tc.desc: An always-failing async task stops after the maximum retry times.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(OsAccountEtcFileManagerTest, PostAsyncTaskRetryExhausted001, TestSize.Level1)
{
    auto attempts = std::make_shared<std::atomic<int32_t>>(0);
    auto task = [attempts]() {
        attempts->fetch_add(1);
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    };
    etcManager_->PostAsyncTask(task);
    ASSERT_TRUE(WaitForAsyncState([attempts]() {
        return attempts->load() >= EXPECTED_ETC_RETRY_TIMES;
    }));
    EXPECT_EQ(attempts->load(), EXPECTED_ETC_RETRY_TIMES);
}

}  // namespace AccountSA
}  // namespace OHOS
