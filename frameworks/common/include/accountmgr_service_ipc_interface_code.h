/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#ifndef OS_ACCOUNT_FRAMEWORK_ACCOUNTMGR_SERVICE_IPC_INTERFACE_CODE_H
#define OS_ACCOUNT_FRAMEWORK_ACCOUNTMGR_SERVICE_IPC_INTERFACE_CODE_H

#include <cstdint>

/* SAID: 200 */
namespace OHOS {
namespace AccountSA {
enum class AccountMgrInterfaceCode : uint32_t {
    UPDATE_OHOS_ACCOUNT_INFO = 1,
    QUERY_OHOS_ACCOUNT_INFO = 2,
    QUERY_OHOS_ACCOUNT_QUIT_TIPS = 3,
    QUERY_OHOS_ACCOUNT_INFO_BY_USER_ID = 4,
    SET_OHOS_ACCOUNT_INFO = 5,
    GET_OHOS_ACCOUNT_INFO = 6,
    GET_OHOS_ACCOUNT_INFO_BY_USER_ID = 7,
    SET_OHOS_ACCOUNT_INFO_BY_USER_ID = 8,
    SUBSCRIBE_DISTRIBUTED_ACCOUNT_EVENT = 9,
    UNSUBSCRIBE_DISTRIBUTED_ACCOUNT_EVENT = 10,
    QUERY_DISTRIBUTE_VIRTUAL_DEVICE_ID = 11,
    QUERY_DISTRIBUTE_VIRTUAL_DEVICE_ID_BY_BUNDLE_NAME = 12,
    QUERY_DEVICE_ACCOUNT_ID = 104,
    GET_APP_ACCOUNT_SERVICE = 105,
    GET_OS_ACCOUNT_SERVICE = 106,
    GET_ACCOUNT_IAM_SERVICE = 107,
    GET_DOMAIN_ACCOUNT_SERVICE = 108,
};

enum class OsAccountInterfaceCode : uint32_t {
    CREATE_OS_ACCOUNT = 0,
    CREATE_OS_ACCOUNT_WITH_SHORT_NAME,
    CREATE_OS_ACCOUNT_WITH_FULL_INFO,
    UPDATE_OS_ACCOUNT_WITH_FULL_INFO,
    CREATE_OS_ACCOUNT_FOR_DOMAIN,
    REMOVE_OS_ACCOUNT,
    IS_OS_ACCOUNT_EXISTS,
    IS_OS_ACCOUNT_ACTIVED,
    IS_OS_ACCOUNT_CONSTRAINT_ENABLE,
    CHECK_OS_ACCOUNT_CONSTRAINT_ENABLED,
    IS_OS_ACCOUNT_VERIFIED,
    GET_CREATED_OS_ACCOUNT_COUNT,
    GET_OS_ACCOUNT_LOCAL_ID_FROM_PROCESS,
    IS_MAIN_OS_ACCOUNT,
    GET_OS_ACCOUNT_LOCAL_ID_FROM_DOMAIN,
    QUERY_MAX_OS_ACCOUNT_NUMBER,
    GET_OS_ACCOUNT_ALL_CONSTRAINTS,
    QUERY_ALL_CREATED_OS_ACCOUNTS,
    QUERY_CURRENT_OS_ACCOUNT,
    QUERY_OS_ACCOUNT_BY_ID,
    GET_OS_ACCOUNT_TYPE_FROM_PROCESS,
    GET_OS_ACCOUNT_TYPE,
    GET_OS_ACCOUNT_PROFILE_PHOTO,
    IS_MULTI_OS_ACCOUNT_ENABLE,
    SET_OS_ACCOUNT_NAME,
    SET_OS_ACCOUNT_CONSTRAINTS,
    SET_OS_ACCOUNT_PROFILE_PHOTO,
    ACTIVATE_OS_ACCOUNT,
    DEACTIVATE_OS_ACCOUNT,
    DEACTIVATE_ALL_OS_ACCOUNTS,
    START_OS_ACCOUNT,
    SUBSCRIBE_OS_ACCOUNT,
    UNSUBSCRIBE_OS_ACCOUNT,
    GET_OS_ACCOUNT_LOCAL_ID_FOR_SERIAL_NUMBER,
    GET_SERIAL_NUMBER_FOR_OS_ACCOUNT,
    GET_OS_ACCOUNT_SWITCH_MOD,
    IS_CURRENT_OS_ACCOUNT_VERIFIED,
    IS_OS_ACCOUNT_COMPLETED,
    SET_CURRENT_OS_ACCOUNT_IS_VERIFIED,
    SET_OS_ACCOUNT_IS_VERIFIED,
    DUMP_STATE,
    GET_CREATED_OS_ACCOUNT_NUM_FROM_DATABASE,
    GET_SERIAL_NUM_FROM_DATABASE,
    GET_MAX_ALLOW_CREATE_ID_FROM_DATABASE,
    GET_OS_ACCOUNT_FROM_DATABASE,
    GET_OS_ACCOUNT_LIST_FROM_DATABASE,
    QUERY_ACTIVE_OS_ACCOUNT_IDS,
    QUERY_OS_ACCOUNT_CONSTRAINT_SOURCE_TYPES,
    SET_GLOBAL_OS_ACCOUNT_CONSTRAINTS,
    SET_SPECIFIC_OS_ACCOUNT_CONSTRAINTS,
    SET_DEFAULT_ACTIVATED_OS_ACCOUNT,
    GET_DEFAULT_ACTIVATED_OS_ACCOUNT,
    GET_OS_ACCOUNT_SHORT_NAME,
    GET_OS_ACCOUNT_NAME,
    GET_OS_ACCOUNT_SHORT_NAME_BY_ID,
    IS_OS_ACCOUNT_FOREGROUND,
    GET_FOREGROUND_OS_ACCOUNT_LOCAL_ID,
    GET_FOREGROUND_OS_ACCOUNTS,
    GET_BACKGROUND_OS_ACCOUNT_LOCAL_IDS,
    SET_OS_ACCOUNT_TO_BE_REMOVED,
    QUERY_MAX_LOGGED_IN_OS_ACCOUNT_NUMBER,
    IS_OS_ACCOUNT_DEACTIVATING,
    GET_OS_ACCOUNT_DOMAIN_INFO,
};

enum class AppAccountInterfaceCode : uint32_t {
    ADD_ACCOUNT = 0,
    ADD_ACCOUNT_IMPLICITLY,
    DELETE_ACCOUNT,
    GET_ACCOUNT_EXTRA_INFO,
    SET_ACCOUNT_EXTRA_INFO,
    ENABLE_APP_ACCESS,
    DISABLE_APP_ACCESS,
    SET_APP_ACCESS,
    CHECK_APP_ACCOUNT_SYNC_ENABLE,
    SET_APP_ACCOUNT_SYNC_ENABLE,
    GET_ASSOCIATED_DATA,
    SET_ASSOCIATED_DATA,
    GET_ACCOUNT_CREDENTIAL,
    SET_ACCOUNT_CREDENTIAL,
    AUTHENTICATE,
    GET_OAUTH_TOKEN,
    GET_AUTH_TOKEN,
    SET_OAUTH_TOKEN,
    DELETE_OAUTH_TOKEN,
    DELETE_AUTH_TOKEN,
    SET_OAUTH_TOKEN_VISIBILITY,
    SET_AUTH_TOKEN_VISIBILITY,
    CHECK_OAUTH_TOKEN_VISIBILITY,
    CHECK_AUTH_TOKEN_VISIBILITY,
    GET_AUTHENTICATOR_INFO,
    GET_ALL_OAUTH_TOKENS,
    GET_OAUTH_LIST,
    GET_AUTH_LIST,
    GET_AUTHENTICATOR_CALLBACK,
    CLEAR_OAUTH_TOKEN,
    GET_ALL_ACCOUNTS,
    GET_ALL_ACCESSIBLE_ACCOUNTS,
    QUERY_ALL_ACCESSIBLE_ACCOUNTS,
    SUBSCRIBE_ACCOUNT,
    UNSUBSCRIBE_ACCOUNT,
    CHECK_APP_ACCESS,
    DELETE_ACCOUNT_CREDENTIAL,
    SELECT_ACCOUNTS_BY_OPTIONS,
    VERIFY_CREDENTIAL,
    CHECK_ACCOUNT_LABELS,
    SET_AUTHENTICATOR_PROPERTIES,
    CREATE_ACCOUNT,
    CREATE_ACCOUNT_IMPLICITLY,
};

enum class AccountIAMInterfaceCode : uint32_t {
    OPEN_SESSION = 0,
    CLOSE_SESSION,
    ADD_CREDENTIAL,
    UPDATE_CREDENTIAL,
    CANCEL,
    DEL_CRED,
    DEL_USER,
    GET_CREDENTIAL_INFO,
    PREPARE_REMOTE_AUTH,
    AUTH_USER,
    CANCEL_AUTH,
    GET_AVAILABLE_STATUS,
    GET_PROPERTY,
    GET_PROPERTY_BY_CREDENTIAL_ID,
    SET_PROPERTY,
    GET_ENROLLED_ID,
    GET_ACCOUNT_STATE,
};

enum class DomainAccountInterfaceCode : uint32_t {
    REGISTER_PLUGIN = 0,
    UNREGISTER_PLUGIN = 1,
    DOMAIN_AUTH = 2,
    DOMAIN_AUTH_USER = 3,
    DOMAIN_AUTH_WITH_POPUP = 4,
    DOMAIN_HAS_DOMAIN_ACCOUNT = 5,
    DOMAIN_UPDATE_ACCOUNT_TOKEN = 6,
    DOMAIN_GET_ACCESS_TOKEN = 7,
    DOMAIN_ACCOUNT_STATUS_ENQUIRY = 8,
    DOMAIN_ACCOUNT_STATUS_LISTENER_REGISTER = 9,
    DOMAIN_ACCOUNT_STATUS_LISTENER_UNREGISTER = 10,
    DOMAIN_IS_AUTHENTICATION_EXPIRED,
    DOMAIN_SET_ACCOUNT_POLICY,
    DOMAIN_GET_ACCOUNT_INFO,
    ADD_SERVER_CONFIG,
    REMOVE_SERVER_CONFIG,
    GET_ACCOUNT_SERVER_CONFIG,
    DOMAIN_UPDATE_ACCOUNT_INFO,
    UPDATE_SERVER_CONFIG,
    GET_SERVER_CONFIG,
    GET_ALL_SERVER_CONFIGS,
};

enum class DomainAccountPluginInterfaceCode : int32_t {
    DOMAIN_PLUGIN_AUTH = 0,
    DOMAIN_PLUGIN_GET_AUTH_STATUS_INFO = 1,
    DOMAIN_PLUGIN_GET_DOMAIN_ACCOUNT_INFO = 2,
    DOMAIN_PLUGIN_ON_ACCOUNT_BOUND = 3,
    DOMAIN_PLUGIN_ON_ACCOUNT_UNBOUND = 4,
    DOMAIN_PLUGIN_IS_ACCOUNT_TOKEN_VALID = 5,
    DOMAIN_PLUGIN_GET_ACCESS_TOKEN = 6,
};

enum class DomainAccountCallbackInterfaceCode : uint32_t {
    DOMAIN_ACCOUNT_CALLBACK_ON_RESULT = 0,
};

enum class OsAccountEventInterfaceCode : uint32_t {
    ACCOUNT_CHANGED = 0,
    ACCOUNT_SWITCHED = 1,
    ON_STATE_CHANGED = 2,
};

enum class StateReplyCallbackInterfaceCode : uint32_t {
    ON_COMPLETE = 0,
};

enum class DistributedAccountEventInterfaceCode : uint32_t {
    ON_ACCOUNT_CHANGED = 0,
};

enum class AppAccountAuthorizationExtensionCallbackInterfaceCode : uint32_t {
    ON_RESULT = 0,
    ON_REQUEST_REDIRECTED,
};

enum class AppAccountAuthenticatorCallbackInterfaceCode : uint32_t {
    ACCOUNT_RESULT = 0,
    ACCOUNT_REQUEST_REDIRECTED,
    ACCOUNT_REQUEST_CONTINUED,
};

enum class AppAccountEventInterfaceCode : uint32_t {
    ACCOUNT_CHANGED = 0,
};

enum class AppAccountAuthenticatorInterfaceCode : uint32_t {
    ADD_ACCOUNT_IMPLICITLY = 0,
    AUTHENTICATE,
    VERIFY_CREDENTIAL,
    CHECK_ACCOUNT_LABELS,
    SET_PROPERTIES,
    IS_ACCOUNT_REMOVABLE,
    CREATE_ACCOUNT_IMPLICITLY,
    AUTH,
};

enum class AppAccountAuthorizationExtensionInterfaceCode : uint32_t {
    START_AUTHENTICATION = 0,
};

enum class AccountIAMCallbackInterfaceCode : uint32_t {
    ON_ACQUIRE_INFO = 0,
    ON_RESULT,
};

enum class IDMCallbackInterfaceCode : uint32_t {
    ON_ACQUIRE_INFO = 0,
    ON_RESULT,
};

enum class GetCredInfoCallbackInterfaceCode : uint32_t {
    ON_CREDENTIAL_INFO = 0,
};

enum class GetSetPropCallbackInterfaceCode : uint32_t {
    ON_RESULT = 0,
};

enum class GetEnrolledIdCallbackInterfaceCode : uint32_t {
    ON_ENROLLED_ID = 0,
};

enum class PreRemoteAuthCallbackInterfaceCode : uint32_t {
    ON_RESULT = 0,
};
}  // namespace AccountSA
}  // namespace OHOS
#endif // OS_ACCOUNT_FRAMEWORK_ACCOUNTMGR_SERVICE_IPC_INTERFACE_CODE_H
