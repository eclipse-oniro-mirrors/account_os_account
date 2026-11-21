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

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include "account_error_no.h"
#include "os_account_etc_file.h"

#ifndef ACCOUNT_TEST
#error "os_account_etc_file_test.cpp must be built with -DACCOUNT_TEST"
#endif

using namespace testing::ext;
using namespace OHOS;
using namespace AccountSA;

namespace {
const std::string TEST_ETC_DIR = "/data/service/el1/public/etc/test/";
const std::string TEST_PASSWD = TEST_ETC_DIR + "passwd";
const std::string TEST_GROUP = TEST_ETC_DIR + "group";
} // namespace

class EtcAccountFileTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void EtcAccountFileTest::SetUpTestCase()
{
    system(("mkdir -p " + TEST_ETC_DIR).c_str());
}

void EtcAccountFileTest::TearDownTestCase()
{
    system(("rm -rf " + TEST_ETC_DIR).c_str());
}

void EtcAccountFileTest::SetUp() {}
void EtcAccountFileTest::TearDown() {}

/**
 * @tc.name: EtcUserEntryFromString001
 * @tc.desc: Valid passwd line with 7 fields.
 */
HWTEST_F(EtcAccountFileTest, EtcUserEntryFromString001, TestSize.Level1)
{
    EtcUserEntry entry;
    ErrCode ret = EtcUserEntry::FromString("zhangsan:x:100:20::/storage/Users/currentUser:/bin/bash", entry);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(entry.userName, "zhangsan");
    EXPECT_EQ(entry.password, "x");
    EXPECT_EQ(entry.uid, 100);
    EXPECT_EQ(entry.gid, 20);
    EXPECT_EQ(entry.gecos, "");
    EXPECT_EQ(entry.homeDir, "/storage/Users/currentUser");
    EXPECT_EQ(entry.shell, "/bin/bash");
}

/**
 * @tc.name: EtcUserEntryFromString002
 * @tc.desc: Invalid passwd lines: wrong field count, empty username, non-numeric uid/gid.
 */
