/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include <stdint.h>
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
    ACCOUNT_MODULE_ACCOUNT_IAM_SERVICE,
    ACCOUNT_MODULE_OHOSACCOUNT_KIT,
    ACCOUNT_MODULE_OHOSACCOUNT_SERVICE
};

// Error code for common
constexpr ErrCode ACCOUNT_COMMON_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_COMMON);
enum {
    ERR_ACCOUNT_COMMON_NOT_INIT_ERROR = ACCOUNT_COMMON_ERR_OFFSET + 0x0001,
    ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR,
    ERR_ACCOUNT_COMMON_NULL_PTR_ERROR,
    ERR_ACCOUNT_COMMON_WRITE_DESCRIPTOR_ERROR,
    ERR_ACCOUNT_COMMON_CHECK_DESCRIPTOR_ERROR,
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
    ERR_ACCOUNT_DATADEAL_FILE_PARSE_FAILED,
    ERR_ACCOUNT_DATADEAL_DIGEST_ERROR,
    ERR_ACCOUNT_DATADEAL_FILE_WRITE_FAILED,
    ERR_ACCOUNT_DATADEAL_JSON_KEY_NOT_EXIST,
    ERR_ACCOUNT_DATADEAL_NOT_READY,
    ERR_ACCOUNT_DATADEAL_JSON_FILE_CORRUPTION
};

// Error code for IAccount
constexpr ErrCode ACCOUNT_IACCOUNT_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_IACCOUNT);
enum {
    ERR_ACCOUNT_ZIDL_ACCOUNT_INFO_CHECK_ERROR = ACCOUNT_IACCOUNT_ERR_OFFSET + 0x0001,
    ERR_ACCOUNT_ZIDL_WRITE_DESCRIPTOR_ERROR,
    ERR_ACCOUNT_ZIDL_READ_RESULT_ERROR,
    ERR_ACCOUNT_ZIDL_WRITE_RESULT_ERROR,
    ERR_ACCOUNT_ZIDL_ACCOUNT_PROXY_ERROR,
    ERR_ACCOUNT_ZIDL_ACCOUNT_STUB_ERROR,
    ERR_ACCOUNT_ZIDL_ACCOUNT_SERVICE_ERROR,
    ERR_ACCOUNT_ZIDL_ACCOUNT_SEND_REQUEST_ERROR,
    ERR_ACCOUNT_ZIDL_WRITE_NAME_ERROR,
    ERR_ACCOUNT_ZIDL_WRITE_UID_ERROR,
    ERR_ACCOUNT_ZIDL_WRITE_ACCOUNT_STATUS_ERROR,
    ERR_ACCOUNT_ZIDL_MGR_NOT_READY_ERROR,
    ERR_ACCOUNT_ZIDL_CHECK_PERMISSION_ERROR,
    ERR_ACCOUNT_ZIDL_WRITE_PARCEL_DATA_ERROR,
    ERR_ACCOUNT_ZIDL_INVALID_RESULT_ERROR,
    ERR_ACCOUNT_ZIDL_ACCOUNT_STUB_USERID_ERROR
};

