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

#ifndef OS_ACCOUNT_FRAMEWORKS_COMMON_ACCOUNT_ERROR_INCLUDE_ACCOUNT_ERROR_NO_H
#define OS_ACCOUNT_FRAMEWORKS_COMMON_ACCOUNT_ERROR_INCLUDE_ACCOUNT_ERROR_NO_H

#include <cstdint>
#include "errors.h"

namespace OHOS {
enum {
    ACCOUNT_MODULE_COMMON = 0x00,
    ACCOUNT_MODULE_ACCOUNTMGR,
    ACCOUNT_MODULE_DATADEAL,
    ACCOUNT_MODULE_IACCOUNT,
    ACCOUNT_MODULE_APPACCOUNT_KIT,
    ACCOUNT_MODULE_APPACCOUNT_SERVICE,
    ACCOUNT_MODULE_OSACCOUNT_KIT,
    ACCOUNT_MODULE_OSACCOUNT_SERVICE,
    ACCOUNT_MODULE_ACCOUNT_IAM_KIT,
    ACCOUNT_MODULE_OHOSACCOUNT_SERVICE,
    ACCOUNT_MODULE_DOMAIN_ACCOUNT_SERVICE,
    ACCOUNT_MODULE_OHOSACCOUNT_KIT
};

// Error code for common
constexpr ErrCode ACCOUNT_COMMON_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_COMMON);
enum {
    ERR_ACCOUNT_COMMON_NOT_INIT_ERROR = ACCOUNT_COMMON_ERR_OFFSET + 0x0001,
    ERR_ACCOUNT_COMMON_NOT_SYSTEM_APP_ERROR,
    ERR_ACCOUNT_COMMON_DUMP_JSON_ERROR,
    ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR,
    ERR_ACCOUNT_COMMON_NULL_PTR_ERROR,
    ERR_ACCOUNT_COMMON_WRITE_DESCRIPTOR_ERROR,
    ERR_ACCOUNT_COMMON_CHECK_DESCRIPTOR_ERROR,
    ERR_ACCOUNT_COMMON_PERMISSION_DENIED,
    ERR_ACCOUNT_COMMON_CHECK_KVSTORE_ERROR,
    ERR_ACCOUNT_COMMON_DELETE_KEY_FROM_KVSTORE_ERROR,
    ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR,
    ERR_ACCOUNT_COMMON_CONNECT_ABILITY_MANAGER_SERVICE_ERROR,
    ERR_ACCOUNT_COMMON_INTERFACE_NOT_SUPPORT_ERROR,
    ERR_ACCOUNT_COMMON_CONNECT_BUNDLE_MANAGER_SERVICE_ERROR,
    ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY_MANAGER,
    ERR_ACCOUNT_COMMON_GET_SYSTEM_ABILITY,
    ERR_ACCOUNT_COMMON_GET_PROXY,
    ERR_ACCOUNT_COMMON_CREATE_DEATH_RECIPIENT,
    ERR_ACCOUNT_COMMON_ADD_DEATH_RECIPIENT,
    ERR_ACCOUNT_COMMON_WRITE_PARCEL_ERROR,
    ERR_ACCOUNT_COMMON_READ_PARCEL_ERROR,
    ERR_ACCOUNT_COMMON_INVALID_PARAMETER,
    ERR_ACCOUNT_COMMON_POST_TASK,
    ERR_ACCOUNT_COMMON_FILE_OPEN_FAILED,
    ERR_ACCOUNT_COMMON_FILE_WRITE_FAILED,
    ERR_ACCOUNT_COMMON_FILE_READ_FAILED,
    ERR_ACCOUNT_COMMON_ACCOUNT_NOT_EXIST_ERROR,
    ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT,
    ERR_ACCOUNT_COMMON_NOT_AUTHENTICATED,
    ERR_ACCOUNT_COMMON_NAME_HAD_EXISTED,
    ERR_ACCOUNT_COMMON_SHORT_NAME_HAD_EXISTED,
    ERR_ACCOUNT_COMMON_DISPLAY_ID_NOT_EXIST_ERROR,
    ERR_ACCOUNT_COMMON_ACCOUNT_IN_DISPLAY_ID_NOT_FOUND_ERROR,
    ERR_ACCOUNT_COMMON_ACCOUNT_SUBSCRIBE_NOT_FOUND_ERROR
};

