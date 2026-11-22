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
#include <string>
#include <vector>
#include "account_error_no.h"
#include "os_account_etc_file.h"
#include "os_account_constants.h"
#include "os_account_control_file_manager.h"
#include "os_account_etc_file_manager.h"

#ifndef ACCOUNT_TEST
#error "os_account_etc_file_manager_test.cpp must be built with -DACCOUNT_TEST"
#endif

namespace OHOS {
namespace AccountSA {
using namespace testing::ext;

namespace {
const int32_t TEST_ID_ONE = 101;
const int32_t TEST_ID_TWO = 102;
const std::string TEST_NAME_ONE = "alice";
const std::string TEST_NAME_TWO = "bob";
const std::string TEST_NAME_ADMIN = "adm1";
const std::string TEST_ETC_DIR = EtcAccountFileUtils::ETC_FILE_DIR;

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

void OsAccountEtcFileManagerTest::TearDown() {}

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
    system(("rm -f " + EtcFilePath(EtcAccountFileUtils::PASSWD_FILE_NAME) + " " +
        EtcFilePath(EtcAccountFileUtils::GROUP_FILE_NAME) + " " +
        EtcFilePath(EtcAccountFileUtils::ETC_FAULT_FLAG_NAME)).c_str());
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

}  // namespace AccountSA
}  // namespace OHOS