// Error code for APPACCOUNT_KIT
constexpr ErrCode APPACCOUNT_KIT_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_APPACCOUNT_KIT);
enum {
    // AppAccount::GetAppAccountProxy 4456449
    ERR_APPACCOUNT_KIT_GET_SYSTEM_ABILITY_MANAGER = APPACCOUNT_KIT_ERR_OFFSET + 0x0001,
    ERR_APPACCOUNT_KIT_GET_ACCOUNT_SYSTEM_ABILITY,
    ERR_APPACCOUNT_KIT_CAST_ACCOUNT_PROXY,
    ERR_APPACCOUNT_KIT_GET_APP_ACCOUNT_SERVICE,
    ERR_APPACCOUNT_KIT_GET_APP_ACCOUNT_PROXY,
    ERR_APPACCOUNT_KIT_CREATE_APP_ACCOUNT_DEATH_RECIPIENT,
    // AppAccount 4456455
    ERR_APPACCOUNT_KIT_NAME_IS_EMPTY,
    ERR_APPACCOUNT_KIT_NAME_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_NAME_CONTAINS_SPECIAL_CHARACTERS,
    ERR_APPACCOUNT_KIT_EXTRA_INFO_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_AUTHORIZED_APP_IS_EMPTY,
    ERR_APPACCOUNT_KIT_AUTHORIZED_APP_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_ASSOCIATED_KEY_IS_EMPTY,
    ERR_APPACCOUNT_KIT_ASSOCIATED_KEY_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_ASSOCIATED_VALUE_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_CREDENTIAL_TYPE_IS_EMPTY,
    ERR_APPACCOUNT_KIT_CREDENTIAL_TYPE_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_CREDENTIAL_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_TOKEN_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_OWNER_IS_EMPTY,
    ERR_APPACCOUNT_KIT_OWNER_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_AUTH_TYPE_OUT_OF_RANGE,
    ERR_APPACCOUNT_KIT_SUBSCRIBER_HAS_NO_OWNER,
    ERR_APPACCOUNT_KIT_INVALID_PARAMETER,
    ERR_APPACCOUNT_KIT_SUBSCRIBER_IS_NULLPTR,
    ERR_APPACCOUNT_KIT_GET_SUBSCRIBE_INFO,
    ERR_APPACCOUNT_KIT_GET_OWNERS,
    ERR_APPACCOUNT_KIT_SET_OWNERS,
    ERR_APPACCOUNT_KIT_APP_ACCOUNT_PROXY_IS_NULLPTR,
    ERR_APPACCOUNT_KIT_SUBSCRIBE,
    ERR_APPACCOUNT_KIT_NO_SPECIFIED_SUBSCRIBER_HAS_BEEN_REGISTERED,
    // AppAccountProxy & AppAccountEventProxy 4456480
    ERR_APPACCOUNT_KIT_WRITE_STRING_NAME,
    ERR_APPACCOUNT_KIT_WRITE_STRING_OWNER,
    ERR_APPACCOUNT_KIT_WRITE_STRING_EXTRA_INFO,
    ERR_APPACCOUNT_KIT_WRITE_STRING_AUTHORIZED_APP,
    ERR_APPACCOUNT_KIT_WRITE_STRING_KEY,
    ERR_APPACCOUNT_KIT_WRITE_STRING_VALUE,
    ERR_APPACCOUNT_KIT_WRITE_BOOL_SYNC_ENABLE,
    ERR_APPACCOUNT_KIT_WRITE_BOOL_VISIBILITY,
    ERR_APPACCOUNT_KIT_WRITE_STRING_ASSOCIATED_KEY,
    ERR_APPACCOUNT_KIT_WRITE_STRING_CREDENTIAL_TYPE,
    ERR_APPACCOUNT_KIT_WRITE_STRING_CREDENTIAL,
    ERR_APPACCOUNT_KIT_WRITE_STRING_TOKEN,
    ERR_APPACCOUNT_KIT_WRITE_STRING_AUTH_TYPE,
    ERR_APPACCOUNT_KIT_WRITE_STRING_BUNDLE_NAME,
    ERR_APPACCOUNT_KIT_WRITE_STRING_ABILITY_NAME,
    ERR_APPACCOUNT_KIT_WRITE_STRING_SESSION_ID,
    ERR_APPACCOUNT_KIT_WRITE_STRING_VECTOR,
    ERR_APPACCOUNT_KIT_WRITE_PARCELABLE_SUBSCRIBE_INFO,
    ERR_APPACCOUNT_KIT_WRITE_PARCELABLE_EVENT_LISTENER,
    ERR_APPACCOUNT_KIT_WRITE_PARCELABLE_OPTIONS,
    ERR_APPACCOUNT_KIT_WRITE_PARCELABLE_CALLBACK,
    ERR_APPACCOUNT_KIT_REMOTE_IS_NULLPTR,
    ERR_APPACCOUNT_KIT_SEND_REQUEST,
    ERR_APPACCOUNT_KIT_READ_PARCELABLE_APP_ACCOUNT_INFO,
    // AppAccountStub 4456503
    ERR_APPACCOUNT_KIT_READ_STRING_NAME,
    ERR_APPACCOUNT_KIT_READ_STRING_OWNER,
    ERR_APPACCOUNT_KIT_READ_STRING_AUTHORIZED_APP,
    ERR_APPACCOUNT_KIT_READ_STRING_ASSOCIATED_DATA,
    ERR_APPACCOUNT_KIT_READ_STRING_CREDENTIAL_TYPE,
    // AppAccountEventStub
    ERR_APPACCOUNT_KIT_READ_PARCELABLE_VECTOR_ACCOUNT_INFO,
};

