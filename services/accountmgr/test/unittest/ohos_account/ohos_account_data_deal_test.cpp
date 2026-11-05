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

#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <sys/stat.h>

#include "account_error_no.h"
#include "account_info.h"
#include "account_log_wrapper.h"
#include "directory_ex.h"
#define private public
#include "ohos_account_data_deal.h"
#undef private

using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AccountSA;

namespace {
const std::string RESOURCE_ROOT_PATH = "/data/test/account/data_deal/";
const std::string TEST_STR_ACCOUNT_NAME = "incubation";
const std::string TEST_STR_OPEN_ID = "test open id";
const std::int32_t TEST_VALID_USER_ID = 100;
const std::int32_t TEST_INVALID_USER_ID = 200;

ErrCode g_mockDeleteRet = ERR_OK;
bool g_mockDeleteEnabled = false;
ErrCode g_mockGetContentRet = ERR_OK;
bool g_mockGetContentEnabled = false;
std::string g_mockGetContentStr;
}

namespace OHOS {
namespace AccountSA {
void SetMockDeleteFileResult(ErrCode ret)
{
    g_mockDeleteRet = ret;
    g_mockDeleteEnabled = true;
}

void SetMockGetContentResult(ErrCode ret, const std::string &content)
{
    g_mockGetContentRet = ret;
    g_mockGetContentStr = content;
    g_mockGetContentEnabled = true;
}

void ResetMockAccountFileOperator()
{
    g_mockDeleteEnabled = false;
    g_mockGetContentEnabled = false;
}

ErrCode AccountFileOperator::DeleteFile(const std::string &path)
{
    if (g_mockDeleteEnabled) {
        g_mockDeleteEnabled = false;
        return g_mockDeleteRet;
    }
    std::unique_lock<std::shared_timed_mutex> lock(fileLock_);
    if (!OHOS::RemoveFile(path)) {
        return ERR_OSACCOUNT_SERVICE_FILE_DELE_ERROR;
    }
#ifdef ENABLE_FILE_WATCHER
    SetValidDeleteFileOperationFlag(path, true);
#endif
    return ERR_OK;
}

ErrCode AccountFileOperator::GetFileContentByPath(const std::string &path, std::string &content)
{
    if (g_mockGetContentEnabled) {
        g_mockGetContentEnabled = false;
        content = g_mockGetContentStr;
        return g_mockGetContentRet;
    }
    if (!IsExistFile(path)) {
        return ERR_OSACCOUNT_SERVICE_FILE_FIND_FILE_ERROR;
    }
    std::shared_lock<std::shared_timed_mutex> lock(fileLock_);
    std::ifstream fin(path);
    if (!fin) {
        return ERR_ACCOUNT_COMMON_FILE_OPEN_FAILED;
    }
    content.assign((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
    return ERR_OK;
}
} // namespace AccountSA
} // namespace OHOS

class OhosAccountDataDealTest : public testing::Test {
public:
    static void SetUpTestCase();

    static void TearDownTestCase();

    void SetUp();

