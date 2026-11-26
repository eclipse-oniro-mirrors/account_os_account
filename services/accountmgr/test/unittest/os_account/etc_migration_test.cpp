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
#include <set>
#include <string>
#include "os_account_etc_file.h"
#include "os_account_constants.h"

using namespace OHOS;
using namespace OHOS::AccountSA;
using namespace testing::ext;

namespace {
const int32_t TEST_LOCAL_ID = 100;
const std::set<std::string> EMPTY_TAKEN;
} // namespace

class EtcMigrationTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void EtcMigrationTest::SetUpTestCase(void) {}
void EtcMigrationTest::TearDownTestCase(void) {}
void EtcMigrationTest::SetUp(void) {}
void EtcMigrationTest::TearDown(void) {}

// ==================== IsEtcNameCompliant ====================

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_ValidName, TestSize.Level1)
{
    EXPECT_TRUE(EtcAccountFileUtils::IsMigrationNameCompliant("zhangsan"));
    EXPECT_TRUE(EtcAccountFileUtils::IsMigrationNameCompliant("a"));
    EXPECT_TRUE(EtcAccountFileUtils::IsMigrationNameCompliant("a1"));
    EXPECT_TRUE(EtcAccountFileUtils::IsMigrationNameCompliant("a_b"));
    EXPECT_TRUE(EtcAccountFileUtils::IsMigrationNameCompliant("user100"));
}

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_EmptyName, TestSize.Level1)
{
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant(""));
}

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_ExceedsMaxSize, TestSize.Level1)
{
    std::string longName(61, 'a');
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant(longName));
    std::string exactMax(60, 'a');
    EXPECT_TRUE(EtcAccountFileUtils::IsMigrationNameCompliant(exactMax));
}

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_StartsWithDigit, TestSize.Level1)
{
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("1abc"));
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("0"));
}

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_StartsWithUnderscore, TestSize.Level1)
{
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("_abc"));
}

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_ContainsUppercase, TestSize.Level1)
{
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("aBc"));
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("ABC"));
}

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_ContainsChinese, TestSize.Level1)
{
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("zhangsan\xe4\xb8\x89"));
}

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_ContainsSpace, TestSize.Level1)
{
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("a b"));
}

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_ContainsSpecialChar, TestSize.Level1)
{
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("a!b"));
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("a-b"));
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("a.b"));
}

HWTEST_F(EtcMigrationTest, IsEtcNameCompliant_ContainsNewline, TestSize.Level1)
{
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("a\nb"));
    EXPECT_FALSE(EtcAccountFileUtils::IsMigrationNameCompliant("a\rb"));
}

// ==================== GenerateCompliantShortName ====================

HWTEST_F(EtcMigrationTest, GenerateName_AlreadyCompliant_Unchanged, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "zhangsan", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "zhangsan");
}

HWTEST_F(EtcMigrationTest, GenerateName_ChineseConvertedToPinyin, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "\xe5\xbc\xa0\xe4\xb8\x89", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "zhangsan");
}

HWTEST_F(EtcMigrationTest, GenerateName_MultiCharChineseConverted, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "\xe6\x9c\x9d\xe9\x98\xb3", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "zhaoyang");
}

HWTEST_F(EtcMigrationTest, GenerateName_UppercaseToLowercase, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "LiSi", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "lisi");
}

HWTEST_F(EtcMigrationTest, GenerateName_SpaceRemoved, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "wang wu", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "wangwu");
}

HWTEST_F(EtcMigrationTest, GenerateName_AllInvalidBecomesA, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "!@#$%", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "a");
}

HWTEST_F(EtcMigrationTest, GenerateName_EmptyInputBecomesA, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "a");
}

HWTEST_F(EtcMigrationTest, GenerateName_StartsWithDigitGetsPrefix, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "123abc", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "a123abc");
}

HWTEST_F(EtcMigrationTest, GenerateName_StartsWithUnderscoreGetsPrefix, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "_test", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "a_test");
}