// Error code for APPACCOUNT_SERVICE
constexpr ErrCode APPACCOUNT_SERVICE_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_APPACCOUNT_SERVICE);
enum {
    // 4521985
    ERR_APPACCOUNT_SERVICE_NAME_IS_EMPTY = APPACCOUNT_SERVICE_ERR_OFFSET + 0x0001,
    ERR_APPACCOUNT_SERVICE_NAME_OUT_OF_RANGE,
    ERR_APPACCOUNT_SERVICE_NAME_INVALID,
    ERR_APPACCOUNT_SERVICE_EXTRA_INFO_OUT_OF_RANGE,
    ERR_APPACCOUNT_SERVICE_OWNERS_ARE_EMPTY,
    ERR_APPACCOUNT_SERVICE_PERMISSION_NAME_IS_EMPTY,
    ERR_APPACCOUNT_SERVICE_BUNDLE_NAME_IS_EMPTY,
    ERR_APPACCOUNT_SERVICE_BUNDLE_NAME_IS_THE_SAME,
    ERR_APPACCOUNT_SERVICE_INVALID_PARAMETER,
    ERR_APPACCOUNT_SERVICE_ACCOUNT_NOT_EXIST,
    ERR_APPACCOUNT_SERVICE_PERMISSION_DENIED,
    // InnerAppAccountManager 4521995
    ERR_APPACCOUNT_SERVICE_CONTROL_MANAGER_PTR_IS_NULLPTR,
    ERR_APPACCOUNT_SERVICE_SUBSCRIBE_MANAGER_PTR_IS_NULLPTR,
    ERR_APPACCOUNT_SERVICE_SESSION_MANAGER_PTR_IS_NULLPTR,
    ERR_APPACCOUNT_SERVICE_AUTHENTICATOR_MANAGER_PTR_IS_NULLPTR,
    // AppAccountControlManager 4521999
    ERR_APPACCOUNT_SERVICE_CREATE_EVENT_HANDLER,
    ERR_APPACCOUNT_SERVICE_DATA_STORAGE_PTR_IS_NULLPTR,
    ERR_APPACCOUNT_SERVICE_ACCOUNT_MAX_SIZE,
    ERR_APPACCOUNT_SERVICE_GET_ACCOUNT_INFO_BY_ID,
    ERR_APPACCOUNT_SERVICE_GET_ACCOUNT_INFO_BY_OWNER,
    ERR_APPACCOUNT_SERVICE_GET_SYNC_ENABLE,
    ERR_APPACCOUNT_SERVICE_GET_ASSOCIATED_DATA,
    ERR_APPACCOUNT_SERVICE_SET_ASSOCIATED_DATA,
    ERR_APPACCOUNT_SERVICE_ACCOUNT_CREDENTIAL_NOT_EXIST,
    ERR_APPACCOUNT_SERVICE_SET_ACCOUNT_CREDENTIAL,
    ERR_APPACCOUNT_SERVICE_ADD_ACCOUNT_INFO,
    ERR_APPACCOUNT_SERVICE_SAVE_ACCOUNT_INFO,
    ERR_APPACCOUNT_SERVICE_REMOVE_INFO_BY_KEY,
    ERR_APPACCOUNT_SERVICE_GET_EXTRA_INFO,
    ERR_APPACCOUNT_SERVICE_SET_EXTRA_INFO,
    ERR_APPACCOUNT_SERVICE_GET_OWNER,
    ERR_APPACCOUNT_SERVICE_GET_BUNDLE_NAME,
    ERR_APPACCOUNT_SERVICE_GET_APP_INDEX,
    ERR_APPACCOUNT_SERVICE_GET_BUNDLE_INFO,
    ERR_APPACCOUNT_SERVICE_GET_AUTHORIZED_APPS,
    ERR_APPACCOUNT_SERVICE_ADD_EXISTING_ACCOUNT,
    ERR_APPACCOUNT_SERVICE_ENABLE_APP_ACCESS_ALREADY_EXISTS,
    ERR_APPACCOUNT_SERVICE_DISABLE_APP_ACCESS_NOT_EXISTED,
    ERR_APPACCOUNT_SERVICE_GET_ACCOUNT_CACHE,
    ERR_APPACCOUNT_SERVICE_DUMP_JSON,
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
    ERR_APPACCOUNT_SERVICE_SUBSCRIBE_INFO_PTR_IS_NULLPTR,
    ERR_APPACCOUNT_SERVICE_EVENT_LISTENER_IS_NULLPTR,
    ERR_APPACCOUNT_SERVICE_GET_OWNERS,
    ERR_APPACCOUNT_SERVICE_OWNERS_SIZE_IS_ZERO,
    ERR_APPACCOUNT_SERVICE_SUBSCRIBE_RECORD_PTR_IS_NULLPTR,
    ERR_APPACCOUNT_SERVICE_SUBSCRIBE_PERMISSION_DENIED,
    ERR_APPACCOUNT_SUBSCRIBER_ALREADY_REGISTERED,
    ERR_APPACCOUNT_SERVICE_WRITE_PARCEL,
    ERR_APPACCOUNT_SERVICE_OTHER,
};

