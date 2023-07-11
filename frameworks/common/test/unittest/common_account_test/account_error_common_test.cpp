/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "account_error_no.h"
#include "account_log_wrapper.h"

using namespace OHOS;
using namespace testing::ext;
using namespace OHOS::AccountSA;

class AccountErrorCodeConvertTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void AccountErrorCodeConvertTest::SetUpTestCase() {}

void AccountErrorCodeConvertTest::TearDownTestCase() {}

void AccountErrorCodeConvertTest::SetUp() {}

void AccountErrorCodeConvertTest::TearDown() {}

/**
 * @tc.name: AccountErrorCodeTest001
 * @tc.desc: App account ConvertToJSErrCode test
 * @tc.type: FUNC
 * @tc.require: issueI5RWXN
 */
HWTEST_F(AccountErrorCodeConvertTest, AccountErrorCodeConvertTest001, TestSize.Level0)
{
    // app account
    ACCOUNT_LOGI("AccountErrorCodeConvertTest001 AppAccount errorCode convert test");
    EXPECT_EQ(ConvertToJSErrCode(ERR_OK), ERR_JS_SUCCESS);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_ACCOUNT_NOT_EXIST), ERR_JS_ACCOUNT_NOT_FOUND);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_GET_ACCOUNT_INFO_BY_ID), ERR_JS_ACCOUNT_NOT_FOUND);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_ADD_EXISTING_ACCOUNT),
        ERR_JS_ACCOUNT_ALREADY_EXIST);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_ACCOUNT_MAX_SIZE), ERR_JS_ACCOUNT_NUMBER_REACH_LIMIT);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SUBSCRIBER_ALREADY_REGISTERED), ERR_JS_LISTENER_ALREADY_REGISTERED);
    EXPECT_EQ(
        ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_ASSOCIATED_DATA_OVER_SIZE), ERR_JS_CUSTOM_DATA_NUMBER_REACH_LIMIT);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_ASSOCIATED_DATA_KEY_NOT_EXIST), ERR_JS_CUSTOM_DATA_NOT_FOUND);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_ACCOUNT_CREDENTIAL_NOT_EXIST), ERR_JS_CREDENTIAL_NOT_EXIST);

    EXPECT_EQ(ConvertToJSErrCode(
        ERR_APPACCOUNT_KIT_NO_SPECIFIED_SUBSCRIBER_HAS_BEEN_REGISTERED), ERR_JS_LISTENER_NOT_REGISTERED);
    EXPECT_EQ(ConvertToJSErrCode(
        ERR_APPACCOUNT_SERVICE_OAUTH_AUTHENTICATOR_NOT_EXIST), ERR_JS_ACCOUNT_AUTHENTICATOR_NOT_EXIST);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_OAUTH_BUSY), ERR_JS_ACCOUNT_SERVICE_BUSY);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_OAUTH_LIST_MAX_SIZE), ERR_JS_AUTHORIZATION_LIST_TOO_LARGE);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_OAUTH_SESSION_NOT_EXIST), ERR_JS_SESSION_NOT_EXIST);
    EXPECT_EQ(
        ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_OAUTH_AUTHENTICATOR_CALLBACK_NOT_EXIST), ERR_JS_SESSION_NOT_EXIST);

    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_OAUTH_TYPE_NOT_EXIST), ERR_JS_AUTH_TYPE_NOT_FOUND);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_OAUTH_TOKEN_NOT_EXIST), ERR_JS_AUTH_TYPE_NOT_FOUND);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_OAUTH_TOKEN_MAX_SIZE), ERR_JS_TOKEN_NUMBER_REACH_LIMIT);

    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_KIT_WRITE_STRING_VECTOR), ERR_JS_SYSTEM_SERVICE_EXCEPTION);

    EXPECT_EQ(ConvertToJSErrCode(ERR_ACCOUNT_COMMON_INVALID_PARAMETER), ERR_JS_INVALID_PARAMETER);

    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_GET_BUNDLE_NAME), ERR_JS_APPLICATION_NOT_EXIST);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_GET_BUNDLE_INFO), ERR_JS_APPLICATION_NOT_EXIST);
    EXPECT_EQ(ConvertToJSErrCode(ERR_APPACCOUNT_SERVICE_DISABLE_APP_ACCESS_NOT_EXISTED), ERR_JS_APPLICATION_NOT_EXIST);

    EXPECT_EQ(ConvertToJSErrCode(
        ERR_APPACCOUNT_SERVICE_AUTHENTICATOR_MANAGER_PTR_IS_NULLPTR), ERR_JS_ACCOUNT_AUTHENTICATOR_SERVICE_EXCEPTION);
    EXPECT_EQ(ConvertToJSErrCode(
        ERR_APPACCOUNT_SERVICE_OAUTH_SERVICE_EXCEPTION), ERR_JS_ACCOUNT_AUTHENTICATOR_SERVICE_EXCEPTION);
}

/**
 * @tc.name: AccountErrorCodeConvertTest002
 * @tc.desc: Os account ConvertToJSErrCode test
 * @tc.type: FUNC
 * @tc.require: issueI5RWXN
 */
