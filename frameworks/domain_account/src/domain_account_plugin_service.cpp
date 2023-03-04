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

#include "domain_account_plugin_service.h"

#include "account_log_wrapper.h"
#include "domain_account_callback_client.h"
#include "domain_auth_callback_client.h"

namespace OHOS {
namespace AccountSA {
DomainAccountPluginService::DomainAccountPluginService(const std::shared_ptr<DomainAccountPlugin> &plugin)
    : innerPlugin_(plugin)
{}

DomainAccountPluginService::~DomainAccountPluginService()
{}

ErrCode DomainAccountPluginService::AuthCommonInterface(const DomainAccountInfo &info,
    const std::vector<uint8_t> &authData, const sptr<IDomainAuthCallback> &callback, AuthMode authMode)
{
    if (innerPlugin_ == nullptr) {
        ACCOUNT_LOGE("innerPlugin_ is nullptr");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    auto callbackClient = std::make_shared<DomainAuthCallbackClient>(callback);
    if (callbackClient == nullptr) {
        ACCOUNT_LOGE("failed to create DomainAuthCallbackClient");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    switch (authMode) {
        case AUTH_WITH_CREDENTIAL_MODE: {
            innerPlugin_->Auth(info, authData, callbackClient);
            break;
        }
        case AUTH_WITH_POPUP_MODE: {
            innerPlugin_->AuthWithPopup(info, callbackClient);
            break;
        }
        case AUTH_WITH_TOKEN_MODE: {
            innerPlugin_->AuthWithToken(info, authData, callbackClient);
            break;
        }
        default: {
            ACCOUNT_LOGE("authMode is invalid");
            return ERR_ACCOUNT_COMMON_INVALID_PARAMTER;
        }
    }
    return ERR_OK;
}

ErrCode DomainAccountPluginService::Auth(const DomainAccountInfo &info, const std::vector<uint8_t> &password,
    const sptr<IDomainAuthCallback> &callback)
{
    return AuthCommonInterface(info, password, callback, AUTH_WITH_CREDENTIAL_MODE);
}

ErrCode DomainAccountPluginService::AuthWithPopup(
    const DomainAccountInfo &info, const sptr<IDomainAuthCallback> &callback)
{
    return AuthCommonInterface(info, {}, callback, AUTH_WITH_POPUP_MODE);
}

ErrCode DomainAccountPluginService::AuthWithToken(
    const DomainAccountInfo &info, const std::vector<uint8_t> &token, const sptr<IDomainAuthCallback> &callback)
{
    return AuthCommonInterface(info, token, callback, AUTH_WITH_TOKEN_MODE);
}


ErrCode DomainAccountPluginService::GetAuthStatusInfo(
    const DomainAccountInfo &accountInfo, const sptr<IDomainAccountCallback> &callback)
{
    if (innerPlugin_ == nullptr) {
        ACCOUNT_LOGE("innerPlugin_ is nullptr");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    auto callbackClient = std::make_shared<DomainAccountCallbackClient>(callback);
    if (callbackClient == nullptr) {
        ACCOUNT_LOGE("failed to create DomainAccountCallbackClient");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    innerPlugin_->GetAuthStatusInfo(accountInfo, callbackClient);
    return ERR_OK;
}

ErrCode DomainAccountPluginService::GetDomainAccountInfo(
    const std::string &domain, const std::string &accountName, const sptr<IDomainAccountCallback> &callback)
{
    if (innerPlugin_ == nullptr) {
        ACCOUNT_LOGE("innerPlugin_ is nullptr");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    auto callbackClient = std::make_shared<DomainAccountCallbackClient>(callback);
    if (callbackClient == nullptr) {
        ACCOUNT_LOGE("failed to create domain account callback client");
        return ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR;
    }
    innerPlugin_->GetDomainAccountInfo(domain, accountName, callbackClient);
    return ERR_OK;
}

ErrCode DomainAccountPluginService::OnAccountBound(const DomainAccountInfo &info, const int32_t localId)
{
    if (innerPlugin_ == nullptr) {
        ACCOUNT_LOGE("innerPlugin_ is nullptr");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    innerPlugin_->OnAccountBound(info, localId);
    return ERR_OK;
}

ErrCode DomainAccountPluginService::OnAccountUnBound(const DomainAccountInfo &info)
{
    if (innerPlugin_ == nullptr) {
        ACCOUNT_LOGE("innerPlugin_ is nullptr");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    innerPlugin_->OnAccountUnBound(info);
    return ERR_OK;
}
}  // namespace AccountSA
}  // namespace OHOS