// Error code for OSACCOUNT_KIT
constexpr ErrCode ACCOUNT_OSACCOUNT_KIT_ERR_OFFSET = ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_OSACCOUNT_KIT);
enum {
    // 4587521
    ERR_OSACCOUNT_KIT_WRITE_LOCALNAME_ERROR = ACCOUNT_OSACCOUNT_KIT_ERR_OFFSET + 0x0001,
    ERR_OSACCOUNT_KIT_WRITE_OSACCOUNT_TYPE_ERROR,
    ERR_OSACCOUNT_KIT_CREATE_OS_ACCOUNT_ERROR,
    ERR_OSACCOUNT_KIT_CREATE_OS_ACCOUNT_FOR_DOMAIN_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_DOMAIN_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_DOMAIN_ACCOUNT_NAME_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_INT_LOCAL_ID_ERROR,
    ERR_OSACCOUNT_KIT_READ_LOCALNAME_ERROR,
    ERR_OSACCOUNT_KIT_REMOVE_OSACCOUNT_ERROR,
    ERR_OSACCOUNT_KIT_READ_OSACCOUNT_TYPE_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_REPLY_ERROR,
    ERR_OSACCOUNT_KIT_READ_IN_LOCAL_ID_ERROR,
    ERR_OSACCOUNT_KIT_REMOTE_IS_NULLPTR,
    ERR_OSACCOUNT_KIT_SEND_REQUEST_ERROR,
    ERR_OSACCOUNT_KIT_GET_SYSTEM_ABILITY_MANAGER_ERROR,
    ERR_OSACCOUNT_KIT_GET_ACCOUNT_SYSTEM_ABILITY_ERROR,
    // 4587537
    ERR_OSACCOUNT_KIT_CAST_ACCOUNT_PROXY_ERROR,
    ERR_OSACCOUNT_KIT_GET_APP_ACCOUNT_SERVICE_ERROR,
    ERR_OSACCOUNT_KIT_GET_APP_ACCOUNT_PROXY_ERROR,
    ERR_OSACCOUNT_KIT_CREATE_APP_ACCOUNT_DEATH_RECIPIENT_ERROR,
    ERR_OSACCOUNT_KIT_IS_OS_ACCOUNT_EXISTS_ERROR,
    ERR_OSACCOUNT_KIT_IS_OS_ACCOUNT_ACTIVED_ERROR,
    ERR_OSACCOUNT_KIT_IS_OS_ACCOUNT_CONSTRAINT_ENABLE_ERROR,
    ERR_OSACCOUNT_KIT_IS_TEST_OS_ACCOUNT_ERROR,
    ERR_OSACCOUNT_KIT_IS_OS_ACCOUNT_VERIFIED_ERROR,
    ERR_OSACCOUNT_KIT_GET_CREATED_OS_ACCOUNT_COUNT_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_LOCAL_ID_FROM_PROCESS_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_LOCAL_ID_FROM_UID_ERROR,
    ERR_OSACCOUNT_KIT_QUERY_MAX_OS_ACCOUNT_NUMBER_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_ALL_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_KIT_QUERY_ALL_CREATED_OS_ACCOUNTS_ERROR,
    ERR_OSACCOUNT_KIT_QUERY_CURRENT_OS_ACCOUNT_ERROR,
    ERR_OSACCOUNT_KIT_QUERY_OS_ACCOUNT_BY_ID_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_TYPE_FROM_PROCESS_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_PROFILE_PHOTO_ERROR,
    ERR_OSACCOUNT_KIT_IS_MULTI_OS_ACCOUNT_ENABLE_ERROR,
    // 4587557
    ERR_OSACCOUNT_KIT_WRITE_STRING_CONSTRAINT_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_INT_UID_ERROR,
    ERR_OSACCOUNT_KIT_READ_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_KIT_READ_IN_UID_ERROR,
    ERR_OSACCOUNT_KIT_SET_OS_ACCOUNT_NAME_ERROR,
    ERR_OSACCOUNT_KIT_SET_OS_ACCOUNT_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_KIT_SET_OS_ACCOUNT_PROFILE_PHOTO_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_STRING_LOCAL_NAME_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_STRING_STOREID_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_STRING_PHOTO_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_STRING_VECTOR_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_KIT_READ_STRING_VECTOR_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_BOOL_ENABLE_ERROR,
    ERR_OSACCOUNT_KIT_GET_DISTRIBUTED_VIRTUAL_DEVICE_ID_ERROR,
    ERR_OSACCOUNT_KIT_ACTIVATE_OS_ACCOUNT_ERROR,
    ERR_OSACCOUNT_KIT_START_OS_ACCOUNT_ERROR,
    ERR_OSACCOUNT_KIT_STOP_OS_ACCOUNT_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_LOCAL_ID_FOR_SERIAL_NUMBER_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_LOCAL_ID_FOR_DOMAIN_ERROR,
    ERR_OSACCOUNT_KIT_GET_SERIAL_NUMBER_FOR_OS_ACCOUNT__ERROR,
    // 4587577
    ERR_OSACCOUNT_KIT_LOCAL_NAME_OUTFLOW_ERROR,
    ERR_OSACCOUNT_KIT_LOCAL_NAME_EMPTY_ERROR,
    ERR_OSACCOUNT_KIT_DOMAIN_NAME_LENGTH_INVALID_ERROR,
    ERR_OSACCOUNT_KIT_LOCAL_ID_INVALID_ERROR,
    ERR_OSACCOUNT_KIT_DOMAIN_ACCOUNT_NAME_LENGTH_INVALID_ERROR,
    ERR_OSACCOUNT_KIT_CANNOT_DELETE_ID_ERROR,
    ERR_OSACCOUNT_KIT_DOMAIN_OUTFLOW_ERROR,
    ERR_OSACCOUNT_KIT_PHOTO_OUTFLOW_ERROR,
    ERR_OSACCOUNT_KIT_TYPE_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_PARCELABLE_SUBSCRIBE_INFO_ERROR,
    ERR_OSACCOUNT_KIT_WRITE_PARCELABLE_EVENT_LISTENER_ERROR,
    ERR_OSACCOUNT_KIT_SUBSCRIBE_ERROR,
    ERR_OSACCOUNT_KIT_UNSUBSCRIBE_ERROR,
    ERR_OSACCOUNT_KIT_SUBSCRIBER_IS_NULLPTR,
    ERR_OSACCOUNT_KIT_GET_SUBSCRIBE_INFO_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_SUBSCRIBE_TYPE_ERROR,
    ERR_OSACCOUNT_KIT_GET_NAME_ERROR,
    ERR_OSACCOUNT_KIT_NO_SPECIFIED_SUBSCRIBER_HAS_BEEN_REGISTERED,
    ERR_OSACCOUNT_KIT_WRITE_BOOL_IS_OSACCOUNT_VERIFIED_ERROR,
    ERR_OSACCOUNT_KIT_DUMP_STATE_ERROR,
    ERR_OSACCOUNT_KIT_GET_CREATED_OS_ACCOUNT_NUM_FROM_DATABASE_ERROR,
    // 4587597
    ERR_OSACCOUNT_KIT_GET_SERIAL_NUM_FROM_DATABASE_ERROR,
    ERR_OSACCOUNT_KIT_GET_MAX_ALLOWED_CREATE_ID_FROM_DATABASE_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_FROM_DATABASE_ERROR,
    ERR_OSACCOUNT_KIT_GET_OS_ACCOUNT_LIST_FROM_DATABASE_ERROR,
    ERR_OSACCOUNT_KIT_CREATE_BASIC_ACCOUNTS_ERROR,
    ERR_OSACCOUNT_KIT_QUERY_ACTIVE_OS_ACCOUNT_IDS_ERROR,
};