HWTEST_F(EtcAccountFileTest, EtcUserEntryFromString002, TestSize.Level1)
{
    EtcUserEntry entry;
    EXPECT_EQ(EtcUserEntry::FromString("zhangsan:x:100:20::", entry),
        ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(EtcUserEntry::FromString("a:b:1:2:c:d:e:f", entry),
        ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(EtcUserEntry::FromString(":x:100:20:::/bin/bash", entry),
        ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(EtcUserEntry::FromString("abc:x:xyz:20:::/bin/bash", entry),
        ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(EtcUserEntry::FromString("abc:x:100:abc:::/bin/bash", entry),
        ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    EXPECT_EQ(EtcUserEntry::FromString("", entry), ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
}

/**
 * @tc.name: EtcUserEntryToString001
 * @tc.desc: Round-trip: FromString -> ToString -> same string.
 */
HWTEST_F(EtcAccountFileTest, EtcUserEntryToString001, TestSize.Level1)
{
    std::string line = "zhangsan:x:100:20::/storage/Users/currentUser:/bin/bash";
    EtcUserEntry entry;
    ASSERT_EQ(EtcUserEntry::FromString(line, entry), ERR_OK);
    EXPECT_EQ(entry.ToString(), line);
}

/**
 * @tc.name: EtcUserEntryFromOsAccountInfo001
 * @tc.desc: Build entry from OsAccountInfo.
 */
HWTEST_F(EtcAccountFileTest, EtcUserEntryFromOsAccountInfo001, TestSize.Level1)
{
    OsAccountInfo info(100, "", OsAccountType::ADMIN, 0);
    EtcUserEntry entry = EtcUserEntry::FromOsAccountInfo(info, "zhangsan");
    EXPECT_EQ(entry.userName, "zhangsan");
    EXPECT_EQ(entry.uid, 100);
    EXPECT_EQ(entry.gid, EtcAccountFileUtils::NORMAL_GROUP_GID);
    EXPECT_EQ(entry.homeDir, EtcAccountFileUtils::DEFAULT_HOME_DIR);
    EXPECT_EQ(entry.shell, EtcAccountFileUtils::DEFAULT_SHELL);
}

/**
 * @tc.name: EtcGroupEntryFromString001
 * @tc.desc: Valid group line with 4 fields.
 */
HWTEST_F(EtcAccountFileTest, EtcGroupEntryFromString001, TestSize.Level1)
{
    EtcGroupEntry entry;
    ErrCode ret = EtcGroupEntry::FromString("normal:x:20:root,zhangsan", entry);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(entry.groupName, "normal");
    EXPECT_EQ(entry.gid, 20);
    ASSERT_EQ(entry.members.size(), 2u);
    EXPECT_EQ(entry.members[0], "root");
    EXPECT_EQ(entry.members[1], "zhangsan");
}

/**
 * @tc.name: EtcGroupEntryFromString002
 * @tc.desc: Valid group line with 3 fields (no members).
 */
HWTEST_F(EtcAccountFileTest, EtcGroupEntryFromString002, TestSize.Level1)
{
    EtcGroupEntry entry;
    ErrCode ret = EtcGroupEntry::FromString("admin:x:80:", entry);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(entry.groupName, "admin");
    EXPECT_EQ(entry.gid, 80);
    EXPECT_TRUE(entry.members.empty());
}

/**
 * @tc.name: EtcGroupEntryToString001
 * @tc.desc: Round-trip: FromString -> ToString.
 */
HWTEST_F(EtcAccountFileTest, EtcGroupEntryToString001, TestSize.Level1)
{
    std::string line = "normal:x:20:root,zhangsan";
    EtcGroupEntry entry;
    ASSERT_EQ(EtcGroupEntry::FromString(line, entry), ERR_OK);
    EXPECT_EQ(entry.ToString(), line);
}

/**
 * @tc.name: ParsePasswdContent001
 * @tc.desc: Normal content parses successfully.
 */
HWTEST_F(EtcAccountFileTest, ParsePasswdContent001, TestSize.Level1)
{
    std::string content =
        "zhangsan:x:100:20::/storage/Users/currentUser:/bin/bash\n"
        "lisi:x:101:20::/storage/Users/currentUser:/bin/bash\n";
    std::vector<EtcUserEntry> entries;
    EXPECT_TRUE(EtcAccountFileUtils::ParsePasswdContent(content, entries));
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].userName, "zhangsan");
    EXPECT_EQ(entries[1].userName, "lisi");
}

/**
 * @tc.name: ParsePasswdContent002
 * @tc.desc: U0 and U1 fixed lines are skipped.
 */
HWTEST_F(EtcAccountFileTest, ParsePasswdContent002, TestSize.Level1)
{
    std::string content =
        "root:x:0:20::/storage/Users/currentUser:/bin/bash\n"
        ":x:1:20::/storage/Users/currentUser:/bin/bash\n"
        "zhangsan:x:100:20::/storage/Users/currentUser:/bin/bash\n";
    std::vector<EtcUserEntry> entries;
    EXPECT_TRUE(EtcAccountFileUtils::ParsePasswdContent(content, entries));
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].userName, "zhangsan");
}

/**
 * @tc.name: ParsePasswdContent003
 * @tc.desc: Bad line causes failure with entries cleared, empty content returns zero entries.
 */
HWTEST_F(EtcAccountFileTest, ParsePasswdContent003, TestSize.Level1)
{
    std::vector<EtcUserEntry> entries;
    std::string badContent = "zhangsan:x:100:20::/storage/Users/currentUser:/bin/bash\nbadline\n";
    EXPECT_FALSE(EtcAccountFileUtils::ParsePasswdContent(badContent, entries));
    EXPECT_TRUE(entries.empty());
    std::string emptyContent = "";
    EXPECT_TRUE(EtcAccountFileUtils::ParsePasswdContent(emptyContent, entries));
    EXPECT_TRUE(entries.empty());
}

/**
 * @tc.name: ParseGroupContent001
 * @tc.desc: Normal group content parses successfully.
 */
HWTEST_F(EtcAccountFileTest, ParseGroupContent001, TestSize.Level1)
{
    std::string content = "normal:x:20:root,zhangsan\nadmin:x:80:root\n";
    std::vector<EtcGroupEntry> entries;
    EXPECT_TRUE(EtcAccountFileUtils::ParseGroupContent(content, entries));
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].groupName, "normal");
    EXPECT_EQ(entries[1].groupName, "admin");
}