// Error code for AccountMgr
constexpr ErrCode ACCOUNT_ACCOUNTMGR_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_ACCOUNTMGR);
enum {
    ERR_ACCOUNT_MGR_DUMP_ERROR = ACCOUNT_ACCOUNTMGR_ERR_OFFSET + 0x0001,
    ERR_ACCOUNT_MGR_GET_REMOTE_SA_ERROR,
    ERR_ACCOUNT_MGR_CONNECT_SA_ERROR,
    ERR_ACCOUNT_MGR_ADD_TO_SA_ERROR,
    ERR_ACCOUNT_MGR_OHOS_MGR_INIT_ERROR
};

// Error code for DataDeal module
constexpr ErrCode ACCOUNT_DATADEAL_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_DATADEAL);
enum {
    ERR_ACCOUNT_DATADEAL_INPUT_FILE_ERROR = ACCOUNT_DATADEAL_ERR_OFFSET + 0x0001,
    ERR_ACCOUNT_DATADEAL_NOT_READY,
    ERR_ACCOUNT_DATADEAL_JSON_FILE_CORRUPTION
};

// Error code for IAccount
constexpr ErrCode ACCOUNT_IACCOUNT_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_IACCOUNT);
enum {
    ERR_ACCOUNT_ZIDL_ACCOUNT_STUB_ERROR = ACCOUNT_IACCOUNT_ERR_OFFSET + 0x0001,
    ERR_ACCOUNT_ZIDL_ACCOUNT_SERVICE_ERROR,
    ERR_ACCOUNT_ZIDL_MGR_NOT_READY_ERROR,
    ERR_ACCOUNT_ZIDL_ACCOUNT_STUB_USERID_ERROR
};

// Error code for APPACCOUNT_KIT
constexpr ErrCode APPACCOUNT_KIT_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_APPACCOUNT_KIT);
enum {
    // AppAccount::GetAppAccountProxy 4456449
    ERR_APPACCOUNT_KIT_GET_APP_ACCOUNT_SERVICE = APPACCOUNT_KIT_ERR_OFFSET + 0x0001,
    ERR_APPACCOUNT_KIT_CREATE_APP_ACCOUNT_DEATH_RECIPIENT,
    // AppAccount 4456455
    ERR_APPACCOUNT_KIT_SUBSCRIBER_IS_NULLPTR,
    ERR_APPACCOUNT_KIT_GET_SUBSCRIBE_INFO,
    ERR_APPACCOUNT_KIT_GET_OWNERS,
    ERR_APPACCOUNT_KIT_SET_OWNERS,
    ERR_APPACCOUNT_KIT_SUBSCRIBE,
    ERR_APPACCOUNT_KIT_NO_SPECIFIED_SUBSCRIBER_HAS_BEEN_REGISTERED,
    // AppAccountProxy & AppAccountEventProxy 4456480
    ERR_APPACCOUNT_KIT_SEND_REQUEST,
    ERR_APPACCOUNT_KIT_READ_PARCELABLE_APP_ACCOUNT_INFO,
    // AppAccountStub 4456503
    ERR_APPACCOUNT_KIT_READ_PARCELABLE_VECTOR_ACCOUNT_INFO,
};