// Error code for OSACCOUNT_SERVICE
constexpr ErrCode ACCOUNT_OSACCOUNT_SERVICE_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_OSACCOUNT_SERVICE);
enum {
    // 4653057
    ERR_OSACCOUNT_SERVICE_MANAGER_BAD_UID_ERROR = ACCOUNT_OSACCOUNT_SERVICE_ERR_OFFSET + 0x0001,
    ERR_OSACCOUNT_SERVICE_MANAGER_QUERY_DISTRIBUTE_DATA_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_NOT_ENABLE_MULTI_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_NAME_SIZE_EMPTY_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_NAME_SIZE_OVERFLOW_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_PHOTO_SIZE_OVERFLOW_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_DOMAIN_NAME_LEN_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_DOMAIN_ACCOUNT_NAME_LEN_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_PHOTO_OUTFLOW_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_LOCAL_NAME_OUTFLOW_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_DOMAIN_SIZE_OVERFLOW_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_ID_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_CANNOT_HIDE_INTERFACE_ERROR,
    ERR_OSACCOUNT_SERVICE_MANAGER_CREATE_OSACCOUNT_TYPE_ERROR,
    // 4653067
    ERR_OSACCOUNT_SERVICE_INNER_SELECT_OSACCOUNT_BYID_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_CREATE_OVERFLOW_OSACCOUNTMAX_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_CANNOT_CREATE_ADMIN_ACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_OS_ACCOUNT_TYPE_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_UPDATE_CONFIG_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_CANNOT_FIND_OSACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_CANNOT_ADMIN_OSACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_VISITOR_ACCOUNT_EXIST_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_CREATE_ACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_CANNOT_DELETE_ADMIN_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_CANNOT_DELE_OSACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_SELECT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_UPDATE_ACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_FIND_ACCOUNT_IN_ACTIVE_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_GET_SERIAL_NUMBER_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_GET_OSACCOUNT_ID_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_STOP_OSACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_GET_TYPE_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_IS_UNVERIFIED_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_GET_ACCOUNT_LIST_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_ALREADY_ACTIVE_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_STOP_ACTIVE_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_SEND_AM_ACCOUNT_START_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_SEND_AM_ACCOUNT_STOP_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_SEND_BM_ACCOUNT_CREATE_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_SEND_BM_ACCOUNT_DELE_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_SEND_IAM_ACCOUNT_DELE_ERROR,
    ERR_OSACCOUNT_SERVICE_PERMISSION_DENIED,
    ERR_OSACCOUNT_SERVICE_INNER_SEND_AM_ACCOUNT_SWITCH_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_SEND_CE_ACCOUNT_SWITCH_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_REMOVE_ACCOUNT_ACTIVED_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_CANNOT_REMOVE_ADMIN_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_DO_NOT_HAVE_PHOTO_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_SER_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_DOMAIN_ALREADY_BIND_ERROR,
    ERR_OSACCOUNT_SERVICE_SUBSCRIBE_MANAGER_PTR_IS_NULLPTR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_TO_BE_REMOVED_ERROR,
    // 4653108
    ERR_OSACCOUNT_SERVICE_CONTROL_FIND_AVAILABLE_ID_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_UPDATE_OSACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_CREATE_OSACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_SELECT_OS_ACCOUNT_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_SELECT_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_GET_CONSTRAINTS_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_GET_TYPE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_INSERT_FILE_EXISTS_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_INSERT_OS_ACCOUNT_INFO_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_INSERT_OS_ACCOUNT_LIST_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_GET_OS_ACCOUNT_LIST_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_DEL_OS_ACCOUNT_INFO_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_UPDATE_FILE_NOT_EXISTS_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_UPDATE_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_MAX_CAN_CREATE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_SELECT_CAN_USE_ID_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_GET_ACCOUNT_LIST_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_SET_ACCOUNT_LIST_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_PHOTO_STR_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_ID_CANNOT_CREATE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_CANNOT_DELETE_ID_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_DO_NOT_HAVE_ALLOW_SERIAL_ERROR,
    // 4653130
    ERR_OSACCOUNT_SERVICE_FILE_DELE_ERROR,
    ERR_OSACCOUNT_SERVICE_FILE_FIND_DIR_ERROR,
    ERR_OSACCOUNT_SERVICE_FILE_FIND_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_FILE_CREATE_DIR_ERROR,
    ERR_OSACCOUNT_SERVICE_FILE_CHANGE_DIR_MODE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_INSERT_OS_ACCOUNT_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_DATA_STORAGE_KEY_NOT_EXISTS_ERROR,
    ERR_OSACCOUNT_SERVICE_DATA_STORAGE_KEY_EXISTED_ERROR,
    ERR_OSACCOUNT_SERVICE_FILE_CREATE_FILE_FAILED_ERROR,
    ERR_OSACCOUNT_SERVICE_CREATE_EVENT_HANDLER,
    ERR_OSACCOUNT_SERVICE_ACCOUNT_INFO_EMPTY_ERROR,
    // 4653141
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_AM_ACCOUNT_START_ERROR,
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_AM_ACCOUNT_STOP_ERROR,
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_BM_ACCOUNT_CREATE_ERROR,
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_BM_ACCOUNT_DELETE_ERROR,
    ERR_OSACCOUNT_SERVICE_SUBSCRIBE_GET_EVENT_HANDLE_ERROR,
    ERR_OSACCOUNT_SERVICE_STANDARD_BYTE_STRING_ERROR,
    ERR_OSACCOUNT_SERVICE_STANDARD_CREATE_IMAGE_SOURCE_ERROR,
    ERR_OSACCOUNT_SERVICE_STANDARD_CREATE_PIXEL_MAP_ERROR,
    ERR_OSACCOUNT_SERVICE_OS_FILE_GET_CONFIG_ERROR,
    ERR_OSACCOUNT_SERVICE_OS_FILE_GET_CONSTRAINTS_LITS_ERROR,
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_STORAGE_ACCOUNT_CREATE_ERROR,
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_STORAGE_ACCOUNT_REMOVE_ERROR,
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_STORAGE_ACCOUNT_START_ERROR,
    ERR_OSACCOUNT_SERVICE_INTERFACE_TO_STORAGE_ACCOUNT_STOP_ERROR,
    ERR_OSACCOUNT_SERVICE_INNER_ACCOUNT_OPERATING_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_SAVE_SPECIFIC_CONSTRAINTS_TO_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_GET_SPECIFIC_CONSTRAINTS_FROM_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_SAVE_GLOBAL_CONSTRAINTS_TO_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_GET_GLOBAL_CONSTRAINTS_FROM_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_SAVE_BASE_CONSTRAINTS_TO_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_CONTROL_GET_BASE_CONSTRAINTS_FROM_FILE_ERROR,
    ERR_OSACCOUNT_SERVICE_GET_DATA_FROM_BASE_CONSTRAINTS_FILE_EMPTY,
    ERR_OSACCOUNT_SERVICE_GET_DATA_FROM_GLOBAL_CONSTRAINTS_FILE_EMPTY,
    ERR_OSACCOUNT_SERVICE_GET_DATA_FROM_SPECIFIC_CONSTRAINTS_FILE_EMPTY,
};