    void TearDown();
};

void OhosAccountDataDealTest::SetUpTestCase() {}

void OhosAccountDataDealTest::TearDownTestCase() {}

void OhosAccountDataDealTest::SetUp(void) __attribute__((no_sanitize("cfi")))
{
    testing::UnitTest *test = testing::UnitTest::GetInstance();
    ASSERT_NE(test, nullptr);
    const testing::TestInfo *testinfo = test->current_test_info();
    ASSERT_NE(testinfo, nullptr);
    string testCaseName = string(testinfo->name());
    ACCOUNT_LOGI("[SetUp] %{public}s start", testCaseName.c_str());
}

void OhosAccountDataDealTest::TearDown()
{
    ResetMockAccountFileOperator();
}

/**
 * @tc.name: OhosAccountJsonNotInitTest001
 * @tc.desc: Test uninit data deal
 * @tc.type: FUNC
 * @tc.require: AR000CUF55
 */
HWTEST_F(OhosAccountDataDealTest, OhosAccountJsonNotInitTest001, TestSize.Level3)
{
    AccountInfo accountInfo;
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    ErrCode errCode = dataDeal.AccountInfoFromJson(accountInfo, TEST_VALID_USER_ID);
    EXPECT_EQ(errCode, ERR_ACCOUNT_DATADEAL_NOT_READY);
}

/**
 * @tc.name: ValidOhosAccountJsonTest001
 * @tc.desc: Test valid account info json file read
 * @tc.type: FUNC
 * @tc.require: AR000CUF6P
 */
HWTEST_F(OhosAccountDataDealTest, ValidOhosAccountJsonTest001, TestSize.Level3)
{
    AccountInfo accountInfo;
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    ErrCode errCode = dataDeal.Init(TEST_VALID_USER_ID);
    EXPECT_EQ(errCode, ERR_OK);

    errCode = dataDeal.AccountInfoFromJson(accountInfo, TEST_VALID_USER_ID);
    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.status_, ACCOUNT_STATE_UNBOUND);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.name_, TEST_STR_ACCOUNT_NAME);
    EXPECT_EQ(accountInfo.userId_, TEST_VALID_USER_ID);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.uid_, TEST_STR_OPEN_ID);
}

/**
 * @tc.name: ValidOhosAccountJsonTest002
 * @tc.desc: Test valid account info json file write
 * @tc.type: FUNC
 * @tc.require: AR000CUF6Q
 */
HWTEST_F(OhosAccountDataDealTest, ValidOhosAccountJsonTest002, TestSize.Level3)
{
    /**
     * @tc.steps: step1. init json object
     */
    AccountInfo accountInfo;
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    ErrCode errCode = dataDeal.Init(TEST_VALID_USER_ID);
    EXPECT_EQ(errCode, ERR_OK);

    /**
     * @tc.steps: step2. read from file
     */
    errCode = dataDeal.AccountInfoFromJson(accountInfo, TEST_VALID_USER_ID);
    EXPECT_EQ(errCode, ERR_OK);

    /**
     * @tc.steps: step3. modify and write
     */
    accountInfo.ohosAccountInfo_.status_ = ACCOUNT_STATE_LOGIN;
    accountInfo.ohosAccountInfo_.uid_ = "rewrite content";
    errCode = dataDeal.AccountInfoToJson(accountInfo);
    EXPECT_EQ(errCode, ERR_OK);
}

/**
 * @tc.name: ValidOhosAccountJsonTest003
 * @tc.desc: Test AccountInfoToJson init not OK
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, ValidOhosAccountJsonTest003, TestSize.Level3)
{
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    dataDeal.initOk_ = false;
    AccountInfo accountInfo;
    ErrCode result = dataDeal.AccountInfoToJson(accountInfo);
    ASSERT_EQ(result, ERR_ACCOUNT_DATADEAL_NOT_READY);
}

/**
 * @tc.name: InvalidOhosAccountJsonTest001
 * @tc.desc: Test invalid json file
 * @tc.type: FUNC
 * @tc.require: AR000CUF55
 */