// Error code for APPACCOUNT_SERVICE
constexpr ErrCode APPACCOUNT_SERVICE_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_APPACCOUNT_SERVICE);
enum {
    // 4521985
    ERR_APPACCOUNT_SERVICE_ACCOUNT_NOT_EXIST = APPACCOUNT_SERVICE_ERR_OFFSET + 0x0001,
    // InnerAppAccountManager 4521995
    ERR_APPACCOUNT_SERVICE_DATA_STORAGE_PTR_IS_NULLPTR,
    ERR_APPACCOUNT_SERVICE_ACCOUNT_MAX_SIZE,
    ERR_APPACCOUNT_SERVICE_GET_ACCOUNT_INFO_BY_ID,
    ERR_APPACCOUNT_SERVICE_GET_ASSOCIATED_DATA,
    ERR_APPACCOUNT_SERVICE_SET_ASSOCIATED_DATA,
    ERR_APPACCOUNT_SERVICE_ACCOUNT_CREDENTIAL_NOT_EXIST,
    ERR_APPACCOUNT_SERVICE_GET_OWNER,
    ERR_APPACCOUNT_SERVICE_GET_BUNDLE_NAME,
    ERR_APPACCOUNT_SERVICE_GET_APP_INDEX,
    ERR_APPACCOUNT_SERVICE_GET_BUNDLE_INFO,
    ERR_APPACCOUNT_SERVICE_ADD_EXISTING_ACCOUNT,
    ERR_APPACCOUNT_SERVICE_ENABLE_APP_ACCESS_ALREADY_EXISTS,
    ERR_APPACCOUNT_SERVICE_DISABLE_APP_ACCESS_NOT_EXISTED,
    ERR_APPACCOUNT_SERVICE_ASSOCIATED_DATA_KEY_NOT_EXIST,
    ERR_APPACCOUNT_SERVICE_ASSOCIATED_DATA_OVER_SIZE,
    // AppAccountOAuth
    ERR_APPACCOUNT_SERVICE_OAUTH_TOKEN_NOT_EXIST,
    ERR_APPACCOUNT_SERVICE_OAUTH_TYPE_NOT_EXIST,
    ERR_APPACCOUNT_SERVICE_OAUTH_TOKEN_MAX_SIZE,
    ERR_APPACCOUNT_SERVICE_OAUTH_LIST_MAX_SIZE,
    ERR_APPACCOUNT_SERVICE_OAUTH_AUTHENTICATOR_NOT_EXIST,
    ERR_APPACCOUNT_SERVICE_OAUTH_AUTHENTICATOR_CALLBACK_NOT_EXIST,
    ERR_APPACCOUNT_SERVICE_OAUTH_SESSION_NOT_EXIST,
    ERR_APPACCOUNT_SERVICE_OAUTH_BUSY,
    ERR_APPACCOUNT_SERVICE_OAUTH_INVALID_RESPONSE,
    ERR_APPACCOUNT_SERVICE_OAUTH_SERVICE_EXCEPTION,
    // AppAccountSubscribeManager 4522034
    ERR_APPACCOUNT_SERVICE_OWNERS_SIZE_IS_ZERO,
    ERR_APPACCOUNT_SUBSCRIBER_ALREADY_REGISTERED,
    ERR_APPACCOUNT_SERVICE_OTHER,
};

// Error code for OSACCOUNT_KIT
constexpr ErrCode ACCOUNT_OSACCOUNT_KIT_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_OSACCOUNT_KIT);
enum {
    ERR_OSACCOUNT_KIT_CREATE_OS_ACCOUNT_FOR_DOMAIN_ERROR = ACCOUNT_OSACCOUNT_KIT_ERR_OFFSET + 0x0001,
    ERR_OSACCOUNT_KIT_READ_LOCALNAME_ERROR,
    ERR_OSACCOUNT_KIT_READ_IN_LOCAL_ID_ERROR,
    ERR_OSACCOUNT_KIT_GET_APP_ACCOUNT_SERVICE_ERROR,
    ERR_OSACCOUNT_KIT_CREATE_APP_ACCOUNT_DEATH_RECIPIENT_ERROR,
    ERR_OSACCOUNT_KIT_IS_OS_ACCOUNT_ACTIVED_ERROR,
    ERR_OSACCOUNT_KIT_IS_OS_ACCOUNT_CONSTRAINT_ENABLE_ERROR,
    ERR_OSACCOUNT_KIT_IS_OS_ACCOUNT_VERIFIED_ERROR,
    ERR_OSACCOUNT_KIT_GET_CREATED_OS_ACCOUNT_COUNT_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_ALL_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_KIT_QUERY_CURRENT_OS_ACCOUNT_ERROR,
    ERR_OSACCOUNT_KIT_READ_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_KIT_GET_DISTRIBUTED_VIRTUAL_DEVICE_ID_ERROR,
    ERR_OSACCOUNT_KIT_SUBSCRIBE_ERROR,
    ERR_OSACCOUNT_KIT_NO_SPECIFIED_SUBSCRIBER_HAS_BEEN_REGISTERED,
};