HWTEST_F(AccountErrorCodeConvertTest, AccountErrorCodeConvertTest002, TestSize.Level0)
{
    // os account
    ACCOUNT_LOGI("AccountErrorCodeConvertTest002 OsAccount errorCode convert test");
    EXPECT_EQ(ConvertToJSErrCode(ERR_ACCOUNT_COMMON_INVALID_PARAMETER), ERR_JS_INVALID_PARAMETER);

    EXPECT_EQ(ConvertToJSErrCode(ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR), ERR_JS_ACCOUNT_NOT_FOUND);

    EXPECT_EQ(
        ConvertToJSErrCode(ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_ALREADY_ACTIVE_ERROR), ERR_JS_ACCOUNT_ALREADY_ACTIVATED);

    EXPECT_EQ(
        ConvertToJSErrCode(ERR_OSACCOUNT_SERVICE_CONTROL_MAX_CAN_CREATE_ERROR), ERR_JS_ACCOUNT_NUMBER_REACH_LIMIT);

    EXPECT_EQ(ConvertToJSErrCode(ERR_OSACCOUNT_SERVICE_MANAGER_NOT_ENABLE_MULTI_ERROR), ERR_JS_MULTI_USER_NOT_SUPPORT);

    EXPECT_EQ(
        ConvertToJSErrCode(ERR_OSACCOUNT_SERVICE_MANAGER_CREATE_OSACCOUNT_TYPE_ERROR), ERR_JS_ACCOUNT_TYPE_NOT_SUPPORT);

    EXPECT_EQ(ConvertToJSErrCode(ERR_OSACCOUNT_SERVICE_MANAGER_ID_ERROR), ERR_JS_ACCOUNT_RESTRICTED);
    EXPECT_EQ(ConvertToJSErrCode(ERR_OSACCOUNT_SERVICE_CONTROL_CANNOT_DELETE_ID_ERROR), ERR_JS_ACCOUNT_RESTRICTED);
    EXPECT_EQ(ConvertToJSErrCode(ERR_OSACCOUNT_SERVICE_CONTROL_ID_CANNOT_CREATE_ERROR), ERR_JS_ACCOUNT_RESTRICTED);

    EXPECT_EQ(ConvertToJSErrCode(
        ERR_OSACCOUNT_KIT_NO_SPECIFIED_SUBSCRIBER_HAS_BEEN_REGISTERED), ERR_JS_LISTENER_NOT_REGISTERED);

    EXPECT_EQ(ConvertToJSErrCode(ERR_OSACCOUNT_SERVICE_PERMISSION_DENIED), ERR_JS_PERMISSION_DENIED);

    EXPECT_EQ(ConvertToJSErrCode(
        ERR_OSACCOUNT_SERVICE_CONTROL_UPDATE_FILE_NOT_EXISTS_ERROR), ERR_JS_SYSTEM_SERVICE_EXCEPTION);
}

/**
 * @tc.name: AccountErrorCodeTest003
 * @tc.desc: Other error code ConvertToJSErrCode test
 * @tc.type: FUNC
 * @tc.require: issueI5RWXN
 */
HWTEST_F(AccountErrorCodeConvertTest, AccountErrorCodeConvertTest003, TestSize.Level0)
{
    // other case
    ACCOUNT_LOGI("AccountErrorCodeConvertTest003 other case errorCode convert test");
    EXPECT_EQ(ConvertToJSErrCode(ERR_ACCOUNT_COMMON_PERMISSION_DENIED), ERR_JS_PERMISSION_DENIED);
}

/**
 * @tc.name: AccountErrorCodeTest004
 * @tc.desc: OsAccountConvertToJSErrCode test
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AccountErrorCodeConvertTest, AccountErrorCodeConvertTest004, TestSize.Level0)
{
    EXPECT_EQ(ConvertToJSErrCode(ERR_OSACCOUNT_SERVICE_INNER_DOMAIN_ALREADY_BIND_ERROR),
        ERR_JS_ACCOUNT_ALREADY_EXIST);
}

/**
 * @tc.name: AccountErrorCodeTest005
 * @tc.desc: OsAccountConvertToJSErrCode test
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AccountErrorCodeConvertTest, AccountErrorCodeConvertTest005, TestSize.Level0)
{
    EXPECT_EQ(ConvertToJSErrCode(-1),
        ERR_JS_SYSTEM_SERVICE_EXCEPTION);
    EXPECT_EQ(ConvertToJSErrCode(ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_ALREADY_EXIST),
        ERR_JS_DOMAIN_PLUGIN_ALREADY_REGISTERED);
}

/**
 * @tc.name: AccountErrorCodeTest006
 * @tc.desc: OsAccountConvertToJSErrCode test
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(AccountErrorCodeConvertTest, AccountErrorCodeConvertTest006, TestSize.Level0)
{
    EXPECT_EQ(ConvertToJSErrCode(ERR_ACCOUNT_COMMON_NOT_SYSTEM_APP_ERROR),
        ERR_JS_IS_NOT_SYSTEM_APP);
}