// Error code for ACCOUNT_IAM_KIT
constexpr ErrCode ACCOUNT_IAM_KIT_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_ACCOUNT_IAM_KIT);
enum {
    ERR_ACCOUNT_IAM_KIT_SEND_REQUEST = ACCOUNT_IAM_KIT_ERR_OFFSET + 0x0001,
    ERR_ACCOUNT_IAM_KIT_PROXY_ERROR,
    ERR_ACCOUNT_IAM_KIT_WRITE_PARCEL_FAIL,
    ERR_ACCOUNT_IAM_KIT_READ_PARCEL_FAIL,
};

// Error code for ACCOUNT_IAM_SERVICE
constexpr ErrCode ACCOUNT_IAM_SERVICE_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_ACCOUNT_IAM_SERVICE);
enum {
    ERR_ACCOUNT_IAM_SERVICE_PERMISSION_DENIED = ACCOUNT_IAM_SERVICE_ERR_OFFSET + 0x0001,
    ERR_ACCOUNT_IAM_SERVICE_GET_STORAGE_SYSTEM_ABILITY,
    ERR_ACCOUNT_IAM_SERVICE_REMOTE_IS_NULLPTR,
    ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL
};

// Error code for OHOSACCOUNT_KIT
constexpr ErrCode ACCOUNT_OHOSACCOUNT_KIT_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_OHOSACCOUNT_KIT);
enum {
    ERR_OHOSACCOUNT_KIT_GET_SYSTEM_ABILITY_MANAGER = ACCOUNT_MODULE_OHOSACCOUNT_KIT + 0x0001,
    ERR_OHOSACCOUNT_KIT_INVALID_PARAMETER,
};