/**
 * @tc.name: ParseGroupContent002
 * @tc.desc: Bad group line causes failure, empty content returns zero entries.
 */
HWTEST_F(EtcAccountFileTest, ParseGroupContent002, TestSize.Level1)
{
    std::vector<EtcGroupEntry> entries;
    std::string badContent = "normal:x:20:root\nbad:group\n";
    EXPECT_FALSE(EtcAccountFileUtils::ParseGroupContent(badContent, entries));
    EXPECT_TRUE(entries.empty());
    EXPECT_TRUE(EtcAccountFileUtils::ParseGroupContent("", entries));
    EXPECT_TRUE(entries.empty());
}

/**
 * @tc.name: IsEtcUserNameRepresentable001
 * @tc.desc: Normal name is representable.
 */
HWTEST_F(EtcAccountFileTest, IsEtcUserNameRepresentable001, TestSize.Level1)
{
    EXPECT_TRUE(EtcAccountFileUtils::IsEtcUserNameRepresentable("zhangsan"));
    EXPECT_TRUE(EtcAccountFileUtils::IsEtcUserNameRepresentable(""));
    EXPECT_FALSE(EtcAccountFileUtils::IsEtcUserNameRepresentable("zhang\nsan"));
    EXPECT_FALSE(EtcAccountFileUtils::IsEtcUserNameRepresentable("zhang\rsan"));
    EXPECT_FALSE(EtcAccountFileUtils::IsEtcUserNameRepresentable("zhang:san"));
}

/**
 * @tc.name: IsGroupMemberRepresentable001
 * @tc.desc: Comma check for group members.
 */
HWTEST_F(EtcAccountFileTest, IsGroupMemberRepresentable001, TestSize.Level1)
{
    EXPECT_TRUE(EtcAccountFileUtils::IsGroupMemberRepresentable("zhangsan"));
    EXPECT_TRUE(EtcAccountFileUtils::IsGroupMemberRepresentable(""));
    EXPECT_FALSE(EtcAccountFileUtils::IsGroupMemberRepresentable("zhang,san"));
    EXPECT_FALSE(EtcAccountFileUtils::IsGroupMemberRepresentable("zhang\nsan"));
    EXPECT_FALSE(EtcAccountFileUtils::IsGroupMemberRepresentable("zhang\rsan"));
    EXPECT_FALSE(EtcAccountFileUtils::IsGroupMemberRepresentable("zhang:san"));
}

/**
 * @tc.name: SplitAndJoinLines001
 * @tc.desc: Split content to lines, then join back.
 */
HWTEST_F(EtcAccountFileTest, SplitAndJoinLines001, TestSize.Level1)
{
    std::vector<std::string> lines;
    EtcAccountFileUtils::SplitContentToLines("a\nb\nc\n", lines);
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
    EXPECT_EQ(lines[2], "c");
    std::string joined = EtcAccountFileUtils::JoinLines(lines);
    EXPECT_EQ(joined, "a\nb\nc\n");
}

/**
 * @tc.name: SplitAndJoinLines002
 * @tc.desc: Trailing blank lines are dropped.
 */
HWTEST_F(EtcAccountFileTest, SplitAndJoinLines002, TestSize.Level1)
{
    std::vector<std::string> lines;
    EtcAccountFileUtils::SplitContentToLines("a\nb\n\n\n", lines);
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
    std::string joined = EtcAccountFileUtils::JoinLines(lines);
    EXPECT_EQ(joined, "a\nb\n");
    lines.clear();
    EtcAccountFileUtils::SplitContentToLines("only", lines);
    EXPECT_EQ(lines.size(), 1u);
}