// Error code for OSACCOUNT_SERVICE
constexpr ErrCode ACCOUNT_OSACCOUNT_SERVICE_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_OSACCOUNT_SERVICE);
enum {
    // 4653057
    ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR = ACCOUNT_OSACCOUNT_SERVICE_ERR_OFFSET + 0x0001,
    ERR_OSACCOUNT_SERVICE_MANAGER_NOT_ENABLE_MULTI_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_ID_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_CREATE_OSACCOUNT_TYPE_ERROR,
    // 4653067
    ERR_OSACCOUNT_SERVICE_INNER_SELECT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_IS_UNVERIFIED_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_IS_UNCOMPLETED_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_ALREADY_ACTIVE_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_STOP_ACTIVE_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_REMOVE_ACCOUNT_ACTIVED_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_DOMAIN_ALREADY_BIND_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_PLUGIN_NOT_EXIST_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_PLUGIN_NOT_ALLOWED_CREATION_ERROR,
    // 4653108
    ERR_OSACCOUNT_SERVICE_CONTROL_GET_TYPE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_INSERT_FILE_EXISTS_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_UPDATE_FILE_NOT_EXISTS_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_MAX_CAN_CREATE_ERROR,
    ERR_OSACCOUNT_SERVICE_LOGGED_IN_ACCOUNTS_OVERSIZE,
    ERR_OSACCOUNT_SERVICE_CONTROL_SELECT_CAN_USE_ID_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_ID_CANNOT_CREATE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_CANNOT_DELETE_ID_ERROR,
    // 4653131
    ERR_OSACCOUNT_SERVICE_FILE_DELE_ERROR,
    ERR_OSACCOUNT_SERVICE_FILE_FIND_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_FILE_CREATE_DIR_ERROR,
    ERR_OSACCOUNT_SERVICE_FILE_CHANGE_DIR_MODE_ERROR,
    ERR_OSACCOUNT_SERVICE_DATA_STORAGE_KEY_NOT_EXISTS_ERROR,
    ERR_OSACCOUNT_SERVICE_DATA_STORAGE_KEY_EXISTED_ERROR,
    ERR_OSACCOUNT_SERVICE_ACCOUNT_INFO_EMPTY_ERROR,
    // 4653142
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_AM_ACCOUNT_START_ERROR,
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_BM_ACCOUNT_CREATE_ERROR,
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_BM_ACCOUNT_DELETE_ERROR,
    ERR_OSACCOUNT_SERVICE_OS_FILE_GET_CONFIG_ERROR,
    ERR_OSACCOUNT_SERVICE_OS_FILE_GET_CONSTRAINTS_LITS_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_OPERATING_ERROR,
    ERR_OSACCOUNT_SERVICE_GET_DATA_FROM_BASE_CONSTRAINTS_FILE_EMPTY,
    ERR_OSACCOUNT_SERVICE_GET_DATA_FROM_GLOBAL_CONSTRAINTS_FILE_EMPTY,
    ERR_OSACCOUNT_SERVICE_GET_DATA_FROM_SPECIFIC_CONSTRAINTS_FILE_EMPTY,
    ERR_OSACCOUNT_SERVICE_STORAGE_PREPARE_ADD_USER_FAILED,
};

// Error code for ACCOUNT_IAM_KIT
constexpr ErrCode ACCOUNT_IAM_KIT_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_ACCOUNT_IAM_KIT);
enum {
    ERR_ACCOUNT_IAM_KIT_INPUTER_ALREADY_REGISTERED = ACCOUNT_IAM_KIT_ERR_OFFSET + 0x0001,
    ERR_ACCOUNT_IAM_KIT_INPUTER_NOT_REGISTERED,
    ERR_ACCOUNT_IAM_KIT_GET_USERID_FAIL,
    ERR_ACCOUNT_IAM_UNSUPPORTED_AUTH_TYPE
};

// Error code for OHOSACCOUNT_SERVICE
constexpr ErrCode ACCOUNT_OHOSACCOUNT_SERVICE_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_OHOSACCOUNT_SERVICE);
enum {
    ERR_OHOSACCOUNT_SERVICE_FILE_CHANGE_DIR_MODE_ERROR = ACCOUNT_OHOSACCOUNT_SERVICE_ERR_OFFSET + 0x0001,
};

// Error code for DOMAIN_ACCOUNT_SERVICE
constexpr ErrCode DOMAIN_ACCOUNT_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_DOMAIN_ACCOUNT_SERVICE);
enum {
    ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_ALREADY_EXIST = DOMAIN_ACCOUNT_ERR_OFFSET + 0x0001,
    ERR_DOMAIN_ACCOUNT_SERVICE_PLUGIN_NOT_EXIST,
    ERR_DOMAIN_ACCOUNT_SERVICE_NOT_DOMAIN_ACCOUNT,
    ERR_DOMAIN_ACCOUNT_SERVICE_LISTENER_NOT_EXIT,
    ERR_DOMAIN_ACCOUNT_SERVICE_INVALID_CALLING_UID,
    ERR_DOMAIN_ACCOUNT_SERVICE_ERR_CODE_LIMIT,
};