// Error code for OHOSACCOUNT_SERVICE
constexpr ErrCode ACCOUNT_OHOSACCOUNT_SERVICE_ERR_OFFSET =
    ErrCodeOffset(SUBSYS_ACCOUNT, ACCOUNT_MODULE_OHOSACCOUNT_SERVICE);
enum {
    ERR_OHOSACCOUNT_SERVICE_NAME_IS_EMPTY = ACCOUNT_MODULE_OHOSACCOUNT_SERVICE + 0x0001,
    ERR_OHOSACCOUNT_SERVICE_FILE_CHANGE_DIR_MODE_ERROR,
};

enum JSErrorCode {
    ERR_JS_SUCCESS = 0,
    ERR_JS_PERMISSION_DENIED = 201,
    ERR_JS_PARAMETER_ERROR = 401,
    ERR_JS_CAPABILITY_NOT_SUPPORTED = 801,
    ERR_JS_SYSTEM_SERVICE_EXCEPTION = 12300001,
    ERR_JS_INVALID_PARAMETER = 12300002,
    ERR_JS_ACCOUNT_NOT_FOUND = 12300003,
    ERR_JS_ACCOUNT_RESTRICTED = 12300004,
    ERR_JS_LISTENER_ALREADY_REGISTERED = 12300005,
    ERR_JS_LISTENER_NOT_REGISTERED = 12300006,
    ERR_JS_PIN_INPUTER_ALREADY_EXIST = 12300007,
    ERR_JS_ACCOUNT_ALREADY_EXIST = 12300008,
    ERR_JS_ACCOUNT_ALREADY_ACTIVATED = 12300009,
    ERR_JS_ACCOUNT_SERVICE_BUSY = 12300010,
    ERR_JS_ACCOUNT_NUMBER_REACH_LIMIT = 12300011,
    ERR_JS_MULTI_USER_NOT_SUPPORT = 12300012,
    ERR_JS_ACCOUNT_TYPE_NOT_SUPPORT = 12300013,
    ERR_JS_TRUST_LEVEL_NOT_SUPPORTED = 12300014,
    ERR_JS_AUTH_TYPE_NOT_SUPPORTED = 12300015,
    ERR_JS_AUTH_TIMEOUT = 12300016,
    ERR_JS_AUTH_SERVICE_BUSY = 12300017,
    ERR_JS_AUTH_SERVICE_LOCKED = 12300018,
    ERR_JS_CREDENTIAL_NOT_EXIST = 12300019,
    ERR_JS_INVALID_CONTEXT_ID = 12300020,

    ERR_JS_APPLICATION_NOT_EXIST = 12400001,
    ERR_JS_ACCOUNT_AUTHENTICATOR_NOT_EXIST = 12400002,
    ERR_JS_ACCOUNT_AUTHENTICATOR_SERVICE_EXCEPTION = 12400003,
    ERR_JS_AUTH_TYPE_NOT_FOUND = 12400004,
    ERR_JS_SESSION_NOT_EXIST = 12400005,
    ERR_JS_AUTHORIZATION_LIST_TOO_LARGE = 12400006,
    ERR_JS_TOKEN_NUMBER_REACH_LIMIT = 12400007,
    ERR_JS_CUSTOM_DATA_NUMBER_REACH_LIMIT = 12400008,
    ERR_JS_CUSTOM_DATA_NOT_FOUND = 12400009
};

int32_t ConvertToJSErrCode(int32_t errCode);
}  // namespace OHOS

#endif  // OS_ACCOUNT_FRAMEWORKS_COMMON_ACCOUNT_ERROR_INCLUDE_ACCOUNT_ERROR_NO_H