/**
 * @tc.name: WriteAndReadEtcFile001
 * @tc.desc: Write file, then read back same content.
 */
HWTEST_F(EtcAccountFileTest, WriteAndReadEtcFile001, TestSize.Level1)
{
    std::string content = "zhangsan:x:100:20:::/bin/bash\n";
    ErrCode ret = EtcAccountFileUtils::WriteEtcFile("passwd", content);
    EXPECT_EQ(ret, ERR_OK);
    std::string readBack;
    ret = EtcAccountFileUtils::ReadEtcFile("passwd", readBack);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(readBack, content);
}

/**
 * @tc.name: IsEtcFileExist001
 * @tc.desc: File exists check.
 */
HWTEST_F(EtcAccountFileTest, IsEtcFileExist001, TestSize.Level1)
{
    EtcAccountFileUtils::WriteEtcFile("passwd", "test");
    bool isExist = false;
    EXPECT_EQ(EtcAccountFileUtils::IsEtcFileExist("passwd", isExist), ERR_OK);
    EXPECT_TRUE(isExist);
    isExist = false;
    EXPECT_EQ(EtcAccountFileUtils::IsEtcFileExist("nonexistent", isExist), ERR_OK);
    EXPECT_FALSE(isExist);
}

/**
 * @tc.name: FaultFlagLifecycle001
 * @tc.desc: Create flag -> check exist -> remove flag -> check not exist.
 */
HWTEST_F(EtcAccountFileTest, FaultFlagLifecycle001, TestSize.Level1)
{
    EtcAccountFileUtils::RemoveEtcFaultFlagFile();
    bool isExist = true;
    EXPECT_EQ(EtcAccountFileUtils::IsEtcFaultFlagFileExist(isExist), ERR_OK);
    EXPECT_FALSE(isExist);

    EXPECT_EQ(EtcAccountFileUtils::CreateEtcFaultFlagFile(), ERR_OK);
    EXPECT_EQ(EtcAccountFileUtils::IsEtcFaultFlagFileExist(isExist), ERR_OK);
    EXPECT_TRUE(isExist);

    EXPECT_EQ(EtcAccountFileUtils::RemoveEtcFaultFlagFile(), ERR_OK);
    EXPECT_EQ(EtcAccountFileUtils::IsEtcFaultFlagFileExist(isExist), ERR_OK);
    EXPECT_FALSE(isExist);
}

/**
 * @tc.name: BuildExpectedEtcContent001
 * @tc.desc: Build expected content from account list.
 */
HWTEST_F(EtcAccountFileTest, BuildExpectedEtcContent001, TestSize.Level1)
{
    std::vector<OsAccountInfo> infos;
    OsAccountInfo normal(100, "", OsAccountType::NORMAL, 0);
    normal.SetShortName("zhangsan");
    normal.SetIsCreateCompleted(true);
    infos.push_back(normal);

    OsAccountInfo admin(101, "", OsAccountType::ADMIN, 0);
    admin.SetShortName("lisi");
    admin.SetIsCreateCompleted(true);
    infos.push_back(admin);

    OsAccountInfo pending(102, "", OsAccountType::NORMAL, 0);
    pending.SetShortName("wangwu");
    pending.SetIsCreateCompleted(false); // not completed, should be filtered
    infos.push_back(pending);

    std::string passwdContent;
    std::string groupContent;
    ErrCode ret = EtcAccountFileUtils::BuildExpectedEtcContent(infos,
        passwdContent, groupContent);
    EXPECT_EQ(ret, ERR_OK);

    std::vector<EtcUserEntry> userEntries;
    EXPECT_TRUE(EtcAccountFileUtils::ParsePasswdContent(passwdContent, userEntries));
    ASSERT_EQ(userEntries.size(), 2u);
    EXPECT_EQ(userEntries[0].userName, "zhangsan");
    EXPECT_EQ(userEntries[0].uid, 100);
    EXPECT_EQ(userEntries[1].userName, "lisi");
    EXPECT_EQ(userEntries[1].uid, 101);
    std::vector<EtcGroupEntry> groupEntries;
    EXPECT_TRUE(EtcAccountFileUtils::ParseGroupContent(groupContent, groupEntries));
    ASSERT_EQ(groupEntries.size(), 2u);
    EXPECT_EQ(groupEntries[0].groupName, "normal");
    ASSERT_EQ(groupEntries[0].members.size(), 3u); // root, zhangsan (normal), lisi (admin)
    EXPECT_EQ(groupEntries[0].members[0], "root");
    EXPECT_EQ(groupEntries[0].members[1], "zhangsan");
    EXPECT_EQ(groupEntries[0].members[2], "lisi");
    EXPECT_EQ(groupEntries[1].groupName, "admin");
    ASSERT_EQ(groupEntries[1].members.size(), 2u);
    EXPECT_EQ(groupEntries[1].members[0], "root");
    EXPECT_EQ(groupEntries[1].members[1], "lisi");
}