HWTEST_F(OhosAccountDataDealTest, InvalidOhosAccountJsonTest001, TestSize.Level3)
{
    /**
     * @tc.steps: step1. first init, should fail
     */
    AccountInfo accountInfo;
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    ErrCode errCode = dataDeal.Init(TEST_INVALID_USER_ID);
    EXPECT_EQ(errCode, ERR_ACCOUNT_DATADEAL_JSON_FILE_CORRUPTION);

    /**
     * @tc.steps: step2. second init, should succeed
     */
    errCode = dataDeal.Init(TEST_INVALID_USER_ID);
    EXPECT_EQ(errCode, ERR_OK);

    /**
     * @tc.steps: step3. check content
     */
    errCode = dataDeal.AccountInfoFromJson(accountInfo, TEST_INVALID_USER_ID);
    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.status_, ACCOUNT_STATE_UNBOUND);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.name_, DEFAULT_OHOS_ACCOUNT_NAME);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.uid_, DEFAULT_OHOS_ACCOUNT_UID);

    /**
     * @tc.steps: step4. update content
     */
    accountInfo.ohosAccountInfo_.status_ = ACCOUNT_STATE_UNBOUND;
    accountInfo.ohosAccountInfo_.name_ = TEST_STR_ACCOUNT_NAME;
    accountInfo.ohosAccountInfo_.uid_ = TEST_STR_OPEN_ID;
    dataDeal.AccountInfoToJson(accountInfo);

    /**
     * @tc.steps: step5. read and recheck file content
     */
    OhosAccountDataDeal dataDealNew(RESOURCE_ROOT_PATH);
    errCode = dataDealNew.Init(TEST_INVALID_USER_ID);
    EXPECT_EQ(errCode, ERR_OK);
    AccountInfo accountInfoNew;
    errCode = dataDeal.AccountInfoFromJson(accountInfoNew, TEST_INVALID_USER_ID);
    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_EQ(accountInfoNew.ohosAccountInfo_.status_, ACCOUNT_STATE_UNBOUND);
    EXPECT_EQ(accountInfoNew.ohosAccountInfo_.name_, TEST_STR_ACCOUNT_NAME);
    EXPECT_EQ(accountInfoNew.ohosAccountInfo_.uid_, TEST_STR_OPEN_ID);
}