// Error code for OHOSACCOUNT_KIT
constexpr ErrCode ACCOUNT_OHOSACCOUNT_KIT_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_OHOSACCOUNT_KIT);
enum {
    ERR_OHOSACCOUNT_KIT_NO_SPECIFIED_CALLBACK_HAS_BEEN_REGISTERED = ACCOUNT_OHOSACCOUNT_KIT_ERR_OFFSET + 0x0001,
    ERR_OHOSACCOUNT_KIT_SUBSCRIBE_ERROR,
    ERR_OHOSACCOUNT_KIT_SUBSCRIBE_MAX_SIZE_ERROR,
    ERR_OHOSACCOUNT_KIT_CALLBACK_ALREADY_REGISTERED_ERROR,
};

enum JSErrorCode {
    ERR_JS_SUCCESS = 0,
    ERR_JS_PERMISSION_DENIED = 201,
    ERR_JS_IS_NOT_SYSTEM_APP = 202,
    ERR_JS_PARAMETER_ERROR = 401,
    ERR_JS_CAPABILITY_NOT_SUPPORTED = 801,
    ERR_JS_SYSTEM_SERVICE_EXCEPTION = 12300001,
    ERR_JS_INVALID_PARAMETER = 12300002,
    ERR_JS_ACCOUNT_NOT_FOUND = 12300003,
    ERR_JS_ACCOUNT_ALREADY_EXIST = 12300004,
    ERR_JS_MULTI_USER_NOT_SUPPORT = 12300005,
    ERR_JS_ACCOUNT_TYPE_NOT_SUPPORT = 12300006,
    ERR_JS_ACCOUNT_NUMBER_REACH_LIMIT = 12300007,
    ERR_JS_ACCOUNT_RESTRICTED = 12300008,
    ERR_JS_ACCOUNT_ALREADY_ACTIVATED = 12300009,
    ERR_JS_ACCOUNT_SERVICE_BUSY = 12300010,
    ERR_JS_LISTENER_ALREADY_REGISTERED = 12300011,
    ERR_JS_LISTENER_NOT_REGISTERED = 12300012,
    ERR_JS_PLUGIN_NETWORK_EXCEPTION = 12300013,
    ERR_JS_ACCOUNT_NOT_AUTHENTICATED = 12300014,
    ERR_JS_ACCOUNT_SHORT_NAME_ALREADY_EXIST = 12300015,
    ERR_JS_ACCOUNT_LOGGED_IN_ACCOUNTS_OVERSIZE = 12300016,

    ERR_JS_AUTH_CREDENTIAL_WRONG_ERROR = 12300101,
    ERR_JS_CREDENTIAL_NOT_EXIST = 12300102,
    ERR_JS_CREDENTIAL_INPUTER_ALREADY_EXIST = 12300103,
    ERR_JS_CREDENTIAL_INPUTER_NOT_EXIST = 12300104,
    ERR_JS_TRUST_LEVEL_NOT_SUPPORTED = 12300105,
    ERR_JS_AUTH_TYPE_NOT_SUPPORTED = 12300106,
    ERR_JS_AUTH_TYPE_NOT_FOUND = 12300107,
    ERR_JS_SESSION_NOT_EXIST = 12300108,
    ERR_JS_AUTH_CANCELLED = 12300109,
    ERR_JS_AUTH_SERVICE_LOCKED = 12300110,
    ERR_JS_AUTH_TIMEOUT = 12300111,
    ERR_JS_AUTH_SERVICE_BUSY = 12300112,
    ERR_JS_ACCOUNT_AUTHENTICATOR_NOT_EXIST = 12300113,
    ERR_JS_ACCOUNT_AUTHENTICATOR_SERVICE_EXCEPTION = 12300114,
    ERR_JS_CREDENTIAL_NUMBER_REACH_LIMIT = 12300115,
    ERR_JS_SESSION_TIMEOUT = 12300116,

    ERR_JS_DOMAIN_PLUGIN_ALREADY_REGISTERED = 12300201,

    ERR_JS_INVALID_CONTEXT_ID = 12300002,

    ERR_JS_APPLICATION_NOT_EXIST = 12400001,
    ERR_JS_CUSTOM_DATA_NOT_FOUND = 12400002,
    ERR_JS_CUSTOM_DATA_NUMBER_REACH_LIMIT = 12400003,
    ERR_JS_TOKEN_NUMBER_REACH_LIMIT = 12400004,
    ERR_JS_AUTHORIZATION_LIST_TOO_LARGE = 12400005
};

int32_t ConvertToJSErrCode(int32_t nativeErrCode);
}  // namespace OHOS

#endif  // OS_ACCOUNT_FRAMEWORKS_COMMON_ACCOUNT_ERROR_INCLUDE_ACCOUNT_ERROR_NO_H