/**
 * @tc.name: BuildExpectedEtcContent002
 * @tc.desc: Accounts with empty shortName are skipped.
 */
HWTEST_F(EtcAccountFileTest, BuildExpectedEtcContent002, TestSize.Level1)
{
    std::vector<OsAccountInfo> infos;
    OsAccountInfo noName(100, "", OsAccountType::NORMAL, 0);
    noName.SetIsCreateCompleted(true); // completed but no shortName
    infos.push_back(noName);

    std::string passwdContent;
    std::string groupContent;
    ErrCode ret = EtcAccountFileUtils::BuildExpectedEtcContent(infos,
        passwdContent, groupContent);
    EXPECT_EQ(ret, ERR_OK);

    std::vector<EtcUserEntry> userEntries;
    EXPECT_TRUE(EtcAccountFileUtils::ParsePasswdContent(passwdContent, userEntries));
    EXPECT_TRUE(userEntries.empty()); // no entries (empty shortName skipped)
}

/**
 * @tc.name: BuildExpectedEtcContent003
 * @tc.desc: Accounts with toBeRemoved are skipped.
 */
HWTEST_F(EtcAccountFileTest, BuildExpectedEtcContent003, TestSize.Level1)
{
    std::vector<OsAccountInfo> infos;
    OsAccountInfo removing(100, "", OsAccountType::NORMAL, 0);
    removing.SetShortName("zhangsan");
    removing.SetIsCreateCompleted(true);
    removing.SetToBeRemoved(true); // being removed, should be filtered
    infos.push_back(removing);

    std::string passwdContent;
    std::string groupContent;
    ErrCode ret = EtcAccountFileUtils::BuildExpectedEtcContent(infos,
        passwdContent, groupContent);
    EXPECT_EQ(ret, ERR_OK);

    std::vector<EtcUserEntry> userEntries;
    EXPECT_TRUE(EtcAccountFileUtils::ParsePasswdContent(passwdContent, userEntries));
    EXPECT_TRUE(userEntries.empty());
}

/**
 * @tc.name: BuildExpectedEtcContent004
 * @tc.desc: Accounts are sorted by uid.
 */