HWTEST_F(EtcMigrationTest, GenerateName_RootInTakenGetsSuffix, TestSize.Level1)
{
    std::set<std::string> taken = { "root" };
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "Root", TEST_LOCAL_ID, taken, newName));
    EXPECT_EQ(newName, "root1");
}

HWTEST_F(EtcMigrationTest, GenerateName_NameConflictGetsSuffix1, TestSize.Level1)
{
    std::set<std::string> taken = { "zhangsan" };
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "zhangsan", TEST_LOCAL_ID, taken, newName));
    EXPECT_EQ(newName, "zhangsan1");
}

HWTEST_F(EtcMigrationTest, GenerateName_NameConflictGetsSuffix2, TestSize.Level1)
{
    std::set<std::string> taken = { "zhangsan", "zhangsan1" };
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "zhangsan", TEST_LOCAL_ID, taken, newName));
    EXPECT_EQ(newName, "zhangsan2");
}

HWTEST_F(EtcMigrationTest, GenerateName_MixedChineseAndAscii, TestSize.Level1)
{
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        "\xe5\xbc\xa0\xe4\xb8\x89test", TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_EQ(newName, "zhangsantest");
}

HWTEST_F(EtcMigrationTest, GenerateName_LongNameTruncated, TestSize.Level1)
{
    std::string longName(100, 'x');
    std::string newName;
    EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
        longName, TEST_LOCAL_ID, EMPTY_TAKEN, newName));
    EXPECT_LE(newName.size(), 60);
    EXPECT_TRUE(EtcAccountFileUtils::IsMigrationNameCompliant(newName));
}

HWTEST_F(EtcMigrationTest, GenerateName_ResultsAlwaysCompliant, TestSize.Level1)
{
    std::vector<std::string> inputs = {
        "", "!", "a", "A", "\xe5\xbc\xa0", "123", "_x", "a b",
        "a-b_c.d", "\xe5\xbc\xa0\xe4\xb8\x89\xe6\x9d\x8e\xe5\x9b\x9b",
        std::string(200, 'y'), "root", "Root"
    };
    for (const auto &input : inputs) {
        std::string newName;
        std::set<std::string> taken = { "root" };
        EXPECT_TRUE(EtcAccountFileUtils::GenerateCompliantShortName(
            input, TEST_LOCAL_ID, taken, newName));
        EXPECT_TRUE(EtcAccountFileUtils::IsMigrationNameCompliant(newName))
            << "Input: " << input << " Output: " << newName;
    }
}

// ==================== BuildExpectedEtcContent ====================

HWTEST_F(EtcMigrationTest, BuildExpectedContent_EmptyAccountList, TestSize.Level1)
{
    std::vector<OsAccountInfo> accounts;
    std::string passwd;
    std::string group;
    ErrCode ret = EtcAccountFileUtils::BuildExpectedEtcContent(accounts, passwd, group);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_NE(passwd.find("root:x:0:20"), std::string::npos);
    EXPECT_NE(group.find("normal:"), std::string::npos);
}

HWTEST_F(EtcMigrationTest, BuildExpectedContent_SingleAccount, TestSize.Level1)
{
    OsAccountInfo info;
    info.SetLocalId(100);
    info.SetShortName("zhangsan");
    info.SetLocalName("zhangsan");
    info.SetType(OsAccountType::NORMAL);
    info.SetIsCreateCompleted(true);
    std::vector<OsAccountInfo> accounts = { info };
    std::string passwd;
    std::string group;
    ErrCode ret = EtcAccountFileUtils::BuildExpectedEtcContent(accounts, passwd, group);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_NE(passwd.find("zhangsan:x:100:20"), std::string::npos);
    EXPECT_NE(group.find("zhangsan"), std::string::npos);
}