/**
 * @tc.name: OversizedOhosAccountJsonTest001
 * @tc.desc: Test oversized account.json is detected and rebuilt during Init
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, OversizedOhosAccountJsonTest001, TestSize.Level3)
{
    int32_t testOversizedUserId = 300;
    std::string oversizedDir = RESOURCE_ROOT_PATH + std::to_string(testOversizedUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(oversizedDir));

    std::string oversizedFile = oversizedDir + "account.json";
    std::ofstream fout(oversizedFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    std::string garbage(1024 * 1024, 'x');
    for (int i = 0; i < 17; i++) {
        fout.write(garbage.data(), garbage.size());
    }
    fout.close();

    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    ErrCode errCode = dataDeal.Init(testOversizedUserId);
    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_TRUE(dataDeal.initOk_);

    struct stat fileStat = {};
    EXPECT_EQ(stat(oversizedFile.c_str(), &fileStat), 0);
    EXPECT_LT(fileStat.st_size, 1024);

    AccountInfo accountInfo;
    errCode = dataDeal.AccountInfoFromJson(accountInfo, testOversizedUserId);
    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.status_, ACCOUNT_STATE_UNBOUND);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.name_, DEFAULT_OHOS_ACCOUNT_NAME);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.uid_, DEFAULT_OHOS_ACCOUNT_UID);

    OHOS::ForceRemoveDirectory(oversizedDir);
}

/**
 * @tc.name: OversizedOhosAccountJsonBoundaryTest001
 * @tc.desc: Test exactly 16MB file does NOT trigger oversized removal
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, OversizedOhosAccountJsonBoundaryTest001, TestSize.Level3)
{
    int32_t testUserId = 303;
    std::string testDir = RESOURCE_ROOT_PATH + std::to_string(testUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(testDir));

    std::string testFile = testDir + "account.json";
    std::ofstream fout(testFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    std::string garbage(16 * 1024 * 1024, 'x');
    fout.write(garbage.data(), garbage.size());
    fout.close();

    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    ErrCode errCode = dataDeal.Init(testUserId);
    EXPECT_EQ(errCode, ERR_ACCOUNT_DATADEAL_JSON_FILE_CORRUPTION);

    OHOS::ForceRemoveDirectory(testDir);
}

/**
 * @tc.name: OversizedOhosAccountJsonBoundaryTest002
 * @tc.desc: Test 16MB+1 byte file triggers oversized removal and rebuild
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, OversizedOhosAccountJsonBoundaryTest002, TestSize.Level3)
{
    int32_t testUserId = 304;
    std::string testDir = RESOURCE_ROOT_PATH + std::to_string(testUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(testDir));

    std::string testFile = testDir + "account.json";
    std::ofstream fout(testFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    std::string garbage(16 * 1024 * 1024, 'x');
    fout.write(garbage.data(), garbage.size());
    fout.put('x');
    fout.close();

    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    ErrCode errCode = dataDeal.Init(testUserId);
    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_TRUE(dataDeal.initOk_);

#ifdef ENABLE_FILE_WATCHER
    EXPECT_TRUE(dataDeal.accountFileOperator_->GetValidDeleteFileOperationFlag(testFile));
    dataDeal.accountFileOperator_->SetValidDeleteFileOperationFlag(testFile, false);
#endif // ENABLE_FILE_WATCHER

    OHOS::ForceRemoveDirectory(testDir);
}

/**
 * @tc.name: OversizedOhosAccountJsonTest002
 * @tc.desc: Test oversized account.json is detected and rebuilt during ParseJsonFromFile
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, OversizedOhosAccountJsonTest002, TestSize.Level3)
{
    int32_t testOversizedUserId = 301;
    std::string oversizedDir = RESOURCE_ROOT_PATH + std::to_string(testOversizedUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(oversizedDir));

    std::string oversizedFile = oversizedDir + "account.json";
    std::ofstream fout(oversizedFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    std::string garbage(1024 * 1024, 'x');
    for (int i = 0; i < 17; i++) {
        fout.write(garbage.data(), garbage.size());
    }
    fout.close();

    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    dataDeal.initOk_ = true;

    AccountInfo accountInfo;
    ErrCode errCode = dataDeal.AccountInfoFromJson(accountInfo, testOversizedUserId);
    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.status_, ACCOUNT_STATE_UNBOUND);
    EXPECT_EQ(accountInfo.ohosAccountInfo_.name_, DEFAULT_OHOS_ACCOUNT_NAME);

    OHOS::ForceRemoveDirectory(oversizedDir);
}

/**
 * @tc.name: ParseJsonInvalidJsonTest001
 * @tc.desc: Test ParseJsonFromFile with invalid (non-oversized) JSON content (branch F)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, ParseJsonInvalidJsonTest001, TestSize.Level3)
{
    int32_t testUserId = 306;
    std::string testDir = RESOURCE_ROOT_PATH + std::to_string(testUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(testDir));
    std::string testFile = testDir + "account.json";
    std::ofstream fout(testFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    fout << "not_a_json";
    fout.close();
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    dataDeal.initOk_ = true;
    AccountInfo accountInfo;
    ErrCode errCode = dataDeal.AccountInfoFromJson(accountInfo, testUserId);
    EXPECT_EQ(errCode, ERR_ACCOUNT_DATADEAL_JSON_FILE_CORRUPTION);
    OHOS::ForceRemoveDirectory(testDir);
}

/**
 * @tc.name: MockInitReadFailTest001
 * @tc.desc: Test Init returns INPUT_FILE_ERROR when GetFileContentByPath fails (branch B)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, MockInitReadFailTest001, TestSize.Level3)
{
    int32_t testUserId = 310;
    std::string testDir = RESOURCE_ROOT_PATH + std::to_string(testUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(testDir));
    std::string testFile = testDir + "account.json";
    std::ofstream fout(testFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    fout << "{}";
    fout.close();
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    SetMockGetContentResult(ERR_OSACCOUNT_SERVICE_FILE_FIND_FILE_ERROR, "");
    ErrCode errCode = dataDeal.Init(testUserId);
    EXPECT_EQ(errCode, ERR_ACCOUNT_DATADEAL_INPUT_FILE_ERROR);
    ResetMockAccountFileOperator();
    OHOS::ForceRemoveDirectory(testDir);
}

/**
 * @tc.name: MockInitOversizedDeleteFailTest001
 * @tc.desc: Test Init oversized path when DeleteFile fails (branch A)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, MockInitOversizedDeleteFailTest001, TestSize.Level3)
{
    int32_t testUserId = 311;
    std::string testDir = RESOURCE_ROOT_PATH + std::to_string(testUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(testDir));
    std::string testFile = testDir + "account.json";
    std::ofstream fout(testFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    std::string garbage(16 * 1024 * 1024, 'x');
    fout.write(garbage.data(), garbage.size());
    fout.put('x');
    fout.close();
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    SetMockDeleteFileResult(ERR_OSACCOUNT_SERVICE_FILE_DELE_ERROR);
    ErrCode errCode = dataDeal.Init(testUserId);
    EXPECT_EQ(errCode, ERR_OK);
    EXPECT_TRUE(dataDeal.initOk_);
    ResetMockAccountFileOperator();
    OHOS::ForceRemoveDirectory(testDir);
}

/**
 * @tc.name: MockInitInvalidJsonDeleteFailTest001
 * @tc.desc: Test Init invalid-JSON path when DeleteFile fails (branch C)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, MockInitInvalidJsonDeleteFailTest001, TestSize.Level3)
{
    int32_t testUserId = 312;
    std::string testDir = RESOURCE_ROOT_PATH + std::to_string(testUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(testDir));
    std::string testFile = testDir + "account.json";
    std::ofstream fout(testFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    fout << "garbage";
    fout.close();
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    SetMockDeleteFileResult(ERR_OSACCOUNT_SERVICE_FILE_DELE_ERROR);
    ErrCode errCode = dataDeal.Init(testUserId);
    EXPECT_EQ(errCode, ERR_ACCOUNT_DATADEAL_JSON_FILE_CORRUPTION);
    ResetMockAccountFileOperator();
    OHOS::ForceRemoveDirectory(testDir);
}

/**
 * @tc.name: MockParseJsonOversizedDeleteFailTest001
 * @tc.desc: Test ParseJsonFromFile oversized path when DeleteFile fails (branch D)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, MockParseJsonOversizedDeleteFailTest001, TestSize.Level3)
{
    int32_t testUserId = 313;
    std::string testDir = RESOURCE_ROOT_PATH + std::to_string(testUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(testDir));
    std::string testFile = testDir + "account.json";
    std::ofstream fout(testFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    std::string garbage(16 * 1024 * 1024, 'x');
    fout.write(garbage.data(), garbage.size());
    fout.put('x');
    fout.close();
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    dataDeal.initOk_ = true;
    SetMockDeleteFileResult(ERR_OSACCOUNT_SERVICE_FILE_DELE_ERROR);
    AccountInfo accountInfo;
    ErrCode errCode = dataDeal.AccountInfoFromJson(accountInfo, testUserId);
    EXPECT_EQ(errCode, ERR_ACCOUNT_DATADEAL_INPUT_FILE_ERROR);
    ResetMockAccountFileOperator();
    OHOS::ForceRemoveDirectory(testDir);
}

/**
 * @tc.name: MockParseJsonReadFailTest001
 * @tc.desc: Test ParseJsonFromFile when GetFileContentByPath fails (branch E)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OhosAccountDataDealTest, MockParseJsonReadFailTest001, TestSize.Level3)
{
    int32_t testUserId = 314;
    std::string testDir = RESOURCE_ROOT_PATH + std::to_string(testUserId) + "/";
    ASSERT_TRUE(OHOS::ForceCreateDirectory(testDir));
    std::string testFile = testDir + "account.json";
    std::ofstream fout(testFile, std::ios::binary);
    ASSERT_TRUE(fout.is_open());
    fout << "{}";
    fout.close();
    OhosAccountDataDeal dataDeal(RESOURCE_ROOT_PATH);
#ifdef ENABLE_FILE_WATCHER
    dataDeal.checkCallbackFunc_ = nullptr;
#endif // ENABLE_FILE_WATCHER
    dataDeal.initOk_ = true;
    SetMockGetContentResult(ERR_OSACCOUNT_SERVICE_FILE_FIND_FILE_ERROR, "");
    AccountInfo accountInfo;
    ErrCode errCode = dataDeal.AccountInfoFromJson(accountInfo, testUserId);
    EXPECT_EQ(errCode, ERR_ACCOUNT_DATADEAL_INPUT_FILE_ERROR);
    ResetMockAccountFileOperator();
    OHOS::ForceRemoveDirectory(testDir);
}