HWTEST_F(EtcAccountFileTest, BuildExpectedEtcContent004, TestSize.Level1)
{
    std::vector<OsAccountInfo> infos;
    OsAccountInfo a(102, "", OsAccountType::NORMAL, 0);
    a.SetShortName("ccc");
    a.SetIsCreateCompleted(true);
    infos.push_back(a);

    OsAccountInfo b(100, "", OsAccountType::NORMAL, 0);
    b.SetShortName("aaa");
    b.SetIsCreateCompleted(true);
    infos.push_back(b);

    OsAccountInfo c(101, "", OsAccountType::NORMAL, 0);
    c.SetShortName("bbb");
    c.SetIsCreateCompleted(true);
    infos.push_back(c);

    std::string passwdContent;
    std::string groupContent;
    ErrCode ret = EtcAccountFileUtils::BuildExpectedEtcContent(infos,
        passwdContent, groupContent);
    EXPECT_EQ(ret, ERR_OK);

    std::vector<EtcUserEntry> userEntries;
    EXPECT_TRUE(EtcAccountFileUtils::ParsePasswdContent(passwdContent, userEntries));
    ASSERT_EQ(userEntries.size(), 3u);
    EXPECT_EQ(userEntries[0].userName, "aaa"); // uid 100
    EXPECT_EQ(userEntries[1].userName, "bbb"); // uid 101
    EXPECT_EQ(userEntries[2].userName, "ccc"); // uid 102
}

/**
 * @tc.name: BuildExpectedEtcContent005
 * @tc.desc: Accounts whose short name contains ':' or equals the reserved "root" are excluded.
 */
HWTEST_F(EtcAccountFileTest, BuildExpectedEtcContent005, TestSize.Level1)
{
    std::vector<OsAccountInfo> infos;
    OsAccountInfo badName(100, "", OsAccountType::NORMAL, 0);
    badName.SetShortName("bad:name");
    badName.SetIsCreateCompleted(true);
    infos.push_back(badName);

    OsAccountInfo rootName(101, "", OsAccountType::NORMAL, 0);
    rootName.SetShortName(EtcAccountFileUtils::RESERVED_ETC_USER_NAME);
    rootName.SetIsCreateCompleted(true);
    infos.push_back(rootName);

    OsAccountInfo okName(102, "", OsAccountType::NORMAL, 0);
    okName.SetShortName("zhangsan");
    okName.SetIsCreateCompleted(true);
    infos.push_back(okName);

    std::string passwdContent;
    std::string groupContent;
    ErrCode ret = EtcAccountFileUtils::BuildExpectedEtcContent(infos,
        passwdContent, groupContent);
    EXPECT_EQ(ret, ERR_OK);

    std::vector<EtcUserEntry> userEntries;
    EXPECT_TRUE(EtcAccountFileUtils::ParsePasswdContent(passwdContent, userEntries));
    ASSERT_EQ(userEntries.size(), 1u);
    EXPECT_EQ(userEntries[0].userName, "zhangsan");
    std::vector<EtcGroupEntry> groupEntries;
    EXPECT_TRUE(EtcAccountFileUtils::ParseGroupContent(groupContent, groupEntries));
    ASSERT_EQ(groupEntries.size(), 2u);
    ASSERT_EQ(groupEntries[0].members.size(), 2u); // root + zhangsan only
    EXPECT_EQ(groupEntries[0].members[0], "root");
    EXPECT_EQ(groupEntries[0].members[1], "zhangsan");
}

/**
 * @tc.name: WriteEtcFileDirNotExist001
 * @tc.desc: WriteEtcFile fails when the etc directory does not exist.
 */
HWTEST_F(EtcAccountFileTest, WriteEtcFileDirNotExist001, TestSize.Level1)
{
    system(("rm -rf " + TEST_ETC_DIR).c_str());
    ErrCode ret = EtcAccountFileUtils::WriteEtcFile("passwd", "test");
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_FILE_NOT_EXIST);
    system(("mkdir -p " + TEST_ETC_DIR).c_str());
}

/**
 * @tc.name: CreateEtcFaultFlagDirNotExist001
 * @tc.desc: CreateEtcFaultFlagFile fails when the etc directory does not exist.
 */
HWTEST_F(EtcAccountFileTest, CreateEtcFaultFlagDirNotExist001, TestSize.Level1)
{
    system(("rm -rf " + TEST_ETC_DIR).c_str());
    ErrCode ret = EtcAccountFileUtils::CreateEtcFaultFlagFile();
    EXPECT_EQ(ret, ERR_ACCOUNT_COMMON_FILE_NOT_EXIST);
    system(("mkdir -p " + TEST_ETC_DIR).c_str());
}