HWTEST_F(EtcMigrationTest, BuildExpectedContent_AccountWithEmptyShortName, TestSize.Level1)
{
    OsAccountInfo info;
    info.SetLocalId(100);
    info.SetShortName("");
    info.SetLocalName("zhangsan");
    std::vector<OsAccountInfo> accounts = { info };
    std::string passwd;
    std::string group;
    ErrCode ret = EtcAccountFileUtils::BuildExpectedEtcContent(accounts, passwd, group);
    EXPECT_EQ(ret, ERR_OK);
    EXPECT_EQ(passwd.find("zhangsan:x:100"), std::string::npos); // empty shortName skipped
}

// ==================== BuildRenamePlan ====================

namespace {
OsAccountInfo MakeAccountForMigration(int32_t id, const std::string &shortName,
    const std::string &localName)
{
    OsAccountInfo info;
    info.SetLocalId(id);
    info.SetLocalName(localName);
    info.SetShortName(shortName);
    info.SetType(OsAccountType::NORMAL);
    info.SetIsCreateCompleted(true);
    return info;
}
} // namespace

HWTEST_F(EtcMigrationTest, BuildRenamePlan_NonCompliantNameRenamed, TestSize.Level1)
{
    std::vector<OsAccountInfo> accounts = { MakeAccountForMigration(100,
        "\xe5\xbc\xa0\xe4\xb8\x89", "zhangsan") };
    std::vector<OsAccountInfo> plan;
    EtcAccountFileUtils::BuildRenamePlan(accounts, plan);
    ASSERT_EQ(plan.size(), static_cast<size_t>(1));
    EXPECT_EQ(plan[0].GetShortName(), "zhangsan");
    EXPECT_EQ(plan[0].GetOldShortName(), "\xe5\xbc\xa0\xe4\xb8\x89");
    EXPECT_EQ(plan[0].GetLocalId(), 100);
    EXPECT_TRUE(EtcAccountFileUtils::IsMigrationNameCompliant(plan[0].GetShortName()));
}

HWTEST_F(EtcMigrationTest, BuildRenamePlan_CompliantNameSkipped, TestSize.Level1)
{
    std::vector<OsAccountInfo> accounts = { MakeAccountForMigration(100, "zhangsan", "name100") };
    std::vector<OsAccountInfo> plan;
    EtcAccountFileUtils::BuildRenamePlan(accounts, plan);
    EXPECT_TRUE(plan.empty());
}

HWTEST_F(EtcMigrationTest, BuildRenamePlan_EmptyShortNameFallsBackToLocalName, TestSize.Level1)
{
    std::vector<OsAccountInfo> accounts = { MakeAccountForMigration(100, "", "lisi") };
    std::vector<OsAccountInfo> plan;
    EtcAccountFileUtils::BuildRenamePlan(accounts, plan);
    ASSERT_EQ(plan.size(), static_cast<size_t>(1));
    EXPECT_EQ(plan[0].GetShortName(), "lisi");
    EXPECT_EQ(plan[0].GetOldShortName(), "");
}

HWTEST_F(EtcMigrationTest, BuildRenamePlan_IncompleteOrRemovingSkipped, TestSize.Level1)
{
    OsAccountInfo incomplete = MakeAccountForMigration(100, "\xe5\xbc\xa0\xe4\xb8\x89", "name100");
    incomplete.SetIsCreateCompleted(false);
    OsAccountInfo removing = MakeAccountForMigration(101, "\xe6\x9d\x8e\xe5\x9b\x9b", "name101");
    removing.SetToBeRemoved(true);
    std::vector<OsAccountInfo> accounts = { incomplete, removing };
    std::vector<OsAccountInfo> plan;
    EtcAccountFileUtils::BuildRenamePlan(accounts, plan);
    EXPECT_TRUE(plan.empty());
}

HWTEST_F(EtcMigrationTest, BuildRenamePlan_ConflictGetsDistinctSuffixes, TestSize.Level1)
{
    std::vector<OsAccountInfo> accounts = {
        MakeAccountForMigration(100, "\xe5\xbc\xa0\xe4\xb8\x89", "name100"),
        MakeAccountForMigration(101, "\xe5\xbc\xa0\xe4\xb8\x89", "name101"),
    };
    std::vector<OsAccountInfo> plan;
    EtcAccountFileUtils::BuildRenamePlan(accounts, plan);
    ASSERT_EQ(plan.size(), static_cast<size_t>(2));
    EXPECT_EQ(plan[0].GetShortName(), "zhangsan");
    EXPECT_EQ(plan[1].GetShortName(), "zhangsan1");
    EXPECT_NE(plan[0].GetShortName(), plan[1].GetShortName());
}

HWTEST_F(EtcMigrationTest, BuildRenamePlan_ExistingCompliantNameBlocksReuse, TestSize.Level1)
{
    std::vector<OsAccountInfo> accounts = {
        MakeAccountForMigration(100, "zhangsan", "name100"),
        MakeAccountForMigration(101, "\xe5\xbc\xa0\xe4\xb8\x89", "name101"),
    };
    std::vector<OsAccountInfo> plan;
    EtcAccountFileUtils::BuildRenamePlan(accounts, plan);
    ASSERT_EQ(plan.size(), static_cast<size_t>(1));
    EXPECT_EQ(plan[0].GetLocalId(), 101);
    EXPECT_EQ(plan[0].GetShortName(), "zhangsan1"); // "zhangsan" taken by account 100
}

HWTEST_F(EtcMigrationTest, BuildRenamePlan_ReservedRootNeverAssigned, TestSize.Level1)
{
    std::vector<OsAccountInfo> accounts = { MakeAccountForMigration(100, "Root", "name100") };
    std::vector<OsAccountInfo> plan;
    EtcAccountFileUtils::BuildRenamePlan(accounts, plan);
    ASSERT_EQ(plan.size(), static_cast<size_t>(1));
    EXPECT_EQ(plan[0].GetShortName(), "root1");
}

/**
 * @tc.name: GenerateName_SuffixExhausted
 * @tc.desc: Candidate and all suffixes 1..999 are taken, rename fails and newName is not written.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(EtcMigrationTest, GenerateName_SuffixExhausted, TestSize.Level1)
{
    std::set<std::string> taken = { "root", "zhangsan" };
    for (uint32_t suffix = 1; suffix <= EtcAccountFileUtils::ETC_NAME_SUFFIX_MAX; ++suffix) {
        taken.insert("zhangsan" + std::to_string(suffix));
    }
    std::string newName = "untouched";
    EXPECT_FALSE(EtcAccountFileUtils::GenerateCompliantShortName(
        "zhangsan", TEST_LOCAL_ID, taken, newName));
    EXPECT_EQ(newName, "untouched"); // no partial write on failure
}

/**
 * @tc.name: BuildRenamePlan_RenameUnexpectedlyFailedSkipped
 * @tc.desc: Account whose generated names are all taken is skipped, plan stays empty.
 * @tc.type: FUNC
 * @tc.require: SR000GGVFG
 */
HWTEST_F(EtcMigrationTest, BuildRenamePlan_RenameUnexpectedlyFailedSkipped, TestSize.Level1)
{
    std::vector<OsAccountInfo> accounts;
    int32_t localId = TEST_LOCAL_ID;
    accounts.push_back(MakeAccountForMigration(localId++, "zhangsan", "holder"));
    for (uint32_t suffix = 1; suffix <= EtcAccountFileUtils::ETC_NAME_SUFFIX_MAX; ++suffix) {
        accounts.push_back(MakeAccountForMigration(localId++, "zhangsan" + std::to_string(suffix),
            "holder"));
    }
    accounts.push_back(MakeAccountForMigration(localId++, "\xe5\xbc\xa0\xe4\xb8\x89", "legacy"));
    std::vector<OsAccountInfo> plan;
    EtcAccountFileUtils::BuildRenamePlan(accounts, plan);
    EXPECT_TRUE(plan.empty()); // rename failed for the legacy account, it is skipped
}
